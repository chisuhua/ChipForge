---
id: phase-4
kind: phase
status: active
phase_refs: []
主题: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
---

# Phase 4: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP

**目标**: mfc FSM 收官 + mmu-chmem CH_MEM 集成 + rv32um 覆盖 + 分支预测 (BTB/GShare)

**状态**: 🔄 active (2026-10-08)

## 完成条件
- [ ] `mfc-cpu-pipeline-multi-cycle-fsm` Phase H archive (50/60 → 60/60)
- [ ] `mmu-chmem-pipeline-integration` archive (Phase A 已 commit a3afcfa, B-E 待)
- [ ] [riscv-tests] rv32um 8/8 PASS (当前 0/8, 真 radix-2 实装)
- [ ] [cpu-l1-mmu-demo] 7/7 PASS (workaround 移除)
- [ ] [mmu-verilator] 5/5 PASS (真 sv32 translation)
- [ ] DMIPS/MHz ≥ 1.4 (硬门禁)
- [ ] PoC-6 CoreMark/MHz ≥ 1.9 (BP 完成)

## 关联
- OpenSpec: `wave5-isa-coverage-and-bp` initiative
- 版本节点: v0.10.x → v0.11.0
- Active changes: `mmu-chmem-pipeline-integration` (6/166), `mfc-cpu-pipeline-multi-cycle-fsm` (50/60), `mfc-extract-fsm-h` (0/20)
