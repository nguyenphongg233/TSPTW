import csv
import os
import re


def parse_alns_best_time(log_path):
    if not os.path.exists(log_path):
        return None, None
    best_iter = 0
    best_time = 0.0
    pattern = re.compile(r"#iter\s*=\s*(\d+)\s*\|\s*obj\s*=\s*\d+.*?\|\s*time\s*=\s*([\d.]+)s")
    with open(log_path, "r", encoding="utf-8") as f:
        for line in f:
            m = pattern.search(line)
            if m:
                best_iter = int(m.group(1))
                best_time = float(m.group(2))
    return best_iter, best_time


def main():
    ref_path = "data/input/reference_costs.csv"
    cp_path = "results/cp_benchmark_results.csv"
    heu_path = "results/heuristic_benchmark_results.csv"
    alns_path = "results/alns_benchmark_results.csv"
    log_dir = "results/logs/"

    out_csv = "results/comparison_detailed.csv"
    out_md = "results/comparison_detailed.md"

    ref = {}
    order = []
    with open(ref_path, "r", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            name = r["filename"].replace(".inp", "")
            ref[name] = r
            order.append(name)

    cp = {}
    if os.path.exists(cp_path):
        with open(cp_path, "r", encoding="utf-8") as f:
            for r in csv.DictReader(f):
                cp[r["instance"]] = r

    heu = {}
    if os.path.exists(heu_path):
        with open(heu_path, "r", encoding="utf-8") as f:
            for r in csv.DictReader(f):
                heu[r["instance"]] = r

    alns = {}
    if os.path.exists(alns_path):
        with open(alns_path, "r", encoding="utf-8") as f:
            for r in csv.DictReader(f):
                alns[r["instance"]] = r

    rows = []
    for name in order:
        r = ref[name]
        n = int(r["n"])
        tp = r["type"]
        slack = int(r["window_slack"])
        ref_cost = int(r["planted_travel_cost"])

        c = cp.get(name, {})
        cp_status = c.get("status", "N/A")
        cp_feas = cp_status in ("OPTIMAL", "FEASIBLE")
        cp_obj = int(c["travel_cost"]) if cp_feas else None
        cp_time = float(c["runtime_sec"]) if "runtime_sec" in c else None

        h = heu.get(name, {})
        heu_feas = h.get("feasible") == "1"
        heu_obj = int(h["travel_cost"]) if heu_feas else None
        heu_time = float(h["runtime_sec"]) if "runtime_sec" in h else None

        a = alns.get(name, {})
        alns_feas = a.get("feasible") == "1"
        alns_obj = int(a["travel_cost"]) if alns_feas else None
        alns_time = float(a["runtime_sec"]) if "runtime_sec" in a else None
        best_iter, best_time = parse_alns_best_time(os.path.join(log_dir, f"{name}.log"))

        gap_cp = ((alns_obj - cp_obj) / cp_obj * 100.0) if (alns_feas and cp_feas and cp_obj) else None
        gap_ref = ((alns_obj - ref_cost) / ref_cost * 100.0) if alns_feas else None
        gap_heu = ((alns_obj - heu_obj) / heu_obj * 100.0) if (alns_feas and heu_feas and heu_obj) else None

        group_key = f"n{n}_{tp}" if tp != "small" else f"n{n}_small"
        rows.append({
            "instance": name,
            "n": n,
            "group": group_key,
            "slack": slack,
            "ref_obj": ref_cost,
            "cp_status": cp_status,
            "cp_obj": cp_obj,
            "cp_time": cp_time,
            "heu_feas": heu_feas,
            "heu_obj": heu_obj,
            "heu_time": heu_time,
            "alns_feas": alns_feas,
            "alns_obj": alns_obj,
            "alns_best_iter": best_iter,
            "alns_best_time": best_time,
            "alns_time": alns_time,
            "gap_cp": gap_cp,
            "gap_heu": gap_heu,
            "gap_ref": gap_ref,
        })

    # 1. Ghi file CSV chi tiết
    with open(out_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "instance", "n", "group", "window_slack", "ref_obj",
            "cp_status", "cp_obj", "cp_time_s",
            "heu_feasible", "heu_obj", "heu_time_s",
            "alns_feasible", "alns_obj", "alns_best_iter", "alns_best_time_s", "alns_total_time_s",
            "alns_vs_cp_pct", "alns_vs_heu_pct", "alns_vs_ref_pct"
        ])
        for row in rows:
            writer.writerow([
                row["instance"], row["n"], row["group"], row["slack"], row["ref_obj"],
                row["cp_status"],
                row["cp_obj"] if row["cp_obj"] is not None else "",
                f"{row['cp_time']:.4f}" if row["cp_time"] is not None else "",
                1 if row["heu_feas"] else 0,
                row["heu_obj"] if row["heu_obj"] is not None else "",
                f"{row['heu_time']:.4f}" if row["heu_time"] is not None else "",
                1 if row["alns_feas"] else 0,
                row["alns_obj"] if row["alns_obj"] is not None else "",
                row["alns_best_iter"] if row["alns_best_iter"] is not None else "",
                f"{row['alns_best_time']:.4f}" if row["alns_best_time"] is not None else "",
                f"{row['alns_time']:.4f}" if row["alns_time"] is not None else "",
                f"{row['gap_cp']:+.2f}%" if row["gap_cp"] is not None else "",
                f"{row['gap_heu']:+.2f}%" if row["gap_heu"] is not None else "",
                f"{row['gap_ref']:+.2f}%" if row["gap_ref"] is not None else "",
            ])

    # 2. Ghi file Markdown báo cáo chi tiết
    group_order = []
    groups = {}
    for row in rows:
        g = row["group"]
        if g not in groups:
            groups[g] = []
            group_order.append(g)
        groups[g].append(row)

    with open(out_md, "w", encoding="utf-8") as f:
        f.write("# Báo cáo so sánh chi tiết (Objective & Runtime) — 75 Testcases\n\n")
        f.write("- **CP-SAT:** Google OR-Tools CP-SAT (`max_time = 30.0s`, đa nhân)\n")
        f.write("- **Heuristic:** Nearest Feasible Neighbor + Randomized Greedy Insertion (C++20, 1 luồng)\n")
        f.write("- **ALNS:** Adaptive Large Neighborhood Search (C++20, `max_time = 20.0s`), kèm cột **`ALNS BestTime`** là thời điểm tìm thấy nghiệm tốt nhất trong log.\n\n")

        f.write("## 1. Tổng hợp trung bình theo từng nhóm Testcase\n\n")
        f.write("| Nhóm Test | Số bài | `RefObj` (TB) | `CP` Giải được | `CPObj` (TB) | `CPTime` (TB) | `Heu` Giải được | `HeuObj` (TB) | `HeuTime` (TB) | `ALNS` Giải được | **`ALNSObj` (TB)** | **`ALNS BestTime` (TB)** | `ALNSTime` | `ALNS vs CP` | `ALNS vs Ref` |\n")
        f.write("| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |\n")

        for g in group_order:
            items = groups[g]
            cnt = len(items)
            avg_ref = sum(x["ref_obj"] for x in items) / cnt

            cp_ok = [x for x in items if x["cp_obj"] is not None]
            cp_cnt = f"{len(cp_ok)}/{cnt}"
            cp_avg_obj = f"{sum(x['cp_obj'] for x in cp_ok) / len(cp_ok):,.0f}" if cp_ok else "—"
            cp_avg_tm = f"{sum(x['cp_time'] for x in items if x['cp_time'] is not None) / cnt:.3f}s"

            heu_ok = [x for x in items if x["heu_obj"] is not None]
            heu_cnt = f"{len(heu_ok)}/{cnt}"
            heu_avg_obj = f"{sum(x['heu_obj'] for x in heu_ok) / len(heu_ok):,.0f}" if heu_ok else "—"
            heu_avg_tm = f"{sum(x['heu_time'] for x in items if x['heu_time'] is not None) / cnt:.4f}s"

            alns_ok = [x for x in items if x["alns_obj"] is not None]
            alns_cnt = f"{len(alns_ok)}/{cnt}"
            alns_avg_obj = f"{sum(x['alns_obj'] for x in alns_ok) / len(alns_ok):,.0f}" if alns_ok else "—"
            alns_best_tm = f"{sum(x['alns_best_time'] for x in alns_ok if x['alns_best_time'] is not None) / len(alns_ok):.4f}s"
            alns_avg_tm = f"{sum(x['alns_time'] for x in items if x['alns_time'] is not None) / cnt:.2f}s"

            if cp_ok:
                both = [x["gap_cp"] for x in items if x["gap_cp"] is not None]
                vs_cp = f"**{sum(both)/len(both):+.2f}%**"
            else:
                vs_cp = "**ALNS WIN**"
            vs_ref_val = sum(x["gap_ref"] for x in alns_ok if x["gap_ref"] is not None) / len(alns_ok)

            f.write(
                f"| `{g}` | {cnt} | {avg_ref:,.0f} | {cp_cnt} | {cp_avg_obj} | {cp_avg_tm} | "
                f"{heu_cnt} | {heu_avg_obj} | {heu_avg_tm} | {alns_cnt} | **{alns_avg_obj}** | "
                f"**{alns_best_tm}** | {alns_avg_tm} | {vs_cp} | **{vs_ref_val:+.2f}%** |\n"
            )

        f.write("\n## 2. Bảng kết quả chi tiết 75/75 Testcases (Objective & Runtime)\n\n")
        f.write("| Instance | $N$ | Slack | `RefObj` | `CP Status` | `CPObj` | `CPTime` | `HeuObj` | `HeuTime` | **`ALNSObj`** | **`ALNS BestIter`** | **`ALNS BestTime`** | `ALNSTime` | `ALNS vs CP` | `ALNS vs Ref` |\n")
        f.write("| :--- | :-: | :-: | :-: | :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |\n")

        for row in rows:
            cp_obj_s = f"{row['cp_obj']:,}" if row["cp_obj"] is not None else "—"
            cp_tm_s = f"{row['cp_time']:.4f}s" if row["cp_time"] is not None else "—"
            heu_obj_s = f"{row['heu_obj']:,}" if row["heu_obj"] is not None else "NO"
            heu_tm_s = f"{row['heu_time']:.4f}s" if row["heu_time"] is not None else "—"
            alns_obj_s = f"**{row['alns_obj']:,}**" if row["alns_obj"] is not None else "NO"
            best_it_s = f"{row['alns_best_iter']:,}" if row["alns_best_iter"] is not None else "—"
            best_tm_s = f"{row['alns_best_time']:.4f}s" if row["alns_best_time"] is not None else "—"
            alns_tm_s = f"{row['alns_time']:.2f}s" if row["alns_time"] is not None else "—"
            vs_cp_s = f"{row['gap_cp']:+.2f}%" if row["gap_cp"] is not None else "ALNS WIN"
            vs_ref_s = f"{row['gap_ref']:+.2f}%" if row["gap_ref"] is not None else "—"

            f.write(
                f"| `{row['instance']}` | {row['n']} | {row['slack']} | {row['ref_obj']:,} | "
                f"{row['cp_status']} | {cp_obj_s} | {cp_tm_s} | "
                f"{heu_obj_s} | {heu_tm_s} | {alns_obj_s} | {best_it_s} | {best_tm_s} | {alns_tm_s} | "
                f"{vs_cp_s} | {vs_ref_s} |\n"
            )

    print(f"[Done] Saved detailed CSV -> {out_csv}")
    print(f"[Done] Saved detailed MD  -> {out_md}")


if __name__ == "__main__":
    main()
