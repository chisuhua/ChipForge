# ⑤ chmem-ptw-walk — 执行文件

> **change**: ★**拆成 2 个 change**（planner 裁决 2026-10-24）—— ① **`chmem-ptw-walk-core`**（批 1：Task 1–4 = 差距表 + `FsmBase` 封装 + `MMU_TLB` 两级 + `MMU_PTW` walk/PTWCache + D16 变更面，**无前置，现在启动**）· ② **`chmem-ptw-walk-wiring`**（批 2：Task 5–7 = 流水线注册 + fetch stall + 目录合规 + 收口，**依赖 `mmu-chmem-phase-b-c-launch` 的 C.1/C.4/C.5**）。
> **★拆分理由**：若两批同属一个 change ⇒ 批 1 完成后 change **无法归档**（AC 未全绿）⇒ 会被 12 月后的 C.x 长期阻塞；拆分后批 1 可独立归档（与 ① 的交付点 A/B 拆分同理）
> **规模**: 批 1 = 中大（**无前置**）· 批 2 = 中（**依赖 C.x**）
> **上游**: **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构 —— 目标架构**已锁定**）· ADR-088 v1.1 §3 PTW 行（**现在不接线**；回退 vaddr = 已知临时降级）
> **tasks 骨架来源**: `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 的 defer 列表（**B.2' / B.3' / B.3'' / B.4' / B.3'''**）—— **批 1 收 B.3'/B.4' 的单元级部分，批 2 收 B.2'/B.3''/B.3''' 的接线部分**
> **v2 修订（2026-10-24，oracle 审阅）**：① **owner 重叠已修正** —— exception handler 的**注册归 C.1**（`mmu_exception_handler_chmem.h:26-28` 自述「接线归 C.1」），**本 change 不碰**（v1 越界）② `#ifndef` 守卫方向修正（**反向**守卫 ⇒ CH_MEM 分支**新增**注册，不是「解除包裹」）③ **D16 变更面确定化**（必须改 `dmem_chmem.h`）④ **FsmBase 已存在**（复用，不新建）⑤ **拆两批** + 排期结论 ⑥ 差距表格式定义

---

## Goal

按 **ADR-085/086 已锁定的目标架构**实装 CH_MEM 侧的 PTW 页表 walk，并接通流水线级 MMU（含 exception handler 注册）。

**必须遵守的目标架构约束**（ADR-085 D01–D21，**不是**自由设计）：

| 决策 | 内容 | 对实现的约束 |
|------|------|-------------|
| **D01** | `MMU_TLB` + `MMU_PTW` **拆分** | 两个独立单元，不是「PTW 作为 MMU 内部函数」 |
| **D04** | `PTWCache` **只存非叶 PDE** | 不能把叶子 PTE 也缓存（否则 §4 语义错） |
| **D08** | `pt_base` 从 **satp 组合派生** | 不是硬编码常量 |
| **D10** | 用 **`FsmBase` 封装**，而非裸 `ch_state_machine` | **不得**直接用裸 DSL（现有 `PtWalkFsmPlugin` 正是裸 DSL ⇒ **不合规**） |
| **D15** | PTW **只回填 TLB**，不直写 PADDR | 禁止 PTW 直接产 PADDR |
| **D16 / D21** | 页表物理读端口**绕过翻译**；`ch_mem` 所有权归 **DMemPlugin** | 不能自建第二个物理内存端口 |
| **D18** | 代码落 **`ip/mmu/plugins/`** | 修 ADR-083 的目录违规 ⇒ **现有 `ip/cpu/plugins/mmu_ptw_chmem.h` 的位置本身不合规**，须迁 |

**明确不做**：不重新设计目标架构；不采用现有 `PtWalkFsmPlugin` 的形态（sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache ⇒ 与 D04/D10/D18 全面不符 ⇒ 接线必返工）。

## Architecture

