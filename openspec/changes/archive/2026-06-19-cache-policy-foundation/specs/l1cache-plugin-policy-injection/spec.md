# l1cache-plugin-policy-injection

## Purpose

L1CachePlugin 接受 `ReplacementPolicy` 注入，保持 Phase 1.3 行为零变化的前提下提供可插拔替换策略。

## Requirements

### ADDED Requirements

#### REQ-LCPPI-001: 构造函数注入

`L1CachePlugin` SHALL 提供构造函数 `explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr)`，其中：
- `policy = nullptr` 是默认值，调用方零感知
- 当 `policy == nullptr` 时，构造函数 MUST 自动创建 `std::make_unique<NoReplacementPolicy>()` 并赋值给 `policy_` 成员

#### REQ-LCPPI-002: lookup 阶段调用 on_access

`L1CachePlugin::lookup()` 阶段（在 `pb.at_stage("lookup", ...)` 回调内）MUST 调用 `policy_->on_access(set, way)`。该调用 MUST 在 D4 合规的 `at_stage` 回调内执行，不引入新的状态机或 `tick()`。

#### REQ-LCPPI-003: 向后兼容

现有 L1CachePlugin 4 个测试（`tests/cache/test_l1_cache_*.cpp`）MUST 100% PASS，行为与 Phase 1.3 完全一致。默认 `nullptr` policy 等价于 hard-coded 行为。

#### REQ-LCPPI-004: 头文件依赖

`ip/cache/tlm/L1CachePlugin.h` MUST 包含 `#include "ip/cache/policies/replacement_policy.h"`，并声明私有成员 `std::unique_ptr<ReplacementPolicy> policy_`。