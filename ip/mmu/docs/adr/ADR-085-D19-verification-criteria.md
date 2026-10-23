# ADR-085-D19 — 验证要求（U1–U11 + D19.1 fault 两层语义）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D19`（父 ADR-085 的 D19） |
| 关联验证判据 | U1–U11 |
| 受影响文件 | `tests/mmu/`、`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

定义 11 条单元级判据、6 项变异对照、e2e 与门禁要求；并以 D19.1 裁决「walk 内 fault（L1，必做）」与「CPU 可见 trap（L2，推迟）」的分层，消除 U9 与 U5 的口径矛盾。

## 决策正文

**单元级**（plugin 级 `pb` + 直接注入）：

| # | 判据 |
|---|------|
| U1 | TLBCache：EPL 路并行 tag 比较；命中 ⇒ 非恒等翻译；miss ⇒ `PADDR_VALID=0` 且 `PTW_ACTIVE=1` |
| U2 | PTWCache：line 内 EPL 路并行比较；命中 ⇒ 给出 `next_ppn` |
| U3 | **`start_level` 选择**（[D7](ADR-085-D07-start-level-selection.md)）：构造"L1 目录项命中" ⇒ `start_level = L0` ⇒ walk **只读 1 次**内存（叶 PTE），不读目录项；冷 cache ⇒ `start_level = L1` ⇒ 2 次 |
| U4 | 叶权限检查（spec step 5）：叶 PTE 权限不满足 ⇒ fault 15（store）/13（load）/12（ifetch） |
| U5 | **编码判定（[D13](ADR-085-D13-pte-encoding-and-permission.md) 核心回归锚，须双向）**：**负向** `{V=1,R=0,W=1,X=0}` 与 `{V=1,R=0,W=1,X=1}` ⇒ fault；**正向** `{V=1,R=1,W=0,X=0}`（只读叶）、`{V=1,R=1,W=1,X=0}`（RW 叶）、`{V=1,R=0,W=0,X=1}`（X-only 叶）⇒ **各自翻译成功**。⚠ 只有负向会奖励"全判 fault"的实现 |
| U6 | superpage（[D14](ADR-085-D14-superpage-two-sided.md)）：sv32 `k=1` 叶 ⇒ `paddr = (PPN[1]<<22) \| (vaddr & 0x3FFFFF)`；并断言 `paddr` 与 4KiB 页路径**不同**（防止退化） |
| U7 | walk 期间地址变更 ⇒ 旧 walk 结果**不**回填到新地址（[D15](ADR-085-D15-walk-refill-tlb-only.md) / FS-C-14） |
| U8 | `PTW_ACTIVE` 在 miss 检出**同拍**为 1（FS-C-12，不靠桩） |
| U9 | **负向门的分层裁决（见下方 D19.1）**：坏 PTE ⇒ walk 进 `FAULT` 态、产出 `fault_code`、**不产出 `{paddr, perms}`**、不回填 TLB（`PADDR_VALID` 保持 0、`PTW_ACTIVE` 保持 1）。⚠ **`EXCEPTION_CODE` cell 的消费侧接线**（→ trap）按 protocol FS-C-18 推迟到 trap 同批，**不是**"不做编码检查" |
| U10 | **失效路径（[D20](ADR-085-D20-satp-invalidation-path.md)）**：① 写 `satp` 后同一 vaddr 的 `PADDR` 必须反映**新根表**（旧 entry 不得命中）；② `satp` 在 walk 飞行中改写 ⇒ 该次 walk **中止**且**不回填** TLB（R8） |
| U11 | **TLB superpage entry（[D14](ADR-085-D14-superpage-two-sided.md) §D14.1）**：4MiB 叶回填后，`vaddr` 落在同一 4MiB 内的**不同 4KiB 子页**都必须命中**同一条** entry 且 `PADDR` 正确 |

#### D19.1 fault 的两层语义（**规范裁决，防止口径漂移**）

[D13](ADR-085-D13-pte-encoding-and-permission.md) 要求"编码不对 ⇒ 页错误"，而 protocol FS-C-18 把 `EXCEPTION_CODE` cell 的消费侧接线推迟到 trap 批次 ⇒ 两个"fault"指的不是同一层。本决策明确分层：

| 层 | 内容 | 归属 |
|----|------|------|
| **L1 — walk 内 fault（本 change 必做）** | 编码/保留位/叶权限判定（[D13](ADR-085-D13-pte-encoding-and-permission.md) step 3/5）+ FSM 进 `FAULT` 态 + 产出 `fault_code`（12/13/15）+ **不产出 `{paddr, perms}`**、不回填 TLB | `chmem-ptw-walk`（U4/U5） |
| **L2 — CPU 可见 trap（推迟）** | 把 `fault_code` 写进 `EXCEPTION_CODE` payload cell，由 trap/CSR 侧消费 | 与 FS-C-18 / trap 同批（`cpu-pipeline-smode-umode` 前后） |

⇒ **U9 的"不产生 fault"只指 L2**。**L1 不可推迟** —— 推迟 L1 等于 walk 对坏 PTE 静默产出错误的 `paddr`（比缺 trap 严重得多）。

**变异对照（项目惯例 ≥3，本决策列 6）**：① 去掉 PTWCache 命中短路 ⇒ U3 RED；② `is_reserved` 写回 `r && w` ⇒ U5 RED；③ superpage 拼接退化为直传 ⇒ U6 RED；④ `PTW_ACTIVE` 改成 FSM busy ⇒ U8 RED；⑤ TLB entry 的 `page_shift` 恒 12 ⇒ U11 RED；⑥ 删掉 `satp_changed` 的 flush ⇒ U10 RED。

**e2e**：真页表（页表内容在内存里）走完整 walk ⇒ 非恒等翻译生效。

**门禁**：5 个架构 gate 全 PASS（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check`）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D09 — walk FSM 的形态与状态集](ADR-085-D09-walk-fsm-state-set.md)
  - [D12 — `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态](ADR-085-D12-ptw-active-combinational.md)
  - [D13 — 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**）](ADR-085-D13-pte-encoding-and-permission.md)
  - [D20 — `satp` 变更 / `sfence.vma` 的失效路径（rev.2 新增）](ADR-085-D20-satp-invalidation-path.md)