```
ip/mmu/plugins/                        ← D18: 落这里
  ├─ mm_u_tlb_chmem.h      (MMU_TLB: L0 + L1 两级, ch_mem 承载)
  ├─ mm_u_ptw_chmem.h      (MMU_PTW: FsmBase 封装, PTWCache 只存非叶 PDE)
  └─ ptw_cache_chmem.h     (PTWCache: 非叶 PDE 缓存)
ip/cpu/cpu_factory_chmem.h   (改)
  └─ 注册 MMUPluginChmem + MMU_PTW + exception handler
ip/cpu/plugins/mmu_exception_handler_chmem.h  (已存在 118 行 ⇒ 注册即可)
```

**跨 change 接口**（本 change 消费 `mmu-chmem-phase-b-c-launch` 的产出）：

| 依赖 | 提供方 | 本 change 的用法 |
|------|--------|-----------------|
| C.1 | `mmu-chmem-phase-b-c-launch` | `CPU_EXCEPTION_CODE` 写入源（PTW fault ⇒ 写 fault code ⇒ exception handler 消费） |
| C.4 | 同上 | `MMUPluginChmem` 注册进 `PipeBuilder`（**注册载体** ⇒ 本 change 的 PTW 挂在已注册的 MMU 上） |
| C.5 | 同上 | `hazard_chmem` 的 `halt_when` wiring（fetch stall 通道）⇒ **CtrlLink 有状态 halt 已就位**（capability `ctrllink-continuous-handle`，ADR-088 §7 引用） |

## Tech Stack

C++23 · CppHDL（`ch_state_machine` **经 `FsmBase` 封装**）· `ch_mem` 物理读（经 DMemPlugin）· Catch2

---

## 排期结论（★v2，两批）

| 批 | 内容 | 前置 | 何时可启动 |
|---|------|------|-----------|
| **批 1** = change **`chmem-ptw-walk-core`** | Task 1（差距表）+ Task 2（`FsmBase` 封装）+ Task 3（`MMU_TLB` 两级）+ Task 4（`MMU_PTW` walk + PTWCache + D16 变更面） | **无** | **现在**（不依赖 C.x） |
| **批 2** = change **`chmem-ptw-walk-wiring`** | Task 5（流水线注册 + fetch stall）+ Task 6（目录合规）+ Task 7（收口） | `mmu-chmem-phase-b-c-launch` 的 **C.1/C.4/C.5**（全部未完成） | **12 月之后**（C.x 窗口 2026-11-30~12-15） |

⇒ **批 1 必须独立可归档**（AC-1…AC-5 + AC-7 不依赖批 2）；批 2 的 AC（AC-8/AC-10/AC-12）标注「待 C.x」。

## Tasks

### Task 1: 事实复核 + ADR-085/086 逐条对照

- [ ] **Step 1: 精读 ADR-085 / ADR-086** —— 提取 D01–D21 的**全部**约束（不只上表 8 条）⇒ 作为实现的**验收清单**
- [ ] **Step 2: 现有实现差距清单** —— `ip/cpu/plugins/mmu_ptw_chmem.h`（`PtWalkFsmPlugin`，sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache）逐条对照 D01/D04/D10/D18 ⇒ 输出**差距表**（⇒ 这张表就是本 change 的任务分解骨架）
- [ ] **Step 3: 差距表（★v2：格式定义，产出物必须可验）** —— 输出一张表，列为：`现有实现位置` | `ADR-085/086 条款`（D01/D04/…）| `差距描述` | `处置`（重写/迁移/新增/废弃）| `落到 Task 号`。**表本身是 Task 2–4 的任务分解骨架** ⇒ 无表 ⇒ 不进实施。⚠️ 迁移 defer 任务时**不得照抄**：B.3'「抽 `build_pt_walk_fsm` 自由函数（保留 `PtWalkFsmPlugin` 为薄包装，`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 零改动）」的前提与 **D10（`FsmBase` 封装）+ D18（落 `ip/mmu/plugins/`）冲突** ⇒ 该项**必须改写为「按 D10 用 `FsmBase` 重写」**，且 **`test_mmu_ptw_fsm_chmem.cpp` 必改**（见 AC-11）
- [ ] **Step 4: owner 边界确认（★v2 更正）** —— 本 change = PTW walk **本体** + **PTW 侧**的流水线接线；**exception handler 的注册归 C.1**（`mmu_exception_handler_chmem.h:26-28` 明写「接线归 C.1」）⇒ **本 change 不注册 exception handler**（v1 越界 ⇒ 双 owner）。`mmu-chmem-phase-b-c-launch` = Phase C（C.1/C.4/C.5/C.6）。⇒ 边界写进两个 change 的文档，互不重叠
- [ ] **Step 5: 前置状态核查** —— `mmu-chmem-phase-b-c-launch` 的 C.1/C.4/C.5 是否已完成？若未 ⇒ 本 change 排队等待（或先做不依赖它们的部分：PTW 单元级实现 + 测试）

