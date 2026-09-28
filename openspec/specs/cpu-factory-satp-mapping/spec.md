# cpu-factory-satp-mapping Specification

## Purpose
TBD - created by archiving change cpu-factory-satp-mapping. Update Purpose after archive.
## Requirements
### Requirement: CpuFactory MMU config maps to satp_value correctly

CpuFactory SHALL compute `satp_value` passed to `RiscvMMUPlugin` constructor from `CPUConfig.mmu_mode` + `CPUConfig.satp_ppn`, per RISC-V Privileged Spec §4.3.1 (satp CSR layout, PPN at bits [21:0]/[43:0] directly, NO shift).

#### Scenario: Bare mode

- **WHEN** `CPUConfig{enable_mmu=true, mmu_mode="bare"}`
- **THEN** `satp_value` MUST be `0` (Bare translation)
- **AND** ADR-049 Bare shortcut MUST trigger identity translation

#### Scenario: Sv32 mode + satp_ppn=P (P > 0)

- **WHEN** `CPUConfig{enable_mmu=true, mmu_mode="sv32", satp_ppn=P}` where P > 0
- **THEN** `satp_value` MUST be `(1ULL << 31) | (P & 0x3FFFFFULL)` (Sv32 RV32 encoding: MODE=bit31, ASID=[30:22], PPN=[21:0])

#### Scenario: Sv32 mode + satp_ppn=0 (RISC-V reset state, legal)

- **WHEN** `CPUConfig{enable_mmu=true, mmu_mode="sv32", satp_ppn=0}` (default value, RISC-V reset state)
- **THEN** `satp_value` MUST be `(1ULL << 31) | 0 = 0x80000000`
- **AND** PTW MUST fall back to identity translation via ADR-049 Bare shortcut (`satp_ppn_==0` triggers)
- **AND** CpuFactory MUST NOT fail-fast assert (this is the legal RISC-V reset state before OS writes satp)

#### Scenario: Sv39 mode + satp_ppn=P

- **WHEN** `CPUConfig{enable_mmu=true, mmu_mode="sv39", satp_ppn=P}` where P > 0
- **THEN** `satp_value` MUST be `(8ULL << 60) | (P & 0xFFFFFFFFFULL)` (Sv39 RV64 encoding: MODE=[63:60]=8, PPN=[43:0])

#### Scenario: Sv48 mode + satp_ppn=P

- **WHEN** `CPUConfig{enable_mmu=true, mmu_mode="sv48", satp_ppn=P}` where P > 0
- **THEN** `satp_value` MUST be `(9ULL << 60) | (P & 0xFFFFFFFFFULL)` (Sv48 RV64 encoding: MODE=[63:60]=9)

### Requirement: RiscvMMUPlugin ctor propagates satp to base class

`RiscvMMUPlugin` constructor MUST call `MMUPlugin::set_satp_value(satp_value)` and mode-aware `set_satp_ppn(extract_ppn(sv_mode, satp_value))` after delegating to base ctor. This ensures `MMUPlugin::satp_ppn_` is non-zero when `CPUConfig.satp_ppn>0`, preventing ADR-049 Bare shortcut from silently producing identity translation.

#### Scenario: ctor with satp_value=0 (Bare)

- **WHEN** `RiscvMMUPlugin` constructed with `satp_value=0`
- **THEN** `MMUPlugin::satp_ppn_` MUST be `0` (Bare shortcut triggers)

#### Scenario: ctor with satp_value=0x80080000 (Sv32, PPN=0x80000)

- **WHEN** `RiscvMMUPlugin` constructed with `sv_mode=Sv32, satp_value=0x80080000`
- **THEN** `MMUPlugin::satp_ppn_` MUST be `0x80000 & 0x3FFFFFULL = 0x80000` (mode-aware mask)
- **AND** PTW walk MUST read root page table at physical address `0x80000 << 12 = 0x80000000`

### Requirement: PicolibcHostMemory identity 4MB superpage leaf PTE plant helper

测试代码 SHALL 提供 `plant_identity_page_table(PicolibcHostMemory&, std::uint64_t pte_base, std::uint64_t vaddr)` helper, planting a single 4MB superpage leaf PTE into the root page table location `pte_base + VPN1*4`.

#### Scenario: 4MB-aligned vaddr, single superpage leaf PTE

