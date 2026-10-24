# ⑤-批2 chmem-ptw-walk-wiring — 执行文件

> **change**: `chmem-ptw-walk-wiring`（★原 `chmem-ptw-walk` 单 plan 拆两批之一，planner 2026-10-24）
> **规模**: 中 · **前置**: `mmu-chmem-phase-b-c-launch` 的 **C.1 / C.4 / C.5**（**全部未完成** ⇒ 12 月之后）+ 批 1 `chmem-ptw-walk-core` 已归档
> **内容**: PTW **流水线接线** —— 工厂注册 + fetch stall + 目录合规（旧 `mmu_ptw_chmem.h` 退役/迁移）+ 台账重判 + ADR-088 §3 状态更新 + 收口
> **上游**: **ADR-085 / ADR-086** · ADR-088 v1.1 §3 PTW 行（「现在不接线」⇒ 本批改为「已接线」）
> **tasks 骨架来源**: `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 的 defer 列表 —— 本批收 **B.2' / B.3'' / B.3'''** 的接线部分
> **姊妹**: 批 1 = `.rddf/plans/chmem-ptw-walk-core.md`（**无前置，已可开工**）· 合并总览 = `.rddf/plans/chmem-ptw-walk.md`
> **owner**: `chmem-ptw-walk`（architect 已定案 2026-10-24）

---

## ⚠️ 开工前置（本批**现在不可开工**）

| 前置 | 状态 | 归属 |
|------|------|------|
| **C.1** `mmu_exception_handler_chmem` CPU_EXCEPTION_CODE wiring | `- [ ]`（`mmu-chmem-phase-b-c-launch/tasks.md:136`）· 2d · **且挂未立项的 `csr-chmem-trap-pc-jump`**（tasks.md:22,34） | `mmu-chmem-phase-b-c-launch` |
| **C.4** `cpu_factory_chmem` 注册 `MMUPluginChmem` + fprintf 删除 | `- [ ]`（tasks.md:137）· 2d | `mmu-chmem-phase-b-c-launch` |
| **C.5** `hazard_chmem` `halt_when()` wiring | `- [ ]`（tasks.md:138）· 1d | `mmu-chmem-phase-b-c-launch` |
| **批 1** `chmem-ptw-walk-core` 归档 | 未开工（**无前置，可立即启动**） | 本线 |

⇒ **C.x 窗口 2026-11-30 ~ 12-15** ⇒ 本批**12 月之后**才能开工（不由 B1 决定）。

---

## Goal

把批 1 的 PTW **单元级**实现**接入 CH_MEM 流水线**，并完成 ADR-085 D18 目录合规与台账重判。

### 本批要做的事（批 1 明确排除的）

| 项 | 批 1 | 批 2 |
|---|---|---|
| `MMU_TLB` / `MMU_PTW` / `PTWCache` 单元级 | ✅ | — |
| `FsmBase` 封装（D10） | ✅ | — |
| D16 物理读变更面（`dmem_chmem.h`） | ✅ | — |
| **工厂注册**（`enable_mmu=true` 真实注册） | ⛔ | ✅ |
| **fetch stall**（CtrlLink 有状态 halt 消费） | ⛔ | ✅ |
| **exception handler 注册** | ⛔ | ⛔ **归 C.1**（本批只消费 `CPU_EXCEPTION_CODE`） |
| **目录合规 D18**（旧 `ip/cpu/plugins/mmu_ptw_chmem.h` 退役/迁移） | ⛔ | ✅ |
| **台账重判**（22 条 PTW 条目） | ⛔ | ✅ |
| **ADR-088 §3 PTW 行状态更新** | ⛔ | ✅ |

---

## Architecture

```
ip/mmu/plugins/                          ← 批 1 已建（mm_u_tlb_chmem.h / mm_u_ptw_chmem.h / ptw_cache_chmem.h）
ip/cpu/cpu_factory_chmem.h   (改: 批 2 —— 注册 MMUPluginChmem + MMU_TLB + MMU_PTW)
ip/cpu/plugins/mmu_exception_handler_chmem.h  (已存在 118 行 ⇒ 注册归 C.1，本批只消费其输入)
ip/cpu/plugins/hazard_chmem.h             (C.5 负责 halt_when wiring，本批依赖其就位)
ip/cpu/plugins/mmu_ptw_chmem.h            (旧 PtWalkFsmPlugin，314 行 ⇒ 退役/迁移，D18)
tests/cpu/test_mmu_ptw_fsm_chmem.cpp       (随旧实现退役而重写)
```

### 跨 change 接口（本批**消费** `mmu-chmem-phase-b-c-launch` 的产出）

| 依赖 | 提供方 | 本批的用法 |
|------|--------|-----------|
| C.1 | `mmu-chmem-phase-b-c-launch` | `CPU_EXCEPTION_CODE` 写入源（PTW fault ⇒ 写 fault code ⇒ exception handler 消费） |
| C.4 | 同上 | `MMUPluginChmem` 注册进 `PipeBuilder`（**注册载体** ⇒ 本批的 PTW 挂在已注册的 MMU 上） |
| C.5 | 同上 | `hazard_chmem` 的 `halt_when` wiring（fetch stall 通道）⇒ **CtrlLink 有状态 halt 已就位**（capability `ctrllink-continuous-handle`） |

---

## Tech Stack

C++23 · CppHDL（`ch_state_machine` **经 `FsmBase` 封装**）· CtrlLink 有状态 halt · Catch2

---

## Tasks

### Task 5: 流水线级接线

- [ ] **Step 1: 前置确认（硬闸）** —— 逐条确认 C.1 / C.4 / C.5 **已完成**（`mmu-chmem-phase-b-c-launch/tasks.md:136-138` 三项 `- [x]`）⇒ **任一未完成 ⇒ 本批不启动**（不降级为「部分接线」）
- [ ] **Step 2: 工厂注册** —— `cpu_factory_chmem.h` 的 `enable_mmu=true` 从 **plumbing-only no-op** 变为**真实注册** MMU_TLB + MMU_PTW（**注册载体由 C.4 提供的 `MMUPluginChmem` 承载** ⇒ 本批在其上挂 PTW，不重复注册 MMU 本体）
- [ ] **Step 3: fetch stall（D16 + C.5）** —— 翻译未完成 ⇒ fetch 停顿 ⇒ 走 CtrlLink 有状态 halt（capability `ctrllink-continuous-handle` 已就位；★`ch_reg <<=` 须在 Simulator 创建前调用，ADR-040 §10.6 + `ctrl-link-chmem-reg-handle` 已交付的 `halt_when_stateful` 通道）
- [ ] **Step 4: exception handler —— 只消费，不注册（★守卫方向更正）** —— exception handler 的注册归 **C.1**（`mmu_exception_handler_chmem.h:26-28` 明写「接线归 C.1」）⇒ **本批不碰注册**。⚠️ **守卫方向**：v1 说「解除 `#ifndef CF_PLUGIN_USE_CH_MEM` 包裹」是**错的** —— `cpu_factory_chmem.h:342-348` 是**反向**守卫（TLM 版**仅在非 CH_MEM 编译**）⇒ 正确动作是**在 CH_MEM 分支新增注册**（由 C.1 负责）⇒ 本批只**消费** `CPU_EXCEPTION_CODE`
- [ ] **Step 5: 无静默退化** —— `enable_mmu=true` 必须真的启用 MMU（现状是 no-op ⇒ 配置与实际行为不一致）⇒ 本批消除该不一致；若 C.1 未完成 ⇒ 本批归档时**显式登记「exception handler 待 C.1」**，**不假装完整**
- [ ] **Step 6: 集成测试** —— MMU + 流水线端到端（prefill TLB → lookup → 命中 → paddr 正确）

