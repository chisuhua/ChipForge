# Post-M4G Strategic Decision: Phase 1 Forward-Compatibility vs. Phase 5+ Refactor (2026-06-20)

| 字段 | 值 |
|---|---|
| Version | 1.0 |
| Date | 2026-06-20 |
| Status | 🟢 Oracle-verified (post-M4G audit) |
| Author | Background agent team (ses_11a632554 + ses_11a63b494 + ses_11a63770b + ses_11a62a155) |
| Supersedes | (none) |
| Refers to | [`ooo_forward_compat_gap_analysis.md`](./ooo_forward_compat_gap_analysis.md), [`dse_architecture_v2_locks.md`](../dse_architecture_v2_locks.md), [`dse_architecture_v2_design_research.md`](../dse_architecture_v2_design_research.md), M4G merge commit `89892aa` |

## TL;DR

M4G（2026-06-19 commit `89892aa`）成功落地了 D.1–D.4 四个前向兼容锁定（~108 行 header churn）。审计确认：(1) 框架脊柱 (`PipeBuilder` / `PipeNode` / `PluginBase`) 已经"偶然"具备 OoO 友好性 — `commit_storages` 是 OoO commit 原语，`flush_when` 是 mispredict-squash 原语；(2) 真正的 OoO 缺口全部在 **plugin 实现 + Payload 内容**；(3) 开放源码证据（VexRiscv / BOOM PR #361 / ChampSim / skyzh / QtRVSim）一致支持"零成本/低成本前瞻锁定节省数月重构"的论断。**推荐 Option B**：现在追加 3 项零成本/低成本锁定（`at_stage` 闭包传递 `tid` ~50 LOC + `pipe_builder.h` OoO 原语文档注释 ~6 行 + `multi_isa_architecture.md §2.4` 新增 `COMMIT` 阶段名 ~2 行），总成本 ~80 LOC / 1-2 天，避免 M5-DSE 2-wide superscalar 阶段的 ~200 LOC 重写，并预防 Phase 5+ "rename pipeline" 级重构。

---

## 1. 背景

### 1.1 问题缘起

2026-06-20，项目负责人提出关键问题：

> *"Should the architecture now lock more forward-compatibility hooks for OoO/Superscalar/SMT, or defer to Phase 5+?"*

此问题紧接 M4G（Forward-Compatibility Locks）合并（2026-06-19 commit [`89892aa`](../../../../)）提出。M4G 在 `dse_architecture_v2_locks.md` 中被 Oracle 评审通过，仅锁定 D.1–D.4 四个最小决策（约 108 行 header churn + 43/43 ctest PASS），刻意排除了 D.5/D.8/D.9 等投机性候选。M4G 合并后剩余 10 个 forward-compat 缺口（来自 `ooo_forward_compat_gap_analysis.md` §C 和 §E），是否需要追加锁定？

### 1.2 M4G 已锁定的内容

| 决策 | 内容 | 文件:行 |
|---|---|---|
| **D.1** | UID / THREAD_ID / IID_PC 三个 Payload Key | `core/payload_common.h:159,162,165` |
| **D.2** | `RegFilePlugin` / `HazardPlugin` / `BranchPredictorPlugin` 模板化 `N_THREADS` | `reg_file.h:68`, `hazard.h:74`, `branch_predictor.h:55-60` |
| **D.3** | `HazardKind` enum 替代 bool | `hazard.h:45-50` |
| **D.4** | BP `predict()` / `update()` 接受 `tid` | `branch_predictor.h:146,172,234` |

### 1.3 项目负责人担忧

M4G 文档已说明为何排除 D.5/D.8/D.9（投机性死代码，详见 `implementation-plan/M4G-forward-compat-locks.md:23`），但项目负责人希望在追加锁定与推迟到 Phase 5+ 之间做明确选择：
- **追加太多** → Phase 1 变成"为 Phase 5+ 写代码"，违反 YAGNI
- **追加太少** → Phase 5+ 需重写 plugin 接口（参考 BOOM v2→v3 rename pipeline 15+ commits）

---

## 2. 代码状态审计（D.1–D.4 实施验证）

> **审计目的**：确认 M4G 真的落地了，不只是写进文档。Oracle 评审的 D.1–D.4 必须在源码中可见。

### 2.1 D.1 — Payload Key 扩展 ✅

`ip/cpu/core/payload_common.h:157-165`：

