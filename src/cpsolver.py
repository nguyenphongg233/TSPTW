import os
import sys
from ortools.sat.python import cp_model


def solve_tsptw_cp(input_data, time_limit=5.0):
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
    obj = 0

    for i in range(n + 1):
        for j in range(n + 1):
            c = input_data[idx]
            idx += 1
            if i != j and j != 0:
                if e[i] + s[i] + c + s[j] <= l[j]:
                    var = model.new_bool_var(f"x_{i}_{j}")
                    x[i][j] = var
                    model.add(f[j] >= f[i] + c + s[j]).only_enforce_if(var)
                    obj = obj + c * var

    for j in range(1, n + 1):
        model.add(sum(x[i][j] for i in range(n + 1)) == 1)
        model.add(sum(x[j][i] for i in range(1, n + 1)) <= 1)

    model.add(sum(x[0][i] for i in range(1, n + 1)) == 1)

    model.minimize(obj)

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = float(time_limit)
    status = solver.solve(model)

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
        cost = int(solver.value(obj))
        completion_time = int(solver.value(f[curr])) if curr != 0 else 0
        return tag, cost, completion_time, solver.wall_time, route

    return "NOT_FEASIBLE", 0, 0, solver.wall_time, []


def benchmark(mode="small", time_limit=5.0):
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
    header = (
        f"{'Instance':<22}{'N':<8}{'Group':<10}{'Status':<14}"
        f"{'TravelCost':<14}{'CompletionTime':<16}{'Time(s)':<12}"
    )
    print(header)
    print("-" * 96)

    total_cases = 0
    feasible_cases = 0
    optimal_cases = 0
    sum_cost = 0
    total_time = 0.0

    with open(csv_path, "w", encoding="utf-8") as csv_file:
        csv_file.write("instance,n,group,status,travel_cost,completion_time,runtime_sec\n")

        for name, group in cases:
            in_path = os.path.join(in_dir, f"{name}.inp")
            if not os.path.exists(in_path):
                print(f"[Skip] Missing {in_path}")
                continue

            with open(in_path, "r", encoding="utf-8") as f_in:
                tokens = list(map(int, f_in.read().split()))

            n = tokens[0]
            status_str, cost, comp_time, elapsed, route = solve_tsptw_cp(tokens, time_limit)

            total_cases += 1
            total_time += elapsed
            is_feas = status_str in ("OPTIMAL", "FEASIBLE")
            if is_feas:
                feasible_cases += 1
                sum_cost += cost
                out_path = os.path.join(out_dir, f"{name}.out")
                with open(out_path, "w", encoding="utf-8") as f_out:
                    f_out.write(f"{len(route)}\n")
                    f_out.write(" ".join(map(str, route)) + "\n")
            if status_str == "OPTIMAL":
                optimal_cases += 1

            cost_str = str(cost) if is_feas else "-"
            comp_str = str(comp_time) if is_feas else "-"
            print(
                f"{name:<22}{n:<8}{group:<10}{status_str:<14}"
                f"{cost_str:<14}{comp_str:<16}{elapsed:<12.4f}"
            )
            csv_file.write(
                f"{name},{n},{group},{status_str},{cost},{comp_time},{elapsed:.6f}\n"
            )
            csv_file.flush()

    print("-" * 96)
    avg_cost = (sum_cost // feasible_cases) if feasible_cases > 0 else 0
    print(
        f"[Summary] Feasible: {feasible_cases}/{total_cases} (Optimal: {optimal_cases}) | "
        f"Avg Feasible Cost: {avg_cost} | Total Time: {total_time:.4f}s"
    )
    print(f"[Saved]   CSV results written to '{csv_path}'")


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--bench":
        mode = sys.argv[2] if len(sys.argv) >= 3 else "small"
        time_limit = float(sys.argv[3]) if len(sys.argv) >= 4 else 5.0
        benchmark(mode, time_limit)
        return

    if len(sys.argv) >= 2 and os.path.exists(sys.argv[1]):
        time_limit = float(sys.argv[2]) if len(sys.argv) >= 3 else 5.0
        with open(sys.argv[1], "r", encoding="utf-8") as f_in:
            input_data = list(map(int, f_in.read().split()))
    else:
        input_data = list(map(int, sys.stdin.read().split()))
        time_limit = 5.0

    if not input_data:
        return

    status_str, cost, comp_time, elapsed, route = solve_tsptw_cp(input_data, time_limit)
    if status_str in ("OPTIMAL", "FEASIBLE"):
        print(f"{status_str} cost = {cost} | completion = {comp_time} | time = {elapsed:.3f}s")
        print(len(route))
        print(" ".join(map(str, route)))
    else:
        print("NOT_FEASIBLE")


if __name__ == "__main__":
    main()
