## ADDED Requirements

### Requirement: cpu_superscalar.json validates 2-wide dispatch

`ip/cpu/configs/cpu_superscalar.json` SHALL define a 2-wide superscalar CPU configuration with `dispatch_width=2` and `n_lanes=2`, validating against `cpu_params_schema.json` 9 new fields including `dispatch_width`, `n_lanes`, `fetch_width`, `commit_width`, `retire_width`.

`cpu_params_schema.json` SHALL accept the 9 new fields as optional, with defaults matching in-order baseline (0 or 1), so existing JSON configs (`cpu_default.json`, `cpu_embedded.json`) continue to validate byte-identically. Three additional fields (`mul_latency`, `icache_latency_cycles`, `dcache_latency_cycles`) already exist in the schema; the `icache_latency_cycles` / `dcache_latency_cycles` fields SHALL be aligned with the `CPUConfig` struct's `icache_latency` / `dcache_latency` naming (no `_cycles` suffix) per design.md Decision 5.

#### Scenario: cpu_superscalar.json validates with 2-wide fields

- **WHEN** `cpu_superscalar.json` is parsed by `parse_config` and validated by `cpu_params_schema.json` (via ajv)
- **THEN** validation SHALL succeed
- **AND** the resulting `CPUConfig` SHALL expose `dispatch_width=2`, `n_lanes=2`, `fetch_width=2`, `commit_width=2`, `retire_width=2`

#### Scenario: existing configs validate unchanged

- **WHEN** `cpu_default.json` and `cpu_embedded.json` are validated against the updated `cpu_params_schema.json`
- **THEN** validation SHALL succeed
- **AND** the absent 9 new fields SHALL be filled with their defaults (0/1)
- **AND** the resulting `CPUConfig` SHALL be byte-equivalent to the pre-M5-DSE configuration

### Requirement: cpu_deep_pipeline.json validates 10-stage config

`ip/cpu/configs/cpu_deep_pipeline.json` SHALL define a 10-stage deep-pipeline configuration with `pipeline_stages=10` and at least 3 multi-cycle MUL sub-pipe stages.

#### Scenario: cpu_deep_pipeline.json validates with 10-stage fields

- **WHEN** `cpu_deep_pipeline.json` is parsed and validated
- **THEN** validation SHALL succeed
- **AND** the resulting `CPUConfig` SHALL expose `pipeline_stages=10` and `mul_latency >= 3`