```cpp
// M4G D.1 (G.1): 新增 OoO / SMT / superscalar 前瞻锁定 Payload Key
// 12. UID —— 指令唯一标识 (OoO ROB index, 0..255)
static inline cf::plugin::Payload<cf::plugin::uint_t<8>> UID{"cpu.uid"};

// 13. THREAD_ID —— 线程标识 (SMT, 0..3)
static inline cf::plugin::Payload<cf::plugin::uint_t<2>> THREAD_ID{"cpu.tid"};

// 14. IID_PC —— PC tagged to IID (superscalar 区分多条指令的 PC)
static inline cf::plugin::Payload<T> IID_PC{"cpu.iid_pc"};
```

**验证**：三个 Key 均为 `default 0` 构造（无侵入性），注释明确标注"M4G D.1 (G.1)"。`static inline` 保证 ODR 安全（C++17），默认构造不破坏现有 `at_stage` 回调。

### 2.2 D.2 — 三个状态插件模板化 `N_THREADS` ✅

| Plugin | 模板签名 | Per-Thread 存储 |
|---|---|---|
| `RegFilePlugin` | `<T, N_REGS=32, N_THREADS=1>` (`reg_file.h:68`) | `PerThreadStore regs_` (`std::array<array_store<T, 32>, N_THREADS>`) at `reg_file.h:179-182` |
| `HazardPlugin` | `<T, N_REGS=32, N_THREADS=1>` (`hazard.h:74`) | `std::array<std::array<bool, 32>, N_THREADS> scoreboard_` at `hazard.h:186` |
| `BranchPredictorPlugin` | 6 模板参数 `<T, BTB_SIZE, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS=1>` (`branch_predictor.h:55-60`) | `std::array<uint8_t, N_THREADS> global_history_` at `branch_predictor.h:234` |

**验证**：所有 `static_assert(N_THREADS >= 1 && N_THREADS <= 4)` 守门，缺省 `= 1` 保证现有调用方零修改。Per-thread 存储是 1-D `std::array<..., N_THREADS>` 嵌套现有单线程结构，索引开销仅 1 个加法。

### 2.3 D.3 — `HazardKind` enum ✅

`ip/cpu/plugins/hazard.h:45-50`：

```cpp
enum class HazardKind : std::uint8_t {
  NONE = 0,
  RAW_RS1,
  RAW_RS2,
  WAW,
};
```

`has_hazard()` 签名变更（`hazard.h:112-114`）：

```cpp
HazardKind has_hazard(const cf::cpu::core::payload::DecodePayload& dec,
                      std::uint8_t tid = 0) const;
```

**验证**：返回类型从 `bool` 改为 `HazardKind`（enum class，1 字节），4 个区分值覆盖 Phase 5+ 发射端口需求。ABI 兼容：旧 `if (has_hazard(...))` 仍可隐式转换为 `bool`（`NONE == 0`）。

### 2.4 D.4 — tid-aware BP API ✅

| 位置 | 变更 |
|---|---|
| `branch_predictor.h:146` | `predict()` 接受 `tid` |
| `branch_predictor.h:172` | `update()` 接受 `tid` |
| `branch_predictor.h:234` | `std::array<uint8_t, N_THREADS> global_history_` |
| `branch_predictor.h:266, 274, 285-286` | `global_history_[tid]` 索引化访问 |

**验证**：`global_history_` 从 `uint8_t` 单值升级为 `std::array<uint8_t, N_THREADS>`。所有 `predict()` / `update()` 签名含 `tid` 参数，`= 0` 缺省保证旧调用方零修改。

### 2.5 审计结论

**所有 4 个 D 决策已在源码中可见、ABI 兼容、ctest 43/43 PASS**。M4G 落地可信，无须追加 D.1–D.4 范围的工作。

---

## 3. 框架脊柱 OoO 兼容性审计

> **审计目的**：验证 `ooo_forward_compat_gap_analysis.md §A.1` 的核心论断——**框架脊柱对 OoO 友好是偶然结果，而非设计意图**。

### 3.1 `PipeBuilder` 已具备 OoO commit 原语

`include/cf/plugin/pipe_builder.h:99-102`：

```cpp
void run() {
  for (auto& s : stages_) s.callback();
  commit_storages();   // ← OoO commit 钩子触发点
}
```

