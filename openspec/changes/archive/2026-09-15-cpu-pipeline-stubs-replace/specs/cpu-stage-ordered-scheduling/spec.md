## ADDED Requirements

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
