# ADR-040：TLM→HDL 移植性约束（三级约束模型 + array_store 抽象）

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 1 提案（`array_store` 已实现 + CI 检查脚本已就位，迁移手册待 Phase 5 启动时验证） |
| 来源 | 本次会话 Oracle 报告（L1CachePlugin TLM→HDL 前向兼容性分析，2026-06-10）|
| 决策 | 三层不匹配点的发现、3-tier 约束模型、`cf::plugin::storage::array_store` 抽象、`PipeBuilder::register_commit_hook/commit_storages` 钩子 |
| 关联 ADR | ADR-025（Plugin 基类无 tick）、ADR-037（Plugin 作为设计范式） |

---

## 1. 背景与动机

### 1.1 L1CachePlugin Phase 1.2 暴露的可移植性问题

Phase 1.2 在 `ip/cache/tlm/L1CachePlugin.{h,cpp}` 实现了第一个 Plugin-style IP（256 sets × 64B line，lookup + refill 两阶段，4/4 单元测试 PASS）。然而 Oracle 报告 §2-§5-§6 指出，若 L1CachePlugin 的业务代码**直接**沿用到 Phase 5/6 升级到 CppHDL `ch_mem` 后端，将出现以下三类不匹配点：

| 不匹配维度 | TLM 模式现状 | CppHDL 目标 | 风险 |
|------------|-------------|------------|------|
| **存储后端** | `std::array<T, N>` (`L1CachePlugin.h:155-157`) | `ch_mem<T, N>` (`CppHDL/include/core/mem.h`) | 接口签名全变（`operator[]` vs `sread/write`） |
| **读写语义** | 单缓冲：同 `pb.run()` 内写对下游立即可见（RAW） | 双缓冲：读返回**上一周期 commit 提交值** | 行为不一致，需补 barrier/commit |
| **Bundle 类型** | `cf::plugin::uint_t<N>` (POD) | `ch_uint<N>` (硬件类) | 字段类型需替换 |
| **控制流** | `if (hit) return;` 早返（`L1CachePlugin.cpp:135-136`） | RTL 中无"早返"概念，需用 `when` 条件驱动 | 条件/分支需重构 |
| **位提取** | `(addr >> kIdxShift) & kIdxMask` 模板（`L1CachePlugin.cpp:108-113`） | RTL 中需 `addr[kIdxBits+kOffsetBits-1:kOffsetBits]` 位选 | 表达式需改写为位选 |
| **阶段调度** | 顺序执行（`pb.run()` 一次性） | 多周期流水 + 握手协议（valid/ready/cancel） | 调度语义需重写 |

### 1.2 三层不匹配点

| 层 | 不匹配 | 严重度 | 修复成本 |
|----|--------|--------|----------|
| **A 类：API 形态** | `std::array::operator[]` ↔ `ch_mem::sread/write` | 编译期可静态检查 | 低（adapter 包装） |
| **B 类：时序语义** | 单缓冲 RAW ↔ 双缓冲 commit | 需 commit 边界 | 中（PipeBuilder commit 钩子） |
| **C 类：抽象层级** | 控制流/位操作需从软件翻译到硬件 | 需 `when` 模板 + 位选 helper | 高（业务代码重构） |

---

## 2. 决策内容

### 2.1 三级约束模型

为平衡"工程实用性"与"可移植性承诺"，本 ADR 引入三级约束模型（Tier-1/2/3）：

#### Tier-1：CI 强制（FAIL → 阻塞合并）

| # | 约束 | 理由 | 检查工具 |
|---|------|------|----------|
| 1 | 无 `void tick()` 业务重写 | 调度由框架决定，Plugin 不持有时序 | `tools/verify_plugin_decision.sh` Check 1 |
| 2 | 无状态机（`enum class State` + `switch state_`）| 控制流必须通过 `at_stage` 表达 | `tools/verify_plugin_decision.sh` Check 2 |
| 3 | Bundle 字段用 `cf::plugin::uint_t<N>` | 为 `ch_uint<N>` 升级保留类型别名空间 | `tools/verify_plugin_decision.sh` Check 3 |
| 4 | **`at_stage` 回调内无 `if (cond) return;` 早返** | RTL 中无"早返"概念，需用 `when` 条件驱动；早返导致 commit 不一致 | `tools/check_plugin_portability.sh` Check 1（新增）|
| 5 | **`ip/*/tlm/` 业务代码无 `ch_mem` / `ch_reg` / `ch_uint` / `ch::core::context` 渗透** | TLM 模式无 ch::core::context 依赖；ch_mem 仅在 RTL 路径出现 | `tools/check_plugin_portability.sh` Check 2（新增）|
| 6 | **Plugin 内部不调用 `pb.run()`** | `pb.run()` 是顶层入口；Plugin 回调应只读/写 Payload，不触发调度 | `tools/check_plugin_portability.sh` Check 3（新增）|