- **WHEN** `plant_identity_page_table(mem, 0x8000F000, 0x80000000)` (pte_base=window_top, vaddr=ELF entry)
- **THEN** root PTE MUST be at `0x8000F000 + ((0x80000000 >> 22) & 0x3FF) * 4 = 0x8000F800`
- **AND** PTE value MUST be `((0x80000) << 10) | 0xDFu = 0x200000DF` (PPN=0x80000 in bits [31:10], flags V|R|W|X|U|A|D = 0xDF in bits [7:0])

#### Scenario: non-4MB-aligned vaddr (guard)

- **WHEN** `plant_identity_page_table(mem, base, vaddr)` where `vaddr & 0x3FFFFFULL != 0`
- **THEN** helper MUST throw `std::invalid_argument` (Sv32 4MB superpage requires PPN[9:0]=0)

### Requirement: CpuFactory unit test coverage

`tests/cpu/test_cpu_factory.cpp` SHALL include `CpuFactory_MMUCfg_PassesSatpValue` TEST_CASE with 5 SECTIONs matching the `make_satp_value` function.

#### Scenario: unit test runs

- **WHEN** ctest `[cpu]` runs
- **THEN** `CpuFactory_MMUCfg_PassesSatpValue` MUST PASS with 9 assertions across 5 SECTIONs (Sv32/Sv39/Sv48/Bare/Negative-PPN-masked)
- **AND** the test MUST catch silent regression if `cpu_factory.h:390` is re-hardcoded to `0`

### Requirement: v0.10.1 workaround preservation (本 change NOT removing workaround)

`tests/soc/test_cpu_l1_mmu_demo.cpp` MUST preserve v0.10.1 `cfg.enable_mmu = false` workaround. True sv32 translation e2e (Phase D.2) is **deferred** to follow-up `cpu-pipeline-mmufault-handler` due to CPU pipeline lacking MMU exception handler.

#### Scenario: workaround retained (current state)

- **WHEN** ctest `[cpu-l1-mmu-demo]` runs
- **THEN** 6/6 test cases MUST PASS (`add`/`addi`/`auipc`/`jal`/`beq`/`json_structure`)
- **AND** `cfg.enable_mmu = false` MUST be the live config
- **AND** header comment MUST document the CPU pipeline limitation

#### Scenario: follow-up tracking

- **WHEN** independent follow-up change `cpu-pipeline-mmufault-handler` is implemented
- **THEN** the `[cpu-l1-mmu-demo]` test MUST be updated to `cfg.enable_mmu = true` + `cfg.satp_ppn = ...` + `plant_identity_page_table(...)`
- **AND** the test MUST still PASS 6/6 (this verifies the MMU exception handler is wired correctly)

### Requirement: No regression in other 4 test families

`[cpu]`, `[cpu-integration]`, `[mmu]`, `[riscv-tests]` MUST NOT regress from current baselines.

#### Scenario: cpu family after change applied

- **WHEN** ctest `[cpu]` runs
- **THEN** MUST achieve ≥118 PASS (current baseline 117 + 1 new TEST_CASE `CpuFactory_MMUCfg_PassesSatpValue`)

#### Scenario: cpu-integration family after change applied

- **WHEN** ctest `[cpu-integration]` runs
- **THEN** MUST achieve ≥81 PASS (no regression)

#### Scenario: mmu family after change applied

- **WHEN** ctest `[mmu]` runs
- **THEN** MUST achieve ≥53 PASS (Bare shortcut behavior unchanged, no regression)

#### Scenario: riscv-tests family after change applied

- **WHEN** ctest `[riscv-tests]` runs
- **THEN** MUST achieve ≥40 PASS (uses `enable_mmu=false`, unaffected by change)

### Requirement: Compatibility with v0.10.1 (no breaking change)

This change MUST NOT break the v0.10.1 API surface: `cfg.enable_mmu=false` continues to skip MMU plugin registration, `mmu_mode` config remains the same enum, and PTE encoding (where used) remains unchanged.

#### Scenario: enable_mmu=false unchanged behavior

- **WHEN** `CPUConfig{enable_mmu=false}` (any `mmu_mode` value)
- **THEN** MMU plugin MUST NOT be registered in `register_early_plugins`
- **AND** behavior MUST be byte-identical to v0.10.1 (no MMU translation, direct vaddr → mem access)

#### Scenario: mmu_mode config backward compatible

- **WHEN** `CPUConfig{mode="sv39"}` (legacy default, no `satp_ppn` set, default 0)
- **THEN** `cfg.satp_ppn` MUST default to 0 (Bare shortcut fires, behavior byte-identical to v0.10.1)