### Task 2: `FsmBase` 封装（D10）

- [ ] **Step 1: 复用框架级 `FsmBase`（★v2：`FsmBase` 已存在，**不得新建**）** —— 框架已有 `cf::plugin::multi_cycle_fsm::FsmBase`（`include/cf/plugin/multi_cycle_fsm.h:47`），既有使用先例：`ip/cpu/plugins/mul_div_fsm_chmem.h:79` ⇒ **直接复用**，**不得**在 `ip/mmu/` 再造一个（否则 ADR-046 豁免与框架脱钩）。核对 `FsmBase` 的构造/派生要求是否满足 ADR-085 D10 的封装约束
- [ ] **Step 2: PTW 改用 `FsmBase`** —— **不得**裸 `ch_state_machine`
- [ ] **Step 3: ADR-046 豁免标记** —— 多周期协议引擎需 `CF_PLUGIN_USE_FSM_EXEMPT`（`tools/verify_plugin_decision.sh` 会查）⇒ 正确标注

### Task 3: `MMU_TLB`（L0 + L1 两级，D01）

- [ ] **Step 1: 两级结构** —— L0（VPN 索引）+ L1（物理页号索引）⇒ 与 TLM 侧多级 TLB（`ip/mmu/lib/multi_level_tlb.*`）语义对齐
- [ ] **Step 2: ASID / `satp` 组合** —— `pt_base` 从 satp 组合派生（D08）⇒ 不用硬编码
- [ ] **Step 3: flush** —— satp 写 / sfence ⇒ L0+L1 全失效（D20）
- [ ] **Step 4: 单元测试** —— 命中/未命中/替换/失效/ASID 切换

### Task 4: `MMU_PTW`（D01/D04/D15）

- [ ] **Step 1: walk 逻辑** —— 多级 walk（sv32 2 级 / sv39 3 级 / sv48 4 级 ⇒ 按 `mmu_mode` 选择）
- [ ] **Step 2: `PTWCache`（D04）** —— **只存非叶 PDE**（叶子 PTE 不进缓存）
- [ ] **Step 3: 只回填 TLB（D15）** —— PTW **不得**直写 PADDR；PADDR 由 TLB 命中路径产出
- [ ] **Step 4: 物理读端口（D16/D21）—— ★v2：变更面**确定**，不是「可能要改」** —— ADR-085 **D16 已判定必须改 `ip/cpu/plugins/dmem_chmem.h`**：`DMemPlugin::mem_` 是 **private** 且**惰性创建于 memory/NORMAL** ⇒ fetch/EARLY 阶段拿到的是**空句柄** ⇒ 须把 `mem_` 的创建提前到 **setup() 期**并暴露句柄给 PTW 的物理读端口 ⇒ **本 change 的受影响文件清单明确包含 `ip/cpu/plugins/dmem_chmem.h`**（不是「可能」）。`ch_mem` 所有权仍归 DMemPlugin（D21）⇒ PTW **不得**自建第二个物理端口
- [ ] **Step 5: fault 语义** —— invalid PTE → code 12 / reserved → code 15 / access fault → 13 ⇒ 写 `CPU_EXCEPTION_CODE`
- [ ] **Step 6: 单元测试** —— **真走 walk**（非预载 TLB）⇒ 移植 B.4' 的判据：sv32 非恒等页表向量 + PTW fault（unit 级）

### Task 5: 流水线级接线

