# ADR-086-D06 — L1 的 entry 粒度选择 + per-way `page_shift` 掩码比较

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D14 `page_shift` 掩码比较） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D06` |
| 关联验证判据 | U-L1-06、U-L1-11、U-L1-12 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/lib/tlb_entry.h` |

## 上下文

用户要求 L1 存「**不同粒度页的 leaf 项**」，并选定 **per-way `page_shift` 掩码比较（单 bank）**。但「4KiB / 2MiB / 1GiB 三种粒度共存」与「per-way 单一掩码」只有一种兼容方式：**同一 way 内的 entry 必须同粒度**。

## 决策正文

**决策（比较机制）**：**单 bank + per-way `page_shift` 掩码比较**。

```
每个 way 带一个 page_shift（3 bit）
命中判定（per way i）:
    mask_i   = ~0 << page_shift_i          # 该 way 的粒度掩码
    tag_hit  = (entry_tag_i == (vaddr & mask_i))
    额外条件: entry_valid_i && (asid/global 匹配)
命中项 = tag 命中的 way（若多 way 同时命中 ⇒ 按 D06.2 的优先级取一个）
```

**代价（必须记录）**：8 个 way ⇒ **8 份不同掩码 + 8 份不同 tag 比较** ⇒ 比较器面积 ×8（ADR-085 D14 §D14.1 已指出"比较器不能再直接比全 VPN"）。**这是用户选定方案 B 的已知代价。**

**D06.1 粒度归属（规范性）**：

| 粒度 | 归属 | 理由 |
|------|------|------|
| **4 KiB** | **L0 TLB** | L0 全相联小而快，4KiB 粒度 tag（`vaddr[31:12]`，20 bit）可直接全 VPN 比较，**无掩码**；且 4KiB 是最热的粒度 |
| **大页**（sv32 4MiB / sv39 2MiB） | **L1 TLB** | 大页翻译条目少、复用度高；tag 短（10~11 bit）⇒ 掩码比较代价低；且"每 way 一个 WAY_GRANULE 槽位"的组织天然匹配大页粒度 |
| **1 GiB** | **L1 TLB**（与 2MiB/4MiB 同 bank） | 32-bit VA 下仅 4 个 1GiB 区域 ⇒ 稀疏，收益低；先不入本期 |

⇒ **L0 与 L1 不同粒度** ⇒ 二者的 tag 宽度与比较方式**不同**（L0 全 VPN 直比 vs L1 掩码比）⇒ **这是本设计的一个实质简化**。

**D06.2 假命中优先级（规范性）**：若同一 VA 同时被 4KiB entry 与大页 entry 覆盖 ⇒ 在**同一 bank 内**不会发生（粒度按 way 归属固定）⇒ 跨 bank 的假命中（4KiB 在 L0、大页在 L1）由 **D02 的级联顺序**天然裁决：**先查 L0（4KiB），命中即用**；L0 miss 才查 L1（大页）⇒ 不会出现二义。

**D06.3 与 ADR-085 D14 的关系**：D14 已引入 `page_shift` 掩码比较机制（4KiB vs 4MiB 两种粒度）。本决策把它扩展为 **per-way** 粒度（原来 per-entry），并**新增粒度归属规则**（4KiB 归 L0、大页归 L1）⇒ D14 §D14.1 的"TLB entry 增加 `page_shift` 字段"从 per-entry 变为 **per-way**（8 个 way ⇒ 8 个 page_shift，不是每 entry 一个）⇒ **entry 宽度因此可省 3 bit**（见 D07）。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D14 §D14.1 `page_shift` 掩码比较）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D03](ADR-086-D03-l1-hit-backfill-l0.md)、[D05](ADR-086-D05-set-index-and-xor-interleave.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D08](ADR-086-D08-l0-organization.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
