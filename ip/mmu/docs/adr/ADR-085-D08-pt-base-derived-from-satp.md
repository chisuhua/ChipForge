# ADR-085-D08 — `pt_base` 不设独立寄存器，取 `satp` 的组合派生视图（修正 M1）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D08`（父 ADR-085 的 D8） |
| 关联验证判据 | U10 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

这是修正项 M1。独立 `pt_base` 寄存器会形成第二个真值源 ⇒ 「模式改了但根表地址没改」的静默错误翻译（v0.10.4 hotfix `a3afcfa` 的同款事故）。本决策定派生位域与 override 入口形态。

## 决策正文

**决策**：

```
sv32 (RV32，本期唯一支持):  satp 是 32 位寄存器
    MODE = satp[31]      (1 = Sv32, 0 = Bare)
    ASID = satp[30:22]   (本期恒 0，位宽预留)
    PPN  = satp[21:0]    ⇒ pt_base = { mode: satp[31], ppn: satp[21:0] }

sv39 / sv48 (未来，RV64 才有):  satp 是 64 位寄存器
    MODE = satp[63:60],  PPN = satp[43:0]
    ⇒ 当前 RV32 上不存在，仅登记形态，不实装
```

> ⚠ **不要写"通式" `ppn = satp[PPN_BITS-1:0]` + `mode = satp[MODE_BITS-1:12]`**：对 sv32 而言 `satp[31:12]` 会**把 ASID 切进 mode**（MODE 只有 bit31，ASID 占 [30:22]）⇒ 这是 rev.1 的错误，已删。⇒ **按 mode 写具体位域，不写参数化通式。**

**不引入独立 `pt_base` 寄存器**。理由：

1. **单一真值源**：`satp` 是 CSR 的规范来源。独立的 `pt_base` 寄存器 = 第二个真值源 ⇒ 两者可能漂移（一个被 `csr_write_satp` 更新、另一个没更新）⇒ 症状是"模式改了但根表地址没改"⇒ **静默的错误翻译**。
   > 项目前例：v0.10.4 hotfix `a3afcfa` 修的正是 `MMUPlugin` 的 `satp_value_` / `satp_ppn_` 双字段语义漂移（TLM 侧 `MMUPlugin.cpp:89-93` 的注释记录了完整过程）。本决策是那次事故的制度化预防。
2. **零成本**：派生是组合逻辑（`bits<>` + 掩码），不占寄存器，不占时序。
3. **CH_MEM 的具体好处**：`satp` 已是 `ch_reg`（成员声明 `MMUPlugin_chmem.h:294`，**访问器 `satp_reg()` 在 `:160`**），读它就是读寄存器 proxy 节点 ⇒ 组合派生天然是 live 节点。

**若将来需要"独立于 satp 的根表"**（调试用影子页表、测试注入）：用**显式的输入端口**（`bind_pt_base_override()`），而不是第二寄存器 ⇒ override 是"影子"，仍以 satp 为默认真值。

**`satp` 的初值双通道纪律**（已踩过的坑）：`Simulator::reset()` 会把 `ch_reg` 打成 `rst_val`（ctor 未给 ⇒ nullptr ⇒ 0）⇒ `satp` 必须**同时**走 ctor init 与 next 值 `<<=`（现有实现见 `MMUPlugin_chmem.h:185-187` ⇒ 本决策继承）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D12 — `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态](ADR-085-D12-ptw-active-combinational.md)
  - [D20 — `satp` 变更 / `sfence.vma` 的失效路径（rev.2 新增）](ADR-085-D20-satp-invalidation-path.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
