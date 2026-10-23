# ADR-040：TLM→HDL 移植性约束（三级约束模型 + array_store 抽象）—— v3.0

| 字段 | 值 |
|------|-----|
| 状态 | ✅ **v3.0 Accepted (2026-10-22, Phase 6d.5 Verilator 后端 + 跨后端 parity)** — v2.0 (2026-09-17) 基础不变，新增 §10 Verilator 集成段 |
| 来源 | v1.0: Phase 1 Oracle 报告（L1CachePlugin TLM→HDL 前向兼容性分析，2026-06-10）<br>v2.0: Phase 6c W0 审计 + Oracle 重构报告（cf::plugin 底层语义翻转，2026-09-16）<br>v3.0: Phase 6d.5 Verilator 后端落地 + 上游 `c27543d` 跨后端 parity 修复（2026-10-21） |
| v2.0 决策 | **CH_MEM 是新正道**（翻转 v1.0 "ch 渗透禁令"）：<br>① 业务代码在 `-DCF_PLUGIN_USE_CH_MEM` 下必须用 `ch_uint/ch_reg/ch_bool/ch_mem`（elaboration 正道）<br>② 引入 `CF_PLUGIN_USE_FSM_EXEMPT` 豁免机制（ADR-046）<br>③ 新增 Tier-1 Check 5: at_stage 回调内禁运行期 if(ch_bool)（ch_bool explicit operator bool 上下文转换）<br>④ `tools/check_plugin_portability.sh` v2.0 重订（5 项检查） |
| v3.0 决策 | **Verilator 成为 CH_MEM 路径的第二仿真后端 ⇒ 移植性约束从"单后端语义"扩展为"双后端语义必须一致"**：<br>① 生成 Verilog 与 CppHDL Simulator 的**同拍语义**成为硬要求（§10.1）<br>② 后端语义差异**必须显式契约化**（上游 `mem-read-timing` R3/R4 先例，§10.2）<br>③ 弃用算子治理：`ch_bool::operator\|\|` → `operator\|`（上游 deprecation，§10.3）<br>④ 回归锚形态翻转：跨后端回归锚 SHALL **锁住对齐**而非**钉死漂移**（§10.4）<br>⑤ **payload key 模态约束**：key 的 value type SHALL 用 `cf::plugin::uint_t<N>`，SHALL NOT 用裸 `bool`/`uintN_t` —— 否则 CH_MEM 下 cell 只能装常量 ⇒ 信号退化为 elaboration 期常量（§10.5）<br>⑥ **生产者/消费者顺序契约**：同 stage 内 EARLY(生产者) 必先于 NORMAL(消费者)，SHALL NOT 依赖 plugin 注册顺序（§10.6） |
| 实现 commit | **25e2672 / 9a03bb2 / 918e577 / baa504b / 3e8ada2 / a38a1e4 / 19d4f5d / edad878 / b68996a** (2026-09-17, M1-M5 8+1 commit 全部推送)<br>v3.0: Phase 6d.5 Verilator 后端 + 上游 `c27543d` / `5b37b7d` / `eccf58f` / `cba0571`（2026-10-21） |
| 关联 ADR | ADR-025（Plugin 基类无 tick）、ADR-037（Plugin 作为设计范式）、ADR-046（多周期 FSM 豁免）、ADR-080（**后端间**对拍协议 —— 原「TLM↔CH_MEM 双模对拍」，**范围已由 ADR-088 §7 重定义为「CppHDL Simulator ↔ Verilog/Verilator」**，v1.3.0 转硬门禁） |

---

## v2.0 重大变更摘要（2026-09-17, Phase 6c M5 落地）

### 翻转的核心：CH_MEM 是新正道

| v1.0 (2026-06-10) | **v2.0 (2026-09-17, Phase 6c M5)** |
|---|---|
| `ip/*/tlm/` 业务代码无 `ch_mem/ch_reg/ch_uint` 渗透 | **`*_chmem.h` 业务代码必须用 `ch_*`**（elaboration 正道） |
| TLM 是唯一模式 | **TLM 是 deprecated 模式**（Phase 6c 开始） |
| `pb.run()` 每周期仿真 | **`pb.elaborate()` 一次性发射 lnode DAG** |
| `CtrlLink::halt_when(std::function<bool()>)` | **`CtrlLink::halt_when(ch_bool)`**（CH_MEM 模式） |
| `array_store` 后端 = `std::array` | **`array_store` 后端 = `ch_mem`**（CH_MEM 模式） |

### v2.0 新增内容

