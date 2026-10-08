# cpu-l1-mmu-demo-regression Specification (MODIFIED by mmu-chmem-pipeline-integration)

## Purpose

This delta MODIFIES the `cpu-l1-mmu-demo-regression` capability to add a 7th TEST_CASE (`cpu_l1_mmu_demo_sv32_translation_flipped`) covering real sv32 translation path through the CPU+MMU+Memory demo. Updates baseline count from 6/6 → 7/7 PASS.

**Source change**: `mmu-chmem-pipeline-integration` (v0.11.0, Oracle 2026-10-07 locked D1-D5)

**承接关系**: TEST_CASE 7 承接自 `mmufault-verilator-sv32-e2e-flip` Part a (原 tasks.md §5)。原 Part a 实装 3 次尝试全 FAIL (2026-10-07 Sisyphus bootstrap session);Part a 实装路径已拆分给本 change。

## MODIFIED Requirements

### Requirement: [cpu-l1-mmu-demo] SHALL pass 7/7 after sv32 translation flip

The file `tests/soc/test_cpu_l1_mmu_demo.cpp` `[cpu-l1-mmu-demo]` family SHALL achieve 7/7 PASS. Each riscv-tests ELF (`rv32ui-p-{add,addi,auipc,jal,beq}`) SHALL reach `tohost=1` within 10000 cycles, triggering `PicolibcHostMemory::exited() == true` and `exit_code() == 0`.

> **MODIFIED 修订 (2026-10-08)**: 由 mmu-chmem-pipeline-integration change 扩展,从 6/6 扩展为 7/7 PASS。新增 TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped` 验证 cfg.enable_mmu=true 真 sv32 翻译路径。

The 7 cases SHALL be: `add`, `addi`, `auipc`, `jal`, `beq`, `cpu_l1_mmu_demo_json_structure` + `cpu_l1_mmu_demo_sv32_translation_flipped` (新增).

#### Scenario: 5 ELF tests PASS (旧, 不变)
- **WHEN** ctest `[cpu-l1-mmu-demo]` invoked (5 ELF subset)
- **THEN** all 5 ELF test cases SHALL PASS
- **AND** `cycles` SHALL be <10000 per ELF
- **AND** `mem.exited()` SHALL be true within cycle budget

#### Scenario: JSON structure test continues to pass (旧, 不变)
- **WHEN** `cpu_l1_mmu_demo_json_structure` test case invoked
- **THEN** it SHALL continue to PASS (regression-proofing)

#### Scenario: TEST_CASE 7 sv32_translation_flipped (新增, mmu-chmem-pipeline-integration)
- **WHEN** TEST_CASE `cpu_l1_mmu_demo_sv32_translation_flipped` runs with:
  - `cfg.enable_mmu = true` (从原 6/6 的 false 翻转为 true)
  - `cfg.satp_ppn = (window_base+60*1024)>>12` (per A5 验证后的修正值,如实际是 0x8000F 则用 0x8000F)
  - `plant_identity_page_table(mem, window_base+60*1024, elf.entry_addr)`
- **AND** ELF (e.g., `add` or `addi` from `tests/cpu/manual_elf/`) loaded
- **THEN** `tohost == 1` SHALL be reached within 10000 cycles
- **AND** `mem.exited()` SHALL be true
- **AND** `exit_code() == 0`
- **AND** `cfg.enable_mmu=true` config SHALL correctly trigger `MMUPluginChmem` registration (per C4 owner gap closure)
- **AND** sv32 PTE walk SHALL complete within PTW FSM (5 状态: IDLE → L0_WAIT → L1_WAIT → DONE)

### Requirement: Regression SHALL not degrade other test families

The change SHALL NOT regress any of: `[riscv-tests]` (40/40 PASS), `[cpu-integration]` (≥77/81 PASS, 4 pre-existing FAIL outside scope), `[mmu]` (≥53/53 PASS). All trace code (`#ifdef CF_DEBUG_*`) SHALL be removed before archive.

> **MODIFIED 修订 (2026-10-08)**: 增加 `[mmu][chmem]` + `[cpu-integration][mmu-chmem]` 2 个新 family。

**新增 family** (mmu-chmem-pipeline-integration 实装引入):
- `[mmu][chmem]` 3/3 PASS (Phase B test_mmu_chmem_basic)
- `[cpu-integration][mmu-chmem]` 1/1 PASS (Phase C test_cpu_chmem_mmu_pipeline)

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
- **WHEN** `grep -rn "CF_DEBUG_IBUS_FETCH\|CF_DEBUG_PICOLIBC_MEM\|CF_DEBUG_MMU" ip/` invoked after archive
- **THEN** matches SHALL be 0 in business code paths (header macro definitions only are allowed)

#### Scenario: new mmu-chmem families added
- **WHEN** ctest `[mmu][chmem]` invoked after Phase B 实装
- **THEN** 3/3 SHALL PASS (test_mmu_chmem_basic: TLB hit / TLB miss / PTW fault)
- **WHEN** ctest `[cpu-integration][mmu-chmem]` invoked after Phase C 实装
- **THEN** 1/1 SHALL PASS (test_cpu_chmem_mmu_pipeline: 真 sv32 PTE ELF tohost=1)

### Requirement: Fix SHALL NOT modify ADR-049 PADDR consumption contract

The fix SHALL NOT modify ADR-049 PADDR contract semantics. Allowed modifications: padding timing of `PADDR_VALID` flag (timing only, not semantics); `PicolibcHostMemory` config field additions (additive only); IBus fetch PADDR/PC fallback path correction. Forbidden modifications: changing the MMU PADDR semantic; removing the ADR-049 contract requirements; modifying `IPBusPlugin::at_stage` core logic.

> **MODIFIED 修订 (2026-10-08)**: 由 mmu-chmem-pipeline-integration 维持原 ADR-049 PADDR 契约,仅扩展 payload key (`MMU_VADDR`/`PADDR_VALID`/`PTW_ACTIVE`) 消费路径。

#### Scenario: PADDR semantic preserved
- **WHEN** the change is committed
- **THEN** `MMUPlugin::read_response` semantics (PADDR = translated physical address) SHALL be preserved
- **AND** the new CH_MEM `MMUPluginChmem` SHALL emit identical semantic for `PADDR`/`PADDR_VALID`/`MMU_VADDR` payload keys

### Requirement: AGENTS.md workaround 标记 REMOVED post-archive

`AGENTS.md` line 41 area (the `[cpu-l1-mmu-demo]` section) SHALL be updated after TEST_CASE 7 PASS verification: the v0.10.1 workaround marker SHALL be replaced with `**[v0.11.0 follow-up: mmu-chmem-pipeline-integration]** — 实装完成, TEST_CASE 7 真 sv32 翻转 PASS (7/7)`. The `[cpu-l1-mmu-demo]` baseline note SHALL reflect 7/7 PASS.

> **MODIFIED 修订 (2026-10-08)**: 当 TEST_CASE 7 真正 7/7 PASS 后,移除 `AGENTS.md` L41 的 v0.10.1 workaround 标记,替换为 `[v0.11.0 follow-up: mmu-chmem-pipeline-integration]` 实装完成标记。

#### Scenario: AGENTS.md updated post-archive
- **WHEN** mmu-chmem-pipeline-integration change is archived
- **THEN** `AGENTS.md` SHALL NOT contain the v0.10.1 workaround marker text for `[cpu-l1-mmu-demo]`
- **AND** `AGENTS.md` SHALL contain the v0.11.0 follow-up marker text
- **AND** the `[cpu-l1-mmu-demo]` baseline note SHALL reflect 7/7 PASS (not 6/6)
