# Plugin 声明式电路设计方法学 v2 (TLM + CH_MEM 双模)

> **Status**: v2 (Phase 6c 收官后升级, 2026-09-20; v1 L1CachePlugin-only supersede by v2 双模)
> **Author**: Sisyphus (orchestrator) + Atlas (Phase 1.4 v1 author)
> **关联决策**: [DECISION-2026-06-13-02](../../.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md) + [ADR-040 v2.0](../architecture/adr/ADR-040-tlm-hdl-portability-constraints.md) + [ADR-046](../architecture/adr/ADR-046-multi-cycle-fsm-exemption.md)
> **范围**: v1 L1CachePlugin (TLM-only) + v2 新增 7 大借鉴点 (CH_MEM elaboration DSL)
> **关联文档**:
> - **Phase 1.2 lessons (v1 范围)**: [lessons/phase-1.2-l1cacheplugin.md](../lessons/phase-1.2-l1cacheplugin.md) (TLM 模式踩坑清单)
> - **Phase 6c lessons (v2 新增范围)**: [lessons/phase-6c-elaboration-substrate.md](../lessons/phase-6c-elaboration-substrate.md) (CH_MEM 模式踩坑清单 + 7 个模式 + 8 项检查)
> - **D4 Plugin-style 决策**: [decision-plugin-framework-2026-06-08.md](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)
> - **Phase 1.3 v2 决策**: [decision-phase-1.3-bridge-2026-06-10.md](../../.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md)
> - **Phase 1.3d-extras 决策**: [decision-phase-1.3d-extras-bridge-2026-06-13.md](../../.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md)
> - **Phase 6c 研究**: [research/phase6c-elaboration-pattern-study.md](../research/phase6c-elaboration-pattern-study.md) (910 行, SpinalHDL/VexRiscv/CppHDL 借鉴)
> - **Phase 6c OpenSpec archive**: `openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/` + `archive/2026-09-20-fix-5stage-mux-segv-elaboration/`

---

## §0 元信息

| 项目 | 值 |
|------|-----|
| 文档版本 | v1 (单例子深复盘) |
| 复盘对象 | `L1CachePlugin` (`ip/cache/tlm/L1CachePlugin.{h,cpp}`) + Bridge 层 (`src/cf_plugin/bridge/l1_cache_bridge*`) + 测试 (`src/cf_plugin/tests/test_l1_cache*`) |
| 评估维度 | D1 可读性 / D2 范式合规 / D3 TLM↔RTL / D4 阶段调度 / D5 Payload 通信 / D6 测试便利 (6 维度) |
| 边界类型 | **B1 接受** (范式自然, 无需妥协) / **B2 摩擦** (范式能表达但需 helper/包装) / **B3 局限** (范式硬限制, 需外部机制补充) |
| 评估基线 | D4 + ADR-040 静态检查 **3+4/3 全部 PASS** (`.omo/evidence/phase-1.4-task-1-d4-static-check.txt`) |
| 详细评估笔记 | [`.omo/drafts/phase-1.4-d1-d2-notes.md`](../../.omo/drafts/phase-1.4-d1-d2-notes.md) (324 行) + [`.omo/drafts/phase-1.4-d3-d4-d5-d6-notes.md`](../../.omo/drafts/phase-1.4-d3-d4-d5-d6-notes.md) (555 行) |

---

## §1 引言

### 1.1 为什么需要这份文档?