1. **CH_MEM 模式编译开关**：`CF_PLUGIN_USE_CH_MEM`（默认 OFF = TLM 兼容；ON = elaboration 正道）
2. **CH_MEM 模式 PipeBuilder.elaborate()**：替代 `pb.run()`，执行所有 at_stage 闭包一次，发射 lnode DAG
3. **Per-stage 信号实体**（VexRiscv Stageable.insert/input/output 移植）：PipeNode cell 装独立 ch 信号
4. **自动 stage 间 ch_reg 插入**（VexRiscv Pipeline.build() Phase 4 移植）：PipeBuilder::auto_insert_stage_regs()
5. **CH_MEM CtrlLink**：`halt_when/throw_when/flush_when` 接收 `ch_bool` 信号句柄
6. **`array_store` ch_mem 后端**（`CF_PLUGIN_USE_CH_MEM` 下）：sread/write + 同步双缓冲 commit
7. **Tier-1 Check 5**（v2.0 新增）：at_stage 回调内禁运行期 `if(ch_bool)` —— ch_bool 有 explicit operator bool() (core/bool.h:48)，C++17 contextual conversion 让 if(ch_bool) 编译期通过，CI grep 静态检查
8. **CF_PLUGIN_USE_FSM_EXEMPT** 机制（ADR-046）：多周期协议引擎豁免 D4 "无状态机" 禁令，但必须用 `chlib::ch_state_machine` DSL

### v2.0 重新设计核心——`cf::plugin` 从"仿真器"变"elaboration DSL"

| 维度 | v1.0 TLM | **v2.0 CH_MEM** |
|---|---|---|
| 闭包执行次数 | N 次 (每周期 run()) | **1 次 (elaborate())** |
| Payload T | POD 值 | **ch 句柄** |
| 仿真驱动 | pb.run() 循环 | **Simulator::tick() / Verilator** |
| Verilog 输出 | 无 | **ch::toVerilog(ctx) → .v 文件** |
| 调度语义 | 周期精确循环 | **lnode DAG elaboration** |

**核心洞察**（来自 Phase 6c Oracle 重构报告）：
- VexRiscv 能编译到 Verilog 不因为 Scala 翻译，是因为"执行即布线"
- C++ 没有 Scala 的 implicit/macro，但 `ch_uint<N>` 操作符重载可达到同样效果
- 不发明翻译器，**让 lambda 体直接操作 ch 类型，执行即发射 lnode DAG**

---

---

## 1. 背景与动机

### 1.1 L1CachePlugin Phase 1.2 暴露的可移植性问题

Phase 1.2 在 `ip/cache/plugins/L1CachePlugin.{h,cpp}` 实现了第一个 Plugin-style IP（256 sets × 64B line，lookup + refill 两阶段，4/4 单元测试 PASS）。然而 Oracle 报告 §2-§5-§6 指出，若 L1CachePlugin 的业务代码**直接**沿用到 Phase 5/6 升级到 CppHDL `ch_mem` 后端，将出现以下三类不匹配点：

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
| 2 | **v1.0 删除（v2.0 重订）**：状态机禁令 | 控制流必须通过 `at_stage` 表达；**多周期协议引擎豁免**（ADR-046）| `tools/verify_plugin_decision.sh` Check 2（**保留原检查，FSM 豁免在 check_plugin_portability.sh v2.0 中通过 `#define CF_PLUGIN_USE_FSM_EXEMPT` 跳过**） |
| 3 | Bundle 字段用 `cf::plugin::uint_t<N>` | 为 `ch_uint<N>` 升级保留类型别名空间（v2.0: `uint_t<N>` = `ch::core::ch_uint<N>` in CH_MEM）| `tools/verify_plugin_decision.sh` Check 3 |
| 4 | **`at_stage` 回调内无 `if (cond) return;` 早返** | RTL 中无"早返"概念，需用 `when` 条件驱动；早返导致 commit 不一致 | `tools/check_plugin_portability.sh` Check 1 |
| 5 | **v2.0 新增**：at_stage 回调内禁运行期 `if(ch_bool)` | ch_bool 有 explicit operator bool()（`core/bool.h:48`），C++17 contextual conversion 让 `if(ch_bool_var)` 编译期通过——必须用 select() 替代；启发式 CI grep 静态检查 | `tools/check_plugin_portability.sh` Check 5（v2.0 新增）|
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

## 5. 与 ADR-025/037/046 的关系

