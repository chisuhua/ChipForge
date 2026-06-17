#!/usr/bin/env python3
"""
DSE Sweep Driver — 笛卡尔积展开 CPUConfig, 批量调用 cpu_sim, 收集指标

用法:
  ./sweep_driver.py --cpu-sim ./build/sim/cpu_sim --cycles 1000000
  ./sweep_driver.py --space '{"pipeline_stages":[3,5], "btb_entries":[16,64]}'
  ./sweep_driver.py --parallel 4 --output results/sweep.json

设计文档: ip/cpu/docs/dse_architecture.md §8.3
"""
import argparse
import itertools
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

DEFAULT_DSE_SPACE = {
    "pipeline_stages":     [3, 5, 7, 10],
    "btb_entries":         [16, 64, 256],
    "branch_predictor":    ["static", "gshare", "tournament"],
    "ext_m":               [False, True],
    "mul_latency":         [1, 3],
    "isa":                 ["rv32i", "rv64i"],
}


def gen_configs(space):
    """笛卡尔积展开 sweep 空间为 CPUConfig 列表"""
    keys = list(space.keys())
    for combo in itertools.product(*[space[k] for k in keys]):
        cfg = dict(zip(keys, combo))
        cfg["name"] = "dse_" + "_".join(
            f"{k}={v}" for k, v in cfg.items() if k != "name"
        )
        # 设置默认值 (与 cpu_default.json 一致)
        cfg.setdefault("clock_freq_mhz", 100)
        cfg.setdefault("enable_branch_predictor", True)
        cfg.setdefault("split_if_id", cfg.get("pipeline_stages", 5) >= 7)
        cfg.setdefault("merge_ex_mem", cfg.get("pipeline_stages", 5) == 3)
        cfg.setdefault("ghr_bits", 8 if cfg.get("btb_entries", 64) <= 64 else 16)
        cfg.setdefault("enable_mmu", False)
        cfg.setdefault("enable_pmp", False)
        cfg.setdefault("ext_a", False)
        cfg.setdefault("ext_f", False)
        cfg.setdefault("ext_d", False)
        cfg.setdefault("ext_zicsr", False)
        cfg.setdefault("ext_zifencei", False)
        cfg.setdefault("use_strict_scoreboard", True)
        cfg.setdefault("collect_stats", True)
        cfg.setdefault("icache_latency_cycles", 1)
        cfg.setdefault("dcache_latency_cycles", 1)
        yield cfg


def parse_metrics(stdout, cfg):
    """解析 cpu_sim stdout, 期望格式: 'KEY=VALUE KEY=VALUE ...'"""
    metrics = {"config": cfg, "config_name": cfg["name"]}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line and not line.startswith("#"):
            k, _, v = line.partition("=")
            try:
                metrics[k.strip()] = float(v.strip())
            except ValueError:
                metrics[k.strip()] = v.strip()
    return metrics


def run_simulation(cfg, cpu_sim_bin, cycles, timeout_sec=300):
    """调用 cpu_sim 二进制跑一个 config"""
    cfg_json = json.dumps({"name": cfg["name"], "type": "cpu", "params": cfg})
    start = time.time()
    try:
        result = subprocess.run(
            [cpu_sim_bin, "--config", cfg_json, "--cycles", str(cycles)],
            capture_output=True, text=True, check=True, timeout=timeout_sec,
        )
        elapsed = time.time() - start
        metrics = parse_metrics(result.stdout, cfg)
        metrics["wall_clock_sec"] = elapsed
        return metrics
    except subprocess.TimeoutExpired:
        return {"config": cfg, "config_name": cfg["name"], "error": "timeout"}
    except subprocess.CalledProcessError as e:
        return {
            "config": cfg, "config_name": cfg["name"],
            "error": f"exit_code={e.returncode}",
            "stderr": e.stderr[:500],
        }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--space", help="JSON dict of param → list (else use DEFAULT_DSE_SPACE)")
    ap.add_argument("--cpu-sim", default="./build/sim/cpu_sim", help="cpu_sim binary path")
    ap.add_argument("--cycles", type=int, default=1_000_000, help="simulation cycles per config")
    ap.add_argument("--output", default="results/sweep.json", help="output JSON path")
    ap.add_argument("--parallel", type=int, default=1, help="parallel workers (default 1 = serial)")
    ap.add_argument("--limit", type=int, default=0, help="limit total configs (0 = no limit)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    # 解析 sweep 空间
    space = json.loads(args.space) if args.space else DEFAULT_DSE_SPACE
    configs = list(gen_configs(space))
    if args.limit > 0:
        configs = configs[:args.limit]
    print(f"[SWEEP] Total configs: {len(configs)}")
    print(f"[SWEEP] Sweep space: {json.dumps(space, indent=2)}")
    print(f"[SWEEP] cpu_sim: {args.cpu_sim}, cycles: {args.cycles}")

    # 跑 sweep
    results = []
    if args.parallel > 1:
        with ProcessPoolExecutor(max_workers=args.parallel) as executor:
            futures = {
                executor.submit(run_simulation, cfg, args.cpu_sim, args.cycles): cfg
                for cfg in configs
            }
            for future in as_completed(futures):
                cfg = futures[future]
                metrics = future.result()
                results.append(metrics)
                if args.verbose:
                    print(f"[SWEEP] {cfg['name']}: {metrics.get('ipc', 'N/A')}")
    else:
        for i, cfg in enumerate(configs):
            print(f"[SWEEP] [{i+1}/{len(configs)}] {cfg['name']}")
            metrics = run_simulation(cfg, args.cpu_sim, args.cycles)
            results.append(metrics)
            if args.verbose or "error" not in metrics:
                print(f"  → IPC={metrics.get('ipc', 'N/A')}, "
                      f"cycles={metrics.get('cycles', 'N/A')}, "
                      f"wall={metrics.get('wall_clock_sec', 'N/A')}s")

    # 写输出
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(results, indent=2))
    print(f"[SWEEP] {len(results)} configs done → {args.output}")

    # 简单统计
    errors = sum(1 for r in results if "error" in r)
    print(f"[SWEEP] Errors: {errors}/{len(results)}")


if __name__ == "__main__":
    main()