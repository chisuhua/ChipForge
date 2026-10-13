# cpu-pipeline-tick-rca Specification

## Purpose

Define the diagnostic methodology contract for the `cpu-pipeline-tick-rca` change: a pure investigation (no product code changes) of why 5 RISC-V simulation tests FAIL when calling `cpu_sim` CLI but PASS when calling C++ API directly. The deliverable is a diagnostic report (in `openspec/changes/cpu-pipeline-tick-rca/design.md`) identifying the most likely root cause and proposing fix direction for the follow-up `cpu-pipeline-tick-fix` change.

## ADDED Requirements

### Requirement: Investigation MUST follow 4-line + control-experiment methodology

The diagnostic process MUST investigate root cause candidates in the following order with explicit control experiment prepended:

1. **Control experiment** (L4 discriminator) — add temporary `--no-mmu` CLI flag to `tools/cpu_sim`, run `cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000` and observe whether `tohost=1` appears in output.
2. **L4 (Config Divergence)** — investigate whether `cfg.enable_mmu` differs between CLI (default `true` from `CPUConfig` struct since `load_config()` does not parse `enable_mmu` from JSON) and C++ API runner (explicit `false` in `test_rv32ui_runner.cpp:109`).
3. **L2 (fetch CtrlLink halt_when)** — investigate `ip/cpu/plugins/ibus.h:54-67` `fetch_ctrl->halt_when` lambda and whether `PTW_ACTIVE` misfires in `enable_mmu=true` configuration.
4. **L3 (StageLinkPlugin EARLY)** — investigate `ip/cpu/plugins/stage_link.h` EARLY closures for cross-stage payload propagation.
5. **L1 (build_cpu_impl + auto_throw)** — static exclusion of the `std::unique_ptr<PipeBuilder>` + Result wrapper + auto_throw difference between CLI and C++ API paths.

#### Scenario: L4 confirmed via control experiment
- **WHEN** control experiment with `--no-mmu` produces `tohost=1` in cpu_sim output
- **THEN** L4 is confirmed as root cause and the diagnostic report MUST document that `cfg.enable_mmu=true` (CLI default) vs `cfg.enable_mmu=false` (runner explicit) is the primary divergence
- **AND** subsequent L2/L3/L1 investigation is OPTIONAL (only for secondary root causes)

#### Scenario: L4 excluded, L2 deep dive
- **WHEN** control experiment with `--no-mmu` still produces `tohost=0`
- **THEN** L4 is excluded and L2 deep dive MUST follow with runtime log points 1 and 2 (defined below)

### Requirement: Investigation MUST respect file scope boundary

The diagnostic process MUST NOT read or modify files outside the documented whitelist:

- **Whitelist (read + write log allowed)**: `tools/cpu_sim/main.cpp`, `ip/cpu/cpu_factory.h`, `ip/cpu/plugins/ibus.h`, `ip/cpu/plugins/dbus.h`, `ip/cpu/plugins/stage_link.h`, `ip/cpu/plugins/mmu.h`, `ip/cpu/plugins/mmu_exception_handler.h` (TLM only), `include/cf/plugin/pipe_builder.h`, `include/cf/plugin/result_macros.h`, `ip/cpu/picolibc_host_memory.h`
- **Exclusion list (read-forbidden, write-forbidden)**: `src/cf_plugin/bridge/*`, `**/*_chmem.h` (CH_MEM files), `build/_deps/**` (CppTLM/CppHDL source), `ip/mmu/lib/**` (mmu internals), `ip/mmu/chmem/**`

#### Scenario: Encountering CH_MEM file during investigation
- **WHEN** investigation naturally leads to a CH_MEM file (e.g., `mmu_exception_handler_chmem.h`)
- **THEN** investigator MUST NOT read or modify that file; instead document in the per-line stop-and-document block that "CH_MEM counterpart is excluded per file scope"

### Requirement: Each investigation line MUST produce a stop-and-document block

For L4, L2, L3 (when investigated), the design.md MUST contain a stop-and-document block with this structure:

```
### <line_id> Stop-and-Document
- **结论**: [confirmed / excluded / partial]
- **证据**: <command + output (≤ 10 lines)>
- **信心度**: [HIGH / MEDIUM / LOW]
- **下一步**: [pivot to next line / skip to §5 综合]
```

