---
id: phase-1
kind: phase
status: completed
phase_refs: []
主题: v0.7.0 Wave 3 CPU pipeline 清债
---

# Phase 1: v0.7.0 Wave 3 CPU pipeline 清债

**目标**: P0#1 canonical-ordering + P0#2 rv32ui LOAD-width 落地, 消除 CPU pipeline 债务

**状态**: ✅ done (v0.7.0 archive, 2026-09-25)

## 完成条件
- [x] P0#1 `cpu-pipeline-canonical-ordering-assert` archive
- [x] P0#2 `cpu-pipeline-fix-rv32ui-load-width` archive
- [x] [cpu-integration] canonical_ordering 测试 PASS
- [x] [riscv-tests] rv32ui 40/40 PASS

## 关联
- OpenSpec: `wave3-cpu-pipeline-debt` initiative
- 版本节点: v0.7.0 (2026-09-25 archive)
