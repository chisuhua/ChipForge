# cache-replacement-policy-abstraction

## Purpose

为 L1/L2 Cache 替换策略提供抽象接口与工厂方法，允许通过字符串配置或构造参数切换实现。

## Requirements

### ADDED Requirements

#### REQ-CRPA-001: ReplacementPolicy 抽象基类

系统 SHALL 提供 `cf::ip::cache::policies::ReplacementPolicy` 抽象基类，包含 4 个虚方法与 1 个静态工厂方法：

- `virtual ~ReplacementPolicy() = default`
- `virtual void on_access(uint32_t set, uint32_t way) = 0`
- `virtual uint32_t select_victim(uint32_t set) = 0`
- `virtual void on_insert(uint32_t set, uint32_t way) = 0`
- `virtual std::string name() const = 0`
- `static std::unique_ptr<ReplacementPolicy> create(const std::string& name)`

命名空间 MUST 为 `cf::ip::cache::policies`，与 cpptlm Policy 隔离。

#### REQ-CRPA-002: 工厂方法支持的策略名

`ReplacementPolicy::create(name)` SHALL 支持以下输入：

- `"None"` → 返回 `std::make_unique<NoReplacementPolicy>()`
- `"LRU"` → 返回 `std::make_unique<LRUPolicy>()`
- 其他字符串 → 抛 `std::runtime_error`

#### REQ-CRPA-003: NoReplacementPolicy 默认行为

`NoReplacementPolicy` SHALL 实现 3 个 no-op 方法（`on_access` / `select_victim` 返回 0 / `on_insert` 跳过），`name()` 返回 `"None"`。该策略 MUST 是 L1CachePlugin 1-way direct-mapped 的默认行为，保持 Phase 1.3 零行为变化。

#### REQ-CRPA-004: LRUPolicy reference implementation

`LRUPolicy` SHALL 实现 1-way LRU 简化版本（`select_victim` 返回 0；`on_access` 增全局 counter；`on_insert` 记 timestamp；`name()` 返回 `"LRU"`）。文件顶部 MUST 包含 Doxygen 注释块，明确标注 `REFERENCE IMPLEMENTATION ONLY`，声明 Phase 1.5 L2CachePlugin 实施时此文件将被 8-way 完整 LRU 整体替换，不做向前兼容迁移。

#### REQ-CRPA-005: 头文件位置

3 个策略头文件 MUST 位于 `ip/cache/policies/`：
- `ip/cache/policies/replacement_policy.h`
- `ip/cache/policies/no_replacement_policy.h`
- `ip/cache/policies/lru_policy.h`