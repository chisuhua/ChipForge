---
initiative: wave5-isa-coverage-and-bp
priority: P0
version_target: v0.11.0
status: placeholder
depends_on:
  - wave5-isa-coverage-and-bp-defer-mfc  # parent handoff
---

# mfc-defer-v0.11.0 — mfc-cpu-pipeline-multi-cycle-fsm Defer to v0.11.0 (Defer + Handoff)

## Why

`mfc-cpu-pipeline-multi-cycle-fsm` (mfc) 是 v0.10.0 wave5-isa-coverage-and-bp 主变更, 47/60 tasks 完成。但实测 (commits 722eca6 + 76ac53f + c4035e5) 暴露 3 个独立 root cause, 3-6 周估时超 v0.10.0 budget:

1. **advance_fsm ad-hoc busy counter** — `ip/cpu/arch/riscv/mul_div_fsm.h:475-500` 实装是 ad-hoc busy++ counter, 不是真 radix-2 iterative (注释写的"实装真 radix-2"未实现)。vendor `[riscv-tests][rv32um]` 8/8 FAIL (cycles=46 exit_code=1)。
3. **CPU pipeline 5-stage flush + 无 BTB 分支预测** — Dhrystone LEGACY + FSM 100M cap 仍 timeout, IPC ~0.05, DMIPS/MHz 0.0351 vs hard gate 1.4 (差 40x)。

Oracle 双复审 (2m39s Metis plan + 19m20s Oracle tech, 2026-10-07) 结论: D' (defer mfc → v0.11.0) 最优, v0.10.0 launch 不阻塞。

## What Changes

mfc 在 v0.10.0 launch 期间保持 47/60 partial advance (含 A'+C'+B2 fix 等关键 commits), 剩余 13 task (E.5 vendor rv32um + G Dhrystone + D.4 fsm.h extract + F stall IPC + H archive) 推迟到 v0.11.0。

### v0.10.0 launch 范围 (现状, 已 ship)
- ✅ **A' (Oracle root cause fix)**: `ip/cpu/arch/riscv/mul_div_fsm.h` 删 negotiate() requires (commit 722eca6), `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS
- ✅ **C' (100M cap livelock confirmed)**: kMaxCycles 10M→100M (commit 76ac53f), DMIPS 0.0351 (commit c4035e5)
- ✅ **B2 fix** (commit b8f4769): at_stage 闭包 `state_==IDLE` 守卫, `mul_fsm` tohost=1 PASS
- ✅ **7stage stage_count baseline 40→41** (commit 81f3c82): v1 mmufault-handler archive 触发
- ✅ **v1 cpu-pipeline-mmufault-handler archive** (commits 4429896 等): MFC + StageLink check 之外的链路

### v0.11.0 推迟 scope (follow-up: mfc-extract-fsm + wave5-bp/wave6)
1. **advance_fsm 真 radix-2 iterative** (1-2 周, `mfc-extract-fsm` follow-up): 32 cycle quotient bit-by-bit + 1 cycle write-back
2. **fetch stall framework 扩展** (1-2 周, 类似 HazardPlugin RAW stall 模式, ADR-045+082): 阻止新指令进 EX 而非 stall EX 本身
3. **BTB 分支预测** (2-4 周, `wave5-bp`/`wave6` scope): 5-stage flush 减少 ~50%

总计 3-6 周, 超 v0.10.0 budget (8+14天), 推到 v0.11.0 重启 mfc。

### mfc-defer-v0.11.0 change 范围 (当前 change)
本 change 不实装新功能, 仅 scope handoff doc + 写真实 evidence + v0.11.0 启动清单。实施方:
- (a) 写真实 evidence file (引用 commit hashes + baseline matrix)
- (b) v0.11.0 mfc 重启 checklist
- (c) v0.11.0 wave6 关联 change 推荐 (mfc-extract-fsm + wave5-bp)

## Out of Scope

- **不修改**: mfc tasks.md 当前 47/60 checkbox (保留作为 partial advance 证据)
- **不实施**: advance_fsm 真 radix-2 (推迟到 v0.11.0)
- **不实施**: fetch stall framework 扩展 (推迟到 v0.11.0)
- **不实施**: BTB 分支预测 (推迟到 wave5-bp/wave6)

## v0.11.0 启动 checklist (mfc 重启 entry point)

当 v0.11.0 启动时:
1. 创建 follow-up change `mfc-extract-fsm-restore` 引用本文档
2. 实装 advance_fsm 真 radix-2 iterative (1-2 周)
3. 跑 vendor rv32um 8/8 PASS (Phase E.5 实成)
4. 实装 fetch stall framework 扩展 (1-2 周)
5. 跑 mfc-extract-fsm baseline matrix (mfc-extract-fsm 实成)
6. archive mfc-cpu-pipeline-multi-cycle-fsm 当前 47/60 partial advance

## Acceptance

本 change 验证现有 mfc 47/60 partial advance + handoff doc 完整。
- ✅ commit 722eca6 (negotiate fix) → div_fsm tohost=1 PASS
- ✅ commit 76ac53f (test cap 100M) → livelock 确认
- ✅ commit c4035e5 (baseline matrix 写真实)
- ✅ dhrystone-baseline-matrix.csv (lifecycle evidence)
- ⏸ vendor rv32um 8/8 PASS → v0.11.0
- ⏸ DMIPS/MHz ≥1.4 → v0.11.0 / wave5-bp