- [ ] **Step 1: 工厂注册** —— `cpu_factory_chmem.h` 的 `enable_mmu=true` 从 **plumbing-only no-op** 变为**真实注册** MMU_TLB + MMU_PTW + exception handler
- [ ] **Step 2: fetch stall（D16 + C.5）** —— 翻译未完成 ⇒ fetch 停顿 ⇒ 走 CtrlLink 有状态 halt（capability 已就位）
- [ ] **Step 3: exception handler 注册（★v2：**移出本 change**）** —— exception handler 的注册归 **C.1**（`mmu-chmem-phase-b-c-launch`，`mmu_exception_handler_chmem.h:26-28` 明写「接线归 C.1」）⇒ **本 change 不碰注册**（v1 越界 ⇒ 与 C.1 双 owner）。⚠️ **守卫方向更正**：v1 说「解除 `#ifndef CF_PLUGIN_USE_CH_MEM` 包裹」是**错的** —— `cpu_factory_chmem.h:342-348` 是**反向**守卫（TLM 版**仅在非 CH_MEM 编译**）⇒ 正确动作是**在 CH_MEM 分支新增注册**（由 C.1 负责）⇒ 本 change 只**消费** `CPU_EXCEPTION_CODE`
- [ ] **Step 4: 无静默退化** —— MMU 配置与实际行为必须一致（`enable_mmu=true` 但没接 MMU = 现状的不一致 ⇒ 本 change 消除它）；本 change 只负责 **MMU/PTW 本体的注册**，exception handler 的注册由 C.1 负责 ⇒ 若 C.1 未完成 ⇒ 本 change 归档时**显式登记「exception handler 待 C.1」**，不假装完整
- [ ] **Step 5: 集成测试** —— MMU+流水线端到端（prefill TLB → lookup → 命中 → paddr 正确）

### Task 6: 目录合规（D18）

- [ ] **Step 1: 迁移文件** —— 代码落 `ip/mmu/plugins/`（`mmu_ptw_chmem.h` 从 `ip/cpu/plugins/` 迁出）
- [ ] **Step 2: include 路径更新** —— 更新所有 includer
- [ ] **Step 3: 双 binary 编译通过** —— TLM + CH_MEM 均绿

### Task 7: 收口

- [ ] **Step 1: 8 门禁** + **Step 2: 全量回归 0 退化**（`[mmu]` 59 / `[chmem]` / `[mmu-verilator]` 全部）
- [ ] **Step 3: 台账更新** —— `[mmu]` 59 条 + PTW 相关 22 条从 `partial` 逐条重判（降级理由「回退 vaddr = 已知临时降级」不再成立 ⇒ 改为 `covered`/`partial`（真实差异））
- [ ] **Step 4: ADR-088 §3 PTW 行状态更新** —— 「现在不接线」⇒ 「已接线（按 ADR-085/086 目标架构）」
- [ ] **Step 5: commit**

---

## Acceptance