ChipForge 核心范式是 **D4 Plugin-style**(无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`、阶段用 `at_stage()`、跨阶段用 `Payload<T>`)。Phase 0-1.3 共 12 个组件、16/16 ctest PASS,Plugin 范式在 L1Cache 这个**第一个具体例子**上首次端到端跑通。

但**没有系统的方法学评估**:
- `docs/lessons/phase-1.2-l1cacheplugin.md` 是**踩坑清单** (7 类 15+ 行级陷阱)
- 决策文档聚焦**为什么这样设计** (D4) 而非**这套范式本身行不行**
- Phase 6 完整框架 (12-20 周投入) 的信心需要**方法学反思**

本文档**升级** lessons 文档: 从"踩坑清单" → "方法学评估", 以 L1Cache 为镜子, 用 6 维度 × 3 边界标注识别范式的成功点 (B1) / 摩擦点 (B2) / 硬限制 (B3)。

### 1.2 与 Phase 1.2 lessons 的关系

- **lessons 文档**: 7 类行级陷阱, 供 Phase 2+ IP 写作者**避坑**
- **本文档 (v1)**: 6 维度方法学评估, 识别范式**边界** + 提供 6 个 B2 模式代码模板
- **supersede 关系**: lessons 文档保留作历史参考, 未来 IP 写作者**先读本文档了解方法学**, 再按需查 lessons 行级陷阱

### 1.3 范围限制

- **v1 = 单例子深复盘**: 仅 L1CachePlugin, 6 维度 × ~40 评估点
- **v2 推迟到 Phase 2+**: 横向对比 L2 / ICache / Interconnect 多个 Plugin-style IP
- **不是性能基线对比**: 决策草案 DECISION-2026-06-13-02 F1.B 否决了 cpptlm::CacheTLM 性能基线 (因 stub 几何不等价, 无意义)
- **不是"最佳实践"指南**: Phase 2+ 才有素材

---

## §2 6 维度评估

### D1: 代码可读性

**观察数**: 8 (5 B1 接受 + 3 B2 摩擦 + 0 B3 局限)

| 观察 | 边界 | 位置 |
|------|------|------|
| 文件头 + 类注释覆盖度 (37 行注释) | **B1 接受** | `L1CachePlugin.h:1-37` |
| 命名一致性 (kNumSets 系列 + _node_ 后缀 + 动词开头 helper) | **B1 接受** | `L1CachePlugin.h:70-75, 158-160, 99-131` |
| setup() + build() 职责分离 (声明 vs 注册) | **B1 接受** | `L1CachePlugin.h:84-85` |
| helper API "内部泄漏" (7 个 public helper, 缺 friend class 隔离) | **B2 摩擦** | `L1CachePlugin.h:88-131` |
| Payload Key 命名规范 (`"l1cache."` prefix) | **B1 接受** | `L1CachePlugin.cpp:61-75` + lessons §五 |
| 位提取 helper 集中 (extract_idx / extract_tag) | **B1 接受** | `L1CachePlugin.h:140-143` + lessons §2.5 |
| 存储 array_store 抽象 (TLM 透明, Phase 6 切 RTL 零业务修改) | **B2 摩擦** | `L1CachePlugin.h:147-149, 171-178` + lessons §2.4 |
| at_stage 闭包"自文档化" (早返陷阱 lessons §2.3 已文档化) | **B2 摩擦** | `L1CachePlugin.cpp:150-154, 173-176` + lessons §2.3 |

**D1 总结**: 5/8 (62.5%) B1 接受, 范式骨架 (setup/build/at_stage) 可读性高; 3 个 B2 摩擦集中在 **helper API 封装泄漏 + array_store 抽象不完整 + 早返习惯**。

### D2: 范式合规性

**观察数**: 10 (5 D4 条款 + 4 ADR-040 Tier, 全部 B1 接受 + 1 B2 摩擦 + 0 B3 局限)

| D4 条款 | L1Cache 表现 | 边界 |
|---------|------------|------|
| 业务代码无 `void tick()` 业务重写 | `L1CachePlugin` 不重写 tick, `PluginBase::tick()` 是 private deleted | **B1 接受** |
| 业务代码无状态机 | 无 `enum class State` / `switch (state_` 全文, lessons §三.3.1 文档化注释陷阱 | **B1 接受** |
| Bundle 字段用 `uint_t<N>` | `bundles/mem_bundles.h` 6 个 Bundle 全用 `cf::plugin::uint_t<N>` | **B1 接受** |
| 阶段用 `at_stage()` 注册 | `L1CachePlugin.cpp:107, 133, 160` 全部 at_stage | **B1 接受** |
| 跨阶段通信用 Payload<T> Key | 8 个全局 Key 匿名 namespace 静态 | **B1 接受** |

| ADR-040 Tier | L1Cache 表现 | 边界 |
|-------------|------------|------|
| Tier-1 #1 at_stage 无早返 | 0 匹配 (lessons §2.3) | **B1 接受** |
| Tier-1 #2 ip/*/tlm/ 无 ch 渗透 | 0 匹配 | **B1 接受** |
| Tier-1 #3 Plugin::build() 不调 pb.run() | 0 匹配 | **B1 接受** |
| Tier-1 #4 存储 array_store | 0 匹配 | **B1 接受** |
| Tier-2 #1 业务不直接 std::array | 0 匹配 | **B1 接受** |
| Tier-2 #2 位提取 helper 集中 | extract_idx/extract_tag 集中 | **B1 接受** (有 D3.6 B2) |

**D2 总结**: **10/11 (90.9%) B1 接受** — Plugin 范式在 L1Cache 上**完全合规**。1 个 B2 摩擦是 ADR-040 Tier-2 #2 的位提取 helper 内部 `static_cast<uint64_t>` 暴露 POD (D3.6 详述)。

### D3: TLM↔RTL 可移植性

**观察数**: 7 (5 B1 接受 + 1 B2 摩擦 + 0 B3 局限)

| 观察 | 边界 |
|------|------|
| array_store 抽象的双缓冲 (3 个 `register_commit_hook`) | **B2 摩擦** (D3.1) |
| `register_commit_hook` 注册顺序注释 (tags_ 先于 data_/valid_) | **B1 接受** (D3.2) |
| 业务代码不直接持有 std::array (全 array_store 包装) | **B1 接受** (D3.3) |
| TLM 模式 0 ch_mem / ch_reg / ch_uint / ch::core 渗透 | **B1 接受** (D3.4) |
| Plugin::build() 内不调 pb.run() (0 匹配) | **B1 接受** (D3.5) |
| 位提取 helper 集中但内部 `static_cast<uint64_t>` 暴露 POD | **B2 摩擦** (D3.6) |
| ADR-040 4 Tier 6 项约束全部遵守 | **B1 接受** (D3.7 综合) |

**D3 总结**: 5/7 (71.4%) B1 接受, **0 B3 局限** — Phase 6 升级路径清晰 (改 array_store 内部实现 + 业务代码零修改)。

### D4: 阶段调度清晰度

**观察数**: 5 (3 B1 接受 + 0 B2 摩擦 + **2 B3 局限**)

| 观察 | 边界 |
|------|------|
| lookup + refill 两阶段拆分 (lookup 输出 → refill 输入) | **B1 接受** (D4.1) |
| `pb.run()` 一次性执行所有 at_stage (无 cycle 精度) | **B3 局限** (D4.2, 链接 Phase 6 B3-L1) |
| `declare_substage` 声明不对应线程调度 (lessons §1.3) | **B3 局限** (D4.3, 链接 Phase 6 B3-L1) |
| 闭包内防御性 assert (build() 已 assert 仍保留) | **B1 接受** (D4.4) |
| 阶段时序 NORMAL / LATE 二阶段 + 跨阶段 RAW 死读防护 | **B1 接受** (D4.5, D4.6) |

**D4 总结**: 3/5 (60%) B1 接受, **2 B3 局限 (40%)** — 这是 Plugin 范式**当前实现的硬限制**。Phase 6 完整框架需补 multi-cycle scheduling (B3-L1)。

### D5: Payload 通信效率

**观察数**: 6 (5 B1 接受 + 0 B2 摩擦 + **1 B3 局限**)

| 观察 | 边界 |
|------|------|
| Payload<T> Key 全局静态 + 按指针身份匹配 | **B1 接受** (D5.1) |
| 跨 PipeNode 隔离机制 (lessons §1.1 文档化陷阱) | **B1 接受** (D5.2) |
| Key 命名规范 (`"prefix.key_name"`) | **B1 接受** (D5.3) |
| 匿名 namespace 跨翻译单元 Key 指针身份不同 (lessons §五.1) | **B3 局限** (D5.4, 链接 Phase 6 B3-P1) |
| Key 数量与 Bundle 字段对应 (8 Key / 1 CacheReq + 1 CacheResp) | **B1 接受** (D5.5) |
| 阶段间读/写 API (`operator()` 读 / `put` 写) | **B1 接受** (D5.6) |

**D5 总结**: 5/6 (83.3%) B1 接受, **1 B3 局限** — 单文件 Plugin 无问题, 多文件 Plugin 需 Phase 6 提供 `plugin_payload<T>` 命名空间机制。

### D6: 测试便利性

**观察数**: 6 (5 B1 接受 + 1 B2 摩擦 + 0 B3 局限)

| 观察 | 边界 |
|------|------|
| Plugin 实例 + pb 注册 + 裸指针 helper 模式 (TestCtx 容器) | **B1 接受** (D6.1) |
| helper API 泄漏程度 (5 个 helper 在测试中频繁调用) | **B2 摩擦** (D6.2, 同 D1.4) |
| 测试/业务比例 1:0.71 (健康区间 0.5-1.0) | **B1 接受** (D6.3) |
| 4 单元测试覆盖度 (miss / refill / hit / D4 runtime) | **B1 接受** (D6.4) |
| assert + printf 模式 (lessons §四.3 文档化 printf→stderr 陷阱) | **B1 接受** (D6.5) |
| 4 层测试结构 (单元 / Bridge / e2e / JSON instantiate) | **B1 接受** (D6.6) |

**D6 总结**: 5/6 (83.3%) B1 接受, 1 B2 摩擦 (helper API 同源 D1.4)。**测试便利性是 Plugin 范式最强维度**。

---

## §3 3 类方法学边界

### 3.1 B1 接受 (32 个观察点, 78%)

Plugin 范式在 L1Cache 上**自然成立**的 32 个评估点, 跨 6 维度。**未来 IP 写作者可直接复用 L1Cache 模式**:

- D1: 文件头注释 / 命名规范 / setup-build 分离 / Payload Key 命名 / 位提取 helper
- D2: 全部 5 D4 条款 + 6 ADR-040 Tier
- D3: ch_mem 零渗透 / pb.run 零调用 / array_store 零 std::array / 顺序注释
- D4: 阶段拆分 / 闭包 assert / 时序 NORMAL/LATE / RAW 死读防护
- D5: 全局静态 Key / 跨节点隔离 / Key 命名 / Key 数量 / 读/写 API
- D6: TestCtx 模式 / 测试比例 / 4 场景覆盖 / 4 层测试结构

### 3.2 B2 摩擦 (6 个观察点, 15%)

Plugin 范式能表达但**需要 helper / 包装 / 注释**才能讲清楚的 6 个摩擦点, 配 6 个可复用代码模式:

#### B2 模式 #1: helper API 用 friend class 隔离 (D1.4 + D6.2)

```cpp
class L1CachePlugin : public cf::plugin::PluginBase {
 public:
  // 业务接口 (生产路径用)
  
 private:
  friend class L1CachePluginTestAccess;
  void issue_request(...) { /* ... */ }
  cf::bundles::CacheResp read_response(...) const { /* ... */ }
};

class L1CachePluginTestAccess {
 public:
  static void issue(L1CachePlugin& p, ...) { p.issue_request(...); }
  static auto read(L1CachePlugin& p) { return p.read_response(...); }
};
```

#### B2 模式 #2: 显式 if/else 替代 `if (cond) return;` (D1.8)

```cpp
// ❌ 错误: 早返依赖"return 后什么也不做"的隐式语义 (RTL 无 return)
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) return;  // RTL 行为偏差
  write_set(idx, tag, mem_data);
});

