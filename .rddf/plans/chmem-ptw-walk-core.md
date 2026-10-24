# ⑤-批1 chmem-ptw-walk-core — 执行文件

> **change**: `chmem-ptw-walk-core`（★原 `chmem-ptw-walk` 单 plan 拆两批之一，planner 2026-10-24）
> **规模**: 中大 · **前置**: **无** ⇒ **现在可启动**
> **内容**: PTW **单元级**实装 —— 差距表 + `FsmBase` 封装 + `MMU_TLB` 两级 + `MMU_PTW` walk + `PTWCache` + D16 物理读变更面
> **明确不含**: 流水线注册 · fetch stall · exception handler 注册 · 目录合规迁移（⇒ 全在 **批 2** `chmem-ptw-walk-wiring`）
> **上游**: **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构 —— 目标架构**已锁定**）· ADR-088 v1.1 §3 PTW 行（**现在不接线**；回退 vaddr = 已知临时降级）
> **tasks 骨架来源**: `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 的 defer 列表 —— 本批收 **B.3' / B.4'** 的单元级部分
> **姊妹**: 批 2 = `.rddf/plans/chmem-ptw-walk-wiring.md`（待 C.x）· 合并总览 = `.rddf/plans/chmem-ptw-walk.md`
> **owner**: `chmem-ptw-walk`（architect 已定案 2026-10-24）⇒ 拆分后沿用 `chmem-ptw-walk-*` 前缀

---

## 为什么必须拆成两个 change（不是两个 task）

若两批同属一个 change ⇒ 批 1 完成后 change **无法归档**（AC 未全绿）⇒ 会被 12 月后的 C.x 长期阻塞。拆分后 **批 1 可独立归档**（与 ① 的交付点 A/B 拆分同理）⇒ **批 1 是 B1 里唯一「完全无前置」的实质工作**。

---

## Goal

按 **ADR-085/086 已锁定的目标架构**实装 CH_MEM 侧的 PTW 页表 walk **单元级**（可独立测试，不接流水线）。

### 必须遵守的目标架构约束（ADR-085 D01–D21，**不是**自由设计）

| 决策 | 内容 | 对实现的约束 | 本批 |
|------|------|-------------|------|
| **D01** | `MMU_TLB` + `MMU_PTW` **拆分** | 两个独立单元，不是「PTW 作为 MMU 内部函数」 | ✅ |
| **D04** | `PTWCache` **只存非叶 PDE** | 不能把叶子 PTE 也缓存（否则 §4 语义错） | ✅ |
| **D08** | `pt_base` 从 **satp 组合派生** | 不是硬编码常量 | ✅ |
| **D10** | 用 **`FsmBase` 封装**，而非裸 `ch_state_machine` | **不得**直接用裸 DSL（现有 `PtWalkFsmPlugin` 正是裸 DSL ⇒ **不合规**） | ✅ |
| **D15** | PTW **只回填 TLB**，不直写 PADDR | 禁止 PTW 直接产 PADDR | ✅ |
| **D16 / D21** | 页表物理读端口**绕过翻译**；`ch_mem` 所有权归 **DMemPlugin** | 不能自建第二个物理内存端口 | ✅（变更面确定） |
| **D18** | 代码落 **`ip/mmu/plugins/`** | 修 ADR-083 的目录违规 | ⛔ **批 2**（见下） |

> ★**D18 的分批说明**：现有 `ip/cpu/plugins/mmu_ptw_chmem.h`（314 行，`PtWalkFsmPlugin`）位置违规。**本批不改它的位置**（改位置 ⇒ 连带改 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 断言 ⇒ 属批 2 的「旧实现退役」范畴）。**本批的新文件直接落 `ip/mmu/plugins/`** ⇒ 新代码**一出生即合规** ⇒ 不制造新的违规面。

### 明确不做

不重新设计目标架构；不采用现有 `PtWalkFsmPlugin` 的形态（sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache ⇒ 与 D04/D10/D18 全面不符 ⇒ 接线必返工）；**不做流水线注册**（批 2）。

---

## Architecture

```
ip/mmu/plugins/                        ← D18: 新文件直接落这里
  ├─ mm_u_tlb_chmem.h      (MMU_TLB: L0 + L1 两级, ch_mem 承载)
  ├─ mm_u_ptw_chmem.h      (MMU_PTW: FsmBase 封装, PTWCache 只存非叶 PDE)
  └─ ptw_cache_chmem.h     (PTWCache: 非叶 PDE 缓存)

