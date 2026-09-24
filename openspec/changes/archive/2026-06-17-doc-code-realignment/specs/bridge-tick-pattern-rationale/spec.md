## ADDED Requirements

### Requirement: Bridge Pattern Allows tick() Adaptation

The project MUST distinguish between two patterns for components that integrate with the CppTLM EventQueue simulation loop:

1. **Business Plugin-style** (strict): Inherits from `cf::plugin::PluginBase`, MUST NOT implement `tick()`, business logic registered via `at_stage()` callbacks. Examples: `L1CachePlugin`, `cpu/plugins/*`.

2. **Bridge Adapter-style** (relaxed): A Bridge class that adapts a Plugin-style component to the CppTLM `ChStreamModuleBase` interface. Bridge classes MAY implement `tick()` to receive `EventQueue` cycle callbacks, but the `tick()` body MUST only do protocol conversion and delegate to the inner Plugin's `pb.run()`. Examples: `L1CacheTLMBridge`, `L1CacheTLMBridgeAdapter`.

#### Scenario: Business Plugin has no tick()
- **WHEN** a developer inspects a class inheriting from `cf::plugin::PluginBase`
- **THEN** the class MUST NOT define a `void tick()` method (D4 enforcement via `private: void tick() = delete`)
- **AND** any attempt to override `tick()` MUST fail at compile time

#### Scenario: Bridge class implements tick() with delegation pattern
- **WHEN** `L1CacheTLMBridge::tick()` is called by the CppTLM `EventQueue`
- **THEN** the body MUST contain exactly one call to `plugin_->pb.run()` (or equivalent) at the end
- **AND** MUST NOT contain business state mutation logic
- **AND** MUST NOT directly call `ch_mem` / `ch_reg` / `ch_uint` (RTL primitives forbidden in TLM business code per ADR-040 Tier-1)

#### Scenario: Bridge and Plugin are kept in separate namespaces/files
- **WHEN** a new Bridge class is added to the project
- **THEN** the Bridge class MUST be located under `src/cf_plugin/bridge/` (per ADR-040 §D2=B)
- **AND** the inner Plugin MUST be located under `ip/<name>/tlm/`
- **AND** Bridge class MUST NOT be in `ip/<name>/` (which is subject to D4 enforcement)