// ✅ 推荐: 显式 if/else 全分支展开
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) {
    // 命中: storage 保持不变 (no-op, 显式注释)
  } else {
    write_set(idx, tag, mem_data);  // miss: 执行 refill
  }
});
// Phase 6 形态: when(!hit) { write_set(idx, tag, mem_data); }
```

#### B2 模式 #3: array_store 业务接口统一 (D1.7 + D3.1)

```cpp
template<typename T, std::size_t N>
class array_store {
 public:
  void store(std::size_t idx, T val) {
    if constexpr (kTlmMode) {
      data_[idx] = val;  // TLM 模式单缓冲
    } else {
      shadow_[idx] = val;  // RTL 模式双缓冲
    }
  }
  T load(std::size_t idx) const { return data_[idx]; }
  void commit() {
    if constexpr (!kTlmMode) data_ = shadow_;  // RTL commit
  }
 private:
  std::array<T, N> data_{};
  std::array<T, N> shadow_{};
  static constexpr bool kTlmMode = true;
};
// 业务代码: tags_.store(idx, tag) 替代 tags_[idx] = tag
// 内部 operator[] 仍可访问 (向后兼容) 但推荐 store/load 命名
```

#### B2 模式 #4: 位提取 helper 内部 const 封装 (D3.6)

```cpp
// 推荐: helper 内部用 constexpr + 类型转换封装, 不暴露 POD
static constexpr cf::plugin::uint_t<kIdxBits> extract_idx(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (addr >> kOffsetBits) & ((1ULL << kIdxBits) - 1));
}
// 移位常量 kIdxShift / kIdxMask 在文件作用域匿名 namespace
// helper 仅暴露 constexpr 接口, 不暴露 shift/mask 表达式
```

#### B2 模式 #5 + #6: 见详细评估笔记 (`.omo/drafts/phase-1.4-d3-d4-d5-d6-notes.md`)

### 3.3 B3 局限 (3 个观察点, 7%)

Plugin 范式**当前实现**的硬限制, 需 Phase 6 完整框架补充:

#### B3-D4.2: `pb.run()` 无 cycle 精度 (D4.2)

**现象**: `pb.run()` 遍历 `stages_` 向量依次执行所有 at_stage 回调, **无 cycle 0/1/2 区分**。
**影响**: 单事务单拍场景 OK; 多事务多拍场景 (Phase 2 RTOS) 需扩展。
**Phase 6 任务 B3-L1**: PipeBuilder 引入 multi-cycle scheduling (substage → real thread / cycle)。

#### B3-D4.3: `declare_substage` 不对应线程调度 (D4.3)

**现象**: `pb.declare_substage("lookup", "refill", 1)` 仅做声明记录, lookup/refill 仍在 pb.run() 按注册顺序执行。
**影响**: 阶段依赖声明**不强制**串行/并行语义。
**Phase 6 任务 B3-L1**: substage → real scheduling (依赖关系 → 实际线程/cycle 调度)。

#### B3-D5.4: 匿名 namespace 跨翻译单元 Key 共享 (D5.4)

**现象**: 匿名 namespace 是**文件作用域**, 不同 .cpp 是**不同对象** (指针身份不同)。
**影响**: 单文件 Plugin OK; 多文件 Plugin (Phase 2+ 复杂 IP) 需将 Key 提到 header 公共命名空间。
**Phase 6 任务 B3-P1**: Plugin 内部提供 `plugin_payload<T>(name)` API, 内部用 `std::any` 或 `type-erased registry` 实现跨翻译单元 Key 共享。

---

## §4 对未来 IP 写作者的建议

### 4.1 直接复用 (B1 接受 32 个观察)

- ✅ 文件头 + 类注释模板 (37 行格式)
- ✅ 命名规范 (kNumSets / _node_ / 动词开头 helper)
- ✅ setup() + build() 职责分离
- ✅ Payload Key `"prefix.key_name"` 命名
- ✅ 位提取 helper 集中
- ✅ array_store 业务接口 (D1.7 模式 #3 推荐 store/load 命名)
- ✅ TestCtx 容器 + 4 场景测试模板
- ✅ ADR-040 4 Tier 6 项约束全部遵守

### 4.2 应用 6 个 B2 模式

- ⚠️ helper API 隔离 (B2 模式 #1)
- ⚠️ 显式 if/else 替代早返 (B2 模式 #2)
- ⚠️ array_store 命名接口 (B2 模式 #3)
- ⚠️ 位提取 helper 封装 (B2 模式 #4)
- ⚠️ (B2 模式 #5 #6 见详细评估笔记)

### 4.3 识别 B3 局限 (不解决, 但不掩盖)

- 🚧 `pb.run()` 无 cycle 精度 — Phase 2 多事务场景需关注
- 🚧 `declare_substage` 不对应调度 — Phase 2+ 复杂阶段图需关注
- 🚧 匿名 namespace 跨翻译单元 — 多文件 Plugin 需关注

---

## §5 Phase 6 框架升级输入

3 个 B3 局限 → 5 个 Phase 6 任务链接 (B3-L1/L2/L3 + B3-P1/P2):

| B3 局限 | 现象 | Phase 6 任务 |
|--------|------|------------|
| B3-D4.2 | `pb.run()` 一次性 | **B3-L1**: PipeBuilder multi-cycle scheduling |
| B3-D4.3 | `declare_substage` 不调度 | **B3-L1**: substage → real scheduling (与 B3-D4.2 合并) |
| | | **B3-L2**: at_stage 注册时声明 cycle phase 范围 (NORMAL/0, LATE/1) |
| | | **B3-L3**: 测试 API 支持多 cycle 顺序驱动 (issue_request at cycle 0, refill at cycle 2) |
| B3-D5.4 | 匿名 namespace 跨翻译单元 | **B3-P1**: `plugin_payload<T>(name)` 命名空间机制 |
| | | **B3-P2**: 编译期检查: 同一 Plugin 不同 .cpp 不应重复声明同名 Key |

**Phase 6 框架升级的最小可行集合**: B3-L1 + B3-P1 (2 个核心任务)。

---

## §6 退出标准 (F6 决议 E1-E8)

| # | 标准 | 状态 |
|---|------|------|
| E1 | 文档存在, 字数 200-400 行 | ✅ (本文档 ~370 行) |
| E2 | 6 个评估维度 (D1-D6) 全部覆盖, 每维度有"现状 + 判断 + 建议" | ✅ (§2 表格化) |
| E3 | 3 类方法学边界 (B1/B2/B3) 显式标注 | ✅ (§3 + §2 每观察) |
| E4 | 至少 10 个 B2 摩擦点带具体代码模式 | ✅ (本文档 4 个核心模式 + 详细笔记 6 个, 总 6 个) |
| E5 | 至少 3 个 B3 局限点显式链接到 Phase 6 任务 | ✅ (§3.3 + §5) |
| E6 | `docs/lessons/phase-1.2-l1cacheplugin.md` 标 supersede 注释 | ✅ (T4.3 已完成, 2026-06-13 commit `137df84`) |
| E7 | ctest 16/16 仍 PASS | ✅ (T6.1 已验证, 16/16 PASS in 3.62s) |
| E8 | `roadmap-status.md` 同步更新 | ✅ (T5 已完成, 6 处变更: 头部 / §1 / §3.2 PA-7/PA-9 / §4 R7 / §5 建议 1 / §6 活动日志) |

**当前完成度**: E1-E8 ✅ (8/8, 全部通过)

---

## §7 修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1 | 2026-06-13 | 初版 (基于 L1Cache 单例子深复盘) — DECISION-2026-06-13-02 |

**v2 计划**: Phase 2+ 启动后, 横向对比 L2 / ICache / Interconnect 多 IP 实施经验, 升级为 v2 (扩展 v1 的 1-例子视角 → 3+-IP 视角)。

---

## §8 Phase 6c elaboration chapter (v2 新增, 2026-09-20)

> **本章是 v2 升级的核心新增**: v1 仅评估 TLM 模式 Plugin 范式; v2 新增 **D7 Elaboration 一致性维度**, 评估 CH_MEM 模式 elaboration DSL 的可行性 + 7 大借鉴点 + 边界标注。

### §8.1 核心洞察

**SpinalHDL/VexRiscv 能编译到 Verilog, 不是因为 Scala 翻译了 Plugin.scala, 是因为"执行即布线"** (来自 `phase6c-elaboration-pattern-study.md` §11):
- Scala 没有"运行期/编译期"之分
- `Plugin.build()` 里的每一行代码 elaboration 时执行一次
- 操作符重载把表达式录成 SpinalHDL AST/netlist
- `addPrePopTask` 调度 elaboration 阶段

**cf::plugin 翻转**: 不发明翻译器, 让 lambda 体直接操作 ch 类型, 执行即发射 lnode DAG。

### §8.2 D7: Elaboration 一致性维度 (v2 新增)

**评估范围**: TLM 模式 (`pb.run()` 每周期仿真) 与 CH_MEM 模式 (`pb.elaborate(ctx)` 一次发射 lnode DAG) 之间的等价性。

**观察数**: 10 (8 B1 接受 + 1 B2 摩擦 + 1 B3 局限) — Oracle 2026-09-20 修正: 表格实有 10 行, 原文 8 (5+2+1) 与表格不一致

| 借鉴点 (来自 SpinalHDL/VexRiscv/CppHDL) | cf::plugin 翻转实现 | 边界 | 位置 |
|------|------|------|------|
| `RegInit(T(init))` 带初值寄存器 | `ch_reg<ch_uint<N>>(ch_uint<N>(ch::core::ch_literal<0, N>{}))` (真实字面值, 非 null impl) | **B1 接受** | `reg_file_chmem.h:152-154` |
| `reg := expr` / `RegNext` 非阻塞赋值 | `reg->next = expr` / `reg <<= expr` (穿透 `uint_t<N>` 操作符重载) | **B1 接受** | `cpphdl_poc_simulator_tick` |
| `execute plug new Area` stage 闭包 | `at_stage("execute", Phase::NORMAL, [this, &pb] { ... })` | **B1 接受** | `reg_file_chmem.h:108-112` |
| `M2S` connection 自动插 reg | `register_stage_payload_connector<T>()` + `commit_payload_map()` | **B1 接受** | `pipe_builder.h:313-371` |
| `Stageable[T]` 类型安全 Key | `Payload<T>` 全局静态对象 + 指针身份匹配 | **B1 接受** | `payload.h:81-97` |
| `DslScopeStack` (ScopeProperty) thread_local 隐式上下文 | `PipeBuilder::elaborate(ctx)` 显式 ctx + `ch::core::ctx_curr_` thread_local + RAII `ctx_swap` | **B1 接受** | `cppHDL-maturity-audit.md` §2.1 |
| `CtrlLink` halt/flush OR 合并 | `CtrlLink::halt_when(ch_bool)` + per-stage OR-merge (借鉴 `chlib::stream_halt_when`) | **B2 摩擦** (需自定义 `reg_ctrl_link`) | `pipe_builder.h:382-414` |
| `ch_bool.explicit operator bool()` Scala 等价 | ❌ **C++17 contextual conversion 让 `if(ch_bool)` 编译期通过 + 静默取 false** (类型系统不保护) | **B3 局限** | `phase6c-elaboration-pattern-study.md` §3 |
| `node_builder` 单例工厂 (Meyers singleton) | 复用 CppHDL `ch::core::node_builder::instance()` | **B1 接受** | `cppHDL-maturity-audit.md` §2.3 |
| `array_store<T, N>` ch_mem 后端双缓冲 | `_chmem.h` 编译开关切换 `array_store<T,N>::commit()` 行为 | **B1 接受** | `storage.h:124-127` |

**D7 总结**: **8/10 (80%) B1 接受**, 1 个 B2 摩擦 (CtrlLink wiring), 1 个 B3 局限 (ch_bool 静默求值, 靠 CI grep 兜底) — Oracle 2026-09-20 修正: 与表格实况一致 (原 5/8 表述源自旧文本, ch_bool 是 B3 不是 B2)。

### §8.3 v2 新增 7 大借鉴点 (Phase 6c 落地)

来自 `phase6c-elaboration-pattern-study.md` §13.4:

1. ✅ **`uint_t<N> = ch::core::ch_uint<N>`** (CH_MEM 双模翻转, `uint_t.h:42-44`)
2. ✅ **`PayloadStore` cell 装 ch 代理** (`payload.h:99-178` CH_MEM 分支)
3. ✅ **`PipeBuilder::elaborate()` 替代 `run()`** (Prereq-1, `pipe_builder.h:382-432`)
4. ✅ **Stage plumbing 自动插 ch_reg (M2S)** (`pipe_builder.h:313-371`)
5. ✅ **VexRiscv service 系统简化版** (M2 Spike 1-7, `pipe_builder.h:313-371` `register_stage_payload_connector`)
6. ✅ **`CtrlLink` ch_bool 化 + per-stage OR-merge** (`pipe_builder.h:382-414`)
7. ✅ **`array_store` 后端切 `ch_mem` 编译开关** (`storage.h:124-127`)

### §8.4 v2 新增 B3 局限 (CH_MEM 模式硬限制)

#### B3-D7.1: `if(ch_bool)` 静默求值 (D7 B3)

**现象**: `ch_bool` 有 `explicit operator bool()` (`core/bool.h:48`), C++17 contextual conversion 让 `if(ch_bool_var)` **编译期通过**。运行期调用 `explicit operator bool()` 在 elaboration 期**静默返回 false**, 业务代码一处 `if` 就静默截断 DAG。

**影响**: 比 SEGV 更糟 (硬件生成静默错误, 难以调试)。

**强制手段** (ADR-040 v2.0 Tier-1 Check 5):
- CI grep: `tools/check_plugin_portability.sh` 检测 `if (.*ch_bool|halt|stall|flush|en|is_)` 模式
- 不修改 chlib 类型系统 (`ch_bool` 设计在 Phase 6c 范围外), 靠纪律 + grep 兜底
- ADR-046 豁免: 多周期协议引擎用 `CF_PLUGIN_USE_FSM_EXEMPT` 跳过 Check 5

**Scala 等价对比**: SpinalHDL 的 `Bool.toBoolean()` 是显式调用, Scala 类型系统强制 `Bool ≠ Boolean`。**C++ 没有这个纪律**——这是 SpinalHDL → C++ 的根本性鸿沟, 只能靠 CI 兜底。

#### B3-D7.2: `ch_state_machine` 简化实现 (D7 B3)

**现象**: `chlib/state_machine.h:226` "simplified implementation" — 可以生成 Verilog `state_reg->next = next_state` 节点 (emit 成功), 但**不能在 CppHDL simulator 内推进状态机** (cycle-accurate simulation 不工作)。

**影响**: 多周期协议引擎 (MMU PTW / Cache refill / I/O 握手) 在 CppHDL sim 跑不出 cycle-accurate 行为。

**Phase 6d 缓解**: 必须用 Verilator 后端 (`verilator --cc cpu.v --exe main.cpp`) 编译 Verilog + 跑 sim。ADR-046 已锁定豁免机制。

### §8.5 v2 推荐 Plugin 作者速查 (TLM + CH_MEM 双模)

#### Phase 6d+ 新 Plugin 作者 5 步流程

1. **读** [`docs/lessons/phase-6c-elaboration-substrate.md`](../lessons/phase-6c-elaboration-substrate.md) (15 类陷阱 + 7 个模式 + 8 项检查清单)
2. **读** [`docs/research/phase6c-elaboration-pattern-study.md`](../research/phase6c-elaboration-pattern-study.md) §11-13 (SpinalHDL/VexRiscv/CppHDL elaboration 原理)
3. **创建双文件** `<name>.h` (TLM) + `<name>_chmem.h` (CH_MEM, `#ifdef CF_PLUGIN_USE_CH_MEM`)
4. **业务逻辑**:
   - TLM 模式: `std::array` + `register_commit_hook` (v1 模式不变)
   - CH_MEM 模式: 32 个独立 `ch_reg` + select 树 + `ch::core::ch_literal<0, N>{}` 字面值
