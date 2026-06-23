#!/usr/bin/env python3
"""
DSE Sweep Driver — 笛卡尔积展开 CPUConfig, 批量调用 cpu_sim, 收集指标

用法:
  ./sweep_driver.py --cpu-sim ./build/src/cf_plugin/cpu_sim --cycles 1000000
  ./sweep_driver.py --space '{"pipeline_stages":[3,5], "btb_entries":[16,64]}'
  ./sweep_driver.py --parallel 4 --output results/sweep.json
  ./sweep_driver.py --seed 42 --limit 100 --output /tmp/sweep100.json

设计文档: ip/cpu/docs/dse_architecture.md §8.3
"""
import argparse
import itertools
import json
import os
import random
import subprocess
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

# M5-DSE M5.15: 默认 DSE sweep 空间 7 维度 (576 configs)
#   pipeline_stages:   4   (3/5/7/10 — embedded/default/superscalar/deep)
#   branch_predictor:  3   (static/bimodal/gshare)
#   btb_entries:       3   (16/64/256)
#   xlen:              2   (32/64 — 取代 M4-DSE 时代的 ext_m+isa 6 维空间)
#   mul_latency:       2   (1/5 — RiscvMulPlugin LATENCY=1 byte-identical / LATENCY=5 deep)
#   icache_latency:    2   (1/3 — T1 重命名 from icache_latency_cycles)
#   dcache_latency:    2   (1/3 — T1 重命名 from dcache_latency_cycles)
# 4*3*3*2*2*2*2 = 576 configs
# 旧 M4-DSE 6-dim 288-config 空间可通过 --space JSON 覆盖保留 (向后兼容)
DEFAULT_DSE_SPACE = {
    "pipeline_stages":     [3, 5, 7, 10],
    "branch_predictor":    ["static", "bimodal", "gshare"],
    "btb_entries":         [16, 64, 256],
    "xlen":                [32, 64],
    "mul_latency":         [1, 5],
    "icache_latency":      [1, 3],
    "dcache_latency":      [1, 3],
}


def gen_configs(space):
    """笛卡尔积展开 sweep 空间为 CPUConfig 列表"""
    keys = list(space.keys())
    for combo in itertools.product(*[space[k] for k in keys]):
        cfg = dict(zip(keys, combo))
        cfg["name"] = "dse_" + "_".join(
            f"{k}={v}" for k, v in cfg.items() if k != "name"
        )
        # 设置默认值 (与 cpu_params_schema.json + cpu_default.json 一致)
        # T1 重命名后字段名 (无 _cycles 后缀, 与 CpuConfig struct 对齐)
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
        # T1 重命名: icache_latency_cycles → icache_latency
        cfg.setdefault("icache_latency", 1)
        # T1 重命名: dcache_latency_cycles → dcache_latency
        cfg.setdefault("dcache_latency", 1)
        # M5-DSE M5.18 superscalar 默认值 (n_lanes/dispatch_width=1 byte-identical)
        cfg.setdefault("n_lanes", 1)
        cfg.setdefault("dispatch_width", 1)
        cfg.setdefault("issue_queue_size", 0)
        cfg.setdefault("rob_size", 0)
        cfg.setdefault("lsq_size", 0)
        cfg.setdefault("rename_table_size", 0)
        cfg.setdefault("retire_width", 1)
        cfg.setdefault("fetch_width", 1)
        cfg.setdefault("commit_width", 1)
        yield cfg


def parse_metrics(stdout, cfg):
    """解析 cpu_sim stdout, 期望格式: 'KEY=VALUE KEY=VALUE ...'"""
    metrics = {"config": cfg, "config_name": cfg["name"]}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line and not line.startswith("#"):
            k, _, v = line.partition("=")
            k = k.strip()
            # 跳过 cpu_sim 的 config=<path> 行, 避免 cfg dict 被覆盖为 path string
            if k == "config":
                continue
            try:
                metrics[k] = float(v.strip())
            except ValueError:
                metrics[k] = v.strip()
    return metrics


