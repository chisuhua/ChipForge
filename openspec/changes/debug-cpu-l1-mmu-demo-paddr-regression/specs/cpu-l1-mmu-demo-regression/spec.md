# debug-cpu-l1-mmu-demo-paddr-regression Specification

## ADDED Requirements

### Requirement: [cpu-l1-mmu-demo] SHALL pass 6/6 after PADDR regression fix

`tests/soc/test_cpu_l1_mmu_demo.cpp` `[cpu-l1-mmu-demo]` family SHALL 6/6 PASS (`add`, `addi`, `auipc`, `jal`, `beq` + `cpu_l1_mmu_demo_json_structure`). Each riscv-tests ELF (`rv32ui-p-{add,addi,auipc,jal,beq}`) SHALL reach `tohost=1` within 10000 cycles, triggering `PicolibcHostMemory::exited() == true` and `exit_code() == 0`.

#### Scenario: 5 ELF tests PASS
- **WHEN** ctest `[cpu-l1-mmu-demo]` invoked
- **THEN** all 6 test cases SHALL PASS
- **AND** `cycles` SHALL be <10000 per ELF
- **AND** `mem.exited()` SHALL be true within cycle budget

#### Scenario: JSON structure test continues to pass
- **WHEN** `cpu_l1_mmu_demo_json_structure` test case invoked
- **THEN** it SHALL continue to PASS (regression-proofing — fix shall not touch JSON structure)

### Requirement: Regression SHALL not degrade other test families

The fix SHALL NOT regress any of: `[riscv-tests]` (40/40 PASS maintained), `[cpu-integration]` (≥77/81 PASS maintained, 4 pre-existing FAIL outside scope), `[mmu]` (≥53/53 PASS maintained). All trace code (`#ifdef CF_DEBUG_*`) SHALL be removed before archive.

#### Scenario: riscv-tests unchanged
- **WHEN** ctest `[riscv-tests]` invoked after fix
- **THEN** 40/40 SHALL continue to PASS

#### Scenario: cpu-integration unchanged
- **WHEN** ctest `[cpu-integration]` invoked after fix
- **THEN** ≥77/81 SHALL continue to PASS (4 pre-existing tohost=1-template-missing failures are out of scope)

#### Scenario: mmu unchanged
- **WHEN** ctest `[mmu]` invoked after fix
- **THEN** ≥53/53 SHALL continue to PASS

#### Scenario: trace code removed
- **WHEN** `grep -rn "CF_DEBUG_IBUS_FETCH\|CF_DEBUG_PICOLIBC_MEM" ip/` invoked after archive
- **THEN** matches SHALL be 0 in business code paths (header macro definitions only are allowed)

### Requirement: Fix SHALL NOT modify ADR-049 PADDR consumption contract

The fix SHALL NOT modify ADR-049 PADDR contract semantics. Allowed modifications:
- Padder timing of `PADDR_VALID` flag (timing only, not semantics)
- PicolibcHostMemory config field additions (additive only)
- IBus fetch PADDR/PC fallback path correction (already in spec, was not implemented)

Forbidden modifications:
- Changing the MMU PADDR semantic (what PADDR means)
- Removing the ADR-049 contract requirements
- Modifying `IPBusPlugin::at_stage` core logic (only trace allowed)

#### Scenario: ADR-049 contract preserved
- **WHEN** `git diff 2026-09-26..HEAD ip/mmu/tlm/MMUPlugin.h ip/mmu/tlm/MMUPlugin.cpp` invoked after fix
- **THEN** diff SHALL be ≤ 5 lines (timing adjustments only, not semantic changes)