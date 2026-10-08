# verilator-mmu-bare-plumbing Specification (MODIFIED by mmu-chmem-pipeline-integration)

## Purpose

This delta MODIFIES the `verilator-mmu-bare-plumbing` capability to add a 4th TEST_CASE (`mmu_sv32_translation_verilator_e2e_flipped`) covering real sv32 translation path through the Verilator e2e test runner.

**Source change**: `mmu-chmem-pipeline-integration` (v0.11.0, Oracle 2026-10-07 locked D1-D5)

**Validation note (Oracle R14)**: The originally proposed "trap PC 跳转断言" AC is REMOVED (per Oracle R14 / D3-A: CH_MEM has no CSR plugin, trap PC jump physically impossible). Replaced with "EXCEPTION_CODE 正确传播 + pipeline stall 在 PTW_ACTIVE=1 时正确生效".

## MODIFIED Requirements

### Requirement: 4 个 `[mmu-verilator]` TEST_CASE family 扩展

The file `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` SHALL contain exactly **4 TEST_CASEs** under the `[mmu-verilator]` family tag. Each TEST_CASE top-level comment SHALL contain the literal substring `**plumbing only — translation semantics NOT verified**`. The file SHALL be conditionally compiled: only when `CF_PLUGIN_USE_CH_MEM` is defined. The file SHALL be registered in `tests/CMakeLists.txt` `CHMEM_TEST_SOURCES` list.

> **MODIFIED 修订 (2026-10-08)**: 由 mmu-chmem-pipeline-integration change 扩展,从 3 个扩展为 4 个,新增 TEST_CASE 4 `mmu_sv32_translation_verilator_e2e_flipped` 验证真 sv32 翻译路径。

**Scope 修订声明** (Oracle/Metis C2 critical): 本 change 仅验证 **CLI plumbing + 真 sv32 翻译 e2e** (新增 TEST_CASE 4)。TLM 端 v0.10.4 hotfix 回归防护由 `[mmu]` family (`test_mmu_cache_integration.cpp::EndToEndTranslationThroughBridge` + `[tlb-refill]` 2 cases) 负责。

#### Scenario: skip-when-absent 一致性
- **WHEN** `CF_VERILATOR_SIM_BIN` file is not present at build time
- **THEN** all 4 TEST_CASEs SHALL `SUCCEED("verilator binary not built — skipping")` and return early

#### Scenario: TEST_CASE 1 baseline 5 ELF tohost=1
- **WHEN** TEST_CASE `mmu_bare_plumbing_tohost1_baseline_5_elf` runs against the 5 vendored `tests/cpu/riscv_tests/elf/rv32ui-p-{add,addi,auipc,beq,jal}` ELFs with `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <elf> --cycles 2000`
- **THEN** each ELF SHALL reach `tohost=1` AND the cycle count SHALL be `≤ baseline_csv[elf] × 1.2`

#### Scenario: TEST_CASE 2 elaboration 0 error + verilator --cc 编译通过
- **WHEN** TEST_CASE `mmu_bare_plumbing_elaboration_zero_error` runs
- **THEN** the elaboration SHALL complete with 0 error AND `pb->to_verilog` SHALL produce non-empty Verilog file AND the `verilator --cc` invocation SHALL exit with status 0.

#### Scenario: TEST_CASE 3 full-chain no-cache (manual_elf)
- **WHEN** TEST_CASE `mmu_bare_plumbing_no_cache_full_chain` runs
- **THEN** the runner SHALL exit cleanly with parseable stdout

#### Scenario: TEST_CASE 4 mmu_sv32_translation_verilator_e2e_flipped (新增, mmu-chmem-pipeline-integration)
- **WHEN** TEST_CASE `mmu_sv32_translation_verilator_e2e_flipped` runs against the vendored `tests/cpu/manual_elf/sv32_pte.elf` with `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`
- **THEN** the ELF SHALL reach `tohost=1` within cycle budget (≤ sv32 baseline × 1.5)
- **AND** `EXCEPTION_CODE` SHALL be correctly propagated (0 in success case)
- **AND** fetch SHALL be correctly stalled during `PTW_ACTIVE=1` cycles (D3-A: combinational mmufault_clear)
- **AND** the sv32 PTE ELF SHALL be vendored from `tests/cpu/manual_elf/build_sv32_pte.S` + registered in `build_manual_elf.sh`

### Requirement: cycle baseline CSV 落盘 (扩展支持 sv32 mode)

The file `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` SHALL contain a header line `elf_name,mode,median_cycles,verilator_version` followed by **5 + 1 = 6 data lines** (5 bare mode lines for vendored RV32UI ELF + 1 sv32 mode line for vendored sv32 PTE ELF). The `mode` column SHALL be either `bare_mmu` or `sv32_mmu`. The `median_cycles` SHALL be computed from 5 runs per ELF and stored as integer.

> **MODIFIED 修订 (2026-10-08)**: CSV 增加 `sv32_mmu` mode 列,支持 TEST_CASE 4 sv32 baseline。

#### Scenario: CSV schema 合规
- **WHEN** the CSV file is parsed by the test harness
- **THEN** each row SHALL have exactly 4 columns matching the schema. The first row SHALL be the header.

#### Scenario: sv32 baseline median 5-run 校准
- **WHEN** any sv32 ELF's 5-run median changes by > 50% from the previously committed CSV (1.5 cap tolerance is wider than bare 1.2)
- **THEN** the change SHALL be flagged as ARCHITECTURAL CHANGE in `tasks.md` §10.

#### Scenario: Verilator 版本锚定
- **WHEN** the Verilator version is upgraded from `5.052` to `5.060` (or any other version bump)
- **THEN** the CSV `verilator_version` column SHALL be updated AND a new baseline regeneration commit SHALL be made

### Requirement: 零回归 + 架构门禁 (扩展 7/7 baseline)

The change SHALL NOT introduce any regression to existing test family counts. The family baselines SHALL be: `[mmu]` 53/53, `[verilator]` 1/1, `[mmu-verilator]` **4/4**, `[cpu-l1-mmu-demo]` **7/7**, `[cpu-integration]` 81/81, `[chmem]` 9/9, `[riscv-tests]` 40/40, `[mmu][chmem]` **3/3** (新增), `[cpu-integration][mmu-chmem]` **1/1** (新增).

> **MODIFIED 修订 (2026-10-08)**: `[cpu-l1-mmu-demo]` baseline 由 6/6 → **7/7 PASS** (TEST_CASE 7 新增)。`[mmu-verilator]` 由 3/3 → **4/4 PASS**。

#### Scenario: regression baselines preserved
- **WHEN** the mmu-chmem-pipeline-integration change is committed
- **THEN** all listed family baselines SHALL remain at the stated counts
- **AND** `[mmu-verilator]` SHALL grow from 3 to 4 TEST_CASEs
- **AND** `[cpu-l1-mmu-demo]` SHALL grow from 6 to 7 TEST_CASEs
- **AND** the existing `[chmem]` and `[cpu-integration]` baselines SHALL be byte-identical preserved