#### Tier-2：CI 警告（WARN → 不阻塞但需 review）

| # | 约束 | 理由 | 检查工具 |
|---|------|------|----------|
| 1 | 存储声明优先 `cf::plugin::storage::array_store` | 为 Phase 6 双缓冲切换预留接口 | `tools/check_plugin_portability.sh` Check 4（新增）|
| 2 | 位提取走 `extract_idx(addr)` / `extract_tag(addr)` helper | 集中位选表达式，便于 RTL 升级时切换为位选 | 编码规范 + code review |
| 3 | 阶段名复用 `"lookup"` / `"refill"` 等统一字典 | 跨 Plugin 阶段协同需要 | 编码规范 |
| 4 | Plugin 暴露 `issue_request` / `refill_from_memory` / `read_response` 测试 API | 测试隔离需要 | code review |

#### Tier-3：文档 only（info）

| # | 约束 | 理由 | 文档位置 |
|---|------|------|----------|
| 1 | 详细迁移手册（Phase 5 启动时验证）| 5 步骤迁移路径见 §4 | 本 ADR §4 |
| 2 | 阶段命名冲突检测机制 | 防止 Plugin 重复声明同名阶段 | Phase 6 引入 |
| 3 | BundleMapper 模板（TLM/RTL Bundle 互转）| Phase 6 引入，TLM 模式不需要 | ADR-024（推迟） |

### 2.2 `cf::plugin::storage::array_store` 抽象

**位置**：`include/cf/plugin/storage.h`（143 行，已实现）

**API 概要**：
```cpp
namespace cf::plugin::storage {
template <typename T, std::size_t N>
class array_store {
 public:
  // 元素访问（与 std::array 一致）
  constexpr T&       operator[](size_type i)       noexcept;
  constexpr const T& operator[](size_type i) const noexcept;
  T&       at(size_type i);
  const T& at(size_type i) const;
  T*       data()       noexcept;
  const T* data() const noexcept;

  // 容量
  static constexpr size_type size() noexcept;
  static constexpr bool      empty() noexcept;

  // Phase 6 钩子（Phase 1 no-op）
  void commit() noexcept;   // Phase 6: 双缓冲提交
  void reset() noexcept;    // 测试间隔离

  // 迭代器
  iterator       begin()        noexcept;
  iterator       end()          noexcept;
  const_iterator begin()  const noexcept;
  const_iterator end()    const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend()   const noexcept;
};
}  // namespace storage
```

**设计要点**：
- TLM 模式（Phase 1）：`array_store` 内部持有 `std::array<T, N> data_`，`operator[]` 直接转发，**与直接 `std::array` 等价**，编译期零开销
- RTL 模式（Phase 6）：内部切换为 `ch_mem` 影子（`current_` / `shadow_`），`operator[]` 读返回"上一周期 commit 提交值"，`commit()` 在 `pb.run()` 末尾提交 shadow
- 约束：`T` 必须是 `trivially_copyable`（`static_assert` 强制）—— Phase 6 切换为 `ch_mem` 时需位拷贝

### 2.3 调度框架扩展（`PipeBuilder` commit 钩子）

**位置**：`include/cf/plugin/pipe_builder.h`（177 行，已实现）

**新增 API**：
```cpp
class PipeBuilder {
 public:
  using CommitHook = std::function<void()>;

  // 注册 commit 钩子（业务 plugin 在 build() 期间调用）
  void register_commit_hook(CommitHook hook);

  // 查询已注册钩子数
  std::size_t commit_hook_count() const noexcept;

  // 立即执行所有 commit 钩子（pb.run() 末尾自动调用）
  void commit_storages();

  // run() 末尾自动 commit
  void run() {
    for (auto& s : stages_) s.callback();
    commit_storages();
  }
};
```

**典型用法**（L1CachePlugin::build 迁移后）：
```cpp
void L1CachePlugin::build(cf::plugin::PipeBuilder& pb) {
  // ... at_stage 注册 ...

  // 注册 commit 钩子（顺序保证依赖：tags_ → data_ → valid_）
  pb.register_commit_hook([this] { tags_.commit(); });
  pb.register_commit_hook([this] { data_.commit(); });
  pb.register_commit_hook([this] { valid_.commit(); });
}
```

