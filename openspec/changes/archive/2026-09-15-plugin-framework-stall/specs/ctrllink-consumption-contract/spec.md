# Spec: `ctrllink-consumption-contract` — CtrlLink 与 PipeBuilder 绑定契约

> **Capability**: 框架层（cf::plugin）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

定义 `PipeBuilder::register_ctrl_link(stage_name, ctrl)` 的语义、生命周期、查询接口。Plugin 在 `build()` 内注册 CtrlLink，框架在 `run()` 内消费。

## Requirements

### REQ-1: 注册时机

**The system shall** 允许 `register_ctrl_link` 在 `pb.build()` 之前调用。典型调用时机是 `Plugin::build()` 闭包内（plugin 已注册到 pb）。

**Where**: `include/cf/plugin/pipe_builder.h` 新 API。

### REQ-2: 生命周期管理（shared_ptr）

**The system shall** 接受 `std::shared_ptr<CtrlLink>` 作为入参，避免 CtrlLink 拷贝约束的放宽。

**API 签名**:
```cpp
void register_ctrl_link(const std::string& stage_name,
                       std::shared_ptr<CtrlLink> ctrl);
```

**Where**: `pipe_builder.h` 新增。

**Rationale**: CtrlLink 当前 `= delete` 拷贝。shared_ptr 方案不要求放宽拷贝约束，保持原 API 严格性。Plugin 在 build() 内 `auto ctrl = std::make_shared<CtrlLink>(); ctrl->halt_when(...); pb.register_ctrl_link("fetch", ctrl);`。

### REQ-3: 同名 stage 多 CtrlLink 累加

**When** 对同一 stage_name 多次调用 `register_ctrl_link`，**the system shall** 累加到内部 `vector<shared_ptr<CtrlLink>>`，不覆盖。

**Where**: `stage_ctrl_links_[stage_name].push_back(ctrl)`。

### REQ-4: 查询 API

**The system shall** 暴露 `bool should_stall_stage(const std::string& stage_name) const` 用于 `pb.run()` 内查询。

**Implementation**:
```cpp
bool should_stall_stage(const std::string& name) const {
  auto it = stage_ctrl_links_.find(name);
  if (it == stage_ctrl_links_.end()) return false;
  for (const auto& c : it->second) {
    if (c && c->should_halt()) return true;
  }
  return false;
}
```

### REQ-5: 调试访问器

**The system shall** 暴露 `std::size_t ctrl_link_count(const std::string& name) const noexcept` 用于测试断言。

### REQ-6: 不修改 CtrlLink 原 API

**The system shall NOT** 修改 `CtrlLink::halt_when/throw_when/flush_when/bypass` 签名或行为。

**Rationale**: 不破坏现有 11 个 `test_ctrl_link.cpp` case。

## Acceptance Scenarios

### Scenario 1: 注册 + 查询

**Given** `auto ctrl = std::make_shared<CtrlLink>(); ctrl->halt_when([]{return true;});`
**When** `pb.register_ctrl_link("fetch", ctrl);`
**Then** `pb.should_stall_stage("fetch") == true`
**And** `pb.ctrl_link_count("fetch") == 1`

### Scenario 2: 未注册 stage

**Given** 未对 "memory" 注册 CtrlLink
**When** `pb.should_stall_stage("memory")`
**Then** 返回 false（no-op）

### Scenario 3: 同 stage 累加

**Given** 对 "fetch" 注册 2 个 CtrlLink
**When** `pb.ctrl_link_count("fetch")`
**Then** 返回 2

### Scenario 4: OR 合并查询

**Given** CtrlLink C1 `halt_when([&a]{return a;})`，C2 `halt_when([&b]{return b;})`
**When** `a=false, b=true`
**Then** `pb.should_stall_stage("fetch") == true`（任一为真即 stall）

## Out of Scope

- ❌ **不修改 CtrlLink 拷贝约束**（保持 `= delete`）
- ❌ **不增加 thread-safe 同步**（CtrlLink 是 build() 注册，run() 单线程消费）
- ❌ **不增加 CtrlLink 序列化**（调试场景）