- [ ] AC-1: PTW 实现**符合 ADR-085 D01/D04/D08/D10/D15/D16/D18/D20/D21 全部约束**（逐条对照表 PASS）
- [ ] AC-2: 代码落 **`ip/mmu/plugins/`**（D18）⇒ `ip/cpu/plugins/mmu_ptw_chmem.h` 已迁出；PTW 用**框架已有**的 `cf::plugin::multi_cycle_fsm::FsmBase`（**复用，不新建**）+ `CF_PLUGIN_USE_FSM_EXEMPT` 标记（ADR-046 先例：`mmu_ptw_chmem.h:96`）
- [ ] AC-3: `PTWCache` **只存非叶 PDE**（D04）⇒ 有 case 证明叶子 PTE 不入缓存；`pt_base` 从 satp **组合派生**（D08）⇒ 非硬编码
- [ ] AC-4: `PTWCache` **只存非叶 PDE**（D04）⇒ 有 case 证明叶子 PTE 不入缓存
- [ ] AC-5: PTW **只回填 TLB**，不直写 PADDR（D15）⇒ 有 case 证明
- [ ] AC-6: 页表读**经 DMemPlugin** 的 `ch_mem`（D16/D21）⇒ 无第二个物理端口
- [ ] AC-7: `pt_base` 从 satp **组合派生**（D08）⇒ 非硬编码
- [ ] AC-8: `cpu_factory_chmem.h` 的 `enable_mmu=true` **真实注册 MMU/PTW**（不再是 plumbing-only no-op）；⚠️ **exception handler 的注册不在本 change scope**（归 C.1）⇒ 归档时若 C.1 未完成，须显式登记「待 C.1」而非默认完整
- [ ] AC-9: sv32 **真走 walk** 的向量测试 PASS（非预载 TLB）+ PTW fault 语义（code 12/13/15）PASS
- [ ] AC-10: 翻译未完成 ⇒ fetch **真实停顿**（CtrlLink 有状态 halt 消费）
- [ ] AC-11: `[mmu]` 59 / `[mmu-chmem-basic]` / `[mmu-verilator]` / `[chmem]` **0 退化**；双 binary 编译通过。⚠️ **例外（显式声明）**：`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` **必须按 D10/D18 重写**（旧实现基于裸 DSL + 违规目录）⇒ 该文件的改动**不算退化**，但必须**逐条核对「旧断言 ⇒ 新断言」的语义等价性**（**不得**删测试或改断言凑绿）
- [ ] AC-12: 8 门禁全 PASS + 全量回归 0 退化
- [ ] AC-13: 台账 PTW 相关条目逐条重判 + ADR-088 §3 PTW 行状态更新

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **ADR-085/086 的目标架构与现有 `PtWalkFsmPlugin` 全面不符** ⇒ 迁移 defer 任务时容易照抄旧形态 | Task 1 Step 2 的**差距表**是任务分解的唯一依据；Task 1 Step 3 明确「B.3' 的前提与 D10 冲突 ⇒ 迁移时改写」 |
| R2 | **物理读端口归属**（D16/D21 归 DMemPlugin）⇒ **确定要改** `dmem_chmem.h`（`mem_` private + 惰性创建于 memory/NORMAL ⇒ fetch/EARLY 拿空句柄 ⇒ 须 setup() 期提前创建并暴露句柄）⇒ 本 change 的受影响文件清单**明确包含** `dmem_chmem.h`（v1 只写「可能要改」⇒ agent 会漏） |
| R3 | 前置（C.1/C.4/C.5）**全部未完成**（`mmu-chmem-phase-b-c-launch/tasks.md:136-138` 全 `- [ ]`，窗口 2026-11-30~12-15；且 **C.1 挂未立项的 `csr-chmem-trap-pc-jump`**，tasks.md:22,34）⇒ **批 2（流水线接线）的实际启动点在 12 月之后**，不由 B1 决定 | ★**拆两批**（v2 结论）：**批 1**（Task 2/3/4 = FsmBase 封装 + TLB + walk 单元级）**不依赖 C.x ⇒ 现在即可启动**；**批 2**（Task 5 接线）排队等 C.x ⇒ 批 1 可独立归档 |
| R4 | 多级 walk（sv39/sv48）在 CH_MEM 侧是**新能力**（TLM 侧 `ptw.cpp` 有完整 walk 可参考，但 TLM 侧是否支持 sv39/sv48 需核实） | Task 1 Step 1 核对 ADR-085 的级数要求；若只要求 sv32 ⇒ 明确范围（不静默缩减） |
| R5 | 「回退 vaddr」的降级行为消失 ⇒ 依赖该降级的既有测试会变 | 台账 22 条 PTW 条目逐条重判（AC-13）+ `[mmu-chmem-basic]` 7 case 的行为钉子需更新（它们的钉子正是「回退 vaddr」！）⇒ **这是必须同步的回归面** |
| R6 | 与 ③ L1Cache 的接口面（MMU → Cache 的 PADDR/有效信号）需一致 | 建议 ③ 与 ⑤ **同时立项**并共用一份接口契约（见索引 §1） |

## 迁移说明

落地为 **两份** plan：`.rddf/plans/chmem-ptw-walk-core.md`（批 1）与 `.rddf/plans/chmem-ptw-walk-wiring.md`（批 2）；本文件为**合并总览**（两批的完整内容 + 边界）。★**owner = `chmem-ptw-walk`**（architect 已定案 2026-10-24）⇒ 拆分后 owner 名沿用 `chmem-ptw-walk-*` 前缀。批 1 建议**完整 rdd-builder P1–P3**；批 2 可 rdd-quick（接线 + 目录 + 收口）。
