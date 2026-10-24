# ⑤ chmem-ptw-walk — 执行总览（★指针文档）

> ★**本文件已改为指针**（planner 2026-10-24）：原单 plan 含两批内容 ⇒ 与执行图 §5.1 的两个 change 名（`chmem-ptw-walk-core` / `chmem-ptw-walk-wiring`）不对齐 ⇒ **已拆分为两份独立执行文件**，避免第三份副本漂移。
>
> | 批 | change | 执行文件 | 前置 | 何时可启动 |
> |---|---|---|---|---|
> | **批 1** | `chmem-ptw-walk-core` | **[`chmem-ptw-walk-core.md`](chmem-ptw-walk-core.md)** | **无** | **现在** |
> | **批 2** | `chmem-ptw-walk-wiring` | **[`chmem-ptw-walk-wiring.md`](chmem-ptw-walk-wiring.md)** | C.1 / C.4 / C.5（全部未完成） | **12 月之后** |
>
> 拆分理由：若两批同属一个 change ⇒ 批 1 完成后 change **无法归档**（AC 未全绿）⇒ 会被 C.x 长期阻塞。拆分后 **批 1 可独立归档**（与 ① 的交付点 A/B 拆分同理）。

---

## 共享背景（两批的共同前提）

### 上游

- **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构 —— 目标架构**已锁定**）
- ADR-088 v1.1 §3 PTW 行（**现在不接线**；回退 vaddr = 已知临时降级）
- **owner = `chmem-ptw-walk`**（architect 已定案 2026-10-24）⇒ 拆分后沿用 `chmem-ptw-walk-*` 前缀

### 目标架构硬约束（ADR-085 D01–D21，**不是**自由设计）

| 决策 | 内容 | 对实现的约束 | 批 |
|------|------|-------------|---|
| **D01** | `MMU_TLB` + `MMU_PTW` **拆分** | 两个独立单元，不是「PTW 作为 MMU 内部函数」 | 1 |
| **D04** | `PTWCache` **只存非叶 PDE** | 不能把叶子 PTE 也缓存 | 1 |
| **D08** | `pt_base` 从 **satp 组合派生** | 不是硬编码常量 | 1 |
| **D10** | 用 **`FsmBase` 封装**，而非裸 `ch_state_machine` | **不得**直接用裸 DSL | 1 |
| **D15** | PTW **只回填 TLB**，不直写 PADDR | 禁止 PTW 直接产 PADDR | 1 |
| **D16 / D21** | 页表物理读端口**绕过翻译**；`ch_mem` 所有权归 **DMemPlugin** | 不能自建第二个物理内存端口 | 1 |
| **D18** | 代码落 **`ip/mmu/plugins/`** | 修 ADR-083 的目录违规 | 1（新文件直接落）· 2（旧文件迁移/退役） |

### 明确不做

不重新设计目标架构；不采用现有 `PtWalkFsmPlugin` 的形态（sv32 2 级 / 裸 DSL / 违规目录 / 无 PTWCache ⇒ 与 D04/D10/D18 全面不符 ⇒ 接线必返工）。

### tasks 骨架来源

`openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 的 defer 列表 —— **批 1 收 B.3'/B.4' 的单元级部分，批 2 收 B.2'/B.3''/B.3''' 的接线部分**。

---

## 边界划分（两批 + C.x 三方，无双 owner）

| 归属 | 内容 |
|------|------|
| **批 1** `chmem-ptw-walk-core` | PTW 单元级：`MMU_TLB` / `MMU_PTW` / `PTWCache` / `FsmBase` 封装 / D16 物理读变更面 / 单元测试 |
| **批 2** `chmem-ptw-walk-wiring` | 流水线接线：工厂注册 / fetch stall / D18 旧实现退役 / 台账重判 / ADR-088 §3 更新 |
| **`mmu-chmem-phase-b-c-launch`** | **C.1** exception handler 注册 · **C.4** `MMUPluginChmem` 注册载体 · **C.5** `hazard_chmem` `halt_when` wiring · **C.6** 集成测试 |

---

## 残余风险（合并两批的风险表）

| # | 风险 | 缓解 | 归属批 |
|---|------|------|--------|
| R1 | ADR-085/086 目标架构与现有 `PtWalkFsmPlugin` 全面不符 ⇒ 照抄旧形态 | 批 1 Task 1 的**差距表**是唯一任务分解依据 | 1 |
| R2 | D16/D21 ⇒ **确定要改** `dmem_chmem.h`（`mem_` private + 惰性创建于 memory/NORMAL ⇒ 须 setup() 期提前创建并暴露句柄） | 受影响文件清单**明确包含** `dmem_chmem.h` | 1 |
| R3 | **误把「单元级」做成「半接线」** ⇒ 与批 2 双写 | 批 1 AC-9 钉死「外部可观测行为零变化（cycle 逐位不变）」 | 1 |
| R4 | 多级 walk（sv39/sv48）在 CH_MEM 侧是**新能力** | Task 1 Step 1 核对 ADR-085 级数要求；只要求 sv32 ⇒ **明确范围不静默缩减** | 1 |
| R5 | 「回退 vaddr」降级行为**本批不变** ⇒ 既有测试钉子不动 | ✅ 批 1 不接线 ⇒ `[mmu-chmem-basic]` 7 case 钉子保持 | 1 |
| R6 | 前置 C.1/C.4/C.5 **全部未完成**（C.1 自身还挂未立项的 `csr-chmem-trap-pc-jump`）⇒ 批 2 12 月后才开工 | 批 2 Task 5 Step 1 **硬闸**（任一未完成 ⇒ 不启动，不降级部分接线） | 2 |
| R7 | 「回退 vaddr」消失 ⇒ 依赖该降级的既有测试会变 | 台账 22 条逐条重判（含族级 §2 同步）+ `[mmu-chmem-basic]` 钉子更新 | 2 |
| R8 | 与 ③ L1Cache 的接口面（MMU → Cache 的 PADDR）需一致 | 批 2 是 PADDR 真正产出的时刻 ⇒ **必须与 ③ 同期对齐接口契约** | 2 |

---

## 排期结论

| 批 | 内容 | 前置 | 何时可启动 |
|---|------|------|-----------|
| **批 1** = change **`chmem-ptw-walk-core`** | 单元级全部 | **无** | **现在**（B1 里唯一「完全无前置」的实质工作） |
| **批 2** = change **`chmem-ptw-walk-wiring`** | 接线全部 | C.1/C.4/C.5 + 批 1 归档 | **12 月之后**（C.x 窗口 2026-11-30~12-15） |

⇒ **批 1 必须独立可归档**（AC 独立于批 2）；批 2 的 AC 标注「待 C.x」。
