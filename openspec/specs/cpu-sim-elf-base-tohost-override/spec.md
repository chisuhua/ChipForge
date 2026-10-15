# cpu-sim-elf-base-tohost-override Specification

## Purpose
TBD - created by archiving change cpu-sim-add-elf-tohost-fix. Update Purpose after archive.
## Requirements
### Requirement: cpu_sim CLI SHALL accept `--base-addr` and `--tohost-addr` flags

The `tools/cpu_sim/main.cpp` argv parser MUST accept two new optional flags:

- `--base-addr HEX` — explicit override for `PicolibcHostMemory::Config::base_addr` (default: ELF `e_entry` aligned to 64KB, or 0 if `e_entry == 0`)
- `--tohost-addr HEX` — explicit override for `PicolibcHostMemory::Config::tohost_addr` (default: ELF `.tohost` section VMA, or 0 if no `.tohost` section)

When BOTH flags are absent, the current behavior MUST be preserved (backward compatible). When either flag is provided, its value MUST override the corresponding default.

The flags MUST follow the existing argv-parsing style (no new dependencies; no CLI11). The `print_usage()` function MUST document the new flags in the same format as existing options.

#### Scenario: Default invocation preserves legacy behavior
- **WHEN** user runs `cpu_sim --config <path> --elf <path> --cycles N` with no `--base-addr` and no `--tohost-addr`
- **THEN** the binary SHALL compute `base_addr` and `tohost_addr` exactly as today (ELF-driven) and behave byte-identical to the pre-change binary

#### Scenario: Explicit base-addr override is applied
- **WHEN** user runs `cpu_sim --elf build/add.elf --base-addr 0x80000000 --cycles 100` (no `--tohost-addr`)
- **THEN** `PicolibcHostMemory` SHALL be constructed with `base_addr = 0x80000000` AND `tohost_addr` from ELF-driven default

#### Scenario: Explicit tohost-addr override is applied
- **WHEN** user runs `cpu_sim --elf build/add.elf --tohost-addr 0x80001000 --cycles 100` (no `--base-addr`)
- **THEN** `PicolibcHostMemory` SHALL be constructed with `tohost_addr = 0x80001000` AND `base_addr` from ELF-driven default

#### Scenario: Both flags override both defaults
- **WHEN** user runs `cpu_sim --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100`
- **THEN** `PicolibcHostMemory` SHALL be constructed with exactly `base_addr = 0x80000000` AND `tohost_addr = 0x80001000`

#### Scenario: Unknown flag is rejected with usage hint
- **WHEN** user runs `cpu_sim --unknown-flag value`
- **THEN** binary SHALL print `FAIL: unknown argument: --unknown-flag` to stderr AND print usage to stdout AND exit with code 1 (same as existing unknown-flag handling)

### Requirement: add.elf linker script SHALL produce a riscv-tests-style ELF layout

The `tests/cpu/manual_elf/build_manual_elf.sh` script MUST invoke `riscv32-unknown-elf-ld` with a linker script that places:

- `.text.init` section at VMA `0x80000000` with the entry-point instruction (`_start`) and `6 × 4 = 24` bytes total
- `.tohost` section at VMA `0x80001000` with `8 × 4 = 32` bytes reserved (zeroed, the test ELF writes to it to signal PASS)
- entry point at `0x80000000`

The output ELF MUST be loadable by `PicolibcHostMemory` with `base_addr = 0x80000000` and `tohost_addr = 0x80001000`, matching the layout that `tests/cpu/riscv_tests/elf/rv32ui-p-*` ELFs use (which already pass 40/40 via `test_rv32ui_runner`).

#### Scenario: add.elf produced with correct sections
- **WHEN** `bash tests/cpu/manual_elf/build_manual_elf.sh` is invoked
- **THEN** `build/add.elf` SHALL contain `.text.init @ 0x80000000` (≥ 24 bytes), `.tohost @ 0x80001000` (≥ 32 bytes), and entry point `0x80000000` (verified by `riscv32-unknown-elf-objdump -h`)

#### Scenario: add.elf exits with tohost=1 within cycle budget
- **WHEN** `cpu_sim --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000` is invoked
- **THEN** `cycles` SHALL be less than 1000 (early exit on `mem.exited()`) AND `tohost=1` SHALL appear in output

### Requirement: 5 pre-existing FAIL tests SHALL pass with the new invocation contract

The following TEST_CASE entries in `tests/cpu/` MUST invoke `cpu_sim` with the new flag set and SHALL be green in the regression report:

- `tests/cpu/test_cpu_sim_real_tohost.cpp` :: `cpu_sim_real_tohost` (`[cpu]` tag)
- `tests/cpu/integration/test_3stage_riscv.cpp` :: `*_add_elf_end_to_end` (`[cpu-integration]` tag)
- `tests/cpu/integration/test_5stage_riscv.cpp` :: `5stage_add_elf_end_to_end` (`[cpu-integration]` tag)
- `tests/cpu/integration/test_7stage_riscv.cpp` :: `7stage_add_elf_end_to_end` (`[cpu-integration]` tag)
- `tests/cpu/integration/test_10stage_riscv.cpp` :: `10stage_add_elf_end_to_end` (`[cpu-integration]` tag)

