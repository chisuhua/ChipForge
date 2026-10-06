# Spec: `multi-cycle-fsm` — 框架级多周期 FSM 通用模板（ADR-046 v2.0 §2.1.1）

> **Capability**: 框架层（cf::plugin multi_cycle_fsm namespace）
> **Status**: PLACEHOLDER — normative spec 由 `mfc-cpu-pipeline-multi-cycle-fsm` archive 时落地；本文件仅声明结构性存在 + 防止 validation 失败。
> **Change**: `mfc-extract-fsm-h`
> **Depends on**: `mfc-cpu-pipeline-multi-cycle-fsm` archive（mul_div_fsm.h 内联 CH_MEM DSL 已落地，本 change 提取到 framework-level header）
> **Created**: 2026-10-06
> **Authors**: ChipForge Plugin Team

## Purpose

`mfc-cpu-pipeline-multi-cycle-fsm` Phase D.1 在 `ip/cpu/arch/riscv/mul_div_fsm.h` 的 `#ifdef CF_PLUGIN_USE_CH_MEM` 块（~180 LOC）落地了 `ch_state_machine` DSL 的内联实现。`mfc-extract-fsm-h` 提取该 DSL 到 framework-level header `include/cf/plugin/multi_cycle_fsm.h`，作为业务 Plugin 可复用的多周期 FSM 模板。

本 spec 仅声明该结构性存在（避免 `mfc-extract-fsm-h` validation 失败），normative 规范由 `mfc-cpu-pipeline-multi-cycle-fsm` archive 时落地。

## ADDED Requirements

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

## Out of Scope

- ❌ **不定义 `ch_state_machine` DSL 内部语义**（由 `mfc-cpu-pipeline-multi-cycle-fsm` spec 接管）
- ❌ **不规定具体 FSM 状态枚举**（由业务 Plugin 自定义，模板仅提供 `enum class` 约束）
- ❌ **不实装 cycle precision 测量**（推迟到 `plugin-framework-cycle-precision`）

## Related

- **ADR-040 v2.0**: TLM→HDL 传输性约束（双文件分离）
- **ADR-046 v2.0**: 多周期 FSM 豁免 + `ch_state_machine` DSL
- **ADR-082**: Plugin::negotiate() capability 协商
- **mfc-cpu-pipeline-multi-cycle-fsm**: parent change（Phase D.1 内联实现，Phase H archive 后由本 change 提取）
- **mfc-extract-fsm-h**: parent change（本 spec 由其创建）