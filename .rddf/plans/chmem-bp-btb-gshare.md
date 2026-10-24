# ② chmem-bp-btb-gshare — 执行文件

> **change**: `chmem-bp-btb-gshare`
> **规模**: 大 · **前置**: ① `chmem-verilator-perf-harness` ✅ **已归档**（2026-10-24，交付点 A：Dhrystone 已在 CH_MEM 跑通 + 基线入库）· **★前置 `chmem-branch-microbench`** ✅ **已归档**（2026-10-24，交付 0c：4 档可控预测率负载 + 无 BP 参照基线）⇒ **本 change 的硬前置全部解除**
> 
> ⚠️ **原 P-2 前置（`chmem-load-use-bypass`）已于 2026-10-24 作废**（实施期 PoC 证伪：禁用 CH_MEM 侧 RAW stall 后 cycles **完全不变** 914544 ⇒ load-use **不是**差距根因；真根因 = **CH_MEM 无分支预测**，见 ADR-088 v1.2 §5.3.1）⇒ **不存在待落地的 load-use 前置**。**绝对值 1.4 门禁的恢复条件 = 本 change 自身落地**（ADR-088 v1.2 §5.3.2 **P-6**：与本 change 同源）⇒ 不再是外部依赖，而是本 change 的**交付物**
> **路线图**: `feat-bp-btb-gshare`（已改绑 CH_MEM-only + 归 B1 第 5 项）· `feat-tlm-backend-retirement` B1 第 5 项
> **上游裁决**: ADR-088 v1.1 §3 BP 行（**CH_MEM-only 且必须做；不适用 §5.2「不做」出口**）+ §5.2 BP 专项删除前置（双前置）
> **被依赖**: B3 删除门禁（`branch_predictor.h` 不可删）

---

## Goal

在 CH_MEM 后端实现**动态分支预测器**：`branch_predictor_chmem.h`（BTB 4K-entry + GShare 13-bit PHT + RAS 8-entry），注册进 `cpu_factory_chmem.h`，并用 ① 的 harness 在 CoreMark 上验证 `CoreMark/MHz ≥ 1.9`（v0.10.0 软门禁；v1.0.0 ≥2.3 为后续目标）。

> ⚠️ **TLM 版 `branch_predictor.h`（319 行）在本 change 全程保留** —— ADR-088 §5.2 BP 专项：删除需**双前置**（① CH_MEM 版存在**且已注册** ② CoreMark harness 在 `cpu_verilator_sim` 跑通并产出实测值）。

## Architecture

**CH_MEM-only 新实现**（不从 TLM 版逐行翻译 —— TLM 版是 C++ 类 + 私有成员，CH_MEM 侧需按 ADR-040 的 `ch_uint` / payload key 模态重写）：

```
ip/cpu/plugins/branch_predictor_chmem.h   (新)
  ├─ BTB   : 4096 entry × (tag + target + valid)  ⇒ ch_reg/ch_mem 承载
  ├─ GShare: 13-bit 全局历史 + PC 索引 ⇒ 2^13 PHT (2-bit saturating counter)
  └─ RAS   : 8-entry call/ret 栈
  └─ 决策入口: 在 fetch 路径给出 (predicted_target, direction)
  └─ misprediction recovery: 走 CtrlLink::flush_when 真实消费（feature 明写）

ip/cpu/cpu_factory_chmem.h   (改)
  └─ 注册 BranchPredictorPluginChmem<T>（canonical ordering 需核对 ADR-048）
```

**D4 / ADR-040 v3.0 硬约束**（`tools/check_plugin_portability.sh` 13/13 + `tools/verify_plugin_decision.sh` 8/8 会机检）：
- 无 `void tick()`（`PluginBase::tick() = delete`）
- 无状态机（禁 `enum class State` + `switch(state_)`）⇒ **BTB/PHT/RAS 必须是无状态组合 + 有状态存储**（`ch_reg` / `ch_mem`），**不得**写 FSM；多周期需求走 ADR-046 豁免（`CF_PLUGIN_USE_FSM_EXEMPT`）但**本 change 优先不用**
- Bundle 字段用 `cf::plugin::uint_t<N>`（CH_MEM 下 = `ch_uint<N>`），**禁**裸 `bool`/`uintN_t`（ADR-040 §10.5：裸类型 ⇒ cell 只能装 elaboration 期常量 ⇒ 信号退化为常量 ⇒ 测试全绿但语义为零）
- at_stage 回调内无 `if (cond) return;` 早返
- at_stage 内禁运行期 `if(ch_bool)`（`ch_bool::operator bool()` 静默取 false ⇒ 靠 CI grep 兜底）
- payload 生产者必须直写 ch 节点（`ch_uint<1>(<ch_bool 表达式>)` ⇒ 复用上游 `bool_to_uint` 分支 ⇒ 同一 `impl()` 指针）