ip/cpu/plugins/dmem_chmem.h   (改: D16 —— mem_ 的创建时机 + 句柄暴露)
```

### 跨 change 接口（本批**不消费**，只登记）

| 依赖 | 提供方 | 批 2 的用法 |
|------|--------|------------|
| C.1 | `mmu-chmem-phase-b-c-launch` | `CPU_EXCEPTION_CODE` 写入源的消费端（exception handler 注册归 C.1 ⇒ **本批不碰**） |
| C.4 | 同上 | `MMUPluginChmem` 注册进 `PipeBuilder`（注册载体） |
| C.5 | 同上 | `hazard_chmem` 的 `halt_when` wiring（fetch stall 通道） |

---

## Tech Stack

C++23 · CppHDL（`ch_state_machine` **经 `FsmBase` 封装**）· `ch_mem` 物理读（经 DMemPlugin）· Catch2

---

## Tasks

### Task 1: 事实复核 + ADR-085/086 逐条对照

- [ ] **Step 1: 精读 ADR-085 / ADR-086** —— 提取 D01–D21 的**全部**约束（不只上表 8 条）⇒ 作为实现的**验收清单**
- [ ] **Step 2: 现有实现差距清单** —— `ip/cpu/plugins/mmu_ptw_chmem.h`（314 行，`PtWalkFsmPlugin`：sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache）逐条对照 D01/D04/D10/D18 ⇒ 输出**差距表**（⇒ 这张表就是本 change 的任务分解骨架）
- [ ] **Step 3: 差距表（格式定义，产出物必须可验）** —— 输出一张表，列为：`现有实现位置` | `ADR-085/086 条款`（D01/D04/…）| `差距描述` | `处置`（重写/迁移/新增/废弃）| `落到 Task 号`。**表本身是 Task 2–4 的任务分解骨架** ⇒ 无表 ⇒ 不进实施。⚠️ 迁移 defer 任务时**不得照抄**：B.3'「抽 `build_pt_walk_fsm` 自由函数（保留 `PtWalkFsmPlugin` 为薄包装，`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 零改动）」的前提与 **D10（`FsmBase` 封装）+ D18（落 `ip/mmu/plugins/`）冲突** ⇒ 该项**必须改写为「按 D10 用 `FsmBase` 重写」**
- [ ] **Step 4: owner 边界确认** —— 本批 = PTW walk **单元级本体**；**exception handler 的注册归 C.1**（`mmu_exception_handler_chmem.h:26-28` 明写「接线归 C.1」）⇒ **本批不注册 exception handler**（越界 ⇒ 双 owner）。`mmu-chmem-phase-b-c-launch` = Phase C（C.1/C.4/C.5/C.6）⇒ 边界写进两个 change 的文档，互不重叠
- [ ] **Step 5: 前置状态核查（★本批只需确认「不依赖」）** —— 确认本批的 Task 2–4 **均不触碰** C.1/C.4/C.5 的产出 ⇒ 若发现某 Task 实际依赖 C.x ⇒ 该 Task 移到批 2，并在本 plan 记录

### Task 2: `FsmBase` 封装（D10）

