# ADR-086-D08 — L0 TLB 的组织（8 项全相联 + 多粒度 + perms）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D02 entry 组织） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D08` |
| 关联验证判据 | U-L1-01、U-L1-04、U-L1-05 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

现状 CH_MEM TLB = 8 项全相联，但 entry 只有 `{valid, vaddr, paddr}`：**无 perms**（权限无处落）、**无 tag 语义**（用全 `vaddr[31:12]` 直接比）、**无 `page_shift`**（ADR-085 D14 要求）。本决策定 L0 的完整组织。

## 决策正文

**决策**：

```
L0 TLB = 8 项全相联（1 set × 8 ways）
每项 = {valid, tag(20b), pfn(22b), perms(4b), asid, global}  # D07 的 leaf entry
实现 = 8 个 ch_reg<ch_uint<64>>（整 entry 一个 reg）+ 1 个 valid 位（可并入 entry）
```

**比较（无掩码，1 拍）**：

```
hit_i = entry_valid_i && (entry_tag_i == vaddr[31:12]) && (asid/global 匹配)
hit   = OR(hit_i)
paddr = select(hit_i, (entry_pfn_i << entry_shift_i) | (vaddr & mask_i))
```

**D08.1 多粒度支持（与 D06 的分工）**：D06 把 4KiB 归 L0、大页归 L1；但 D03 指出「L1 命中回填 L0」需要 **L0 能存大页 entry** ⇒ **L0 必须支持多粒度** ⇒ **L0 的比较方式必须是 per-entry 掩码比较**（不是全 VPN 直比）：

```
hit_i = valid_i && ((entry_tag_i & mask_i) == (vaddr_tag & mask_i)) && asid/global 匹配
mask_i = 由该 entry 的 page_shift 决定
```

⚠ **代价**：L0 的 8 路比较器变成 per-entry 掩码比较 ⇒ **面积 ×8**（8 份不同掩码）。
⇒ **本期默认：L0 支持多粒度**（per-entry 掩码比较）—— 因为 D03 的「L1→L0 回填」需要它，否则大页访问永远 2 拍，L1 的容量收益无法摊薄。代价仅限 L0 的 8 个槽位（远小于 L1 的 8 way）。
⇒ 该取舍登记为架构文档 §0 **Q2**（用户可否决：否决则 L0 退化为单粒度、大页不回填）。

**D08.2 与 ADR-085 D02 的关系**：ADR-085 D02 已要求 TLBCache 补 `perms` 与 `page_shift` ⇒ 本决策是它的**两级化**（8 项 → L0 8 项 + L1 2048 ways），字段集一致。

**D08.3 替换策略**：8 项全相联 ⇒ 满时选 victim。现状 CH_MEM 无策略（固定槽位覆盖）⇒ 本期**沿用固定槽位/轮转**（不引入组合替换逻辑，理由同 ADR-085 D06）⇒ 登记为已知限制。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D02 entry 组织、D14 `page_shift`）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D03](ADR-086-D03-l1-hit-backfill-l0.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