| ADR | 内容 | 与 ADR-040 关系 |
|-----|------|----------------|
| **ADR-025** | Plugin 基类无 `tick()` | Tier-1 #1 直接引用，是 Tier-1 基础 |
| **ADR-037** | Plugin 作为设计范式（D4） | 决定业务代码必须 Plugin-style；v2.0: D4 在 elaboration 语义下兑现（不再只是 TLM 仿真） |
| **ADR-046** | 多周期协议引擎豁免 D4 | v2.0 新增关联：FSM 豁免通过 `#define CF_PLUGIN_USE_FSM_EXEMPT` 触发，check_plugin_portability.sh v2.0 跳过此 Plugin 的 Tier-1 Check 5 |
| **ADR-029** | 模块级 `ImplMode` | v2.0: TLM_ONLY/RTL_ONLY 字段保留但实际只有 CH_MEM 路径（TLM deprecated）；ImplMode 字段 v0.3.0 移除 |
| **ADR-031** | StageLink/CtrlLink/DirectLink | CtrlLink v2.0 ch_bool 化（ADR-046）；`halt_when(ch_bool)` 直接 OR 合并到 stage stall 信号 |
| **ADR-024** | Bundle 三层分层 | Mapper 模板仍未实现；v2.0 由 cf::plugin::uint_t = ch_uint 双模别名替代 |

---

## 6. 验证命令（v2.0）

```bash
# Tier-1 强制（5 条）—— 阻塞合并
bash tools/verify_plugin_decision.sh    # 旧 3 条 (tick/state/uint_t) - v2.0 不变
bash tools/check_plugin_portability.sh  # v2.0 重订: 5 项检查
                                      # [1/5] at_stage 回调内 if-return 早返 (v1.0)
                                      # [2/5] _chmem.h 文件必须含 ch_* / TLM 文件不含 ch_* (v2.0 翻转)
                                      # [3/5] Plugin::build() 内不调用 pb.run() (v2.0 加强: TLM 已废弃)
                                      # [4/5] [WARN] 存储声明优先 array_store 或 ch_mem
                                      # [5/5] [WARN] at_stage 回调内禁运行期 if(ch_bool) (v2.0 新增)

# Tier-2 警告（4 条）—— code review
# Tier-3 文档（3 条）—— 本 ADR §4 迁移手册

# 整体必须通过
bash tools/verify_adr.sh                # 现有 ADR 漂移检测 (含 ADR-046)

# Phase 6c M5 之后, 单一脚本:
bash tools/check_plugin_portability.sh  # 5/5 PASS (含 ADR-046 FSM 豁免)
```

`tools/check_plugin_portability.sh` v2.0 是本 ADR 的**核心 CI 检查**。v1.0 → v2.0 关键差异：
- v1.0: 4 项检查（早返 / ch_mem 渗透 / pb.run / array_store）
- v2.0: 5 项检查（+ ch渗透禁令翻转 + if(ch_bool) grep）

---

## 7. 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-06-10 | 1.0 | 初始版本：3-tier 约束模型 + `array_store` 抽象 + 5 步迁移手册 |
| | | 配套 `include/cf/plugin/storage.h`（143 行）+ `pipe_builder.h` 扩展（`register_commit_hook` / `commit_storages`）|
| | | 配套 `tools/check_plugin_portability.sh`（4 项检查）|
| 2026-09-16 | **2.0** | **Phase 6c 落地**：CH_MEM 是新正道 |
| | | - 翻转 "ch 渗透禁令" → "`*_chmem.h` 必须含 ch_*" |
| | | - 新增 Tier-1 Check 5: at_stage 内禁运行期 `if(ch_bool)` |
| | | - Tier-1 #2 状态机禁令保留 + ADR-046 FSM 豁免机制 |
| | | - `CF_PLUGIN_USE_CH_MEM` 编译开关 + `CF_PLUGIN_USE_FSM_EXEMPT` 豁免标记 |
| | | - `PipeBuilder::elaborate()` + `to_verilog()` + `create_simulator()` |
| | | - `array_store` CH_MEM 后端 = `ch_mem` (sread/write + commit 双缓冲) |
| | | - `CtrlLink::halt_when(ch_bool)` + OR 合并借鉴 `stream_halt_when` |
| | | - Per-stage 信号实体 + auto_insert_stage_regs() (VexRiscv Pipeline.build() Phase 4 移植) |
| | | - `uint_t<N>` = `ch::core::ch_uint<N>` (CH_MEM 模式别名) |
| | | 配套 ADR-046（多周期 FSM 豁免）+ ADR-037 修订（D4 elaboration 兑现）|
| | | 配套 `tools/check_plugin_portability.sh` v2.0（5 项检查）|
| | | 配套 `tests/framework/test_cppHDL_hello_poc.cpp` + `test_plugin_elaborate_hello_poc.cpp` |
| | | 配套 `ip/cpu/plugins/reg_file_chmem.h` + `int_alu_chmem.h` (M3 PoC 骨架) |
| 2026-10-21 | **3.0** | **Phase 6d.5 Verilator 后端落地 + 跨后端 parity**（详见 §10） |
| | | - Verilator 成为 CH_MEM 路径**第二仿真后端** ⇒ 移植性约束扩为"双后端语义必须一致" |
| | | - 上游 CppHDL `c27543d`：async 读端口归组合 + 建 `read_port → data_proxy` 数据流边 ⇒ Simulator 的 `ch_mem::aread` 与生成 Verilog 的连续 `assign` **同为 0-cycle** |
| | | - 上游归档 `sim-mem-read-timing-align-2026-10` (`5b37b7d`) + 跨后端 parity (`eccf58f`) + `dual-backend-mem-read-timing-parity-2026-10` (`cba0571`) |
| | | - **ChipForge 侧业务代码零改动**；`[chmem]` 72/73 → **73/73 PASS**（19389 assertions），vendored ELF **47/47 全 PASS**（16410 assertions），`[verilator]` 48 ELF 全 PASS |
| | | - 弃用算子治理：`ch_bool::operator\|\|` → `operator\|`（上游 deprecation）—— 框架内 4 处已清零（`ctrl-link-chmem-reg-handle`） |
| | | - 回归锚形态翻转：`tests/cpu/test_cpu_chmem_mem_read_drift.cpp` 从"钉死 1 拍漂移"反转为"**锁住 0-cycle 对齐**" |

