# M5 — 联调 + 文档收尾

> **本文件位置**: `ip/cpu/docs/implementation-plan/M5-verification.md`
> **状态**: 🟢 v2.0 子任务 9/9 完成; 🟡 M5-DSE 子阶段 (M5.10-M5.19) 待启动
> **估算**: 1-2 d (v2.0) + 1.5 周 (M5-DSE)
> **总体任务清单**: 见 [`README.md` §6 M5 行](README.md)
> **DSE 详细设计**: 见 [`../dse_architecture.md`](../dse_architecture.md)

## 1. 目标

把 CPU + L1CachePlugin + PicolibcHostMemory 联调成完整 SoC, 跑通 4-6 个基础 RISC-V ELF (add/sub/and/or/sll/srli), 文档收尾, ADR 起草, git tag。这是"v2.0 → v3.0 实战验证"的关门阶段。

**M5-DSE 子阶段**: 在 M5 v2.0 基础上, 完成流水线深度 3/5/7/10 级的真实展开 + MUL 子流水 + Python sweep 工具链, 实现可执行的 CPU 架构 DSE。

## 2. 任务清单 (v2.0)

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M5.1** | 联调架构搭建: `soc/cpu_l1_picolibc/demo.json` 配置 cpu + l1 + host_mem | `soc/cpu_l1_picolibc/demo.json` | SoC 实例化通过 | 0.2d |
| **M5.2** | 编译 4-6 个 RISC-V ELF: add.S / sub.S / and.S / or.S / sll.S / srli.S | `ip/cpu/tests/manual_elf/` | 6 个 .elf 编译通过 | 0.2d |
| **M5.3** | 实施 `tests/integration/test_demo_soc.cpp` (联调 6 个 ELF) | `ip/cpu/tests/integration/` | 6/6 tohost=1 PASS | 0.4d |
| **M5.4** | 修订 `ip/cpu/README.md` (M1-M5 实施后现状) | `ip/cpu/README.md` | 文档完整 | 0.1d |
| **M5.5** | 修订 `ip/cpu/docs/README.md` (索引更新) | `ip/cpu/docs/README.md` | 索引完整 | 0.05d |
| **M5.6** | 起草 ADR-XXX: Plugin 推迟决策 (FPU/MMU/Exception 推迟到 Phase 5+) | `docs/architecture/adr/ADR-XXX-*.md` | ADR Accepted | 0.2d |
| **M5.7** | 起草 `docs/lessons/m1-m5-cpu-implementation.md` (M1-M5 实施复盘) | `docs/lessons/` | lessons 文档存在 | 0.2d |
| **M5.8** | 最终 `git commit` + `git tag phase-1.5-cpu-v2.0-2026-MM-DD` | — | tag 创建 | 0.05d |
| **M5.9** | 4-6 ELF 全 PASS + 16/16 ctest 不退化 + D4 + ADR-040 3+4/3 PASS | — | 全部验收通过 | (累计) |

## 2.1 任务清单 (M5-DSE 子阶段) — 见 dse_architecture.md §9 Phase C + D + E

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M5.10** | `TopologyBuilder::expand(cfg)` 实现 3/5/7/10 级拓扑展开 | `ip/cpu/cpu_factory.h` | 4 种深度编译通过 | 0.5d |
| **M5.11** | 3/5 级集成测试拓扑断言 (`test_3stage` / `test_5stage` 升级) | `tests/cpu/integration/test_*stage_riscv.cpp` | topology 断言 PASS | 0.3d |
| **M5.12** | 7 级集成测试 (`test_7stage_riscv.cpp` 新建) | `tests/cpu/integration/` | 7 个 Node + 拓扑 PASS + tohost=1 | 0.5d |
| **M5.13** | 10 级深流水测试 (`test_10stage_riscv.cpp` 新建) | `tests/cpu/integration/` | ≥10 Node + 拓扑 PASS + tohost=1 | 0.5d |
| **M5.14** | `RiscvMulPlugin<T, 1/3/5>` 模板实例化 + 子流水 | `ip/cpu/arch/riscv/mul.h` | mul_latency=3 比 1 慢 ≥2 cycle | 0.5d |
| **M5.15** | `tools/dse/sweep_driver.py` + `parse_results.py` + `pareto_analyzer.py` | `tools/dse/` 新建 | 跑通 100 个 config | 1d |
| **M5.16** | `cpu_sim` 二进制支持 `--config --cycles` | `cpu_sim` main | 命令行接口可用 | 0.5d |
| **M5.17** | 完整 sweep 一次 (4×3×3×2×2×2×2 = 576 config) | `tools/dse/` | 输出 results/sweep.json | 0.5d |
| **M5.18** | 新建 `cpu_superscalar.json` + `cpu_deep_pipeline.json` 示例配置 | `ip/cpu/configs/` | JSON Schema 校验通过 | 0.2d |
| **M5.19** | `cpu_params_schema.json` 加 12 个新字段 schema | `ip/cpu/configs/cpu_params_schema.json` | ajv 校验通过 | 0.3d |
| **M5-DSE 累计** | | | **0/10** | **~5 d** |

