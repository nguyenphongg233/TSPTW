import concurrent.futures
import os
import sys
import time
from ortools.sat.python import cp_model


def solve_tsptw_cp(input_data, time_limit=30.0, num_workers=8):
    t0 = time.perf_counter()
    model = cp_model.CpModel()
    n = input_data[0]
    idx = 1

    e = [0] * (n + 1)
    l = [0] * (n + 1)
    s = [0] * (n + 1)
    f = [model.new_int_var(0, 0, "f_0")]

    for i in range(1, n + 1):
        e[i] = input_data[idx]
        l[i] = input_data[idx + 1]
        s[i] = input_data[idx + 2]
        f.append(model.new_int_var(e[i] + s[i], l[i], f"f_{i}"))
        idx += 3

    x = [[0] * (n + 1) for _ in range(n + 1)]
    obj_terms = []

    for i in range(n + 1):
        row_offset = idx + i * (n + 1)
        ei_si = e[i] + s[i]
        for j in range(1, n + 1):
            if i == j:
                continue
            c = input_data[row_offset + j]
            if ei_si + c + s[j] <= l[j]:
                var = model.new_bool_var(f"x_{i}_{j}")
                x[i][j] = var
                model.add(f[j] >= f[i] + c + s[j]).only_enforce_if(var)
                obj_terms.append(c * var)

    for j in range(1, n + 1):
        in_vars = [x[i][j] for i in range(n + 1) if not isinstance(x[i][j], int)]
        out_vars = [x[j][i] for i in range(1, n + 1) if not isinstance(x[j][i], int)]
        model.add(sum(in_vars) == 1)
        model.add(sum(out_vars) <= 1)

    depot_out = [x[0][i] for i in range(1, n + 1) if not isinstance(x[0][i], int)]
    model.add(sum(depot_out) == 1)

    model.minimize(cp_model.LinearExpr.sum(obj_terms))

    build_time = time.perf_counter() - t0
    remaining_time = max(1.0, float(time_limit) - build_time)

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = remaining_time
    solver.parameters.num_search_workers = int(num_workers)
    status = solver.solve(model)

    total_wall = time.perf_counter() - t0

    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        tag = "OPTIMAL" if status == cp_model.OPTIMAL else "FEASIBLE"
        route = []
        curr = 0
        for _ in range(n):
            for nxt in range(1, n + 1):
                if not isinstance(x[curr][nxt], int) and solver.value(x[curr][nxt]) == 1:
                    route.append(nxt)
                    curr = nxt
                    break
        cost = int(solver.value(obj_terms[0] if False else sum(
            input_data[1 + 3 * n + (0 if k == 0 else route[k - 1]) * (n + 1) + route[k]]
            for k in range(n)
        ))) if len(route) == n else int(solver.objective_value)
        completion_time = int(solver.value(f[curr])) if curr != 0 else 0
        return tag, cost, completion_time, total_wall, route

    return "NOT_FEASIBLE", 0, 0, total_wall, []


def _run_one_case(args):
    name, group, in_path, out_dir, time_limit, workers_per_proc = args
    if not os.path.exists(in_path):
        return name, 0, group, "MISSING", 0, 0, 0.0

    with open(in_path, "r", encoding="utf-8") as f_in:
        tokens = list(map(int, f_in.read().split()))

    n = tokens[0]
    status_str, cost, comp_time, elapsed, route = solve_tsptw_cp(tokens, time_limit, workers_per_proc)

    if status_str in ("OPTIMAL", "FEASIBLE"):
        out_path = os.path.join(out_dir, f"{name}.out")
        with open(out_path, "w", encoding="utf-8") as f_out:
            f_out.write(f"{len(route)}\n")
            f_out.write(" ".join(map(str, route)) + "\n")

    return name, n, group, status_str, cost, comp_time, elapsed