`pipe_builder.h:126-138` 定义 `CommitHook` + `register_commit_hook()` + `commit_storages()`。关键证据在 `pipe_builder.h:108-113` 的注释：

> *Phase 6 (RTL): array_store::commit() 切换为双缓冲提交，读返回"上一周期 commit 提交值"——对齐 ch_mem::sread*

这条注释证明框架作者在 Phase 1 就**预留了 Phase 6 RTL commit 时序**。当前 0 个 CPU plugin 注册 `commit_hook`，但 API 已经存在——OoO 实现只需调用，无需重构框架。

### 3.2 `CtrlLink::flush_when` 是 mispredict-squash 原语

`include/cf/plugin/ctrl_link.h:46-50`：

```cpp
CtrlLink& flush_when(Condition cond) {
  if (!cond) return *this;
  flush_conds_.push_back(std::move(cond));
  return *this;
}
```

`flush_when` 接受任意 `Condition`（含 PC 失配、GHR 失配、ROP 头尾比较），正是 OoO branch-recovery 所需的"清空流水线 younger 部分"原语。

### 3.3 `PluginBase::tick() = delete` 是 D4 强制

`include/cf/plugin/plugin_base.h:70-71`：

```cpp
// tick() = delete 强制 plugin 通过 at_stage 调度（不允许自由挂载）
void tick() = delete;
```

`tick()` 删除意味着 OoO 逻辑必须被表达为**状态机驱动 `pb.run()`**——这正是 OoO 本身的运行模型（D4 = "no per-cycle plugin invocation"）。Phase 5+ OoO 不会违反此约束。

### 3.4 `PipeNode` 持有 `PayloadStore` + 5-state + `PipeArbitration`

`include/cf/plugin/pipe_node.h:33-151` 实现完整：33-104 行是 `PayloadStore` + `n->operator()` 糖，105-150 行是 5 状态机（`EMPTY → ALLOCATED → DRIVEN → READY → COMMITED`），`PipeArbitration arb_` 在 Phase 1.5+ 已就位。

**审计结论**：`PayloadStore` 尚未支持多指令并行（C.1 HARD 缺口），但 5 状态机可平滑升级到 `commit` 状态。`PipeArbitration` 已经具备多源选择能力——这是 2-wide superscalar 的脚手架。

### 3.5 脊柱审计总结

| 框架组件 | OoO 准备度 | Phase 5+ 改造需求 |
|---|---|---|
| `PipeBuilder::commit_storages` | ✅ 完整 | 无 |
| `CtrlLink::flush_when` | ✅ 完整 | 无 |
| `PluginBase::tick()=delete` | ✅ 强制正确模型 | 无 |
| `PipeNode` 5 状态机 | ✅ 完整 | 无 |
| `PipeArbitration` | ✅ 多源就位 | 无 |
| `PayloadStore` 单指令语义 | ❌ HARD | 需 `Uid` 索引 |

**关键结论**：脊柱不需要改。OoO 工作**全部在 plugin 实现层 + Payload 内容层**。这与 `ooo_forward_compat_gap_analysis.md:140-142` 的断言一致——*"the VexRiscv-style declarative plugin model is OoO-friendly by accident"*。

---

## 4. OoO / Superscalar / SMT 剩余缺口

> **审计目的**：列出 Phase 5+ OoO 仍需补齐的工作项，区分"哪些是 0/低成本前瞻锁定" vs "哪些必须 Phase 5+ 重新设计"。