## Tech Stack

C++23 · CppHDL（`ch_uint` / `ch_reg` / `ch_mem` / `ch_state_machine`）· Catch2 · 零 TLM 代码改动

---

## Tasks

### Task 1: 事实复核 + TLM 版语义提取（不写 CH_MEM 代码）

- [x] **Step 1: 读 TLM 版** —— `ip/cpu/plugins/branch_predictor.h`（319 行）+ `.cpp`（23 行）⇒ 提取**可测语义**：BTB 索引/替换策略/tag 比较、GShare 13-bit 历史移位与 PHT 2-bit 饱和计数、RAS 8-entry push/pop、misprediction 时的恢复动作
- [x] **Step 2: 提取 BP 判据全集（★v2 修正：v1 误引台账 §7 且漏 4 条）** —— BP 相关条目共 **14 条**，分布在台账 **§4 条目区**（不是 §7 裁决表）：
  - **10 条 `not-covered`**（§4 行 264–266, 271–274 + 行 349, 351, 357）：`no_predict_initially` · `predict_after_update` · `btb_entry` · `custom_btb_size` · `btb_runtime_16` · `btb_runtime_64` · `btb_runtime_256` · `d2_branch_predictor_multi_thread` · `d4_branch_predictor_tid_param` · `branch_predictor_set_tid_stores_tid`
  - **4 条 `partial`**（§4 行 267–270，**v1 完全漏掉**）：`not_taken_returns_zero` · `multiple_addresses` · `reset` · `per_thread_ghr_isolation`
  - ⇒ **14 条全部要重判**（10 条降级 + 4 条升级方向）；行号以台账实际为准，实施时 grep 复核（pattern: `^\| \`(btb_|predict|no_predict|not_taken|multiple_addresses|reset|per_thread_ghr|d2_branch_predictor|d4_branch_predictor|branch_predictor_set_tid)`）
- [x] **Step 3: 确认 canonical ordering** —— BP 在 fetch 路径 ⇒ 查 ADR-048 的注册规范序 + `cpu_factory_chmem.h` 现有 8-plugin 顺序 ⇒ 定 BP 的注册位（**必须**用 `tools/verify_plugin_decision.sh` 验证）
- [x] **Step 4: 确认 ① 已归档** —— ①（`chmem-verilator-perf-harness`）✅ 已归档 2026-10-24（基线 CSV 入库 + harness ctest PASS）⇒ **本 change 的功能实现可立即开工**；★但 **性能验收（Task 6 Step 4）另需**：`chmem-branch-microbench` 基线（增益度量）+ P-2 落地（绝对值判据）

### Task 1.5: ★实施期差距表 + 机制结论（2026-10-24 实测产出）

> **本节是 Task 1 的交付物**（plan 要求「输出一张表… 无表 ⇒ 不进实施」）。
PoC 代码：`tests/cpu/test_cpu_chmem_bp_poc.cpp`（**8 case / 51 assertions 全绿**，
> 归属 `chipforge_tests_chmem` binary）。

#### 差距表（现有 `PtWalkFsmPlugin` / `branch_predictor.h` → ADR-085/086 目标架构）

| 现有实现位置 | ADR-085/086 条款 | 差距描述 | 处置 | 落到 Task |
|---|---|---|---|---|
| `ip/cpu/plugins/mmu_ptw_chmem.h`（**不在 BP 范围**，仅记录） | D01/D04/D10/D18 | 裸 `ch_state_machine` DSL / 违规目录 / 无 PTWCache | **不属本 change** | ⑤ 批 1/2 |
| `ip/cpu/plugins/branch_predictor.h:150` `BranchPredictorPlugin`（**TLM 侧**，319 行） | ADR-088 §3 BP 行 | CH_MEM 侧**完全不存在**（符号数 0）⇒ **不是「移植」而是「CH_MEM-only 新写」** | **新写** `branch_predictor_chmem.h`（不复用 TLM 类的私有成员结构） | Task 2/3/4 |
| `branch_predictor.h:85` `kNumThreads`（TLM 模板参数） | ADR-040 §10.5 | CH_MEM 侧**无框架级多线程 TID 机制** ⇒ 3 条 TID 台账条目**无对应物** | **不实现**（台账显式 `not-covered` + 理由「CH_MEM 侧无框架级多线程 TID 机制」） | Task 7 Step 3 |
| `branch_predictor.h:48` `btb_entries`（运行时构造参数） | brief §3.3 | `ch_mem` depth 是**编译期模板参数** ⇒ 运行时改表项数**无载体** | **固定容量 + 运行时逻辑条目数**（读 0 ⇒ 未命中，PoC P-4 已证） | Task 2 Step 1 |
| —（新） | ADR-040 §10.6 | 生产者/消费者顺序契约 | BTB/PHT 更新走 **payload 驱动**（生产者写 payload，消费者 `bits<>` 派生地址） | Task 2/3 |
| —（新） | ADR-046 | 多周期协议引擎豁免 | **不需要**：BTB/GShare/RAS 全是**组合读 + 状态存储**，**无 FSM** ⇒ **不写 `CF_PLUGIN_USE_FSM_EXEMPT`** | Task 2/3/4 |

