# ADR-085-D10 — walk FSM 用框架级 `FsmBase` 封装（不再裸用 `ch_state_machine`）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D10`（父 ADR-085 的 D10） |
| 关联验证判据 | U9 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`、`include/cf/plugin/multi_cycle_fsm.h` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

项目现有 3 条 FSM 路线并存（2 处 `FsmBase` + 2 处裸用 `ch_state_machine`）。本决策统一到框架级 `FsmBase`，并顺带满足 protocol 的「句柄同源」机械锚。

## 决策正文

**决策**：PTW FSM 继承 `cf::plugin::multi_cycle_fsm::FsmBase<StateEnum, NStates>`（`include/cf/plugin/multi_cycle_fsm.h:46-126`），而不是直接用 `chlib::ch_state_machine`。

**理由**：
1. `FsmBase` 已封装 `ctx_swap` + `set_entry(enum 0)` + 逐 state `on_active` 注册 + `build()`（`multi_cycle_fsm.h:85-104`）⇒ 消除手写样板。
2. `FsmBase::state_out()` 直接给出状态输出节点 ⇒ **正好满足 protocol AC-FS-15 的"句柄同源"机械锚**（`pb.plugins()` 取句柄 + 指针相等）。
3. **一致性**：项目已有 2 处用 `FsmBase`（`ip/cpu/arch/riscv/mul_div_fsm.h:122`、`ip/cpu/plugins/mul_div_fsm_chmem.h:79`），而 `mmu_ptw_chmem.h` 与 `l1_cache_refill_fsm_chmem.h` 是裸用 ⇒ 3 条路线并存是技术债。
4. `FsmBase` 提供 `cap.provide("multi_cycle_fsm", this)`（`multi_cycle_fsm.h:65-67`）⇒ 走 ADR-082 的 capability 协商 ⇒ 未来若需要"多 FSM 依赖关系"有现成机制。

**注意**：`FsmBase::create_chmem_fsm()` 要求 entry state 的**枚举值必须是 0**（`multi_cycle_fsm.h:96`）⇒ `IDLE=0` ✓。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D09 — walk FSM 的形态与状态集](ADR-085-D09-walk-fsm-state-set.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