#### Scenario: Inconclusive investigation result
- **WHEN** investigation cannot definitively confirm or exclude a line within reasonable effort (e.g., 1 hour for L2, 30 min for L3)
- **THEN** the stop-and-document block MUST be marked `inconclusive` (not forced into confirmed/excluded) and the next-line investigation MUST proceed

### Requirement: Temporary debug log MUST be reverted before archive

Any temporary `std::cerr` debug log added during investigation (control experiment flag, log point 1/2/3, etc.) MUST be reverted before the change is committed or archived.

#### Scenario: Adding --no-mmu temporary flag
- **WHEN** investigator adds `--no-mmu` to `tools/cpu_sim/main.cpp` for control experiment
- **THEN** the change MUST be applied via patch file or `git stash` (not committed to HEAD)
- **AND** the change MUST be reverted via `git checkout tools/cpu_sim/main.cpp` after the experiment concludes
- **AND** the revert MUST be verified by re-running `./build/bin/chipforge_tests [cpu]` and confirming baseline 128/129 (1 FAIL) is preserved

#### Scenario: Adding log points in framework files
- **WHEN** investigator adds `std::cerr` log to `ip/cpu/plugins/ibus.h` or `ip/cpu/plugins/stage_link.h`
- **THEN** the change MUST be reverted before the change is committed
- **AND** `git status --porcelain` MUST be empty for these files (verified before tasks.md §6.3)
- **AND** the architecture gate (`bash tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh`) MUST pass 0 FAIL as a fallback verification

### Requirement: Final state MUST preserve 0-regression baseline

The change MUST preserve the 0-regression baseline after commit. Specifically:

- `[riscv-tests]` MUST be 48/48 PASS
- `[cpu]` MUST be 128/129 PASS (1 FAIL `cpu_sim_real_tohost` preserved — change does NOT fix it)
- `[cpu-integration]` MUST be 30/35 PASS (4 FAIL `*_add_elf_end_to_end` + 1 SIGTERM dhrystone preserved)
- `[cpu-l1-mmu-demo]` MUST be 6/6 PASS
- `[cpu-integration][mul-div-fsm-integration]` MUST be 4/4 PASS
- Architecture gate (33 + 8 + 12) MUST be 0 FAIL

#### Scenario: Temporary debug log not reverted
- **WHEN** a temporary debug log accidentally persists in a framework file
- **THEN** the architecture gate's static checks (specifically `check_plugin_portability.sh` for `ch_mem` symbol leakage in TLM files) MAY catch the regression
- **AND** the change MUST NOT be committed until the log is reverted

#### Scenario: --no-mmu flag accidentally committed
- **WHEN** `--no-mmu` CLI flag is accidentally committed in `tools/cpu_sim/main.cpp`
- **THEN** the change fails scope contract (no product code changes allowed)
- **AND** MUST be reverted before archive

### Requirement: Diagnostic report MUST recommend fix direction without producing patch

The `design.md` MUST contain a fix recommendation section (`§5.3 修复建议`) that:

- Lists ranked fix candidates with abstract description (NOT concrete patch)
- Each candidate includes: trigger condition, affected files (allowed scope only), risk assessment (LOW/MEDIUM/HIGH), coordination requirements with other in-flight changes
- The actual patch implementation is OUT OF SCOPE for this change and reserved for the follow-up `cpu-pipeline-tick-fix` change

#### Scenario: Fix recommendation for L4 confirmed
- **WHEN** L4 is confirmed as root cause
- **THEN** §5.3 MUST recommend at least: (a) modify `load_config()` to parse `enable_mmu` from JSON params, (b) change `CPUConfig::enable_mmu` default to `false`, AND/OR (c) cpu_sim CLI explicitly pin `cfg.enable_mmu = false` as work-around
- **AND** MUST flag coordination requirement with `mmu-chmem-pipeline-integration` change owner (mmu tests expect `enable_mmu=true` default in some contexts)

#### Scenario: Fix recommendation for L2 confirmed
- **WHEN** L2 is confirmed (fetch CtrlLink halt_when misfires)
- **THEN** §5.3 MUST recommend modifying `ibus.h:54-67` halt_when lambda to add Bare-mode guard, AND/OR `MmuExceptionHandler` payload propagation
- **AND** MUST flag MEDIUM risk: stall semantics consumed by `HazardPlugin` and `PTW` paths, Bare-mode byte-identical preservation required
