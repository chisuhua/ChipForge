## MODIFIED Requirements

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