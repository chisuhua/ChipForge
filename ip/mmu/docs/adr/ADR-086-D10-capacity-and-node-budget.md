# ADR-086-D10 — 容量与节点预算（目标 256×8，本期默认小值）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量上限） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D10` |
| 关联验证判据 | U-L1-11 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/configs/params_schema.json` |

## 上下文

用户给定的 L1 目标组织（256 set × 8 way × EPL 8 = 16384 entry）折算成 CH_MEM 节点是 **~4~5 万个 `ch_reg`**。ADR-085 D17 自己判过「2048 entry ⇒ 数千寄存器节点 ⇒ 不可接受（CH_MEM 全量已 833s/轮）」⇒ 目标值是那个"已判定不可接受"例子的 **8 倍**。必须显式分级。

## 决策正文

**决策（两级容量）**：

| 阶段 | `sets` | `ways` | `epl` | entry 数 | `ch_reg` 估算 | 何时 |
|------|-------:|-------:|------:|---------:|---------------:|------|
| **P0（本期默认）** | 16 | 4 | 8 | **512** | ~1.5 千 | 立即 |
| P1 | 64 | 4 | 8 | 2 048 | ~6 千 | 抬升后 |
| **P2（架构目标）** | **256** | **8** | 8 | **16 384** | **~4~5 万** | 目标（不在本期） |

⇒ **架构上钉死 P2 为目标容量**（用户给定值），**本期实现 P0**（用户在"目标 vs 本期默认"上已确认此取舍）。

**节点预算推算（规范性）**：

```
ch_reg 数 = entry 数 × 每 entry 的 reg 数
  P0: 512 entry  × 3 reg（valid / tag+pfn / perms）≈ 1 536
  P2: 16384 entry × 3 reg                     ≈ 49 152
参照：ADR-085 D17 的「不可接受」线 = 2048 entry ≈ 6 000 reg
```

**缓解手段（按收益排序）**：

| # | 手段 | 效果 | 代价 |
|---|------|------|------|
| 1 | **entry 打包成 1 个 `ch_uint<64>`**（D07 已算出 8B 够） | 每 entry 从 3 reg → **1 reg** ⇒ 节点数 ÷3（P2：4.9 万 → **1.6 万**） | 写回需整 entry 重算（组合逻辑多几级） |
| 2 | **line 内共享 tag**（一条 line 的 8 个 entry 覆盖**连续 8 个 4KiB 页** ⇒ 共享一个 32KiB 粒度 tag） | 每 line 的 reg 数 24 → **~10** ⇒ 节点数 ÷2.4 | ⚠ 与 D06 的「大页归 L1」冲突：4KiB 归 L0 ⇒ L1 的 line 内不是 4KiB 页 ⇒ **本手段在当前粒度分工下不适用** ⇒ 记为 P1 之后、粒度分工调整后的可选项 |
| 3 | **`array_store`**（ADR-040 Tier-2，CH_MEM 双缓冲 commit swap） | 写法简化 + 避免逐 entry 的 `ch_reg` 数组 | **不降节点数**（节点数由 entry 数决定） |
| 4 | **分阶段落地**（本决策的 P0/P1/P2） | 本期可控 | 目标容量推迟 |

**CH_MEM 全量测试时长的风险（必须记录）**：CH_MEM 全量基线 **833 s/轮**（AGENTS.md 2026-10-20 实测）⇒ 节点数放大 ⇒ elaboration 时间线性增长 ⇒ **P0（1.5 千 reg）预估 +5~10%**；**P2（1.6 万 reg）预估 +50~100%** ⇒ P2 落地时需要重估 CI 时限。

**配置化**：`sets` / `ways` / `epl` 三个旋钮进入 `ip/mmu/configs/params_schema.json`（新增 `l1` 段），默认值按本决策的 P0。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量上限与 per-mode 默认表）
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)、[D12](ADR-086-D12-verification-criteria.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
