# CPU DSE Research Directory

This directory contains research reports and surveys that inform the CPU Design Space Exploration (DSE) architecture.

## Documents

### Architecture & Design

| Document | Description | Date | Status |
|----------|-------------|------|--------|
| [dse_architecture.md](../dse_architecture.md) | DSE architecture v1.0 — current implementation plan | 2026-06-17 | ✅ Implemented |
| [dse_architecture_v2_locks.md](../dse_architecture_v2_locks.md) | DSE v2.0 Phase 1 forward-compatibility locks (D.1-D.4, Oracle-approved) | 2026-06-17 | 🟢 Accepted for Phase 1 |
| [dse_architecture_v2_design_research.md](../dse_architecture_v2_design_research.md) | DSE v2.0 design research — gem5/BOOM/XiangShan/Chipyard/SMT surveys + Phase 5+ proposals | 2026-06-17 | 🔵 Reference (Phase 5+ prep) |

### Reference Surveys

| Document | Description | Date | Source |
|----------|-------------|------|--------|
| [gem5_dse_reference.md](../gem5_dse_reference.md) | gem5 DSE patterns and architectural insights | 2026-06-17 | Background agent bg_9668b73e |
| [ooo_forward_compat_gap_analysis.md](../ooo_forward_compat_gap_analysis.md) | OoO/Superscalar/SMT forward-compatibility gap analysis | 2026-06-17 | Background agent bg_45968b90 |
| [dse-open-source-riscv-survey.md](../../../docs/research/dse-open-source-riscv-survey.md) | Open-source RISC-V CPU DSE survey (BOOM, XiangShan, etc.) | 2026-06-17 | Background agent bg_2d204563 |
| [smt-interface-design-survey.md](./smt-interface-design-survey.md) | SMT (Simultaneous Multithreading) interface design survey | 2026-06-17 | Background agent bg_41470f52 |
| [dse-framework-survey.md](./dse-framework-survey.md) | CPU DSE frameworks and methodology survey | 2026-06-17 | Background agent bg_a592c1cb |

## Research Coverage

These documents were produced by 5 parallel research agents investigating:

1. **gem5 OoO Architecture** — ROB, IQ, LSQ, Rename, and SMT implementation patterns
2. **Open-source RISC-V CPUs** — BOOM, XiangShan, CHIP, and other academic designs
3. **SMT Interface Design** — Intel HT, AMD Zen, IBM POWER, and SPARC T-series
4. **OoO Forward Compatibility** — Gap analysis for current ChipForge architecture
5. **DSE Frameworks** — Academic tools (Mishra/EXPRESSION, OpenDSE, ESESC, Sniper) and industrial tools (SiFive, Cadence Tensilica, Synopsys ARC)

## Key Insights

- **Forward-compatibility is critical**: ~150 lines of Phase 1 decisions can prevent ~2000 lines of Phase 5+ refactoring
- **Framework spine is OoO-friendly**: VexRiscv-style declarative plugin model doesn't need major changes for OoO
- **Predictive modeling is the most leveraged DSE technique**: 3-4 orders of magnitude speedup over full simulation
- **SMT requires explicit design**: ThreadContext as first-class citizen, per-thread predictor state, and resource partitioning policies
- **gem5 patterns are transferable**: Parameterization, branch predictor factory, and SMT queue policies can inform ChipForge DSE

## Related Documentation

- [CPU IP Documentation Index](../README.md)
- [Implementation Plan](../implementation-plan/README.md)
- [Status Dashboard](../status.md)