def run_simulation(cfg, cpu_sim_bin, cycles, elf_path=None, timeout_sec=300):
    """调用 cpu_sim 二进制跑一个 config

    cpu_sim (M5.16 / tools/cpu_sim/main.cpp) 期望 --config 是 JSON 文件路径,
    不是 inline JSON string (load_config 用 std::ifstream 打开).
    故: 写到 /tmp 临时 JSON, 跑完 unlink.

    elf_path: 可选, 加载到 PicolibcHostMemory 驱动 RV32I 解释器
             (M4.15+); 缺省时走无 --elf 路径, tohost=0 占位.
    """
    cfg_json_str = json.dumps({"name": cfg["name"], "type": "cpu", "params": cfg})
    cfg_path = None
    start = time.time()
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False, dir="/tmp",
        ) as f:
            cfg_path = f.name
            f.write(cfg_json_str)
        cmd = [cpu_sim_bin, "--config", cfg_path, "--cycles", str(cycles)]
        if elf_path:
            cmd += ["--elf", elf_path]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True, timeout=timeout_sec,
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
    finally:
        if cfg_path and Path(cfg_path).exists():
            Path(cfg_path).unlink()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--space", help="JSON dict of param → list (else use DEFAULT_DSE_SPACE)")
    # M5.16 cpu_sim 二进制实际路径 (Task 5 commit 47c6722 实施)
    ap.add_argument("--cpu-sim", default="./build/src/cf_plugin/cpu_sim",
                    help="cpu_sim binary path (M5.16 实际位置 ./build/src/cf_plugin/cpu_sim)")
    ap.add_argument("--cycles", type=int, default=1_000_000,
                    help="simulation cycles per config (默认 1M; smoke test 显式传 --cycles 100)")
    ap.add_argument("--output", default="results/sweep.json", help="output JSON path")
    # DoD: --parallel 默认 = os.cpu_count()
    ap.add_argument("--parallel", type=int, default=os.cpu_count() or 1,
                    help=f"parallel workers (默认 = os.cpu_count() = {os.cpu_count() or 1}; 设 1 串行)")
    ap.add_argument("--limit", type=int, default=0, help="limit total configs (0 = no limit)")
    # M4.15: --elf 加载 ELF 程序到 PicolibcHostMemory 驱动 RV32I 解释器
    ap.add_argument("--elf", default=None,
                    help="ELF program to load into PicolibcHostMemory (M4.15+; "
                         "缺省 = 无 --elf, tohost=0 占位)")
    # M5.15: --seed 用于 deterministic matrix (同 seed → 同顺序)
    ap.add_argument("--seed", type=int, default=0,
                    help="random seed for deterministic matrix shuffle (回归验证用, 默认 0)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    # 解析 sweep 空间
    space = json.loads(args.space) if args.space else DEFAULT_DSE_SPACE
    configs = list(gen_configs(space))

    # M5.15: 确定性矩阵 — 同 seed 产出同顺序
    # IMPORTANT: shuffle BEFORE limit, 这样不同 seed 在前 N 个 config 上有不同 ordering
    # 这样 smoke test --limit 100 --seed 0 与重跑 --limit 100 --seed 0 顺序一致
    rng = random.Random(args.seed)
    rng.shuffle(configs)
    if args.limit > 0:
        configs = configs[:args.limit]
    print(f"[SWEEP] Total configs: {len(configs)} (seed={args.seed}, limit={args.limit or 'none'})")
    print(f"[SWEEP] Sweep space: {json.dumps(space, indent=2)}")
    print(f"[SWEEP] cpu_sim: {args.cpu_sim}, cycles: {args.cycles}, parallel: {args.parallel}, elf: {args.elf or '(none)'}")

    # 跑 sweep
    results = []
    if args.parallel > 1:
        # ProcessPoolExecutor (与 multiprocessing.Pool 等价)
        # 用 idx 跟踪 + 重组, 保证 results 顺序与 cfg 提交顺序一致 (deterministic matrix)
        # 否则 as_completed 按完成顺序 yield, 顺序会因 OS 调度非确定
        with ProcessPoolExecutor(max_workers=args.parallel) as executor:
            future_to_idx = {
                executor.submit(run_simulation, cfg, args.cpu_sim, args.cycles, args.elf): i
                for i, cfg in enumerate(configs)
            }
            results_by_idx = {}
            for future in as_completed(future_to_idx):
                idx = future_to_idx[future]
                results_by_idx[idx] = future.result()
                if args.verbose:
                    cfg = configs[idx]
                    print(f"[SWEEP] [{idx+1}/{len(configs)}] {cfg['name']}: "
                          f"{results_by_idx[idx].get('ipc', 'N/A')}")
            results = [results_by_idx[i] for i in range(len(configs))]
    else:
        for i, cfg in enumerate(configs):
            print(f"[SWEEP] [{i+1}/{len(configs)}] {cfg['name']}")
            metrics = run_simulation(cfg, args.cpu_sim, args.cycles, args.elf)
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