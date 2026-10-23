# ADR-085-D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D05`（父 ADR-085 的 D5） |
| 关联验证判据 | U2 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`（新）、`ip/mmu/configs/` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

用户提案的「cacheline 64B ⇒ 8 项」需要明确 8 项的实体宽度口径。本决策固定 entry 为 8B（与 Sv mode 解耦）并定义 tag 组成。

## 决策正文

**决策**：

| 项 | 决策 | 理由 |
|---|---|---|
| PTE 实体宽度 | sv32 = **4B**，sv39/48 = **8B**（RISC-V spec） | 事实 |
| cache entry 宽度 | **固定 8B**（sv32 时只用低 4B，高 4B 保留为 0） | ⇒ **cacheline 格式与 Sv mode 解耦** ⇒ 未来支持 sv39/48 **不需要改 cache 结构**（否则是"硅后重设计"级别的问题） |
| cacheline size | 可配，**TLM 默认 64B / CH_MEM 默认见 [D17](ADR-085-D17-chmem-capacity-limit.md)** | 用户提案 |
| EPL | = cacheline_size / 8B ⇒ 64B 时为 **8 entries/line**；**实际启用条数以 [D17](ADR-085-D17-chmem-capacity-limit.md) 的 per-mode 表为准**（CH_MEM 默认 4） | 用户提案的 "8" 的来源：64B / 8B |
| sets / ways | 可配；**per-mode 默认值见 [D17](ADR-085-D17-chmem-capacity-limit.md)** | — |

**澄清（对应 §0 Q2）**：用户提案中的"64B ⇒ 8 页"，确切含义是 **8 个 PTE**（同一父表内相邻索引的目录项），**不是** 8 个 4KB 虚拟页。若按 sv32 的 4B 实体宽度算，同一条 64B line 能装 **16 个 PTE**——本决策采纳"固定 8B entry"的口径 ⇒ **8 个/line 恒定**。

**tag 组成**：`{level(2b, 预留 sv39/48 的 L0/L1/L2), parent_ppn, asid}`。
- `level`：同一个 `parent_ppn` 在不同 level 上语义不同 ⇒ 必须区分。
- `parent_ppn`：**下一级表的基址 PPN**，由上一级 PTE 给出 ⇒ 是 lookup 的输入。
- `asid`：**必须预留位宽**（即使当前 `asid_bits=0`）。理由：不同地址空间的 `parent_ppn` 可能相同 ⇒ 不 tag ASID 会**跨进程串页表**。当前 ASID=0（单地址空间）所以不触发，但**位宽现在不定死，将来加 ASID 就要改 line 格式** ⇒ 结构性缺陷。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D04 — PTWCache 只缓存**非叶**目录项（PDE），不缓存叶 PTE](ADR-085-D04-ptwcache-no-leaf-pte.md)
  - [D06 — PTWCache 的 index / set 映射与替换策略](ADR-085-D06-ptwcache-index-replace-policy.md)
  - [D17 — CH_MEM 容量上限（elaboration 节点爆炸）](ADR-085-D17-chmem-capacity-limit.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
