# Spec: `pb-stall-loop` — PipeBuilder::run() 声明式 stall 循环

> **Capability**: 框架层（cf::plugin）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

`PipeBuilder::run()` 当前是单 pass，对所有 stage × phase 闭包顺序调用，不消费 `CtrlLink`。本 spec 定义 `pb.run()` 的 stall loop 行为：当某 stage 注册的 CtrlLink 触发 halt 条件时，该 stage 的所有 phase 闭包本 cycle 不被调用。

## Requirements

### Requirement: Stage-level stall 检查

The system SHALL 在 `pb.run()` 的 canonical stage 循环中，对每个 stage 调用 `should_stall_stage(stage_name)`。

**Where**: `include/cf/plugin/pipe_builder.h:100-122` 的 `run()` 方法。

**Rationale**: `plugin-style-design-methodology-v1.md:118-123` 承认 `pb.run()` 无 cycle 区分；DSE 文档 `dse_architecture_v2_design_research.md:219-225` 期望 `flush_when` / `commit_storages` 是 OoO 原语。本 spec 兑现 halt 这一个原语。

#### Scenario: 空 CtrlLink 无副作用

- **WHEN** PipeBuilder 注册了 3 个 plugin，每个 plugin 1 个 at_stage 闭包
- **AND** 无 CtrlLink 注册
- **AND** 调用 `pb.run()`
- **THEN** 所有 3 个闭包按既有顺序执行（与 `cpu-pipeline-stubs-replace` commit A 后行为一致）

### Requirement: OR 合并语义

The system SHALL 返回任一 CtrlLink 的 `should_halt()` OR 结果（当多个 CtrlLink 绑定到同一 stage）。

**Where**: `PipeBuilder::should_stall_stage(name)` 内部实现。

**Rationale**: `docs/architecture/plugin-framework.md:402-418` 明确定义 CtrlLink OR 合并语义。

#### Scenario: OR 合并多 CtrlLink

- **WHEN** 2 个 CtrlLink 绑定到 "fetch"：C1 `halt_when([&a] { return a; })`, C2 `halt_when([&b] { return b; })`
- **AND** `a=false, b=true`
- **THEN** `should_stall_stage("fetch") == true`

- **WHEN** `a=true, b=false`
- **THEN** `should_stall_stage("fetch") == true`

- **WHEN** `a=false, b=false`
- **THEN** `should_stall_stage("fetch") == false`

### Requirement: Skip all phases on stall

The system SHALL 当 `should_stall_stage(stage_name) == true` 时跳过该 stage 的所有 3 个 phase（EARLY/NORMAL/LATE）的闭包。

**Where**: `run()` 内 `for (int p_idx = 0; p_idx < 3; ++p_idx)` 循环外层包 stall 检查。

#### Scenario: 单 CtrlLink halt 触发

- **WHEN** PipeBuilder 注册 plugin P 在 stage "fetch" 的 NORMAL 闭包
- **AND** 注册 CtrlLink C 绑定到 "fetch"，`halt_when([&flag] { return flag; })`，初始 `flag=false`
- **AND** `pb.run()` 第 1 次，`flag=false`
- **THEN** P 的 fetch 闭包被执行

- **WHEN** `pb.run()` 第 2 次，`flag=true`
- **THEN** P 的 fetch 闭包**不被执行**

- **WHEN** `pb.run()` 第 3 次，`flag=false`
- **THEN** P 的 fetch 闭包被执行

### Requirement: 其他 stage 不受影响

The system SHALL 当 stage A stall 时继续执行后续 stage B/C/D 的所有 phase（stall 状态是 per-stage，不全局）。

**Where**: `run()` 内 stall 检查仅跳过当前 stage 的 phase 闭包，不影响后续 stage。

#### Scenario: stall 不影响其他 stage

- **WHEN** PipeBuilder 注册 plugin P1 在 stage "fetch" 和 P2 在 stage "execute"
- **AND** 注册 CtrlLink 绑定到 "fetch"，halt_when 返回 true
- **AND** `pb.run()`
- **THEN** P1 的 fetch 闭包不执行
- **AND** P2 的 execute 闭包正常执行

### Requirement: 末位异常抛出

The system SHALL 在所有 stage 执行完毕后检查所有 CtrlLink 的 `should_throw()`，若有任一为 true 则抛出 `PluginException`。

**Where**: `run()` 末尾 `commit_storages()` 之前。

**Rationale**: 与 halt 平行，让 throw 成为框架级原语而非 Plugin 主动消费。

#### Scenario: throw_when 抛出

- **WHEN** 1 个 CtrlLink 绑定到 "fetch"，`throw_when([&f] { return f; })`, `f=true`
- **AND** `pb.run()`
- **THEN** 抛出 `PluginException`，所有 commit_storages 不执行

### Requirement: 空 CtrlLink no-op

The system SHALL 当 stage 未注册任何 CtrlLink 时行为完全等同于 `plugin-framework-stall` 之前。

**Where**: `should_stall_stage(name)` 返回 false 当 `stage_ctrl_links_[name]` 为空。

**Rationale**: 向后兼容，308→318 测试零退化。

#### Scenario: 未注册 stage no-op

- **WHEN** PipeBuilder 注册 plugin P 在 stage "execute"，未对 "fetch" 注册 CtrlLink
- **AND** 调用 `pb.should_stall_stage("fetch")`
- **THEN** 返回 false（无副作用，等同于 stall 框架实装前行为）

### Requirement: 框架不消费 flush_when / bypass

The system SHALL NOT 自动消费 `CtrlLink::flush_when` 或 `CtrlLink::bypass`。这两个原语由 Plugin 在 `at_stage` 闭包内显式消费。

**Where**: `run()` 内不调用 `c.should_flush()` 或 `c.bypass_active(key)`。

**Rationale**: flush 语义依赖具体 Plugin（"清空哪个 stage"），框架无法统一决定。推迟到 `cpu-pipeline-mispredict`。

#### Scenario: flush_when 不框架消费

- **WHEN** 1 个 CtrlLink 绑定到 "fetch", `flush_when([&f] { return f; })`, `f=true`
- **AND** `pb.run()`
- **THEN** fetch 闭包正常执行（flush 不影响）；Plugin 自己在闭包内可调 `ctrl.should_flush()` 决定清空 payload