# ADR-085-D15 — walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D15`（父 ADR-085 的 D15） |
| 关联验证判据 | U7 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

TLM 侧在 walk 回调里直接写 `PADDR` cell，在 CH_MEM 的组合 DAG 里会绕过查表 ⇒ 违反 FS-C-13/14。本决策定「回填 TLB + 锁存 walk_vaddr」的三条规范性约束。

## 决策正文

**决策**（规范性，三条）：
1. **PTW 不直接写 `PADDR` / `PADDR_VALID`**。walk 成功后，PTW 把结果写进 **TLB 的对应 entry**（key = walk 锁存的 vaddr）；`PADDR` / `PADDR_VALID` **永远由 TLB 查表产生**。
2. walk 请求时锁存 `walk_vaddr`（`ch_reg`）；结果只回填该 vaddr 的 entry。
3. walk 期间取指地址改变（branch redirect）**不取消** walk，只是不回填到新地址。

**理由**：
- 这三条**自动满足** fetch-stall protocol 的 **FS-C-14**（walk 期间的地址变更不得让旧 walk 的结果污染新地址）与 **FS-C-13**（`PADDR_VALID` 是逐地址的组合有效位，非脉冲）——不需要额外的取消/打标逻辑。
- TLM 侧的做法**不可照搬**：TLM 的 `MMUPlugin.cpp:130-136` 在 walk 回调里**直接写** `(*node)(Key::PADDR)` / `PADDR_VALID` ⇒ 在 TLM 的命令式模型下可行（回调闭包捕获了 node 与 vaddr），但在 CH_MEM 的组合 DAG 里，"直接写 cell"会**绕过查表** ⇒ 违反 FS-C-13/14 ⇒ **CH_MEM 侧必须走 TLB 回填路径**。

⇒ 这条同时把 `MMU_TLB` 与 `MMU_PTW` 的职责边界钉死（§2.1）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D09 — walk FSM 的形态与状态集](ADR-085-D09-walk-fsm-state-set.md)
  - [D11 — TLB ↔ PTW 接口契约](ADR-085-D11-tlb-ptw-interface.md)
  - [D12 — `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态](ADR-085-D12-ptw-active-combinational.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
