# ADR-086-D10 — 容量与节点预算（目标 256×8，本期默认小值）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量上限） |
| 架构文档 | [mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D10` |
| 关联验证判据 | U-L1-11 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`、`ip/mmu/configs/params_schema.json` |

> **引用约定**：本文件沿用 ADR-086 组的章节编号。`§0`–`§11` 指 [架构文档 `mmu-l1-tlb-architecture.md`](../mmu-l1-tlb-architecture.md) 的对应章节（其中 `§0` 是待确认项）；`§D0x.y` 指**编号对应子 ADR 内**的子节。其它文档的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

## 上下文

用户给定的 L1 目标组织（256 set × 8 way × EPL 8 = 16384 entry）折算成 CH_MEM 节点是 **~1.6 万个 `ch_reg`**（entry 打包成 1 个 `ch_uint<96>` 时）。ADR-085 D17 自己判过「2048 entry ⇒ 数千寄存器节点 ⇒ 不可接受（CH_MEM 全量已 833s/轮）」⇒ 必须显式分级。

## 决策正文

**决策（两级容量）**：

| 阶段 | `sets` | `ways` | `epl` | `M` | entry 数 | `ch_reg` 估算 | 何时 |
|------|-------:|-------:|------:|----:|---------:|---------------:|------|
| **P0（本期默认）** | 16 | 4 | 8 | 1 | **512** | ~512 | 立即 |
| P1 | 64 | 4 | 8 | 1 | 2 048 | ~2 048 | 抬升后 |
| **P2（架构目标）** | **256** | **8** | 8 | 1 | **16 384** | **~16 384** | 目标（不在本期）|

⇒ **架构上钉死 P2 为目标容量**（用户给定值），**本期实现 P0**（用户在"目标 vs 本期默认"上已确认此取舍）。

**节点预算推算（规范性）**：

```
ch_reg 数 = entry 数 × 每 entry 的 reg 数
  P0: 512 entry  × 1 reg（整 entry 打包成 1 个 ch_uint<96>）≈ 512
  P2: 16384 entry × 1 reg                          ≈ 16 384
参照：ADR-085 D17 的「不可接受」线 = 2048 entry（每 entry 2~3 reg）≈ 6 000
```

⇒ **entry 打包是关键缓解**：若按 3 个 `ch_reg`/entry（valid / tag+pfn / perms 分开），P2 = ~4.9 万 ⇒ 是 D17 那条「不可接受」线的 8 倍；打包成 1 个 `ch_uint<96>` 后降到 1.6 万 ⇒ **落在同一量级**，可接受。

**缓解手段（按收益排序）**：

| # | 手段 | 效果 | 代价 |
|---|------|------|------|
| 1 | **entry 打包成 1 个 `ch_uint<96>`** | 每 entry 从 3 reg → **1 reg** ⇒ 节点数 ÷3 | 写回需整 entry 重算（组合逻辑多几级）|
| 2 | **本期只落地 2 MiB 粒度 way**（`M=1`）| 4 KiB 粒度 way（`M=512`）的节点数是 2 MiB 的 **512 倍/way** ⇒ 本期不落地 | L1 不提供 4 KiB 粒度缓存（4 KiB 归 L0）|
| 3 | **`array_store`**（ADR-040 Tier-2，CH_MEM 双缓冲 commit swap）| 写法简化 + 避免逐 entry 的 `ch_reg` 数组 | **不降节点数** |
| 4 | **分阶段落地**（P0/P1/P2）| 本期可控 | 目标容量推迟 |

**CH_MEM 全量测试时长的风险（必须记录）**：CH_MEM 全量基线 **833 s/轮**（AGENTS.md 2026-10-20 实测）⇒ 节点数放大 ⇒ elaboration 时间线性增长 ⇒ **P0（512 entry）预估 +2~5%**；**P2（16 384 entry）预估 +30~80%** ⇒ P2 落地时需重估 CI 时限。

**配置化**：`sets` / `ways` / `epl` / `cacheline_size` / `entry_size` 五个旋钮进入 `ip/mmu/configs/params_schema.json`（新增 `l1` 段），默认值按本决策的 P0；`lines_per_way`（`M`）是**导出量**，不作为配置项（由粒度与 `epl` 导出）。

## 交叉引用

- 架构文档：[mmu-l1-tlb-architecture.md](../mmu-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D17 容量上限与 per-mode 默认表）
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D07](ADR-086-D07-leaf-entry-fields-and-width.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
