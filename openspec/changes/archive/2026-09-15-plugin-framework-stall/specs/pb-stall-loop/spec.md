# Spec: `pb-stall-loop` — PipeBuilder::run() 声明式 stall 循环

> **Capability**: 框架层（cf::plugin）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

`PipeBuilder::run()` 当前是单 pass，对所有 stage × phase 闭包顺序调用，不消费 `CtrlLink`。本 spec 定义 `pb.run()` 的 stall loop 行为：当某 stage 注册的 CtrlLink 触发 halt 条件时，该 stage 的所有 phase 闭包本 cycle 不被调用。

## Requirements

### REQ-1: Stage-level stall 检查

**The system shall** 在 `pb.run()` 的 canonical stage 循环中，对每个 stage 调用 `should_stall_stage(stage_name)`。

**Where**: `include/cf/plugin/pipe_builder.h:100-122` 的 `run()` 方法。

**Rationale**: `plugin-style-design-methodology-v1.md:118-123` 承认 `pb.run()` 无 cycle 区分；DSE 文档 `dse_architecture_v2_design_research.md:219-225` 期望 `flush_when` / `commit_storages` 是 OoO 原语。本 spec 兑现 halt 这一个原语。

### REQ-2: OR 合并语义

**When** 多个 CtrlLink 绑定到同一 stage，**the system shall** 返回任一 CtrlLink 的 `should_halt()` OR 结果。

**Where**: `PipeBuilder::should_stall_stage(name)` 内部实现。

**Rationale**: `docs/architecture/plugin-framework.md:402-418` 明确定义 CtrlLink OR 合并语义。

### REQ-3: Skip all phases on stall

**When** `should_stall_stage(stage_name) == true`，**the system shall** 跳过该 stage 的所有 3 个 phase（EARLY/NORMAL/LATE）的闭包。

**Where**: `run()` 内 `for (int p_idx = 0; p_idx < 3; ++p_idx)` 循环外层包 stall 检查。

### REQ-4: 其他 stage 不受影响

**When** stage A stall，**the system shall** 继续执行后续 stage B/C/D 的所有 phase。

**Where**: stall 状态是 per-stage，不全局。

### REQ-5: 末位异常抛出

**After** 所有 stage 执行完毕，**the system shall** 检查所有 CtrlLink 的 `should_throw()`，若有任一为 true 则抛出 `PluginException`。

**Where**: `run()` 末尾 `commit_storages()` 之前。

**Rationale**: 与 halt 平行，让 throw 成为框架级原语而非 Plugin 主动消费。

### REQ-6: 空 CtrlLink no-op

**When** stage 未注册任何 CtrlLink，**the system shall** 行为完全等同于 `plugin-framework-stall` 之前。

**Where**: `should_stall_stage(name)` 返回 false 当 stage_ctrl_links_[name] 为空。

**Rationale**: 向后兼容，308→318 测试零退化。

### REQ-7: 框架不消费 flush_when / bypass

**The system shall NOT** 自动消费 `CtrlLink::flush_when` 或 `CtrlLink::bypass`。这两个原语由 Plugin 在 `at_stage` 闭包内显式消费。

**Where**: `run()` 内不调用 `c.should_flush()` 或 `c.bypass_active(key)`。

**Rationale**: flush 语义依赖具体 Plugin（"清空哪个 stage"），框架无法统一决定。推迟到 `cpu-pipeline-mispredict`。

## Acceptance Scenarios

### Scenario 1: 空 CtrlLink 无副作用

**Given** PipeBuilder 注册了 3 个 plugin，每个 plugin 1 个 at_stage 闭包
**And** 无 CtrlLink 注册
**When** `pb.run()`
**Then** 所有 3 个闭包按既有顺序执行（与 `cpu-pipeline-stubs-replace` commit A 后行为一致）

### Scenario 2: 单 CtrlLink halt 触发

**Given** PipeBuilder 注册 plugin P 在 stage "fetch" 的 NORMAL 闭包
**And** 注册 CtrlLink C 绑定到 "fetch"，`halt_when([&flag] { return flag; })`，初始 `flag=false`
**When** `pb.run()` 第 1 次，`flag=false`
**Then** P 的 fetch 闭包被执行

**When** `pb.run()` 第 2 次，`flag=true`
**Then** P 的 fetch 闭包**不被执行**

**When** `pb.run()` 第 3 次，`flag=false`
**Then** P 的 fetch 闭包被执行

### Scenario 3: OR 合并多 CtrlLink

**Given** 2 个 CtrlLink 绑定到 "fetch"：C1 `halt_when([&a] { return a; })`，C2 `halt_when([&b] { return b; })`
**When** `a=false, b=true`
**Then** should_stall_stage("fetch") == true

**When** `a=true, b=false`
**Then** should_stall_stage("fetch") == true

**When** `a=false, b=false`
**Then** should_stall_stage("fetch") == false

### Scenario 4: throw_when 抛出

**Given** 1 个 CtrlLink 绑定到 "fetch"，`throw_when([&f] { return f; })`，`f=true`
**When** `pb.run()`
**Then** 抛出 `PluginException`，所有 commit_storages 不执行

### Scenario 5: flush_when 不框架消费

**Given** 1 个 CtrlLink 绑定到 "fetch"，`flush_when([&f] { return f; })`，`f=true`
**When** `pb.run()`
**Then** fetch 闭包正常执行（flush 不影响）；Plugin 自己在闭包内可调 `ctrl.should_flush()` 决定清空 payload