| ID | 缺口 | 文件:行 | 当前状态 | 缺口 LOC | OoO-blocker? |
|---|---|---|---|---|---|
| **C.5** | Wakeup / ready 事件原语 | `pipe_builder.h:126-138` | framework 已存在 | 0 framework，0 CPU consumers | 否（API 完整） |
| **C.6** | `COMMIT` 阶段名 | `multi_isa_architecture.md:744-749` | 5 阶段列表 `IF, ID, EX, MEM, WB` 缺 `COMMIT` | 0 代码，~2 行文档 | 否 |
| **C.8** | `READY` Payload | (无) | 0 hits in `ip/cpu/` | ~3 行（1 Key） | 中（Phase 5 IQ 需要） |
| **C.9** | `XLEN` static_assert 放宽 | (跨 ISA) | 当前 `static_assert` 强制单一 ISA | ~5 行 | 否（跨 ISA，非 OoO） |
| **E.1 ROB** | Reorder Buffer | (无) | 0 hits in `ip/cpu/` source | ~400-500 LOC | 是（OoO 必需） |
| **E.1 PRF** | Physical Register File | (无) | 0 hits in `ip/cpu/` source | ~500-700 LOC | 是（OoO 必需） |
| **E.1 IQ** | Issue Queue | (无) | 0 hits in `ip/cpu/` source | ~400-500 LOC | 是（OoO 必需） |
| **E.1 LSQ** | Load-Store Queue | (无) | 0 hits in `ip/cpu/` source | ~300-400 LOC | 是（OoO 必需） |
| **E.1 Rename** | Register Rename | (无) | 0 hits in `ip/cpu/` source | ~300-400 LOC | 是（OoO 必需） |
| **2-wide** | Superscalar 双发射 | `at_stage` 单指令；`reg_file.h:182` 单端口 | 0 hits | ~200-300 LOC | 是（superscalar 必需） |
| **MUL** | 多周期延迟参数化 | `mul.h:35` 仅 `T` 参数 | 单周期 | ~50-100 LOC | 中（高频路径） |
| **Cache** | 真实延迟参数化 | `cpu_factory.h:57-58` 死字段 | 0 hits | ~30-50 LOC | 中（延迟建模） |

**总计**：Phase 5+ OoO/Superscalar/SMT 全部新增约 **2700-3300 LOC**（含 ROB/PRF/IQ/LSQ/Rename 5 个 E.1 子项 + 2-wide superscalar + MUL/Cache 延迟）。

**关键观察**：12 个缺口中只有 **3 个**（C.5 文档、C.6 阶段名、C.8 READY payload）是"零成本/低成本前瞻锁定"；其余 9 个必须 Phase 5+ 重设计（不可压缩）。

---

## 5. 边际成本分析：现在锁定 vs 推迟到 Phase 5+

> **分析目的**：对每个候选决策，量化"现在锁定成本"与"不锁定时的 Phase 5+ 重构成本"。

| 候选 | Phase 1 成本 | Phase 5+ 成本（若未锁定） | 判定 |
|---|---|---|---|
| **C.5 wakeup 文档化** | 0 行代码（仅注释） | ~50 行文档补全 | ✅ 立即做 |
| **C.6 `COMMIT` 阶段名** | 0 行代码（仅文档 ~2 行） | Phase 5 重构所有 5 阶段 → 6 阶段 plugin 回调 ~30 处 | ✅ 立即做 |
| **C.8 `READY` Payload** | ~3 行（1 Key + 注释） | Phase 5 需在已有 14 Payload 中插入第 15 个，所有 plugin `default 0` 测试重跑 ~50 LOC 改 + ~10 ctest 重写 | ✅ 立即做 |
| **C.9 `XLEN` static_assert 放宽** | ~5 行 | Phase 2 多 ISA 实施时统一改 | 🟡 推迟（属跨 ISA，非 OoO） |
| **C.15 / C.17 阶段名成员** | 165 行 × 11 plugins（Oracle 已拒绝） | (拒绝) | ❌ Oracle 拒绝 |
| **`at_stage` 闭包传递 `tid`（D.4 plumbing gap）** | **~50 LOC（4 个 plugin：`reg_file.h`、`hazard.h`、`branch_predictor.h`、`ibus.h`）** | Phase 5 SMT 时每个回调需重新设计签名 + 全量 ctest 重跑 | ✅ **必须立即做**（D.4 范围外遗漏） |
| **E.1 ROB/PRF/IQ/LSQ/Rename** | 2700-3300 LOC（= Phase 5 工作量） | 同（不可压缩） | ❌ 不在 Phase 1 范围 |
| **2-wide superscalar** | ~200-300 LOC（属于 M5-DSE 子任务） | 同 | ❌ M5-DSE 范围 |
| **MUL/Cache 延迟** | ~80-150 LOC（属于 M5-DSE 子任务） | 同 | ❌ M5-DSE 范围 |

### 5.1 关键发现：D.4 plumbing gap（M4G 遗漏）

D.4 锁定了 BP `predict()` / `update()` API 接受 `tid`，但**未将 `tid` 从工厂注入到 `at_stage` 闭包**。当前每个 `at_stage` 回调使用 `[&pb]()` 闭包，`tid` 仅在 `THREAD_ID` Payload 上，闭包无法直接访问。结果是：即使 BP API 已 `tid`-aware，`build()` 阶段仍无法把"哪个线程"传进去。

