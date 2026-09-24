# riscv-tests-fixture Specification

## Purpose
TBD - created by archiving change riscv-tests-rv32ui. Update Purpose after archive.
## Requirements
### Requirement: Catch2 fixture SHALL run each rv32ui-p ELF as a separate TEST_CASE

The test fixture at `tests/cpu/integration/test_rv32ui_runner.cpp` SHALL provide 41 separate `TEST_CASE` instances (one per ELF), generated via macro loop over a static list of `rv32ui-p-*` names (excluding `fence_i` and `ma_data`). Each TEST_CASE SHALL be tagged `[cpu-integration][riscv-tests]` for tag-based filtering. Each TEST_CASE SHALL load `tests/cpu/riscv_tests/elf/rv32ui-p-<NAME>` via `cf::tools::load_elf_full`, construct a `PicolibcHostMemory` with a window base aligned to the ELF's `entry_addr` (typically `0x80000000` for riscv-tests env/p ELFs) and `tohost_addr` from the load result, inject all ELF sections via `load_section`, build a CPU via `CpuFactory`, write `entry_addr` to fetch PC, and run up to 10000 cycles.

**Hard REQUIRE** (runner-mechanics): the TEST_CASE SHALL REQUIRE that `mem.exited() == true` — i.e., the test ELF must reach a tohost write within the 10000-cycle cap. Failing this means the runner itself is broken (loader, memory model, fetch chain, exception plugin stuck), not a CPU compliance issue.

**Soft CHECK** (compliance signal): the test SHALL record `mem.exit_code()` and cycle count into the CSV BEFORE the REQUIRE chain, and treat `exit_code != 0` (FAIL encoding) as a CSV `category=真 bug` signal but NOT a hard REQUIRE failure. This preserves Catch2's "all-green merge" expectation while exposing real CPU bugs for Wave 2 triage.

#### Scenario: TEST_CASE for rv32ui-p-add passes when CPU is correct
- **WHEN** the test binary is invoked with `[riscv-tests]` tag and the `rv32ui-p-add` ELF exists and the CPU executes correctly
- **THEN** `mem.exited() == true` (REQUIRE satisfied) AND CSV row SHALL record `status=PASS, category=`

#### Scenario: TEST_CASE for rv32ui-p-jalr exposes a real CPU bug without failing the build
- **WHEN** the test binary runs the `rv32ui-p-jalr` TEST_CASE and the CPU has a jalr bug
- **THEN** `mem.exited() == true` (REQUIRE satisfied — runner works) AND CSV row SHALL record `status=FAIL, category=真 bug, fail_stage=execute` AND TEST_CASE SHALL still PASS Catch2 green (compliance bug captured only in CSV, Wave 2 will fix)

#### Scenario: missing ELF file fails the runner
- **WHEN** `tests/cpu/riscv_tests/elf/rv32ui-p-add` does not exist on disk
- **THEN** the corresponding TEST_CASE SHALL fail with `REQUIRE(fs::exists(...))` (runner-mechanics violation) and CSV row SHALL record `category=runner_setup_error`

#### Scenario: ELF without .tohost section fails the runner
- **WHEN** an ELF with `result.tohost_addr == UINT64_MAX` is loaded
- **THEN** the TEST_CASE SHALL fail with `REQUIRE(result.tohost_addr != UINT64_MAX)`, preventing silent hang

#### Scenario: cycle cap exceeded fails the runner
- **WHEN** the test ELF does not reach tohost within 10000 iterations
- **THEN** the TEST_CASE SHALL fail with `REQUIRE(mem.exited() == true)` and CSV row SHALL record `category=timeout`

### Requirement: 41 ELF names SHALL be the explicit source-of-truth list

The fixture SHALL define a static `constexpr std::array<const char*, 41> kRv32uiPElfs` containing the exact ELF basenames (no `.elf` suffix) in the order: `add, addi, and, andi, auipc, beq, bge, bgeu, blt, bltu, bne, jal, jalr, lb, lbu, ld_st, lh, lhu, lui, lw, or, ori, sb, sh, simple, sll, slli, slt, slti, sltiu, sltu, sra, srai, srl, srli, st_ld, sub, sw, xor, xori`. The list SHALL exclude `fence_i` (requires Zifencei extension, classified as feature stub) and `ma_data` (misaligned access behavior platform-undefined). This list SHALL be referenced both by the macro that generates TEST_CASE instances and by the triage CSV writer.

