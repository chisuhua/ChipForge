## Context
M4G (commit 89892aa, 2026-06-19) shipped D.1-D.4 as the first-stage forward-compat locks for OoO/Superscalar/SMT readiness, at ~108 LOC header churn. D.1 relaxed `READ_*_PAYLOAD` to `READ_*_PAYLOAD_FROM_PIPE`, D.2 relaxed `get/set_xlen` to virtual, D.3 made MUL latency a registered callback, D.4 added `tid` parameter to `at_stage` callbacks. The architectural review (`post-m4g-strategic-decision-2026-06-20.md`) identified 3 residual gaps that did not justify inclusion in M4G but will block M5-DSE 2-wide superscalar work.

Gap A is the most consequential: D.4's `at_stage` API exposes a `tid` parameter, but in 4 plugins (reg_file.h:146 decode read, reg_file.h:165 writeback write, hazard.h:150 build callback, branch_predictor.h:110 update closure), the closures capture `tid=0` as a constant. The factory never dispatches a real tid. When M5-DSE attempts SMT or per-thread superscalar, every one of these closures will silently route to thread 0's regfile/hazard-unit/BTB.

Gap B is documentation-only but high-value: `pipe_builder.h:104-138` already implements the OoO commit primitive via `register_commit_hook` + `commit_storages` + `CtrlLink::flush_when`. None of it is labeled. Phase 5 designers reading the file will reinvent the mechanism (the same mistake BOOM/ChampSim made in their early versions).

Gap C is naming convention: `multi_isa_architecture.md §2.4` lists 5 stages (FETCH/DECODE/EXECUTE/MEM/WB). Adding a 6th `COMMIT` row with the note "in-order 隐含; OoO 显式" locks the stage name so Phase 5 doesn't have to choose between `at_stage("retire")` vs `at_stage("commit")`.

The open-source evidence (`multi_isa_o3_evidence.md`, `chipforge_v2_locks_research.md`) confirms every plugin-pipeline CPU (BOOM, ChampSim, XiangShan) regrets these three omissions in their early versions; locking them now at ~80 LOC is the cheapest possible insurance.

## Goals / Non-Goals
**Goals**: 3 gaps locked; 0 behavior change; 1-2 day effort; ~80 LOC; M5-DSE ready
**Non-Goals**: ROB/IQ/PRF/LSQ/Rename; 2-wide superscalar implementation; READY payload; XLEN static_assert relaxation; D.5 stage-name members

## Decisions
### Decision 1: tid plumbing via factory-side dispatch (not per-plugin-construction)
**选择**: Plugins get `set_tid(uint8_t)` method; `cpu_factory::build_cpu` loops `tid=0..N_THREADS-1` to call set_tid + run() per thread.
**理由**: Plugins are static singletons; per-construction threading is impossible without rewriting plugin signatures. Factory-side dispatch is the minimum-blast-radius change.
**替代方案**: Per-call `tid` parameter in `at_stage` callbacks — rejected because at_stage is currently `void()` and adding a parameter would break all 11 plugins' build() methods.

### Decision 2: commit_hook documentation as the OoO commit primitive
**选择**: 6 lines in `pipe_builder.h:104-138` calling out commit_hook as the OoO commit primitive and `CtrlLink::flush_when` as the mispredict-squash primitive.
**理由**: Zero code change; prevents Phase 5 designers from reinventing the mechanism.
**替代方案**: Leave undocumented — rejected because every plugin-pipeline CPU (BOOM, ChampSim) regrets this omission.

### Decision 3: COMMIT stage added to 5-stage list as a "naming convention"
**选择**: Add 6th stage `COMMIT` to `multi_isa_architecture.md §2.4` with note "in-order 隐含; OoO 显式".
**理由**: Zero code; naming convention prevents `at_stage("retire")` vs `at_stage("commit")` fragmentation in Phase 5.
**替代方案**: Stage name deferred until Phase 5 — rejected because early naming is cheap.

### Decision 4: Do NOT add READY payload, XLEN static_assert relaxation, or D.5 stage-name members
**选择**: Stay with M4G scope. Only the 3 plumbing/doc gaps are added.
**理由**: Oracle 2026-06-17 explicitly rejected speculative dead code (D.8/D.9); READY payload has 0 consumers; XLEN is cross-ISA scope, not OoO; D.5 was rejected as wrong abstraction.
**替代方案**: Add all of them — rejected per Oracle verdict.

## Risks / Trade-offs
### Risk 1: set_tid pattern may not scale to 2-wide superscalar
- **缓解**: set_tid is a plugin-level method; factory can call set_tid(0) + run() + set_tid(1) + run() for SMT, or set_tid(0..N_LANES-1) per cycle for superscalar. Pattern is general.
- **概率**: 低
- **回退**: If 2-wide needs per-lane tid instead of per-thread, refactor to per-PipeNode tid.

### Risk 2: ctest 36/36 may break due to tid loop changing execution order
- **缓解**: For N_THREADS=1 (default), tid loop runs set_tid(0) + run() once — identical to current behavior. Verify with ctest before commit.
- **概率**: 低
- **回退**: If ctest breaks, guard tid loop with `if (n_threads > 1)`.

### Risk 3: Documentation-only changes may be skipped during Phase 5 implementation
- **缓解**: Cite `pipe_builder.h:104-138` comment in `dse_architecture_v2_design_research.md` Phase 5 section. Add to v2-locks §7.3 演化规则.
- **概率**: 中
- **回退**: Re-validate at Phase 5 kickoff.

## Migration Plan
1. **本 change 实施** (1-2 days, 1 person):
   - Edit 4 plugins: reg_file.h, hazard.h, branch_predictor.h, add set_tid
   - Edit cpu_factory.h: tid loop
   - Edit pipe_builder.h: documentation
   - Edit multi_isa_architecture.md: COMMIT stage
   - Verify: ctest 36/36, verify_adr.sh, verify_no_ghost_refs.sh
2. **commit 1**: ~80 LOC across 6 files
3. **M5-DSE kickoff precondition**: M5-DSE 2-wide channel work MUST reference this change as prerequisite

## Open Questions
1. Should `set_tid` be on `PluginBase` (virtual) or on each plugin separately? — Current decision: virtual on PluginBase (uniform interface); revisit if 3rd-party plugins break.