def benchmark(mode="all", time_limit=30.0, procs=4, workers_per_proc=4):
    in_dir = "data/input/"
    out_dir = "data/output/cpSolver/"
    res_dir = "results/"
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(res_dir, exist_ok=True)

    nC = [10, 20, 50]
    nT = ["01", "02", "03", "04", "05", "06", "07", "08", "09", "10"]

    bnC = [200, 500, 1000]
    types = ["tight", "med", "wide"]
    bnT = ["01", "02", "03", "04", "05"]

    cases = []
    if mode in ("all", "small"):
        for n in nC:
            for tid in nT:
                cases.append((f"n{n}_{tid}", "small"))

    if mode in ("all", "big"):
        for n in bnC:
            for tp in types:
                for tid in bnT:
                    cases.append((f"n{n}_{tp}_{tid}", tp))

    csv_path = os.path.join(res_dir, "cp_benchmark_results.csv")
    print(f"[CP-SAT Benchmark] Mode: {mode} | TimeLimit: {time_limit}s | Procs: {procs} x {workers_per_proc} workers")
    header = (
        f"{'Instance':<22}{'N':<8}{'Group':<10}{'Status':<14}"
        f"{'TravelCost':<14}{'CompletionTime':<16}{'Time(s)':<12}"
    )
    print(header)
    print("-" * 96)

    tasks = [
        (name, group, os.path.join(in_dir, f"{name}.inp"), out_dir, time_limit, workers_per_proc)
        for name, group in cases
    ]

    results_map = {}
    t_wall_start = time.perf_counter()

    with concurrent.futures.ProcessPoolExecutor(max_workers=procs) as executor:
        future_to_name = {executor.submit(_run_one_case, t): t[0] for t in tasks}
        for future in concurrent.futures.as_completed(future_to_name):
            name, n, group, status_str, cost, comp_time, elapsed = future.result()
            results_map[name] = (n, group, status_str, cost, comp_time, elapsed)

            is_feas = status_str in ("OPTIMAL", "FEASIBLE")
            cost_str = str(cost) if is_feas else "-"
            comp_str = str(comp_time) if is_feas else "-"
            print(
                f"{name:<22}{n:<8}{group:<10}{status_str:<14}"
                f"{cost_str:<14}{comp_str:<16}{elapsed:<12.4f}",
                flush=True
            )

    wall_elapsed = time.perf_counter() - t_wall_start

    total_cases = 0
    feasible_cases = 0
    optimal_cases = 0
    sum_cost = 0
    total_cpu_time = 0.0

    with open(csv_path, "w", encoding="utf-8") as csv_file:
        csv_file.write("instance,n,group,status,travel_cost,completion_time,runtime_sec\n")
        for name, _ in cases:
            if name not in results_map:
                continue
            n, group, status_str, cost, comp_time, elapsed = results_map[name]
            if status_str == "MISSING":
                continue

            total_cases += 1
            total_cpu_time += elapsed
            if status_str in ("OPTIMAL", "FEASIBLE"):
                feasible_cases += 1
                sum_cost += cost
            if status_str == "OPTIMAL":
                optimal_cases += 1

            csv_file.write(f"{name},{n},{group},{status_str},{cost},{comp_time},{elapsed:.6f}\n")

    print("-" * 96)
    avg_cost = (sum_cost // feasible_cases) if feasible_cases > 0 else 0
    print(
        f"[Summary] Feasible: {feasible_cases}/{total_cases} (Optimal: {optimal_cases}) | "
        f"Avg Feasible Cost: {avg_cost} | Wall Time: {wall_elapsed:.4f}s (Sum: {total_cpu_time:.4f}s)"
    )
    print(f"[Saved]   CSV results written to '{csv_path}'")


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--bench":
        mode = sys.argv[2] if len(sys.argv) >= 3 else "all"
        time_limit = float(sys.argv[3]) if len(sys.argv) >= 4 else 30.0
        procs = int(sys.argv[4]) if len(sys.argv) >= 5 else 4
        workers = int(sys.argv[5]) if len(sys.argv) >= 6 else 4
        benchmark(mode, time_limit, procs, workers)
        return

    if len(sys.argv) >= 2 and os.path.exists(sys.argv[1]):
        time_limit = float(sys.argv[2]) if len(sys.argv) >= 3 else 30.0
        workers = int(sys.argv[3]) if len(sys.argv) >= 4 else 8
        with open(sys.argv[1], "r", encoding="utf-8") as f_in:
            input_data = list(map(int, f_in.read().split()))
    else:
        input_data = list(map(int, sys.stdin.read().split()))
        time_limit = 30.0
        workers = 8

    if not input_data:
        return

    status_str, cost, comp_time, elapsed, route = solve_tsptw_cp(input_data, time_limit, workers)
    if status_str in ("OPTIMAL", "FEASIBLE"):
        print(f"{status_str} cost = {cost} | completion = {comp_time} | time = {elapsed:.3f}s")
        print(len(route))
        print(" ".join(map(str, route)))
    else:
        print("NOT_FEASIBLE")


if __name__ == "__main__":
    main()
