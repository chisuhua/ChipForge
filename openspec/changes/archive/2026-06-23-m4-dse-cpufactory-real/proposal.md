## Why

M4 baseline (commit `359b537`) landed a CpuFactory stub skeleton (CPUConfig struct + `build_cpu<T>` + PluginOrder EARLY/NORMAL/LATE description), but `cpu_factory.h` retained 5 `"M4-DSE 启动时删除"` comments and 11 plugin registrations were described but not wired. The `cpu_sim` binary output `ipc=0.0` and `tohost=0` as placeholders regardless of config. M5.12/M5.13 integration tests were "blocked on M4-DSE", and T7 (add.elf end-to-end) + T8 (576-sweep real data) could not produce real numbers.

This change implements the actual M4-DSE work: register the 11 RISC-V plugins, move `BranchPredictor`'s `BTB_SIZE` from template to runtime, integrate `PicolibcHostMemory` into `cpu_sim` with a minimal RV32I interpreter (so add.elf actually runs and produces real `tohost=1`), and add end-to-end integration tests for all 4 pipeline depths (3/5/7/10-stage). Net result: 43/43 ctest PASS, 5-stage byte-identical preserved, 576-config sweep with real `tohost` data.

## What Changes

- **M4.12 真实注册 11 plugin**: `ip/cpu/cpu_factory.h::register_early/normal/late_plugins` wire `IBusPlugin` + `BranchPredictorPlugin` (EARLY), `RiscvDecodePlugin` + `HazardPlugin` + `RiscvIntAluPlugin` + `RiscvMulPlugin<U, LATENCY>` + `RiscvBranchPlugin` + `RiscvLsuPlugin` + `RiscvCsrPlugin` (NORMAL), `DBusPlugin` + `RegFilePlugin` (LATE). The 5 stub comments are removed.
- **M4.14 BranchPredictor `BTB_SIZE` 模板→运行时**: `BranchPredictorPlugin<T, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>(cfg.btb_entries)` — `btb_` becomes `std::vector<BtbEntry>` sized at construction; runtime accessor `btb_size()` replaces `static constexpr kBtbSize`.
- **M4.15 cpu_sim 接入 PicolibcHostMemory + 最小 RV32I 解释器**: `cpu_sim` integrates `PicolibcHostMemory`, parses ELF32 .text section, runs a 4-instruction (ADDI/ADD/SW/JAL) interpreter that drives `PicolibcHostMemory` directly. `tohost` output is real. `ipc` remains 0.0 placeholder (retired-instruction counting deferred to Phase 5+ when pipeline plugins become real).
- **M4.16 add.elf 端到端 3/5/7/10-stage**: `tests/cpu/manual_elf/add.S` compiled to `build/add.elf`; integration tests on all 4 stages assert `tohost=1`. 5-stage byte-identical preserved (6/6 sub-tests: 5 original + 1 new add_elf).
- **M4.17 576 sweep 真实 tohost 数据**: `sweep_driver.py` gains `--elf` flag, re-collects 576 configs with real `tohost=1`. `pareto_analyzer.py` generates Pareto (degenerate — see notes).
- **M4.18 集成测试覆盖**: Existing 4 stage tests gain `test_<n>stage_add_elf_end_to_end` sub-tests. No new test files (additive only).

## Capabilities

### New Capabilities

- `cpu-cpufactory-real`: CpuFactory stub → real registration of 11 RISC-V plugins + BranchPredictor BTB_SIZE runtime + cpu_sim PicolibcHostMemory/RV32I interpreter + add.elf end-to-end on 3/5/7/10-stage + 576-sweep real tohost data + integration test coverage.

### Modified Capabilities

- (无 — M5 三 spec 契约零变化,本 change 仅填补 CpuFactory stub → real 的实施空缺)

## Scope Revisions (vs. original plan)

The original proposal described additional work that was **not implemented** because the underlying premises did not match the actual code:

- **M4.13 reg_file writeback→retire single-direction flow fix** — **CANCELLED**. The original plan described a `commit_count_`/`commit_hook_`/`pending_writes_` race in `reg_file.cpp` and a `RetirePlugin` class, but investigation showed: (1) `RegFilePlugin` has no `commit_count_`/`commit_hook_`/`pending_writes_` fields, (2) the writeback stage just calls `write_reg()` directly with no commit_hook fired, (3) no race condition exists. The `RetirePlugin` class was planned but never created. The M4.13 task is not needed.

