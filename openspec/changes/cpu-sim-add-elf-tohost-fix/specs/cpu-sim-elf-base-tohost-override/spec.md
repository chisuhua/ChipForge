# cpu-sim-elf-base-tohost-override Specification

## Purpose

Define the contract for `tools/cpu_sim` CLI to support explicit `--base-addr` and `--tohost-addr` overrides, enabling custom RV32I ELF programs (without `.tohost` section in their linker script) to execute through the existing `PicolibcHostMemory` tohost detection path. This is a workaround for the 5 pre-existing RISC-V simulation test failures documented in `tests/README.md §3.2` (out of scope for root-cause fix in Phase 6d.6+).

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

#### Scenario: regression count net improvement
- **WHEN** `bash tools/run_chipforge_tests.sh --all` runs after the change is applied
- **THEN** overall FAIL count MUST drop by 5 (from 7 to 2; remaining 2 are F7 Dhrystone SIGTERM and F8 CH_MEM st_ld, both out of scope per design.md Open Questions)

### Requirement: No regression in other test families

The new flag set MUST NOT introduce failures in any other test family. Specifically, the following families MUST remain at their pre-change PASS counts (post F1 architecture-gate fix, this change targets the 5 cpu_sim-relative failures only):

- `[framework]` 102/102 PASS
- `[cache]` 25/25 PASS
- `[mmu]` 59/59 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[riscv-tests]` 48/48 PASS
- `[soc]` 20/20 PASS
- `[chmem]` 53/54 PASS (F8 st_ld FAIL is pre-existing backlog, out of scope)
- `[verilator]` 1/1 PASS
- `[mmu-verilator]` 3/3 PASS

#### Scenario: full TLM regression has zero new failures
- **WHEN** `./build/bin/chipforge_tests` (no filter) runs after the change
- **THEN** the ONLY failures MUST be: `test_dhrystone.cpp:78` SIGTERM (F7, pre-existing, 30s budget) and 0 other unexpected failures

#### Scenario: full CH_MEM regression has zero new failures
- **WHEN** `./build/bin/chipforge_tests_chmem` (no filter) runs after the change
- **THEN** the ONLY failure MUST be: `chmem_vendored_elf_tohost1` `rv32ui-p-st_ld` tohost=3 (F8, pre-existing backlog, out of scope)