#### ★ PoC 机制结论（7 条，**对实现的硬约束**）

| # | 结论 | PoC 锚 | 对实现的约束 |
|---|---|---|---|
| 1 | **C++ 侧常量 ⇒ elaboration 期常量**（ADR-040 §10.5 的机制根源） | P-5 / P-5b | 预测方向/目标/写使能**必须**是 ch 节点，**禁** `static_cast<bool>` |
| 2 | **mem 端口地址必须来自「已被驱动」的信号** —— payload 派生**正确**（含逐拍变化）；**未被驱动**的 `ch_reg` proxy ⇒ 索引解析成 **0**（写到**错误表项** ⇒ 静默错乱）；★但**被 op 节点 `<<=` 驱动**的 reg（生产 `pc_reg <<= select(...)` 正是此形）**完全可行** | **P-2N（负例，已钉死）** / **P-2P（补测，关键）** / P-2 | BTB/PHT **写**地址 = `bits<...>(payload PC)`（payload 驱动）；BTB **读**地址 = `bits<...>(pc_reg)`（`pc_reg` 被驱动 ⇒ 可用）|
| 3 | **读/写共用同一地址表达式** | P-2 | BTB 读端口与写端口用同一 `bits<>` 表达式 |
| 4 | **enable 可以是 reg proxy**（与 addr 限制不同） | P-2B | 写使能可用 payload 里的真实 `BRANCH_TAKEN` |
| 5 | **2-bit PHT 可行** | P-3 | `ch_mem<ch_uint<2>, N>` 承载 2-bit 饱和计数器 ⇒ 体积不膨胀 |
| 6 | **越界索引读 = 0** | P-4 | 「固定容量 + 运行时逻辑条目数」成立 |
| 7 | **`ch_reg` 跨拍保持成立**（1→3→7） | P-1 | GHR 13-bit 全局历史用 `ch_reg` 可行（brief §5.2 P-1 待验项**通过**） |

#### ★ 分支信号源已核实（BP 更新的 payload 输入全部存在）