---

## 10. v3.0 — Verilator 集成段（2026-10-22）

> **本段记录 Phase 6d.5 落地后 CH_MEM 路径的移植性约束变化**。v2.0 的三级约束模型（Tier-1 / Tier-2 / Tier-3）、`array_store` 抽象、`CF_PLUGIN_USE_FSM_EXEMPT` 豁免机制、Tier-1 Check 1-5 全部**不变**。v3.0 只增加一条**跨后端**维度的约束。

### 10.1 双后端语义必须一致（新增硬约束）

CH_MEM 路径现在有**两个**仿真后端：

| 后端 | 入口 | 用途 |
|---|---|---|
| CppHDL Simulator（解释器） | `pb->create_simulator()` | 快速功能仿真 / 单元测试 |
| Verilator（生成 Verilog 后编译） | `pb->to_verilog()` → `verilator` → `sim_main` | RTL 级验证 / 时序形态检查 |

⇒ **同一份 `ch_*` 业务代码 SHALL 在两个后端产出语义一致的结果**。

这不是新约束而是 v2.0 的**自然推论**，但 Phase 6d.5 落地时它第一次成为**可观测的失败面**：上游 `mem-read-timing` R3 曾要求 sync + async 两类读端口都归入 `sequential_instr_list_` ⇒ Simulator 的 `ch_mem::aread` 相对生成 Verilog 的连续 `assign` **滞后 1 拍**。

CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器）⇒ regfile 在同一次 `eval_sequential()` 内先于读端口锁存（regfile node id < `load_read` node id）⇒ **load 的结果 = 上一个访存地址的内容** ⇒ 10 个含 load 的 rv32ui ELF 全错。

### 10.2 后端语义差异必须显式契约化（方法论）

上游把该差异写成**显式 capability + 契约条款 + 配套 pin 测试**（`openspec/specs/mem-read-timing/spec.md` R3/R4），而非隐含在实现里。

⇒ **ChipForge 侧沿用同一方法论**（ADR-083 单一 source of truth 的延伸）：

1. 后端语义差异 SHALL 落在**上游显式契约**里，而不是靠 ChipForge 适配
2. ChipForge 的回归锚 SHALL **钉住契约要求的行为**（此处 = 0-cycle 对齐），而不是钉住当前的缺陷
3. 修复若由上游落地，ChipForge 侧 SHALL **零业务代码改动**

**本次落地的实证**（上游 `c27543d`，2026-10-21）：

| 成分 | 作用 | 变异对照结果 |
|---|---|---|
| ① `read_port → data_proxy` 的 **src 边**（提供拓扑序） | **承重件** | 只撤分类、边保留 ⇒ ELF 仍全绿；两者都撤 ⇒ 精确重现 10 个 FAIL |
| ② **分类**（async 归组合） | 决定与 Verilog 的 0-cycle 对齐 | 决定 `[mem-drift]` pin 的检测对象 |

### 10.3 弃用算子治理（可移植性）

