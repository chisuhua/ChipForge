# ADR-085-D20 — `satp` 变更 / `sfence.vma` 的失效路径（rev.2 新增）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D20`（父 ADR-085 的 D20） |
| 关联验证判据 | U10 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

TLBCache / PTWCache 的 tag 不含 `satp.ppn` ⇒ 换根表后旧 entry 仍命中 ⇒ 静默错误翻译。本决策定本期「变更即 flush-all」与后续 `sfence.vma` 的两级机制，并定义飞行中 walk 的中止规则。

## 决策正文

**问题**：TLBCache 与 PTWCache 的 tag 里**不含** `satp.ppn`。若 `satp` 被改写（换地址空间/换根表），旧 entry 仍然命中 ⇒ **用旧页表的翻译** ⇒ 静默错误。rev.1 全文无失效路径，是缺口。

**决策（分两级，按落地成本排序）**：

| 阶段 | 机制 | 成本 |
|------|------|------|
| **本期（walk change）** | **`satp` 变更 ⇒ flush-all**：保留一个影子寄存器 `satp_prev`，`satp_changed = (satp != satp_prev)` ⇒ 同一拍把所有 entry 的 `valid` 写 0（`array_store` 的 next 值全清） | +1 个 `ch_reg` + 1 个比较器 + entry 的 valid 多一路 clear |
| **`sfence.vma` 落地后** | 由 `sfence.vma` 显式触发 flush（全清；ASID 粒度属 ASID 支持范围） | 需 CSR/指令支持（P3#11 `cpu-pipeline-smode-umode`） |

**理由**：本期没有 `sfence.vma` 指令（S-mode CSR 未落地），但 `satp` 的**写入通路**存在（`satp_reg()` 访问器已公开）。没有 flush 就等于"改 satp 不生效"，比"多清一次"危险得多 ⇒ **fail-safe 取全清**。

**已知限制（登记）**：`satp` 全清是粗粒度（每次 satp 写都清空两个 cache）；`sfence.vma` 的 ASID 粒度失效为 follow-up。**不接受"不实现失效"**。

**验证**：U10 — 写 `satp` 后同一 vaddr 的 `PADDR` 必须反映新根表（旧 entry 不得命中）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D08 — `pt_base` 不设独立寄存器，取 `satp` 的组合派生视图（修正 M1）](ADR-085-D08-pt-base-derived-from-satp.md)
  - [D19 — 验证要求（U1–U11 + D19.1 fault 两层语义）](ADR-085-D19-verification-criteria.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
