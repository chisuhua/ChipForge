# Spec: `ctrllink-consumption-contract` — CtrlLink 与 PipeBuilder 绑定契约

> **Capability**: 框架层（cf::plugin）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

定义 `PipeBuilder::register_ctrl_link(stage_name, ctrl)` 的语义、生命周期、查询接口。Plugin 在 `build()` 内注册 CtrlLink，框架在 `run()` 内消费。

## Requirements

### Requirement: 注册时机

The system SHALL 允许 `register_ctrl_link` 在 `pb.build()` 之前调用。典型调用时机是 `Plugin::build()` 闭包内（plugin 已注册到 pb）。

**Where**: `include/cf/plugin/pipe_builder.h` 新 API。

#### Scenario: 注册 + 查询

- **WHEN** `auto ctrl = std::make_shared<CtrlLink>(); ctrl->halt_when([]{return true;}); pb.register_ctrl_link("fetch", ctrl);`
- **THEN** `pb.should_stall_stage("fetch") == true`
- **AND** `pb.ctrl_link_count("fetch") == 1`

### Requirement: 生命周期管理（shared_ptr）

The system SHALL 接受 `std::shared_ptr<CtrlLink>` 作为入参，避免 CtrlLink 拷贝约束的放宽。

**API 签名**:
```cpp
void register_ctrl_link(const std::string& stage_name,
                       std::shared_ptr<CtrlLink> ctrl);
```

**Where**: `pipe_builder.h` 新增。

**Rationale**: CtrlLink 当前 `= delete` 拷贝。shared_ptr 方案不要求放宽拷贝约束，保持原 API 严格性。Plugin 在 build() 内 `auto ctrl = std::make_shared<CtrlLink>(); ctrl->halt_when(...); pb.register_ctrl_link("fetch", ctrl);`。

#### Scenario: shared_ptr 入参保留生命周期

- **WHEN** `auto ctrl = std::make_shared<CtrlLink>(); ctrl->halt_when([]{return true;}); pb.register_ctrl_link("fetch", ctrl);`
- **AND** Plugin::build() 闭包结束（ctrl 局部 shared_ptr 离开作用域）
- **THEN** PipeBuilder 仍持有 ctrl 的所有权，调用 `pb.should_stall_stage("fetch")` 返回 true
- **AND** CtrlLink 不会被销毁（pb 内部 `vector<shared_ptr<CtrlLink>>` 延长生命周期）

### Requirement: 同名 stage 多 CtrlLink 累加

The system SHALL 对同一 stage_name 多次调用 `register_ctrl_link` 时累加到内部 `vector<shared_ptr<CtrlLink>>`，不覆盖。

**Where**: `stage_ctrl_links_[stage_name].push_back(ctrl)`。

#### Scenario: 同 stage 累加

- **WHEN** 对 "fetch" 注册 2 个 CtrlLink 且调用 `pb.ctrl_link_count("fetch")`
- **THEN** 返回 2

### Requirement: 查询 API

The system SHALL 暴露 `bool should_stall_stage(const std::string& stage_name) const` 用于 `pb.run()` 内查询。

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

#### Scenario: 未注册 stage

- **WHEN** 未对 "memory" 注册 CtrlLink 且调用 `pb.should_stall_stage("memory")`
- **THEN** 返回 false（no-op）

#### Scenario: OR 合并查询

- **WHEN** CtrlLink C1 `halt_when([&a]{return a;})`, C2 `halt_when([&b]{return b;})` 绑定到 "fetch"
- **AND** `a=false, b=true`
- **THEN** `pb.should_stall_stage("fetch") == true`（任一为真即 stall）

### Requirement: 调试访问器

The system SHALL 暴露 `std::size_t ctrl_link_count(const std::string& name) const noexcept` 用于测试断言。

#### Scenario: ctrl_link_count 反映注册数

- **WHEN** 对 "fetch" 注册 3 个 CtrlLink 且对 "decode" 注册 1 个 CtrlLink
- **AND** 调用 `pb.ctrl_link_count("fetch")` 和 `pb.ctrl_link_count("decode")`
- **THEN** 分别返回 3 和 1

### Requirement: 不修改 CtrlLink 原 API

The system SHALL NOT 修改 `CtrlLink::halt_when/throw_when/flush_when/bypass` 签名或行为。

**Rationale**: 不破坏现有 11 个 `test_ctrl_link.cpp` case。

#### Scenario: CtrlLink 原 API 签名不变

- **WHEN** 检查 `include/cf/plugin/ctrl_link.h` 中 `halt_when/throw_when/flush_when/bypass` 4 个方法的签名
- **THEN** 签名与 `plugin-framework-stall` archive 前一致（参数类型、返回类型、noexcept 标注均不变）
- **AND** 现有 `test_ctrl_link.cpp` 11 个测试 case 在 `pb.register_ctrl_link` 实装后仍全部 PASS（无行为回归）

## Out of Scope

- ❌ **不修改 CtrlLink 拷贝约束**（保持 `= delete`）
- ❌ **不增加 thread-safe 同步**（CtrlLink 是 build() 注册，run() 单线程消费）
- ❌ **不增加 CtrlLink 序列化**（调试场景）