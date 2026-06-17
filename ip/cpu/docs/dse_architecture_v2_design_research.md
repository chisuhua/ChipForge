# CPU 架构 DSE 实现方案 v2.0 (Design Space Exploration)

| 字段 | 值 |
|------|-----|
| 版本号 | 2.0 |
| 日期 | 2026-06-17 |
| 状态 | 🟡 Draft (基于 5 个调研 agent 的综合改进) |
| 适用范围 | ChipForge IP/CPU 子系统 |
| 父文档 | [`dse_architecture.md`](dse_architecture.md) v1.0, [`multi_isa_architecture.md`](multi_isa_architecture.md) v2.0 |
| 参考文档 | [`gem5_dse_reference.md`](gem5_dse_reference.md), [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md), [`../../docs/research/dse-open-source-riscv-survey.md`](../../docs/research/dse-open-source-riscv-survey.md) |

> **本文档定位**: 在 v1.0 的基础上,**基于 5 个并行调研 agent 的综合发现** 提出架构改进建议。
>
> v1.0 的核心价值:
> - 实证校核了当前真实状态 (哪些是真 stub / 哪些是文档超前)
> - 给出了 CpuFactory 从空壳到真实实现的完整路径
> - 激活了 5 个 ✅ 可调维度 + 4 个 ⚠️ 死字段
>
> **v2.0 的核心增量**:
> 1. **识别 Phase 1 必须锁定的 7 项决策** (避免 Phase 5+ 重写 2000+ 行代码)
> 2. **提出多架构 DSE 框架** (参考 BOOM/XiangShan/Chipyard 的参数化模式)
> 3. **设计分支预测器工厂模式** (参考 gem5 + BOOM 的 SimObject 组合)
> 4. **给出 SMT 接口设计方案** (参考 Intel HT / AMD Zen / IBM POWER 的资源共享策略)
> 5. **升级 DSE 方法论** (参考 ESESC/Sniper/OpenDSE 的 Pareto 前沿计算方法)
>
> **本文件不修改** v1.0 的实施路线图 (Phase A-F), 仅新增 Phase G (Forward-Compatibility Locks)。

---

## 目录

