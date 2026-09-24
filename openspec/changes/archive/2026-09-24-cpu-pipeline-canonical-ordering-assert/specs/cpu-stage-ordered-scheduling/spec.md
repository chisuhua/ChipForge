## MODIFIED Requirements

### Requirement: Plugin registration order MUST be canonical (MMUPlugin before IBusPlugin/DBusPlugin)

The canonical stage order in `PipeBuilder::run()` is determined by the first-occurrence order of stage names in registered callbacks (per the cpu-stage-ordered-scheduling spec). This order is sensitive to the order in which Plugins' `build()` methods register their `at_stage` callbacks. To prevent silent regression of the MMU stall mechanism (which depends on `tlb_lookup_ifetch` appearing before `fetch` in canonical order), Plugin registration order MUST be canonical:

1. **MMUPlugin** (`RiscvMMUPlugin`) MUST register its `at_stage("tlb_lookup_ifetch", ...)` and `at_stage("tlb_lookup_loadstore", ...)` callbacks BEFORE `IBusPlugin` and `DBusPlugin` register their `at_stage("fetch", ...)` and `at_stage("memory", ...)` callbacks respectively.
2. **`cpu_factory.h::register_early_plugins()` MUST enforce this order** via `static_assert` (compile-time check) on registration sequence counters.
3. **The check MUST be a hard error** at compile time, not a runtime warning.

#### Scenario: Correct order compiles successfully
- **WHEN** `cpu_factory.h::register_early_plugins()` is invoked with `MMUPlugin::build()` registering callbacks before `IBusPlugin::build()`
- **THEN** the `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, ...)` MUST pass at compile time
- **AND** no warning or error MUST be emitted

#### Scenario: Incorrect order fails to compile
- **WHEN** `cpu_factory.h::register_early_plugins()` is invoked with `IBusPlugin::build()` registering callbacks before `MMUPlugin::build()`
- **THEN** the `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, ...)` MUST fail at compile time
- **AND** the compiler MUST emit an error message identifying the canonical ordering requirement
- **AND** no object file MUST be produced