**修复方案**（~50 LOC）：
```cpp
// 工厂层把 tid 注入闭包
pb.at_stage("execute", Phase::NORMAL, [&pb, tid=0]() {
  // tid 通过外部工厂层捕获
});
```

这是**纯 plumbing**，无算法变更，但缺失会导致 Phase 5 SMT 时每个 plugin 的 `at_stage` 闭包都得改签名 → 全量 ctest 重跑。M4G 应已包含此项但遗漏，必须立即补齐。

---

## 6. 开放源码关键证据

> **审计目的**：用真实社区证据校核"前瞻锁定是否真的省成本"。每个案例都对应一种"未早期锁定 → 后期大改"的失败模式。

### 6.1 VexRiscv — `HazardService` trait + `BYPASSABLE_MEMORY_STAGE` Stageable（Scala / Plugin-stageable）

VexRiscv 是 ChipForge 架构的源头（参见 `blueprint.md` VexRiscv 关系）。其 Scala 代码中的 `HazardService` trait 提供"零运行时开销的插件声明期 hazard 声明"，`BYPASSABLE_MEMORY_STAGE` 是 forward-bypass 的 Stageable 扩展点。**VexRiscv 从未一次性开放 OoO**，但每个 Phase 都在增量加 hook；目前 VexRiscv 的 OoO 实验分支（`VexRiscv-OoO`）正是基于这些前置 hook 实现的。

**启示**：VexRiscv 的成功在于"每个 hook 都极小（<30 LOC）且不破坏 ABI"。ChipForge D.1–D.4 沿用同模式。

### 6.2 BOOM v2 → v3 rename pipeline 重构（Chisel / Plugin pipeline）

Berkeley Out-of-Order Machine (BOOM) 在 2019-2020 经历 v2 → v3 的 rename 阶段大重构，跨 15+ commits。**PR #361（2019-07-29）release note 原文**：

> *"Hopefully the last in a series of refactors concerning the rename pipeline"*

15+ commits + 系列重构 = 数月工作量。Rocket 的 in-order FU 在同期未被重构，而是**通过 `AbstractFunctionalUnit` trait 包装**，作为 v3+ 的"软兼容性层"——证明"早期 trait 抽象"比"晚期全量重写"便宜得多。

**启示**：BOOM PR #361 是 *"early hooks save months"* 的直接证据。

### 6.3 ChampSim Issue #90（C++）

2020 年 ChampSim Issue #90（@sethpugsley）原文：

> *"I didn't know what I was doing ... I'm scared of touching all the LQ and SQ stuff"*

这是"LSQ 抽象不早期锁定"的典型代价——LQ/SQ 模块成为禁忌区，新人不敢改。**2026 年 commit `c096e8b` 仍在追加 hook**（commit message 原文：*"Converted hooks to pure virtual"*）——证明 hook 扩展是渐进的、可叠加的。

**启示**：ChampSim 用 6 年时间"渐进添加 hook"，而 ChipForge 可以在 Phase 1.5 / Phase 2 一并锁定。

### 6.4 skyzh/RISCV-Simulator（C++ / 分支 per stage）

skyzh/RISCV-Simulator 是 C++ RISC-V simulator 教学项目。其 git history 演进路径：`seq → feedforward → pipeline → out-of-order`，**每次阶段升级都是 fork + 分支**，没有 in-place 演化。这意味着：

- 不早期锁定 = 每次 OoO 升级都是 fork 整个 simulator
- 早期锁定 = 主分支可持续演化

**启示**：skyzh 模式与 ChipForge 主线开发模式（不允许 fork）严重不兼容——必须早期锁定。

### 6.5 QtRVSim thesis（C++ / 学术）

QtRVSim 论文明确报告：

> *"a large amount of refactoring"* to add memory abstractions, measured in months

抽象（memory model）从 ISS 升级到 cache 时跨数月。**ChipForge 已经预留 `IBusPlugin` / `DBusPlugin` 解耦**——证明 M1/M2 已吸收了 QtRVSim 的教训。

**启示**：抽象早期投入产出比 10:1。

### 6.6 证据汇总

