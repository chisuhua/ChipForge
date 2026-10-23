# ADR-086-D08 — L0 TLB 的组织（8 项全相联 + **多粒度** + perms）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D02 entry 组织） |
| 架构文档 | [mmu-l0-tlb-architecture.md](../mmu-l0-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D08` |
| 关联验证判据 | U-L1-01、U-L1-04、U-L1-05 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l0-tlb-architecture.md`](../mmu-l0-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

现状 CH_MEM TLB = 8 项全相联，但 entry 只有 `{valid, vaddr, paddr}`：**无 perms**（权限无处落）、**无显式 tag**（用全 `vaddr[31:12]` 直接比）、**无 `page_shift`**（ADR-085 D14 要求）。用户已定「**L0 支持多粒度**」。

## 决策正文

**决策**：

```
L0 TLB = 8 项全相联（1 set × 8 ways）
每项 = {valid, global, perms, tag(27b), pfn(44b), asid, reserved}   # D07 的 leaf entry（12B）
```

**比较（per-entry 掩码，1 拍）**：

```
mask_i    = ~0 << page_shift_i
hit_i     = valid_i && (entry_tag_i == (vaddr & mask_i)) && (asid/global 匹配)
hit       = OR(hit_i)
paddr     = select(hit_i, (entry_pfn_i << page_shift_i) | (vaddr & mask_i))
```

**D08.1 多粒度支持（用户已定：支持）**：L0 的 8 个槽位**各自带 `page_shift`** ⇒ 4 KiB 与 2 MiB entry 可共存 ⇒ 比较器为 **per-entry 掩码比较**（8 份掩码）。
⇒ **好处**：L1 命中回填 L0 时，**大页 entry 也能回填**（否则大页访问永远 2 拍，ADR-086 D03 的回填落空）。
⇒ **代价**：L0 的 8 路比较器各带自己的掩码 ⇒ 面积 ×8（仅 8 个槽位，远小于 L1）。
⇒ **`page_shift` 的存放位置**：per-entry ⇒ **进 entry**（D07 的 `reserved` 3 bit 中的 3 bit 正好够）⇒ 与 L1 的 per-way 粒度**不同**（L1 的 `page_shift` 是 way 级常量，不进 entry）⇒ 两者不冲突，但实现时要区分命名（`entry.page_shift` vs `way.page_shift`）。

**D08.2 命中优先级**：同一 VA 可能被 4 KiB entry 与 2 MiB entry 同时覆盖 ⇒ **规定小粒度优先**（4 KiB 更"具体"）⇒ 命中顺序 = 槽位序号小的优先 ⇒ 配置上把 4 KiB entry 放在前面。

**D08.3 与 ADR-085 D02 的关系**：ADR-085 D02 已要求 TLBCache 补 `perms` 与 `page_shift` ⇒ 本决策是它的**两级化**（8 项 → L0 8 项 + L1 2048 ways），字段集一致（D07）。

**D08.4 替换策略**：8 项全相联 ⇒ 满时选 victim。现状 CH_MEM 无策略（固定槽位覆盖）⇒ 本期**沿用固定槽位/轮转**（不引入组合替换逻辑，理由同 ADR-085 D06）⇒ 登记为已知限制。

**D08.5 端口**：2 个查表端口（ifetch / loadstore）**共享同一存储体** ⇒ 2 份比较逻辑 ⇒ 面积 ×2（已知代价，ADR-085 D02 已记录）。

## 交叉引用

- 架构文档：[mmu-l0-tlb-architecture.md](../mmu-l0-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D02 entry 组织、D14 `page_shift`）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D03](ADR-086-D03-l1-hit-backfill-l0.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