| BP 需要 | CH_MEM 现成来源 | 出处 |
|---|---|---|
| 分支 PC | `KT::PC`（branch stage） | `branch_chmem.h:167` `pc_val = n->operator()(KeyType::PC)` |
| 真实方向 | `BranchPlugin::BRANCH_TAKEN` | `branch_chmem.h:236` |
| 目标地址 | `RvKey::BRANCH_TARGET` | `branch_chmem.h:237` |
| PC-mux 接入点 | `pc_reg <<= select(br_taken, br_target, pc+4)` | `ibus_chmem.h` `update_pc`（`branch` LATE） |
| **预测路径的 PC** | `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ proxy 正常跟踪 ⇒ 可作 mem 读地址） | PoC **P-2P 通过** |

⇒ **BP 可落地**：在 fetch 期用 BTB 预测目标，产出 `predicted_target` / `predicted_taken`
payload；在 branch LATE 参与 PC-mux；branch stage 的真实信号驱动 BTB/PHT 更新。

### Task 2: BTB 实现（CH_MEM）

- [ ] **Step 1: 数据结构** —— 4096 entry；entry = tag + target + valid。CH_MEM 侧承载方式：`ch_mem`（数组）+ 组合读（`ch_mem` 的读是组合的 ⇒ 读 4096 项不可行 ⇒ 需**索引直寻**：`ch_mem` 支持索引访问 ⇒ 用 tag 数组 + target 数组双 `ch_mem`，或单数组打包）
- [ ] **Step 2: 索引函数** —— PC 相关位（对齐/折叠）⇒ 4K entry ⇒ 12-bit 索引；tag = 更高位
- [ ] **Step 3: 命中判定 + fallback** —— 未命中 ⇒ **static predict fallback**（feature 明写「BTB miss 走 static predict fallback」）⇒ 方向恒 not-taken
- [ ] **Step 4: payload key 模态** —— 预测结果（target / direction）用 `cf::plugin::uint_t<N>` key；方向信号必须是**活节点**（`ch_uint<1>(<ch_bool 表达式>)`）⇒ **不得** `static_cast<bool>`
- [ ] **Step 5: 单元测试（★v2：AC-3 出口写死，避免 agent 编造 case）** —— CH_MEM 是**静态 elaboration**（无运行时重配置面）⇒ `custom_btb_size` / `btb_runtime_{16,64,256}` 这类「运行时改表项数」判据在 CH_MEM 侧**无对应物**。**出口二选一（必须显式选一个，不得含糊）**：
  - (a) **不适用** —— 判「CH_MEM 静态 elaboration 无运行时重配置面」⇒ 台账标 `not-covered` + **显式手写理由**（⚠️ 台账红线：`check_coverage_parity_ledger.sh` 要求 not-covered 必须有显式理由、不得为继承标记、**同一理由 ≤10 次** ⇒ 这 4 条共享 1 个理由 = 4 ≤ 10 ✓ 合规）
  - (b) **改成编译期多实例** —— 4 个不同容量参数的模板实例各测一次 ⇒ 判 `covered`
  - ⇒ **建议 (b)**（保留可测性）；若 (b) 与 `ch_uint<N>` 静态宽度冲突 ⇒ 选 (a) 并显式登记。**无论哪种，都必须同步台账 §4 与 §2 族级表**

### Task 3: GShare 实现（CH_MEM）

- [ ] **Step 1: 全局历史寄存器** —— 13-bit，**跨拍保持** ⇒ `ch_reg<ch_uint<13>>`；置位时机 = 分支实际 taken（需拿到「本拍真实方向」⇒ 与 hazard/branch 的关系要理清）
- [ ] **Step 2: PHT** —— 2^13 项 × 2-bit 饱和计数器；索引 = `GLOBAL_HISTORY XOR PC_bits`
- [ ] **Step 3: 预测方向** —— PHT 读出 ⇒ 阈值判定 ⇒ **必须写成活节点**（禁 `if(ch_bool)` 运行期判断 ⇒ 用 select 树）
- [ ] **Step 4: 单元测试（★v2：补 oracle 的 RAS 角例）** —— 迁移 TLM 侧 GShare 判据（`predict_after_update` / `no_predict_initially`）+ 2-bit 饱和边界（0→1→2→3 不回退）；★**RAS 角例**（v1 遗漏）：**同拍 call + ret**（压栈与弹栈在同一拍）⇒ 必须先给明确行为定义（返回**后进先出**的旧值 or 本拍新值）⇒ 有 case 覆盖

### Task 4: RAS 实现（CH_MEM）

- [ ] **Step 1: 8-entry 栈** —— push（call）/`ch_reg` 数组 or `ch_mem` + 指针；pop（ret）
- [ ] **Step 2: 单元测试（★v2：理由修正）** —— 3 条 TID 条目（`d2_branch_predictor_multi_thread` / `d4_branch_predictor_tid_param` / `branch_predictor_set_tid_stores_tid`）测的是**框架级多线程 TID 机制**（`PluginBase::set_tid`），BP 只是被测对象之一 ⇒ CH_MEM 侧**无框架级 TID 机制** ⇒ 判「不适用」的理由必须写「**CH_MEM 侧无框架级多线程 TID 机制**」，**不是**「BP 无 TID 参数面」（v1 把框架机制与 BP 混为一谈）⇒ 台账显式标注，**不默默丢弃**
  - ⚠️ 同族另有 6 条 TID 条目（`plugin_base_set_tid_*` / `reg_file_set_tid_stores_tid` / `hazard_set_tid_stores_tid` / `pipe_builder_run_calls_set_tid_*`）为 `partial` ⇒ **同批重判**，否则台账自相矛盾

### Task 5: misprediction recovery（CtrlLink 消费）

- [ ] **Step 1: 接线** —— 预测错误 ⇒ 走 `CtrlLink::flush_when` 真实消费（feature 明写）
- [ ] **Step 2: ADR-040 §10.6 顺序契约** —— 生产者/消费者顺序：同一 stage 内 EARLY(生产者) 先于 NORMAL(消费者) ⇒ **不得依赖 plugin 注册顺序** ⇒ 用 `declare_substage` 或同 stage 名
- [ ] **Step 3: 验证** —— `check_plugin_portability.sh` 13/13 仍绿；新增 case 覆盖「预测错误 ⇒ flush 发生」

### Task 5.5: 容量与体积预算（★v2：oracle 补充，实施前必做）

- [ ] **Step 1: 先量 baseline** —— BTB 4K entry + PHT 2^13×2bit ⇒ `ch_mem` 阵列规模约 **16Kbit + tag 阵列** ⇒ 直接影响**生成 Verilog 的体积**与**verilator 编译耗时**（`dmem` 靠 `--public-flat-rw` 访问，`sim_main.cpp:16-25`）⇒ **实施前先量当前（无 BP）**的 Verilog 体积 + verilator 编译耗时作为 baseline
- [ ] **Step 2: 估算与判定** —— 加上 BP 阵列后的估算体积/耗时 vs baseline ⇒ 若超出可接受范围（**阈值由 Step 1 实测确定**，不预设）⇒ **上报 architect**（选项：减 PHT 表项 / 改 1-bit PHT / 分 Bank）

### Task 6: 工厂注册

- [ ] **Step 1: 注册** —— `ip/cpu/cpu_factory_chmem.h` 加 `BranchPredictorPluginChmem<T>`（**canonical ordering 位置须过 ADR-048 断言**）
- [ ] **Step 2: 无静默退化** —— 若某配置下 BP 不可用 ⇒ **fail-fast throw**（对齐 `enable_cache=true` 的既有风格），**不得**静默关掉
- [ ] **Step 3: ELF 级回归** —— `[chmem] chmem_vendored_elf_tohost1`（47 ELF）+ `[verilator]`（48 ELF）**0 退化**
- [ ] **Step 4: 性能验收（★v4：依 0c 微基准实测形态修订判据方向）** ——
  - **中间验收（不依赖 CoreMark）**：用 `chmem-branch-microbench` 的四档负载测「启用 BP vs 禁用 BP」的 **cycles/iter 降幅**
  - **★判据方向（0c 实测纠正）**：CH_MEM 侧的分支代价与**方向变化次数**相关（0c 基线：方向恒定的 m0=66034 / m1=68034 **最少**，方向变化的 m2=79534 **最多**，伪随机 m3=73012 居中）⇒ **BP 的收益空间在「方向变化」档** ⇒ 判据 = 「**方向变化频繁**的档（m2/m3）Δcycles **显著大于**方向恒定档（m0/m1）」；**不是** v1 假设的「恒 taken 收益最大」
  - **★对外接口需求**：本 change **必须提供「可关闭 BP 的配置面」**（如 `CPUConfig` 字段）⇒ 否则「启用/禁用 BP 对比」无法做
  - **落地后重录 0c 基线** —— 0c 的 CSV 是**无 BP 参照值** ⇒ ② 落地后须用同一脚本重测并**显式重录**（附 commit 说明）
  - **绝对值判据**（「CoreMark/MHz ≥1.9」）属**交付点 B** ⇒ 本 change **不宣告绝对值达标**；且依 **P-6**，CH_MEM 的绝对值达标还须先有 ② 本身
  - **中间验收（不依赖 CoreMark）**：用 `chmem-branch-microbench` 的**四档可控预测率负载**测「启用 BP vs 禁用 BP」的 **cycles/iter 降幅** ⇒ 判据 = 「`pattern_6of8` 档降幅显著（阈值由 microbench 基线实测确定）」+「**四档单调性成立**（`always_taken` 收益最大 → `pseudo_random` 最小）」⇒ **单调性破坏 ⇒ BP 实现有 bug**（这是本 change 的核心验收锚）
  - **★对外接口需求**：本 change **必须提供「可关闭 BP 的配置面」**（如 `CPUConfig` 字段）⇒ 否则「启用/禁用 BP 对比」无法做（microbench plan R3 已登记此依赖）
  - **绝对值判据**（「CoreMark/MHz ≥1.9」）⇒ 属**交付点 B**（`chmem-verilator-coremark-harness`）⇒ 本 change **不宣告绝对值达标**；且依 **P-2**，CH_MEM 的绝对值达标还须先补 load-use 通路（DMIPS 1.24467 → ≥1.4）
  - ⇒ 本 change 的性能 AC = 「**中间验收（microbench 增益 + 单调性）**」⇒ **不被 CoreMark 移植的不确定性阻塞**（P-5 的目的）
  - **开工前置**：① 必须**已合入并入库基线 CSV**（D4「两个 change + 硬依赖」）⇒ ① 未合入则**本 Task 不得开工**（Task 1–5 可先做）
  - **测量**：用 ① 的 harness 跑 CoreMark ⇒ `CoreMark/MHz` **实测值**记入基线 CSV（mode=`verilator+bp`）⇒ 与**无 BP** 的同条件实测对比 ⇒ 记录增益
  - **达标判定**：**「CoreMark/MHz ≥ 1.9」在 CH_MEM 后端不成立为默认判据**（cycle 不可比 ⇒ ADR-088 §3 M 扩展行同源裁决）⇒ 本 Task 的判据是「**产出可比实测值 + 增益可量化**」，而**不是**「≥1.9」；「≥1.9 是否仍作为门禁」由**判据重校准 ADR**（owner = architect，窗口 = ① 合入后、本 change 开工前）裁决 ⇒ ADR 未出时**只登记实测值，不宣告达标**（AC-8 的可判定性由此保证）

### Task 7: 收口

- [ ] **Step 1: 8 门禁** —— 全绿（`check_plugin_portability.sh` 的 Check 5/7/13 是本 change 的主要抓手）
- [ ] **Step 2: 全量回归** —— `run_chipforge_tests.sh --all` ⇒ 0 退化
- [ ] **Step 3: 台账更新（★v2：条数修正 + 族级同步）** —— **14 条** BP 相关条目逐条重判（10 条 `not-covered` + 4 条 `partial`）：10 条 → `covered`（CH_MEM 锚 = `branch_predictor_chmem.h` + 新 case + 实测）；4 条 `partial` → `covered`/`partial`（视 CH_MEM 是否实现对应语义）；3 条 TID → `not-covered` + 「CH_MEM 侧无框架级多线程 TID 机制」显式理由。
  - ⚠️ **必须同时改台账 §2 族级表**（`cpu` 族判定行）—— 条目级三态是**从族级继承**的（台账 §2 注记）⇒ 只改条目不改族级 ⇒ 台账自相矛盾，且 `check_coverage_parity_ledger.sh` **只查条目不查族级** ⇒ 漂移会静默通过
  - ⇒ 重判判据：**14 条条目态 + §2 族级判定行** 同时更新，且 `check_coverage_parity_ledger.sh` 仍 PASS
- [ ] **Step 4: ADR-088 §5.2 BP 双前置状态** —— 记录「① 已达成 / ② 已达成」⇒ B3 删除门禁的 BP 阻塞解除
- [ ] **Step 5: commit** —— 分批：① CH_MEM BP 组件 ② 工厂注册 ③ 测试 ④ 台账/ADR 同步

---

## Acceptance

- [ ] AC-1: `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 BTB 4K + GShare 13-bit PHT(2-bit) + RAS 8-entry
- [ ] AC-2: `cpu_factory_chmem.h` **已注册** `BranchPredictorPluginChmem`（`grep` 可验），且 canonical ordering 断言 PASS
- [ ] AC-3: ★**BP 相关 14 条台账条目全部重判完毕**（10 条 `not-covered` + 4 条 `partial`：`not_taken_returns_zero` / `multiple_addresses` / `reset` / `per_thread_ghr_isolation`）⇒ 每条有明确态 + CH_MEM 锚点 + 显式理由；且台账 **§2 族级判定行同步更新**（族级继承规则）
- [ ] AC-4: BTB miss ⇒ static predict fallback（未命中不崩、方向恒 not-taken）
- [ ] AC-5: 预测方向与目标为**活节点**（cell `impl()->is_const() == false`）⇒ 不得 `static_cast<bool>`（ADR-040 §10.5）
- [ ] AC-6: misprediction ⇒ CtrlLink flush **真实消费**（有 case 证明 flush 发生）
- [ ] AC-7: `[chmem]` + `[verilator]` 全量 **0 退化**（47 + 48 ELF）
- [ ] AC-8: ★**中间验收**（`chmem-branch-microbench` 四档）：① **方向变化频繁**的两档（`pattern_6of8` / `pseudo_random`）的 cycles/iter 降幅**可量化**（阈值由 0c 微基准基线确定）② **方向恒定**的两档（`always_taken` / `always_not_taken`）降幅**显著更小**（★**机制化判据**—— 0c 实测证明：方向恒定档本就无「方向变化惩罚」⇒ BP 的收益空间小；**0c 实测的基线形态**：恒定档 66034/68034 cycles 反而**最少**，变化档 79534 最多 ⇒ **v1 的「恒 taken 收益最大」是错的**）③ 若 BP 无效，四档降幅应 ≈0 ④ 本 change **提供「可关闭 BP 的配置面」**。**绝对值判据（CoreMark ≥1.9）不属本 change**（交付点 B + ADR-088 v1.2 P-6）⇒ 本 change **不宣告绝对值达标**
- [ ] AC-9: 8 门禁全 PASS + 全量回归 0 退化
- [ ] AC-10: 台账 **14 条** BP 条目重判完毕 + **§2 族级判定行同步** + ADR-088 §5.2 BP 双前置状态更新
- [ ] AC-11: **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，且需双前置）

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | ~~CH_MEM 组合通路下 BTB/GShare 的「跨拍状态」语义不同~~ → **v2 降级（oracle 取证）**：框架先例 `csr_chmem.h:499` 的 `mcycle_ <<= mcycle_+1` 就写在 at_stage(fetch,NORMAL) 闭包内 ⇒ CH_MEM 下 at_stage 闭包在 `elaborate()` 期**执行一次**建立 DAG 边 ⇒ **天然满足**「`<<=` 须在 Simulator 创建前」⇒ GShare 13-bit 全局历史用 `ch_reg` **可行** | 保留 **Task 3 Step 1 的最小 PoC** 作 diligence（非阻塞门禁）；若 PoC 失败 ⇒ 按 R6 的触发阈值上报 |
| R2 | PHT 2^13 × 2-bit 在 Verilator 生成后**规模**（≈16Kbit）⇒ 仿真速度与 Verilog 体积 | 规模可接受（PHT 2-bit × 8192 = 16Kbit）⇒ 但需在 harness 上量仿真耗时；若过慢 ⇒ 与 architect 议（减表项 or 改 1-bit） |
| R3 | 预测错误的 recovery 需要「真实方向」信号 ⇒ 与 hazard/branch 的数据流耦合可能踩 ADR-040 §10.6 顺序契约 | Task 5 Step 2 显式核对；生产者用同 stage 名 EARLY、消费用 NORMAL |
| R4 | **CoreMark/MHz 可能达不到 1.9**（CH_MEM 全组合的 cycle 数与 TLM 不可比 ⇒ 数值不可直接对标） | AC-8 已设计为「记录实测值，达标判定依 ADR」⇒ 避免用不可比的数值下结论 |
| R5 | 3 条 TID 条目（+ 同族 6 条 `partial`）在 CH_MEM 侧无对应（**无框架级多线程 TID 机制**） | 显式标注 + 台账写明差异 ⇒ **不静默丢弃**；⚠️ 台账红线「同一理由 ≤10 次」⇒ 这批共享 1 个理由（≤9 条）合规，但**不得**扩大化 |
| R6 | **兜底句会诱发 AI 静默挂起**（v1 常见：「登记为决策」「报 architect」「与 architect 议」） | ★每处兜底补**触发阈值 + 落点 + 时限**：GShare PoC 不可行 ⇒ **触发** = Task 3 Step 1 的 PoC 连续 2 次失败 ⇒ **落点** = ADR-088 §3 追加一行 + 本 change Task 记录 ⇒ **时限** = 本 change 内必须闭环，不得挂起等回复 |
| R7 | ~~**CH_MEM 的 load-use 通路缺失**（原假设：纯 stall 无旁路 ⇒ DMIPS 1.24467 < 1.4）⇒ 绝对值性能验收受阻~~ → **★前提已被 PoC 证伪（2026-10-24）**：禁用 CH_MEM 侧 RAW stall 后 **cycles 完全不变（914544）** ⇒ load-use **不是**根因 ⇒ **P-2 `chmem-load-use-bypass` 已作废，不立项**。**真根因 = CH_MEM 无分支预测**（ADR-088 v1.2 §5.3.1：比率随分支密度单调升 add 1.037 / jal 1.078 / beq 1.098 / Dhrystone 1.151）⇒ **与本 change 同源** | **本 change 即根因的解法** ⇒ 绝对值 1.4 门禁的恢复条件 = **本 change 落地**（ADR-088 v1.2 §5.3.2 **P-6**）⇒ 性能 AC 走 **microbench 中间验收**（P-5，绝对值由本 change 的副产品恢复）
| R7 | 「不适用 §5.2『不做』出口」意味着 B3 时 TLM BP **必须**能删 —— 若本 change 延期，B3 会被阻塞 | 把本 change 排在 ① 之后立即执行；进度风险已在 ADR-088 §3 BP 行登记 |

