# TLM 退役 B1 承载力拆解（TLM Retirement — B1 Capacity Plan）

> **来源**: ADR-088（冻结契约 + 语义裁决锚）· `docs/architecture/tlm-coverage-parity-ledger.md`（463 条覆盖台账，§5 已给 11 项 B1 待办）· 本文件的**实测盘点**（2026-10-24，HEAD `84ab102`）
> **性质**: **规划输入**（供 architect 做 B1 拆分决策 + B2 语义裁决），**不是**实施方案。实施须逐项立项（OpenSpec change）。
> **对应路线图**: `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B1 段）· `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G2 门禁）

---

## 0. 一句话结论

CH_MEM 侧与 TLM 侧的**注册差距 = 10 处**（TLM 工厂 19 处 `register_plugin` vs CH_MEM 工厂 9 处）。这 10 处可分三类：**接线缺失**（实现已在，只需接线 ⇒ 最便宜）、**实现缺失**（大工程）、**架构决策**（CH_MEM 组合式可能根本不需要）。**B1 的正确做法是先把「接线缺失」清零，再逐项决策「实现缺失 / 架构决策」** —— 否则会用 4 个大工程掩盖 3 个一行修复。

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
| A2 | **MMU PTW FSM 未注册** | `ip/cpu/plugins/mmu_ptw_fsm_chmem.h`（`PtWalkFsmPlugin`，314 行）存在；`grep -c PtWalkFsmPlugin ip/cpu/cpu_factory_chmem.h` = **0** | 中（需接 MMUPluginChmem 的 miss ⇒ PTW 通路 + 异常回传） | 独立 change：`chmem-ptw-fsm-register`（**依赖 B2 的 PTW 语义裁决**） |
| A3 | **isa/xlen 契约校验未接** | `cf::cpu::detail::validate_isa_xlen` 只在 TLM 工厂 `ip/cpu/cpu_factory.h` 接线；`cpu_factory_chmem.h:99` 的 `CPUConfig` 解析**根本不读 `isa`** | 小（复用同一 helper + CH_MEM 工厂加一行调用 + 明确 CH_MEM 侧 `isa` 语义） | 独立 change：`chmem-isa-contract-validate` |

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
| C1 | **StageLinkPlugin**（TLM 119 行） | CH_MEM 为**全组合通路**（`cpu_factory_chmem.h:437-441` 无 stage 寄存器）⇒ stage 间传播无对应物 | 「不迁」还是「迁（保持架构对称）」？ |
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

## 5. 需要 architect 决策的问题（阻塞项）

1. **A3 前置**：CH_MEM 工厂的 `CPUConfig` 是否接受 `isa`？接受 ⇒ 复用 `validate_isa_xlen`；不接受 ⇒ 显式拒绝非 `rv32im` 值。
2. **C1**：StageLinkPlugin 迁不迁？
3. **C2**：CH_MEM 是否需要多级 TLB？
4. **C3**：CH_MEM 是否需要 3/7/10-stage？
5. **C4**：M 扩展语义规范取哪边？（B2 正式表态）
6. **B1/B3 的先后**：BP（性能基线硬依赖）是否先于 L1Cache 本体（覆盖面最大）？
