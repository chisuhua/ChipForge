## Why

`docs/architecture/overview.md §"可插拔策略模式"` 表格明确列出 L1/L2 Cache 替换策略（LRU/PLRU/Random/FIFO/RRIP），且 `ip/README.md` 标准结构包含 `policies/` 子目录。但实际 `ip/cache/` **没有** `policies/` 目录（对比 `ip/tilecore/policies/` 存在但空），当前 `L1CachePlugin` 是 hard-coded 替换策略（直接映射无替换）。这是 A7 架构债务——文档承诺"可插拔策略"但代码不支持。

本 change 目标是：实现最小 ReplacementPolicy 抽象接口（LRU）+ L1CachePlugin 集成注入点，消除架构债务；为 Phase 1.5 L2CachePlugin 铺路。

## What Changes

- **新增** `ip/cache/policies/replacement_policy.h`（最小抽象接口：4 虚方法 + 1 工厂方法）
- **新增** `ip/cache/policies/lru_policy.h`（LRU 实现，针对 direct-mapped 的 1-way 简化）
- **新增** `ip/cache/policies/no_replacement_policy.h`（直接映射 no-op 实现，保持向后兼容）
- **重构** `ip/cache/tlm/L1CachePlugin.h/.cpp` 集成 ReplacementPolicy 注入（默认 no-op，可选 LRU）
- **新增** `tests/cache/test_replacement_policy.cpp`（4 单元测试：工厂/默认/no-op/LRU 基本操作）
- **更新** `docs/architecture/overview.md §"可插拔策略模式"` 标注"✅ Phase 1.4 落地，LRU + NoReplacement"
- **更新** `bundles/README.md` 关联说明
- **更新** `CHANGELOG.md` 记录 v0.0.4

## Capabilities

### New Capabilities

- `cache-replacement-policy-abstraction`: 提供 L1/L2 Cache 替换策略的抽象接口（`ReplacementPolicy` 抽象基类 + 工厂方法），允许通过 JSON 配置或构造参数切换实现。
- `l1cache-plugin-policy-injection`: L1CachePlugin 接受 `ReplacementPolicy` 注入，默认 `NoReplacementPolicy`（保持 Phase 1.3 行为），可选 LRU。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 新增: `ip/cache/policies/{replacement_policy,lru_policy,no_replacement_policy}.h` (~150 LOC)
  - 修改: `ip/cache/tlm/L1CachePlugin.{h,cpp}` (集成注入点)
  - 新增: `tests/cache/test_replacement_policy.cpp` (~80 LOC)
  - 修改: `docs/architecture/overview.md` (~10 行)
  - 修改: `bundles/README.md` (关联段)
  - 修改: `CHANGELOG.md`
- **依赖**:
  - 本 change 依赖 CHANGE-002 (ip-catalog-status) 完成后 cache 状态稳定
  - 与 L1CachePlugin 现有 4 个测试必须 100% PASS（不破坏 Phase 1.3 行为）
- **CI 影响**: ctest 列表增加 `test_replacement_policy` 5 个 PASS
- **runtime 影响**: 默认行为零变化（注入 `NoReplacementPolicy` 等价于 hard-coded 行为）
- **breaking 变更**: 无（仅扩展，不改现有接口）

## Alternatives Considered

### Alternative A: 删除 `overview.md §"可插拔策略模式"` 整个 Cache 行

**放弃理由**: L2CachePlugin 必然需要替换策略（8-way L2 不能 no-op），现在不实现等于把债务推到 L2Cache 实施时。**不放弃**：实现最小 LRU。
