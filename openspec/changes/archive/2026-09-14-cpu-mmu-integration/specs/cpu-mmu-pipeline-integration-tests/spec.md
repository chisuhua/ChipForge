## ADDED Requirements

### Requirement: cpu-mmu-integration MUST include 8 pipeline integration tests covering 3/5/7/10-stage + RiscV hooks

The change MUST add 8 `TEST_CASE` entries: 5 stage cases (1 per pipeline depth 3/5/7/10-stage + 1 enable_mmu=false bit-identical baseline) to existing `[cpu-integration]` test files + 3 RiscV hook integration tests in a separate `test_cpu_riscv_mmu_hooks.cpp` file. All 8 new tests MUST PASS. The total `[cpu-integration]` count MUST become **33 cases** (25 baseline + 8 new) all PASS; the project-wide `chipforge_tests` total MUST become **314 cases** (306 baseline + 8 new) all PASS.

#### Scenario: 5-stage build with enable_mmu=true succeeds
- **WHEN** `test_5stage_riscv.cpp` runs `EnableMMU5StageBuilds` with `enable_mmu=true` config
- **THEN** 5 stages + 3 MMU substages (csr_write_satp, sfence_vma, mmu_exit) MUST all be declared; the test MUST PASS

#### Scenario: 5-stage enable_mmu=false preserves bit-identical baseline
- **WHEN** `test_5stage_riscv.cpp` runs `EnableMMUDisabledBitIdenticalBaseline` with `enable_mmu=false` config
- **THEN** the resulting PipeBuilder MUST be bit-identical to the pre-change baseline; all 25 `[cpu-integration]` tests MUST still PASS

#### Scenario: 3-stage build with enable_mmu=true succeeds
- **WHEN** `test_3stage_riscv.cpp` runs `EnableMMU3StageBuilds` with `enable_mmu=true` config
- **THEN** 3 stages + 3 MMU substages MUST all be declared; the test MUST PASS

#### Scenario: 7-stage build with enable_mmu=true succeeds (commit/retire)
- **WHEN** `test_7stage_riscv.cpp` runs `EnableMMU7StageCommitRetire` with `enable_mmu=true` config
- **THEN** 7 stages + 3 MMU substages MUST all be declared; the test MUST PASS

#### Scenario: 10-stage build with enable_mmu=true succeeds (deep pipeline)
- **WHEN** `test_10stage_riscv.cpp` runs `EnableMMU10StageDeepPipeline` with `enable_mmu=true` config
- **THEN** 10 stages + 3 MMU substages MUST all be declared; the test MUST PASS

#### Scenario: RiscV hook CSR write routes to MMUPlugin
- **WHEN** `test_cpu_riscv_mmu_hooks.cpp` runs `CSRWriteSatpRoutesToRiscVMMUPlugin` simulating CPU CSR write to `sat` with value `0x8000_1000ULL`
- **THEN** `RiscvMMUPlugin::satp_value()` MUST return `0x8000_1000ULL`; `RiscvMMUPlugin::csr_write_satp` MUST have been invoked

#### Scenario: RiscV hook SFENCE.VMA routes to MMUPlugin
- **WHEN** `test_cpu_riscv_mmu_hooks.cpp` runs `SFENCEVMARoutesToRiscVMMUPlugin` simulating CPU SFENCE.VMA with `rs1=0x4000_0000, rs2=0` after pre-filling TLB entry at `vaddr=0x4000_0000`
- **THEN** `RiscvMMUPlugin::sfence_vma` MUST have been invoked; the pre-filled TLB entry MUST be invalidated

#### Scenario: RiscV hook exception propagation works end-to-end
- **WHEN** `test_cpu_riscv_mmu_hooks.cpp` runs `MMUExceptionPropagatesToCPU` simulating page fault via MMUPlugin with `EXCEPTION_CODE=13`
- **THEN** the CPU `at_stage("mmu_exit")` closure MUST read `EXCEPTION_CODE=13`; the CPU exception path MUST receive `13`