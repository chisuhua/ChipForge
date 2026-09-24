## Context

本 change 落地 A7 架构债务 + 修正 L1Cache 容量注释错误。

**当前状态 (2026-06-18 审计)**:
- `ip/cache/tlm/L1CachePlugin.h` (199 LOC) + `.cpp` (254 LOC): 256 sets × 1 way × 64B = **16KB** L1 (direct-mapped, 无替换逻辑)
- **重要修正**: 原 L1CachePlugin.h L18 注释写"256 sets × 64-byte = 32KB L1"——这是**注释错误**（实际 RAM = 256×1×64 = 16384 字节 = 16KB；32KB 是按 8-way 误算）。本 change 在 header 注释与 `ip/cache/README.md §4` 同步修正
- `docs/architecture/overview.md §"可插拔策略模式"` 表格列 LRU/PLRU/Random/FIFO/RRIP 5 种策略 + 3 种预取 + 3 种写策略
- `ip/cache/README.md §4` "可插拔策略"表格列 5 种替换策略（但实现为空）
- `ip/cache/policies/` 子目录**不存在**（v0.0.5 之前曾存在空目录，v0.0.5 已删除）
- 0 个 `policies/` 实现

**约束**:
- L1CachePlugin 现有 4 个测试 (`tests/cache/test_l1_cache_*.cpp`) 必须 100% PASS（不破坏 Phase 1.3 行为）
- D4 Plugin-style 强制：业务代码无 `tick()`、无状态机
- `cf::plugin::uint_t<N>` 是字段类型基线（D4 合规）
- Bridge 层不直接调用 ch_mem/ch_reg/ch_uint（ADR-040 Tier-1）
- **本 change 不修改** `ip/README.md`（v0.0.5 已加 STATUS 约定段，与本 change 职责不同）
- **本 change 不创建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 已创建 `IP_STATUS_TEMPLATE.md` 覆盖零代码 IP，本 change 仅更新现有 `ip/cache/README.md`）

**利益相关者**:
- L2CachePlugin 实施者：直接复用此接口（8-way 时升级为完整 LRU，本 change 的 1-way LRU 会被替换）
- 配置驱动测试者：通过 JSON params 选择 LRU
- 架构师：消除"文档说一套代码做一套"印象

## v1 已知问题修复 (来自 archive/2026-06-18-cache-policy-foundation-v1-original)

| v1 Issue | 本 v2 修复 |
|---|---|
| Issue 1: proposal 引用 `ip/tilecore/policies/` 存在但空 | 删除该对比句；v0.0.5 已删除所有空目录 |
| Issue 2: 32KB 容量错误（应为 16KB） | L1CachePlugin.h L18 注释已修正；ip-cache README §4 同步修正 |
| Issue 4: CHANGE-XXX 编号引用混乱 | 本 change 明确引用 CHANGE-002 + v0.0.5 (empty-directory-cleanup) |
| Issue 5: tasks §5.3 改 `ip/README.md` 加 `policies/` 契约 | 删除该 task；改为更新 `ip/cache/README.md §4`（限定范围，不污染全局） |
| Issue 6: tasks §6.4 测试基线"51/60"无来源 | 改为引用 CHANGELOG 实际基线 16/16 + 增量 5 = 21/21 |
| Issue 7: LRU 1-way 简化对 L2 无参考价值 | design + code 注释显式说明"reference implementation，Phase 1.5 整体替换" |

## Goals / Non-Goals

**Goals:**
- 实现最小 `ReplacementPolicy` 抽象接口（4 虚方法 + 1 工厂方法）
- 实现 `NoReplacementPolicy` (直接映射 no-op，保持向后兼容) + `LRUPolicy` (1-way reference 实现)
- L1CachePlugin 接受 `ReplacementPolicy` 注入，默认 `NoReplacementPolicy`
- 修正 L1CachePlugin.h L18 注释错误（32KB → 16KB），同步修正 `ip/cache/README.md §4`
- 5 个单元测试（工厂 / LRU-create / none-create / unknown-name-throws / LRU-basic）
- 文档同步（`overview.md` + `ip/cache/README.md §4` 标注"✅ Phase 1.4 落地"）

**Non-Goals:**
- **不**实现 PLRU/Random/FIFO/RRIP（推迟到 Phase 1.5 L2CachePlugin）
- **不**实现预取策略（推迟）
- **不**实现写策略切换（推迟）
- **不**实现 JSON 配置驱动策略选择（推迟到 Phase 1.5 完整配置层）
- **不**改 L1CachePlugin 的核心行为（lookup + refill 两阶段算法）
- **不**实现跨测试或性能 benchmark
- **不**修改 `ip/README.md`（v0.0.5 已处理 STATUS 约定）
- **不**新建 `IP_README_TEMPLATE.md`（v0.0.5 已建 `IP_STATUS_TEMPLATE.md` 覆盖零代码 IP 场景；本 change 仅更新现有 `ip/cache/README.md`）

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
- 与 `overview.md §"可插拔策略模式"` 表格描述完全一致
- 4 方法覆盖 `access/insert/victim/identify` 4 个生命周期
- 工厂方法为未来 JSON 驱动铺路（`create("LRU")` / `create("None")` / `create("Unknown")` → `runtime_error`）

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

