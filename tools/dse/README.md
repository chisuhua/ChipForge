# DSE Tools (Design Space Exploration)

> **🔵 阻塞中 (2026-06-17)**: `sweep_driver.py` 和 `pareto_analyzer.py` 已创建 (Python 3 脚本可被 import / --help / 单元测试),但**端到端运行阻塞于 `cpu_sim` 二进制** (M5.16 任务)。
>
> **当前可用**:
> - ✅ `sweep_driver.py --help` (CLI 解析)
> - ✅ `pareto_analyzer.py --help` (CLI 解析)
> - ✅ `sweep_config.example.json` 静态校验
> - ✅ Pareto 算法可被单元测试 (单测可绕开 `cpu_sim` 直接构造 mock results)
>
> **不可用**:
> - ❌ `sweep_driver.py` 调用 `cpu_sim` 实际跑仿真 (依赖 M5.16 实施)
> - ❌ 真实 sweep 输出 (`results/sweep.json` 为空)
>
> **状态**: 🔵 待 M5.16 实施 (见 [`../../ip/cpu/docs/dse_architecture.md`](../../ip/cpu/docs/dse_architecture.md) §9 Phase E)
> **创建**: 2026-06-17 (dse_architecture.md v1.0)

CPU 架构设计空间探索工具集 — 配合 `ip/cpu/cpu_factory.h` 的 `build_cpu()` 批量跑不同配置, 收集性能指标, 计算 Pareto 前沿。

## 工具列表

| 脚本 | 作用 | 依赖 |
|------|------|------|
| `sweep_driver.py` | 笛卡尔积展开 CPUConfig, 批量调用 `cpu_sim` 二进制, 收集指标到 JSON | `cpu_sim` 二进制 (M5.16 实施) |
| `pareto_analyzer.py` | 给定 sweep 结果, 计算多目标 Pareto 前沿 | `sweep_driver.py` 输出 |
| `sweep_config.example.json` | sweep 空间配置示例 | — |

## 快速开始

```bash
# 1. 编译 cpu_sim (需 M5.16 实施)
cmake --build build --target cpu_sim

# 2. 跑默认 sweep (576 configs, 单线程)
python3 tools/dse/sweep_driver.py --cpu-sim ./build/sim/cpu_sim --output results/sweep.json

# 3. 4 线程并行 sweep
python3 tools/dse/sweep_driver.py --parallel 4 --output results/sweep_parallel.json

# 4. 自定义 sweep 空间
python3 tools/dse/sweep_driver.py \
  --space '{"pipeline_stages":[3,5,7], "btb_entries":[16,64,256], "ext_m":[false,true]}' \
  --output results/custom_sweep.json

# 5. 分析 Pareto 前沿
python3 tools/dse/pareto_analyzer.py \
  --input results/sweep.json \
  --maximize ipc \
  --minimize cycles wall_clock_sec \
  --output results/pareto.json \
  --top 10
```

## 工作流

```
       ┌─────────────────────┐
       │  sweep_config.json  │
       │  (param → list)     │
       └──────────┬──────────┘
                  ▼
       ┌─────────────────────┐
       │  sweep_driver.py    │  笛卡尔积展开
       │  gen_configs()      │  ─────────► N 个 CPUConfig
       └──────────┬──────────┘
                  ▼
   ┌──────────────────────────────────┐
   │  for cfg in configs:             │
   │    subprocess.run(cpu_sim, ...)  │  并行 (--parallel N)
   │    parse stdout → metrics        │
   └──────────────┬───────────────────┘
                  ▼
       ┌─────────────────────┐
       │  results/sweep.json │  所有 metrics 汇总
       └──────────┬──────────┘
                  ▼
       ┌─────────────────────┐
       │ pareto_analyzer.py  │  Pareto 前沿计算
       │ compute_pareto()    │
       └──────────┬──────────┘
                  ▼
       ┌─────────────────────┐
       │ results/pareto.json │  不可被支配的最优配置
       └─────────────────────┘
```

## sweep 输出格式

每个 CPUConfig 跑一次后,`cpu_sim` 二进制 stdout 应输出形如:

```
IPC=1.23
cycles=1000000
branch_miss_rate=2.1
cache_hit_rate=98.5
mul_latency_effective=1
wall_clock_sec=0.45
```

`sweep_driver.py` 解析这些 `KEY=VALUE` 行到 `sweep.json`,每个 entry 结构:

```json
{
  "config": { "pipeline_stages": 5, "btb_entries": 64, "branch_predictor": "gshare", ... },
  "config_name": "dse_pipeline_stages=5_btb_entries=64_...",
  "ipc": 1.23,
  "cycles": 1000000,
  "branch_miss_rate": 2.1,
  "cache_hit_rate": 98.5,
  "wall_clock_sec": 0.45
}
```

## 设计依据

- `ip/cpu/docs/dse_architecture.md` §8 — DSE Sweep 工具设计
- `ip/cpu/configs/cpu_params_schema.json` — CPUConfig 字段定义
- `ip/cpu/configs/cpu_superscalar.json` + `cpu_deep_pipeline.json` — DSE 配置示例