#!/usr/bin/env python3
"""
Pareto 前沿分析 — 给定 sweep 结果, 找出在多目标上不被支配的配置

用法:
  ./pareto_analyzer.py --input results/sweep.json --maximize ipc --minimize cycles area_proxy
  ./pareto_analyzer.py --input results/sweep.json --output results/pareto.json
"""
import argparse
import json
from pathlib import Path


def compute_pareto(results, maximize, minimize):
    """Pareto 前沿: 不被任何其他点在所有维度上严格更优

    缺失 metric 的处理: 最大化维度默认 -inf (最差),最小化维度默认 +inf (最差),
    这样缺失 metric 的点会被任何有效点"支配",正确地被排除出 Pareto 前沿。
    """
    NEG_INF = float("-inf")
    POS_INF = float("inf")
    pareto = []
    for i, r in enumerate(results):
        dominated = False
        for j, other in enumerate(results):
            if i == j:
                continue
            # other 必须在所有 maximize 维度上 >= r, 且在所有 minimize 维度上 <= r
            better_max = all(other.get(m, NEG_INF) >= r.get(m, NEG_INF) for m in maximize)
            better_min = all(other.get(m, POS_INF) <= r.get(m, POS_INF) for m in minimize)
            strictly_better = better_max and better_min and (
                any(other.get(m, NEG_INF) > r.get(m, NEG_INF) for m in maximize)
                or any(other.get(m, POS_INF) < r.get(m, POS_INF) for m in minimize)
            )
            if strictly_better:
                dominated = True
                break
        if not dominated:
            pareto.append(r)
    return pareto


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", required=True, help="sweep.json path (from sweep_driver.py)")
    ap.add_argument("--maximize", nargs="+", default=["ipc"], help="metrics to maximize")
    ap.add_argument("--minimize", nargs="+", default=["cycles", "wall_clock_sec"], help="metrics to minimize")
    ap.add_argument("--output", default=None, help="output pareto.json path")
    ap.add_argument("--top", type=int, default=0, help="show top N pareto points (0=all)")
    args = ap.parse_args()

    results = json.loads(Path(args.input).read_text())
    print(f"[PARETO] Total points: {len(results)}")
    print(f"[PARETO] Maximize: {args.maximize}, Minimize: {args.minimize}")

    # 过滤有效点 (有所有需要的 metric)
    valid = [
        r for r in results
        if all(m in r for m in args.maximize + args.minimize) and "error" not in r
    ]
    print(f"[PARETO] Valid points (all metrics present, no error): {len(valid)}")

    pareto = compute_pareto(valid, args.maximize, args.minimize)
    print(f"[PARETO] Pareto front size: {len(pareto)}")

    # 按 ipc 降序排
    if "ipc" in args.maximize:
        pareto.sort(key=lambda r: r.get("ipc", 0), reverse=True)

    if args.top > 0:
        pareto = pareto[:args.top]

    print("[PARETO] Top configurations:")
    for i, r in enumerate(pareto):
        cfg = r.get("config", {})
        ipc = r.get("ipc", "N/A")
        cycles = r.get("cycles", "N/A")
        stages = cfg.get("pipeline_stages", "?")
        btb = cfg.get("btb_entries", "?")
        bp = cfg.get("branch_predictor", "?")
        mul = cfg.get("mul_latency", "?")
        isa = cfg.get("isa", "?")
        print(f"  [{i+1}] IPC={ipc} stages={stages} BTB={btb} BP={bp} MUL={mul} ISA={isa} cycles={cycles}")

    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(pareto, indent=2))
        print(f"[PARETO] Saved → {args.output}")


if __name__ == "__main__":
    main()