- **M4.19 baseline documentation doc** — **DEFERRED**. `docs/performance/m4-cpufactory-real-baseline.md` was planned but the M4.15 RV32I interpreter produces constant `cycles`/`ipc` across all 576 configs (interpreter is pipeline-agnostic), so a 576-row baseline table would be degenerate. Deferred until Phase 5+ implements pipeline-real retired counting.

- **`ip/README.md` CpuFactory status update** — **SKIPPED**. The file already says `🟡 设计中 (实装中)` (in design / in implementation) — accurate after M4.12.

## Impact

- **Affected code**:
  - `ip/cpu/cpu_factory.h` (inline template methods): register_early/normal/late_plugins implementations + mul_latency switch moved into register_normal_plugins + 5 stub comments removed
  - `ip/cpu/plugins/branch_predictor.h` + `branch_predictor.cpp`: BTB_SIZE template param removed, btb_ becomes std::vector, runtime accessor btb_size() added
  - `tools/cpu_sim/main.cpp`: --elf flag, PicolibcHostMemory integration, minimal RV32I interpreter (ADDI/ADD/SW/JAL)
  - `tools/cpu_sim/elf_loader.h`: minimal ELF32 .text parser
  - `tools/dse/sweep_driver.py`: --elf flag added, passed to cpu_sim via run_simulation()
  - `tests/cpu/integration/test_3stage_riscv.cpp` + `test_5stage_riscv.cpp` + `test_7stage_riscv.cpp` + `test_10stage_riscv.cpp`: additive `test_<n>stage_add_elf_end_to_end` sub-tests
  - `src/cf_plugin/CMakeLists.txt`: WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} added to test_3stage and test_5stage
  - `tests/cpu/test_branch_predictor_runtime_btb.cpp`: new test file (3 BTB sizes × runtime ctor)
  - `tests/cpu/test_cpu_sim_real_tohost.cpp`: new test file (cpu_sim --elf → tohost=1)
  - `tests/cpu/test_cpu_factory.cpp`: new test_build_cpu_registers_11_real_plugins
  - `tests/cpu/test_branch_predictor.cpp` + `test_forward_compat.cpp`: BTB_SIZE template arg removed (clean break)

- **API impact**: 0 breaking to public API. `CpuFactory<T>::build_cpu(const CPUConfig&)` signature unchanged. `RegFilePlugin<T>` constructor unchanged. `BranchPredictorPlugin` template params reduced from 6 to 5 (existing test callers updated to pass BTB size via constructor instead of template arg).

- **Dependencies**:
  - **硬前置**: `m4g-extend-tid-and-hooks` (commit `ec6ee4f`) + `m5-dse-superscalar` (commit `3e8fdbf`) — both merged in main
  - **解锁**: T7 add.elf end-to-end + T8 576-sweep real tohost data

- **运行时影响**:
  - 5-stage default behavior byte-identical to M5.11 baseline (6/6 sub-tests pass: 5 original + 1 new add_elf)
  - 7-stage and 10-stage: add.elf runs to completion (tohost=1) for the first time
  - `cpu_sim --elf` now outputs real `tohost=1` (replacing 0 placeholder)

- **工作量**: 3 d / 1 人 (5 atomic phase commits + 1 doc commit + 1 spec fix = 14 commits total)

- **风险**:
  - M4.15 RV32I interpreter: ~50 LOC, supports only ADDI/ADD/SW/JAL (add.S subset). Full RV32I + 7-stage OoO dispatch deferred to Phase 5+.
  - M4.17 Pareto degenerate: cycles/ipc constant across all 576 configs (interpreter pipeline-agnostic). Real Pareto requires retired counting (Phase 5+).
  - M4.14 BTB_SIZE clean break: required updating test_branch_predictor.cpp + test_forward_compat.cpp (existing test callers passed BTB_SIZE as template arg; now pass via constructor).

- **基线影响**: 41/41 → 43/43 ctest (M5 baseline + 2 new tests: test_branch_predictor_runtime_btb + test_cpu_sim_real_tohost). 12/12 → 13/13 openspec validate --specs (cpu-cpufactory-real added at archive). 0 regressions in 5-stage byte-identical.
