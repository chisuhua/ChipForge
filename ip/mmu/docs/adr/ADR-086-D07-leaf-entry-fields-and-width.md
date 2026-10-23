# ADR-086-D07 — leaf entry 的字段与 12B 宽度（Sv39）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D05 固定 8B entry） |
| 架构文档 | [mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D07` |
| 关联验证判据 | U-L1-05、U-L1-11 |
| 受影响文件 | `ip/mmu/lib/tlb_entry.h`、`ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l1-tlb-architecture.md`](../mmu-l1-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

ADR-086 rev.1 的 D07 按 **sv32** 算（tag 20 + pfn 22 + … = 47 bit ⇒ 8B 够）。**平台改为 Sv39 后不再成立**：tag = 27 bit、pfn = 44 bit ⇒ 需 12B。

## 决策正文

**决策**：leaf entry（L0 / L1 共用同一结构）字段集（**Sv39**）：

| 字段 | 宽度 | 说明 |
|------|-----:|------|
| `valid` | 1 | 有效位 |
| `global` | 1 | RISC-V Global 位（ASID 忽略）|
| `perms` | 4 | R/W/X/U（A/D 位预留，ADR-085 D13）|
| `tag` | **27** | **Sv39 VPN** = `vaddr[38:12]`；比较时按粒度掩码（D06）⇒ 存全 VPN 可让**不同粒度共用同一 tag 字段** |
| `pfn` | **44** | Sv39 PPN，**按该 entry 自身页粒度**存（4 KiB ⇒ 44 bit；2 MiB ⇒ 35 bit）|
| `asid` | **16** | Sv39 ASID 位宽（本期只预留，不参与比较）|
| `reserved` | 3 | 预留 per-entry `page_shift` 等 |
| **合计** | **96 bit = 12B** | ⇒ `cacheline_size = 96B`（8 × 12B，EPL 仍 8）|

**D07.1 若本期不实现 ASID**：裁掉 `asid` 16 bit ⇒ 80 bit ⇒ 仍 > 64 ⇒ **8B 不可行** ⇒ 12B 是下限（可裁到 10B 但不对齐）。

**D07.2 与 ADR-085 D05 的 8B 是不同实体（规范性）**：

| | ADR-085 D05 的 8B | 本决策的 12B |
|---|---|---|
| 实体 | **PTWCache 目录项** entry | **leaf 翻译** entry |
| 字段 | `{level, parent_ppn, asid, next_ppn, perms}` | `{valid, global, perms, tag, pfn, asid, reserved}` |
| 缓存内容 | 页**目录项** | 页**翻译结果** |
| 所属 | ADR-085 的 `PTWCache` | 本组的 `L0 TLB` / `L1 TLB` |

⇒ 两者**不共享结构**、**不共享缓存** ⇒ 实现时不得因"宽度接近"而合并。

**D07.3 `pfn` 的粒度坑（复述 ADR-085 D14 的教训）**：2 MiB entry 的 `pfn` = 2 MiB 页基址 PPN（**35 bit**），**不是** 4 KiB 粒度 PPN（44 bit）。存错 ⇒ `paddr` 静默左移 9 位 ⇒ 译出错误地址且无任何报错 ⇒ 判据 `U-L1-05` 专门锚这一点。

**D07.4 与 `lib/tlb_entry.h` 的关系**：`TLBEntry<TAG_BITS, ASID_BITS>`（`ip/mmu/lib/tlb_entry.h:48-65`）的模板参数需扩到 Sv39 口径（`TAG_BITS = 27`、`ASID_BITS = 16`），且 `pfn` 当前是 `uint_t<64>` ⇒ 44 bit 够用 ✓；但**结构体本身不含 `global`/`reserved` 之外的新字段** ⇒ 只需把 `perms` 的 A/D 预留位与 `page_shift`（per-way ⇒ 不进 entry）说清。

## 交叉引用

- 架构文档：[mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D05 8B 固定宽度、D14 §D14.1 `pfn` 粒度）
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D08](ADR-086-D08-l0-organization.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