## 迁移说明

落地为 `.rddf/plans/chmem-bp-btb-gshare.md`；建议**完整 rdd-builder P1–P3**（规模大 + 触及 CH_MEM 组合通路的架构假设 + 需 architect 参与 R1/R2 判定），不走 rdd-quick。

---

## 实施期修正（2026-10-24，Task 1 产出）

| # | plan v1 的设计 | 实测问题 | 修正 |
|---|---|---|---|
| 1 | Task 2 Step 5「单元测试出口二选一：(a) 不适用 / (b) 编译期多实例」 | **`ch_mem` depth 是编译期模板参数** ⇒ (b) 的「不同容量模板实例」在 `ch_mem<...,N>` 上**可行但代价 = 每实例一份数组**；而台账条目 `custom_btb_size` / `btb_runtime_*` 的语义是「**运行时**改表项数」⇒ 即便 (b) 也**不满足**「运行时」语义 | 出口定为 **(a) 不适用** + **显式手写理由**（`ch_mem` 编译期固定 ⇒ CH_MEM 静态 elaboration 无运行时重配置面）⇒ 4 条 `not-covered`（10 条中的 `custom_btb_size`/`btb_runtime_{16,64,256}` 实为 3 条 + `not_taken_returns_zero` 等另计）⇒ 台账红线「同一理由 ≤10 次」：BP 的 10 条按 4 组差异化理由写 |
| 2 | Task 6 Step 4「提供『可关闭 BP 的配置面』（**如 `CPUConfig` 字段**）」 | **CH_MEM 工厂不接 `CPUConfig`**（`cpu_factory_chmem.h:95-107` 显式「不读 cfg, 不引 CpuConfig 依赖」= ADR-088 §9.1 NG1-3）⇒ **照抄会挂** | 改为**工厂参数 + CLI flag 双层**（`build_cpu(..., std::optional<bool> enable_bp)` + `cpu_verilator_sim --enable-bp/--no-bp`），且**用普通 `bool` 不用 `optional<bool>`**（避免 nullopt 与「显式关闭」语义撞车） |
| 3 | Task 3 Step 1「GHR 用 `ch_reg`（跨拍保持）」 | 未验证 | **PoC P-1 通过**（1→3→7）⇒ 结论成立，**无需降级** |
| 4 | Task 5 Step 1「misprediction ⇒ `CtrlLink::flush_when`」 | CH_MEM 的 `flush_when(Condition)` **无有状态重载**（`ctrl_link.h:110` 只有 `Condition` 版；有状态版只有 `halt_when_stateful`）⇒ misprediction flush 信号若需跨拍保持**无对应 API** | misprediction 的 flush 信号按**组合路径**设计（misprediction 当拍即知）⇒ 不需要跨拍保持 ⇒ 用 `flush_when(ch_bool)` 即可 |
| 5 | Task 5.5「先量 baseline（Verilog 体积 + verilator 编译耗时）」 | 未执行 | **Task 1.5 已查 `ch_mem` 是真数组存储**（`memimpl.h:27` `init_data_` vector + depth）⇒ Verilog 里是 `reg [N-1:0] mem [depth-1:0]` ⇒ **4096×32bit = 16Kbit reg array，体积可控**（不展开成 mux）⇒ 预算风险等级从「高」降为「中」（仍需实测编译耗时） |

