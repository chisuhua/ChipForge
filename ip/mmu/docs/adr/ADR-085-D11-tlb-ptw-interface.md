# ADR-085-D11 — TLB ↔ PTW 接口契约

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D11`（父 ADR-085 的 D11） |
| 关联验证判据 | U1 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

定义两个协作单元之间的请求/响应字段、握手时序与「PTW 忙时丢弃不排队」的协议语义。

## 决策正文

**请求**（TLB → PTW）：

| 字段 | 宽度 | 来源 |
|------|------|------|
| `vaddr` | XLEN | TLB 查表端口的输入地址 |
| `asid` | `asid_bits` | 当前 ASID（本期恒 0，位宽预留） |
| `access_type` | 2b | 00=load / 01=store / 10=ifetch（决定 fault code 12/13/15） |

**响应**（PTW → TLB）：

| 字段 | 宽度 | 含义 |
|------|------|------|
| `paddr` | XLEN | 翻译结果（叶 PTE 的 PPN 拼接页内偏移） |
| `perms` | 4b | R/W/X/U |
| `fault_code` | 8b | 0=无 fault；否则 12/13/15（按 access_type） |

**协议**：
- 请求**同拍发起**（TLB miss 的组合派生），无握手延迟。
- PTW 忙时：请求被**丢弃**（不排队），TLB 下一拍重新发起（[D3](ADR-085-D03-tlb-dual-port-arbitration.md)）。
- 响应在 walk 完成的那一拍由 PTW 写回 TLB 的对应 entry（**按 walk 锁存的 vaddr 打标**，见 [D15](ADR-085-D15-walk-refill-tlb-only.md)）。
- **PTW 侧不直接写 `PADDR` / `PADDR_VALID`**（[D15](ADR-085-D15-walk-refill-tlb-only.md)）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D03 — MMU_TLB 的 2 端口与"同拍双 miss"](ADR-085-D03-tlb-dual-port-arbitration.md)
  - [D15 — walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制）](ADR-085-D15-walk-refill-tlb-only.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