### Task 6: 目录合规（D18）+ 旧实现退役

- [ ] **Step 1: 旧实现退役决策（★先定后动）** —— `ip/cpu/plugins/mmu_ptw_chmem.h`（314 行，`PtWalkFsmPlugin`：sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache ⇒ 与 D04/D10/D18 全面不符）—— **退役**（删除）或**降级为薄包装**？⚠️ **不得照抄 defer 项 B.3' 的「保留为薄包装」**（其前提与 D10 冲突）⇒ 决策须由 Task 1（批 1）的差距表给出
- [ ] **Step 2: 迁移文件** —— 若保留：把其位置从 `ip/cpu/plugins/` 迁到 `ip/mmu/plugins/`（D18）；若退役：删除并清理 includer
- [ ] **Step 3: include 路径更新** —— 更新所有 includer（当前：`ip/mmu/plugins/MMUPlugin_chmem.h:20`（注释）+ `tests/cpu/test_mmu_ptw_fsm_chmem.cpp:40`（真 include））
- [ ] **Step 4: 测试重写（★不是「删测试凑绿」）** —— `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` **必须按 D10/D18 重写**（旧实现基于裸 DSL + 违规目录）⇒ 该文件的改动**不算退化**，但必须**逐条核对「旧断言 ⇒ 新断言」的语义等价性**
- [ ] **Step 5: 双 binary 编译通过** —— TLM + CH_MEM 均绿

### Task 7: 收口

