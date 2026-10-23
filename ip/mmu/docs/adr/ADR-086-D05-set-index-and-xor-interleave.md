# ADR-086-D05 — L1 的 set index 与 XOR interleave（基准 16MiB）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D05` |
| 关联验证判据 | U-L1-12 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l1-tlb-architecture.md`](../mmu-l1-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

用户给定：**set interleave 的基准 size = 16MiB**（= 每 way 的 VA 覆盖）。平台 Sv39 ⇒ 有效 VA 39 bit = 512 GiB ⇒ 16MiB 块有 32768 个 ⇒ 必须**折叠**到 256 个 set。

## 决策正文

**决策（参考实现 + 4 条规范性性质）**：

```
16MiB 块索引  blk = vaddr[38:24]          # 15 bit（Sv39 39-bit VA）
way    (3 bit) = blk[2:0]                  # 块在 set 内的 8 个槽位
set    (8 bit) = blk[14:7] XOR {blk[6:3], 4'b0}
```

```c
// 参考实现（可替换，但必须满足下表 4 条性质）
uint8_t l1_set_index(uint64_t vaddr) {
    const uint16_t blk = (uint16_t)((vaddr >> 24) & 0x7FFF);   // vaddr[38:24]
    return (uint8_t)(((blk >> 7) & 0xFF) ^ (((blk >> 3) & 0x0F) << 4));
}
uint8_t l1_way_index(uint64_t vaddr) {
    return (uint8_t)((vaddr >> 24) & 0x07);                    // blk[2:0]
}
```

**4 条规范性性质（实现必须满足）**：

| # | 性质 | 理由 |
|---|------|------|
| 1 | `set` / `way` 必须是 vaddr 的**纯函数** ⇒ **refill 与 lookup 必须同式** | 否则 refill 写错槽位（判据 `U-L1-12`）|
| 2 | 折叠必须**用满 15 bit**（`blk[14:0]` 全部参与）| 否则信息丢失 ⇒ 冲突集中 |
| 3 | 折叠必须**自反**（XOR 天然可逆）⇒ 同一 VA 恒定映射到同一槽位 | 幂等性（重复 lookup 不换槽）|
| 4 | 相邻 16MiB 块（`blk` 差 1）应落到**不同 set 或不同 way** | 避免同一大页被拆散到多个 set |

**为什么需要 interleave（理由）**：

1. **不折叠的偏斜**：`set` 只由 VA 高位决定 ⇒ 同一 16MiB/128MiB 跨度内的所有页落**同一个 set** ⇒ 该 set 的 8 个 way 立刻被同一段 VA 填满 ⇒ 冲突集中在 1/256 的 set 上，其余 set 长期空置 ⇒ **有效容量降到 1/256**。
2. **折叠的收益**：把段内页分散到 256 个 set ⇒ 有效容量恢复。
3. **代价**：1 组额外 XOR（8 bit，1 级逻辑）⇒ 可忽略。

**为什么基准是 16MiB**：16MiB = 每 way 的 VA 覆盖（D04）⇒ 块索引与 way 槽位**同粒度** ⇒ `way = blk[2:0]` 是自然切分，不需要额外的 line 索引（4 KiB 粒度 way 通过 `M` 条 line 覆盖，见 D04）。

**与 ADR-085 D06 的关系**：D06 的「index = `parent_ppn` 低位」是 PTWCache（**目录项**）的规则，**与本决策无关**（本组 L1 存 leaf 翻译）⇒ 不冲突，但**命名易混**（两者都叫 index/set）⇒ 文档中一律加限定词。

## 交叉引用

- 架构文档：[mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md)
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
