# ADR-085-D06 — PTWCache 的 index / set 映射与替换策略

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D06`（父 ADR-085 的 D6） |
| 关联验证判据 | U2 |
| 受影响文件 | `ip/mmu/policies/`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

CH_MEM 是组合逻辑通路，`ip/mmu/policies/` 的虚函数替换策略不可直接使用。本决策定 index 映射、CH_MEM 侧的组合式 victim 选择策略与 lib/plugins 分层。

## 决策正文

**决策**：
- **index** = `parent_ppn` 的低位（低 `log2(sets)` 位）⇒ 不同父表可能映射到同一 set ⇒ 需要 `parent_ppn` 做 tag ⇒ 冲突由替换策略处理。
- **替换策略**：复用 `ip/mmu/policies/` 的抽象（None / FIFO / LRU / RRIP），**但**：
  - `policies/` 是**纯 C++ 虚函数**（运行期调用，F6）⇒ **CH_MEM 组合逻辑不可直接使用**（虚函数调用是 elaboration 期的 C++ 代码，不是 DAG 节点；虽然可以"在 elaboration 期算出 victim"但那要求 `<<=` 表达式是节点，而虚函数返回的是 C++ 值 ⇒ 不可行）。
  - ⇒ **CH_MEM 需要组合式 victim 选择器**：4-way 用 PLRU（每 way 1 bit + 树形比较）；RRIP 需要 counter（组合加法器），成本更高。
  - ⇒ **本期决策：CH_MEM 的 PTWCache 用 1 way（直接映射）+ 多 set**，规避组合替换逻辑；TLB 保持全相联（现状）。**未来上 4-way 时用 PLRU**。
- **`lib/` 与 `plugins/` 的分层**：替换策略的**算法语义**（哪个 victim 逻辑上"最老"）放 `lib/`（纯 C++，可单测）；**组合选择器**（PLRU 位操作）放 `plugins/`（CH_MEM）。二者由同一份 spec 定义，避免语义漂移。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配](ADR-085-D05-ptwcache-entry-width-epl.md)
  - [D17 — CH_MEM 容量上限（elaboration 节点爆炸）](ADR-085-D17-chmem-capacity-limit.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
