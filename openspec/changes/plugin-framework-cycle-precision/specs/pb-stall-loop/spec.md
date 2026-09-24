## MODIFIED Requirements

### Requirement: PipeBuilder::run(cycle_count=N) SHALL execute N cycles and update CURRENT_CYCLE counter

`PipeBuilder::run()` SHALL accept an optional `cycle_count_t cycles = 1` parameter that controls the number of cycles to execute. For each cycle, the runner SHALL traverse all registered callbacks in canonical stage order × phase order (per the cpu-stage-ordered-scheduling spec), apply stall checks per stage, then increment the `cf::plugin::CURRENT_CYCLE` Payload counter by 1. After `cycles` iterations complete, `run()` SHALL return `Result<void>`. If `throw_when` triggers during any cycle, `run()` SHALL throw `PluginException` immediately and stop the cycle loop without incrementing `CURRENT_CYCLE` further.

#### Scenario: Single cycle run updates CURRENT_CYCLE to 1
- **WHEN** `pb.run()` is called with default `cycles = 1` (no argument)
- **THEN** after the run completes, `cf::plugin::CURRENT_CYCLE` SHALL equal 1
- **AND** `pb.run()` SHALL be equivalent to `pb.run(1)`

#### Scenario: Multi-cycle run updates CURRENT_CYCLE to N
- **WHEN** `pb.run(1000)` is called
- **THEN** after the run completes, `cf::plugin::CURRENT_CYCLE` SHALL equal 1000
- **AND** exactly 1000 stage × phase iterations SHALL have executed

#### Scenario: throw_when stops the cycle loop
- **WHEN** `pb.run(1000)` is called
- **AND** at cycle 5, a `throw_when` CtrlLink triggers
- **THEN** `pb.run()` SHALL throw `PluginException` after cycle 5 completes (or before cycle 5 begins, depending on evaluation order)
- **AND** `CURRENT_CYCLE` SHALL equal the cycle count when the throw occurred (e.g., 5)
- **AND** cycles 6-1000 SHALL NOT execute

### Requirement: Default `pb.run()` with no argument SHALL preserve back-compat behavior

`pb.run()` called with no argument SHALL be exactly equivalent to `pb.run(1)`, executing exactly one cycle and incrementing `CURRENT_CYCLE` by 1. This preserves the pre-cycle-precision behavior where each `pb.run()` call advances the simulation by one cycle.

#### Scenario: Back-compat equivalence
- **WHEN** `pb.run()` is called (legacy code, no argument)
- **THEN** exactly one cycle SHALL execute
- **AND** `CURRENT_CYCLE` SHALL increment by 1 (from previous value to previous+1)
- **AND** existing tests written before cycle-precision feature SHALL pass unchanged