#### Scenario: list size matches 41
- **WHEN** the fixture is loaded
- **THEN** `kRv32uiPElfs.size() == 41`

#### Scenario: excluded names are not in the list
- **WHEN** searching the list for "fence_i" or "ma_data"
- **THEN** neither SHALL be found

#### Scenario: ld_st and st_ld are included (HazardPlugin probes)
- **WHEN** searching the list for "ld_st" and "st_ld"
- **THEN** both SHALL be found (load-store hazard tests directly stress HazardPlugin scoreboard)

### Requirement: Cycle cap of 10000 SHALL bound each test execution

Each TEST_CASE SHALL run with a hard cycle cap of 10000 via the `for (uint64_t i = 0; i < MAX_CYCLES; ++i) { pb->run(); if (mem.exited()) break; }` loop. If `mem.exited()` returns false after 10000 cycles, the TEST_CASE SHALL fail with INFO logging `cycles=10000 timeout` and the CSV row SHALL be tagged `category=timeout`.

#### Scenario: PASS test completes well under cap
- **WHEN** `rv32ui-p-add` is run
- **THEN** the loop SHALL exit via `mem.exited()` after fewer than 100 cycles (add is trivial: addi+add+sw+jal loop is ~5 cycles)

#### Scenario: failing test that loops infinitely triggers timeout
- **WHEN** a CPU bug causes the test ELF to spin forever (e.g., jalr returns to wrong PC)
- **THEN** the loop SHALL hit cycle 10000 and the TEST_CASE SHALL fail with INFO `cycles=10000 timeout`, classified as `category=timeout` in CSV (Wave 2 fix candidate)

### Requirement: JUnit reporter SHALL emit per-ELF pass/fail summary

The test binary SHALL be invokable with `--reporter JUnit::out=build/rv32ui-baseline-junit.xml` to produce a JUnit-format XML report capturing per-TEST_CASE pass/fail status, cycles, and category. This enables CI trend tracking and is the canonical reporting contract for Phase 1.5 graduation criteria.

#### Scenario: JUnit output file is created
- **WHEN** `chipforge_tests "[riscv-tests]" --reporter JUnit::out=build/rv32ui-baseline-junit.xml` is invoked
- **THEN** `build/rv32ui-baseline-junit.xml` SHALL exist and contain 41 `<testcase>` elements with pass/fail status matching the actual run

#### Scenario: JUnit XML is schema-valid
- **WHEN** the XML file is parsed by `junit-xml-validator` or similar standard tool
- **THEN** it SHALL parse without errors (validates against JUnit ANT schema)

### Requirement: Fixture SHALL use enable_mmu=false

The fixture's `CPUConfig` SHALL explicitly set `enable_mmu = false` (overriding the `cpu_default.json` default of `true`). Wave 1 runs riscv-tests in M-mode bare-metal (satp=0, no translation); the MMU path is Wave 2 `soc-cpu-l1-mmu-demo` scope. This matches the convention established in `tests/cpu/integration/test_5stage_riscv.cpp:38`.

#### Scenario: MMU is disabled for all riscv-tests
- **WHEN** any TEST_CASE in the fixture runs
- **THEN** `cfg.enable_mmu == false` and `MMUPlugin` SHALL NOT be registered in `CpuFactory::build_cpu`

### Requirement: Fixture SHALL reside in tests/cpu/integration/

The fixture file SHALL be located at `tests/cpu/integration/test_rv32ui_runner.cpp` (auto-discovered via `tests/CMakeLists.txt` GLOB_RECURSE). The vendored ELF directory SHALL be at `tests/cpu/riscv_tests/elf/` (committed to git as plain text, ~270KB total).

