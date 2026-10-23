# ADR-085-D01 — MMU 分解为 MMU_TLB + MMU_PTW（两个协作单元，同一 MMU 插件内）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D01`（父 ADR-085 的 D1） |
| 关联验证判据 | U1 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

现有 MMU 把「查表」与「多级页表遍历」混在同一插件里，导致 TLB 命中路径被 PTW 的多周期结构污染。本决策确定两者的物理分工边界，是后续 20 条决策的骨架。

## 决策正文

**决策**：MMU 在实现上分为两个**协作单元**，对外表现为**一个** MMU 插件（保持 `MMUPluginChmem` 的单一注册点，避免破坏 ADR-040 §10.6 的顺序契约）。

**理由**：
- 与业界一致（Rocket: TLB + PTW；BOOM: TLB + PTW；CVA6: TLB + PTW + PTW cache），迁移到真实核时心智模型零成本。
- **职责的物理性质不同**：TLB 是组合查表（0/1 拍），PTW 是多周期串行引擎（4~6 拍）。合成一个单元会导致"TLB 命中路径被 PTW 的多周期结构污染"。
- **ADR-046 的边界更清晰**：只有 PTW 需要多周期 FSM 豁免（`CF_PLUGIN_USE_FSM_EXEMPT`），TLB 保持纯组合。

**后果**：MMU 内部有两个"查表"结构（TLBCache / PTWCache），需要分别的容量参数。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D04 — PTWCache 只缓存**非叶**目录项（PDE），不缓存叶 PTE](ADR-085-D04-ptwcache-no-leaf-pte.md)
  - [D07 — PTWCache 命中 ⇒ `start_level` 选择（"从哪一级开始 walk"）](ADR-085-D07-start-level-selection.md)
  - [D12 — `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态](ADR-085-D12-ptw-active-combinational.md)
  - [D15 — walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制）](ADR-085-D15-walk-refill-tlb-only.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
