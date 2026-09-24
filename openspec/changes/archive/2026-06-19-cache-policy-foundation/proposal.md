## Why

`docs/architecture/overview.md §"可插拔策略模式"` 表格明确列出 L1/L2 Cache 替换策略（LRU/PLRU/Random/FIFO/RRIP），且 `ip/cache/README.md §4` 表格列出 5 种替换策略。但实际 `L1CachePlugin` 是 hard-coded 替换策略（256 sets × 1 way direct-mapped，无替换逻辑）；`ip/cache/policies/` 子目录不存在。这是 A7 架构债务——文档承诺"可插拔策略"但代码不支持。

本 change 目标是：实现最小 `ReplacementPolicy` 抽象接口（LRU）+ L1CachePlugin 集成注入点，消除架构债务；为 Phase 1.5 L2CachePlugin 铺路。

## What Changes

- **新增** `ip/cache/policies/replacement_policy.h`（最小抽象接口：4 虚方法 + 1 工厂方法 + Doxygen）
- **新增** `ip/cache/policies/no_replacement_policy.h`（直接映射 no-op，实现向后兼容的默认行为）
- **新增** `ip/cache/policies/lru_policy.h`（LRU 实现，**仅作为接口契约的 reference implementation**；1-way 简化，未来 L2 实施时整体替换）
- **重构** `ip/cache/tlm/L1CachePlugin.h/.cpp` 集成 ReplacementPolicy 注入（默认 `NoReplacementPolicy`，可选 LRU）
- **新增** `tests/cache/test_replacement_policy.cpp`（5 单元测试：工厂/LRU-create/none-create/unknown-name-throws/LRU-basic）
- **修正** `ip/cache/tlm/L1CachePlugin.h` L18 注释：原 `256 sets × 64-byte = 32KB` 是注释错误（**实际是 256 × 1 way × 64B = 16KB**，8-way 推迟到 Phase 1.5），已在 L1CachePlugin.h 头注释修正
- **同步修正** `ip/cache/README.md §4` 表格中"`256 sets × 64-byte = 32KB L1`"描述 → "`256 × 1 way × 64B = 16KB L1`"
- **更新** `ip/cache/README.md §4` 标注"✅ Phase 1.4 落地：NoReplacementPolicy + LRUPolicy 接口契约"
- **更新** `docs/architecture/overview.md §"可插拔策略模式"` L1/L2 Cache 行加 "✅ Phase 1.4 落地 (NoReplacementPolicy + LRUPolicy)"
- **更新** `CHANGELOG.md` 记录 v0.0.4（"L1CachePlugin 容量注释修正 + ReplacementPolicy 接口落地"）

## Capabilities

### New Capabilities

- `cache-replacement-policy-abstraction`: 提供 L1/L2 Cache 替换策略的抽象接口（`ReplacementPolicy` 抽象基类 + 工厂方法），允许通过 JSON 配置或构造参数切换实现。**接口稳定性优先于策略完备性**：本 change 只落地 2 个策略（NoReplacement + LRU reference），其他策略（PLRU/Random/FIFO/RRIP）推迟到 Phase 1.5 L2CachePlugin 实施时按需扩展。
- `l1cache-plugin-policy-injection`: L1CachePlugin 接受 `ReplacementPolicy` 注入，默认 `NoReplacementPolicy`（保持 Phase 1.3 行为零变化），可选 LRU。注入方式为构造函数参数 + 默认 nullptr，调用方零感知。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 新增: `ip/cache/policies/{replacement_policy,no_replacement_policy,lru_policy}.h` (~150 LOC)
  - 修改: `ip/cache/tlm/L1CachePlugin.{h,cpp}` (集成注入点 + 修正 32KB→16KB 注释)
  - 修改: `ip/cache/README.md §4` (容量描述修正 + 落地状态标注)
  - 新增: `tests/cache/test_replacement_policy.cpp` (~80 LOC)
  - 修改: `docs/architecture/overview.md §"可插拔策略模式"` (~5 行)
  - 修改: `CHANGELOG.md`
- **依赖与时序**:
  - 本 change 依赖 CHANGE-002 (ip-catalog-status-correct) 完成后 cache 状态描述稳定（避免双向修改 ip-cache README）
  - 本 change 在 v0.0.5 (empty-directory-cleanup) **之后**实施——v0.0.5 已删除 `ip/cache/test/` 等空目录，本 change 测试放 `tests/cache/`（沿用 v0.0.5 约定的"测试在 `tests/<ip>/`"），不与 `ip/cache/` 目录冲突
  - 本 change **不**触及 `ip/README.md` 的全局目录约定——避免与 v0.0.5 STATUS 段重复声明
- **基线影响**:
  - 与 L1CachePlugin 现有 4 个测试 (`tests/cache/test_l1_cache_*.cpp`) 必须 100% PASS（不破坏 Phase 1.3 行为）
  - 当前 ctest 基线: 16/16 PASS（CHANGELOG v0.0.5 + Phase 1.3d-extras 后）+ 5 新增 = 21/21 PASS
- **runtime 影响**: 默认行为零变化（注入 `NoReplacementPolicy` 等价于 hard-coded 行为）
- **breaking 变更**: 无（仅扩展，不改现有接口）
- **与 archive/2026-06-18-cache-policy-foundation-v1-original 关系**: v1-original 9 项问题已在本版本修复（详见 design §"v1 已知问题修复"）

## Alternatives Considered

### Alternative A: 删除 `overview.md §"可插拔策略模式"` 整个 Cache 行

**放弃理由**: L2CachePlugin 必然需要替换策略（8-way L2 不能 no-op），现在不实现等于把债务推到 L2Cache 实施时。**不放弃**：实现最小 LRU reference + 接口契约。

### Alternative B: 直接在 L1CachePlugin 内置 LRU，不抽接口

**放弃理由**: 与 L2CachePlugin 复用冲突，且违反 D4 Plugin-style（替换策略本质是可替换组件）。**不放弃**：抽象接口 + LRU reference 实现。

### Alternative C: 等 Phase 1.5 L2CachePlugin 时一起实施

**放弃理由**: 推迟债务 = 推迟问题。架构师审计 (A7) 已明确指出"文档说一套代码做一套"印象。**不放弃**：本 change 落地最小接口 + reference 实现，L2 实施时直接扩展。