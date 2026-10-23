# ADR-086-D05 — L1 的 set index 与 XOR interleave

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D05` |
| 关联验证判据 | U-L1-03、U-L1-12 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

用户给定 L1 的 index 规则为「**用 `2M × cacheline_size / 8B` 做 set interleave**」。拆开后是三个约束：① 每 set 覆盖 `2M × EPL`；② 每 way 覆盖 `WAY_GRANULE`；③ set 之间用 XOR 折叠打散。三者在「1 entry = 1 个 WAY_GRANULE 区域」的前提下**互相矛盾**（见架构文档 §0 Q1）。本决策先定**与粒度无关的骨架**，把粒度相关的常量留给 D06。

## 决策正文

**决策（参数化骨架，与 entry 粒度解耦）**：

```
WAY_GRANULE_SHIFT = G          # 槽位粒度的对数（sv32 4MiB ⇒ 22；sv39 2MiB ⇒ 21）
EPL              = 8           # cacheline_size(64B) / entry_size(8B)
SET_SPAN_SHIFT   = G + 3       # 每 set 覆盖 = WAY_GRANULE × EPL
SETS             = 256         # 8 bit
WAYS             = 8           # 3 bit ⇒ 每 set 内 WAY_GRANULE 槽位数 = 2^3 = 8

每 set 覆盖 = 2^(G+3)          # sv32: 4MiB × 8 = 32MiB；sv39: 2MiB × 8 = 16MiB
256 set 覆盖 = 2^(G+3) × 256
   sv32 (G=22): 32MiB × 256 = 8GiB   ⇒ 2× 过覆盖 4GiB VA ⇒ 组相连 + 替换确有必要 ✓
   sv39 (G=21): 16MiB × 256 = 4GiB   ⇒ 恰好铺满 32-bit VA
```

**index 计算（两级）**：

```
way  = vaddr[G+2 : G]        # 该 set 跨度内的 WAY_GRANULE 槽位号（3 bit）
set  = fold(vaddr[31 : G+3]) # 8 bit，见下
```

**`fold` 的定义与 XOR interleave 的适用条件（规范性）**：

- **主项**：`vaddr[31 : G+3]` 的高 8 位（= set 跨度的基址粒度）。
- **XOR 折叠项**：来自 **WAY_GRANULE 之下**的位（`vaddr[G-1 : 0]` 的高位折叠），使**同一个 WAY_GRANULE 区域内的多个 entry 分散到不同 set**。
- ⚠ **前置条件（硬约束）**：折叠源**必须**落在 entry 粒度之下 ⇒ **仅当 `entry_shift < G`（entry 粒度细于槽位粒度）时可用**。
  - 若 `entry_shift == G`（entry 粒度 = 槽位粒度，1 entry = 1 区域）⇒ 区域内所有 entry 的折叠源位恒同 ⇒ 折叠退化为常量 XOR ⇒ **等价于无折叠**，但**不破坏正确性**（只是 set 分布偏斜）。
  - ⇒ **本决策取"折叠恒等安全"的实现**：折叠项存在但**不参与正确性判定**，只影响分布。**正确性只依赖 `set`/`way` 的一致计算** ⇒ 任何折叠策略都可用 ⇒ 分布调优可后置。

**为什么需要 interleave（理由）**：

1. **不折叠的偏斜**：`set` 只由 VA 高位决定 ⇒ 同一 16MiB/32MiB 跨度内的所有页落**同一个 set** ⇒ 该 set 的 8 个 way 立刻被同一段 VA 填满 ⇒ 冲突集中在 1/256 的 set 上，其余 set 长期空置 ⇒ **有效容量降到 1/256**。
2. **折叠的收益**：把段内页分散到 256 个 set ⇒ 有效容量恢复。
3. **代价**：折叠项引入 1 组额外 XOR（8 bit，1 级逻辑）⇒ 可忽略。

**与 ADR-085 D06 的关系**：D06 的「index = `parent_ppn` 低位」是 PTWCache（**目录项**）的规则，**与本决策无关**（本组 L1 存 leaf 翻译，不存目录项）⇒ 不冲突，但**命名易混**（两者都叫 index/set）⇒ 文档中一律加限定词。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)（§0 Q1 记录本决策的自洽性前提）
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D12](ADR-086-D12-verification-criteria.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
