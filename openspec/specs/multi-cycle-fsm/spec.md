# multi-cycle-fsm Specification

## Purpose
TBD - created by archiving change mfc-extract-fsm-h. Update Purpose after archive.
## Requirements
### Requirement: 框架级独立 header 存在

The system SHALL 提供独立 `include/cf/plugin/multi_cycle_fsm.h` 头文件，作为多周期 FSM 通用模板入口。该 header：

- 不包含任何 TLM 业务逻辑（仅类型、模板、DSL API）
- 在 `#ifdef CF_PLUGIN_USE_CH_MEM` 块下提供 CH_MEM DSL 入口（`create_chmem_fsm` / `state_out` 等）
- 在 TLM 模式下保留 `state()` / `tick()` ad-hoc enum API（向后兼容 ADR-048）
- 提供 `negotiate(CapabilityTable&)` capability 入口（ADR-082 集成，声明 "multi_cycle_fsm" capability）

**Where**: `include/cf/plugin/multi_cycle_fsm.h`（新建，约 100 LOC）

**Rationale**: ADR-040 v2.0 严格分离（TLM 文件 `mul_div_fsm.h` 不含 `ch_*` 字面）+ ADR-046 v2.0 算术多周期 FSM 豁免配套 + ADR-082 negotiate 复用入口。

#### Scenario: 业务 Plugin 复用入口

- **WHEN** 业务 Plugin `MulDivFsmPlugin` 已实现并继承 `cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4>`
- **AND** 检查 include 路径 `ip/cpu/plugins/mul_div_fsm.cpp` 不含 TLM 业务逻辑（`grep "ch_uint\|ch_reg\|ch_state_machine\|ch::core"` 命中 = 0）
- **AND** 检查 `include/cf/plugin/multi_cycle_fsm.h` 提供 `FsmBase<StateEnum, NStates>` 模板
- **THEN** ADR-040 v2.0 严格分离达成（TLM-only 文件 0 渗透）
- **AND** `MulDivFsmPlugin::negotiate` 通过 `FsmBase::negotiate` 提供 "multi_cycle_fsm" capability