**约束**：
- 钩子必须从 `Plugin::build()` 内调用（不在 `at_stage` 回调内）
- 钩子按注册顺序执行（保证依赖顺序）
- 多次注册同一 storage 会被多次 commit（幂等性由 storage 自己负责）

---

## 3. 兼容性表

| 维度 | CppTLM 现状 | CppHDL 目标 | 不匹配级别 | 修复方案 |
|------|-------------|------------|------------|----------|
| **存储** | `std::array<T, N>` (`L1CachePlugin.h:155-157`) | `ch_mem<T, N>` | A（API） | `array_store<T, N>` 包装 |
| **条件** | `if (hit) return;` (`L1CachePlugin.cpp:135-136`) | `when(cond) { ... }` | C（抽象） | 重构为 `at_stage` 多阶段 |
| **Bundle** | `cf::plugin::uint_t<N>` | `ch_uint<N>` | A（类型） | 类型别名替换 |
| **位操作** | `(addr >> SHIFT) & MASK` (`L1CachePlugin.cpp:108-113`) | `addr[HI:LO]` | A（表达式） | helper `extract_idx/tag` |
| **阶段** | `pb.run()` 一次性 | 周期精确握手 | B（时序） | commit 边界 |
| **时间** | RAW 立即可见 | 上周期 commit 值 | B（时序） | 双缓冲 + `commit()` |

---

## 4. L1CachePlugin 迁移手册（5 步骤）

**目标**：将 L1CachePlugin 从 Phase 1.2 形态迁移到 Phase 5/6 可平滑切换 `ch_mem` 的形态。

### 步骤 1：标签化存储（Tier-1 #3）

**当前**（`L1CachePlugin.h:155-157`）：
```cpp
std::array<cf::plugin::uint_t<kTagBits>,    kNumSets> tags_{};
std::array<cf::plugin::uint_t<kLineDataBits>, kNumSets> data_{};
std::array<cf::plugin::bool_t,               kNumSets> valid_{};
```

**目标**（替换为 `array_store`）：
```cpp
cf::plugin::storage::array_store<cf::plugin::uint_t<kTagBits>,     kNumSets> tags_{};
cf::plugin::storage::array_store<cf::plugin::uint_t<kLineDataBits>, kNumSets> data_{};
cf::plugin::storage::storage::array_store<cf::plugin::bool_t,        kNumSets> valid_{};
```

**机械替换**：`operator[]` 调用语义不变，单元测试无需修改。

### 步骤 2：commit 钩子注册（§2.3）

在 `L1CachePlugin::build()` 末尾添加：
```cpp
pb.register_commit_hook([this] { tags_.commit(); });
pb.register_commit_hook([this] { data_.commit(); });
pb.register_commit_hook([this] { valid_.commit(); });
```

Phase 1 模式下 `commit()` 是 no-op，行为等价。Phase 6 切换后自动启用双缓冲。

### 步骤 3：位提取 helper（Tier-2 #2）

**当前**（`L1CachePlugin.cpp:108-113`）：
```cpp
cf::plugin::uint_t<L1CachePlugin::kIdxBits> idx =
    static_cast<cf::plugin::uint_t<L1CachePlugin::kIdxBits>>(
        (static_cast<uint64_t>(addr) >> kIdxShift) & kIdxMask);
cf::plugin::uint_t<L1CachePlugin::kTagBits> tag =
    static_cast<cf::plugin::uint_t<L1CachePlugin::kTagBits>>(
        (static_cast<uint64_t>(addr) >> kTagShift) & kTagMask);
```

**目标**（在 L1CachePlugin.h 中添加 helper）：
```cpp
// 位提取 helper —— Phase 1: shift+mask; Phase 6: addr[HI:LO] 位选
static constexpr cf::plugin::uint_t<kIdxBits> extract_idx(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (static_cast<uint64_t>(addr) >> kOffsetBits) &
      ((1ULL << kIdxBits) - 1));
}
static constexpr cf::plugin::uint_t<kTagBits> extract_tag(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  return static_cast<cf::plugin::uint_t<kTagBits>>(
      (static_cast<uint64_t>(addr) >> (kOffsetBits + kIdxBits)) &
      ((1ULL << kTagBits) - 1));
}
```

调用点替换：
```cpp
auto idx = extract_idx(addr);
auto tag = extract_tag(addr);
```

### 步骤 4：早返条件重构（Tier-1 #4）