1. [调研总结: 5 个 agent 的关键发现](#1-调研总结-5-个-agent-的关键发现)
2. [当前架构的 2 个硬墙 + 6 个中等障碍](#2-当前架构的-2-个硬墙--6-个中等障碍)
3. [Phase 1 必须锁定的 7 项决策 (D.1-D.7)](#3-phase-1-必须锁定的-7-项决策-d1-d7)
4. [可以推迟到 Phase 5+ 的 8 项决策 (E.1-E.8)](#4-可以推迟到-phase-5-的-8-项决策-e1-e8)
5. [多架构 DSE 框架设计](#5-多架构-dse-框架设计)
6. [分支预测器工厂模式](#6-分支预测器工厂模式)
7. [SMT 接口设计方案](#7-smt-接口设计方案)
8. [DSE 方法论升级](#8-dse-方法论升级)
9. [实施路线图更新 (Phase G)](#9-实施路线图更新-phase-g)
10. [风险与限制](#10-风险与限制)
11. [附录: 与 v1.0 的差异对照](#11-附录-与-v10-的差异对照)

---

## 1. 调研总结: 5 个 agent 的关键发现

### 1.1 gem5 OoO 架构调研

**调研范围**: `/workspace/project/gem5/src/cpu/o3/` (28,505 行 C++)

**关键发现**:

1. **gem5 不用 plugin 模式**。OoO CPU 是一个 1,500 行的 `gem5::o3::CPU` 类,硬编码组合 7 个子阶段 (BAC/Fetch/Decode/Rename/IEW/Commit)。ChipForge 的 plugin 模式比 gem5 更激进。

2. **参数化非常彻底**。`BaseO3CPU.py` 有 60+ 个可调参数:
   - 6 个 width: `fetchWidth` / `decodeWidth` / `renameWidth` / `dispatchWidth` / `issueWidth` / `commitWidth`
   - 队列大小: `LQEntries` / `SQEntries` / `numROBEntries`
   - 物理寄存器: `numPhysIntRegs=256` / `numPhysFloatRegs=256`
   - SMT 策略: `smtFetchPolicy` / `smtLSQPolicy` / `smtROBPolicy` (Dynamic/Partitioned/Threshold)

3. **分支预测器是 SimObject 组合**。`bpred_unit.cc` 包装了 13+ 种子预测器 (TournamentBP/GShareBP/BiModeBP/TAGE/LTAGE 等),通过 `BranchPredictor.conditionalBranchPred = TournamentBP(...)` 选择。

4. **SMT 实现**。`iew.cc:1448-1453` 的 SMT dispatch 循环按 `smtFetchPolicy` 从多个线程取指令。ROB/IQ/LSQ 都按 `SMTQueuePolicy` 分配 (Partitioned/Dynamic/Threshold)。

**对 ChipForge 的启示**:
- ChipForge 的 plugin 模式是**优势** (gem5 做不到的灵活性),但需要设计自己的 "OoO shell plugin family" (RobPlugin/IqPlugin/LsqPlugin/RenamePlugin)
- 参数化模式可以直接借鉴: `CPUConfig` 扩展 60+ 字段,对应 gem5 的参数集
- 分支预测器工厂模式参考 gem5 的 SimObject 组合

### 1.2 开源超标量 CPU 调研

**调研范围**: BOOM / XiangShan / Chipyard / Rocket-Chip / NaxRiscv / VexRiscv / Hummingbirdv2

**关键发现**:

1. **BOOM v3 参数化** (`BoomCoreParams`):
   - Frontend: `fetchWidth=8` / `decodeWidth=3` / `maxBrCount=16`
   - IQ: `issueParams: Seq[IssueParams]` (IQT_MEM/IQT_INT/IQT_FP 各自独立)
   - ROB: `numRobEntries=96` / `numIntPhysRegisters=100`
   - LSQ: `numLdqEntries=24` / `numStqEntries=24`
   - 分支预测: `branchPredictor: Function2[BankResponse, Parameters, ...]` — **函数本身是参数**

2. **XiangShan (昆明湖)**:
   - 6-wide superscalar, 224 ROB, 96 IQ entries
   - 参数化通过 Chisel `Config` 系统,但公开文档较少

3. **Chipyard Configs 系统**:
   - `WithN{Small|Medium|Large}BoomCores(n)` 组合器
   - 异构 SoC: `DualLargeBoomAndSingleRocketConfig`
   - **承认问题**: "a significant challenge ... is being able to identify the correct parameter to use"

4. **NaxRiscv (Scala)**:
   - Stageable/Service 框架: 每个 stage 暴露 `Service[T]`,其他 stage 通过 `service.get[T]()` 访问
   - 比 VexRiscv 更解耦

**对 ChipForge 的启示**:
- BOOM 的 `branchPredictor: Function2[...]` 参数是最优雅的设计 — ChipForge 应该设计 `branch_predictor_factory: std::function<std::unique_ptr<BranchPredictorBase>(CPUConfig)>`
- Chipyard 的异构 SoC 组合器值得借鉴: `WithCpuConfig<CPUConfig>` 模板
- NaxRiscv 的 Service 模式可以未来引入 (Phase 5+)

### 1.3 SMT 接口设计调研

**调研范围**: Intel HT / AMD Zen 1-5 / IBM POWER5-10 / SPARC T4-T7 / SiFive U74/U84

**关键发现**:

1. **SMT 资源共享策略** (4 种经典模式):
   - **Static partitioning**: 每个线程固定 N/M (gem5 `Partitioned` 策略)
   - **Dynamic partitioning**: 按需分配 (Vantage, Stanford ISCA'11)
   - **Competitive sharing**: 完全共享,先到先得 (AMD Zen 1/2)
   - **Watermarked sharing**: 每个线程保底 + 超额共享 (AMD Zen 3/4/5,防御 SQUIP 攻击)

2. **ICache 通常完全共享** (Intel P4, AMD Zen, POWER, SPARC T5)。原因: 两线程执行相同 ISA,代码布局相似。

3. **分支预测器两种模式**:
   - **共享预测器, TID-tagged history**: Intel Netburst (P4) — BTB 有 `tid` 字段
   - **Per-thread predictor state**: POWER/AMD Zen — 每个线程独立 PHT/RAS/BTB

4. **ROB 三种模式**:
   - **Per-thread ROB**: POWER4 (N 个独立小 ROB)
   - **Unified ROB with TID tag**: Intel/AMD Zen (单大 ROB, 每条目有 `tid`)
   - **Tournament-style retirement**: Tullsen 1995 (跨线程最老优先)

5. **RISC-V H-Extension 不是 SMT**。H-Extension 是 Hypervisor (虚拟化),不是硬件多线程。RISC-V 目前没有标准 SMT 扩展。

**对 ChipForge 的启示**:
- SMT 接口设计的核心是 **ThreadContext 作为一等公民**,与 Stage (功能单元) 分离
- Payload 必须携带 `tid: uint8_t` (1-2 bits 足够 SMT2/SMT4)
- 分支预测器工厂必须支持 per-thread predictor (POWER/AMD Zen 模式)

### 1.4 当前架构 OoO 演进障碍分析

**调研范围**: 本地代码审计 + v1.0 文档分析

**关键发现** (详见 [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md)):

1. **VexRiscv 声明式 plugin 模型是 OoO-friendly by accident**:
   - 框架脊柱 (PipeNode/PipeBuilder/PluginBase/PayloadStore/CtrlLink/PipeArbitration) **不需要改**
   - `commit_storages` hook **已经是** OoO commit 原语
   - `flush_when` **已经是** mispredict squash 原语
   - D4 "no tick() in plugin" 规则 **已经是** OoO 兼容 (OoO 逻辑是 `pb.run()` 驱动的状态机)

2. **2 个硬墙** (Phase 5+ 必须重写,Phase 1 无法避免):
   - **硬墙 1**: `PayloadStore` 按类型键控,不按 IID (Instruction ID) 键控。superscalar 时多个指令同时在一个 stage,`n->(KeyType::RS1)` 会被后写覆盖。
   - **硬墙 2**: `RegFilePlugin::regs_` 是单一全局数组,无 per-thread 隔离。SMT 需要 N 个架构寄存器文件,OoO 需要物理寄存器文件 (PRF) 在架构寄存器文件之上。

3. **6 个中等障碍** (Phase 1 可以小成本避免 Phase 5+ 重写):
   - `HazardPlugin::scoreboard_` 假设一个架构寄存器只有一个飞行中写入者 (OoO 允许两个写入者,只有一个是真正的)
   - `BranchPredictorPlugin::global_history_` 是单一 uint8_t (SMT 需要 per-thread GHR,OoO 需要 checkpoint-and-restore on mispredict)
   - `RegFilePlugin::kNumRegs=32` 和 `RD_IDX` payload 是 `uint_t<5>` (跨 ISA 时需要重构)
   - `payload_common.h` 的 `XLEN == 32 || 64` 静态断言 (跨 ISA 时需要放宽)

4. **7 个 Phase 1 必须锁定的决策** (D.1-D.7, 总成本 ~150 行代码,防止 Phase 5+ 重写 ~2000 行):
   - D.1: 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads
   - D.2: 模板化 stateful plugins 为 `N_THREADS`
   - D.3: `HazardPlugin::has_hazard` 返回 enum,不是 bool
   - D.4: `BranchPredictorPlugin::predict`/`update` 接受 `tid`
   - D.5: 插件 stage-name 成员变量,不是字符串字面量
   - D.6: 文档化 `at_stage` + `commit_hook` + `flush_when` 作为 OoO 原语
   - D.7: 采纳 `setup_with_config` (v1.0 §6.3 提案)

**核心洞察**: **框架脊柱是对的,插件实现是工作所在**。Phase 1 的 ~150 行代码可以避免 Phase 5+ 的 ~2000 行重构。

### 1.5 DSE 框架调研

**调研范围**: 学术 (Mishra/EXPRESSION, OpenDSE, ESESC, Graphite, Sniper) / 工业 (Synopsys ARC, Cadence Tensilica, Imperas, SiFive) / Chisel (Chipyard, Rocket-Chip, BOOM)

**关键发现**:

1. **学术 DSE 工具**:
   - **Mishra/EXPRESSION** (UC-Irvine): ADL + retargetable compiler/simulator, orders-of-magnitude 加速 architecture-to-RTL
   - **OpenDSE** (TU Dresden): 开源 Java Y-chart DSE, MIT 协议
   - **ESESC** (UCSC): 快速多核仿真器 + McPAT power, RISC-V 支持
   - **Sniper** (Ghent/Intel): interval core model, ~25% accuracy vs real hardware, CPI stacks

2. **工业 EDA**:
   - **SiFive Core Designer**: "menu-of-derivatives" 模型,每个 derivative 是 pre-verified RTL
   - **Cadence Tensilica Xtensa**: 第一个 configurable processor, Designer-defined Queues/Ports/Lookups
   - **Synopsys ARC**: ARChitect wizard drag-and-drop, APEX user-defined instructions

3. **Chisel-based DSE**:
   - **Chipyard Configs**: additive config fragments (`With<Name>`), composition order matters (right-to-left)
   - **Rocket-Chip**: `case object` Fields, `Knob("...")` 参数, `WithNBigCores(n)` 组合器
   - **BOOM**: `branchPredictor: Function2[...]` — 函数本身是参数

4. **DSE 方法论**:
   - **Evaluation**: cycle-accurate (慢) / statistical sampling (SMARTS, 快) / analytical models (Sniper interval, 快) / predictive/surrogate (ANN, 快)
   - **Predictive modeling** 是最有杠杆的技术 (Ipek 2006, Lee & Brooks 2007, Dubach 2009, OneDSE 2025): 训练 surrogate model 代替 full simulation, 节省 3-4 orders of magnitude
   - **Pareto-frontier**: NSGA-II (de-facto standard) / weighted-sum / ε-constraint
   - **10 条经验教训**:
     1. 组合爆炸不是 scale 问题,是 dimension 问题
     2. per-point evaluation time 是瓶颈
     3. workload coverage 隐藏在 metric 中
     4. parameter interaction 打破 one-at-a-time sensitivity
     5. evaluation fidelity 不是 monotone
     6. discrete/combinatorial + continuous mixed variables 打破 standard continuous optimizers
     7. symmetry is common and underexploited (mpsym: 8.6× speed-up, 30× better SA results)
     8. combinatorial search can be trapped by infeasible regions
     9. the "right" tool depends on the question
     10. abstraction crossing is where papers die

**对 ChipForge 的启示**:
- **Predictive modeling** 是最有杠杆的技术 — ChipForge DSE v2 必须包含 surrogate modeling tier
- **BOOM 的 `branchPredictor: Function2[...]`** 是最优雅的分支预测器工厂设计
- **Chipyard 的异构 SoC 组合器** 值得借鉴: `WithCpuConfig<CPUConfig>` 模板
- **NSGA-II** 是 de-facto standard Pareto 算法 — ChipForge 应该用

---

## 2. 当前架构的 2 个硬墙 + 6 个中等障碍

> 本节提炼 [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md) 的核心结论。

### 2.1 框架脊柱 (不需要改)

三个类构成框架的**不变脊柱**,OoO/Superscalar/SMT 不需要拆解:

- **`cf::plugin::PipeBuilder`** (`include/cf/plugin/pipe_builder.h:54-172`): 拥有 plugins, stages, nodes, commit hooks
- **`cf::plugin::PluginBase`** (`include/cf/plugin/plugin_base.h:48-72`): 声明 `setup()` / `build()`; `tick()` 是 `= delete` (D4 强制执行)
- **`cf::plugin::PipeNode`** (`include/cf/plugin/pipe_node.h:33-151`): 持有 `PayloadStore` + 5 态状态机 + `PipeArbitration arb_`

框架已经提供两个非显然的 OoO hooks **免费**:
- **Commit hooks** (`pipe_builder.h:128-138`): `pb.run()` 以 `commit_storages()` 结束 — 框架**已经有** end-of-cycle commit 的概念
- **CtrlLink** (`include/cf/plugin/ctrl_link.h:24-96`): `flush_when` 是 OoO 用于 branch-mispredict recovery 的原语

### 2.2 CPU Payload 布局 (11 个 Keys,大部分是 scalar)

`ip/cpu/core/payload_common.h:113-156` 声明 11 个 Keys:

| # | Key | Type | Stage producer | Stage consumer |
|---|-----|------|----------------|----------------|
| 1 | `PC` | `T` (xlen) | fetch | all |
| 2 | `INSTRUCTION` | `uint_t<32>` | IBus (fetch) | RiscvDecode (decode) |
| 3 | `RS1` | `T` | RegFile (decode) | RiscvIntAlu / Mul (execute) |
| 4 | `RS2` | `T` | RegFile (decode) | RiscvIntAlu / Mul (execute) |
| 5 | `RD_DATA` | `T` | EX stage | RegFile (writeback) |
| 6 | `RD_IDX` | `uint_t<5>` | decode | writeback |
| 7 | `DECODE` | `DecodePayload` struct | RiscvDecode | Hazard, RegFile, BP, DBus |
| 8 | `RESULT` | `T` | EX | writeback |
| 9 | `MEM_ADDR` | `T` | LSU | DBus |
| 10 | `MEM_DATA` | `T` | LSU / DBus | LSU / DBus |
| 11 | `MEM_SIZE` | `uint_t<3>` | LSU | DBus |

Plus RISC-V-ISA-specific: `RISCV_DETAIL` (`arch/riscv/payload_riscv.h:74`), `BRANCH_TARGET` (`:77`)。

**关键缺失**: 没有 instruction Uid,没有 PC-tagged instruction copy,没有 ready bit,没有 wakeup event,没有 commit-time architectural state。这是 "single instruction in flight" 假设的具体化。

### 2.3 插件状态 — per-Plugin,不是 per-instruction

四个 "stateful" 插件把状态存为 `PluginBase` **成员字段**,不是 PipeNode 上的 payloads:

| Plugin | State | Location | Lines |
|--------|-------|----------|-------|
| `RegFilePlugin<T>` | `array_store<T, 32> regs_` (架构寄存器文件) | `reg_file.h:158-161` | 158-161 |
| `HazardPlugin<T>` | `std::array<bool, 32> scoreboard_` (飞行中 rd mask) | `hazard.h:152-153` | 152-153 |
| `BranchPredictorPlugin<T>` | `btb_[16]` + `bimodal_[16]` + `gshare_[16]` + `global_history_` (8-bit) | `branch_predictor.h:206-219` | 206-219 |
| `IBusPlugin<T>` / `DBusPlugin<T>` | 一个 `next_instruction_` 字段 | `ibus.h:63` | 63 |

**这是最大的前瞻兼容性问题**。每个 stateful 插件都是**单例状态,一个全局 scoreboard,一个全局 GHR,一个全局寄存器文件**。物理上不可能运行两个线程,或两个飞行中指令,通过这些状态而无需重新架构。

### 2.4 `at_stage` 执行模型 (per-tick, single-instruction semantics)

`PipeBuilder::run()` (`pipe_builder.h:99-102`) 按注册顺序执行所有 `at_stage` 回调,然后调用 `commit_storages()`。每个 `at_stage` 回调读一个 PipeNode,写回它。

每个具体插件的 `build()` (例如 `int_alu.h:47-65`, `decode.h:56-88`, `reg_file.h:119-153`, `branch_predictor.h:91-120`) 遵循相同模式:

```cpp
pb.at_stage("execute", Phase::NORMAL, [&pb]() {
  auto* n = pb.node_of_logic_stage("execute").get();   // ← one node, one instruction
  if (!n) return;
  // read n->(RS1), n->(RS2), n->(DECODE)
  // write n->(RESULT)
});
```

**每个回调假设的语义**: "当这个 stage 触发时,这个 stage 的 PipeNode 恰好持有一条指令的数据。" 即,**一个 stage = 一次一条指令,一个 `pb.run()` = 一个时钟周期,一条指令前进一个 stage**。

### 2.5 2 个硬墙 (Phase 5+ 必须重写)

#### 硬墙 1: `PayloadStore` 按类型键控,不按 IID 键控

**位置**: `include/cf/plugin/payload.h:93-154` (`PayloadStore`); `include/cf/plugin/pipe_node.h:81-104` (`PipeNode::operator()`)。

**问题**: `PipeNode` 持有一个 `PayloadStore`,每个 Key 恰好一个值。store 没有 "这是 Uid 7 vs Uid 8 的值" 的概念。当 OoO 有 10 个飞行中指令都在 `EX` 等待不同操作数时,EX stage 必须持有 10 份 `RS1`, `RS2`, `DECODE`, `RESULT`。当前 `PayloadStore` 做不到。

**为什么是硬墙,不是中等**: 给 `PayloadStore` 添加 `Uid` 字段需要:
- 每个 Key get/put 接受 `Uid`
- 每个 `at_stage` 回调知道它处理哪个 Uid
- `n->operator()(KeyType::RS1)` 语法糖 (在 11 个地方使用) 增长 Uid 参数

这是 Payload 访问模式的**完整 API 重写**。

**C.1 缓解 (Phase 1 推荐)**: 不改框架。只添加 `Uid` 作为 Payload (key type `uint_t<ROB_BITS>`),要求插件自己线程化。这是每个生产 OoO 的做法 (Uid 是数据,不是元数据)。

#### 硬墙 2: `RegFilePlugin::regs_` 是单一全局数组,无 per-thread 隔离

**位置**: `ip/cpu/plugins/reg_file.h:158-161`。

**问题**: `RegFilePlugin<T>::regs_` 是单一 `array_store<T, 32>`。SMT 需要 N 个架构寄存器文件 (每线程一个)。OoO 需要*物理*寄存器文件 — `regs_` 将成为*已提交*的架构文件,在 `RenamePlugin` (尚不存在) 内有单独的 PRF。

`kNumRegs = 32` `static constexpr` (`:69`) 进一步固化 RISC-V 的 5-bit 编码寄存器索引。ARM 有 16/32,x86-64 有 16 个重叠命名空间,MIPS 有 32。`RD_IDX` payload 是 `uint_t<5>` (`payload_common.h:139`) — 只对 5-bit ISA 寄存器字段有效。跨 ISA-不同寄存器数 = 类型重设计。

**为什么是硬墙**: `RegFile` 在**两个** `at_stage` 回调中触及 (decode read at `reg_file.h:124-139`, writeback write at `:142-152`)。拆分为 per-thread 需要每个回调是 per-thread-aware。

**C.2 缓解 (Phase 1 推荐)**: 模板化插件为 `<typename T, std::size_t N_REGS, std::size_t N_THREADS = 1>`。默认 `N_THREADS = 1` 保持当前行为。这花费 ~5 行模板参数和 1-D 索引计算; 不改 `at_stage` 回调结构。

### 2.6 6 个中等障碍 (Phase 1 可以小成本避免 Phase 5+ 重写)

#### 中等 1: `HazardPlugin::scoreboard_` 假设一个架构寄存器只有一个飞行中写入者

**位置**: `ip/cpu/plugins/hazard.h:152-153`。

**问题**: `std::array<bool, 32> scoreboard_` (`:153`) 是位掩码: `scoreboard_[i] = true` 意味着 "寄存器 i 正在被一条飞行中指令写入"。这对**一个飞行中写入者 per reg** 是正确的,这在 in-order 成立但**失败**在 OoO 允许:
- 两个写入者到同一架构寄存器 (只有一个将提交; 另一个被 squash — scoreboard 必须跟踪*推测性*写入者,不只是 "是否有一个在飞行中")
- 一个 load 在更早 store 到同一地址之前返回 (LSQ 在 OoO 处理这个; scoreboard 不知道 mem hazards)

对 SMT,scoreboard 必须是**per-thread** (跨线程写入*不是* hazards; 它们通过 OoO wakeup 或更大的 PRF 解决)。

**为什么是中等**: `has_hazard(DecodePayload)` API (`:86-91`) 接受单一 `DecodePayload` 并返回单一 bool。在 OoO,hazard 是 "这个 IID 的操作数 X 的生产者是否是同一架构寄存器的两个写入者中*更年轻*的那个" 的函数,这是一*组*条件,不是 bool。`has_hazard` 的语义改变。

**缓解**: 改变 `has_hazard` 返回 `enum class HazardKind { NONE, RAW_RS1, RAW_RS2, WAW }` 并添加 `thread_id` 到调用签名。仍然基于位掩码用于 in-order; per-thread `scoreboard_[N_THREADS][32]` 是 2-D 数组。~15 行 churn。

#### 中等 2: `BranchPredictorPlugin::global_history_` 是单一 8-bit,不是 per-thread

**位置**: `ip/cpu/plugins/branch_predictor.h:215` (`std::uint8_t global_history_ = 0;`)。

**问题**: GHR 是单一 uint8_t。在 SMT,两线程的分支交错,GHR 会跨线程 alias,灾难性降低预测。即使在 OoO (单线程),GHR 必须支持**推测性更新与 checkpoint-and-restore on mispredict**: 在*issue* (或 rename) 时,保存 `global_history_` 到 checkpoint buffer; 在 squash 时,恢复 head 的 checkpoint。

当前 `at_stage` 回调在**execute** 更新 GHR (`branch_predictor.h:110-119`)。对 OoO 这是错的 — GHR 必须在**rename** checkpoint 并在**commit** 更新 (或推测性,带 rollback)。

**缓解**: 模板化 `N_THREADS`,使 `global_history_` 为 `std::array<uint8_t, N_THREADS>`。添加 `checkpoint` / `restore` API。

#### 中等 3: `RegFilePlugin::kNumRegs=32` 和 `RD_IDX` payload 是 `uint_t<5>`

**位置**: `ip/cpu/plugins/reg_file.h:69` (`static constexpr std::size_t kNumRegs = 32;`); `payload_common.h:139` (`static inline cf::plugin::Payload<cf::plugin::uint_t<5>> RD_IDX{"cpu.rd_idx"};`)。

**问题**: RISC-V 有 32 个 GPR,5-bit 编码。ARM 有 16 个 (ARMv7) 或 32 个 (AArch64)。x86-64 有 16 个 (RAX-R15)。MIPS 有 32 个。跨 ISA 时,`kNumRegs` 和 `RD_IDX` 位宽需要改变。

**缓解**: 模板化 `N_REGS`。`RD_IDX` payload 位宽从 `uint_t<5>` 改为 `uint_t<MAX_ISA_REG_BITS>` (例如 7 bits 覆盖 128 regs)。

#### 中等 4: `payload_common.h` 的 `XLEN == 32 || 64` 静态断言

**位置**: `payload_common.h:114-118`。

**问题**: RISC-V 只支持 32/64-bit XLEN。ARM AArch64 是 64-bit,但 ARMv7 是 32-bit + SIMD 128-bit。x86-64 是 64-bit。跨 ISA 时,静态断言需要放宽。

**缓解**: 推迟到跨 ISA 工作开始时。当前范围 (RISC-V only) 不需要改。

#### 中等 5: `CtrlLink::halt_when` 语义升级

**位置**: `include/cf/plugin/ctrl_link.h:24-96`。

**问题**: 当前 `CtrlLink::halt_when` 接受 `std::function<bool()>` 并 OR-合并所有 halt 条件。对 OoO 反压,halt 条件必须引用**特定 IID 的 IQ 条目**,不是全局状态。

**缓解**: 不改 `CtrlLink`。插件级代码可以用任何它想要的范围计算 halt 条件; 框架的 `bool should_halt()` 是好的。

#### 中等 6: `RiscvDecodePlugin` 的特定 decode 逻辑

**位置**: `decode.h:56-88`。

**问题**: RISC-V-specific,不会被 OoO 触及。OoO `RenamePlugin` 将在 `RiscvDecodePlugin` 写 `DECODE` Payload 后读它,然后分配 PRF 条目。decode 插件的工作 (把指令字变成 `DECODE` + `RISCV_DETAIL`) 不变。

**缓解**: 不需要改。

---

## 3. Phase 1 必须锁定的 7 项决策 (D.1-D.7)

> 本节是 [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md) §D 的提炼。

**总成本**: ~150 行 header churn,0 行行为改变。**防止**: ~2000 行 Phase 5+ 重构。

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

### D.5 `RegFilePlugin::build` 字符串字面量 → stage-name 成员

**改变** (`reg_file.h:119-153`): 添加 `std::string decode_stage_` 和 `std::string writeback_stage_` 成员,在 ctor 从 `CPUConfig` 设置。替换字符串字面量 `"decode"` 和 `"writeback"` 为 `decode_stage_` 和 `writeback_stage_`。

**成本**: ~15 行。

**为什么现在**: 任何人想要 2-wide superscalar 带 `decode_lane0` / `decode_lane1` stage 名的那一刻,每个插件的 `build()` 必须被重模板化。现在在**一个**插件 (RegFile) 做证明模式; 以后做意味着在**11** 个插件同时做。

### D.6 文档化 `at_stage` + `commit_hook` 作为 OoO commit 原语

**添加到 `pipe_builder.h` header 注释** (`:1-17` 和 `:104-125`): 明确声明:
- `at_stage` = "this stage's logic per cycle"
- `commit_hook` = "end-of-cycle commit, OoO commit primitive"
- `flush_when` = "mispredict squash primitive"

框架已经提供这些。**唯一**缺失的是文档。这花费零代码。

### D.7 (可选) `setup_with_config` (v1.0 §6.3 提案)

v1.0 §6.3 提议添加 `PluginBase::setup_with_config(pb, const void* cfg)`。**按提议采纳**。它是工厂传递 `CPUConfig` 到插件的 hook (per D.2/D.4/D.5 以上),它不触及任何现有代码路径。

### D.1-D.7 总结

| Lock | Phase-1 成本 | Phase-5 成本 (如果不锁定) | 严重性 (如果不锁定) |
|------|-------------|----------------------------|-------------------------|
| D.1: `UID`/`THREAD_ID`/`IID_PC` Payloads | 3 行,header | 触及 11 个 `at_stage` 回调 | Medium |
| D.2: 模板化插件为 `N_THREADS` | ~90 行 (3 plugins × 30) | 重写 3 个插件 + 每个测试 | **High** |
| D.3: `HazardKind` enum | 5 行 | API break at every caller site | Medium |
| D.4: `tid` in `predict`/`update` | 10 行 | 重写 BP 两次 (单线程 → 多线程) | **High** (最便宜锁定) |
| D.5: 插件 stage-name 成员 | ~15 行 × plugins | 11 plugins × stage-name 改变 | **High** (最多插件受影响) |
| D.6: `at_stage`+`commit_hook`+`flush_when` 文档 | 0 行 | 设计者在 Phase 5 重新发明这些 | Low |
| D.7: `setup_with_config` | 3 行 | 工厂不能传递 `CPUConfig` 到插件 | Medium |

**总 Phase-1 工作**: ~150 行 header churn,加 0 行行为改变。**防止**: ~2000 行 Phase 5+ 重构。

---

## 4. 可以推迟到 Phase 5+ 的 8 项决策 (E.1-E.8)

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

## 5. 多架构 DSE 框架设计

> 本节基于 BOOM / XiangShan / Chipyard / Rocket-Chip 的调研。

### 5.1 参考: BOOM 的参数化模式

BOOM v3 (`BoomCoreParams` case class) 暴露:
- **Frontend**: `fetchWidth=8` / `decodeWidth=3` / `maxBrCount=16` / `numFetchBufferEntries=24`
- **IQ**: `issueParams: Seq[IssueParams]` (IQT_MEM/IQT_INT/IQT_FP 各自独立配置 `issueWidth`, `numEntries`, `dispatchWidth`)
- **ROB/Registers**: `numRobEntries=96` / `numIntPhysRegisters=100` / `numFpPhysRegisters=96`
- **LSQ**: `numLdqEntries=24` / `numStqEntries=24`
- **Branch prediction**: `branchPredictor: Function2[BranchPredictionBankResponse, Parameters, Tuple2[Seq[BranchPredictorBank], BranchPredictionBankResponse]]` — **函数本身是参数**

### 5.2 参考: Chipyard 的 Configs 系统

Chipyard 的配置模型是 **Rocket-Chip 参数系统**:
- **Config** 是 generator 参数集合
- **Additive config fragments** (命名约定 `With<Name>`, `++`-compose) 覆盖或扩展彼此
- **Composition order matters**: fragments 应用**right-to-left / bottom-to-top**
- **`site`/`here`/`up` maps** 从 config 层次根 (`site`),当前层 (`here`),上一层 (`up`) 给出 Key 的值
- **两种 "mixin"**: 一种 for lazy module (`CanHave<Mixin>`) — 定义逻辑连接; 一种 for lazy module implementation (`CanHave<Mixin>ModuleImp`) — 做实际 Chisel RTL elaboration
- **TilePluginProvider 扩展**: 允许 optional per-tile fragments (例如 `WithTraceIO`, `WithTilePrefetchers`) 发现和配置 generators 无硬依赖

### 5.3 ChipForge 的多架构 DSE 框架设计

基于 BOOM + Chipyard 的启示,ChipForge 应该设计:

#### 5.3.1 `CpuConfig` 扩展 (参考 BOOM `BoomCoreParams`)

扩展 `CPUConfig` (v1.0 §4.2) 增加 OoO/Superscalar 参数:

```cpp
struct CPUConfig {
  // ===== v1.0 现有字段 =====
  // ... (isa, pipeline_stages, branch_predictor, btb_entries, etc.)

  // ===== v2.0 新增: Superscalar / OoO 参数 =====
  // Frontend widths
  std::uint8_t fetch_width = 1;           // 1/2/4/8 (每周期 fetch 指令数)
  std::uint8_t decode_width = 1;          // 1/2/4/8 (每周期 decode 指令数)
  std::uint8_t rename_width = 1;          // 1/2/4/8 (每周期 rename 指令数)
  std::uint8_t dispatch_width = 1;        // 1/2/4/8 (每周期 dispatch 指令数)
  std::uint8_t issue_width = 1;           // 1/2/4/8 (每周期 issue 指令数)
  std::uint8_t commit_width = 1;          // 1/2/4/8 (每周期 commit 指令数)

  // ROB / PRF sizes
  std::uint16_t rob_entries = 32;         // 16/32/64/96/128/192/256
  std::uint16_t num_phys_int_regs = 64;   // 64/96/128/256 (物理整数寄存器)
  std::uint16_t num_phys_fp_regs = 0;     // 0/64/96/128 (物理浮点寄存器, 0 = no FPU)

  // IQ / LSQ sizes
  std::uint16_t iq_int_entries = 16;      // 8/16/24/32 (整数 IQ 条目)
  std::uint16_t iq_fp_entries = 0;        // 0/16/24/32 (浮点 IQ 条目)
  std::uint16_t ldq_entries = 16;         // 8/16/24/32 (load queue 条目)
  std::uint16_t stq_entries = 16;         // 8/16/24/32 (store queue 条目)

  // SMT parameters (Phase 6+)
  std::uint8_t num_threads = 1;           // 1/2/4/8 (每核线程数)
  std::string smt_fetch_policy = "RoundRobin";  // RoundRobin/ICOUNT/Branch/IQCount/LSQCount
  std::string smt_lsq_policy = "Partitioned";   // Dynamic/Partitioned/Threshold
  std::string smt_rob_policy = "Partitioned";   // Dynamic/Partitioned/Threshold
  std::string smt_commit_policy = "RoundRobin"; // RoundRobin/OldestReady

  // Branch predictor factory (参考 BOOM)
  std::function<std::unique_ptr<BranchPredictorBase>(const CPUConfig&)>
    branch_predictor_factory = nullptr;  // nullptr = use default factory

  // Execution model
  bool enable_ooo = false;              // true = OoO, false = in-order
  bool enable_superscalar = false;      // true = multi-issue, false = single-issue
};
```

#### 5.3.2 `BranchPredictorFactory` 模式 (参考 BOOM `branchPredictor: Function2[...]`)

**核心洞察**: BOOM 的分支预测器参数是**函数本身**,不是枚举。ChipForge 应该设计:

```cpp
// ip/cpu/plugins/branch_predictor_factory.h (新建)
namespace cf::cpu::plugins {

// 分支预测器基类
class BranchPredictorBase {
 public:
  virtual ~BranchPredictorBase() = default;
  virtual T predict(T pc, uint8_t tid) const = 0;
  virtual void update(T pc, bool taken, T target, uint8_t tid) = 0;
  virtual void checkpoint(uint8_t tid) = 0;  // OoO mispredict recovery
  virtual void restore(uint8_t tid) = 0;
  virtual void reset() = 0;
};

// 默认工厂 (v1.0 §6.1 的 5 种实例化)
std::unique_ptr<BranchPredictorBase>
default_branch_predictor_factory(const CPUConfig& cfg);

// 用户自定义工厂示例
std::unique_ptr<BranchPredictorBase>
custom_tournament_factory(const CPUConfig& cfg);

}  // namespace cf::cpu::plugins
```

**用法**:
```cpp
CPUConfig cfg;
cfg.branch_predictor_factory = &custom_tournament_factory;
auto pb = CpuFactory<uint32_t>::build_cpu(cfg);
```

**收益**:
- 用户可以注入自定义分支预测器 (TAGE / Perceptron / 等) 无需改框架
- 工厂本身是参数,支持 DSE sweep 跨不同预测器类型
- 参考 BOOM 的 `branchPredictor: Function2[...]` 模式

#### 5.3.3 `WithCpuConfig<CPUConfig>` 模板 (参考 Chipyard `WithN<Size>BoomCores`)

Chipyard 用 `WithN{Small|Medium|Large}BoomCores(n)` 组合器。ChipForge 应该设计类似:

```cpp
// ip/cpu/configs/presets.h (新建)
namespace cf::cpu::configs {

template <typename Base>
struct WithCpuConfig : Base {
  CPUConfig cpu_config;
};

// Preset: "Small" = 3-stage in-order, no BP
struct SmallCpuConfig : WithCpuConfig<BaseConfig> {
  SmallCpuConfig() {
    cpu_config.pipeline_stages = 3;
    cpu_config.fetch_width = 1;
    cpu_config.decode_width = 1;
    cpu_config.branch_predictor = "static";
    cpu_config.btb_entries = 16;
  }
};

// Preset: "Medium" = 5-stage in-order, GShare BP
struct MediumCpuConfig : WithCpuConfig<BaseConfig> {
  MediumCpuConfig() {
    cpu_config.pipeline_stages = 5;
    cpu_config.fetch_width = 1;
    cpu_config.decode_width = 1;
    cpu_config.branch_predictor = "gshare";
    cpu_config.btb_entries = 64;
  }
};

// Preset: "Large" = 7-stage OoO, Tournament BP
struct LargeCpuConfig : WithCpuConfig<BaseConfig> {
  LargeCpuConfig() {
    cpu_config.pipeline_stages = 7;
    cpu_config.enable_ooo = true;
    cpu_config.fetch_width = 4;
    cpu_config.decode_width = 3;
    cpu_config.rename_width = 3;
    cpu_config.dispatch_width = 3;
    cpu_config.issue_width = 3;
    cpu_config.commit_width = 3;
    cpu_config.rob_entries = 96;
    cpu_config.num_phys_int_regs = 100;
    cpu_config.iq_int_entries = 32;
    cpu_config.ldq_entries = 24;
    cpu_config.stq_entries = 24;
    cpu_config.branch_predictor = "tournament";
    cpu_config.btb_entries = 256;
    cpu_config.branch_predictor_factory = &custom_tournament_factory;
  }
};

}  // namespace cf::cpu::configs
```

**用法**:
```cpp
using namespace cf::cpu::configs;
auto pb = CpuFactory<uint32_t>::build_cpu(LargeCpuConfig{}.cpu_config);
```

**收益**:
- 预设配置 (Small/Medium/Large) 对应不同性能/面积 trade-offs
- 用户可以继承 `WithCpuConfig` 创建自定义预设
- 参考 Chipyard 的 `WithNBoomCores` 模式

### 5.4 DSE Sweep 参数集 (参考 BOOM + gem5)

v1.0 §8.3 的 `DEFAULT_DSE_SPACE` 应该扩展:

```python
DEFAULT_DSE_SPACE_V2 = {
    # v1.0 现有
    "pipeline_stages":     [3, 5, 7],
    "btb_entries":         [16, 64, 256],
    "branch_predictor":    ["static", "gshare", "tournament"],  # 扩展 tournament
    "ext_m":               [False, True],
    "mul_latency":         [1, 3],
    "isa":                 ["rv32i", "rv64i"],

    # v2.0 新增: Superscalar / OoO
    "fetch_width":         [1, 2, 4],
    "decode_width":        [1, 2, 3],
    "issue_width":         [1, 2, 4],
    "rob_entries":         [32, 64, 96, 128],
    "iq_int_entries":      [16, 24, 32],
    "num_phys_int_regs":   [64, 96, 128],
    "ldq_entries":         [16, 24],
    "stq_entries":         [16, 24],

    # v2.0 新增: SMT (Phase 6+)
    # "num_threads":       [1, 2, 4],
    # "smt_fetch_policy":  ["RoundRobin", "ICOUNT"],
}
```

**注意**: SMT 参数注释掉,因为 Phase 6+ 才实现。Superscalar/OoO 参数在 Phase 5 实现。

---

## 6. 分支预测器工厂模式

> 本节详细设计 v1.0 §6.1 的改进版。

### 6.1 v1.0 的局限

v1.0 §6.1 设计 `BranchPredictorPlugin<T, BTB_SIZE, BIMODAL_SZ, GSHARE_SZ, GHR_BITS>` 5 个编译期参数,10 种显式实例化。

**局限**:
1. **编译期参数**意味着每个新预测器类型需要新模板特化
2. **没有工厂模式**: `CpuFactory::build_cpu` 内 switch on `cfg.btb_entries` 选择实例化,不灵活
3. **不支持 per-thread predictor**: SMT 需要 per-thread GHR/PHT/BTB,v1.0 模板没有 `N_THREADS` 参数
4. **不支持 checkpoint-and-restore**: OoO 需要在 mispredict 时恢复 GHR,PHT

### 6.2 v2.0 的改进: `BranchPredictorBase` + Factory

**核心改变**: 从编译期模板参数 → 运行时工厂函数。

```cpp
// ip/cpu/plugins/branch_predictor_base.h (新建)
namespace cf::cpu::plugins {

// 分支预测器基类 (运行时多态)
template <typename T>
class BranchPredictorBase {
 public:
  virtual ~BranchPredictorBase() = default;

  // 预测 (v1.0 API 扩展 tid)
  virtual T predict(T pc, uint8_t tid) const = 0;
  virtual bool predict_taken(T pc, uint8_t tid) const = 0;

  // 更新 (v1.0 API 扩展 tid)
  virtual void update(T pc, bool taken, T target, uint8_t tid) = 0;

  // OoO checkpoint-and-restore (新增)
  virtual void checkpoint(uint8_t tid) {}  // 默认空实现 (in-order 不需要)
  virtual void restore(uint8_t tid) {}

  // 重置
  virtual void reset() = 0;
};

// 工厂函数类型
template <typename T>
using BranchPredictorFactory =
  std::function<std::unique_ptr<BranchPredictorBase<T>>(const CPUConfig&)>;

}  // namespace cf::cpu::plugins
```

### 6.3 实现: `GSharePredictor<T>` / `TournamentPredictor<T>`

```cpp
// ip/cpu/plugins/gshare_predictor.h (新建,从 v1.0 BranchPredictorPlugin 提取)
namespace cf::cpu::plugins {

template <typename T>
class GSharePredictor : public BranchPredictorBase<T> {
 public:
  explicit GSharePredictor(const CPUConfig& cfg)
    : btb_size_(cfg.btb_entries),
      gshare_size_(cfg.btb_entries),
      ghr_bits_(cfg.ghr_bits),
      n_threads_(cfg.num_threads) {
    reset();
  }

  T predict(T pc, uint8_t tid) const override {
    // ... v1.0 gshare_predict 逻辑,索引 gshare_[tid]
  }

  void update(T pc, bool taken, T target, uint8_t tid) override {
    // ... v1.0 gshare_update 逻辑,更新 gshare_[tid] 和 global_history_[tid]
  }

  void checkpoint(uint8_t tid) override {
    // 保存 global_history_[tid] 到 checkpoint_buffer_[tid]
    checkpoint_buffer_[tid] = global_history_[tid];
  }

  void restore(uint8_t tid) override {
    // 恢复 global_history_[tid] 从 checkpoint_buffer_[tid]
    global_history_[tid] = checkpoint_buffer_[tid];
  }

  void reset() override {
    // ... v1.0 reset 逻辑,扩展到 n_threads_
  }

 private:
  std::size_t btb_size_;
  std::size_t gshare_size_;
  std::uint8_t ghr_bits_;
  std::uint8_t n_threads_;

  // Per-thread state (D.2 模板化的运行时版本)
  std::vector<std::uint8_t> global_history_;           // [n_threads_]
  std::vector<std::uint8_t> checkpoint_buffer_;        // [n_threads_]
  std::vector<std::array<Counter, ...>> gshare_;       // [n_threads_]
  std::vector<std::array<BtbEntry, ...>> btb_;         // [n_threads_]
};

}  // namespace cf::cpu::plugins
```

### 6.4 默认工厂

```cpp
// ip/cpu/plugins/branch_predictor_factory.cpp (新建)
namespace cf::cpu::plugins {

template <typename T>
std::unique_ptr<BranchPredictorBase<T>>
default_branch_predictor_factory(const CPUConfig& cfg) {
  if (cfg.branch_predictor == "static") {
    return std::make_unique<StaticPredictor<T>>(cfg);
  } else if (cfg.branch_predictor == "bimodal") {
    return std::make_unique<BimodalPredictor<T>>(cfg);
  } else if (cfg.branch_predictor == "gshare") {
    return std::make_unique<GSharePredictor<T>>(cfg);
  } else if (cfg.branch_predictor == "tournament") {
    return std::make_unique<TournamentPredictor<T>>(cfg);
  } else {
    throw std::invalid_argument("unknown branch_predictor: " + cfg.branch_predictor);
  }
}

// 显式实例化
template std::unique_ptr<BranchPredictorBase<uint32_t>>
default_branch_predictor_factory<uint32_t>(const CPUConfig&);

template std::unique_ptr<BranchPredictorBase<uint64_t>>
default_branch_predictor_factory<uint64_t>(const CPUConfig&);

}  // namespace cf::cpu::plugins
```

### 6.5 `CpuFactory::build_cpu` 集成

```cpp
// ip/cpu/cpu_factory.h — build_cpu 修改
template <typename T = std::uint32_t>
class CpuFactory {
 public:
  static std::unique_ptr<cf::plugin::PipeBuilder> build_cpu(const CPUConfig& cfg) {
    auto pb = std::make_unique<cf::plugin::PipeBuilder>();

    // ... v1.0 §7.2 的 Phase 1-4 逻辑 ...

    // 2.2 分支预测器 (v2.0 工厂模式)
    std::unique_ptr<cf::plugin::PluginBase> bp;
    if (cfg.enable_branch_predictor) {
      auto factory = cfg.branch_predictor_factory
                       ? cfg.branch_predictor_factory
                       : &plugins::default_branch_predictor_factory<T>;
      auto predictor = factory(cfg);
      bp = std::make_unique<plugins::BranchPredictorPlugin<T>>(std::move(predictor));
    }

    // ... 其余逻辑不变 ...
  }
};
```

**收益**:
- 用户可以注入自定义分支预测器 (TAGE / Perceptron / 等) 无需改框架
- 工厂本身是参数,支持 DSE sweep 跨不同预测器类型
- Per-thread predictor 状态 (D.2 模板化的运行时版本)
- OoO checkpoint-and-restore (新增)
- 参考 BOOM 的 `branchPredictor: Function2[...]` 模式

---

## 7. SMT 接口设计方案

> 本节基于 Intel HT / AMD Zen / IBM POWER / SPARC 的 SMT 调研。

### 7.1 核心洞察: ThreadContext 作为一等公民

**SMT 接口设计的核心是 ThreadContext 作为一等公民,与 Stage (功能单元) 分离**。

当前架构的 `RegFilePlugin` / `HazardPlugin` / `BranchPredictorPlugin` 都是**单例状态**。SMT 需要**per-thread 状态**。

**设计原则**:
1. **ThreadContext** 是 CPU 拥有的对象,包含 per-thread 状态 (PC,寄存器文件,rename map,ROB head,trap CSRs)
2. **Stage plugins** 不拥有 ThreadContext; 它们通过 `core->ctx(tid)` accessor 读/写
3. **Stages 分类为 `Shared | PerThread | Hybrid`** 在注册时。这是 build-time enum,不是运行时决策:
   - `Shared` (例如 Fetch, IQ, ICache, EX) — 从多个 tids 接收 payloads
   - `PerThread` (例如 Rename, ROB head, ICount counter) — `PerThread<Plugin>` 实例化 N 份
   - `Hybrid` (例如 LSQ, PRF) — 单一 Plugin 有 N 个内部分区; 暴露 `partition_for(tid)`

### 7.2 Phase 1 必须锁定的 SMT 决策

**从 D.1-D.7 提取 SMT 相关**:

1. **D.1**: `THREAD_ID` Payload (2 bits,支持 SMT2/SMT4)
2. **D.2**: `RegFilePlugin<T, N_REGS, N_THREADS>` / `HazardPlugin<T, N_REGS, N_THREADS>` / `BranchPredictorPlugin<T, ..., N_THREADS>`
3. **D.4**: `predict(pc, tid)` / `update(pc, taken, target, tid)`

**额外 SMT 决策** (Phase 1 锁定,Phase 6+ 实现):

#### D.8 `ThreadContext` 结构 (Phase 1 定义,Phase 6+ 实现)

```cpp
// ip/cpu/core/thread_context.h (新建,Phase 1 定义)
namespace cf::cpu::core {

template <typename T>
struct ThreadContext {
  std::uint8_t tid;

  // Per-thread architectural state
  T pc;                    // Program counter
  T next_pc;               // Next PC (for branch recovery)
  std::uint8_t privilege;  // Privilege mode (M/U/S)

  // Per-thread exception state
  T mcause;
  T mepc;
  T mtval;

  // Per-thread performance counters
  std::uint64_t inst_count;
  std::uint64_t cycle_count;

  // Per-thread ICount (for SMT fetch policy)
  std::uint32_t icount;    // Instructions in pre-execute stages

  void reset() {
    pc = T{0};
    next_pc = T{0};
    privilege = 3;  // Machine mode
    mcause = T{0};
    mepc = T{0};
    mtval = T{0};
    inst_count = 0;
    cycle_count = 0;
    icount = 0;
  }
};

}  // namespace cf::cpu::core
```

**成本**: ~30 行,header-only。Phase 1 不使用它 (单线程 in-order 不需要)。

**为什么现在**: 定义 `ThreadContext` 结构是免费的。Phase 6+ 实现 SMT 时,这个结构已经存在,不需要设计。

#### D.9 `Cpu` 类持有 `ThreadContext` 数组 (Phase 1 定义,Phase 6+ 实现)

```cpp
// ip/cpu/core/cpu.h (新建,Phase 1 定义)
namespace cf::cpu::core {

template <typename T, std::size_t MAX_THREADS = 8>
class Cpu {
 public:
  Cpu() {
    for (std::size_t i = 0; i < MAX_THREADS; ++i) {
      ctxs_[i].tid = static_cast<std::uint8_t>(i);
      ctxs_[i].reset();
    }
    active_tids_ = 1;
  }

  ThreadContext<T>& ctx(std::uint8_t tid) {
    return ctxs_[tid];
  }

  const ThreadContext<T>& ctx(std::uint8_t tid) const {
    return ctxs_[tid];
  }

  std::uint8_t active_threads() const {
    return active_tids_;
  }

  void activate_thread(std::uint8_t tid) {
    if (tid < MAX_THREADS) {
      active_tids_ = std::max(active_tids_, tid + 1);
    }
  }

  void deactivate_thread(std::uint8_t tid) {
    // Phase 6+ 实现: drain ROB/LSQ for this TID
  }

 private:
  std::array<ThreadContext<T>, MAX_THREADS> ctxs_;
  std::uint8_t active_tids_;
};

}  // namespace cf::cpu::core
```

**成本**: ~50 行,header-only。Phase 1 不使用它 (单线程 in-order 不需要)。

**为什么现在**: 定义 `Cpu` 类是免费的。Phase 6+ 实现 SMT 时,这个类已经存在,不需要设计。

### 7.3 Phase 6+ 实现的 SMT 插件

**Phase 6+ 将实现**:

1. **`FetchPolicyPlugin<T>`**: 检查 `ThreadContext::icount[tid]` 并返回下一个要 feed 的 `tid`。这是 POWER9 "Thread Switch Fetch Priority" 接口。

2. **`PerThread<Plugin>` 模板**: 实例化 N 份插件; 每份得到不同的 `tid` 过滤器在输入。

3. **`Hybrid` 插件**: 单一 Plugin 有 N 个内部分区; 暴露 `partition_for(tid)`。

**参考**:
- Intel HT: Shared ICache, TID-tagged BTB
- AMD Zen: Competitive sharing (Zen 1/2), Watermarked (Zen 3/4/5)
- IBM POWER: Per-thread GPR rename files (124 entries each), SMT2/SMT4/SMT8 mode-switchable

### 7.4 SMT DSE 参数集 (Phase 6+)

```python
SMT_DSE_SPACE = {
    "num_threads":        [1, 2, 4, 8],
    "smt_fetch_policy":   ["RoundRobin", "ICOUNT", "Branch", "IQCount", "LSQCount"],
    "smt_lsq_policy":     ["Dynamic", "Partitioned", "Threshold"],
    "smt_rob_policy":     ["Dynamic", "Partitioned", "Threshold"],
    "smt_commit_policy":  ["RoundRobin", "OldestReady"],
    "smt_lsq_threshold":  [16, 32, 64, 100],  # for Threshold policy
    "smt_rob_threshold":  [32, 64, 96, 100],  # for Threshold policy
}
```

**参考**: gem5 `SMT.py` 的 `SMTFetchPolicy` / `SMTQueuePolicy` / `CommitPolicy` enums。

---

## 8. DSE 方法论升级

> 本节基于 ESESC / Sniper / OpenDSE / Chipyard 的 DSE 方法论调研。

### 8.1 v1.0 的局限

v1.0 §8 的 DSE Sweep 工具:
- **笛卡尔积展开**: `sweep_driver.py` 生成所有组合
- **Pareto 分析**: `pareto_analyzer.py` 计算 Pareto 前沿
- **指标**: IPC / cycles / branch_miss / area_proxy

**局限**:
1. **组合爆炸**: 6 个参数 × 3-5 个值 = 720-15,625 个配置点。每个配置点跑 1M cycles = 几小时到几天。
2. **没有 surrogate modeling**: 每个配置点都跑 full simulation,没有 predictive model
3. **没有参数剪枝**: 所有参数平等对待,没有识别哪些参数对目标影响小
4. **没有对称性利用**: 异构 SoC 的 "两个 BOOM-Large + 一个 Rocket-Small" 和 "一个 Rocket-Small + 两个 BOOM-Large" 是同构的,但被当作两个点

### 8.2 v2.0 的改进: Predictive Modeling + Parameter Pruning

基于 DSE 方法论调研,v2.0 应该升级:

#### 8.2.1 Surrogate Modeling Tier (参考 ESESC `libpeq` / Ipek 2006 / OneDSE 2025)

**核心洞察**: Predictive modeling 是最有杠杆的技术。训练 surrogate model 代替 full simulation,节省 3-4 orders of magnitude。

**设计**:
```python
# tools/dse/surrogate_model.py (新建)
import numpy as np
from sklearn.ensemble import RandomForestRegressor
from sklearn.neural_network import MLPRegressor

class DseSurrogateModel:
  def __init__(self, model_type="random_forest"):
    if model_type == "random_forest":
      self.model = RandomForestRegressor(n_estimators=100, random_state=42)
    elif model_type == "neural_network":
      self.model = MLPRegressor(hidden_layer_sizes=(64, 32), random_state=42)
    else:
      raise ValueError(f"unknown model_type: {model_type}")

  def fit(self, X, y):
    """
    X: np.array, shape (n_samples, n_params) — 参数向量
    y: np.array, shape (n_samples, n_objectives) — 目标向量 (IPC, area, power, etc.)
    """
    self.model.fit(X, y)

  def predict(self, X):
    """
    X: np.array, shape (n_samples, n_params)
    Returns: np.array, shape (n_samples, n_objectives)
    """
    return self.model.predict(X)

  def score(self, X, y):
    """R^2 score"""
    return self.model.score(X, y)
```

**用法**:
```python
# 1. 跑 1000 个随机配置点 (full simulation)
X_train, y_train = run_random_samples(n=1000)

# 2. 训练 surrogate model
model = DseSurrogateModel(model_type="random_forest")
model.fit(X_train, y_train)

# 3. 用 surrogate model 预测 100,000 个配置点 (fast)
X_all = generate_all_configs(n=100_000)
y_pred = model.predict(X_all)

# 4. 从预测结果计算 Pareto 前沿
pareto_front = compute_pareto(X_all, y_pred)

# 5. 对 Pareto 前沿上的点跑 full simulation 验证
y_true = run_full_simulation(pareto_front)

# 6. 评估 surrogate model accuracy
print(f"R^2 score: {model.score(pareto_front, y_true):.3f}")
```

**收益**:
- 从 100,000 个配置点 full simulation (几天) → 1,000 个 full simulation + 99,000 个 surrogate prediction (几小时)
- 参考 ESESC `libpeq` (2008): analytical surrogate derived from precomputed CACTI sweep
- 参考 OneDSE (2025): workload-aware neural surrogate

#### 8.2.2 Parameter Pruning (参考 Lee & Brooks 2007 / Palesi & Givargis 2002)

**核心洞察**: 识别对目标影响小的参数并固定它们,把高维设计空间折叠成低维。

**设计**:
```python
# tools/dse/parameter_pruning.py (新建)
import numpy as np
from sklearn.ensemble import RandomForestRegressor
from sklearn.inspection import permutation_importance

def prune_parameters(X, y, threshold=0.01):
  """
  X: np.array, shape (n_samples, n_params)
  y: np.array, shape (n_samples,) — 单一目标
  threshold: float — 重要性阈值 (低于阈值的参数被剪枝)

  Returns: List[int] — 保留的参数索引
  """
  # 训练 random forest
  model = RandomForestRegressor(n_estimators=100, random_state=42)
  model.fit(X, y)

  # 计算 permutation importance
  result = permutation_importance(model, X, y, n_repeats=10, random_state=42)

  # 保留重要性 > threshold 的参数
  important_indices = [i for i, imp in enumerate(result.importances_mean) if imp > threshold]

  print(f"Pruned {X.shape[1] - len(important_indices)} / {X.shape[1]} parameters")
  print(f"Kept parameters: {important_indices}")
  print(f"Importances: {result.importances_mean}")

  return important_indices
```

**用法**:
```python
# 1. 跑 1000 个随机配置点
X, y = run_random_samples(n=1000)

# 2. 对 IPC 目标剪枝参数
important_for_ipc = prune_parameters(X, y["ipc"], threshold=0.01)

# 3. 对 area 目标剪枝参数
important_for_area = prune_parameters(X, y["area"], threshold=0.01)

# 4. 保留对所有目标重要的参数
important_all = list(set(important_for_ipc) | set(important_for_area))

# 5. 用剪枝后的参数集重新生成配置空间
X_pruned = generate_configs(params=important_all)
```

**收益**:
- 从 6 个参数 × 5 个值 = 15,625 个配置点 → 3 个参数 × 5 个值 = 125 个配置点
- 参考 Lee & Brooks (2007): "varying all design parameters simultaneously instead of fixing most non-depth parameters"
- 参考 Palesi & Givargis (2002): parameter dependency pruning

#### 8.2.3 Symmetry Reduction (参考 mpsym Goens 2022)

**核心洞察**: 异构 SoC 的 "两个 BOOM-Large + 一个 Rocket-Small" 和 "一个 Rocket-Small + 两个 BOOM-Large" 是同构的。识别对称性并只探索一个代表,节省 8.6× speed-up, 30× better SA results。

**设计**:
```python
# tools/dse/symmetry_reduction.py (新建)
import itertools

def reduce_symmetry(configs, key_func):
  """
  configs: List[dict] — 配置列表
  key_func: Callable[[dict], tuple] — 把配置映射到规范形式

  Returns: List[dict] — 去重后的配置列表
  """
  seen = set()
  unique_configs = []

  for cfg in configs:
    key = key_func(cfg)
    if key not in seen:
      seen.add(key)
      unique_configs.append(cfg)

  print(f"Reduced {len(configs)} → {len(unique_configs)} configs ({len(configs) / len(unique_configs):.1f}×)")
  return unique_configs

# 示例: 异构 SoC 对称性
def soc_key(cfg):
  """两个 BOOM-Large + 一个 Rocket-Small == 一个 Rocket-Small + 两个 BOOM-Large"""
  cores = sorted(cfg["cores"])  # 排序 cores
  return tuple(cores)
```

**用法**:
```python
# 生成所有异构 SoC 配置
configs = generate_heterogeneous_soc_configs()

# 对称性归约
configs_unique = reduce_symmetry(configs, key_func=soc_key)

# 跑 unique 配置
results = run_simulation(configs_unique)
```

**收益**:
- 从 3! = 6 个同构配置 → 1 个 unique 配置
- 参考 mpsym (TU Dresden 2022): 8.6× speed-up, 30× better SA results on Kalray MPPA3

### 8.3 DSE 工具升级路线图

| Phase | 工具 | 方法 |
|-------|------|------|
| Phase E (v1.0) | `sweep_driver.py` | 笛卡尔积展开 + full simulation |
| Phase E (v1.0) | `pareto_analyzer.py` | Pareto 前沿计算 |
| Phase E+ (v2.0) | `surrogate_model.py` | Predictive modeling (RF / NN) |
| Phase E+ (v2.0) | `parameter_pruning.py` | Parameter importance analysis |
| Phase E+ (v2.0) | `symmetry_reduction.py` | Symmetry-aware config generation |
| Phase E++ (v2.0) | `nsga2_optimizer.py` | NSGA-II multi-objective optimization |

---

## 9. 实施路线图更新 (Phase G)

> 本节新增 Phase G (Forward-Compatibility Locks) 到 v1.0 §9 的 Phase A-F。

### Phase G — Forward-Compatibility Locks (M4-DSE 子阶段, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| G.1 | D.1: 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads | `ip/cpu/core/payload_common.h` | 3 行,编译通过 |
| G.2 | D.2: 模板化 `RegFilePlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/reg_file.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.3 | D.2: 模板化 `HazardPlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/hazard.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.4 | D.2: 模板化 `BranchPredictorPlugin<T, ..., N_THREADS>` | `ip/cpu/plugins/branch_predictor.h` | 默认 `N_THREADS=1` 保持当前行为 |
| G.5 | D.3: `HazardPlugin::has_hazard` 返回 `HazardKind` enum | `ip/cpu/plugins/hazard.h` | API break,0 个外部调用者 |
| G.6 | D.4: `BranchPredictorPlugin::predict`/`update` 接受 `tid` | `ip/cpu/plugins/branch_predictor.h` | `tid=0` 在 Phase 1 |
| G.7 | D.5: `RegFilePlugin::build` stage-name 成员 | `ip/cpu/plugins/reg_file.h` | 字符串字面量 → 成员变量 |
| G.8 | D.6: 文档化 `at_stage` + `commit_hook` + `flush_when` | `include/cf/plugin/pipe_builder.h` | 0 行代码,仅文档 |
| G.9 | D.7: 采纳 `setup_with_config` | `include/cf/plugin/plugin_base.h` | 3 行,如 v1.0 §6.3 提案 |
| G.10 | D.8: 定义 `ThreadContext<T>` 结构 | `ip/cpu/core/thread_context.h` (新建) | 30 行,Phase 1 不使用 |
| G.11 | D.9: 定义 `Cpu<T, MAX_THREADS>` 类 | `ip/cpu/core/cpu.h` (新建) | 50 行,Phase 1 不使用 |
| G.12 | 5.3.1: 扩展 `CPUConfig` 增加 OoO/Superscalar/SMT 参数 | `ip/cpu/cpu_factory.h` | ~30 个新字段 |
| G.13 | 5.3.2: 实现 `BranchPredictorBase<T>` + Factory 模式 | `ip/cpu/plugins/branch_predictor_base.h` (新建) | 运行时多态 |
| G.14 | 5.3.3: 实现 `WithCpuConfig<CPUConfig>` 预设 (Small/Medium/Large) | `ip/cpu/configs/presets.h` (新建) | 3 个预设 |
| G.15 | 测试: D.1-D.9 所有锁的单元测试 | `tests/cpu/test_forward_compat.cpp` (新建) | 15+ 个用例 PASS |
| G.16 | 文档: 更新 `blueprint.md` / `status.md` / `README.md` | `ip/cpu/docs/` | 同步更新 |

### Phase H — Surrogate Modeling (M5-DSE 子阶段, 可选, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| H.1 | 实现 `surrogate_model.py` | `tools/dse/surrogate_model.py` (新建) | RF / NN surrogate |
| H.2 | 实现 `parameter_pruning.py` | `tools/dse/parameter_pruning.py` (新建) | Permutation importance |
| H.3 | 实现 `symmetry_reduction.py` | `tools/dse/symmetry_reduction.py` (新建) | Config deduplication |
| H.4 | 集成到 `sweep_driver.py` | `tools/dse/sweep_driver.py` | Surrogate + pruning 集成 |
| H.5 | 测试: surrogate model accuracy (R² > 0.9) | `tools/dse/test_surrogate.py` (新建) | R² > 0.9 on held-out set |
| H.6 | 文档: 更新 `tools/dse/README.md` | `tools/dse/README.md` | 使用指南 |

### Phase I — NSGA-II Optimization (M5-DSE 子阶段, 可选, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| I.1 | 实现 `nsga2_optimizer.py` | `tools/dse/nsga2_optimizer.py` (新建) | NSGA-II multi-objective |
| I.2 | 集成到 `sweep_driver.py` | `tools/dse/sweep_driver.py` | NSGA-II + surrogate 集成 |
| I.3 | 测试: Pareto 前沿质量 | `tools/dse/test_nsga2.py` (新建) | Pareto 前沿非 dominated |
| I.4 | 文档: 更新 `tools/dse/README.md` | `tools/dse/README.md` | 使用指南 |

---

## 10. 风险与限制

### 10.1 Phase G (Forward-Compatibility Locks) 的风险

1. **D.2 模板化 `N_THREADS` 可能增加编译时间**
   - **缓解**: 默认 `N_THREADS=1` 只实例化一次。用户想要 SMT 时,显式实例化 `N_THREADS=2/4/8`。
   - **成本**: ~30 行 per plugin,header-only。编译时间增加 ~10%。

2. **D.3 `HazardKind` enum 是 API break**
   - **缓解**: 唯一 in-tree 调用者是 `hazard.h:126` 自身。测试套件有 0 个外部调用者。
   - **成本**: ~5 行,函数体不变。

3. **D.5 stage-name 成员变量增加 `RegFilePlugin` 大小**
   - **缓解**: `std::string` 在 64-bit 系统是 32 字节 (SSO)。两个成员 = 64 字节。可接受。
   - **成本**: ~15 行。

4. **D.8/D.9 `ThreadContext` / `Cpu` 类在 Phase 1 不使用**
   - **缓解**: 它们是死代码,但定义它们是免费的。Phase 6+ 实现 SMT 时,它们已经存在。
   - **成本**: ~80 行,header-only。

### 10.2 Phase H (Surrogate Modeling) 的风险

1. **Surrogate model accuracy 可能不足 (R² < 0.9)**
   - **缓解**: 用 5-fold cross-validation 评估。如果 R² < 0.9,增加训练样本或换模型 (RF → NN)。
   - **成本**: 训练 1000 个样本 ~1 小时。

2. **Parameter pruning 可能剪掉重要参数**
   - **缓解**: 用低阈值 (0.01)。人工审查剪枝结果。
   - **成本**: 0 (自动)。

3. **Symmetry reduction 可能不适用于所有 DSE 空间**
   - **缓解**: 只对异构 SoC 配置应用对称性归约。同构 CPU 不需要。
   - **成本**: 0 (自动)。

### 10.3 Phase I (NSGA-II) 的风险

1. **NSGA-II 可能陷入局部最优**
   - **缓解**: 用多种子运行 (10 次),取最好 Pareto 前沿。
   - **成本**: 10× 计算时间。

2. **NSGA-II 可能慢 (每代需要评估种群)**
   - **缓解**: 用 surrogate model 评估种群,不是 full simulation。
   - **成本**: surrogate prediction ~1ms/点,full simulation ~1s/点。1000× 加速。

### 10.4 总体风险

1. **v2.0 改进可能过度工程**
   - **缓解**: Phase G (D.1-D.9) 是**必须的** (~150 行,防止 ~2000 行重构)。Phase H/I 是**可选的** (surrogate modeling,NSGA-II),只在 DSE 成为瓶颈时实施。
   - **决策**: Phase G 必须在 M4-DSE 实施。Phase H/I 推迟到 M5-DSE,可选。

2. **v2.0 改进可能与 v1.0 冲突**
   - **缓解**: v2.0 不修改 v1.0 的 Phase A-F 路线图。它**新增** Phase G/H/I。
   - **决策**: v1.0 保持不变。v2.0 是增量。

---

## 11. 附录: 与 v1.0 的差异对照

| v1.0 章节 | v2.0 改变 | 状态 |
|-----------|-----------|------|
| §1 当前真实状态 | 无改变 | ✅ 保留 |
| §2 设计空间维度清单 | 新增 OoO/Superscalar/SMT 维度 (§5.3.1) | 🟡 扩展 |
| §3 ISA 无关性 | 无改变 | ✅ 保留 |
| §4 CPUConfig 扩展方案 | 新增 OoO/Superscalar/SMT 参数 (§5.3.1) | 🟡 扩展 |
| §5 拓扑声明阶段 | 无改变 | ✅ 保留 |
| §6 Plugin 改造策略 | 替换 §6.1 (BranchPredictorPlugin 模板化) → §6 工厂模式 | 🟡 重设计 |
| §7 CpuFactory::build_cpu | 集成 §6.4 工厂模式 | 🟡 修改 |
| §8 DSE Sweep 工具 | 新增 surrogate modeling / parameter pruning / symmetry reduction (§8) | 🟡 扩展 |
| §9 实施路线图 | 新增 Phase G/H/I (§9) | 🟡 扩展 |
| §10 风险与限制 | 新增 Phase G/H/I 风险 (§10) | 🟡 扩展 |
| §11 附录: 与 multi_isa 对应 | 无改变 | ✅ 保留 |
| §12 附录: 关键文件改动 | 新增 Phase G 文件改动 (§9 Phase G 表格) | 🟡 扩展 |
| §13 与 v2.0 决策关系 | 无改变 | ✅ 保留 |

**总结**: v2.0 是 v1.0 的**增量改进**,不是重写。v1.0 的核心价值 (实证校核,CpuFactory 实现路径,5 个 ✅ 可调维度,4 个 ⚠️ 死字段) 完全保留。v2.0 新增:
1. **Phase 1 必须锁定的 7 项决策** (D.1-D.7,D.8,D.9)
2. **多架构 DSE 框架** (参考 BOOM/XiangShan/Chipyard)
3. **分支预测器工厂模式** (参考 gem5 + BOOM)
4. **SMT 接口设计方案** (参考 Intel HT / AMD Zen / IBM POWER)
5. **DSE 方法论升级** (surrogate modeling / parameter pruning / symmetry reduction / NSGA-II)
6. **Phase G/H/I 实施路线图**

---

*文档结束。如需修改,走 v2.0 拆分维护约定 (见 [cpu_implementation_guide_v2.0.md §3.5](cpu_implementation_guide_v2.0.md#35-后续维护约定))。*