上游把 `ch_bool::operator||` / `operator&&` 标记为 `[[deprecated]]`，提示改用 `operator|` / `operator&`（1-bit OR/AND；`select()` 用于值选择）。

⇒ **本项目标准**：CH_MEM 路径的 1-bit 布尔运算 SHALL 用 `operator|` / `operator&`，SHALL NOT 用已弃用的 `operator||` / `operator&&`。

**框架内范围已清零**（openspec change `ctrl-link-chmem-reg-handle`，2026-10-22）：`include/cf/plugin/` 内 4 处（`ctrl_link.h:150,160` + `pipe_builder.h:434,435`）全部改为 `operator|`。

**业务侧剩余**：`ip/**/*_chmem.h` 另有 ≥60 `||` + ≥90 `&&` ⇒ **另立 change**（30+ 文件各需 `[chmem]` 73/73 + `[verilator]` 48 ELF 回归）。

⚠️ **统计方法约束**：统计 deprecated 算子站点**不能用 grep 数 `||`** —— 同行不含 `ch_bool` 字面量的用法会漏（例：`pipe_builder.h:434` 的 `stall = stall || cl->halt_condition();`）。SHALL 用编译器 `-Wdeprecated-declarations` 逐 TU 统计。

### 10.4 回归锚形态翻转：锁住对齐，而非钉死漂移

`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`（`[mem-drift]`）在缺陷期断言"**存在** 1 拍滞后"（`mismatches == 6` 且 `off_by_one == 6`）。上游修复后它**必然 FAIL**（实测 `0 == 6`）。

⇒ 现改为断言"**skew 已消除**"（每拍观测值 == 当前地址内容，且 != 上一拍地址内容），test case 更名 `chmem_mem_read_drift_aread_has_no_skew` ⇒ 回归锚从"钉死缺陷"变成"**锁住修复**"。

**断言强度**：同时断言"每拍观测值 == 当前地址内容"与"观测值 != 上一拍地址内容"。后者不是冗余 —— 若只断言前者，一个"恰好总是等于上一拍"的错误实现也能在地址回绕的特定序列下蒙混过关。

### 10.5 payload key 模态约束（CH_MEM 双模的隐含前提）

**规则**：跨阶段共享的 `cf::plugin::Payload<T>` key，其 value type `T` **SHALL** 用 `cf::plugin::uint_t<N>`（或 `bool_t`），**SHALL NOT** 用裸 `bool` / `uintN_t` / `std::uintN_t`。

**理由**：`PayloadStore` 的 `put` / `get` / `has` 是**精确类型匹配**（`include/cf/plugin/payload.h:122-128`）。

| key 声明 | TLM 模式 cell | CH_MEM 模式 cell | 可否承载运行期 DAG 节点 |
|---|---|---|---|
| `Payload<uint_t<64>>` | `uint64_t`（POD） | `ch_uint<64>`（代理） | ✅ CH_MEM |
| `Payload<std::uint64_t>` | `uint64_t`（POD） | `uint64_t`（**POD**） | ❌ CH_MEM 只能装常量 |

⇒ 裸类型 key 在 CH_MEM 模式下 cell 里装的是 **POD 常量**：消费端读取得到的是 elaboration 期求值的快照 ⇒ **信号退化为常量**。

**已发生的实例**：`ip/cpu/plugins/csr_chmem.h:191-194` 用 `static_cast<bool>(select(...))` 把运行期表达式折成常量 bool 写入 `Payload<bool>`，消费端 `csr_chmem.h:318` / `ip/cpu/arch/riscv/int_alu_chmem.h:374` 再 `ch_bool(bool)` 读回 ⇒ **CSR 写使能经 payload 通道实际是常量化**（测试改走 `last_csr_write_en_` 观察口 `csr_chmem.h:280-281` 绕开了 payload 通道，故全绿）⇒ **"全绿但零语义"在本仓是既有模式，不是理论风险**。

**TLM 模式零影响**：`uint_t<N>` 在 TLM 解析为裸整型（`include/cf/plugin/uint_t.h:73-87`），`uint_t<8>` ≡ `uint8_t`、`uint_t<64>` ≡ `uint64_t`。唯一差异是 1-bit 标志位：TLM 下 `uint_t<1>` = `uint8_t`（非 `bool`）⇒ 值语义仍 0/1，读点 SHALL 用 `static_cast<bool>` 或等价显式转换。

**CI 落地**：`tools/check_plugin_portability.sh` **Check 13**（WARN-only + 已知违规 allowlist）扫 `ip/**` + `bundles/**` 的非 `*_chmem.h` 文件，禁止裸类型 payload key。修完已知违规后可升级为 FAIL。

