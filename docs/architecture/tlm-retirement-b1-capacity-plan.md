# TLM 退役 B1 承载力拆解（TLM Retirement — B1 Capacity Plan）

> **来源**: ADR-088（冻结契约 + 语义裁决锚）· `docs/architecture/tlm-coverage-parity-ledger.md`（463 条覆盖台账，§5 已给 11 项 B1 待办）· 本文件的**实测盘点**（2026-10-24，HEAD `84ab102`）
> **性质**: ~~规划输入~~ → **已定案**（2026-10-24，architect 主持）：§5 = B1 决策定案 + change 立项清单，可据此逐项立项实施。
> **配套**: ADR-088 v1.1（§3 五行语义表态 + §5.2 BP 专项前置）· ADR-087 v1.1（§D4 护栏切换）
> **对应路线图**: `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B1 段）· `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G2 门禁）

---

## 0. 一句话结论

CH_MEM 侧与 TLM 侧的**注册差距 = 10 处**（TLM 工厂 19 处 `register_plugin` vs CH_MEM 工厂 9 处）。这 10 处可分三类：**接线缺失**（实现已在，只需接线 ⇒ 最便宜）、**实现缺失**（大工程）、**架构决策**（CH_MEM 组合式可能根本不需要）。**B1 的正确做法是先把「接线缺失」清零，再逐项决策「实现缺失 / 架构决策」** —— 否则会用 4 个大工程掩盖 3 个一行修复。

**定案后修正（2026-10-24，architect）**：① **C1（StageLink）从缺口清单移除**（CH_MEM 框架有 `auto_insert_stage_regs()` 且实测 **0 调用** ⇒ 组合式无需跨拍传播）；② **MMU 缺口范围扩大**（实测 CH_MEM 工厂 `enable_mmu=true` 是 plumbing-only no-op ⇒ MMU **流水线级完全未接线**，不止「PTW 未注册」）；③ **A3 议题改写为 A3'**（CH_MEM 工厂**不接受 `CPUConfig`** ⇒ 原前提不成立）。详见 §5。

## 1. 实测盘点：CH_MEM 工厂注册了什么

`ip/cpu/cpu_factory_chmem.h` 实测注册 9 处（`grep -c register_plugin`）：

| 已注册 | 备注 |
|--------|------|
| `RiscvDecodePluginChmem` | 译码 ✅ |
| `HazardPlugin` | 冒险 ✅ |
| `IBusPlugin` | 取指 ✅ |
| `RegFilePlugin` | 寄存器堆 ✅ |
| `RiscvIntAluPlugin`（19 层 select 树，含 8 条 M-op） | ALU ✅ |
| `RiscvCsrPluginChmem`（566 行） | CSR ✅（TLM 侧是纯 stub） |
| `Branch` | 分支判定 ✅ |
| `DMemPlugin` | 数据存储 ✅ |
| `MmuExceptionHandlerPlugin` | ⚠️ **在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 ⇒ CH_MEM 模式实际不注册** |

**TLM 侧独有的 plugin 类**（`grep -oE "plugins::[A-Za-z]+Plugin" ip/cpu/cpu_factory.h`）：
`BranchPredictorPlugin` · `DBusPlugin` · `HazardPlugin` · `IBusPlugin` · `MmuExceptionHandlerPlugin` · `RegFilePlugin` · `RiscvMMUPlugin` · `StageLinkPlugin`

⇒ 差集 = **BP / DBus / StageLink**（无 CH_MEM 变体）· **MmuExceptionHandler**（有变体未接线）· **RiscvMMUPlugin**（CH_MEM 有 `MMUPluginChmem` 变体，但语义不同：TLM 全 walk vs CH_MEM TLB-only）

## 2. 缺口分类与建议处置

### 2.1 类 A：接线缺失（实现已在，只差注册/接线）—— 建议 B1 首批

