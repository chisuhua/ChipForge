## ADDED Requirements

### Requirement: cpu_params_schema.json MUST define enable_mmu and mmu_mode fields with conditional dependency

The `ip/cpu/configs/cpu_params_schema.json` MUST define `enable_mmu` (boolean, default `true`) and `mmu_mode` (string, enum `["sv32", "sv39", "sv48"]`, default `"sv39"`) fields, with conditional dependencies enforcing that `mmu_mode` is REQUIRED when `enable_mmu=true`. (Already implemented in `cpu_params_schema.json:42-52, 203-204`; this spec records the contract.)

#### Scenario: enable_mmu field present with boolean type
- **WHEN** `cpu_params_schema.json` is parsed
- **THEN** `properties.enable_mmu` MUST exist with `type=boolean` and `default=true`

#### Scenario: mmu_mode field present with sv32/sv39/sv48 enum
- **WHEN** `cpu_params_schema.json` is parsed
- **THEN** `properties.mmu_mode` MUST exist with `type=string` and `enum=["sv32", "sv39", "sv48"]` and `default="sv39"`

#### Scenario: enable_mmu=true requires mmu_mode
- **WHEN** the schema validates a config with `enable_mmu=true` but no `mmu_mode`
- **THEN** validation MUST fail (mmu_mode is REQUIRED when enable_mmu=true)

#### Scenario: enable_mmu=true triggers RiscvMMUPlugin registration (already in production)
- **WHEN** `build_cpu()` is called with `enable_mmu=true`
- **THEN** `RiscvMMUPlugin` MUST be registered in the PluginOrder array (per `cpu-mmu-pipeline-registration` spec requirement)

#### Scenario: enable_mmu=false does NOT require mmu_mode
- **WHEN** the schema validates a config with `enable_mmu=false` and no `mmu_mode`
- **THEN** validation MUST succeed (mmu_mode is NOT required when MMU is disabled)