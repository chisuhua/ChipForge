## ADDED Requirements

### Requirement: Compile-time topology depth selection

`cpu_factory.h` SHALL provide a `TopologyBuilder<N_STAGES>` template that expands the CPU pipeline to exactly `N_STAGES` physical `PipeNode` instances at compile time, supporting `N_STAGES ∈ {3, 5, 7, 10}` as the four canonical depths.

`build_cpu(config)` SHALL dispatch on `config.pipeline_stages` to one of the four pre-instantiated topologies, with behavior byte-identical to M4G-extend baseline when `pipeline_stages == 5` (default).

#### Scenario: 5-stage topology produces 5 nodes

- **WHEN** `CpuFactory::build_cpu` is called with `config.pipeline_stages == 5`
- **THEN** the resulting `PipeBuilder` SHALL contain exactly 5 `PipeNode` instances, one per logical stage (`fetch`, `decode`, `execute`, `memory`, `writeback`)
- **AND** the existing 36/36 ctest SHALL continue to pass byte-identically

#### Scenario: 7-stage topology produces 7 nodes including retire

- **WHEN** `CpuFactory::build_cpu` is called with `config.pipeline_stages == 7`
- **THEN** the resulting `PipeBuilder` SHALL contain 7 `PipeNode` instances, where `writeback` and `commit` share a physical `RETIRE` node per `multi_isa_architecture.md §2.4` 7-row table

#### Scenario: 10-stage topology produces 10 or more nodes

- **WHEN** `CpuFactory::build_cpu` is called with `config.pipeline_stages == 10`
- **THEN** the resulting `PipeBuilder` SHALL contain at least 10 `PipeNode` instances with deep-pipeline splits (≥3 sub-pipe stages between `fetch` and `execute`)

#### Scenario: 3-stage topology produces 3 nodes for embedded

- **WHEN** `CpuFactory::build_cpu` is called with `config.pipeline_stages == 3`
- **THEN** the resulting `PipeBuilder` SHALL contain 3 `PipeNode` instances, where `fetch`/`decode` share `IF` and `execute`/`memory` share `EXMEM` per `multi_isa_architecture.md §2.4` 3-row table