| # | 缺口 | 实测证据 | 预估 | 建议 |
|---|------|---------|------|------|
| A1 | **MMU exception handler 未注册** | `ip/cpu/cpu_factory_chmem.h:342-349` 的注册块被 `#ifndef CF_PLUGIN_USE_CH_MEM` 包住；CH_MEM 变体 `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行）**已存在** | 极小（删 `#ifndef` + 补 include + 回归异常路径） | 独立 change：`chmem-mmu-exception-handler-register` |
| A2 | **MMU 流水线级未接线**（PTW + MMU 本体 + exception handler 三者同批） | ⚠️ 实测扩大：`cpu_factory_chmem.h:86` 自述 `enable_mmu=true` **不注册任何 MMU plugin**（"NO translation occurs"）⇒ MMU 在 CH_MEM 侧**仅单元级存在**（`[mmu-chmem-basic]` 直接构造，不走工厂）。PTW FSM（`mmu_ptw_chmem.h`）0 注册；exception handler 被 `#ifndef` 排除 | 大（MMU 本体接线 + PTW + 异常回传；依 ADR-088 §3 PTW 行「现在不接线」，实装归 `chmem-ptw-walk`） | **归 `mmu-chmem-phase-b-c-launch` / `chmem-ptw-walk` owner 裁决**（不新立 change） |
| ~~A3~~ → **A3'** | ~~isa/xlen 契约校验未接~~ → **CH_MEM 工厂无 config 入口** | ⚠️ 实测纠正：CH_MEM 工厂签名是散装 `std::optional` 参数（`cpu_factory_chmem.h:96-110`），**不接受 `CPUConfig`**（注释自述「不读 cfg, 不引 CpuConfig 依赖 NG1/NG2/NG3」）⇒ 原议题前提不成立 | **不立项**：ISA 契约覆盖面**限定为 TLM 侧 `CpuFactory`**（ADR-087 永久 spec 已补限定）；CH_MEM 侧约束走**编译期** `static_assert`（ADR-087 §D4，归 Change 2） | 见 §5 A3' 定案 |

> A3 需先由 architect 明确：**CH_MEM 工厂的 `CPUConfig` 是否接受 `isa`**（当前 `:99` 不读）—— 若接受，契约校验可直接复用；若不接受（CH_MEM 固定 RV32IM），则应**显式拒绝** `isa != rv32im` 而不是静默忽略。

### 2.2 类 B：实现缺失（大工程）—— 建议逐项独立立项

| # | 缺口 | TLM 侧规模 | CH_MEM 侧现状 | 依赖/风险 |
|---|------|-----------|--------------|----------|
| B1 | **BP（BTB/Bimodal/GShare）** | `ip/cpu/plugins/branch_predictor.h` 319 行 + `.cpp` 23 行 | **无变体**（`branch_chmem.h` 只是分支判定，不含预测器） | 硬依赖：CoreMark PoC-6 的被测对象 ⇒ 删除 TLM 前必须先有 CH_MEM 版（否则性能基线无处可测） |
| B2 | **DBus（宽/窄访存）** | `ip/cpu/plugins/dbus.h` 150 行 + `.cpp` 20 行 | **无变体**（`DMemPlugin` 取代，但宽度语义未对齐） | 与 B3（L1Cache 本体）耦合：DBus 的命中判定依赖 Cache |
| B3 | **L1CachePlugin 本体** | `ip/cache/plugins/L1CachePlugin.h` 228 行 + `.cpp` 285 行 | **只有 refill FSM**（`l1_cache_refill_fsm_chmem.h` 229 行） | 连带 `MMUCacheIntegration` 4 条 not-covered 台账条目；**最高价值缺口** |
| B4 | **LSU** | `ip/cpu/arch/riscv/lsu.h` 82 行 | **无变体** | 与 B2 合并考虑（访存路径同源） |

### 2.3 类 C：架构决策（CH_MEM 组合式可能根本不需要）—— **需 architect 裁决**

| # | 议题 | 事实 | 决策问题 |
|---|------|------|---------|
| ~~C1~~ | ~~**StageLinkPlugin**~~ | ✅ **已裁决：不迁**（且非缺口）—— CH_MEM 框架有 `auto_insert_stage_regs()`（实测 0 调用），手写 StageLink 无对应需求；未来需时序流水走框架 API | （已闭合，见 §5 C1 行） |
| C2 | **多级 TLB / TLB 抽象**（`ip/mmu/lib/{multi_level_tlb,tlb,tlb_base,tlb_entry,tlb_factory,tlb_lookup}.*` ≈ 500 行） | CH_MEM 用 8 项全相联 TLB（`MMUPluginChmem`） | CH_MEM 是否需要多级 TLB？若否 ⇒ B3 删除时该族整体退役（台账 22 条 `partial` 需重判） |
| C3 | **3/7/10-stage 流水线拓扑** | CH_MEM 仅 5-stage（且全组合） | CH_MEM 是否需要多级流水形态？若否 ⇒ 大量 `[cpu-integration]` 台账条目由 `partial` 转 `not-covered`（需显式 ADR） |
| C4 | **M 扩展多周期语义** | TLM radix-2（MUL=1c/MULH=3c/DIV=33c）vs CH_MEM 组合（1 拍），差 ~286c，**原 cycle 判据失效** | 「CH_MEM 为规范」还是「补 FSM 路径」？（ADR-088 §3 第 1 项，B2 正式表态） |

## 3. 建议的 B1 执行顺序

