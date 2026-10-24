# ② chmem-bp-btb-gshare — 执行文件

> **change**: `chmem-bp-btb-gshare`
> **规模**: 大 · **前置**: ① `chmem-verilator-perf-harness`（**硬依赖** —— 本 change 的核心 AC「CoreMark/MHz ≥1.9」需要 ① 的测量链路）
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

- [ ] **Step 1: 读 TLM 版** —— `ip/cpu/plugins/branch_predictor.h`（319 行）+ `.cpp`（23 行）⇒ 提取**可测语义**：BTB 索引/替换策略/tag 比较、GShare 13-bit 历史移位与 PHT 2-bit 饱和计数、RAS 8-entry push/pop、misprediction 时的恢复动作
- [ ] **Step 2: 提取 BP 判据全集（★v2 修正：v1 误引台账 §7 且漏 4 条）** —— BP 相关条目共 **14 条**，分布在台账 **§4 条目区**（不是 §7 裁决表）：
  - **10 条 `not-covered`**（§4 行 264–266, 271–274 + 行 349, 351, 357）：`no_predict_initially` · `predict_after_update` · `btb_entry` · `custom_btb_size` · `btb_runtime_16` · `btb_runtime_64` · `btb_runtime_256` · `d2_branch_predictor_multi_thread` · `d4_branch_predictor_tid_param` · `branch_predictor_set_tid_stores_tid`
  - **4 条 `partial`**（§4 行 267–270，**v1 完全漏掉**）：`not_taken_returns_zero` · `multiple_addresses` · `reset` · `per_thread_ghr_isolation`
  - ⇒ **14 条全部要重判**（10 条降级 + 4 条升级方向）；行号以台账实际为准，实施时 grep 复核（pattern: `^\| \`(btb_|predict|no_predict|not_taken|multiple_addresses|reset|per_thread_ghr|d2_branch_predictor|d4_branch_predictor|branch_predictor_set_tid)`）
- [ ] **Step 3: 确认 canonical ordering** —— BP 在 fetch 路径 ⇒ 查 ADR-048 的注册规范序 + `cpu_factory_chmem.h` 现有 8-plugin 顺序 ⇒ 定 BP 的注册位（**必须**用 `tools/verify_plugin_decision.sh` 验证）
- [ ] **Step 4: 确认 ① 已合入** —— ① 的基线 CSV 入库 + harness ctest PASS ⇒ **否则停止**（硬依赖）

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
- [ ] **Step 4: 性能验收（★v2：依赖顺序 + 达标判定口径写死）** ——
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
- [ ] AC-8: CoreMark 在 `cpu_verilator_sim` 产出 `CoreMark/MHz` **实测值**并入库；**达标判定 = 「产出可比实测值 + 相对无 BP 的增益可量化」**（不是「≥1.9」—— 后者待**判据重校准 ADR** 裁决）；ADR 未出 ⇒ **只登记实测值，不宣告达标**（判据不依赖未决事项）
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
| R7 | 「不适用 §5.2『不做』出口」意味着 B3 时 TLM BP **必须**能删 —— 若本 change 延期，B3 会被阻塞 | 把本 change 排在 ① 之后立即执行；进度风险已在 ADR-088 §3 BP 行登记 |

## 迁移说明

落地为 `.rddf/plans/chmem-bp-btb-gshare.md`；建议**完整 rdd-builder P1–P3**（规模大 + 触及 CH_MEM 组合通路的架构假设 + 需 architect 参与 R1/R2 判定），不走 rdd-quick。
