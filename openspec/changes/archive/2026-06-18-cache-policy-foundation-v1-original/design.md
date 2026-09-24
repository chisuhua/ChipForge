## Context

本 change 落地 A7 架构债务——`ip/cache/policies/` 子目录缺失 + `L1CachePlugin` 不可插拔替换策略。

**当前状态**:
- `ip/cache/tlm/L1CachePlugin.cpp` (254 LOC) 是 hard-coded 实现：256 sets × 1 way (直接映射)，无替换逻辑
- `docs/architecture/overview.md` 表格列 LRU/PLRU/Random/FIFO/RRIP 5 种策略 + 3 种预取 + 3 种写策略
- `ip/README.md` 标准约定含 `policies/` 子目录
- 0 个 `policies/` 实现

**约束**:
- L1CachePlugin 现有 4 个测试 (`tests/cache/test_l1_cache_plugin_unit.cpp` 等) 必须 100% PASS（不破坏 Phase 1.3 行为）
- D4 Plugin-style 强制：业务代码无 `tick()`、无状态机
- `cf::plugin::uint_t<N>` 是字段类型基线（D4 合规）
- Bridge 层不直接调用 ch_mem/ch_reg/ch_uint（ADR-040 Tier-1）

**利益相关者**:
- L2CachePlugin 实施者：直接复用此接口
- 配置驱动测试者：通过 JSON params 选择 LRU
- 架构师：消除"文档说一套代码做一套"印象

## Goals / Non-Goals

**Goals:**
- 实现最小 `ReplacementPolicy` 抽象接口（4 虚方法 + 1 工厂方法）
- 实现 `NoReplacementPolicy` (直接映射 no-op，保持向后兼容) + `LRUPolicy` (1-way LRU = LRU 简化)
- L1CachePlugin 接受 `ReplacementPolicy` 注入，默认 `NoReplacementPolicy`
- 5 个单元测试（工厂 / 默认 / no-op / LRU 基本 / LRU 边界条件）
- 文档同步（`overview.md` 标注"✅ Phase 1.4 落地"，`bundles/README.md` 关联）

**Non-Goals:**
- **不**实现 PLRU/Random/FIFO/RRIP（推迟到 Phase 1.5 L2CachePlugin）
- **不**实现预取策略（推迟）
- **不**实现写策略切换（推迟）
- **不**实现 JSON 配置驱动策略选择（推迟到 Phase 1.5 完整配置层）
- **不**改 L1CachePlugin 的核心行为（lookup + refill 两阶段算法）
- **不**实现跨测试或性能 benchmark

## Decisions

### Decision 1: `ReplacementPolicy` 接口最小集

**选择**: **4 虚方法 + 1 工厂方法**

```cpp
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual void on_access(uint32_t set, uint32_t way) = 0;
    virtual uint32_t select_victim(uint32_t set) = 0;
    virtual void on_insert(uint32_t set, uint32_t way) = 0;
    virtual std::string name() const = 0;
    static std::unique_ptr<ReplacementPolicy> create(const std::string& name);
};
```

**理由**:
- 与 `overview.md` 表格描述完全一致
- 4 方法覆盖 `access/insert/victim/identify` 4 个生命周期
- 工厂方法为未来 JSON 驱动铺路

**替代方案**:
- ❌ 2 方法 (only victim + identify): 丢失 on_access 时机点（影响 LRU 准确性）
- ❌ 6 方法 (增加 reset + on_evict): 过度设计

### Decision 2: L1CachePlugin 注入方式

**选择**: **构造参数 + 默认值**

```cpp
class L1CachePlugin : public cf::plugin::PluginBase {
public:
    explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr);
    // ...
private:
    std::unique_ptr<ReplacementPolicy> policy_;  // 默认 nullptr → NoReplacementPolicy
};
```

**理由**:
- 简单，调用方零感知（默认 nullptr 行为不变）
- 与 D4 Plugin-style 一致（无 tick）
- 未来可扩展 JSON 配置层

**替代方案**:
- ❌ 全局静态 `set_policy()`: 多实例冲突
- ❌ 通过 `PipeBuilder` 注入: 增加复杂度，本阶段不需要

### Decision 3: LRU 实现简化

**选择**: **1-way LRU = access counter 单调递增**

对于 L1CachePlugin 当前 1-way (直接映射)，LRU 简化为：
- `select_victim(set)` 返回唯一 way (0)
- `on_access(set, way)` 增加全局 access counter（未来 8-way 时可改为 per-set counter）
- `on_insert(set, way)` 记录 way 的 timestamp

**理由**:
- L1 当前是 1-way，没有"选择受害者"语义
- LRU 简化实现足以验证接口和注入点
- 8-way L2 LRU 推迟到 L2CachePlugin

**替代方案**:
- ❌ 完整 8-way LRU 栈实现: 过度工程
- ❌ 随机策略: 测试覆盖率低

## Risks / Trade-offs

**[Risk 1]** L1CachePlugin 重构破坏 Phase 1.3 行为 → **Mitigation**: 默认 `nullptr` policy → `NoReplacementPolicy` 行为等价；所有 4 个现有测试必须 100% PASS
**[Risk 2]** LRU 1-way 简化对 L2 无参考价值 → **Mitigation**: 在代码注释显式说明"L1 1-way 简化版，L2 实施时升级为完整 LRU"
**[Risk 3]** ReplacementPolicy 接口可能与未来 CppTLM Policy 不兼容 → **Mitigation**: 命名空间 `cf::ip::cache::policies` 与 cpptlm 隔离
**[Risk 4]** 文档同步可能遗漏 → **Mitigation**: 在 `bundles/README.md` 加交叉引用，PR 评审检查 7 文档 grep

## Migration Plan

无运行时迁移（仅扩展）。**步骤**:
1. 创建 `ip/cache/policies/replacement_policy.h`（commit 1）
2. 创建 `no_replacement_policy.h` + `lru_policy.h`（commit 2）
3. 重构 `L1CachePlugin.h/cpp` 集成注入（commit 3）
4. 5 个单元测试（commit 4）
5. 文档同步（commit 5）
6. PR + 验证 ctest

**回滚策略**: 5 个 commit 可逐个 revert；接口变更前向兼容（默认 nullptr）

## Open Questions

1. LRU 实现是否使用 `<set>` / `<unordered_set>` 外部容器？ → **本 change 决定**: **不**（保持零外部依赖，仅用 vector + counter）
2. 是否在 L1CachePlugin 的 `at_stage` 回调中触发 `on_access`？ → **本 change 决定**: **是**（在 `lookup` 阶段触发，与 D4 一致）
3. `ReplacementPolicy::create()` 工厂支持哪些策略名字？ → **本 change 决定**: 仅 `"LRU"` 和 `"None"` (default)；其他返回 `nullptr` + 抛 `runtime_error`