### Decision 3: LRU 实现作为 reference（非生产就绪）

**选择**: **1-way LRU = access counter 单调递增**，显式标注为 "reference implementation"

```cpp
// 注释必须包含:
// "REFERENCE IMPLEMENTATION ONLY — 适用于 L1CachePlugin 1-way 直接映射。
//  Phase 1.5 L2CachePlugin 实施时此文件将被 8-way 完整 LRU 整体替换，
//  不做向前兼容迁移。如需 L2 LRU 请等待 Phase 1.5。"
```

对于 L1CachePlugin 当前 1-way (直接映射)，LRU 简化为：
- `select_victim(set)` 返回唯一 way (0)
- `on_access(set, way)` 增加全局 access counter（未来 8-way 时可改为 per-set counter）
- `on_insert(set, way)` 记录 way 的 timestamp

**理由**:
- L1 当前是 1-way，没有"选择受害者"语义
- LRU 简化实现足以验证接口和注入点
- 8-way L2 LRU 推迟到 L2CachePlugin
- **明确标注 reference 性质**，避免后人误用（v1 Issue 7）

**替代方案**:
- ❌ 完整 8-way LRU 栈实现: 过度工程（且当前 L1 用不上）
- ❌ 随机策略: 测试覆盖率低

### Decision 4: 不修改 `ip/README.md`

**选择**: **本 change 仅更新 `ip/cache/README.md`**

**理由**:
- v0.0.5 (empty-directory-cleanup) 已在 `ip/README.md` 顶部加 STATUS 约定段（含 7 IP 状态表）
- v0.0.5 已创建 `docs/templates/IP_STATUS_TEMPLATE.md` 覆盖零代码 IP
- 本 change 职责是 cache IP 内部策略接口，**不应污染全局 IP 库约定**
- 如果未来需要 `IP_README_TEMPLATE.md`（给所有 IP README 统一结构），应作为独立 change 提案

**替代方案**:
- ❌ 在 `ip/README.md` 加 `policies/` 子目录契约：违反 v0.0.5 "避免空目录"原则；当前零 IP 有 `policies/`，列出来反而误导

## Risks / Trade-offs

**[Risk 1]** L1CachePlugin 重构破坏 Phase 1.3 行为 → **Mitigation**: 默认 `nullptr` policy → `NoReplacementPolicy` 行为等价；所有 4 个现有测试必须 100% PASS
**[Risk 2]** LRU 1-way 简化对 L2 无参考价值 → **Mitigation**: 在代码注释显式说明"reference implementation，Phase 1.5 整体替换"，并在 `ip/cache/README.md §4` 加相同说明
**[Risk 3]** ReplacementPolicy 接口可能与未来 CppTLM Policy 不兼容 → **Mitigation**: 命名空间 `cf::ip::cache::policies` 与 cpptlm 隔离
**[Risk 4]** 文档同步可能遗漏 → **Mitigation**: tasks 显式列 4 个文档修改点（overview.md + ip-cache README §4 + L1CachePlugin.h 注释 + CHANGELOG.md），PR 评审用 `grep` 验证

## Migration Plan

无运行时迁移（仅扩展）。**步骤**:
1. 修正 `ip/cache/tlm/L1CachePlugin.h` L18 注释（32KB → 16KB）（commit 1，本 change 之前已执行）
2. 创建 `ip/cache/policies/replacement_policy.h`（commit 2）
3. 创建 `no_replacement_policy.h` + `lru_policy.h`（commit 3）
4. 重构 `L1CachePlugin.h/cpp` 集成注入（commit 4）
5. 5 个单元测试（commit 5）
6. 文档同步：ip-cache README §4 + overview.md + CHANGELOG.md（commit 6）
7. PR + 验证 ctest

**回滚策略**: 6 个 commit 可逐个 revert；接口变更前向兼容（默认 nullptr）

## Open Questions

1. LRU 实现是否使用 `<set>` / `<unordered_set>` 外部容器？ → **本 change 决定**: **不**（保持零外部依赖，仅用 vector + counter）
2. 是否在 L1CachePlugin 的 `at_stage` 回调中触发 `on_access`？ → **本 change 决定**: **是**（在 `lookup` 阶段触发，与 D4 一致）
3. `ReplacementPolicy::create()` 工厂支持哪些策略名字？ → **本 change 决定**: 仅 `"LRU"` 和 `"None"` (default)；其他返回 `nullptr` + 抛 `runtime_error`
4. LRU 1-way 是否在 select_victim 返回 0 时打日志提示"reference implementation"？ → **本 change 决定**: **否**（避免日志噪音；reference 性质通过注释传达）