> **设计依据**: 见 [`../dse_architecture.md` §5 + §6 + §8](../dse_architecture.md)。

## 3. 依赖

- ✅ M4 完成 (CpuFactory + JSON + 集成测试)
- ✅ L1CachePlugin (Phase 1.2 已落地)
- ✅ PicolibcHostMemory (M4 实施)
- 🟡 M5-DSE 额外依赖 M4-DSE (M4.12-M4.19) 完成

## 4. 完成判据

### 4.1 v2.0 判据

- [ ] M5.1-M5.3 联调架构 + 4-6 ELF + 集成测试 PASS
- [ ] M5.4-M5.7 文档 + ADR 收尾
- [ ] M5.8 git tag 创建
- [ ] M5.9 全部 9 项验收通过:
  - 4-6 个 RISC-V ELF 跑通 (add/sub/and/or/sll/srli)
  - 16/16 ctest 不退化
  - D4 (Plugin-style) 3+4/3 PASS
  - ADR-040 3+4/3 PASS

### 4.2 M5-DSE 判据 (新增)

- [ ] M5.10: 4 种流水线深度 (3/5/7/10) 编译通过, 拓扑 Node 数符合预期
- [ ] M5.11-M5.13: 4 个集成测试 (3/5/7/10 级) 全 PASS, 各跑通 add.elf
- [ ] M5.14: `mul_latency=1/3/5` 编译通过, 性能差异符合预期
- [ ] M5.15: sweep_driver.py 跑通 100 个 config 不崩溃
- [ ] M5.16: cpu_sim 接受 `--config --cycles` 参数
- [ ] M5.17: 完整 sweep (576 config) 输出 results/sweep.json + Pareto 前沿
- [ ] M5.18: 2 个新 JSON 示例文件通过 schema 校验
- [ ] M5.19: cpu_params_schema.json 12 个新字段定义完整

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 联调时 CPU ↔ L1Cache 桥接接口不匹配 | M4 阶段先验证桥接; M5 启动前做小规模冒烟测试 |
| 6 个 ELF 跑通率低 (漏指令 / 异常处理) | M3 阶段已覆盖 RV32I 全指令; M5 遇问题先回退到 1-2 个 ELF (add/sub) |
| ADR 起草被驳回 | M5 启动前先有 ADR 草案 (从 `.omo/drafts/` 提); 不要在最后关头写 |
| 文档收尾被遗忘 (M5.4-M5.7 容易跳过) | M5 启动时即在 status.md 创建 M5.4-M5.7 任务, 强制每周 review |
| **M5-DSE**: 7/10 级流水线 RV32I 指令兼容性 | 7/10 级只对 fetch / execute 阶段加 split, 指令解码 + 写回不变, RV32I 指令应全部兼容 |
| **M5-DSE**: Python sweep 脚本依赖外部工具 (cpu_sim 二进制) | M5.16 先确保 cpu_sim 二进制可独立调用, sweep 仅做参数枚举 |
| **M5-DSE**: 576 config sweep 时间过长 | 提供 `--parallel` 选项, 用 multiprocessing 加速; 或提供 `--limit N` 限制总跑数 |

## 6. 任务编号约定

- v2.0: `M5.x` 其中 x = 1..9
- M5-DSE: `M5.x` 其中 x = 10..19 (新增)

## 7. v2.0 收尾后状态

M5 完成即 v2.0 文档从"Accepted (待实施)" 升级为 "Accepted (已实施)"。**status.md** 的"v2.0 文档状态"字段从 🟡 → 🟢。

M5-DSE 完成后, **status.md** 的"M4-DSE / M5-DSE 子阶段"行从 🔵 → 🟢。**dse_architecture.md** 文档从"待实施"升级为"已实施"。

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M5 行
- 联调架构: [`README.md`](README.md) §7
- 任务状态: [`../status.md`](../status.md) §5.1
- 决策入口: [`../cpu_implementation_guide_v2.0.md`](../cpu_implementation_guide_v2.0.md)
- **DSE 详细设计**: [`../dse_architecture.md`](../dse_architecture.md)