### 10.6 生产者/消费者顺序契约（CH_MEM elaboration）

**规则**：payload cell 内的 ch 代理是「C++ 侧重绑的指针」—— `logic_buffer<T>::operator=` 仅重绑 `node_impl_`（上游 `lnode/logic_buffer.tpp:21-26`），不发射 assign 节点；消费者通过 `PayloadStore::get` 拿到的是**读取瞬间的快照**。

⇒ **生产者 SHALL 先写，消费者随后读**。

**唯一不依赖 plugin 注册顺序的做法**：生产者与消费者 SHALL 挂在**同一个 stage 名**下，生产者用 **EARLY** phase、消费者用 **NORMAL** phase（`Phase` enum: `EARLY=0 < NORMAL=1 < LATE=2`；`include/cf/plugin/pipe_builder.h` 的 `elaborate()` 在同一 stage 内按 phase 顺序执行，同 phase 桶内按插入序稳定排序）。

生产者 **SHALL NOT** 注册新的 stage 名 —— `canonical_stage_order()` 是 stage 名字在 `stages_` 的**首现序**，与 plugin 注册顺序无关；新增 stage 名会改变 canonical order 并可能把生产者排到消费者**之后**。生产者需要额外 node 时 SHOULD 用 `declare_substage(parent, sub)`（只建 node，不进 `stages_`）。

**已确认的失效形态**：`ip/cpu/cpu_factory_chmem.h:204` 在 `pb->build()` 之前注册 `at_stage("fetch", EARLY, populate)` ⇒ `"fetch"` 被钉在 `stages_` 最前；而插件的 `at_stage(...)` 只能在 `pb->build()` 内注册（`pipe_builder.h:156-157`）⇒ 任何以 `"fetch"` 为消费端的 producer 若用新 stage 名，**必然排在消费者之后** ⇒ `has()` 恒 false ⇒ **永久静默 Bare**（CH_MEM 的 `elaborate()` 只跑一遍，没有 TLM "下一拍 cell 还在"的补偿）。

**验证纪律**：动态行为 SHALL 经 Simulator 求值（tick ≥ 2 拍后读值）判定；SHALL NOT 用 C++ 侧直接转换或缓冲读取判定（上游 `ch_uint<N>::operator uint64_t` 对非常量节点报错并返回默认值）；探测 cell 是否存在 SHALL 用 `has()` + const-get，SHALL NOT 用非 const `operator()`（miss 时会 emplace `ch_uint<N>()` null impl，污染 `has()` 与 DAG）。

**K5 边界约束**：CppHDL 的表达式即节点 ⇒ 同一 API（如 `OR_combined_halt_conds()`）两次调用得到两个**语义等价但指针不同**的节点 ⇒ 测试断言「值等价」而非「指针相等」；只有「cell 传递」路径（消费者读生产者重绑进 cell 的 proxy）才可断言指针相等。

### 10.7 v3.0 范围纪律

**In scope**：

- Verilator 作为 CH_MEM 第二后端的**移植性约束**表述（§10.1）
- 后端语义差异的**契约化方法论**（§10.2）
- 弃用算子标准与范围（§10.3）
- 跨后端回归锚的形态要求（§10.4）
- **payload key 模态约束 + 生产者/消费者顺序契约**（§10.5 / §10.6）

**Out of scope**：

- **不改 v2.0 的任何约束**（三级模型 / `array_store` / FSM 豁免 / Tier-1 Check 1-5）
- **不改 TLM 模式语义**
- **不引入新的 CI 门禁**（跨后端 parity 的硬门禁归 ADR-080（**范围见 ADR-088 §7**），v1.3.0 窗口）—— 本次新增的 Check 13 是 **WARN-only**，不阻塞 CI
- **不清业务侧 deprecated 算子**（另立 change）
- **不修 CSR / CPU 侧 payload key**（姊妹 change `chmem-cpu-keys-modality`，挂 Phase C.1 入口）

### 10.8 已知违规登记（payload key 模态）

