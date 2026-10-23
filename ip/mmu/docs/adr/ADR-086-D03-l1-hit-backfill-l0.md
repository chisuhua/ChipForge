# ADR-086-D03 — L1 命中回填 L0（shadow fill）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D03` |
| 关联验证判据 | U-L1-04 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

若 L1 命中**不**回填 L0，则每次访问该页都要付 L1 的 2 拍延迟（见 D02）⇒ L1 的容量优势被延迟抵消，退化为纯容量扩展，L0 的"快路径"角色失效。

## 决策正文

**决策**：**L1 命中 ⇒ 立即回填 L0**（把该 entry 复制进 L0 的全相联表）。

**方向与现行 spec 一致**：`openspec/specs/mmu-multi-level-coherence/spec.md` 的 "MUST implement shadow fill: when a deeper level hits during lookup, the matching entry MUST be inserted into all shallower levels" —— 深命中填浅层，**同向** ⇒ 语义可直接复用，本决策只把它接到 CH_MEM 侧。

**规则（规范性）**：

1. L1 命中 ⇒ 写 L0 的一个槽位（`valid=1` + `{tag, pfn, perms, page_shift}`）。
2. **粒度降级写回**：L1 存的是**大页** entry（`page_shift ≥ WAY_GRANULE`），L0 存的是 **4 KiB** 粒度 ⇒ 回填时**不能**把大页 entry 原样塞进 L0（L0 的 tag 是 4 KiB 粒度）。
   ⇒ **默认：L1 → L0 不回填大页 entry**；大页命中直接用 L1 的结果（2 拍）。
   ⇒ 理由：4 KiB 粒度的 L0 无法表达"这一整段 VA 都映射到同一 pfn"，除非把 L0 扩成多粒度（那要动 per-way 掩码比较器，见 D06/D08）⇒ **本期不做**。
3. ⇒ **因此 L1 → L0 的回填在"4 KiB 粒度"上没有对象**（L1 只存大页）⇒ **本决策降级为：回填机制存在但本期无实际回填对象**；真正需要回填的是 **PTW walk 结果 → L0**（ADR-085 D15 的回填路径，4 KiB 粒度）。

> ⚠ **本决策的诚实表述**：用户要求"L1 命中回填 L0"，但在"**L0 = 4 KiB 粒度、L1 = 大页粒度**"的分工下（见 D06），**大页 entry 无法降级写成 L0 的 4 KiB entry** ⇒ 回填无对象。
> ⇒ **可选替代**：把 L0 也扩成多粒度（`page_shift` per-entry + 掩码比较）⇒ L0 的 8 个槽位可放 4KiB 或大页 entry ⇒ 回填成立。代价：L0 比较器从"全 VPN 直接比"变成"per-entry 掩码比"（D08 记录该代价）。
> ⇒ **默认采纳"可选替代"**（L0 支持多粒度），因为：① 回填是 L1 容量收益兑现的前提（D02 的 2 拍代价必须被摊薄）；② 代价仅限 L0 的 8 个槽位（比较器 ×8 掩码，远小于 L1）；③ 与 ADR-085 D14 §D14.1 的 `page_shift` 掩码比较机制同源，不新增概念。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D02](ADR-086-D02-cascaded-lookup-latency-tiers.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D08](ADR-086-D08-l0-organization.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
