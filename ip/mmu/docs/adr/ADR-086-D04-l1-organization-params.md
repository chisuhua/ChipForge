# ADR-086-D04 — L1 的组织参数：256 set × 8 way × EPL 8

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量表） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D04` |
| 关联验证判据 | U-L1-03、U-L1-11 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/configs/params_schema.json` |

## 上下文

用户给定 L1 的目标组织：**256 set × 8 ways**，cacheline 内放多个页表项。这与 ADR-085 D17 给 CH_MEM 定的容量（TLBCache 1 set × 8 way × EPL 1 = 8 entry）**差 3 个数量级**，必须显式处理节点预算。

## 决策正文

**决策（目标容量）**：

```
L1 TLB = 256 sets × 8 ways × EPL 8
        = 2048 条 cacheline
        = 16384 个 leaf entry        （EPL = cacheline_size / 8B = 64B / 8B = 8）
```

**参数化（三个独立旋钮，配置项分离）**：

| 参数 | 目标值 | 说明 |
|------|-------:|------|
| `sets` | 256 | set 数量（`2^8`） |
| `ways` | 8 | 组相连度 ⇒ 决定冲突率与替换逻辑成本 |
| `epl` | 8 | **每 cacheline 的 entry 数** = `cacheline_size / entry_size`（8B） |
| `cacheline_size` | 64 B | 固定 ⇒ `epl = 8` |
| `entry_size` | 8 B | 见 D07 |

⚠ **`ways` 与 `epl` 是正交的两条轴**：`ways` = 同一 set 内有几条候选线；`epl` = 一条线内有几个 entry。ADR-085 D06/D05 分别定过这两个（但对象是 PTWCache），本决策把它们**显式分离**并入配置。

**容量核算（VA 覆盖度）**：

```
每 set 覆盖的 VA 跨度 = WAY_GRANULE × epl
每 way 覆盖的 VA 跨度 = WAY_GRANULE
```

| 平台 | `WAY_GRANULE` | 每 set 跨度 | 256 set 覆盖 | vs 4 GiB VA |
|------|---------------:|------------:|-------------:|------------:|
| **sv32**（当前，32-bit） | **4 MiB** | 32 MiB | **8 GiB** | **2× 过覆盖** |
| sv39（未来，64-bit） | 2 MiB | 16 MiB | 4 GiB | 远小于 2^39 VA |

⇒ sv32 下 **2× 过覆盖** ⇒ 同一 VA 区间平均有 2 个候选槽位 ⇒ **组相连 + 替换策略确有必要**（这正是 `ways=8` 的意义）；sv39 下过覆盖极大 ⇒ 需要**哈希式 index**（见 D05）。

**⚠ 节点预算冲突（必须记录，见 D10）**：16384 entry × 每 entry 2~3 个 `ch_reg` ⇒ **~4 万~5 万寄存器节点**。ADR-085 D17 自己判过"64 set × 4 way × 8 = 2048 entry ⇒ 数千节点 ⇒ 不可接受（CH_MEM 全量已 833s/轮）" ⇒ **本目标是那个"已判定不可接受"例子的 8 倍**。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D05 entry 宽度、D06 替换策略、D17 容量表）
- 相关决策：[D05](ADR-086-D05-set-index-and-xor-interleave.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D10](ADR-086-D10-capacity-and-node-budget.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
