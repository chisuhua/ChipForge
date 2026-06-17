# CPU 架构 DSE v2.0 — Phase 1 Forward-Compatibility Locks

| 字段 | 值 |
|------|-----|
| 版本号 | 2.0-locks |
| 日期 | 2026-06-17 |
| 状态 | 🟢 **Accepted for Phase 1 Implementation** |
| 适用范围 | ChipForge IP/CPU 子系统 |
| 父文档 | [`dse_architecture.md`](dse_architecture.md) v1.0 |
| 设计研究 | [`dse_architecture_v2_design_research.md`](dse_architecture_v2_design_research.md) (完整调研与 Phase 5+ 设计) |
| Oracle 评审 | ✅ 已审核（2026-06-17） |

> **本文档定位**: 从 `dse_architecture_v2_design_research.md` 提取的**可立即实施的 Phase 1 锁定决策**。
>
> **Oracle 评审结论** (2026-06-17):
> - ✅ **D.1-D.4 是正确的**: ~50 行代码，零行为改变，防止 ~2000 行 Phase 5+ 重构
> - ❌ **D.5/D.8/D.9 应删除**: 投机性死代码，违反 YAGNI
> - ❌ **BranchPredictorFactory 应删除**: 破坏插件模型一致性
> - ❌ **§5-§8 应推迟**: 伪装成 Phase 1 设计的 Phase 5+ 实现
>
> **本文档仅包含 D.1-D.4**。完整的设计研究（多架构 DSE 框架、分支预测器工厂、SMT 接口、DSE 方法论升级）保留在 [`dse_architecture_v2_design_research.md`](dse_architecture_v2_design_research.md)，推迟到 Phase 5 准备时实施。

---

## 目录

