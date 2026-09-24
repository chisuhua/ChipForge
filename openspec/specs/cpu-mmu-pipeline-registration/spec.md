# cpu-mmu-pipeline-registration Specification

## Purpose
TBD - created by archiving change cpu-mmu-integration. Update Purpose after archive.
## Requirements
### Requirement: cpu_factory MUST conditionally register RiscvMMUPlugin when enable_mmu=true

The `cpu_factory.h::build_cpu()` function MUST register `RiscvMMUPlugin` in the PluginOrder array when `config.enable_mmu == true`, and MUST NOT register it when `config.enable_mmu == false`. The registration MUST map `config.mmu_mode` ("sv32"/"sv39"/"sv48") to the corresponding `cf::ip::mmu::SvMode` value for the MMUPlugin constructor.

#### Scenario: enable_mmu=true registers RiscvMMUPlugin
- **WHEN** `build_cpu()` is called with `config.enable_mmu=true` and `config.mmu_mode="sv39"`
- **THEN** the PluginOrder array MUST contain `RiscvMMUPlugin`; the MMUPlugin constructor MUST be invoked with `SvMode::Sv39` and 2-level TLB 8/8 + LRU + ptw_max_inflight=2

#### Scenario: enable_mmu=false preserves bit-identical baseline
- **WHEN** `build_cpu()` is called with `config.enable_mmu=false`
- **THEN** the PluginOrder array MUST NOT contain `RiscvMMUPlugin`; the resulting PipeBuilder MUST be bit-identical to the pre-change baseline (all 25 `[cpu-integration]` tests PASS without modification)

#### Scenario: mmu_mode mapping covers sv32/sv39/sv48
- **WHEN** `config.mmu_mode` is set to each of `"sv32"`, `"sv39"`, `"sv48"`
- **THEN** the corresponding `cf::ip::mmu::SvMode` value MUST be passed to the MMUPlugin constructor (`SvMode::Sv32`, `SvMode::Sv39`, `SvMode::Sv48` respectively)

### Requirement: RiscvMMUPlugin MUST declare 3 substages for hook routing

`RiscvMMUPlugin::setup()` MUST declare three substages via `pb.declare_substage()`: `csr_write_satp` (parent: `execute`), `sfence_vma` (parent: `execute`), `mmu_exit` (parent: `memory`). These substages enable the CPU pipeline to dispatch RISC-V CSR writes and SFENCE.VMA instructions to the MMU hook layer.

#### Scenario: 5-stage pipeline includes 3 MMU substages
- **WHEN** `build_cpu()` is called with `enable_mmu=true` and `pipeline_stages=5`
- **THEN** `pb.has_stage("csr_write_satp")`, `pb.has_stage("sfence_vma")`, and `pb.has_stage("mmu_exit")` MUST all return `true`

#### Scenario: 7-stage pipeline includes 3 MMU substages
- **WHEN** `build_cpu()` is called with `enable_mmu=true` and `pipeline_stages=7`
- **THEN** all 3 MMU substages MUST be declared

#### Scenario: 10-stage pipeline includes 3 MMU substages
- **WHEN** `build_cpu()` is called with `enable_mmu=true` and `pipeline_stages=10`
- **THEN** all 3 MMU substages MUST be declared

