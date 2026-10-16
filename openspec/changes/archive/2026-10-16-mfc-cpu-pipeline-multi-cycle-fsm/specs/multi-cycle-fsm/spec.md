# mfc-cpu-pipeline-multi-cycle-fsm Specification

## Purpose
TBD - created by archiving change mfc-cpu-pipeline-multi-cycle-fsm. Update Purpose after archive.

> **Phase-A 过渡条款（Metis 2026-09-28 审查后修订，spec v0.2 草案）**
>
> 本 change 分 Phase A-H 实施。**Requirement 1 (ch_state_machine DSL)** 的强制约束**仅在 Phase B.2 完成后生效**：
> - Phase A 允许 `enum class State + switch(state_)` ad-hoc 实现（TLM 模式）作为骨架过渡，**但必须在 `ip/cpu/arch/riscv/mul_div_fsm.h` 文件头显式声明 `#define CF_PLUGIN_USE_FSM_EXEMPT` 标记（ADR-046 v2.0 §2.1.1 算术多周期豁免）**
> - Phase A 测试 `[cpu][mul-div-fsm]` 不强制验证 ch_state_machine DSL 行为，但必须验证 FSM 状态转换正确性（IDLE→MULTIPLY→WRITE_BACK）
> - Phase B.2 [GREEN] 把 Phase A 的 ad-hoc 计数器改造为 `chlib::ch_state_machine` DSL，验收时强制走 DSL 路径
>
> **Phase-A 实施窗口的硬约束**（不能等到 Phase B 才做）：
> 1. **MUL=1 cycle / DIV=33 cycle** 必须实测 PASS（ad-hoc counter 精度足够）
> 2. **`busy-cycles` Payload Key** 必须在 Phase A 定义（Plugin 内部 namespace，避免污染 framework）
> 3. **TLM-CH_MEM cycle parity**：Phase A 不验证（CH_MEM 实现推迟到 Phase D）
> 4. **CI 门禁**：Phase A 通过 `tools/verify_plugin_decision.sh`（D4 合规 + dynamic_cast=0）

## ADDED Requirements

### Requirement: MulDivFsmPlugin uses ch_state_machine DSL for MUL/DIV state machine (ADR-046)

`MulDivFsmPlugin` SHALL describe the MUL/DIV state machine using `ch_state_machine` DSL (not ad-hoc stall counter). States: `IDLE → MULTIPLY | DIVIDE(33 cycle) → WRITE_BACK`. State transitions SHALL be elaborated at `pb.elaborate(ctx)` time and emit Verilog `always_ff @(posedge clk)` in CH_MEM mode. TLM mode SHALL walk the same state machine via `pb.run()` cycle iterations.

> **过渡条款见上文 Purpose §Phase-A 过渡条款**

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

### Requirement: MulDivFsmPlugin declares capability via Plugin::negotiate() (ADR-082)

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

### Requirement: MulDivFsmPlugin uses MulDivResult for elaboration-time fail-fast (ADR-047)

`MulDivFsmPlugin::build(pb)` SHALL start with `MulDivResult` validation of `cfg.xlen == 32` and any capability requirements derived from `negotiate()`. Validation failures SHALL throw `PluginException` (not silently continue). This eliminates the "runtime crash vs my-bug" ambiguity from VexRiscv's plugin system.

#### Scenario: cfg.xlen != 32 fails fast
- **WHEN** `cfg.xlen == 64` passed to `MulDivFsmPlugin::build(pb)`
- **THEN** `MulDivResult::err("MulDivFsmPlugin requires RV32")` SHALL be returned
- **AND** `PipeBuilder::build()` SHALL throw `PluginException("MulDivFsmPlugin requires RV32")`

#### Scenario: cfg.xlen == 32 succeeds
- **WHEN** `cfg.xlen == 32` passed
- **THEN** `MulDivResult::ok()` SHALL be returned
- **AND** FSM instantiation proceeds to `ch_state_machine` elaboration

### Requirement: RV32M extension is functional in riscv-tests (PoC-1 hard gate)

`riscv-tests rv32um-p-*` (mul/mulh/mulhsu/mulhu/div/divu/rem/remu) SHALL 100% PASS via `[riscv-tests]` family ctest. Current stub state (FAIL with `category=feature_stub`) SHALL transition to PASS through the FSM implementation above.

#### Scenario: 8 rv32um tests PASS
- **WHEN** ctest `[riscv-tests]` invoked (filtered to rv32um-p-* subset, 8 cases)
- **THEN** all 8 cases SHALL PASS
- **AND** bus-cycles Payload Key SHALL reflect MUL=1c, DIV/DIVU/REM/REMU=33c per instruction

#### Scenario: Dhrystone baseline ≥1.4 DMIPS/MHz
- **WHEN** Dhrystone benchmark runs on 5-stage CH_MEM configuration (Verilator sim)
- **THEN** DMIPS/MHz SHALL be ≥1.4
- **AND** stall during DIV SHALL account for <5% of total cycles