1. [当前架构的 2 个硬墙 + 6 个中等障碍](#1-当前架构的-2-个硬墙--6-个中等障碍)
2. [Phase 1 必须锁定的 4 项决策 (D.1-D.4)](#2-phase-1-必须锁定的-4-项决策-d1-d4)
3. [可以推迟到 Phase 5+ 的 8 项决策 (E.1-E.8)](#3-可以推迟到-phase-5-的-8-项决策-e1-e8)
4. [实施路线图](#4-实施路线图)
5. [风险与限制](#5-风险与限制)
6. [附录: 与设计研究文档的关系](#6-附录-与设计研究文档的关系)

---

## 1. 当前架构的 2 个硬墙 + 6 个中等障碍

> 本节提炼 [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md) 的核心结论。

### 1.1 框架脊柱 (不需要改)

三个类构成框架的**不变脊柱**,OoO/Superscalar/SMT 不需要拆解:

- **`cf::plugin::PipeBuilder`** (`include/cf/plugin/pipe_builder.h:54-172`): 拥有 plugins, stages, nodes, commit hooks
- **`cf::plugin::PluginBase`** (`include/cf/plugin/plugin_base.h:48-72`): 声明 `setup()` / `build()`; `tick()` 是 `= delete` (D4 强制执行)
- **`cf::plugin::PipeNode`** (`include/cf/plugin/pipe_node.h:33-151`): 持有 `PayloadStore` + 5 态状态机 + `PipeArbitration arb_`

框架已经提供两个非显然的 OoO hooks **免费**:
- **Commit hooks** (`pipe_builder.h:128-138`): `pb.run()` 以 `commit_storages()` 结束 — 框架**已经有** end-of-cycle commit 的概念
- **CtrlLink** (`include/cf/plugin/ctrl_link.h:24-96`): `flush_when` 是 OoO 用于 branch-mispredict recovery 的原语

**核心洞察**: VexRiscv 声明式 plugin 模型是 **OoO-friendly by accident**。框架脊柱不需要改；工作全部在插件实现。

### 1.2 2 个硬墙 (Phase 5+ 必须重写)

#### 硬墙 1: `PayloadStore` 按类型键控,不按 IID 键控

**位置**: `include/cf/plugin/payload.h:93-154` (`PayloadStore`); `include/cf/plugin/pipe_node.h:81-104` (`PipeNode::operator()`)。

**问题**: `PipeNode` 持有一个 `PayloadStore`,每个 Key 恰好一个值。store 没有 "这是 Uid 7 vs Uid 8 的值" 的概念。当 OoO 有 10 个飞行中指令都在 `EX` 等待不同操作数时,EX stage 必须持有 10 份 `RS1`, `RS2`, `DECODE`, `RESULT`。当前 `PayloadStore` 做不到。

**缓解 (Phase 1 推荐)**: 不改框架。只添加 `Uid` 作为 Payload (key type `uint_t<ROB_BITS>`),要求插件自己线程化。这是每个生产 OoO 的做法 (Uid 是数据,不是元数据)。→ **D.1**

#### 硬墙 2: `RegFilePlugin::regs_` 是单一全局数组,无 per-thread 隔离

**位置**: `ip/cpu/plugins/reg_file.h:158-161`。

**问题**: `RegFilePlugin<T>::regs_` 是单一 `array_store<T, 32>`。SMT 需要 N 个架构寄存器文件 (每线程一个)。OoO 需要*物理*寄存器文件 — `regs_` 将成为*已提交*的架构文件,在 `RenamePlugin` (尚不存在) 内有单独的 PRF。

**缓解 (Phase 1 推荐)**: 模板化插件为 `<typename T, std::size_t N_REGS, std::size_t N_THREADS = 1>`。默认 `N_THREADS = 1` 保持当前行为。→ **D.2**

### 1.3 6 个中等障碍 (Phase 1 可以小成本避免 Phase 5+ 重写)

| # | 障碍 | 位置 | Phase 1 缓解 |
|---|------|------|-------------|
| 1 | `HazardPlugin::scoreboard_` 假设一个架构寄存器只有一个飞行中写入者 | `hazard.h:152-153` | 返回 `HazardKind` enum + `tid` 参数 → **D.3** |
| 2 | `BranchPredictorPlugin::global_history_` 是单一 uint8_t | `branch_predictor.h:215` | 模板化 `N_THREADS` + `tid` 参数 → **D.4** |
| 3 | `RegFilePlugin::kNumRegs=32` 和 `RD_IDX` payload 是 `uint_t<5>` | `reg_file.h:69`, `payload_common.h:139` | 模板化 `N_REGS` (已在 D.2 覆盖) |
| 4 | `payload_common.h` 的 `XLEN == 32 || 64` 静态断言 | `payload_common.h:114-118` | 推迟到跨 ISA 工作开始时 (E.7) |
| 5 | `CtrlLink::halt_when` 语义升级 | `ctrl_link.h:24-96` | 不改 `CtrlLink` (E.3) |
| 6 | `RiscvDecodePlugin` 的特定 decode 逻辑 | `decode.h:56-88` | 不需要改 (E.4) |

---

## 2. Phase 1 必须锁定的 4 项决策 (D.1-D.4)

> **Oracle 评审**: D.1-D.4 是"sound and should ship"。总成本 ~50 行 header churn，0 行行为改变，防止 ~2000 行 Phase 5+ 重构。

### D.1 添加 `Uid` 和 `ThreadId` 作为 Payloads (无框架改变)

**添加到 `payload_common.h`**:
```cpp
static inline cf::plugin::Payload<cf::plugin::uint_t<8>>   UID{"cpu.uid"};          // 0..255, ROB index
static inline cf::plugin::Payload<cf::plugin::uint_t<2>>   THREAD_ID{"cpu.tid"};   // 0..3
static inline cf::plugin::Payload<T>                       IID_PC{"cpu.iid_pc"};   // PC tagged to IID
```

**成本**: 1 个文件 3 行。**没有插件**需要被触及。*想要*使用它们的插件这样做。在 Phase 1,`UID` 到处是 `0`; 它在 Phase 5 变得有意义。

**为什么现在**: 读 `PC` 的 11 个 `at_stage` 回调 (例如 `branch_predictor.h:99, 115`, `int_alu.h:54-55`, `reg_file.h:127`) 可以*增量*迁移到读 `IID_PC`。第二条指令在一个 stage 的那一刻 (这是 superscalar 工作的第一天),`PC` 变得模糊,迁移被强制 — 此时迁移是**时间压力下的 11 个文件编辑**而不是悠闲的 Phase 1 清理。

### D.2 添加 `thread_id` 参数到 stateful 插件 (模板,不是 API)

**改变 `RegFilePlugin<T>` → `RegFilePlugin<T, N_REGS=32, N_THREADS=1>`** (`reg_file.h:62`)。
**改变 `HazardPlugin<T>` → `HazardPlugin<T, N_REGS=32, N_THREADS=1>`** (`hazard.h:55`)。
**改变 `BranchPredictorPlugin<T>` → `BranchPredictorPlugin<T, BTB_SIZE=16, …, N_THREADS=1>`** (`branch_predictor.h:55`)。

存储变为:
- `RegFilePlugin::regs_` = `std::array<array_store<T, N_REGS>, N_THREADS>`
- `HazardPlugin::scoreboard_` = `std::array<std::array<bool, N_REGS>, N_THREADS>`
- `BranchPredictorPlugin::global_history_` = `std::array<uint8_t, N_THREADS> global_history_{}`

**成本**: ~30 行 per plugin,header-only。默认 `N_THREADS = 1` 保持当前 ABI。

**为什么现在**: 这**不是** Phase 1 的行为改变。它是模板参数,在 Phase 5 之前是 `1`。但一旦 Phase 5 想要 SMT,这是 "添加新插件" 和 "重写现有插件并破坏每个使用它的测试" 的区别。

### D.3 `HazardPlugin::has_hazard` 返回 enum,不是 bool

**改变 `bool has_hazard(const DecodePayload&)` → `HazardKind has_hazard(const DecodePayload&, uint8_t tid)`** (`hazard.h:86-91`)。

```cpp
enum class HazardKind : uint8_t { NONE = 0, RAW_RS1, RAW_RS2, WAW };
HazardKind has_hazard(const DecodePayload& dec, uint8_t tid) const;
```

**成本**: ~5 行,函数体不变。`build()` at `hazard.h:116-150` 更新为线程化 `tid` 通过 (tid 来自 `n->(KeyType::THREAD_ID)`,在 Phase 1 是 `0`)。

**为什么现在**: 改变 `has_hazard` 的返回类型从 `bool` 到 `enum` 是**API break** 触及每个调用者。唯一 in-tree 调用者是 `hazard.h:126` 自身 (`build()` 回调)。测试套件有 0 个外部调用者 (测试直接用 `has_raw`/`has_waw`,不是 `has_hazard`)。所以成本是**接近零**。收益: 在 Phase 5,OoO 用返回*哪个*操作数 pending 的 CAM lookup 替换位掩码检查 — enum 让新代码与旧代码在单一接口中共存。

### D.4 `BranchPredictorPlugin::predict` 和 `update` 接受 `tid`

**改变** (`:129-143` 和 `:155-168`):
```cpp
T predict(T pc, uint8_t tid) const;
void update(T pc, bool taken, T target, uint8_t tid);
```

**成本**: 琐碎。`tid = 0` 在 Phase 1。`global_history_` 变为 `std::array<uint8_t, N_THREADS> global_history_{}` 并按 `tid` 索引。

**为什么现在**: 整个代码库中最便宜的前瞻兼容胜利。10 行重构。单线程模式无行为改变。

### D.1-D.4 总结

| Lock | Phase-1 成本 | Phase-5 成本 (如果不锁定) | 严重性 (如果不锁定) |
|------|-------------|----------------------------|-------------------------|
| D.1: `UID`/`THREAD_ID`/`IID_PC` Payloads | 3 行,header | 触及 11 个 `at_stage` 回调 | Medium |
| D.2: 模板化插件为 `N_THREADS` | ~90 行 (3 plugins × 30) | 重写 3 个插件 + 每个测试 | **High** |
| D.3: `HazardKind` enum | 5 行 | API break at every caller site | Medium |
| D.4: `tid` in `predict`/`update` | 10 行 | 重写 BP 两次 (单线程 → 多线程) | **High** (最便宜锁定) |

**总 Phase-1 工作**: ~108 行 header churn,0 行行为改变。**防止**: ~2000 行 Phase 5+ 重构。

---

## 3. 可以推迟到 Phase 5+ 的 8 项决策 (E.1-E.8)

> 本节是 [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md) §E 的提炼。

**以下项目**不是 Phase 1 工作。它们是 Phase 5+ 工作,框架已经支持 (或需要设计新插件)。**不要在 Phase 1 触及它们**。

### E.1 ROB, PRF, IQ, LSQ, Rename

这些是**新插件** (`CommitPlugin`, `RenamePlugin`, `IssueQueuePlugin`, `LSQPlugin`)。它们不是要重构的抽象; 它们是新代码。框架的 `at_stage` + `commit_hook` 已经支持它们。

**不要**在 Phase 1 开始设计它们。风险是基于理论 OoO 需求的过度工程。等到有真正的工作负载要求 OoO。

### E.2 Per-instruction Payload 存储 ("Uid-keyed PayloadStore")

**不要**重构 `PayloadStore` 按 IID 键控。OoO 模型是: 每个 PipeNode 持有最多一条指令 (当前 "stage" 视图)。多个飞行中指令持有**在 IQ / ROB / LSQ**,不在 `PayloadStore`。"Uid-keyed store" 诱惑是**错的**: 它把 ROB 复制到 Payload 系统。

正确的 OoO 模型是:
- `PipeNode` = **一条飞行中指令** (就像今天)
- `ROB` = 一个插件成员,按 IID 索引,持有 `{arch_dst, prf_dst, done_bit, exception}`
- `IQ` = 一个插件成员,按 IID 索引,持有 `{op_class, prf_src1, prf_src2, ready1, ready2}`
- `PRF` = 一个插件成员,大 `array_store<T, 4096>` (可配置),按物理 reg ID 索引

Payload 系统**保持不变**。这是使当前架构 OoO-friendly 的设计点。

### E.3 `CtrlLink::halt_when` 语义升级

当前 `CtrlLink::halt_when` 接受 `std::function<bool()>` 并 OR-合并所有 halt 条件。对 OoO 反压,halt 条件必须引用**特定 IID 的 IQ 条目**,不是全局状态。插件级代码可以用任何它想要的范围计算 halt 条件; 框架的 `bool should_halt()` 是好的。

**不要**改变 `CtrlLink`。按原样使用它。

### E.4 `RiscvDecodePlugin` 的特定 decode 逻辑

`decode.h:56-88` 是 RISC-V-specific,不会被 OoO 触及。OoO `RenamePlugin` 将在 `RiscvDecodePlugin` 写 `DECODE` Payload 后读它,然后分配 PRF 条目。decode 插件的工作 (把指令字变成 `DECODE` + `RISCV_DETAIL`) 不变。

### E.5 `RegFilePlugin::writeback` 直接写 `regs_`

对 in-order,这是正确的。对 OoO,**writeback 回调变为 PRF 写**,新的 **CommitPlugin** 在 commit 时做架构寄存器文件写。现有 `RegFilePlugin::writeback` 是 Phase 5+ 重构,不是 Phase 1 重构。这也是 OoO 移植**必须**在这个插件改变的唯一事物 — 使 RegFile 成为 Phase 5+ **确实**被重设计的一个插件。

**为什么这 OK**: RegFile 是唯一插件其 writeback *语义* 改变 (PRF vs arch)。Hazard, BP, IBus, DBus, RegFile read 都保持语义相同 (它们读架构状态)。Phase 5 重写**范围限定到 RegFile 的 writeback 回调** + 新 CommitPlugin。

### E.6 空的 `cpu_factory.h` stub

`cpu_factory.h:73-127` 是空 stub。M4-DSE 将填充它 (per v1.0 §7)。OoO 添加发生在**同一工厂文件**,当 Phase 5 实例化 `IssueQueuePlugin`, `RenamePlugin`, `CommitPlugin` (除了现有 11 个)。不要在 Phase 1 为 OoO 预设计工厂。

### E.7 `payload_common.h` `XLEN == 32 || 64` static_assert

`payload_common.h:114-118` 固化 RISC-V 的 XLEN。这对 Phase 1 **是好的** (RISC-V 是唯一目标)。Phase 5 OoO 模型是**per-ISA**,不是 cross-ISA。Cross-ISA 是 v1.0 §2.3 推迟,不是 OoO。保留它。

### E.8 `array_store` Phase 6 double-buffer

`include/cf/plugin/storage.h:69-137` 文档化 `array_store` 在 Phase 1 是 single-buffered,将在 Phase 6 变为 double-buffered 用于 `ch_mem` parity。这是 RTL-only。它不影响 TLM OoO 建模 (每个 `pb.run()` 是一个周期,显式 `commit()` 是 OoO commit point)。**不要**在 Phase 1 重构 `array_store`。

---

## 4. 实施路线图

### Phase G — Forward-Compatibility Locks (M4-DSE 子阶段, 2 天)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| G.1 | D.1: 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads | `ip/cpu/core/payload_common.h` | 3 行,编译通过 |
| G.2 | D.2: 模板化 `RegFilePlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/reg_file.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.3 | D.2: 模板化 `HazardPlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/hazard.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.4 | D.2: 模板化 `BranchPredictorPlugin<T, ..., N_THREADS>` | `ip/cpu/plugins/branch_predictor.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.5 | D.3: `HazardPlugin::has_hazard` 返回 `HazardKind` enum | `ip/cpu/plugins/hazard.h` | API break,0 个外部调用者 |
| G.6 | D.4: `BranchPredictorPlugin::predict`/`update` 接受 `tid` | `ip/cpu/plugins/branch_predictor.h` | `tid=0` 在 Phase 1 |
| G.7 | 测试: D.1-D.4 所有锁的单元测试 | `tests/cpu/test_forward_compat.cpp` (新建) | 8+ 个用例 PASS |
| G.8 | 文档: 更新 `blueprint.md` / `status.md` / `README.md` | `ip/cpu/docs/` | 同步更新 |

**总工作量**: 2 天（~108 行 header churn + 测试）。

---

## 5. 风险与限制

### 5.1 D.2 模板化 `N_THREADS` 可能增加编译时间

- **缓解**: 默认 `N_THREADS=1` 只实例化一次。用户想要 SMT 时,显式实例化 `N_THREADS=2/4/8`。
- **成本**: ~30 行 per plugin,header-only。编译时间增加 ~10%。

### 5.2 D.3 `HazardKind` enum 是 API break

- **缓解**: 唯一 in-tree 调用者是 `hazard.h:126` 自身。测试套件有 0 个外部调用者。
- **成本**: ~5 行,函数体不变。

### 5.3 `array_store` Phase 6 double-buffer 假设

- **问题**: 如果 `RegFilePlugin` 从 `array_store<T, 32>` 迁移到 `std::array<array_store<T, 32>, N_THREADS>`,per-thread 双缓冲合约需要显式处理。
- **缓解**: 当前提案不涉及 `array_store` 改变。Phase 6 实施 SMT 时,需要重新评估双缓冲策略。

### 5.4 总体风险

- **v2.0-locks 可能过度保守**: 仅锁定 D.1-D.4 可能遗漏其他前瞻兼容性问题。
  - **缓解**: 完整的设计研究保留在 `dse_architecture_v2_design_research.md`，Phase 5 准备时可以重新评估。
  - **决策**: 优先 ship D.1-D.4，不在 Phase 1 添加投机性代码。

---

## 6. 附录: 与设计研究文档的关系

### 6.1 文档拆分原因

`dse_architecture_v2_design_research.md` 包含完整的设计研究（~1400 行），覆盖：
- 5 个调研 agent 的综合发现（gem5、BOOM、XiangShan、Chipyard、Intel HT、AMD Zen、IBM POWER）
- 多架构 DSE 框架设计（§5）
- 分支预测器工厂模式（§6）
- SMT 接口设计方案（§7）
- DSE 方法论升级（§8）

**Oracle 评审结论**: 这些内容是高质量的**设计研究**，但被错误地定位为 Phase 1 交付物。实际上它们是**伪装成 Phase 1 设计的 Phase 5+ 实现**。

### 6.2 拆分策略

| 文档 | 内容 | 实施时机 |
|------|------|---------|
| **dse_architecture_v2_locks.md** (本文档) | D.1-D.4 + E.1-E.8 | **Phase 1 (M4-DSE)**，2 天 |
| **dse_architecture_v2_design_research.md** | §1-§4 (gap analysis) + §5-§8 (设计研究) | **Phase 5 准备**，重新评估后实施 |

### 6.3 从设计研究文档删除的内容

以下内容从 Phase 1 实施范围中移除（保留在设计研究文档中供 Phase 5 参考）：

| 原始章节 | 内容 | Oracle 评审结论 | 处理 |
|---------|------|---------------|------|
| D.5 | stage-name 成员 | 错误抽象（superscalar lanes 不是 stage 名称） | **删除** |
| D.6 | 文档化 `at_stage` + `commit_hook` | 0 行代码，可以推迟 | 推迟到 Phase 5 |
| D.7 | `setup_with_config` | 可选，可以推迟 | 推迟到 Phase 5 |
| D.8 | `ThreadContext<T>` 结构 | 投机性死代码（无消费者） | **删除** |
| D.9 | `Cpu<T, MAX_THREADS>` 类 | 投机性死代码（无消费者） | **删除** |
| §5 | 多架构 DSE 框架设计 | Phase 5+ 实现 | 推迟到 Phase 5 |
| §6 | 分支预测器工厂模式 | 破坏插件模型一致性 | **删除**（用 D.2 模板化替代） |
| §7 | SMT 接口设计方案 | Phase 6+ 实现 | 推迟到 Phase 6 |
| §8 | DSE 方法论升级 | 隐藏的 C++ 耦合风险 | 推迟到 Phase H/I |

### 6.4 参考文档

- **Gap 分析**: [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md) (615 行)
- **gem5 参考**: [`gem5_dse_reference.md`](gem5_dse_reference.md) (1209 行)
- **开源 RISC-V 调研**: [`../../docs/research/dse-open-source-riscv-survey.md`](../../docs/research/dse-open-source-riscv-survey.md) (~480 行)
- **SMT 接口调研**: [`research/smt-interface-design-survey.md`](research/smt-interface-design-survey.md) (~2500 词)
- **DSE 框架调研**: [`research/dse-framework-survey.md`](research/dse-framework-survey.md) (~3000 词)

---

*文档结束。如需修改，走 v2.0 拆分维护约定 (见 [cpu_implementation_guide_v2.0.md §3.5](cpu_implementation_guide_v2.0.md#35-后续维护约定))。*

*Oracle 评审记录: 2026-06-17，bg_df09c224，ses_12b79fdb9ffe8ZzxHifTroJecD*
