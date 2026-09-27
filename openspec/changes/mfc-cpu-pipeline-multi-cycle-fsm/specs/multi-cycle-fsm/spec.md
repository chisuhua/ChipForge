# mfc-cpu-pipeline-multi-cycle-fsm Specification

## Purpose
TBD - created by archiving change mfc-cpu-pipeline-multi-cycle-fsm. Update Purpose after archive.

## ADDED Requirements

### Requirement: MulDivFsmPlugin SHALL use ch_state_machine DSL for MUL/DIV state machine (ADR-046)

`MulDivFsmPlugin` SHALL describe the MUL/DIV state machine using `ch_state_machine` DSL (not ad-hoc stall counter). States: `IDLE → MULTIPLY | DIVIDE(33 cycle) → WRITE_BACK`. State transitions SHALL be elaborated at `pb.elaborate(ctx)` time and emit Verilog `always_ff @(posedge clk)` in CH_MEM mode. TLM mode SHALL walk the same state machine via `pb.run()` cycle iterations.

#### Scenario: MUL completes in 1 cycle
- **WHEN** RV32M `MUL` instruction issued with operands set
- **THEN** state machine `IDLE → MULTIPLY → WRITE_BACK` SHALL transition in 1 cycle total
- **AND** result Payload Key SHALL equal the 32-bit lower-half product

#### Scenario: DIV completes in 33 cycle
- **WHEN** RV32M `DIV` instruction issued with operands set
- **THEN** state machine `IDLE → DIVIDE × 33 cycles → WRITE_BACK` SHALL transition in 33 cycles total
- **AND** result Payload Key SHALL equal integer quotient

#### Scenario: TLM-CH_MEM cycle parity
- **WHEN** identical MUL/DIV input sequence applied to TLM and CH_MEM modes
- **THEN** cycle count SHALL differ by at most 0 (TLM and CH_MEM Verilator sim match exactly)

### Requirement: MulDivFsmPlugin SHALL declare capability via Plugin::negotiate() (ADR-082)

`MulDivFsmPlugin::negotiate(CapabilityTable& cap)` SHALL declare `provides` and `requires` capabilities. Top-level `PipeBuilder::build()` SHALL call `negotiate()` on all registered Plugin instances **before** calling `build()`, in topological order derived from `requires` edges. Plugins that fail to satisfy `requires` SHALL cause `PipeBuilder::build()` to throw `PluginException` carrying a `PluginError` from `MulDivResult::err(...)`.

#### Scenario: negotiate() called before build()
- **WHEN** `PipeBuilder::build()` invoked with `MulDivFsmPlugin` registered
- **THEN** `MulDivFsmPlugin::negotiate(cap)` SHALL be invoked before `MulDivFsmPlugin::build(pb)`
- **AND** all `requires` references SHALL be checked against `provides` from other registered Plugins

#### Scenario: missing flush_broadcaster fails fast
- **WHEN** `MulDivFsmPlugin::requires` includes `flush_broadcaster`
- **AND** no other Plugin in the registry `provides` `flush_broadcaster`
- **THEN** `PipeBuilder::build()` SHALL throw `PluginException("missing provider flush_broadcaster")` at elaboration time

#### Scenario: dynamic_cast forbidden by CI gate
- **WHEN** any `build()` method body in the codebase contains a `dynamic_cast` call
- **THEN** `tools/verify_plugin_decision.sh` (CI gate #8) SHALL exit with non-zero status
- **AND** the PR SHALL be blocked from merging

### Requirement: MulDivFsmPlugin SHALL use MulDivResult for elaboration-time fail-fast (ADR-047)

`MulDivFsmPlugin::build(pb)` SHALL start with `MulDivResult` validation of `cfg.xlen == 32` and any capability requirements derived from `negotiate()`. Validation failures SHALL throw `PluginException` (not silently continue). This eliminates the "runtime crash vs my-bug" ambiguity from VexRiscv's plugin system.

#### Scenario: cfg.xlen != 32 fails fast
- **WHEN** `cfg.xlen == 64` passed to `MulDivFsmPlugin::build(pb)`
- **THEN** `MulDivResult::err("MulDivFsmPlugin requires RV32")` SHALL be returned
- **AND** `PipeBuilder::build()` SHALL throw `PluginException("MulDivFsmPlugin requires RV32")`

#### Scenario: cfg.xlen == 32 succeeds
- **WHEN** `cfg.xlen == 32` passed
- **THEN** `MulDivResult::ok()` SHALL be returned
- **AND** FSM instantiation proceeds to `ch_state_machine` elaboration

### Requirement: RV32M extension SHALL be functional in riscv-tests (PoC-1 hard gate)

`riscv-tests rv32um-p-*` (mul/mulh/mulhsu/mulhu/div/divu/rem/remu) SHALL 100% PASS via `[riscv-tests]` family ctest. Current stub state (FAIL with `category=feature_stub`) SHALL transition to PASS through the FSM implementation above.

#### Scenario: 8 rv32um tests PASS
- **WHEN** ctest `[riscv-tests]` invoked (filtered to rv32um-p-* subset, 8 cases)
- **THEN** all 8 cases SHALL PASS
- **AND** bus-cycles Payload Key SHALL reflect MUL=1c, DIV/DIVU/REM/REMU=33c per instruction

#### Scenario: Dhrystone baseline ≥1.4 DMIPS/MHz
- **WHEN** Dhrystone benchmark runs on 5-stage CH_MEM configuration (Verilator sim)
- **THEN** DMIPS/MHz SHALL be ≥1.4
- **AND** stall during DIV SHALL account for <5% of total cycles