#### Scenario: new file is auto-discovered
- **WHEN** the file is added at the specified path
- **THEN** re-running CMake configure SHALL pick it up automatically (GLOB_RECURSE) and `chipforge_tests` SHALL include the new tests without manual CMakeLists.txt edit

### Requirement: matrix schema SHALL classify `feature stub` category for unimplemented functionality

The CSV triage output SHALL use the `category=feature stub` literal value when a test ELF fails because an ISA feature is explicitly OUT OF SCOPE per the archived OpenSpec change decision (rather than a CPU logic bug). For Wave 2 of Phase 1.5 this applies to LOAD width extraction (`LB`, `LH`, `LBU`, `LHU`): the DBusPlugin LOAD path returns `read_word` for all load widths (`dec.funct3` is ignored at LOAD stage), so byte/half loads return mismatched data and the test's verification step fails. The runner SHALL mark these rows with `category=feature stub` and `notes=LOAD width extraction OOS (Wave 2)` rather than `category=真 bug`.

#### Scenario: rv32ui-p-lb fails with feature stub
- **WHEN** the test binary runs `rv32ui-p-lb` TEST_CASE
- **THEN** the CSV row SHALL contain `status=FAIL, category=feature stub, notes=LOAD width extraction OOS (Wave 2)` and the TEST_CASE SHALL still PASS Catch2 green (compliance bug captured in CSV only, deferred to Wave 2 `cpu-pipeline-rv32ui-load-width` change)

#### Scenario: rv32ui-p-sb fails with feature stub (load-verify cascading)
- **WHEN** the test binary runs `rv32ui-p-sb` TEST_CASE (which stores a byte then reads back with `lb` to verify)
- **THEN** the CSV row SHALL contain `status=FAIL, category=feature stub` because the store succeeds but the load-back verification fails due to the same LOAD width OOS

### Requirement: 40 ELF name list SHALL be the source-of-truth

The fixture SHALL define a static list of exactly 40 ELF basenames (no `.elf` suffix), updated from the v0.2.2 spec's stale "41" count. The list matches the disk state at commit feb602a: `add, addi, and, andi, auipc, beq, bge, bgeu, blt, bltu, bne, jal, jalr, lb, lbu, ld_st, lh, lhu, lui, lw, or, ori, sb, sh, simple, sll, slli, slt, slti, sltiu, sltu, sra, srai, srl, srli, st_ld, sub, sw, xor, xori`. The list SHALL exclude `fence_i` (requires Zifencei extension, feature stub) and `ma_data` (misaligned access behavior platform-undefined).

#### Scenario: list size matches 40
- **WHEN** the fixture is loaded
- **THEN** the ELF name list SHALL have exactly 40 entries (not 41)

#### Scenario: kRv32uiPElfs array size reflects source-of-truth
- **WHEN** the fixture is compiled
- **THEN** `sizeof(kRv32uiPElfs) / sizeof(const char*) == 40`

### Requirement: triaged category SHALL be set during runner initialization

The runner at `tests/cpu/integration/test_rv32ui_runner.cpp` SHALL hard-code the known LOAD-family ELF names (`lb, lbu, lh, lhu, lw, ld_st, sb, sh, st_ld, sw`) and assign `category=feature stub` + `notes=LOAD width extraction OOS (Wave 2)` to those rows before the REQUIRE chain executes. This is a deliberate hardcoded list — not derived from instruction decoding — because the classification is a triage decision based on the archived OpenSpec spec `riscv-tests-rv32ui` (v0.2.2) rather than runtime observation.

#### Scenario: known LOAD-family ELFs get feature stub classification
- **WHEN** the fixture processes any ELF whose basename is in the hardcoded LOAD-family list
- **THEN** the CSV row SHALL contain `category=feature stub, notes=LOAD width extraction OOS (Wave 2)` regardless of whether the test passes or fails

#### Scenario: non-LOAD-family ELFs fall back to 真 bug classification
- **WHEN** the fixture processes an ELF not in the hardcoded LOAD-family list and the test fails
- **THEN** the CSV row SHALL contain `category=真 bug` (raw failure classification)

