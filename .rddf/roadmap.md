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
- `feat-bp-btb-gshare` — 分支预测 BTB + GShare + RAS (PoC-6 CoreMark ≥1.9) — defer → v1.0.0 (2026-10-19 决策) (refs: phase-4, phase-5)
- `feat-dhrystone-dmips-gate` — Dhrystone DMIPS/MHz ≥1.4 硬门禁 (mfc Phase G) — ✅ archived 2026-10-15 (refs: phase-4)
- `feat-mfc-multi-cycle-fsm` — MUL/DIV FSM 化 + ADR-082 negotiate 集成 (refs: phase-2, phase-4)
- `feat-mmu-chmem-integration` — CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner) (refs: phase-4)
- `feat-phase-6d-rtl-verification` — Phase 6d 5-stage Pipeline CH_MEM + Verilator + MMU/PTW FSM (跨 cpu/mmu/cache) (refs: phase-4)
- `feat-rv32um-m-extension` — RV32UM M 扩展真实 radix-2 覆盖 (rv32um 8/8) (refs: phase-4)
- `feat-rv64-isa-declaration-integrity` — RV64 ISA 声明真实性 + XLEN 就绪度门禁 (ADR-087 实施) (refs: phase-4)
- `feat-tlm-backend-retirement` — TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除) (refs: phase-4, phase-5)

<!-- AUTO-SPRINT -->

### Objectives

<!-- 手工维护段：放在 AUTO-SPRINT 之后，避免被 `rddf roadmap --update-agent-md` / refresh 的 AUTO-INDEX 生成区吞掉（AUTO-INDEX 与 AUTO-SPRINT 之间的内容由生成器重写）。根因修复（生成器应保留手工段）归 governance follow-up。 -->

- `objective-ip-cpu-tlm-backend-retirement` — TLM 后端退役编排 (B0 冻结 → B1 承载力 → B2 语义裁决 → B3 删除) (refs: phase-4, phase-5)
- `objective-ip-mmu-mmu-chmem-phase-b-e` — MMU CH_MEM Phase B–E (TLB/PTW depth) (refs: phase-4)
- `objective-soc-cpu-v0110-launch` — v0.11.0 launch gate (mfc + mmu-chmem 收官) (refs: phase-4)
- `objective-soc-cpu-v100-launch` — v1.0.0 launch gate (Wave 5 S/U + RV32A + BTB) (refs: phase-5)
- `objective-soc-cpu-v120-launch` — v1.2.0 验证基建版 (refs: phase-5)
- `objective-soc-cpu-v130-launch` — v1.3.0 产品化版 (refs: phase-5)