Each test's `exec_cmd` payload MUST include `--base-addr 0x80000000 --tohost-addr 0x80001000` and `--cycles` MUST be at least `1000` (per AGENTS.md §known test status "all to tohost=1 within 10000 cycles via workaround").

#### Scenario: test_cpu_sim_real_tohost passes
- **WHEN** `chipforge_tests [cpu]` runs the `cpu_sim_real_tohost` TEST_CASE
- **THEN** the test SHALL pass with no `FAILED` assertion AND no `SIGTERM` signal

#### Scenario: 4 stage end-to-end tests pass
- **WHEN** `chipforge_tests [cpu-integration]` runs the 4 `*_add_elf_end_to_end` TEST_CASEs
- **THEN** all 4 SHALL pass with `tohost=1` appearing in `cpu_sim` stdout

### Requirement: No regression in other test families

The new flag set MUST NOT introduce failures in any other test family. Specifically, the following families MUST remain at their pre-change PASS counts:

- `[riscv-tests]` 48/48 PASS
- `[mmu]` 59/59 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[framework]` 89/89 PASS
- `[chmem]` 9/9 PASS (框架部分)
- `[verilator]` 1/1 case PASS (40 rv32ui ELF)

#### Scenario: full TLM regression has zero new failures
- **WHEN** `bash tools/run_chipforge_tests.sh` runs after the change
- **THEN** the cpu_sim-relative FAIL count MUST be ≤ pre-change baseline (5 expected) AND no other family gets new failures

#### Scenario: full CH_MEM regression has zero new failures
- **WHEN** `./build/bin/chipforge_tests_chmem` runs after the change
- **THEN** the only failures MUST be pre-existing (e.g. `cpphdl_poc_chbool_contextual_conversion` context pollution flake) and not introduced by this change

### Requirement: Deferral notice — 5 cpu_sim CLI tests remain FAIL (root cause in `cpu-pipeline-tick-rca`)

This change MUST be documented as **scope partial** (AGENTS.md:61): the 5 cpu_sim CLI tests (`test_cpu_sim_real_tohost` + 4 `*_add_elf_end_to_end`) **still FAIL after the workaround**, because the root cause is a framework-level bug in the cpu_sim CLI path (PC advances via writeback +4 but decode/memory payload does not propagate, tohost write never triggers `check_tohost`). This is NOT fixed by the CLI flag workaround.

The root-cause investigation and fix MUST be deferred to `openspec/changes/cpu-pipeline-tick-rca/`. This requirement exists to record the honest outcome and prevent future confusion ("did cpu-sim-add-elf-tohost-fix fix the tests?").

#### Scenario: Honest outcome recorded in CHANGELOG
- **WHEN** this change is archived
- **THEN** the archive metadata MUST record "scope partial: 5 FAIL → 5 FAIL, root cause in `cpu-pipeline-tick-rca`" and link to that change's RCA tasks

#### Scenario: TLM API path is unaffected (sanity baseline)
- **WHEN** the same `rv32ui-p-add` ELF runs through `test_rv32ui_runner` (TLM C++ API, not cpu_sim CLI)
- **THEN** it SHALL still pass 40/40 (this baseline is unaffected by the cpu_sim CLI bug; the workaround only affects the CLI path)


### Requirement: cpu_sim CLI Bare-mode fetch MUST NOT stall (cpu-pipeline-tick-fix F3b)

When MMUPlugin publishes `SATP_MODE=0` (Bare mode per RISC-V Privileged Spec §4.3.1 — e.g. satp CSR not yet programmed, or explicit Bare mode config), `ip/cpu/plugins/ibus.h` `fetch_ctrl->halt_when` MUST return `false` so the fetch stage proceeds with identity translation instead of stalling on `PTW_ACTIVE=1`.

#### Scenario: Bare-mode fetch proceeds without PTW stall (cpu-pipeline-tick-fix F3b)
- **GIVEN** CPUConfig with `enable_mmu=true` but `satp_value MODE=0` (Bare mode)
- **WHEN** cpu_sim CLI runs any workload (e.g. add.elf)
- **THEN** `pb->run()` cycles proceed normally with `(*tlb_node)(SATP_MODE)==0` triggering `halt_when` return `false`; `tohost` is writable within cycles<30
- **AND** the Bare-mode byte-identical behavior with v0.10.4 (`a3afcfa`) is preserved

#### Scenario: Real Sv32/Sv39/Sv48 translation stall preserved (cpu-pipeline-tick-fix F3b)
- **GIVEN** CPUConfig with `enable_mmu=true` AND satp CSR programmed with non-zero MODE (e.g. sv_mode=Sv39, `satp_value >> 60 = 8`)
- **WHEN** `fetch_ctrl->halt_when` lambda runs and PTW_ACTIVE=1
- **THEN** returns `true` (preserves v0.10.4 hotfix `a3afcfa` strict Bare-shortcut judgment for real Sv modes)
- **AND** `[mmu]` 59/59 + `[cache] EndToEndTranslationThroughBridge` PASS maintained