```
第 1 批（低成本、高确定性，建议立即立项）
  A1 exception handler 注册  →  A3 isa 契约校验  →  A2 PTW 接线（依赖 B2 的 PTW 裁决）
第 2 批（实现缺失，需独立设计 + 承载力验证）
  B3 L1Cache 本体（最高价值，连带 4 条 not-covered）  →  B2/B4 DBus+LSU  →  B1 BP（性能基线依赖）
第 3 批（架构决策落地后）
  C1~C4 裁决 → 台账重判（463 条中 ~250 条 `partial` 需按裁决结果重判）→ B3 删除的判据就绪
```

**依据**: 台账 §5 的 11 项 B1 待办与本文件的实测盘点**逐项对齐**（台账给「缺什么」，本文件给「为什么缺 + 多大 + 谁决策」）。

## 4. 与 B2 的耦合

| B2 裁决项（ADR-088 §3） | 阻塞的 B1 工作 |
|---|---|
| MMU PTW 语义（TLM 完整 walk vs CH_MEM 回退 vaddr） | A2（PTW 接线） |
| CSR 语义（TLM stub vs CH_MEM 实装 ⇒ 规范取 CH_MEM） | 台账 `[cpu]` 137 条中 CSR 相关条目的最终判定 |
| M 扩展 cycle 语义 | C4 + 台账 `[mul-div-fsm]` 13 条 |
| BP | B1（是否值得补 CH_MEM 版） |
| MMU exception handler | A1（接线即可，但语义归属要一致） |

⇒ **B2 应先于 B1 的第 2 批启动**（否则会出现「按错误的语义假设补实现」）。

## 5. B1 决策定案（2026-10-24，architect 主持）

> ⚠️ **前提修正（实测，2026-10-24）**：本节 v1.0 曾问「CH_MEM 工厂的 `CPUConfig` 是否接受 `isa`」—— 实测该问题**前提不成立**：CH_MEM 工厂签名是散装 `std::optional` 参数（`ip/cpu/cpu_factory_chmem.h:96-110`），注释自述「不读 cfg, 不引 CpuConfig 依赖 (NG1, NG2, NG3 显式声明)」⇒ **CH_MEM 工厂根本没有 config 入口**。故原 A3 议题改写为 **A3'**（见下）。另实测 CH_MEM 工厂的 `enable_mmu=true` 是 **plumbing-only no-op**（不注册任何 MMU plugin）⇒ MMU 在 CH_MEM 侧**流水线级完全未接线**（缺口范围大于原判的「PTW 未注册」）。

