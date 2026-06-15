# M5 — 联调 + 文档收尾

> **本文件位置**: `ip/cpu/docs/implementation-plan/M5-verification.md`
> **状态**: 🟡 待启动 (依赖 M4 完成)
> **估算**: 1-2 d
> **总体任务清单**: 见 [`README.md` §6 M5 行](README.md)

## 1. 目标

把 CPU + L1CachePlugin + PicolibcHostMemory 联调成完整 SoC, 跑通 4-6 个基础 RISC-V ELF (add/sub/and/or/sll/srli), 文档收尾, ADR 起草, git tag。这是"v2.0 → v3.0 实战验证"的关门阶段。

## 2. 任务清单

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

## 3. 依赖

- ✅ M4 完成 (CpuFactory + JSON + 集成测试)
- ✅ L1CachePlugin (Phase 1.2 已落地)
- ✅ PicolibcHostMemory (M4 实施)

## 4. 完成判据

- [ ] M5.1-M5.3 联调架构 + 4-6 ELF + 集成测试 PASS
- [ ] M5.4-M5.7 文档 + ADR 收尾
- [ ] M5.8 git tag 创建
- [ ] M5.9 全部 9 项验收通过:
  - 4-6 个 RISC-V ELF 跑通 (add/sub/and/or/sll/srli)
  - 16/16 ctest 不退化
  - D4 (Plugin-style) 3+4/3 PASS
  - ADR-040 3+4/3 PASS

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 联调时 CPU ↔ L1Cache 桥接接口不匹配 | M4 阶段先验证桥接; M5 启动前做小规模冒烟测试 |
| 6 个 ELF 跑通率低 (漏指令 / 异常处理) | M3 阶段已覆盖 RV32I 全指令; M5 遇问题先回退到 1-2 个 ELF (add/sub) |
| ADR 起草被驳回 | M5 启动前先有 ADR 草案 (从 `.omo/drafts/` 提); 不要在最后关头写 |
| 文档收尾被遗忘 (M5.4-M5.7 容易跳过) | M5 启动时即在 status.md 创建 M5.4-M5.7 任务, 强制每周 review |

## 6. 任务编号约定

`M5.x` 其中 x = 1..9 (与本文件 §2 表格 # 列对应)

## 7. v2.0 收尾后状态

M5 完成即 v2.0 文档从"Accepted (待实施)" 升级为 "Accepted (已实施)"。**status.md** 的"v2.0 文档状态"字段从 🟡 → 🟢。

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M5 行
- 联调架构: [`README.md`](README.md) §7
- 任务状态: [`../status.md`](../status.md)
- 决策入口: [`../cpu_implementation_guide_v2.0.md`](../cpu_implementation_guide_v2.0.md)
