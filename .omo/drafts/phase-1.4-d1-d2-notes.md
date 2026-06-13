# D1 + D2 评估笔记 — L1CachePlugin 方法学复盘

> **来源**: Phase 1.4 (DECISION-2026-06-13-02, F1.A + F2 6 维度)
> **关联**: `.omo/plans/phase-1.4-plugin-design-methodology-review.md` T2
> **基线**: D4 + ADR-040 静态检查 3+4/3 全部 PASS (`.omo/evidence/phase-1.4-task-1-d4-static-check.txt`)
> **范围**: D1 (代码可读性) + D2 (范式合规性),仅评估 L1CachePlugin
> **日期**: 2026-06-13

---

## 元信息

| 项目 | 值 |
|------|-----|
| 复盘对象 | `L1CachePlugin` (`ip/cache/tlm/L1CachePlugin.{h,cpp}`) + 桥接层 (`src/cf_plugin/bridge/l1_cache_bridge.{h,cpp,adapter.{h,cpp}}`) |
| 评估维度 | D1 (代码可读性) + D2 (范式合规性) |
| 边界类型 | B1 (接受) / B2 (摩擦) / B3 (局限) |
| D4 条款对照 | 5 条款 (无 tick() / 无状态机 / uint_t<N> / at_stage() / Payload<T>) |
| ADR-040 条款 | 4 Tier (Tier-1 #1-4, Tier-2 #1-2) |

---

## D1: 代码可读性

### D1.1 文件头 + 类注释覆盖度 — **B1 接受**

**观察**: `L1CachePlugin.h:1-37` (37 行文件头注释) 含 7 个信息块:
- 功能描述 / 作者 / 最后修改日期
- 设计目标 (验证 Plugin-style 可行性)
- 架构 (256 sets × 64B direct-mapped)
- D4 合规说明
- Phase 0 限制 (`uint_t<512>` 退化)
- 约束 (TLM 模式不引入 ch::core::context)

**判断**: 信息密度高,跨 reader (业务开发者 / 框架作者 / 验证 reviewer) 一次到位,无需另开 README。**B1 接受**。

### D1.2 命名一致性 — **B1 接受**

**观察**:
- 编译期常量: `kNumSets` / `kTagBits` / `kIdxBits` / `kOffsetBits` / `kLineDataBits` / `kAddrBitsRaw` (L1CachePlugin.h:70-75) — 全部 Google C++ Style 命名规范
- 节点指针: `lookup_node_` / `refill_node_` / `payload_node_` (L1CachePlugin.h:158-160) — 语义明确,后缀 `_node_` 区分节点类型
- helper API: `issue_request` / `refill_from_memory` / `read_response` / `is_set_valid` / `read_tag` (L1CachePlugin.h:99-131) — 动词开头,语义自解释

**判断**: 命名分层清晰 (常量/成员/helper 各有规范),新人读代码 5 分钟内能掌握符号系统。**B1 接受**。

### D1.3 setup() + build() 分离的清晰度 — **B1 接受**

**观察**:
- `setup(PipeBuilder& pb)` 仅做"跨 Plugin 引用声明" (L1CachePlugin.h:84)
- `build(PipeBuilder& pb)` 只做 `at_stage()` 注册 (L1CachePlugin.h:85)
- 分离后 `setup` 可在编译期完成拓扑图构建,`build` 专注于阶段注册,职责单一

**判断**: D4 Plugin 范式骨架,两函数分离体现"声明 vs 实现"边界,业务逻辑全部在 at_stage 闭包内。**B1 接受**。

### D1.4 helper API 的"内部泄漏" — **B2 摩擦**

**观察** (L1CachePlugin.h:88-131):
- 7 个 helper API 公开:`issue_request` / `refill_from_memory` / `read_response` / `is_set_valid` / `read_tag` / `read_data` / `write_set`
- 全部接收 `const std::shared_ptr<PipeNode>&` 参数或返回 `cf::bundles::*` 类型
- 文件注释 (L88-97) 明确说"生产环境应通过 Bundle 流式接口" + "单元测试为了避免注册外部 traffic generator, 直接调用"

**摩擦点**:
- helper API 是**为单元测试而生**,但放在 `public:` 块,生产路径如果不慎调用会绕过 Bundle 流式接口
- 7 个 API 占据 50+ 行代码(L99-L131),接近业务代码 1/3 长度
- 没有 `#ifdef CF_UNIT_TEST` 守卫(虽 lessons §四.4 提到 `helper->issue_request(...)` 是测试惯例,但缺少编译期强制)

**判断**: **B2 摩擦** — 范式能表达(单元测试需要绕过 Bundle 注册外部 traffic_gen 是合理需求),但**缺少 helper 边界**的编译期或文档化强制。

**对未来 IP 写作者的建议** — **B2 模式 #1**:
```cpp
// 推荐: 用 CRTP 或 friend class 把 helper API 限制在测试编译单元
class L1CachePlugin : public cf::plugin::PluginBase {
 public:
  // ... 业务接口 ...
  
 private:
  // 测试辅助: 仅 friend 类可访问
  friend class L1CachePluginTestAccess;
  
  void issue_request(...) { /* ... */ }
  cf::bundles::CacheResp read_response(...) const { /* ... */ }
};

// test_l1_cache_plugin_unit.cpp
class L1CachePluginTestAccess {
 public:
  static void issue(L1CachePlugin& p, ...) { p.issue_request(...); }
  static auto read(L1CachePlugin& p) { return p.read_response(...); }
};
```

### D1.5 Payload<T> Key 命名规范 — **B1 接受**

**观察** (lessons §五):
- Key 命名格式 `"prefix.key_name"`,prefix = 模块名 (如 `"l1cache.addr"`)
- 匿名 namespace 文件作用域 (单 .cpp 翻译单元共享,跨 .cpp 不同实例)
- lessons §1.1 文档化了 "at_stage 创建独立 PipeNode" 陷阱 + 共享 `payload_node_` 修复

**判断**: 命名规范有 `prefix.` 风格避免冲突,跨翻译单元隔离机制清晰,**已有 lessons 文档化**,未来 IP 写作者可复用。**B1 接受**。

### D1.6 位提取 helper 集中度 — **B1 接受**

**观察** (L1CachePlugin.h:140-143):
```cpp
static cf::plugin::uint_t<kIdxBits> extract_idx(
    cf::plugin::uint_t<kAddrBitsRaw> addr);
static cf::plugin::uint_t<kTagBits> extract_tag(
    cf::plugin::uint_t<kAddrBitsRaw> addr);
```

- 集中在类内 `static` helper,`constexpr` 编译期,零运行时开销 (lessons §2.5)
- 修改地址位宽 (`kOffsetBits` / `kIdxBits` / `kTagBits`) 只改一处
- Phase 6 RTL 升级时,helper 内部从 `shift+mask` 切换为 `addr[HI:LO]`,调用点零修改

**判断**: 位提取逻辑集中到 helper 是**B1 接受**的范式实践,Phase 6 迁移友好。**B1 接受**。

### D1.7 存储类型选择 (array_store) — **B2 摩擦**

**观察** (L1CachePlugin.h:147-149):
```cpp
using TagStore  = cf::plugin::storage::array_store<cf::plugin::uint_t<kTagBits>,     kNumSets>;
using DataStore  = cf::plugin::storage::array_store<cf::plugin::uint_t<kLineDataBits>, kNumSets>;
using ValidStore = cf::plugin::storage::array_store<cf::plugin::bool_t,            kNumSets>;
```

- L1CachePlugin.h:171-178 用 `TagStore tags_` / `DataStore data_` / `ValidStore valid_` 三实例化(lessons §2.4)
- `array_store` 内部封装 `std::array` (TLM) / `ch_mem` (RTL) 切换
- 但 lessons §2.4 明确指出**问题**:`tags_[idx] = tag` 在 TLM 是 `std::array::operator[]`,在 RTL 是 `ch_mem::write(idx, tag)`,**API 形态不一致**

**摩擦点**:
- TLM 模式用 `operator[]` 单参数,RTL 模式用 `write(addr, data)` 双参数
- Phase 6 升级时所有 `tags_[idx] = ...` 调用点都需修改 (lessons §2.4 "Phase 1 模式: 直接转发到 std::array" vs "Phase 6 模式: 写入 shadow buffer")
- `array_store` 抽象**没有真正屏蔽** `ch_mem::write` 的双参数 API

**判断**: **B2 摩擦** — `array_store` 是过渡抽象,Phase 1 用着 OK,但**没有**为 Phase 6 RTL 升级提供**完整 API 屏蔽**。需要在 Phase 6 完善 (例如提供 `store(idx, val)` 命名接口)。

### D1.8 at_stage 闭包"自文档化"程度 — **B2 摩擦**

**观察** (lessons §2.3):
```cpp
// ❌ 错误: 早返依赖"return 后什么也不做"的隐式语义
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) return;  // RTL 无 return 概念
  write_set(idx, tag, mem_data);
});
```

- 早返在 TLM 模式"看似正确" (单缓冲,return = 不写)
- 在 RTL 模式"行为偏差" (双缓冲,return 跳过 commit 边界)
- lessons §2.3 推荐:**显式 if/else** 或 Phase 6 的 `when(cond) { ... }` 形式

**摩擦点**:
- `if (cond) return;` 是 C++ 程序员的本能写法,迁移到 RTL 时是隐藏陷阱
- lessons §2.3 记录了这个教训但**没有**编译期/静态检查工具 (lessons §2.3 "配套检查: tools/check_plugin_portability.sh Check 1" 扫描这种模式 — **但本仓库 check_plugin_portability.sh T1.3 evidence 显示 Check 1 [PASS] 0 个匹配,即 L1Cache 源码本身没有违规**)
- 也就是:**L1Cache 源码 OK,但**未来 IP 写作者可能踩这个坑

**判断**: **B2 摩擦** — 范式能表达 (用 if/else 替代),但**习惯陷阱**需要文档化 + code review 把关。

**对未来 IP 写作者的建议** — **B2 模式 #2**:
```cpp
// ✅ 推荐: 显式 if/else 替代 if (cond) return
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) {
    // 命中: storage 保持不变 (no-op, 显式注释)
  } else {
    write_set(idx, tag, mem_data);  // miss: 执行 refill
  }
});
// Phase 6 形态: when(!hit) { write_set(idx, tag, mem_data); }
```

---

## D2: 范式合规性

### D2 条款 1: 业务代码无 `void tick()` 业务重写 — **B1 接受 (有边界)**

**观察**:
- `L1CachePlugin` 继承 `cf::plugin::PluginBase`,**不重写** `tick()`(L1CachePlugin.h:67)
- `PluginBase::tick()` 注释 (L28) 写明 "PluginBase::tick() 已是 private deleted"
- 但 **Bridge 层**有 `tick()` 方法 (`src/cf_plugin/bridge/l1_cache_bridge.cpp` `tick()` 在末尾调 `pb_.run()`,符合 v2 决策 D1' "末尾挂载契约")

**判断**:
- **业务代码 (ip/cache/tlm/L1CachePlugin.cpp)**: 无 `tick()` 重写 — **B1 接受**
- **框架层 (src/cf_plugin/bridge/)**: 有 `tick()` (末尾挂载) — 符合 D1' 契约,**B1 接受**
- **D4 §3.1 grep 静态检查** (`tools/verify_plugin_decision.sh` Check 1): **PASS** (T1.3 evidence 验证)

### D2 条款 2: 业务代码无状态机 — **B1 接受 (有边界)**

**观察**:
- L1CachePlugin.h:39-199 全文无 `enum class State` / `switch (state_` / `state_` 成员变量
- 跨阶段通信全部走 Payload<T> Key (lessons §1.1)
- lessons §三.3.1 记录"注释中包含 enum class State 也会触发失败" (历史陷阱,已修复)

**判断**: 
- 业务代码**完整遵守** — **B1 接受**
- lessons 文档化陷阱 → 未来 IP 写作者不会重蹈覆辙 — **B1 接受**

### D2 条款 3: Bundle 字段用 `uint_t<N>` — **B1 接受**

**观察**:
- `bundles/mem_bundles.h` 6 个 Bundle 全部字段用 `cf::plugin::uint_t<N>` (lessons §一 引用)
- L1CachePlugin.h:51 引用 `bundles/mem_bundles.h`,不直接定义 Bundle
- 业务代码不出现 `ch_uint<>` / `uint64_t addr` (verify_plugin_decision.sh Check 2)

**判断**: **B1 接受** — Bundle 字段用 `uint_t<N>` 已成定式,无需妥协。

### D2 条款 4: 阶段用 `at_stage()` 注册 — **B1 接受**

**观察** (L1CachePlugin.cpp build()):
- `pb.at_stage("lookup", Phase::NORMAL, [...]())` (lookup 阶段,NORMAL 时序)
- `pb.at_stage("refill", Phase::LATE, [...]())` (refill 阶段,LATE 时序)
- 无 `if (state_ == ...)` 模式,无 `switch (state_` 模式
- `declare_substage("lookup", "refill", 1)` 声明 lookup→refill 串行依赖 (lessons §1.3 "不对应线程调度"边界)

**判断**:
- 阶段注册**完整** — **B1 接受**
- 跨阶段时序 (NORMAL / LATE) + substage 声明已足够表达 — **B1 接受**
- **B3 局限** (Phase 6 输入): `declare_substage` 不对应线程调度,Phase 6 完整框架需补 (D4 评估任务)

### D2 条款 5: 阶段间通信用 Payload<T> Key — **B1 接受**

**观察** (L1CachePlugin.cpp 匿名 namespace):
- `Payload<uint_t<64>> g_addr{"l1cache.addr"}`
- `Payload<bool_t>     g_hit{"l1cache.hit"}`
- 等 Key 全局静态,按指针身份匹配,跨 PipeNode 隔离
- lessons §1.1 记录"at_stage 创建独立 PipeNode"陷阱 → 共享 `payload_node_` 修复

**判断**: **B1 接受** — Payload Key 范式已成定式,跨阶段通信无显式成员变量做 IPC。

### D2 额外: ADR-040 4 Tier 移植性约束 — **混合 (B1 + B2 + B3)**

**观察** (T1.3 evidence,`check_plugin_portability.sh` 4/4 PASS):

| Tier | 约束 | L1Cache 表现 | 边界 |
|------|------|------------|------|
| Tier-1 #1 | at_stage 回调内无 `if (cond) return;` 早返 | 0 匹配,源码 OK | B1 接受 |
| Tier-1 #2 | `ip/*/tlm/` 无 ch_mem / ch_reg / ch_uint / ch::core 渗透 | 0 匹配,纯 C++ std::array | B1 接受 |
| Tier-1 #3 | Plugin::build() 内未调用 `pb.run()` | 0 匹配,pb.run() 在 helper API | B1 接受 |
| Tier-1 #4 | 存储声明优先 `array_store` (或无 std::array 存储) | 0 匹配,L1Cache 已用 array_store | B1 接受 |
| Tier-2 #1 | 业务代码不直接持有 `std::array` (推荐 array_store 包装) | L1CachePlugin.h:171-178 用 array_store | B1 接受 |
| Tier-2 #2 | 位提取集中 helper,避免 shift+mask 散布 | extract_idx / extract_tag 集中 | B1 接受 |

**判断**: ADR-040 4 Tier 全部**已遵守** — B1 接受。但 Tier-2 是"鼓励但不强制",**未来 IP 写作者可能忽略** → **B2 摩擦** (需 code review + lessons 文档化)。

---

## 跨维度观察 (D1+D2 共同发现)

### 跨观察 1: 测试 helper API 暴露是"范式妥协"还是"合理设计选择"?

- **D1.4** 标记 helper API 泄漏为 **B2 摩擦**
- **D2 条款 1** 接受 helper 不算"业务 tick() 重写"(在 public 但不重写 tick)
- 两者**不矛盾**: 范式允许 helper API 存在(测试便利),但**缺少编译期/文档化强制**(未来 IP 可能误用)
- **统一建议**: helper API 用 friend class 隔离 + lessons 文档化

### 跨观察 2: "L1Cache 看着 OK" 不等于 "方法学通过验证"

- D4 + ADR-040 全部 PASS → **当前 L1Cache 是 D4 合规的**
- 但 lessons §1.1 §2.3 §2.4 §2.5 记录了**已踩过的坑** — 这些坑**已被 lessons 文档化**,未来 IP 写作者**可能重复**
- **方法学通过的证据** = (a) L1Cache 静态检查 PASS + (b) lessons 文档化 + (c) **未来 IP 不重蹈覆辙** (Phase 2+ 验证)
- 本次 Phase 1.4 只能验证 (a) + (b),(c) 需 Phase 2+ 横向对比

---

## 对未来 IP 写作者的建议 (B2 模式 + 跨观察)

### B2 模式 #1: helper API 用 friend class 隔离 (D1.4)

详见 D1.4 节代码块。

### B2 模式 #2: 显式 if/else 替代 `if (cond) return;` (D1.8)

详见 D1.8 节代码块。

### B2 模式 #3: array_store 用 store(idx, val) 命名接口 (D1.7 + D2 ADR-040)

```cpp
// 推荐: array_store 提供 store(idx, val) + load(idx) 命名接口
// 内部 TLM: store -> std::array::operator[] = val
// 内部 RTL: store -> ch_mem::write(idx, val), 末尾 commit
template<typename T, std::size_t N>
class array_store {
 public:
  void store(std::size_t idx, T val) {
    if (tlm_mode_) {
      data_[idx] = val;
    } else {
      shadow_[idx] = val;  // 双缓冲
    }
  }
  T load(std::size_t idx) const { return data_[idx]; }
  void commit() {
    if (!tlm_mode_) data_ = shadow_;  // RTL 模式 commit
  }
 private:
  std::array<T, N> data_{};
  std::array<T, N> shadow_{};  // RTL 模式
  bool tlm_mode_ = true;
};
```

### 跨观察 建议: 复盘文档 + 自动化检查双轨

- **方法学 v1 文档** (本次产出) + **check_plugin_portability.sh** (已有 4/4 PASS) 双轨保障
- 未来 IP 写作者**先读方法学 v1** + **跑 check_plugin_portability.sh** + **code review 把关 Tier-2 软约束**

---

## T2 评估小结

| 维度 | 观察数 | B1 接受 | B2 摩擦 | B3 局限 |
|------|--------|---------|---------|---------|
| D1 可读性 | 8 | 5 (D1.1/1.2/1.3/1.5/1.6) | 3 (D1.4/1.7/1.8) | 0 |
| D2 范式合规 | 5 条款 + 4 Tier | 5 条款 B1 + 4 Tier B1 | 1 (Tier-2 软约束) | 0 |
| 合计 | 18 | 14 | 4 | 0 |

**B3 局限 0 个** — 意味着 L1Cache 上 D1+D2 维度**没有发现范式硬限制**。但 D3 (TLM↔RTL) + D4 (调度) 可能有 (T3 任务)。

**对未来 IP 写作者**:
- ✅ B1 接受 5+5+4 = 14 个成功点 (直接复用 L1Cache 模式)
- ⚠️ B2 摩擦 4 个 (helper API 隔离 / 显式 if/else / array_store 命名接口 / Tier-2 软约束) — 用提供的 3 个代码模式

**D1+D2 评估完成,进入 D3-D6 评估 (T3)**。
