# ADR-085-D09 — walk FSM 的形态与状态集

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D09`（父 ADR-085 的 D9） |
| 关联验证判据 | U9 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`、`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

现有 `PtWalkState` 把 level 编进状态（`L0_WAIT`/`L1_WAIT`）⇒ 未来支持 sv39/48 需状态爆炸。本决策改为「固定 5 态 + `cur_level` 寄存器」，并锁定枚举值不变以免破坏既有断言。

## 决策正文

**决策**：状态集 = `{IDLE=0, REQ=1, WAIT=2, DONE=3, FAULT=4}` + **一个 `cur_level` 寄存器**（不把 level 编进状态）。

| 状态 | 语义 | 拍行为 |
|------|------|--------|
| `IDLE` | 空闲 | 接受 walk 请求 ⇒ 下一拍进 `REQ` |
| `REQ` | 发出**首级** PTE 读 | 呈现首级读地址（由 `satp` 派生的 `pt_base` + `VPN[i]` 算出）；若本拍 `pte_valid` 已有效（cache 命中 / 0 拍驱动）也**在本拍捕获** ⇒ 下一拍进 `WAIT` |
| `WAIT` | 收并判定**第 2 级起**的 PTE | 收数据 ⇒ 编码判定（[D13](ADR-085-D13-pte-encoding-and-permission.md)）⇒ **指针**：呈现下一级地址、**留在 `WAIT`**（回 `REQ` 会多耗一拍）；**叶** ⇒ `DONE` |
| `DONE` | walk 成功 | 产出 `{paddr, perms}` + 回填 TLB ⇒ 下一拍回 `IDLE` |
| `FAULT` | walk 失败 | 产出 `{fault_code}` ⇒ 下一拍回 `IDLE` |

**为什么用 `cur_level` 寄存器而不是把 level 编进状态**：
- sv32=2 级 / sv39=3 级 / sv48=4 级 ⇒ 编进状态需要 2×2×2×3×3 = 多个状态组合，状态爆炸。
- `cur_level` 寄存器让状态集**与 Sv mode 无关** ⇒ 未来支持 sv39/48 **零状态扩展**。

**关键兼容性收益**：本状态集的**枚举值与现有 `PtWalkState` 完全一致**（`mmu_ptw_chmem.h:131-137`：`IDLE=0, L0_WAIT=1, L1_WAIT=2, DONE=3, FAULT=4`）⇒

- **枚举值不变 ⇒ 断言值不变**：现有 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 的断言 `state()==1 / ==2 / ==3 / ==4` **数值不用改**（语义上 `1` 从 `L0_WAIT` 变为 `REQ`、`2` 从 `L1_WAIT` 变为 `WAIT`）。
- **但"零改动可继续通过"是错的（rev.1 的错误陈述，已撤回）**：§9 会**删除 `PtWalkFsmPlugin`**（3a 的自由函数化），而该测试的 `PtwFixture`（`test_mmu_ptw_fsm_chmem.cpp:50-66`）**直接实例化这个类** ⇒ 插件一删，fixture 编译失败。⇒ 测试必须改写为：调用 `build_pt_walk_fsm()` 自由函数 + 由生产插件（`MmuPtwPlugin`）暴露 **`ptw_fsm()` 访问器**（这也是 fetch-stall protocol **AC-FS-15 (a) 句柄同源锚**所要求的）。**准确表述：零"断言值"变更，"构造方式"必须改。**
- fetch-stall protocol 的 **AC-FS-15 anti-vacuity 机械锚**（`state_out() == PtWalkState::FAULT`，枚举值 **4**）**数值上零改动成立**（但见 [D13](ADR-085-D13-pte-encoding-and-permission.md)：它喂的坏 PTE **编码**要改）。

**逐拍对齐验证**（现有测试的驱动序列在新语义下依然自洽）：

| tick | 旧语义 | 新语义 | 测试驱动 | 转移 |
|------|--------|--------|---------|------|
| 0 | IDLE，`start=1` ⇒ L0_WAIT | IDLE，`start=1` ⇒ REQ（发出 L1 目录项地址） | `start=1` | IDLE→REQ ⇒ `state()==1` ✓ |
| 1 | L0_WAIT，收 L1 PTE ⇒ L1_WAIT | **REQ**：本拍 `pte_valid` 已直驱 ⇒ 当拍捕获并判定 L1 目录项（指针）⇒ 进 `WAIT` | `pte_valid=1, V=1, R=W=X=0`（指针） | REQ→WAIT ⇒ `state()==2` ✓ |
| 2 | L1_WAIT，收叶 ⇒ DONE | **WAIT**：叶 PTE 就绪 ⇒ 判定 ⇒ `DONE` | `pte_valid=1, V=1, R=1`（叶） | WAIT→DONE ⇒ `state()==3` ✓ |
| 3 | DONE ⇒ IDLE | DONE ⇒ IDLE | 全 0 | ⇒ `state()==0` ✓ |

> **本表是"现有测试的 0 拍直驱场景"**（`pte_valid` 由测试直接驱动，不经物理内存）。真实 `sread`（1 拍）场景见 §4.2：`REQ` 发地址、`WAIT` 收数据，2 级 ⇒ 多 1 拍。两者**状态转移序列相同**（`IDLE→REQ→WAIT→DONE`），只是每状态的停留拍数不同 ⇒ 这正是不改断言值的原因。

⇒ **建议**：保留枚举**值**，把 `L0_WAIT`/`L1_WAIT` 重命名为 `REQ`/`WAIT`，并更新 `mmu_ptw_chmem.h` 与测试里的**注释/命名**（不改断言值）。

**ADR-046 豁免**：walk FSM 必须带 `CF_PLUGIN_USE_FSM_EXEMPT` 标记（既有 `mmu_ptw_chmem.h:96` 已有）。

**转移条件必须用 `transition_when(ch_bool, target)`**，**禁** `if (ch_bool)`（上游 `chlib/state_machine.h:34-46` 显式警告：`ch_bool` 的 contextual bool 转换在 describe 期静默求值为 false）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D10 — walk FSM 用框架级 `FsmBase` 封装（不再裸用 `ch_state_machine`）](ADR-085-D10-fsm-base-wrapper.md)
  - [D15 — walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制）](ADR-085-D15-walk-refill-tlb-only.md)
  - [D19 — 验证要求（U1–U11 + D19.1 fault 两层语义）](ADR-085-D19-verification-criteria.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
