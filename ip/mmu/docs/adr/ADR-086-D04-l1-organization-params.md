# ADR-086-D04 — L1 的组织参数：256 set × 8 way × EPL 8（基准 16MiB）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量表） |
| 架构文档 | [mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D04` |
| 关联验证判据 | U-L1-03、U-L1-11 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/configs/params_schema.json` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l1-tlb-architecture.md`](../mmu-l1-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

用户给定 L1 的目标组织：**256 set × 8 ways**，每条 line 8 个 entry，**set interleave 的基准是 16MiB**。平台已确定为 **Sv39** ⇒ 2 MiB 页合法 ⇒ 用户的"line 覆盖 16MiB"成立。

## 决策正文

**决策（目标容量与参数分离）**：

```
L1 = 256 sets × 8 ways
每 way:
  · 固定粒度 page_shift（2MiB=21 或 4KiB=12）        ← per-way，见 D06
  · M 条 line，M = 16MiB / (EPL × 2^page_shift)
        2MiB 粒度 ⇒ M = 16MiB / (8 × 2MiB) = 1
        4KiB 粒度 ⇒ M = 16MiB / (8 × 4KiB) = 512
  · 每 line = EPL(8) × entry_size(12B) = 96B
每 way 覆盖 VA 跨度 = 16MiB          ← set interleave 的基准
每 set 覆盖 = 8 × 16MiB = 128MiB
```

**参数化（四个独立旋钮 + 一个导出量）**：

| 参数 | 目标值 | 说明 |
|------|-------:|------|
| `sets` | **256** | set 数量（`2^8`）⇒ 决定 VA 折叠的桶数 |
| `ways` | **8** | 组相连度 ⇒ 同一 set 内候选 line 数 ⇒ 决定冲突率与替换逻辑成本 |
| `epl` | **8** | 每 line 的 entry 数 ⇒ 决定 line 宽度 |
| `cacheline_size` | **96 B** | = `epl × entry_size` = 8 × 12B |
| `entry_size` | **12 B** | 见 D07（Sv39 leaf entry）|
| `lines_per_way` (`M`) | 1（2MiB）/ 512（4KiB）| **导出量** = `16MiB / (epl × 2^page_shift)` |

⚠ **`ways` 与 `epl` 是正交的两条轴**：`ways` = 同一 set 内有几条候选 line；`epl` = 一条 line 内有几个 entry。ADR-085 D05/D06 分别定过这两个（但对象是 PTWCache），本决策把它们**显式分离**并入配置。

**容量核算**：

```
entry 总数（2MiB 粒度）= 256 × 8 × 8 × 1 = 16 384
每槽位争用的 16MiB 块数 = (2^39 / 2^24) / (256 × 8) = 32768 / 2048 = 16
```

⇒ **16 块争 1 槽位** ⇒ 组相连 + 替换策略确有必要 ✓（这正是 `ways = 8` 的价值，而不是"多存几份"）。

**4KiB 粒度 way 的容量代价**：若某 way 用 4 KiB 粒度，`M = 512` ⇒ 该 way 有 512 条 line ⇒ 单 way 节点数是 2 MiB 粒度的 512 倍 ⇒ **本期只落地 2 MiB 粒度 way**（D10）。

**⚠ 节点预算冲突**：见 D10。

## 交叉引用

- 架构文档：[mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md)
- 相关决策：[D05](ADR-086-D05-set-index-and-xor-interleave.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D10](ADR-086-D10-capacity-and-node-budget.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
