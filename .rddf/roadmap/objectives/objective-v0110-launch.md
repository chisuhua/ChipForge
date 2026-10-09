---
id: objective-v0110-launch
status: active
created: 2026-10-08
last_revised: 2026-10-08
review_by: 2027-01-06
owner: rdd-planner
priority: P0
manual_deps:
  - objective-mmu-chmem-phase-b-e
supersedes: null
theme: v0.11.0 launch gate — Wave 5 收官
---

# Objective: objective-v0110-launch

## 1. 驱动诊断（Why now）

Wave 5 (v0.10.x-v0.11.0) 是 ISA 覆盖 + 分支预测收官窗口。当前债务:

1. **rv32um 0/8 FAIL** — `ip/cpu/arch/riscv/mul_div_fsm.h:475-500` `advance_fsm` 是 ad-hoc busy counter 非真 radix-2 iterative (AGENTS.md L36 记录, defer v0.11.0)
2. **dhrystone livelock** — [cpu-integration][dhrystone] 60s+ timeout, CPU mul/div FSM 问题 (commit 76ac53f "确认 livelock")
3. **MMU CH_MEM owner 真空** — `cpu_factory_chmem.h:80-85` 自证 "Real sv32 CH_MEM owner TBD", mmu-chmem-pipeline-integration Phase A 已 commit (a3afcfa) 但 B-E 待实施
4. **DMIPS/MHz 0.0351 距硬门禁 1.4 差 40x** — 需真 radix-2 + fetch stall + BP 协同

## 2. 目标愿景 + 完成判据

v0.11.0 launch 全绿 (HANDOFF §10 退出标准 + AGENTS.md 已知测试状态):

| 判据 | 当前 | 目标 |
|------|------|------|
| [riscv-tests] | 40/48 (rv32um 0/8) | **48/48** |
| [mmu] | 59/59 (Phase A 后) | 维持 |
| [cpu-l1-mmu-demo] | 6/6 | **7/7** (workaround 移除) |
| [mmu-verilator] | 3/3 (plumbing only) | **5/5** (真 sv32) |
| [cpu-integration] | 85 PASS + 8 rv32um FAIL + dhrystone livelock | **全绿** |
| DMIPS/MHz | 0.0351 | **≥ 1.4** (硬门禁) |
| [cpu] | 19/19 | 维持 |

## 3. 架构依据

- ADR-083: Plugin-style 单一 source of truth (业务 IP 双编译模式 TLM/CH_MEM)
- ADR-082: negotiate 集成 (mfc Phase G)
- ADR-046: 多周期 FSM 豁免 (MUL/DIV FSM 化, `CF_PLUGIN_USE_FSM_EXEMPT`)
- ADR-040 v2.0: CH_MEM 是新正道 (mmu-chmem Phase B-E)
- RISC-V Spec §4.3.1: satp CSR encoding (mmu-chmem Phase A D4-A/D5-TLM)

## 5. 反例

- 非目标: 不在 v0.11.0 窗口做 Sv39/48 MMU 完整翻译 (D1 收窄)
- 非目标: 不做多级 TLB / LRU-RRIP / ASID≠0 (D1 out-of-scope)
- 非目标: 不引入 CSR plugin (CH_MEM 物理不可能, Oracle R11)

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-mmu-chmem-phase-b-e` — mmu-chmem Phase B-E 完成 (CH_MEM MMU 集成)
- `mfc-cpu-pipeline-multi-cycle-fsm` Phase H archive (50/60 → 60/60)
- `mfc-extract-fsm-h` 真 radix-2 实装 (rv32um + dhrystone 修)

### 9.2 Go / No-Go Decision Gate
- **Go 条件** (全部满足):
  - [ ] [riscv-tests] 48/48
  - [ ] [mmu] 59/59 + [cpu-l1-mmu-demo] 7/7
  - [ ] [mmu-verilator] 5/5
  - [ ] DMIPS/MHz ≥ 1.4
  - [ ] 4 architecture gates 0 error
  - [ ] `bash tools/run_chipforge_tests.sh --all` 全 PASS
- **No-Go 触发** (任一):
  - DMIPS/MHz < 1.4 且无明确修复路径
  - rv32um 仍有 FAIL
  - PoC-6 CoreMark 无 BP 支撑 (< 1.9)

## 10. next_sprint_candidates
- `mfc-extract-fsm-restore-radix2` — 真 radix-2 advance_fsm (1-2 周, 修 rv32um + dhrystone)
- `bp-btb-coordination-3items` — pc_reg mux / stall / fetch stage 协议 (0.5d, Phase C 入口)
- `mmu-chmem-phase-b` — MMUPlugin CH_MEM 实装 (10d, HANDOFF §6)

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建 | — | initial |