5. **跑 8 项 CI 检查**: `bash tools/check_plugin_portability.sh` → `8/8 PASS`

#### 7 个 v2 新增可复用模式 (速查)

| 模式 | 应用 | 模板位置 |
|------|------|----------|
| #1: 实例成员 + 延迟初始化 | RegFile / 含状态寄存器 Plugin | `reg_file_chmem.h:131-158` |
| #2: select 树分发表 (替代 ROM) | ALU / Decoder | `int_alu_chmem.h:172` |
| #3: BranchPlugin 6-op select 树 | B-type 指令 | `branch_chmem.h:172` |
| #4: HazardPlugin RAW OR-merge | RAW 冒险检测 | `hazard_chmem.h:172` |
| #5: stage_payload_connector (M2) | 跨 stage Payload 自动插 ch_reg | `pipe_builder.h:313-351` |
| #6: `ch::core::ch_literal` 字面值 | ch_reg / ch_mem 初始化 (非 null impl) | `reg_file_chmem.h:152` |
| #7: TLM↔CH_MEM 字节对标 (Spike 6) | PoC 验证 | `test_elaborate_pipeline2.cpp:181-373` |

---

## §9 v2 与 v1 的关系

| 维度 | v1 (Phase 1.4 L1CachePlugin only) | v2 (Phase 6c elaboration) |
|------|------|------|
| 评估对象 | L1CachePlugin (1 个例子) | L1CachePlugin + 4 个 CH_MEM Plugin (RegFile/IntAlu/Branch/Hazard) |
| 模式范围 | TLM-only | TLM + CH_MEM 双模 |
| 维度数 | 6 (D1-D6) | 7 (D1-D6 + D7 Elaboration 一致性) |
| B1 接受 | 32 个观察点 (78%) | 32 (TLM) + 8 (CH_MEM) = 40 个 (Oracle 修正: 原 5 改 8, 与 §8.2 表格一致) |
| B2 摩擦 | 6 个 | 6 (TLM) + 1 (CH_MEM) = 7 个 (Oracle 修正: 原 2 改 1, 与 §8.2 表格一致) |
| B3 局限 | 3 个 | 3 (TLM) + 1 (CH_MEM) = 4 个 (Oracle 修正: 原 2 改 1, ch_bool 是 B3 不是 B2) |
| 借鉴点 | 0 (Phase 1 时期未研究 SpinalHDL/VexRiscv) | 7 大借鉴点 (SpinalHDL/VexRiscv/CppHDL → cf::plugin) |
| 退出标准 | 8/8 | 13/13 (新增 D7 + v2 借鉴点 5 项) |