| # | 决策问题 | **定案** | 依据 |
|---|---------|---------|------|
| **A3'** | CH_MEM 侧如何承接 ISA 契约？ | **CH_MEM 工厂不接受 `CPUConfig`，故 ISA 契约的「工厂级校验」在 CH_MEM 侧无对应物。** 裁决：① CH_MEM 侧**不引入** config 入口（维持 NG1–NG3 的显式声明）；② ISA 契约的覆盖面**限定为 TLM 侧 `CpuFactory`**（已在 ADR-087 永久 spec 中补此限定）；③ CH_MEM 侧的 ISA 约束表达为**编译期**：`CpuFactoryChmem<T>` 的 `T` 由调用方决定，`static_assert` 护栏（ADR-087 §D4）即 CH_MEM 侧契约载体 —— 归 Change 2 | ADR-088 §9.1 实测行 · ADR-087 §D4 |
| **C1** | StageLinkPlugin 迁不迁？ | **不迁**（且不需独立决策项）。依据：① TLM 的 `stage_link.h` 是**手写**的 stage 间 Payload 传播；② CH_MEM 侧框架提供 `PipeBuilder::auto_insert_stage_regs()`（ADR-040 §v2.0 新增 4），且**实测全仓 0 调用** ⇒ CH_MEM 当前流水线**无 stage 寄存器**（全组合）；③ 故 CH_MEM 侧既不需要手写 StageLink，也不需要自动插入（组合式无跨拍传播需求）。**若未来需要多级时序流水**，走框架的 `auto_insert_stage_regs()`（不是迁 StageLinkPlugin）⇒ 该项**从 B1 缺口清单移除**（转 C3 的下属问题） | `auto_insert_stage_regs` 全仓 0 命中 · ADR-040 §v2.0 新增 4 |
| **C2** | CH_MEM 是否需要多级 TLB？ | **要 —— 但架构已由 ADR-085/086 锁定，实装归未立项 `chmem-ptw-walk`**，不在 B1 另立方向。ADR-085 D01 已定 `MMU_TLB` + `MMU_PTW` 拆分、D04 `PTWCache`、D10 `FsmBase` 封装、**D18 代码落 `ip/mmu/plugins/`（修 ADR-083 目录违规）** ⇒ CH_MEM 侧 MMU 的目标架构是「多级 TLB + PTW walk」，当前 8 项全相联 TLB 是**过渡形态**。**B1 动作**：不新建 TLB 结构，等 `chmem-ptw-walk` 按 ADR-085/086 落地 | ADR-085 D01/D04/D10/D18 · ADR-086 · ADR-088 §3 PTW 行 |
| **C3** | CH_MEM 是否需要 3/7/10-stage 拓扑？ | **不阻塞 B3；降为「性能优化项」而非「退役前置」**。依据：① CH_MEM 是**全组合通路**（无 stage 寄存器）⇒ 多级拓扑在 CH_MEM 侧的语义 = 「插入 stage 寄存器」（走 `auto_insert_stage_regs()`），属**时序/面积**优化，不改变 ISA 语义；② 性能门禁（DMIPS ≥1.4 / CoreMark ≥1.9）的**载体是 Verilator 后端**（B1 item 1 重建测量链路），不是「TLM 的 3/7/10-stage 配置」；③ 故 `[cpu-integration]` 中的 3/7/10-stage 条目在 B3 时按「**拓扑形态不迁移**」处置，但**须在台账显式记录**（不是静默退役）。**B1 动作**：无（不实现多级）；**B3 前置**：台账该批条目标 `not-covered` + 理由「CH_MEM 单形态 5-stage（全组合），多级拓扑为 TLM 侧 DSE 能力」+ 显式 ADR 认可 | `cpu_factory_chmem.h:3` 自述「8-plugin 5-stage」· ADR-088 §5.2 判据 2 |
| **C4** | M 扩展语义规范取哪边？ | **CH_MEM 组合式为规范（功能 + cycle 均是）** —— 见 ADR-088 §3 M 扩展行（含降级项：`chmem-mext-fsm-cycle-parity` 转可选演进；`rv32um-baseline-matrix.csv` 降为 TLM-only 历史基线） | ADR-088 §3 · oracle 取证（`int_alu_chmem.h:324-334` vs `mul_div_fsm.h:144-148`） |
| **B1/B3 先后** | BP 与 L1Cache 本体谁先？ | **BP 先（与其前置测量链路同批），L1Cache 本体次之。** 依据：① BP 是 **CoreMark/MHz ≥1.9 硬门禁的唯一载体**（已承诺的路线图门禁，判「不做」= 门禁孤儿化）；② BP 的验收需要 Verilator 侧 CoreMark harness（B1 item 1），两者**必须同批**否则无法验证；③ L1Cache 本体覆盖面更大（连带 4 条 `not-covered`）但**不阻塞任何已承诺门禁** ⇒ 排第二 | ADR-088 §3 BP 行 + §5.2 BP 专项前置 |

### 5.1 由本定案产生的 B1 change 清单（建议立项顺序）

| 顺序 | change（建议名） | 内容 | 依赖 | 规模 |
|---|---|---|---|---|
| 1 | `chmem-verilator-perf-harness` | Dhrystone/CoreMark harness 移植到 `cpu_verilator_sim --elf` 路径 + 基线 CSV 重建（**BP 验收的前置**） | 无 | 中 |
| 2 | `chmem-bp-btb-gshare` | `branch_predictor_chmem.h`（BTB/Bimodal/GShare）+ CH_MEM 工厂注册 | #1（验收依赖） | 大 |
| 3 | `chmem-l1cache-本体` | L1CachePlugin CH_MEM 本体（当前只有 refill FSM）+ 工厂 `enable_cache=true` 解禁 | 无（可并行 #1） | 大 |
| 4 | `chmem-dbus-lsu` | DBus / LSU 的 CH_MEM 通路（宽度语义对齐） | #3（命中判定依赖 Cache） | 中 |
| 5 | `chmem-mmu-pipeline-integration` | MMU 流水线级接线（含 **exception handler 注册** + PTW 注册）—— **归 `mmu-chmem-phase-b-c-launch` / `chmem-ptw-walk` 的 owner 裁决**，不新立 | `chmem-ptw-walk`（PTW 实装） | 大 |
| — | `chmem-tlm` 目录名治理（B0 剩余项 86） | `ip/{mmu,cpu}/tlm/` 双模共享文件迁 `core/` | 无 | 小 |

> **owner 裁决待办**：PTW walk FSM 的实装归属需在 `mmu-chmem-phase-b-c-launch`（status: ready，10/33 tasks）的 P1 阶段确认 —— 归它 or 归未立项 `chmem-ptw-walk`，**避免双 owner**。
> **B2 → B1 的顺序约束**：本定案已完成 ⇒ B1 可按上表立项（不再有「按错误语义假设补实现」的风险）。
