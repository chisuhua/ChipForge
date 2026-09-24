# cpu-stage-ordered-scheduling Specification

## Purpose
TBD - created by archiving change cpu-pipeline-stubs-replace. Update Purpose after archive.
## Requirements
### Requirement: PipeBuilder::run() MUST execute callbacks in canonical stage order × phase order

`PipeBuilder::run()` MUST invoke registered callbacks in canonical pipeline order, not pure insertion order. Canonical order = (stage_first_occurrence_order) × (Phase enum order: EARLY→NORMAL→LATE). Stage order MUST be derived from first-occurrence order in the registered callbacks array (e.g., the first `at_stage("fetch", ...)` call establishes fetch as the first stage; the first `at_stage("decode", ...)` establishes decode as the second stage; and so on). The Phase enum value of each callback MUST be honored: callbacks registered with EARLY run before NORMAL within the same stage, which run before LATE. This replaces the current implementation (`pipe_builder.h:100-109`) which calls callbacks in raw insertion order.

#### Scenario: Stage-first-occurrence order determines canonical execution
- **WHEN** Plugin A's `build()` registers `at_stage("decode", NORMAL)` closure first, then Plugin B's `build()` registers `at_stage("fetch", NORMAL)` closure, then `pb.run()` is called
- **THEN** the fetch closure (Plugin B) MUST execute before the decode closure (Plugin A), because fetch was first declared as a stage even though it was registered later

#### Scenario: Phase ordering within a stage
- **WHEN** two plugins register at_stage("decode", EARLY) and at_stage("decode", NORMAL) closures respectively, and `pb.run()` is called
- **THEN** the EARLY closure MUST execute before the NORMAL closure within the same stage

#### Scenario: Multi-thread execution preserves stage order
- **WHEN** `pb.run()` is called with `n_threads=4` and 12 callbacks spanning 5 stages
- **THEN** each thread executes its assigned callbacks in canonical stage order (no thread sees a writeback closure execute before an execute closure completes)

### Requirement: Stage first-occurrence order derivation MUST be deterministic

The canonical stage order MUST be derived from the first occurrence of each unique stage name string in the registered callbacks array (insertion order). This MUST be computed at `pb.run()` start and cached for the duration of that run() invocation. New `at_stage("newstage", ...)` registrations after `pb.run()` have started MUST NOT alter the in-progress execution order.

#### Scenario: New stage introduced mid-pipeline doesn't break ordering
- **WHEN** `pb.run()` begins with callbacks for stages {fetch, decode, execute} and a new `at_stage("memory", ...)` is registered before completion of the fetch stage
- **THEN** the fetch, decode, execute closures MUST execute in canonical order for the current run; the new memory closure MUST NOT execute until the next `pb.run()` call

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

