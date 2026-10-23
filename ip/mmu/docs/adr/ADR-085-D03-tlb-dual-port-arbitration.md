# ADR-085-D03 — MMU_TLB 的 2 端口与"同拍双 miss"

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D03`（父 ADR-085 的 D3） |
| 关联验证判据 | U1 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

TLB 有 ifetch / loadstore 两个查表端口，可同拍 miss。本决策定义 walk 请求的仲裁与丢弃重试策略。

## 决策正文

两个端口可同拍 miss。**决策**：TLB 的 miss **信号**可以同拍产生 2 个；**walk 请求**由 PTW 侧仲裁（[D11](ADR-085-D11-tlb-ptw-interface.md)），优先级 **ifetch > loadstore**；低优先级的请求**丢弃**（不排队）⇒ 下一拍重新发起。

理由：排队需要 FIFO + 上下文保存，在 CH_MEM 的组合数据通路里成本高、收益低；且"丢弃并重试"是自愈的（下一拍 TLB 仍 miss ⇒ 重新请求）。

**风险**：ifetch 与 loadstore 持续同时 miss ⇒ loadstore 可能**饥饿**。记录为已知限制（§7 R3）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D11 — TLB ↔ PTW 接口契约](ADR-085-D11-tlb-ptw-interface.md)
  - [D16 — PTW 的物理内存读端口（绕过翻译）](ADR-085-D16-ptw-physical-read-port.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
