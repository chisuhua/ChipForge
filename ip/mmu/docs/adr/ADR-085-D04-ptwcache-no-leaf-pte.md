# ADR-085-D04 — PTWCache 只缓存**非叶**目录项（PDE），不缓存叶 PTE

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D04`（父 ADR-085 的 D4） |
| 关联验证判据 | U2 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

这是用户提案的修正项 M3：TLBCache 存叶 PTE、PTWCache 存页目录项，两者职责不重叠。本决策同时给出「PTWCache 同时缓存叶」的替代方案与采纳条件。

## 决策正文

**决策**：PTWCache 的 entry 是 `{valid, asid, level, parent_ppn, next_ppn, perms}`，**不含叶 PTE**。

**理由**：
- **职责不重叠**：叶 PTE 的缓存是 MMU_TLB 的定义职责。若 PTWCache 也存叶，则"TLB miss → walk" 的意义被稀释（walk 可能直接从 PTWCache 拿到叶 ⇒ TLB 变成可有可无）。
- **访问频率差异巨大**：叶 PTE 每个新页访问 1 次；目录项在 1 个页表被多个兄弟索引共享（sv32 的 L1 表有 1024 项 ⇒ 一次 L1 读服务 1024 个 L0 读）⇒ 目录项的复用率远高于叶 ⇒ 分开缓存能各自用合适的容量。
- **一致性成本不同**：叶 PTE 的 A/D 位会被硬件改写（写回）⇒ 缓存叶 PTE 需要处理"缓存副本与内存副本不一致"；目录项是只读的 ⇒ 分开缓存让只读部分彻底免于一致性负担。

**代价（必须记录）**：**walk 最少需要 1 次物理内存访问**（读叶 PTE），即使 PTWCache 全命中。因为叶 PTE 不在 PTWCache 里。

**替代方案（记录，暂不采纳）**：
- **方案 B：PTWCache 同时缓存叶 PTE**（业界 CVA6 的 PTW cache 风格）。优点：全命中时 walk 零访存。缺点：与 TLB 重叠 + 引入叶 PTE 写回的一致性负担 + 污染策略要在"目录项（高复用）"与"叶（低复用）"之间折中。
- 采纳条件：若实测显示"walk 的访存延迟"是 v1.0.0 窗口的性能瓶颈，可作为 `chmem-ptw-fsm-cycle-parity` 的 follow-up 评估。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D01 — MMU 分解为 MMU_TLB + MMU_PTW（两个协作单元，同一 MMU 插件内）](ADR-085-D01-mmu-tlb-ptw-split.md)
  - [D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配](ADR-085-D05-ptwcache-entry-width-epl.md)
  - [D07 — PTWCache 命中 ⇒ `start_level` 选择（"从哪一级开始 walk"）](ADR-085-D07-start-level-selection.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