- [ ] **Step 1: 复用框架级 `FsmBase`（已存在，**不得新建**）** —— 框架已有 `cf::plugin::multi_cycle_fsm::FsmBase`（`include/cf/plugin/multi_cycle_fsm.h:47`），既有使用先例：`ip/cpu/plugins/mul_div_fsm_chmem.h:65-67` ⇒ **直接复用**，**不得**在 `ip/mmu/` 再造一个（否则 ADR-046 豁免与框架脱钩）。核对 `FsmBase` 的构造/派生要求是否满足 ADR-085 D10 的封装约束
- [ ] **Step 2: PTW 改用 `FsmBase`** —— **不得**裸 `ch_state_machine`
- [ ] **Step 3: ADR-046 豁免标记** —— 多周期协议引擎需 `CF_PLUGIN_USE_FSM_EXEMPT`（`tools/verify_plugin_decision.sh` 会查）⇒ 正确标注

### Task 3: `MMU_TLB`（L0 + L1 两级，D01）

- [ ] **Step 1: 两级结构** —— L0（VPN 索引）+ L1（物理页号索引）⇒ 与 TLM 侧多级 TLB（`ip/mmu/lib/multi_level_tlb.*`）语义对齐
- [ ] **Step 2: ASID / `satp` 组合** —— `pt_base` 从 satp 组合派生（D08）⇒ 不用硬编码
- [ ] **Step 3: flush** —— satp 写 / sfence ⇒ L0+L1 全失效（D20）
- [ ] **Step 4: 单元测试** —— 命中/未命中/替换/失效/ASID 切换

### Task 4: `MMU_PTW`（D01/D04/D15）+ D16 变更面

- [ ] **Step 1: walk 逻辑** —— 多级 walk（sv32 2 级 / sv39 3 级 / sv48 4 级 ⇒ 按 `mmu_mode` 选择）
- [ ] **Step 2: `PTWCache`（D04）** —— **只存非叶 PDE**（叶子 PTE 不进缓存）
- [ ] **Step 3: 只回填 TLB（D15）** —— PTW **不得**直写 PADDR；PADDR 由 TLB 命中路径产出
- [ ] **Step 4: 物理读端口（D16/D21）—— 变更面确定**，不是「可能要改」** —— ADR-085 **D16 已判定必须改 `ip/cpu/plugins/dmem_chmem.h`**：`DMemPlugin::mem_` 是 **private** 且**惰性创建于 memory/NORMAL** ⇒ fetch/EARLY 阶段拿到的是**空句柄** ⇒ 须把 `mem_` 的创建提前到 **setup() 期**并暴露句柄给 PTW 的物理读端口 ⇒ **本 change 的受影响文件清单明确包含 `ip/cpu/plugins/dmem_chmem.h`**。`ch_mem` 所有权仍归 DMemPlugin（D21）⇒ PTW **不得**自建第二个物理端口
- [ ] **Step 5: fault 语义** —— invalid PTE → code 12 / reserved → code 15 / access fault → 13 ⇒ 写 `CPU_EXCEPTION_CODE`（★本批**只产出 fault code**，不接线到 exception handler ⇒ 接线归批 2 / C.1）
- [ ] **Step 6: 单元测试** —— **真走 walk**（非预载 TLB）⇒ 移植 B.4' 的判据：sv32 非恒等页表向量 + PTW fault（unit 级）

---

## Acceptance