- [ ] **Step 1: 8 门禁** —— 特别验 `verify_plugin_decision.sh`（`CF_PLUGIN_USE_FSM_EXEMPT` 标记）+ `check_plugin_portability.sh`（`_chmem.h` 双文件分离）
- [ ] **Step 2: 全量回归 0 退化** —— `[mmu]` 59 / `[mmu-chmem-basic]` 7 / `[mmu-verilator]` 3 / `[chmem]` 130 · 失败集合**恰为 1 个 pre-existing**（`tests/cpu/integration/test_7stage_riscv.cpp:124`）
- [ ] **Step 3: 台账更新（★红线）** —— `[mmu]` 族 59 条 + PTW 相关 22 条从 `partial` **逐条**重判（降级理由「回退 vaddr = 已知临时降级」不再成立 ⇒ 改为 `covered` / `partial`（真实差异））。⚠️ **台账红线**：条目态**继承族级 §2** ⇒ **改条目必须同改族级**；`not-covered` 必须**显式手写理由**、不得为继承标记、**同一理由 ≤10 次**
- [ ] **Step 4: ADR-088 §3 PTW 行状态更新** —— 「现在不接线」⇒ 「已接线（按 ADR-085/086 目标架构）」⇒ **同时更新 §5.3.2 与 §7 的相关引用**（ADR 改动须走 `docs/architecture/adr.md` 注册表 + `verify_adr.sh` 校验）
- [ ] **Step 5: commit** —— 分批提交（实现 / 测试 / 文档 / 归档）

---

## Acceptance

- [ ] **AC-1**: `cpu_factory_chmem.h` 的 `enable_mmu=true` **真实注册 MMU/PTW**（不再是 plumbing-only no-op）；⚠️ **exception handler 的注册不在本 change scope**（归 C.1）⇒ 归档时若 C.1 未完成，须显式登记「待 C.1」而非默认完整
- [ ] **AC-2**: 翻译未完成 ⇒ fetch **真实停顿**（CtrlLink 有状态 halt **实际消费** ⇒ 有 case 证明停顿发生，非「配了但没触发」）
- [ ] **AC-3**: 代码落 **`ip/mmu/plugins/`**（D18）⇒ `ip/cpu/plugins/mmu_ptw_chmem.h` 已迁出或退役；旧 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 已按 D10/D18 重写且**语义等价性逐条核对**
- [ ] **AC-4**: `[mmu]` 59 / `[mmu-chmem-basic]` / `[mmu-verilator]` / `[chmem]` **0 退化**；双 binary 编译通过
- [ ] **AC-5**: 8 门禁全 PASS + 全量回归 0 退化（失败集合恰 1 个 pre-existing）
- [ ] **AC-6**: 台账 PTW 相关条目**逐条**重判（**含族级 §2 同步**）+ ADR-088 §3 PTW 行状态更新
- [ ] **AC-7**: **真 sv32 translation 端到端**（非恒等页表）—— `[mmu-verilator]` 的 sv32 translation 语义有 case 覆盖（当前该 family 3 case 只验 Bare plumbing ⇒ 本批新增 sv32 真翻译 case）

---

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **前置（C.1/C.4/C.5）全部未完成** ⇒ 12 月之后才能开工 ⇒ B1 的「PTW 接线」在本窗口**交付不了** | ★**拆两批**（批 1 现在可开工，单元级交付）；本批**不降级为部分接线**（Task 5 Step 1 硬闸）⇒ 宁可延后也不产生「半接线」的隐性降级 |
| R2 | C.1 自身**挂未立项**的 `csr-chmem-trap-pc-jump`（tasks.md:22,34）⇒ C.1 可能进一步延后 | 本批的开工点随之顺延；**不影响批 1** ⇒ B1 仍有实质交付（批 1） |
| R3 | 「回退 vaddr」的降级行为消失 ⇒ **依赖该降级的既有测试会变** | 台账 22 条 PTW 条目逐条重判（AC-6）+ `[mmu-chmem-basic]` 7 case 的行为钉子需更新（它们的钉子正是「回退 vaddr」）⇒ **必须同步的回归面** |
| R4 | **半接线**（注册了 PTW 但 fetch stall 未生效）⇒ 静默错误翻译 | AC-2 要求「有 case 证明停顿发生」；AC-1 要求配置与行为一致 |
| R5 | 旧实现退役决策（删除 vs 薄包装）**照抄 defer 项** ⇒ 违反 D10 | Task 6 Step 1 强制「先定后动」，且 Task 1（批 1）的差距表是唯一依据 |
| R6 | 与 ③ L1Cache 的接口面（MMU → Cache 的 PADDR/有效信号）需一致 | 本批是 PADDR 真正产出的时刻 ⇒ **本批与 ③ 必须同期对齐接口契约**（③ 阶段 B 依赖本批） |

---

## 迁移说明

- **执行顺序**：Task 5（接线，含前置硬闸）→ Task 6（目录合规 + 旧实现退役）→ Task 7（收口）
- **与其它 change 的关系**：依赖 C.x（12 月后）+ 批 1 归档；**产出被 ③ 阶段 B 依赖**（PADDR 真正可用）
- **不在本 change 范围**：PTW 单元级实现（⇒ 批 1）· exception handler 注册（⇒ C.1）· `MMUPluginChmem` 本体注册（⇒ C.4）· `hazard_chmem` 的 `halt_when` wiring（⇒ C.5）
