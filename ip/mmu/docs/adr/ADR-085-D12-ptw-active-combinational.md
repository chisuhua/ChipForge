# ADR-085-D12 — `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D12`（父 ADR-085 的 D12） |
| 关联验证判据 | U8 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`.rddf/protocols/fetch_stall_protocol_v0.11.0.md` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

fetch-stall protocol FS-C-12 已用逐拍反例证明：registered busy 实现会晚一拍置位 ⇒ 丢指令 + 垃圾裸提交。本决策定派生式，并明确其**只成立于 miss 窗口内**（fault 排除项属 trap 批次）。

## 决策正文

**决策**：`PTW_ACTIVE = sv_active && !paddr_valid`（组合，同一拍）。

**理由**：fetch-stall protocol **FS-C-12** 已用逐拍反例证明：若 `PTW_ACTIVE` 由 FSM 的 busy 状态驱动，则"registered busy"实现会晚一拍置 1 ⇒ 进入拍 `T` 的 `hold=0` ⇒ `pc_reg` 越过 `B`（丢指令）**且** `T+1` 的 bare-B 垃圾裸提交。protocol §2.1 的 rev.3 注释记录了完整推导。

⇒ **架构含义**：TLB 的 miss 与 PTW 的接受**之间允许有间隙**，但取指冻结必须从 miss 检出同拍开始 ⇒ 冻结由 **TLB** 侧发起，不由 **PTW** 侧发起。

⇒ 这条把 `MMU_TLB` 与 `MMU_PTW` 的耦合点限定得很清楚：**PTW 不参与 stall 决策**。

**⚠ 生命周期的 scope（必读，否则该式会永久冻结取指）**：上式是 **miss 窗口内**的表达式，**不含 fault 排除项**。walk 进 `FAULT` 后 `paddr_valid` 恒 0 ⇒ 若原样用于所有场景，`PTW_ACTIVE` 会永远为 1 ⇒ **PC 永久冻结**（这正是 protocol rev.5 的 BLOCKER，AC-FS-14 ⑤ 已裁决"fault 拍必须排除"）。

| 场景 | `PTW_ACTIVE` 口径 | 归属 |
|------|------------------|------|
| **3c/3g 接线窗口**（本 change） | `sv_active && !paddr_valid`；**窗口定义见 protocol AC-FS-15 ②**（按**取指事实**定义：从该地址 miss 检出起直到 `PADDR_VALID=1` 或测试窗口结束，**含 walk 进 `FAULT` 的拍及其后各拍**）⇒ 上式在**整窗**恒 1，恰好满足该判据 | `chmem-ptw-walk` |
| **fault 排除项**（`&& !walk_fault`） | 与 trap / `EXCEPTION_CODE` 消费侧**同批**落地（`cpu-pipeline-smode-umode` 前后） | 推迟，见 [D19.1](ADR-085-D19-verification-criteria.md) 的 L2 |

⇒ **本 change 不得自行发明 fault 排除逻辑**（会与 trap 批次的 `walk_fault` 语义打架），但也**不得**把上式当成全场景公式写进实现注释 —— 必须注明"miss 窗口内成立"。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D15 — walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制）](ADR-085-D15-walk-refill-tlb-only.md)
  - [D19 — 验证要求（U1–U11 + D19.1 fault 两层语义）](ADR-085-D19-verification-criteria.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
