# ChipForge Roadmap

## 元信息
- **版本**: 1
- **创建时间**: 2026-10-08
- **最后更新**: 2026-10-08
- **当前阶段**: phase-4

## Phase Skeleton

| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | v0.7.0 Wave 3 CPU pipeline 清债 | completed | 2026-09-24 | 2026-09-25 |
| phase-2 | v0.8.0 Wave 3 MMU 实内存 + 周期精度 | active | 2026-09-25 | |
| phase-3 | v0.9.0 Wave 4 Cache DSE | deferred | | |
| phase-4 | v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP | active | 2026-10-08 | |
| phase-5 | v1.0.0+ Wave 6 产品化 | deferred | | |
<!-- AUTO-INDEX -->

## Fragment Index (auto-generated)

### Phases
- `phase-1` — v0.7.0 Wave 3 CPU pipeline 清债
- `phase-2` — v0.8.0 Wave 3 MMU 实内存 + 周期精度
- `phase-3` — v0.9.0 Wave 4 Cache DSE
- `phase-4` — v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
- `phase-5` — v1.0.0+ Wave 6 产品化

### Features
- `feat-bp-btb-gshare` — 分支预测 BTB + GShare + RAS (PoC-6 CoreMark ≥1.9) (refs: phase-4)
- `feat-dhrystone-dmips-gate` — Dhrystone DMIPS/MHz ≥1.4 硬门禁 (mfc Phase G) (refs: phase-4)
- `feat-mfc-multi-cycle-fsm` — MUL/DIV FSM 化 + ADR-082 negotiate 集成 (refs: phase-2, phase-4)
- `feat-mmu-chmem-integration` — CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner) (refs: phase-4)
- `feat-phase-6d-rtl-verification` — Phase 6d 5-stage Pipeline CH_MEM + Verilator + MMU/PTW FSM (refs: phase-4)
- `feat-rv32um-m-extension` — RV32UM M 扩展真实 radix-2 覆盖 (rv32um 8/8) (refs: phase-4)

### Objectives
- `objective-v0110-launch` — v0.11.0 launch gate — Wave 5 收官 (P0)
- `objective-mmu-chmem-phase-b-e` — mmu-chmem Phase B-E CH_MEM 集成实施 (P0)

<!-- AUTO-SPRINT -->
