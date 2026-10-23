# ADR-086-D06 — L1 的 entry 粒度（Sv39：4 KiB / 2 MiB）+ per-way `page_shift` 掩码比较

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D14 `page_shift` 掩码比较） |
| 架构文档 | [mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D06` |
| 关联验证判据 | U-L1-05、U-L1-06、U-L1-11 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/lib/tlb_entry.h` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l1-tlb-architecture.md`](../mmu-l1-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

用户要求 L1 存「**不同粒度页的 leaf 项**」，并选定 **per-way `page_shift` 掩码比较（单 bank）**。用户表述 "each entry can be 2MiB or 4KiB" 是 per-entry 字面表述，但同一句的 "a line can be 16MiB or 32KiB" 只有在 **per-way**（一条 line 内 8 个 entry 同粒度）时才成立 ⇒ 取 per-way。平台 **Sv39** ⇒ 2 MiB 合法；**1 GiB 本期不考虑**（用户已定）。

## 决策正文

**决策（粒度集合）**：

| 粒度 | 叶 PTE 所在级（Sv39） | `page_shift` | VA 跨度 | 本期 |
|------|----------------------|-------------:|--------|:----:|
| **4 KiB** | `VPN[0]`（叶表） | 12 | `vaddr[11:0]` | ✅ |
| **2 MiB** | `VPN[1]`（中间表） | 21 | `vaddr[20:0]` | ✅ |
| 1 GiB | `VPN[2]`（根表） | 30 | `vaddr[29:0]` | ❌ 本期不考虑 |

**决策（比较机制）：单 bank + per-way `page_shift` 掩码比较**。

```
每个 way 带一个 page_shift（3 bit）
命中判定（per way i）:
    mask_i  = ~0 << page_shift_i              # 该 way 的粒度掩码
    tag_hit = (entry_tag_i == (vaddr & mask_i))
    命中条件 = entry_valid_i && tag_hit && (asid/global 匹配)
```

**代价（必须记录）**：8 个 way ⇒ **8 份不同掩码 + 8 份不同 tag 比较** ⇒ 比较器面积 ×8（ADR-085 D14 §D14.1 已指出"比较器不能再直接比全 VPN"）。这是用户选定方案 B 的已知代价。

**D06.1 粒度归属（规范性）**：

| 粒度 | 归属 | 理由 |
|------|------|------|
| **4 KiB** | **L0 TLB 为主**，L1 可选（`M=512`，本期不落地） | L0 全相联小而快，4 KiB 粒度 tag 直接比；L1 存 4 KiB 的节点代价是 2 MiB 的 512 倍（每 way） |
| **2 MiB** | **L1 TLB 为主** | 2 MiB 页天然对齐 16MiB 块（D04）⇒ `M=1`，无浪费 |
| 1 GiB | 本期不考虑 | 用户已定 |

⇒ **L1 的 8 个 way 可混合粒度**（部分 way 2 MiB、部分 4 KiB）—— 但本期只落地 2 MiB 粒度 way。

**D06.2 假命中优先级（规范性）**：若同一 VA 同时被 4 KiB entry 与 2 MiB entry 覆盖 ⇒ 在**同一 bank 内**不会发生（一个 way 只有一种粒度，但**不同 way 可能有不同粒度**）⇒ **跨 way 仍可能假命中** ⇒ **规定：命中时按 way 序号取第一个命中**，且**配置上把 4 KiB 粒度 way 排在 2 MiB 粒度 way 之后**（4 KiB 更"具体"，应优先）⇒ 判据 `U-L1-06` 锚定"恰好 1 个 way 命中"。

**D06.3 与 ADR-085 D14 的关系**：D14 已引入 `page_shift` 掩码比较机制（4 KiB vs 4 MiB 两种粒度）。本决策扩展为 **per-way** 粒度（原来 per-entry），并新增粒度归属规则。⇒ ADR-085 D14 §D14.1 的"TLB entry 增加 `page_shift` 字段"从 per-entry 变为 **per-way**（8 个 way ⇒ 8 个 page_shift，不是每 entry 一个）⇒ **entry 宽度因此可省 3 bit**（见 D07）。

## 交叉引用

- 架构文档：[mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D14 §D14.1 `page_shift` 掩码比较）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D04](ADR-086-D04-l1-organization-params.md)、[D05](ADR-086-D05-set-index-and-xor-interleave.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D08](ADR-086-D08-l0-organization.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