| 文件 | 违规 key | 归属 |
|---|---|---|
| `bundles/csr_bundles.h:42-54` | `CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `Zicsr_OP` / `TRAP_PENDING`（4 个 `Payload<bool>` + 1 个 `Payload<uint8_t>`） | ✅ **已修 (2026-10-22 `chmem-cpu-keys-modality`)** —— 5 个 key 改 `uint_t<1>` / `uint_t<8>` |
| `ip/cpu/plugins/csr_chmem.h:191-194,258` | 运行期表达式被 `static_cast<bool>(select(...))` **折成常量**后写入 | ✅ **已修 (2026-10-22 `chmem-cpu-keys-modality`)** —— 3 处折叠（`CSR_READ_VALID` / `CSR_WRITE_EN` / `TRAP_PENDING`）改为直写 `ch_uint<1>` 节点（`ch_uint<1>(ch_bool)` 走上游 `bool_to_uint` 分支 ⇒ 复用同一 impl 指针） |
| `bundles/csr_bundles.h:17` | 声称 `TRAP_PENDING` 由 `ibus_chmem.h` 消费 —— 实测 `ibus_chmem.h` 中 `TRAP_PENDING` 出现 **0** 次 ⇒ CH_MEM trap 重定向路径疑似从未接线 | ⏸ **仍登记**（`chmem-cpu-keys-modality` 已把注释改为如实记录；**接线**归 `mmu-chmem-phase-b-c-launch` 的 C.1） |
| `bundles/csr_bundles.h:12` | 注释自陈 "1-bit (ch_bool)" 与实际声明 `Payload<bool>` 矛盾 | ✅ **已修 (2026-10-22 `chmem-cpu-keys-modality`)** —— 改为 "1-bit (uint_t<1>)" |
| `ip/cpu/tlm/cpu_keys.h:48` | `CPU_EXCEPTION_CODE`（`Payload<uint8_t>`） | ✅ **已修 (2026-10-22 `chmem-cpu-keys-modality`)** —— 4 个 key 全改 `uint_t<N>`，且 `static_assert` 加 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 |
| `ip/cache/plugins/cache_keys.h:52` | `vipt_fallback`（`Payload<uint8_t>`） | ⏸ **仍登记** —— CH_MEM 侧消费点未落地 ⇒ 现在改无消费点可验证（归"引入其 CH_MEM 消费者的那个 change"） |
| `ip/mmu/tlm/mmu_keys.h` | 其余 8 个 key（`PERMS` / `PTW_ASID` / `PTW_L0_RAW` / `PTW_L1_RAW` / `PTW_L2_RAW` / `PTW_FAULT` / `SATP_MODE` 等） | ⏸ **仍登记** —— 同上（其 CH_MEM 消费点由 `chmem-ptw-walk` 的 `RiscvMMUPluginChmem` 适配器引入） |



## 8. v2.0 范围纪律

### 8.1 9 周 Phase 6c 时间盒 (W0-W9)

```
W0 (Day 1-3): CppHDL 成熟度审计 + OpenSpec change 草稿 + CHANGELOG
  ✅ docs/audit/cppHDL-maturity-audit.md (W0 审计报告)
  ✅ openspec/changes/plugin-elaboration-substrate/{proposal,tasks}.md
  ✅ CHANGELOG.md v0.3.x 占位条目

W1-2 (M1): 底层翻转 — uint_t / payload / pipe_builder.elaborate()
  ✅ include/cf/plugin/uint_t.h (双模 + CF_PLUGIN_USE_CH_MEM 开关)
  ✅ include/cf/plugin/payload.h (cell 装 ch 代理 in CH_MEM)
  ✅ include/cf/plugin/pipe_builder.h (elaborate() + run() deprecated)
  ✅ tests/framework/test_cppHDL_hello_poc.cpp (W0 PoC)
  ✅ tests/framework/test_plugin_elaborate_hello_poc.cpp (M1 PoC)

W3-4 (M2): Stage plumbing — per-stage 信号 + 自动 ch_reg + CtrlLink ch_bool
  ✅ include/cf/plugin/pipe_builder.h::auto_insert_stage_regs() 设计
  ✅ include/cf/plugin/ctrl_link.h (halt_when(ch_bool) + OR 合并)
  ✅ include/cf/plugin/storage.h (array_store ch_mem 后端)

W5-6 (M3): RegFile + IntAlu PoC — 32 ch_reg + select 树
  ✅ ip/cpu/plugins/reg_file_chmem.h (32 ch_reg + x0 select 屏蔽)
  ✅ ip/cpu/arch/riscv/int_alu_chmem.h (if/else → select 树)

W7-8 (M4): Decoder + Branch + Hazard + 5 级流水线 Verilog
  ✅ 骨架记录在 OpenSpec change tasks.md
  ⚠️ 完整 5 级 RTL sim 跑通需要 Harness 迁移 (W9)
  ⏸ 实际代码待 Phase 6d 完整实现