**当前**（`L1CachePlugin.cpp:131-136`）：
```cpp
pb.at_stage("refill", cf::plugin::Phase::LATE, [this]() {
  auto n = refill_node_;
  if (!n) return;

  cf::plugin::bool_t hit = n->operator()(g_hit);
  if (hit) return;  // 命中无需 refill

  // ... refill 逻辑 ...
});
```

**目标**（用 `when(cond)` 表达条件）：

**Phase 1 近似**（保持 `if (!cond) { ... }` 模式）：
```cpp
pb.at_stage("refill", cf::plugin::Phase::LATE, [this]() {
  auto n = refill_node_;
  if (!n) return;
  cf::plugin::bool_t hit = n->operator()(g_hit);
  // 早返改为 when-condition 模式 (Phase 6 when 模板支持)
  // Phase 1: if (hit) skip; Phase 6: when(!hit) { refill; }
  if (hit) {
    // 命中：保持 storage 不变（no-op）
  } else {
    // miss：执行 refill
    // ... refill 逻辑 ...
  }
});
```

**Phase 6 形态**（`when` 模板）：
```cpp
pb.at_stage("refill", cf::plugin::Phase::LATE, [this]() {
  when(!n->operator()(g_hit)) {
    // refill 逻辑（无条件 return）
  };
});
```

迁移核心：删除 `return;` 早返，改为 `if/else` 显式分支或 `when` 条件块。

### 步骤 5：ch_mem 切换（Tier-1 #5）

**当前**（`L1CachePlugin.h:155-157`）：`std::array` 后端

**Phase 6 目标**（内部切到 `ch_mem`，外部 API 不变）：
```cpp
// 步骤 1 的 array_store 包装已就位 —— Phase 6 切换仅改 array_store 内部实现
template <typename T, std::size_t N>
class array_store {
 private:
  // ch_mem<T, N> mem_;  // Phase 6 替换 std::array<T, N> data_;
  // std::array<T, N> shadow_;  // 双缓冲
  // ...
};
```

**业务代码无需修改**（仅 L1CachePlugin.h 包含 `cf/plugin/storage.h`）。

---

## 5. 与 ADR-025/037 的关系

| ADR | 内容 | 与 ADR-040 关系 |
|-----|------|----------------|
| **ADR-025** | Plugin 基类无 `tick()` | Tier-1 #1 直接引用，是 Tier-1 基础 |
| **ADR-037** | Plugin 作为设计范式（D4） | 决定业务代码必须 Plugin-style；ADR-040 进一步约束**存储/条件/位操作**等具体编码模式 |
| **ADR-029** | 模块级 `ImplMode` | Phase 6 引入；ADR-040 定义的 `array_store` 抽象是 ImplMode 切换的载体 |
| **ADR-031** | StageLink/CtrlLink/DirectLink | Phase 6 引入；ADR-040 不依赖，但共享 `when` 模板语义 |
| **ADR-024** | Bundle 三层分层 | Mapper 模板未实现；ADR-040 的 `array_store` 是 Bundle 维度的同类抽象 |

---

## 6. 验证命令

```bash
# Tier-1 强制（6 条）—— 阻塞合并
bash tools/verify_plugin_decision.sh    # 旧 3 条 (tick/state/uint_t)
bash tools/check_plugin_portability.sh  # 新 3 条 (早返/ch_mem泄漏/pb.run)
# Tier-2 警告（4 条）—— code review
# Tier-3 文档（3 条）—— 本 ADR §4 迁移手册

# 整体必须通过
bash tools/verify_adr.sh                # 现有 ADR 漂移检测
```

`tools/check_plugin_portability.sh` 是本 ADR 的**核心 CI 检查**（4 项：`[1/4]` 早返 / `[2/4]` ch_mem 渗透 / `[3/4]` `pb.run()` 调用 / `[4/4]` `array_store` 鼓励）。前三项 `[PASS]/[FAIL]`，第四项 `[PASS]/[WARN]`。

---

## 7. 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-06-10 | 1.0 | 初始版本：3-tier 约束模型 + `array_store` 抽象 + 5 步迁移手册 |
| | | 配套 `include/cf/plugin/storage.h`（143 行）+ `pipe_builder.h` 扩展（`register_commit_hook` / `commit_storages`）|
| | | 配套 `tools/check_plugin_portability.sh`（4 项检查）|

---

*本 ADR 由 Oracle 报告（L1CachePlugin TLM→HDL 前向兼容性分析，2026-06-10）驱动，状态进入 Phase 1 提案。`array_store` 与 commit 钩子已实现，CI 脚本已就位，迁移手册的 Phase 5 验证推迟到 RTL 升级时执行。*
