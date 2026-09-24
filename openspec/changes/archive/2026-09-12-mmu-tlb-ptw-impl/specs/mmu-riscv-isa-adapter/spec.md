## ADDED Requirements

### Requirement: RiscvMMUPlugin MUST intercept satp CSR writes

The `RiscvMMUPlugin` MUST intercept writes to the `satp` CSR (CSR address 0x180) and configure `sv_mode` (Bare/Sv32/Sv39/Sv48) + root page table `paddr` accordingly.

#### Scenario: satp write configures Sv39 root page table
- **WHEN** `at_stage("csr_write_satp")` is called with `value={MODE=Sv39, PPN=0x80001000}`
- **THEN** `MMUPlugin::sv_mode_` MUST be set to Sv39 and `MMUPlugin::root_paddr_` MUST be set to `0x80001000 << 12 = 0x8000_1000_0000`

#### Scenario: satp write to Bare disables translation
- **WHEN** `at_stage("csr_write_satp")` is called with `value={MODE=Bare, PPN=0}`
- **THEN** `MMUPlugin::sv_mode_` MUST be set to Bare and all subsequent vaddrs MUST bypass TLB lookup (treated as paddr directly)

#### Scenario: satp write invalidates TLB (ASID context switch)
- **WHEN** `at_stage("csr_write_satp")` is called with new satp value (different from current)
- **THEN** `MultiLevelTLB::invalidate_all()` MUST be invoked before the new satp takes effect (prevents stale address translations)

### Requirement: RiscvMMUPlugin MUST implement SFENCE.VMA as TLB invalidation

The `RiscvMMUPlugin` MUST map the SFENCE.VMA instruction to `MultiLevelTLB` invalidation operations based on rs1 (vaddr) and rs2 (asid) operands.

#### Scenario: SFENCE.VMA with rs1=vaddr, rs2=asid invalidates single entry
- **WHEN** `at_stage("sfence_vma")` is called with `rs1=vaddr=0x1000, rs2=asid=0x5`
- **THEN** `MultiLevelTLB::invalidate_vaddr(vaddr=0x1000, asid=0x5)` MUST be invoked

#### Scenario: SFENCE.VMA with rs1=-1 invalidates all
- **WHEN** `at_stage("sfence_vma")` is called with `rs1=-1` (x0 not selected)
- **THEN** `MultiLevelTLB::invalidate_all()` MUST be invoked regardless of rs2

#### Scenario: SFENCE.VMA with rs2=-1 invalidates all entries for one vaddr across all asids
- **WHEN** `at_stage("sfence_vma")` is called with `rs1=vaddr=0x1000, rs2=-1`
- **THEN** for each asid in [0..max_asid], `MultiLevelTLB::invalidate_vaddr(vaddr=0x1000, asid)` MUST be invoked

### Requirement: RiscvMMUPlugin MUST map exceptions 12/13/15 to pl::EXCEPTION_CODE

The `RiscvMMUPlugin` MUST map page-related RISC-V exceptions to `pl::EXCEPTION_CODE`:
- 12 = instruction page fault (exec access, page level)
- 13 = load page fault (read access, page level)
- 15 = store/AMO page fault (write access, page level)

(Distinguished from access fault codes 1/4/6 which are delegated to `RiscvCsrPlugin` integration, out of scope.)

#### Scenario: Exec page fault
- **WHEN** `at_stage("tlb_lookup_ifetch")` detects leaf PTE has `X=0` for exec access
- **THEN** `pl::EXCEPTION_CODE = 12` MUST be written

#### Scenario: Load page fault
- **WHEN** `at_stage("tlb_lookup_loadstore")` detects leaf PTE has `R=0` for load access
- **THEN** `pl::EXCEPTION_CODE = 13` MUST be written

#### Scenario: Store page fault
- **WHEN** `at_stage("tlb_lookup_loadstore")` detects leaf PTE has `W=0` for store access
- **THEN** `pl::EXCEPTION_CODE = 15` MUST be written

#### Scenario: Page table walk fault (reserved PTE encoding)
- **WHEN** Sv39 walk encounters PTE with reserved encoding (`R=1, W=1, X=1`)
- **THEN** `pl::EXCEPTION_CODE = 15` MUST be written (consistent with RISC-V spec)

### Requirement: RiscvMMUPlugin MUST register with CpuFactory PluginOrder

The `RiscvMMUPlugin` MUST be registered with `CpuFactory::PluginOrder` to integrate with the RISC-V CPU plugin suite (11-Plugin 套件). Registration MUST occur in the same atomic commit that adds `RiscvMMUPlugin`.

#### Scenario: RiscvMMUPlugin in PluginOrder
- **WHEN** `CpuFactory::build_cpu(config)` is called with `enable_mmu=true`
- **THEN** `RiscvMMUPlugin` MUST appear in `PluginOrder` and be registered with `PipeBuilder` via `pb.register_plugin(...)`

#### Scenario: mmu disabled bypasses RiscvMMUPlugin
- **WHEN** `CpuFactory::build_cpu(config)` is called with `enable_mmu=false`
- **THEN** `RiscvMMUPlugin` MUST NOT be registered; all vaddrs treated as paddr (Bare mode default)

### Requirement: RiscvMMUPlugin MUST NOT break existing 11-Plugin suite

The `RiscvMMUPlugin` integration MUST NOT regress any of the 11 existing CPU Plugin tests. **实测**: `[cpu]` 114 cases + `[cpu-configs]` 9 + `[cpu-integration]` 25 = **148 cases** total (revised from earlier incorrect "16"; binary run confirms 25).

#### Scenario: All existing CPU tests pass post-integration
- **WHEN** `./build/bin/chipforge_tests "[cpu]"` is run after RiscvMMUPlugin commits
- **THEN** all 114 `[cpu]` cases MUST pass; same for `[cpu-configs]` (9) and `[cpu-integration]` (25)