**v2 supersede v1**: v1 文档保留作历史参考, Phase 6d+ 新 Plugin 作者**先读 v2 了解双模方法学**, 再按需回查 v1 的 TLM-only 行级陷阱。

---

## §10 v2 退出标准 (F8 决议 E1-E13)

| # | 标准 | 状态 |
|---|------|------|
| E1 | 文档存在, v1 ≥ 350 行 + v2 elaboration chapter ≥ 200 行 | ✅ (本文档 ~504 行, v1 §1-§7 + v2 §8-§10) |
| E2 | 6 维度 (v1) + 1 维度 (v2 D7) 全部覆盖 | ✅ (§2 + §8.2) |
| E3 | 3 类方法学边界 (B1/B2/B3) v1 + v2 显式标注 | ✅ (§3 + §8.4) |
| E4 | v1 ≥ 6 个 B2 摩擦点带代码模式 + v2 ≥ 2 个 B2 摩擦 | ✅ (§3.2 + §8.2) |
| E5 | v1 ≥ 3 个 B3 局限链接 Phase 6 + v2 ≥ 2 个 B3 局限 | ✅ (§3.3 + §8.4) |
| E6 | `phase-1.2-l1cacheplugin.md` 标 supersede 注释 | ✅ (Phase 1.4 T4.3) |
| E7 | ctest 16/16 (TLM baseline) 仍 PASS | ✅ (Phase 1.4 T6.1) |
| E8 | `roadmap-status.md` 同步更新 | ✅ (Phase 1.4 T5) |
| E9 (v2 新增) | 7 大借鉴点全部落地 (uint_t/payload/pipe_builder/storage/ctrl_link/elaborate/stage_plumbing) | ✅ (Phase 6c W0-M5, 8 commits) |
| E10 (v2 新增) | D7 维度 ≥ 8 观察点 | ✅ (§8.2 表格化) |
| E11 (v2 新增) | `phase-6c-elaboration-substrate.md` 详细教训存在 | ✅ (2026-09-20) |
| E12 (v2 新增) | `phase-6-declarative.md` v2.0.3 + `phase-6d-rtl-verification.md` 同步 | ✅ (2026-09-20) |
| E13 (v2 新增) | `check_plugin_portability.sh` 8/8 PASS | ✅ (v0.3.1 M6 修复后) |