- [ ] **AC-1**: PTW 实现**符合 ADR-085 D01/D04/D08/D10/D15/D16/D20/D21 全部约束**（逐条对照表 PASS）
- [ ] **AC-2**: PTW 用**框架已有**的 `cf::plugin::multi_cycle_fsm::FsmBase`（**复用，不新建**）+ `CF_PLUGIN_USE_FSM_EXEMPT` 标记（ADR-046 先例：`mmu_ptw_chmem.h:96`）
- [ ] **AC-3**: `PTWCache` **只存非叶 PDE**（D04）⇒ 有 case 证明叶子 PTE 不入缓存；`pt_base` 从 satp **组合派生**（D08）⇒ 非硬编码
- [ ] **AC-4**: PTW **只回填 TLB**，不直写 PADDR（D15）⇒ 有 case 证明
- [ ] **AC-5**: 页表读**经 DMemPlugin** 的 `ch_mem`（D16/D21）⇒ 无第二个物理端口；`dmem_chmem.h` 的 `mem_` 创建时机已按 D16 调整
- [ ] **AC-6**: sv32 **真走 walk** 的向量测试 PASS（非预载 TLB）+ PTW fault 语义（code 12/13/15）PASS
- [ ] **AC-7**: 8 门禁全 PASS + 全量回归 0 退化（失败集合恰 1 个 pre-existing：`tests/cpu/integration/test_7stage_riscv.cpp:124`）· `[mmu]` 59 / `[mmu-chmem-basic]` 7 / `[chmem]` 130 维持
- [ ] **AC-8**: **本批不接线** —— `cpu_factory_chmem.h` 的 `enable_mmu=true` **仍是 plumbing-only no-op**（不属本批 scope）⇒ 归档时**显式登记**「流水线接线待批 2（且待 C.1/C.4/C.5）」，**不假装完整**
- [ ] **AC-9**: **零流水线副作用** —— 本批新增文件**未被任何工厂注册** ⇒ CH_MEM 后端的**外部可观测行为零变化**（`[chmem]` / `[verilator]` 的 cycle 数与 TOHOST **逐位不变**）

---

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **ADR-085/086 的目标架构与现有 `PtWalkFsmPlugin` 全面不符** ⇒ 迁移 defer 任务时容易照抄旧形态 | Task 1 Step 2 的**差距表**是任务分解的唯一依据；Task 1 Step 3 明确「B.3' 的前提与 D10 冲突 ⇒ 迁移时改写」 |
| R2 | **物理读端口归属**（D16/D21 归 DMemPlugin）⇒ **确定要改** `dmem_chmem.h`（`mem_` private + 惰性创建于 memory/NORMAL ⇒ fetch/EARLY 拿空句柄 ⇒ 须 setup() 期提前创建并暴露句柄）⇒ 受影响文件清单**明确包含** `dmem_chmem.h`（v1 只写「可能要改」⇒ agent 会漏） |
| R3 | **误把「单元级」做成「半接线」** —— 边角接线（注册进工厂）⇒ 外部行为变化 ⇒ 与批 2 双写 | **AC-9 钉死「外部可观测行为零变化」**（cycle 逐位不变）⇒ 任何意外注册都会被抓 |
| R4 | 多级 walk（sv39/sv48）在 CH_MEM 侧是**新能力**（TLM 侧 `ptw.cpp` 有完整 walk 可参考，但 TLM 侧是否支持 sv39/sv48 需核实） | Task 1 Step 1 核对 ADR-085 的级数要求；若只要求 sv32 ⇒ **明确范围（不静默缩减）** |
| R5 | 「回退 vaddr」的降级行为**本批不变**（不接线 ⇒ 降级仍在）⇒ 无既有测试需改 | ✅ 本批**不接线** ⇒ `[mmu-chmem-basic]` 7 case 的「回退 vaddr」钉子**保持不变**（这正是批 1 无退化的关键） |
| R6 | 与 ③ L1Cache 的接口面（MMU → Cache 的 PADDR/有效信号）需一致 | 建议 ③ 与 ⑤ **同时立项**并共用一份接口契约；本批只到 TLB 单元级 ⇒ 接口面尚未定型 ⇒ 由 ③ 主导 |

---

## 迁移说明

- **执行顺序**：Task 1（复核 + 差距表）→ Task 2（FsmBase）→ Task 3（TLB）→ Task 4（PTW + D16）
- **与其它 change 的关系**：**完全无前置** ⇒ 可与 ②/③/④ 并行
- **归档条件**：AC-1…AC-9 全绿 ⇒ **可独立归档**，不因 C.x 阻塞
- **不在本 change 范围**：流水线注册 · fetch stall · exception handler 注册 · 旧 `mmu_ptw_chmem.h` 的位置迁移与退役（⇒ 批 2）· 台账重判（⇒ 批 2，因接线前条目态不变）
