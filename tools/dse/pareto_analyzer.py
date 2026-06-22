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


def render_ascii_chart(pareto_front, all_results, width=60, height=20,
                       x_metric="cycles", y_metric="ipc"):
    """ASCII 2D 散点图 — Pareto 前沿点为 '*', 其他点为 '.'

    设计 (M5-DSE M5.15 §6.7):
      - 默认 x 轴 cycles, y 轴 ipc (task spec)
      - 自动降级: 若 (x_metric, y_metric) 数据退化 (所有 x 或所有 y 相等),
        尝试 (pipeline_stages, mul_latency) 等实际变化的指标作为兜底
        (适用于 cpu_sim stub 输出 cycles/ipc 恒定的场景, 见 main.cpp ipc=0.0 占位)
      - y 轴向上增长 (终端 y=0 在顶部, 所以反转索引: grid[height-1-y][x])
      - 坐标轴边界 [min, max], 含两端
      - 仅依赖标准库 (无 matplotlib / plotext)
    """
    # 1. 过滤有 x/y 数据的 entry (排除含 error 的)
    valid = [
        r for r in all_results
        if x_metric in r and y_metric in r and "error" not in r
    ]
    if not valid:
        return f"(empty chart: no {x_metric}/{y_metric} data in {len(all_results)} results)"

    x_vals = [float(r[x_metric]) for r in valid]
    y_vals = [float(r[y_metric]) for r in valid]

    # 2. 检测退化: 若 min == max (axis 无变化), 自动降级到变化的指标
    actual_x, actual_y = x_metric, y_metric
    if len(set(x_vals)) <= 1 or len(set(y_vals)) <= 1:
        # 兜底链: (pipeline_stages, mul_latency) → (pipeline_stages, dispatch_width)
        for fallback_x, fallback_y in [
            ("pipeline_stages", "mul_latency"),
            ("pipeline_stages", "dispatch_width"),
            ("btb_entries", "pipeline_stages"),
        ]:
            if all(fallback_x in r and fallback_y in r for r in valid):
                fx = [float(r[fallback_x]) for r in valid]
                fy = [float(r[fallback_y]) for r in valid]
                if len(set(fx)) > 1 and len(set(fy)) > 1:
                    actual_x, actual_y = fallback_x, fallback_y
                    x_vals, y_vals = fx, fy
                    break
        else:
            # 全部兜底都退化 — 数据真的一致
            return (f"(chart degenerate: all {x_metric}={x_vals[0]}, "
                    f"{y_metric}={y_vals[0]} across {len(valid)} results — "
                    f"cpu_sim stub 输出恒定, 实装 PicolibcHostMemory 后会有变化)")

    # 3. 计算归一化坐标 (含两端点, 避免边界 0-width)
    min_x, max_x = min(x_vals), max(x_vals)
    min_y, max_y = min(y_vals), max(y_vals)
    span_x = max_x - min_x if max_x > min_x else 1.0
    span_y = max_y - min_y if max_y > min_y else 1.0

    # 4. 初始化网格 (height 行 × width 列)
    grid = [[" "] * width for _ in range(height)]

    # 5. 画所有点 (.)
    for r in valid:
        x = int((r[actual_x] - min_x) / span_x * (width - 1))
        y = int((r[actual_y] - min_y) / span_y * (height - 1))
        # clamp 到合法范围 (防御 float 边界)
        x = max(0, min(width - 1, x))
        y = max(0, min(height - 1, y))
        grid[height - 1 - y][x] = "."

    # 6. 画 Pareto 前沿 (*) — 覆盖 .
    pareto_valid = [r for r in pareto_front if actual_x in r and actual_y in r]
    for r in pareto_valid:
        x = int((r[actual_x] - min_x) / span_x * (width - 1))
        y = int((r[actual_y] - min_y) / span_y * (height - 1))
        x = max(0, min(width - 1, x))
        y = max(0, min(height - 1, y))
        grid[height - 1 - y][x] = "*"

    # 7. 格式化输出 (含 y/x 轴标签)
    chart_lines = ["".join(row) for row in grid]
    # 顶部 + 底部标签 (cycles 是 x 轴, ipc 是 y 轴)
    header = f"[PARETO-CHART] {actual_y} (Y) vs {actual_x} (X) | . = all, * = Pareto front | n={len(valid)} (front={len(pareto_valid)})"
    footer = (f"           x: {actual_x} ∈ [{min_x:g}, {max_x:g}]  |  "
              f"y: {actual_y} ∈ [{min_y:g}, {max_y:g}]")
    if (actual_x, actual_y) != (x_metric, y_metric):
        footer += f"  |  (fell back from {x_metric}/{y_metric} due to degenerate axes)"
    return "\n".join([header] + chart_lines + [footer])


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

    # M5.15 §6.7: ASCII chart 在 Pareto front 算出后, "Top configurations:" 前输出
    print()
    print(render_ascii_chart(pareto, results))
    print()

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