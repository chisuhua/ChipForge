## ADDED Requirements

### Requirement: CSV triage matrix SHALL be written before each TEST_CASE's REQUIRE chain

Each TEST_CASE in `tests/cpu/integration/test_rv32ui_runner.cpp` SHALL write its own CSV row to `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` BEFORE the runner-mechanics REQUIRE assertions execute (using a static `std::once_flag` to truncate the file with a single header row on the first write, then appending subsequent rows). This ensures that (a) failing tests still produce CSV records (REQUIRE would otherwise abort the test before CSV writing), and (b) the CSV is idempotent across runs (subsequent `chipforge_tests "[riscv-tests]"` invocations re-truncate via the once_flag and re-write the same row set).

The CSV path SHALL be set via a CMake-injected compile definition `RV32UI_CSV_PATH` rooted at `${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` (NOT a relative path, because ctest working directory is the build directory and would otherwise produce a CSV outside the source tree).

#### Scenario: CSV is created with 47 rows after a run
- **WHEN** `chipforge_tests "[riscv-tests]"` runs to completion (regardless of individual pass/fail)
- **THEN** `${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` SHALL exist with 1 header row + 41 rv32ui-p data rows + 5 pre-existing RISC-V subprocess rows = 47 total lines

#### Scenario: CSV is idempotent across runs
- **WHEN** `chipforge_tests "[riscv-tests]"` runs twice in succession
- **THEN** the CSV SHALL have the same 47 lines after both runs (not 94), because the static once_flag truncates the file at first TEST_CASE invocation

#### Scenario: failing test still produces a CSV row
- **WHEN** `rv32ui-p-jalr` TEST_CASE fails a REQUIRE (e.g., tohost never written, cycle cap exceeded)
- **THEN** the CSV row for `rv32ui-p-jalr` SHALL still be present in the file (written before REQUIRE) recording `status=FAIL, category=runner_setup_error` or `category=timeout`

### Requirement: CSV schema SHALL include elf, status, fail_stage, category, cycles, notes

The CSV header SHALL be exactly: `elf,status,fail_stage,category,cycles,notes`. Each row SHALL contain values for all 6 columns. The `category` field SHALL use one of the literal values `真 bug`, `feature stub`, `toolchain`, `timeout`, `runner_setup_error`, or be empty (for PASS rows, where `status=PASS` implies category is unclassified). The `fail_stage` field SHALL be empty for PASS rows. The placeholder "N/A" SHALL NOT appear in any column.

#### Scenario: schema is parseable
- **WHEN** `head -1 soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` is run
- **THEN** the first line SHALL equal `elf,status,fail_stage,category,cycles,notes`

#### Scenario: PASS row has empty fail_stage and empty category
- **WHEN** a test like `rv32ui-p-add` PASSes (exit_code=0)
- **THEN** its row SHALL be `rv32ui-p-add,PASS,,,5,` where 5 is the actual cycle count and both `fail_stage` and `category` are empty (PASS implies unclassified)

### Requirement: FAIL rows SHALL classify failure into one of 4 categories

A TEST_CASE failure SHALL be classified into exactly one of: `真 bug` (real CPU bug needing Wave 2 fix), `feature stub` (CPU lacks the feature, e.g., CSR), `toolchain` (toolchain/env issue resolved by vendored ELF approach), `timeout` (10000 cycle cap exceeded, often indicates pipeline stall bug). The classification logic SHALL use Catch2's `INFO` macros to capture diagnostic context (e.g., `INFO("expected exit_code=0 got=" << exit_code)`) and the fixture SHALL determine it from cycle count + exit_code value + FAIL message context.

#### Scenario: 真 bug classification
- **WHEN** `rv32ui-p-jalr` fails with exit_code=1 (FAIL signal) and cycles < 10000
- **THEN** the CSV row SHALL have `category=真 bug` and `fail_stage=execute` (jalr execute bug)

#### Scenario: timeout classification
- **WHEN** a test fails with cycles >= 10000 (cycle cap hit)
- **THEN** the CSV row SHALL have `category=timeout` and `fail_stage=` empty

#### Scenario: toolchain classification
- **WHEN** one of the 5 pre-existing RISC-V tests (`test_*stage_riscv` × 4 + `test_cpu_sim_real_tohost`) fails
- **THEN** its CSV row SHALL have `category=toolchain` (these were toolchain-related in v0.1.3 and expected to convert to PASS in v0.2.0)

### Requirement: Pre-existing RISC-V test rows SHALL be appended via subprocess invocation

The 5 pre-existing RISC-V test rows (`test_3stage_riscv`, `test_5stage_riscv`, `test_7stage_riscv`, `test_10stage_riscv`, `test_cpu_sim_real_tohost`) SHALL be appended to the SAME CSV via the riscv-tests fixture's helper using the `exec_cmd` pattern (matching `tests/cpu/integration/test_3stage_riscv.cpp:111-121`). The fixture SHALL invoke `./build/bin/cpu_sim --elf build/add.elf` (or the equivalent add.elf path) for each of the 5 test ELF scenarios, parse the resulting `tohost=N` output, and record each as a CSV row with `category=toolchain` (these were toolchain-related in v0.1.3; Wave 1 expects them to convert to PASS via the loader/memory upgrades and vendored ELF approach, but CSV records the result regardless). They SHALL appear after all 41 rv32ui-p rows.

#### Scenario: 5 pre-existing rows appear in CSV with category=toolchain
- **WHEN** the CSV is generated by running `chipforge_tests "[riscv-tests]"`
- **THEN** it SHALL contain rows for: `test_3stage_riscv`, `test_5stage_riscv`, `test_7stage_riscv`, `test_10stage_riscv`, `test_cpu_sim_real_tohost` (5 rows) with `category=toolchain` (or `category=` if converted PASS via loader upgrade — substring check, not exact match)

#### Scenario: filter strategy allows 47-row CSV to be produced
- **WHEN** the user invokes `chipforge_tests "[riscv-tests]"` (single tag filter)
- **THEN** ONLY the 41 riscv-tests-p TEST_CASEs run directly; the 5 pre-existing rows SHALL come from the fixture's subprocess `exec_cmd` invocations (NOT from running their original TEST_CASEs under the `[riscv-tests]` filter, which would NOT match because those tests are tagged `[cpu-integration]` only)

### Requirement: CSV SHALL be the canonical Wave 2 input

The CSV file `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` SHALL be the canonical input for Wave 2 `cpu-pipeline-fix-rv32ui-N` triage work. It SHALL be committed to git after each Wave 1 run (not gitignored). The schema and location SHALL NOT change without an ADR or migration spec.

#### Scenario: CSV is committed to git
- **WHEN** `git status soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` is run after a test run
- **THEN** the file SHALL be either clean (already committed) or staged for commit (newly created/modified)

#### Scenario: Wave 2 reads the CSV
- **WHEN** Wave 2's `cpu-pipeline-fix-rv32ui-N` OpenSpec change is opened
- **THEN** the change's proposal.md SHALL reference `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` as the input triage artifact