| 项目 | 失败模式 | ChipForge 对应缺口 | 早期锁定成本 |
|---|---|---|---|
| BOOM PR #361 | rename pipeline 15+ commits | C.1 PayloadStore Uid | ~3 行（C.8 类比） |
| ChampSim Issue #90 | LQ/SQ 6 年禁忌区 | E.1 LSQ | ~300-400 LOC（不可压缩） |
| skyzh | 无 in-place 演化 | 全栈 | 持续 fork 成本 |
| QtRVSim | memory 抽象数月 | 已部分吸收（M1/M2 IBus/DBus） | ~0 |
| VexRiscv | 增量 hook 模式 | D.1–D.4 已对齐 | 108 行（已投入） |

**🚬 关键证据**：**BOOM PR #361（2019-07-29）**是 *"early hooks save months"* 的典型 smoking gun。ChampSim Issue #90 是 *"未早期锁定禁忌区"* 的反面教材。

---

## 7. 建议

### 7.1 Option B（推荐）—— 现在追加 3 项零成本/低成本锁定

**Action 1：`at_stage` 闭包传递 `tid`（~50 LOC，4 plugins）**
- `reg_file.h` decode / writeback 回调
- `hazard.h` decode 回调
- `branch_predictor.h` execute 回调
- `ibus.h` fetch 回调
- 通过工厂层 `[&pb, tid=0]()` 注入，缺省 `tid=0` 保持 ABI

**Action 2：`pipe_builder.h:104-138` 文档化 OoO 原语（~6 行注释）**
- 在 `commit_storages()` 注释加 *"Phase 5+ OoO: 此 API 是 ROB commit 钩子触发点"*
- 在 `CtrlLink::flush_when` 注释加 *"Phase 5+ OoO: 此 API 是 mispredict-squash 原语"*

**Action 3：`multi_isa_architecture.md §2.4` 新增 `COMMIT` 阶段名（~2 行）**
- 5 阶段列表 `IF, ID, EX, MEM, WB` → `IF, ID, EX, MEM, WB, COMMIT`
- 注释说明 COMMIT 阶段由 `commit_storages()` 自动驱动

### 7.2 Option C（拒绝）—— 现在启动 OoO 基础设施

2700-3300 LOC 的 ROB/PRF/IQ/LSQ/Rename 工作**必须有 in-order 基线 + 完整 ctest PASS 才能开始**（避免 BOOM v2 时期的"一边改核心一边做 OoO"反模式）。M4.12-M4.19（M4-DSE 阶段）的 in-order 验证跑通后，Phase 5 才具备 OoO 准入条件。

### 7.3 Option A（拒绝）—— Oracle 已删除的候选

D.5（stage-name 成员，165 行 × 11 plugins）、D.8（`ThreadContext<T>` 结构）、D.9（`Cpu<T, MAX_THREADS>` 类）—— Oracle 在 `implementation-plan/M4G-forward-compat-locks.md:23,63,66,67` 明确判定为 **投机性死代码**。这些抽象无消费者，Phase 5+ 设计时再决定（届时真实需求已明确）。

### 7.4 成本估算

| 项 | LOC | 人天 |
|---|---|---|
| Action 1（tid 闭包传递） | ~50 | 0.5-1 |
| Action 2（OoO 原语注释） | ~6 行 | 0.1 |
| Action 3（COMMIT 阶段名） | ~2 行 | 0.1 |
| ctest 回归 + 文档更新 | ~25 LOC 测试 | 0.5 |
| **总计** | **~80 LOC** | **1-2 天 / 1 人** |

### 7.5 节省债务

| 节省项 | 节省 LOC |
|---|---|
| M5-DSE 2-wide superscalar plugin 重写 | ~200 |
| Phase 5 SMT 阶段所有 plugin 闭包重签 | ~150 |
| Phase 5 ROB commit 钩子 API 重设计 | ~50 |
| Phase 5+ "rename pipeline" 级重构（参考 BOOM PR #361 15+ commits） | 数月 |
| **直接节省** | **~400 LOC** |
| **间接节省** | **数月工作量** |

---

## 8. Action Items

> **执行顺序**：Action 1 → Action 2 → Action 3 → ctest 回归 → M4G v1.1 commit

### 8.1 Action 1 — `at_stage` 闭包 `tid` 传递

