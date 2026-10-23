# ADR-085-D17 — CH_MEM 容量上限（elaboration 节点爆炸）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D17`（父 ADR-085 的 D17） |
| 关联验证判据 | — |
| 受影响文件 | `ip/mmu/configs/`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

CH_MEM 的 `ch_reg` 数组规模 = `sets × ways × EPL × entry_bits` ⇒ 按真实规模落地会产生数千寄存器节点，而 CH_MEM 全量测试已 833s/轮。本决策定 per-mode 默认容量与分阶段抬升。

## 决策正文

**决策**：MMU 的两个 cache 容量**全部配置化**，且 **CH_MEM 模式给一个小默认值**。

**理由**：CH_MEM 的 `ch_reg` 数组规模 = `sets × ways × EPL × entry_bits`。当前 TLB 是 8 项 × 3 个 `ch_reg` = 24 个寄存器；若 PTWCache 按 "64 sets × 4 ways × 8 entries" 落地 = 2048 个 entry ⇒ 每个 entry 若用 2~3 个 `ch_reg` ⇒ **数千个寄存器节点** ⇒ elaboration 节点数爆炸。

后果量化：CH_MEM 全量测试已 **833s/轮**（`AGENTS.md` 记录，2026-10-20 实测）⇒ 节点数再放大一个量级将**不可接受**。

⇒ **默认（CH_MEM）**：

| 结构 | sets | ways | EPL | entries 合计 | 备注 |
|------|-----:|-----:|----:|-------------:|------|
| TLBCache | 1 | 8 | 1 | 8 | 保持现状（全相联 8 项），EPL=1 ⇒ 与现有实现同构 |
| PTWCache | 1 | 1 | 4 | 4 | 直接映射，规避组合替换逻辑（[D6](ADR-085-D06-ptwcache-index-replace-policy.md)） |

⇒ **TLM 模式**可以用真实规模（config 驱动）。

**记录**：这是 **CH_MEM 模式的功能性裁剪**，不是架构目标。架构目标规模在 §8 的实施清单里分阶段抬升。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配](ADR-085-D05-ptwcache-entry-width-epl.md)
  - [D06 — PTWCache 的 index / set 映射与替换策略](ADR-085-D06-ptwcache-index-replace-policy.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
