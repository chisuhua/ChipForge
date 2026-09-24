## ADDED Requirements

### Requirement: Plugin SHALL declare multi-cycle opcodes via latency_table() virtual function

Plugins MAY override `cf::plugin::PluginBase::latency_table()` to declare which opcodes have multi-cycle latency (latency > 1). The default implementation returns an empty `std::unordered_map<uint32_t, uint32_t>`, indicating all opcodes execute in 1 cycle. Plugins with multi-cycle instructions (e.g., MUL, DIV, MOD, REM in RV32M extension) SHALL override this function to return a map from opcode (or decoded opcode class enum) to cycle count.

#### Scenario: Plugin without override has empty latency_table
- **WHEN** a Plugin inherits from `cf::plugin::PluginBase` without overriding `latency_table()`
- **THEN** `latency_table()` SHALL return an empty map
- **AND** the framework SHALL treat all opcodes as 1-cycle (no stall injection)

#### Scenario: IntAluPlugin overrides latency_table for DIV family
- **WHEN** `IntAluPlugin` overrides `latency_table()`
- **THEN** the returned map SHALL include `{DIV → 35, DIVU → 35, REM → 35, REMU → 35}` (per RISC-V spec conservative upper bound)
- **AND** MUL/MULH/MULHU/MULHSU SHALL remain 1-cycle
- **AND** ADD/SUB/etc. ALU ops SHALL remain 1-cycle

### Requirement: PipeBuilder SHALL consume latency_table() to inject N-1 stall cycles after multi-cycle opcodes

`PipeBuilder::run_with_latency()` (or `run()` when latency injection is enabled) SHALL read each Plugin's `latency_table()` to determine which opcodes require multi-cycle stall. When an execute-stage opcode is detected to have `latency > 1`, the runner SHALL inject `latency - 1` stall cycles via `CtrlLink::halt_when(in_flight_opcode_active)` registered on ALL pipeline stages (IF/ID/EX/MEM/WB) to prevent pipeline advancement during the multi-cycle operation.

#### Scenario: DIV instruction stalls for 35 cycles
- **WHEN** execute stage encounters a DIV opcode (with `latency_table()[DIV] = 35`)
- **THEN** the execute stage SHALL halt for 35 cycles total (cycle 0 execute + 34 stall cycles)
- **AND** all 5 stages (IF/ID/EX/MEM/WB) SHALL register `halt_when(div_active)` CtrlLinks
- **AND** the stall SHALL propagate to fetch stage (no new instruction fetched until DIV completes)

#### Scenario: MUL instruction completes in 1 cycle
- **WHEN** execute stage encounters a MUL opcode (with `latency_table()[MUL] = 1`)
- **THEN** no stall SHALL be injected (1-cycle default behavior)
- **AND** subsequent instructions can proceed in the next cycle

#### Scenario: Multi-cycle opcodes do not overlap
- **WHEN** execute stage encounters DIV (35 cycles) followed by another DIV (35 cycles)
- **THEN** the second DIV SHALL NOT start until the first DIV completes (35 cycles after first)
- **AND** no overlap between the two DIV operations SHALL occur
- **AND** total cycle count SHALL be exactly 70 cycles for both DIV operations