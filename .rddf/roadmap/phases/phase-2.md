---
id: phase-2
kind: phase
status: active
phase_refs: []
主题: v0.8.0 Wave 3 MMU 实内存 + 周期精度
---

# Phase 2: v0.8.0 Wave 3 MMU 实内存 + 周期精度

**目标**: P1#3 mmu-paddr-consume + P1#4 cycle-precision + P1#5 multi-cycle + P1#6 mmu-config-json

**状态**: 🟡 部分收官 (2026-10-08)

## 完成条件
- [x] P1#3 `mmu-paddr-consume-and-real-memory` archive (commit 8a14402)
- [x] P1#4 `plugin-framework-cycle-precision` archive (2026-10-08, Phase F optional 降级)
- [x] P1#6 `mmu-config-json-driven` archive (2026-10-08, 由 mmu-chmem 承担)
- [ ] P1#5 `multi-cycle` → 被 wave5 `mfc-cpu-pipeline-multi-cycle-fsm` supersede (50/60 IN_PROGRESS)

## 完成标准 (phase-2 → phase-3 推进)
- [ ] `mfc-cpu-pipeline-multi-cycle-fsm` Phase H archive (50/60 → 60/60)
- [ ] [riscv-tests] rv32um 8/8 PASS

## 关联
- OpenSpec: `wave3-mmu-real-memory-and-cycle` initiative
- 版本节点: v0.8.0