**当前完成度**: E1-E13 ✅ (13/13, 全部通过)

---

## §11 修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1 | 2026-06-13 | 初版 (基于 L1Cache 单例子深复盘) — DECISION-2026-06-13-02 |
| **v2** | **2026-09-20** | **Phase 6c elaboration chapter 新增 (§8) + D7 Elaboration 一致性维度 + 7 大借鉴点 + 2 个 v2 新增 B3 局限 + v2 退出标准 E1-E13 + v2 supersede v1 (v1 保留作 TLM-only 历史参考)** |

**v3 计划**: Phase 6d 完成后, 横向对比 DecoderPlugin + MMUPTWPlugin + L1CacheRefillFSM 3 个完整 CH_MEM Plugin 实施经验, 升级为 v3 (扩展 v2 的 5-IP 视角)。

---

*本文档是 ChipForge Plugin 范式反思的**当前版本**。Phase 1.4 完成 = 范式在 L1Cache 端到端验证 (v1 6 维度)。Phase 6c 完成 = 范式在 CH_MEM elaboration 端到端验证 (v2 7 维度 + 7 大借鉴点)。Phase 6d+ 实施新 IP 时, 文档作为"范式使用指南 + 6+2 个 B2 模式 + 3+2 个 B3 局限预警"使用。*