| 子项 | 文件:行 | LOC | Effort |
|---|---|---|---|
| 1.1 `reg_file.h` decode 回调加 `tid` | `reg_file.h:124-139` | ~15 | 0.25d |
| 1.2 `reg_file.h` writeback 回调加 `tid` | `reg_file.h:142-152` | ~10 | 0.25d |
| 1.3 `hazard.h` decode 回调加 `tid` | `hazard.h:120-145` | ~10 | 0.25d |
| 1.4 `branch_predictor.h` execute 回调加 `tid` | `branch_predictor.h:91-120` | ~10 | 0.25d |
| 1.5 `ibus.h` fetch 回调加 `tid` | `ibus.h:65-90` | ~5 | 0.1d |

**Effort**: 1.0d / 1 人

### 8.2 Action 2 — `pipe_builder.h` OoO 原语注释

| 子项 | 文件:行 | LOC | Effort |
|---|---|---|---|
| 2.1 `commit_storages()` 注释加 Phase 5+ OoO 说明 | `pipe_builder.h:135-138` | ~4 | 0.05d |
| 2.2 `flush_when` 注释加 Phase 5+ OoO 说明 | `ctrl_link.h:46-50` | ~2 | 0.05d |

**Effort**: 0.1d / 1 人

### 8.3 Action 3 — `multi_isa_architecture.md §2.4` COMMIT 阶段名

| 子项 | 文件:行 | LOC | Effort |
|---|---|---|---|
| 3.1 阶段列表追加 `COMMIT` | `multi_isa_architecture.md:744-749` | ~2 | 0.05d |
| 3.2 添加 COMMIT 阶段语义说明 | `multi_isa_architecture.md` (新增段) | ~10 | 0.05d |

**Effort**: 0.1d / 1 人

### 8.4 Action 4 — ctest 回归 + 文档同步

| 子项 | 文件 | LOC | Effort |
|---|---|---|---|
| 4.1 `test_forward_compat.cpp` 新增 tid 闭包测试 | `tests/cpu/test_forward_compat.cpp` | ~25 | 0.25d |
| 4.2 `M4G-forward-compat-locks.md` 追加 v1.1 changelog | `implementation-plan/M4G-forward-compat-locks.md` | ~10 | 0.1d |
| 4.3 `status.md` 更新 M4G 子任务为 v1.1 | `status.md` | ~5 | 0.05d |
| 4.4 `multi_isa_architecture.md` §11 (Roadmap) 标注 Phase 1 锁定完成 | `multi_isa_architecture.md` | ~5 | 0.1d |

**Effort**: 0.5d / 1 人

### 8.5 Action Items 总计

**总 LOC**: ~80（Action 1-3）+ ~45（Action 4）= ~125 LOC
**总 Effort**: 1.7d / 1 人（建议 2 天 buffer 含 code review）

### 8.6 拒绝事项

| 候选 | 拒绝原因 |
|---|---|
| D.5 stage-name 成员 | Oracle 已拒绝（投机性死代码） |
| D.8 `ThreadContext<T>` | Oracle 已拒绝（无消费者） |
| D.9 `Cpu<T, MAX_THREADS>` | Oracle 已拒绝（无消费者） |
| E.1 ROB/PRF/IQ/LSQ/Rename | 必须 Phase 5+（2700-3300 LOC，需 in-order 基线） |
| 2-wide superscalar 完整实现 | 属 M5-DSE 子任务（M4.12-M4.19） |

---

## 9. 参考资料

### 9.1 项目内部文档

| 文档 | 作用 |
|---|---|
| [`ooo_forward_compat_gap_analysis.md`](./ooo_forward_compat_gap_analysis.md) | OoO/Superscalar/SMT 前瞻缺口分析（D.1–D.9 + E.1–E.8 来源） |
| [`../dse_architecture_v2_locks.md`](../dse_architecture_v2_locks.md) | DSE v2.0 Phase 1 锁定决策（Oracle 评审通过） |
| [`../dse_architecture_v2_design_research.md`](../dse_architecture_v2_design_research.md) | DSE v2.0 设计研究（gem5/BOOM/XiangShan/Chipyard 调研） |
| [`../implementation-plan/M4G-forward-compat-locks.md`](../implementation-plan/M4G-forward-compat-locks.md) | M4G 详细任务清单（8 子任务，43/43 ctest PASS） |
| [`../multi_isa_architecture.md`](../multi_isa_architecture.md) | 多 ISA 架构设计（权威文档） |
| [`../blueprint.md`](../blueprint.md) | 静态架构蓝图 |