### Task 1 实测留证（2026-10-24，branch `feature/chmem-bp-btb-gshare`）

| 项 | 实测 |
|---|---|
| PoC 文件 | `tests/cpu/test_cpu_chmem_bp_poc.cpp`（**8 case / 51 assertions 全绿**） |
| PoC 归属 | `chipforge_tests_chmem` binary（CH_MEM 专属 ⇒ 需 `#ifdef CF_PLUGIN_USE_CH_MEM` **整体 guard**，且 guard 必须在**所有 include 之前**，否则 TLM binary 会编译 CppHDL 头 ⇒ 符号重复定义） |
| 探索规模 | 实施期共跑 **21 个对照 case**（含 A/B/C 三变体 + 4 决定性对照 + 逐拍观测 + 全 entry 扫描 + 驱动边界补测）⇒ 收敛为最终 **8 个**（保留有决策价值的 + 钉死 1 个负例） |
| 机制结论 | **7 条**（见 Task 1.5 表）⇒ 核心 = **mem 端口地址必须来自「已被驱动」的信号**（payload 派生 ✓；未被驱动的 reg ⇒ 索引 0 ⇒ 静默错；**被 op 节点驱动的 reg ✓**） |
| 分支信号源 | 已核实齐备（`KT::PC` / `BRANCH_TAKEN` / `BRANCH_TARGET` / `update_pc` 的 PC-mux） |
| 差错误判（已更正） | 实施中曾判定「套件内存在测试间干扰」⇒ **该判断错误**（行号映射错误）；事实 = **套件失败集合 == 单跑失败集合** ⇒ 无干扰 |