W9 (M5): Harness 迁移 + ADR 收口
  ✅ tools/check_plugin_portability.sh v2.0 重订 (5 项检查)
  ✅ ADR-046 新增 (多周期 FSM 豁免)
  ✅ ADR-040 v2.0 修订 (本文档)
  ⏸ ADR-037 修订 (下一会话)
  ⏸ Harness 迁移 ([cpu-integration] 测试 → CppHDL sim runner / Verilator) - 需要构建环境
```

### 8.2 已交付 vs 推迟

**已交付** (Phase 6c 9 周时间盒内):
- W0-W5 全部 M1-M3 源代码 + 文档
- M4/M5 文档骨架 (OpenSpec change tasks.md)
- ADR-046 + ADR-040 v2.0 + CHANGELOG v0.3.x + check_plugin_portability.sh v2.0

**明确推迟** (Phase 6d+):
- W7-8 5 级流水线 Verilog 实际生成 + riscv-tests tohost=1 验证
- W9 Harness 迁移 (pb.run() → CppHDL sim runner / Verilator)
- IBus/DBus LOAD width extraction
- MMU/PTW 多周期 FSM (依赖 ADR-046 豁免 + ch_state_machine)
- L1Cache refill FSM

### 8.3 关键交付物 (Phase 6c 已落地)

| 文件路径 | 行数 | 内容 |
|---|---|---|
| `include/cf/plugin/uint_t.h` | 75 | 双模 uint_t<N> + bool_t + CF_PLUGIN_USE_CH_MEM 开关 |
| `include/cf/plugin/payload.h` | 195 | 双模 PayloadStore + ch 代理 cell |
| `include/cf/plugin/pipe_builder.h` | 510 | 双模 PipeBuilder + elaborate() + auto_insert_stage_regs() |
| `include/cf/plugin/ctrl_link.h` | 175 | 双模 CtrlLink + ch_bool 化 |
| `include/cf/plugin/storage.h` | 180 | 双模 array_store + ch_mem 后端 |
| `tools/check_plugin_portability.sh` | 230 | 5 项检查 (Tier-1 ch渗透翻转 + if(ch_bool)) |
| `tests/framework/test_cppHDL_hello_poc.cpp` | 165 | W0 PoC 测试源码 |
| `tests/framework/test_plugin_elaborate_hello_poc.cpp` | 130 | M1 PoC 测试源码 |
| `ip/cpu/plugins/reg_file_chmem.h` | 130 | RegFile CH_MEM PoC 骨架 |
| `ip/cpu/arch/riscv/int_alu_chmem.h` | 130 | IntAlu CH_MEM PoC 骨架 |
| `docs/audit/cppHDL-maturity-audit.md` | 197 | W0 审计报告 |
| `openspec/changes/plugin-elaboration-substrate/proposal.md` | 220 | Phase 6c 设计 |
| `openspec/changes/plugin-elaboration-substrate/tasks.md` | 200 | Phase 6c 任务 |
| `CHANGELOG.md` (v0.3.x 段) | 50 | Phase 6c 启动标记 |
| `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` | 220 | 新增 ADR-046 |

**总 Phase 6c 交付代码行数**（不含示例参考）：约 **2815 行**

---

## 9. 下一步会话行动

### 9.1 M5 剩余 (Phase 6d 之前完成)

- [ ] **ADR-037 v2.0 修订**：记录 D4 在 elaboration 语义下兑现
- [ ] **Harness 迁移**：tests/cpu/integration/test_*stage_riscv.cpp 从 `pb.run()` 迁到 `pb.elaborate()` + `ch::Simulator::tick()` / Verilator
- [ ] **测试验证**：在 CF_PLUGIN_USE_CH_MEM 下 ctest 全部 PASS

### 9.2 Phase 6d (下一阶段)

- [ ] DecoderPlugin + BranchPlugin + HazardPlugin 完整 CH_MEM 版本
- [ ] 5 级流水线 Verilog 实际生成 → riscv-tests add/addi/auipc/jal/beq RTL sim tohost=1
- [ ] MMU/PTW 多周期 FSM (使用 chlib::ch_state_machine + CF_PLUGIN_USE_FSM_EXEMPT 豁免)
- [ ] L1Cache refill FSM
- [ ] Verilator 后端集成（生成 .v → 编译 VL1Cache → 替换 C++ sim）

---

*本 ADR v2.0 由 Phase 6c W0 审计 + Oracle 重构报告驱动（2026-09-16）。核心洞察：VexRiscv 能编译 Verilog 不因为 Scala 翻译，是因为"执行即布线"——cf::plugin 不发明翻译器，让 lambda 体直接操作 ch 类型，执行即发射 lnode DAG。CH_MEM 是新正道，TLM 是 deprecated 模式。*
