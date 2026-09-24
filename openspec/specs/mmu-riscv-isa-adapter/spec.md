# mmu-riscv-isa-adapter Specification

## Purpose
TBD - created by archiving change mmu-tlb-ptw-impl. Update Purpose after archive.
## Requirements
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

The `RiscvMMUPlugin` MUST be registered with `CpuFactory::PluginOrder` to integrate with the RISC-V CPU plugin suite (11-Plugin 套件). Registration MUST occur in the same atomic commit that adds `RiscvMMUPlugin`. **EXPANDED (cpu-mmu-integration)**: Registration is CONDITIONAL on `config.enable_mmu`; when `config.enable_mmu=false`, the plugin MUST NOT be registered (preserves bit-identical 25-test [cpu-integration] baseline).

#### Scenario: RiscvMMUPlugin in PluginOrder when enable_mmu=true
- **WHEN** `CpuFactory::build_cpu(config)` is called with `enable_mmu=true`
- **THEN** `RiscvMMUPlugin` MUST appear in `PluginOrder` and be registered with `PipeBuilder` via `pb.register_plugin(...)` with SvMode mapped from `config.mmu_mode`

#### Scenario: mmu disabled bypasses RiscvMMUPlugin (bit-identical baseline)
- **WHEN** `CpuFactory::build_cpu(config)` is called with `enable_mmu=false`
- **THEN** `RiscvMMUPlugin` MUST NOT be registered; all vaddrs treated as paddr (Bare mode default); all 25 [cpu-integration] tests MUST pass without modification

#### Scenario: RiscvMMUPlugin at_stage substages registered with CPU pipeline
- **WHEN** `RiscvMMUPlugin` is registered with PipeBuilder
- **THEN** three substages MUST be declared via `pb.declare_substage()`: `csr_write_satp` (parent: `execute`), `sfence_vma` (parent: `execute`), `mmu_exit` (parent: `memory`)
- **THEN** `RiscvMMUPlugin::at_stage("csr_write_satp")` MUST read `cpu_keys::SAT` from lookup node and invoke `RiscvMMUPlugin::csr_write_satp(satp_value)`
- **THEN** `RiscvMMUPlugin::at_stage("sfence_vma")` MUST read `cpu_keys::SFENCE_VADDR`/`SFENCE_ASID` and invoke `RiscvMMUPlugin::sfence_vma(rs1_vaddr, rs2_asid)` with the 4-way RISC-V Spec §6.2 dispatch
- **THEN** `RiscvMMUPlugin::at_stage("mmu_exit")` MUST read `mmu_keys::EXCEPTION_CODE` and propagate to `cpu_keys::CPU_EXCEPTION_CODE`

### Requirement: RiscvMMUPlugin MUST NOT break existing 11-Plugin suite

The `RiscvMMUPlugin` integration MUST NOT regress any of the 11 existing CPU Plugin tests when `enable_mmu=false` is used (bit-identical baseline). When `enable_mmu=true`, the existing tests MUST still pass (with possible new stages in `pb.has_stage()` queries).

#### Scenario: enable_mmu=false preserves all existing CPU tests
- **WHEN** `./build/bin/chipforge_tests "[cpu-integration]"` is run after RiscvMMUPlugin commits with `enable_mmu=false`
- **THEN** all 25 `[cpu-integration]` cases MUST pass without modification (bit-identical baseline)

#### Scenario: enable_mmu=true adds 8 new cpu-mmu-integration tests
- **WHEN** the new `cpu-mmu-integration` change lands with 5 stage (3-stage + 5-stage + 5-stage-disabled + 7-stage + 10-stage) + 3 RiscV hook integration tests
- **THEN** `./build/bin/chipforge_tests "[cpu-integration]"` MUST show 33 cases (25 baseline + 8 new) all PASS

#### Scenario: enable_mmu=true path updates node/stage/plugin count assertions (B2 contract)
- **WHEN** MMUPlugin is registered (`enable_mmu=true` default), the resulting PipeBuilder MUST have `node_count()` and `stage_count()` reflect the new substages (5 from MMUPlugin::setup + 3 from RiscvMMUPlugin::at_stage closures); `CpuFactory::build_cpu()` MUST return exactly **12 plugins** (11 baseline + RiscvMMUPlugin)
- **THEN** the following assertions MUST be updated accordingly:
  - `tests/cpu/integration/test_7stage_riscv.cpp` `node_count()` / `stage_count()` assertions reflect the +5/+5 increase (commit 1 task 1.5)
  - `tests/cpu/integration/test_10stage_riscv.cpp` `10stage_topology_from_config` audited for `cpu_deep_pipeline.json` `enable_mmu:true` path (commit 1 task 1.6)
  - `tests/cpu/test_cpu_factory.cpp` `build_cpu_registers_11_real_plugins` `plugins.size()` updated from `==11` to `==12` (commit 1 task 1.7)
- **THEN** all `enable_mmu=true` path tests pass with the updated counts; `enable_mmu=false` baseline (25 `[cpu-integration]` cases) MUST remain bit-identical (no MMU registration)