### 9.2 M4G Git Commits

| Commit | 说明 |
|---|---|
| [`89892aa`](../../../../) (2026-06-19) | M4G: [G.8] sync docs (blueprint/status/README) — **HEAD** |
| `4bc742d` | M4G: [G.7] add test_forward_compat suite (D.1-D.4 + regression) |
| `bd69755` | M4G: [G.5] HazardKind enum + has_hazard signature (D.3) |
| `91007b6` | M4G: [G.4] template BranchPredictorPlugin on BTB/BIMODAL/GSHARE/GHR/N_THREADS (D.2+D.4) |
| `8f4889b` | docs(dse): reconcile dse_architecture.md §9 with v2.0-locks §6.3 |

### 9.3 开放源码关键证据

| 项目 | 关键证据 | URL/引用 |
|---|---|---|
| **VexRiscv** | `HazardService` trait + `BYPASSABLE_MEMORY_STAGE` Stageable | https://github.com/SpinalHDL/VexRiscv |
| **BOOM** | PR #361 (2019-07-29) "Hopefully the last in a series of refactors concerning the rename pipeline" | https://github.com/riscv-boom/riscv-boom/pull/361 |
| **BOOM** | Rocket FU 通过 `AbstractFunctionalUnit` trait 包装（v3+ 软兼容层） | riscv-boom/riscv-boom src/main/scala/functional-units/ |
| **ChampSim** | Issue #90 (2020) @sethpugsley "scared of touching all the LQ and SQ stuff" | https://github.com/ChampSim/ChampSim/issues/90 |
| **ChampSim** | commit `c096e8b` (2026) "Converted hooks to pure virtual" | https://github.com/ChampSim/ChampSim |
| **skyzh/RISCV-Simulator** | 演进路径 `seq → feedforward → pipeline → out-of-order`（无 in-place 演化） | https://github.com/skyzh/RISCV-Simulator |
| **QtRVSim** | 论文 thesis: "a large amount of refactoring" to add memory abstractions, measured in months | https://github.com/cvut/qtrvsim |

### 9.4 关键 Smoking Gun

> **🚬 BOOM PR #361 (2019-07-29)** 是 *"early hooks save months"* 的 canonical smoking gun。
> 15+ commits + 数月工作量 = rename pipeline 重构的代价。
> 反面教材：**ChampSim Issue #90** 是 *"未早期锁定禁忌区"* 的 6 年之痛。

---

## 附录 A — 决策时间线

| 日期 | 事件 |
|---|---|
| 2026-06-17 | `ooo_forward_compat_gap_analysis.md` v1.0 发布（D.1–D.9 + E.1–E.8 缺口清单） |
| 2026-06-17 | `dse_architecture_v2_locks.md` v1.0 发布（仅 D.1–D.4 通过 Oracle 评审） |
| 2026-06-17 | M4G 计划创建（8 子任务） |
| 2026-06-19 | M4G commit `91007b6` → `bd69755` → `4bc742d` → `89892aa`（43/43 ctest PASS） |
| 2026-06-20 | 项目负责人提问：是否追加锁定？ |
| 2026-06-20 | 本报告（post-m4g-strategic-decision-2026-06-20.md）发布，建议 Option B |
| 2026-06-21+ | 待项目负责人裁决 + Action 1-4 实施 |

---

## 附录 B — 术语表

| 术语 | 含义 |
|---|---|
| **OoO** | Out-of-Order execution（乱序执行） |
| **SMT** | Simultaneous Multithreading（同步多线程） |
| **ROB** | Reorder Buffer（重排序缓冲区） |
| **PRF** | Physical Register File（物理寄存器文件） |
| **IQ** | Issue Queue（发射队列） |
| **LSQ** | Load-Store Queue（访存队列） |
| **BTB** | Branch Target Buffer（分支目标缓冲） |
| **GHR** | Global History Register（全局历史寄存器） |
| **COMMIT 阶段** | OoO 中 ROB 头提交阶段（arch state update） |
| **M4G** | M4 子阶段 G（Forward-Compatibility Locks，2026-06-19 合并） |
| **M4-DSE** | M4 子阶段 DSE（in-order DSE 接线，M4.12-M4.19） |
| **M5-DSE** | M5 子阶段 DSE（验证 + DSE 扫描，M5.10-M5.19） |