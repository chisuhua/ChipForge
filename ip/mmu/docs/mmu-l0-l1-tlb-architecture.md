# MMU L0 / L1 TLB 架构设计 — 两级 leaf 翻译缓存

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始 —— 归属未立项 change `chmem-ptw-walk` 的后续增量） |
| 决策组 | [ADR-086](adr/ADR-086-l0-l1-tlb-architecture.md) —— 伞索引，登记 **D01–D12** 共 12 条子 ADR（一决策一文件） |
| 增量对象 | [ADR-085](adr/ADR-085-mmu-ptw-architecture.md)（MMU_PTW 架构）—— **不回改**，delta 见 [D11](adr/ADR-086-D11-relation-to-adr085-and-spec.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 适用范围 | **仅 CH_MEM 侧**（用户已定：不做 TLM 侧） |

> **本文档 = 设计正文**：现状事实 / 职责边界 / 决策索引 / 数据流与时序 / 与现有架构的关系 / 替代方案 / 后果 / 实施清单 / 验证命令。
> **决策本身 = 12 个子 ADR**：每个决策一个文件，含上下文 / 决策正文 / 交叉引用；§3 是它们的索引。
>
> **引用约定**：`§0`–`§11` 指**本文档**的章节（`§3` 为决策索引）；`§D0x.y` 指**对应子 ADR 内**的子节；**其它文档**的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

---

## 0. 待确认项（默认决策已给出，用户可否决）

### Q1（首要）—— L1 的 **entry 粒度 / way 槽位粒度**：`2M` 与当前 32-bit 平台不兼容

**用户给定**：L1 = 256 set × 8 way，用 `2M × cacheline_size / 8B` 做 set interleave（"2M 是大页的 size"）。

**拆解为三个约束后互相矛盾**：

| # | 约束 | 推出 |
|---|------|------|
| ① | `256 × 8 × EPL 8` | entry 数 = **16384** |
| ② | 每 set 覆盖 = `2M × EPL` = 16MiB，256 set = 4GiB | **铺满 32-bit VA** |
| ③ | 每 way 覆盖 = 16MiB / 8 = **2MiB** | way 槽位 = **2 MiB** |

**矛盾点**：

1. **2 MiB 在当前平台不是合法页大小** —— 全仓 XLEN = 32 bit（`MMUPlugin_chmem.h:81`）⇒ sv32；**sv32 的合法巨页是 4 MiB**（ADR-085 D14 已钉 `pteshift=22`）；**2 MiB 是 sv39 的粒度**（需 64-bit XLEN）。
2. **way 槽位粒度必须 ≥ 最大支持页大小** —— 否则一个 4 MiB 巨页**跨 2 个 2MiB 槽位** ⇒ 1 个 entry 无法表达 ⇒ 架构不成立。
3. **XOR 折叠要求 entry 粒度 < 槽位粒度** —— 否则同一区域内的所有 entry 折叠源位恒同 ⇒ 折叠退化为常量 ⇒ 分布偏斜（正确性不受影响，见 D05）。

**三个自洽候选**：

| 候选 | entry 粒度 | way 槽位 | 256×8 槽位 vs 区域数 | 成立性 |
|------|-----------|----------|------------------------|--------|
| **(a) 2 MiB 粒度** | 2 MiB | 2 MiB | 2048 区域 ↔ 2048 槽位 = **1:1 完美铺满** | ⚠ 需 2 MiB 页支持 ⇒ **sv39 / 64-bit 平台**；且 **EPL 无意义**（每 way 1 entry）⇒ "多页表项 per cacheline" 不成立 |
| **(b) 4 MiB 粒度**（sv32 合法巨页） | 4 MiB | 4 MiB | 1024 区域 ↔ 2048 槽位 = **2× 过覆盖** | ✅ 当前平台成立；每区域 2 个候选槽位 ⇒ **组相连确有必要** ✓；⚠ ways 若取 4 才是 1:1（256×4）⇒ 与用户的 8 ways 冲突，但 2× 过覆盖正是组相连的意义所在 |
| **(c) 4 KiB 粒度** | 4 KiB | 2 MiB | 1M 区域 ↔ 2048 槽位 | ❌ 每 set 需 64 line ⇒ 总 **1M entry** ⇒ 节点绝对不可接受 |

⇒ **默认取 (b)**：entry 粒度 = 4 MiB、way 槽位 = 4 MiB、每 set 覆盖 32 MiB、256 set 覆盖 8 GiB（4 GiB VA 的 2× 过覆盖）、**保持 ways = 8**（过覆盖正是组相连的用武之地）。
⇒ **代价**：用户的 `2M` 改为 `4M`；且 `EPL 8` 在 (b) 下也需要重新解释（见 §0 Q1 补充）—— 每 way 存 1 个 4 MiB entry 时 EPL 无意义，EPL 只有在"每 way 存多条 line、每 line 多个 entry"时才成立 ⇒ **P0 默认按"每 way 1 条 line"实现，EPL 参数保留但当前恒为 1**。
⇒ **若将来平台升级到 64-bit sv39** ⇒ 切回 (a)（2 MiB 粒度），`WAY_GRANULE` 已是参数（D05）。

### Q2 —— L0 是否支持多粒度（决定 D03 的回填能否成立）

- **支持**（L0 的 8 个槽位可放 4KiB 或大页 entry，比较器改 per-entry 掩码比）⇒ L1→L0 回填成立 ⇒ L1 容量收益可摊薄（D02 的 2 拍代价）。
- **不支持**（L0 恒 4KiB，比较器全 VPN 直比，面积最简）⇒ **大页命中不回填 L0** ⇒ 大页访问永远 2 拍。
⇒ **默认"支持"**（D08.1 已按"支持"写），代价 = L0 比较器 ×8 掩码（仅 8 个槽位，远小于 L1）。

### Q3 —— 目标容量的落地节奏

目标 256 set × 8 way × EPL 8 = 16384 entry ⇒ ~1.6~4.9 万 `ch_reg`（D10）。**默认本期实现 P0 = 16 set × 4 way × EPL 8 = 512 entry**，P2（256×8）作为架构目标登记、不在本期。

---

## 1. 背景与动机

### 1.1 现状事实（实测代码）

| # | 事实 | 位置 |
|---|------|------|
| **F1** | CH_MEM TLB = **8 项全相联**；entry = `{valid, vaddr, paddr}` ⇒ **无 `perms`、无 `page_shift`、无显式 tag**（tag = `vaddr[31:12]` 直接比） | `ip/mmu/plugins/MMUPlugin_chmem.h:83,191-283` |
| **F2** | **2 个查表端口**（ifetch / loadstore）**共享同一存储体** ⇒ 2 份比较逻辑 | 同上 `emit_ifetch` / `emit_loadstore` 均调 `emit_lookup` |
| **F3** | `PTW_ACTIVE` / `EXCEPTION_CODE` **恒写常量 0** | `MMUPlugin_chmem.h:281-282` |
| **F4** | CH_MEM 侧**没有** PTWCache 实体，**也没有**多级 TLB 概念（无 `levels`） | `MMUPlugin_chmem.h` 全文 |
| **F5** | TLM 侧**有** `MultiLevelTLB`：**并行**查所有层、取**最深** hit、shadow fill 浅层、deep evict 反向失效 | `ip/mmu/lib/multi_level_tlb.cpp:20-56` |
| **F6** | TLM 侧 TLB 模板 `TLB<ENTRIES, WAYS, …>` 是**组相连**（`set_idx = vpn % kSets`，WAYS 路比较），支持 `PORTS` 1..8 | `ip/mmu/lib/tlb.h:37-95` |
| **F7** | **live 配置的两级都是全相联**：`L0 = 8 entries / assoc 8`、`L1 = 8 entries / assoc 8` ⇒ **L1 不是组相连** | `soc/mmu_minimal.json:21-23` |
| **F8** | 配置 schema 已建模 `levels[]`：`entries` / `associativity` / `num_lookup_ports` / `lookup_latency_cycles` / `replacement_policy`；`TLBFactory` 11 组特化 + 4 种替换策略 | `ip/mmu/configs/params_schema.json`、`ip/mmu/lib/tlb_factory.h` |
| **F9** | **全仓 XLEN = 32 bit** ⇒ sv32；**sv32 合法巨页 = 4 MiB**；2 MiB 是 sv39 粒度 | `MMUPlugin_chmem.h:81`、`mmu_ptw_chmem.h:151`、ADR-085 D14 |
| **F10** | `openspec/specs/mmu-multi-level-coherence/spec.md` 有 **MUST 级**条款：「并行查所有层 + 取最深 hit」「shadow fill」「deep evict 反向失效」 | 该 spec 全文 |
| **F11** | CH_MEM 全量测试基线 **833 s/轮** | `AGENTS.md`（2026-10-20 实测） |
| **F12** | ADR-085 已定 `TLBCache`（**leaf**）+ `PTWCache`（**目录项**）两单元 ⇒ **本组与它不同源** | ADR-085 D01/D04 |

### 1.2 问题的本质

CH_MEM 侧的 TLB 是一个 **8 项、单粒度、无 perms、无层级**的扁平结构 ⇒ 三个后果：

1. **容量太小** ⇒ 任何稍微大的工作集都穿透到 PTW（而 PTW 尚未实装，F3）；
2. **不支持大页** ⇒ ADR-085 D14 要求的 `page_shift` 掩码比较没有落点；
3. **无 perms** ⇒ 权限检查无处落（ADR-085 D13 要求 `perms` 进 TLB entry）。

用户提出的两级结构（L0 小而快全相联 + L1 大而慢组相连 + 级联）正是针对这三点。

### 1.3 目标架构

```
                    ┌───────────────────────────────────────────┐
   vaddr ─────────► │ L0 TLB：8 项全相联 · 1 拍 · 4KiB 粒度      │──hit──► paddr
                    └───────────────┬───────────────────────────┘
                                    │ miss（同拍 PTW_ACTIVE=1）
                                    ▼
                    ┌───────────────────────────────────────────┐
                    │ L1 TLB：256 set × 8 way · 2 拍 · 大页粒度  │──hit──► paddr
                    │  · 组相连 + XOR interleave                │   （并回填 L0）
                    │  · per-way page_shift 掩码比较            │
                    └───────────────┬───────────────────────────┘
                                    │ miss
                                    ▼
                    ┌───────────────────────────────────────────┐
                    │ PTW walk（ADR-085 定义，本组不改）        │
                    └───────────────────────────────────────────┘
```

**关键点**：两级**都只缓存 leaf 翻译**；PTWCache（目录项缓存）由 ADR-085 独立定义，与本组**共存但职责不同**。

---

## 2. 术语与职责边界

| 术语 | 本组含义 | 冲突警示 |
|------|---------|---------|
| `L0 TLB` | 本组定义的 **leaf 翻译缓存第 0 级**（8 项全相联，4KiB） | ⚠ `ip/mmu/lib/multi_level_tlb.h:25` 与 `soc/mmu_minimal.json` 的 `L0` 指 **TLM 侧 TLB 深度层**（另一套语义） |
| `L1 TLB` | 本组定义的 **leaf 翻译缓存第 1 级**（256×8 组相连，大页） | ⚠ 同上 |
| `leaf 翻译` | 叶 PTE 派生的 `{paddr, perms, page_shift}` | 与 ADR-085 的 `PTWCache`（页**目录项**）严格区分 |
| `WAY_GRANULE` | way 槽位的 VA 粒度（sv32 = 4 MiB） | 参数化，见 D05 |
| `EPL` | 每 cacheline 的 entry 数 = `cacheline_size / entry_size` | 与 `ways` **正交**（D04） |

---

## 3. 决策索引（D01–D12）

**12 条决策各自独立成文件**（一个决策一个文件，位于 [`adr/`](adr/) 目录，编号 `ADR-086-D01` … `ADR-086-D12`）。下表是索引；每条决策的**上下文 / 决策正文 / 交叉引用**见对应文件。

| 编号 | 决策标题 | 一句话要点 | 关联判据 |
|------|---------|-----------|-----------|
| [D01](adr/ADR-086-D01-l0-l1-leaf-cache-split.md) | L0 / L1 两级 **leaf 翻译缓存**的职责划分 | 两级都只存 leaf 翻译；L0 = 8 项全相联 / 4KiB；L1 = 256 set × 8 way / 大页；级联 L0→L1→PTW | U-L1-01/03 |
| [D02](adr/ADR-086-D02-cascaded-lookup-latency-tiers.md) | 级联 lookup 与 hit latency 分档 | 串行级联；L0 命中 1 拍 / L1 命中 2 拍 / PTW 4~6 拍；**与 openspec spec 的并行查分叉** | U-L1-02/10 |
| [D03](adr/ADR-086-D03-l1-hit-backfill-l0.md) | L1 命中回填 L0 | 回填是 L1 容量收益兑现的前提；**大页 entry 无法降级写成 L0 的 4KiB entry** ⇒ 需 L0 支持多粒度（§0 Q2） | U-L1-04 |
| [D04](adr/ADR-086-D04-l1-organization-params.md) | L1 的组织参数 | `256 sets × 8 ways × EPL 8` = 16384 entry；`sets`/`ways`/`epl` 三旋钮独立；**ways 与 epl 正交** | U-L1-11 |
| [D05](adr/ADR-086-D05-set-index-and-xor-interleave.md) | set index 与 XOR interleave | `way = vaddr[G+2:G]`；`set = fold(vaddr[31:G+3])`；XOR 折叠仅在 entry 粒度 < 槽位粒度时才有意义 | U-L1-12 |
| [D06](adr/ADR-086-D06-multi-granularity-page-shift-mask.md) | entry 粒度选择 + per-way `page_shift` 掩码比较 | **单 bank + per-way 掩码**（比较器面积 ×8）；**4KiB 归 L0 / 大页归 L1**；跨 bank 假命中由级联顺序裁决 | U-L1-06 |
| [D07](adr/ADR-086-D07-leaf-entry-fields-and-width.md) | leaf entry 字段与 8B 宽度 | `{valid, tag(20b), pfn(22b), perms(4b), asid, global, reserved}` = 60 bit ⇒ 8B；**与 ADR-085 D05 的 8B 目录项 entry 是不同实体** | U-L1-05 |
| [D08](adr/ADR-086-D08-l0-organization.md) | L0 的组织 | 8 项全相联；`paddr` 按 `page_shift` 参数化合成；支持多粒度（§0 Q2） | U-L1-01 |
| [D09](adr/ADR-086-D09-coherence-and-invalidation.md) | 一致性与失效 | 4 条路径：L1 evict ⇒ 反向失效 L0 / `satp` 变更 ⇒ 两级 flush / `sfence.vma` 推迟 / walk 结果按粒度落 L0 或 L1 | U-L1-07/08/09 |
| [D10](adr/ADR-086-D10-capacity-and-node-budget.md) | 容量与节点预算 | 目标 256×8 不在本期；本期 P0 = 16×4×8 = 512 entry；缓解：entry 打包成 1 个 `ch_uint<64>`（节点 ÷3） | U-L1-11 |
| [D11](adr/ADR-086-D11-relation-to-adr085-and-spec.md) | 与 ADR-085 / openspec spec 的关系 | **13 条 delta 表**；spec 分叉登记；命名冲突处置 | — |
| [D12](adr/ADR-086-D12-verification-criteria.md) | 验证判据 | `U-L1-01` … `U-L1-12` + **7 项变异对照** + e2e + 5 门禁 | 全部 |

---

## 4. 数据流与时序

### 4.1 L0 命中（1 拍）

| 拍 | 动作 | `PTW_ACTIVE` | `hold` |
|----|------|:-------------:|:-----:|
| T | L0 8 路全 VPN 比较 ⇒ 命中 ⇒ `paddr = (pfn << shift) \| (vaddr & mask)` ⇒ 写 `PADDR` / `PADDR_VALID=1` | 0 | 0 |

### 4.2 L0 miss → L1 命中（2 拍）

| 拍 | 动作 | `PTW_ACTIVE` | `hold` |
|----|------|:-------------:|:-----:|
| T | L0 miss ⇒ **`PTW_ACTIVE=1`（同拍）** ⇒ 向 L1 发请求 | **1** | 1 |
| T+1 | L1 8-way 掩码比较 ⇒ 命中 ⇒ 产出 `paddr`（大页拼接）⇒ 回填 L0（4KiB 场景） | 1 | 1 |
| T+2 | `PADDR_VALID=1`，`PTW_ACTIVE=0` ⇒ 释放 | 0 | 0 |

### 4.3 双 miss → PTW（4~6 拍）

| 拍 | 动作 |
|----|------|
| T | L0 miss ⇒ `PTW_ACTIVE=1` ⇒ L1 查询 |
| T+1 | L1 miss ⇒ 交 PTW walk（ADR-085 §4.2 的 `REQ`/`WAIT`/`DONE` 序列） |
| T+2~T+5 | walk 进行中（`PTW_ACTIVE` 全程 1） |
| T+6 | `DONE` ⇒ 结果按粒度回填 L0（4KiB）或 L1（大页）⇒ `PADDR_VALID=1` |

### 4.4 回填与失效

| 事件 | 动作 | 决策 |
|------|------|------|
| L1 命中 | 回填 L0（4KiB 粒度场景） | D03 |
| PTW 成功 | 按粒度回填 L0 / L1 | D09 |
| L1 evict | 反向失效 L0 同 VA 副本 | D09 |
| `satp` 变更 | 两级 flush-all | D09 |
| `sfence.vma` | 精确失效（推迟到 CSR 批次） | D09 |

---

## 5. 与现有架构的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-085**（MMU_PTW 架构） | **增量对象，不回改**。13 条 delta（D02 entry 组织 / D12 `PTW_ACTIVE` 窗口 / D14 `page_shift` per-way / D15 回填落点 / D17 容量表 / D19 判据编号 / D20 flush 范围 受影响）见 [D11](adr/ADR-086-D11-relation-to-adr085-and-spec.md)。⚠ **两个 leaf 缓存并存**：本组的 L1 存 leaf 翻译，ADR-085 的 PTWCache 存**目录项** |
| **`openspec/specs/mmu-multi-level-coherence/spec.md`** | ⚠ **语义分叉**：spec 要求"**并行**查所有层 + 取**最深** hit"，本组用**串行级联**。该 spec 约束 **TLM 侧** C++ 类（用户已定不做 TLM 侧）⇒ **不是违约，是后端分叉**。shadow fill 方向与 deep-evict 反向失效**语义一致，可直接复用**。CH_MEM 侧实现**不得**复用 `MultiLevelTLB` / `multi_tlb_` 命名 |
| **`ip/mmu/lib/multi_level_tlb.h` / `soc/mmu_minimal.json`** | ⚠ **命名冲突**：这里的 `L0`/`L1` 是 **TLM 侧 TLB 深度层**（`L0 最快最小`），与本组的 `L0 TLB`/`L1 TLB`（leaf 翻译缓存）**同名不同义**。处置见 D11 决策 3：TLM 侧不动，文档内写全「L0 TLB」并注明"leaf 翻译缓存"，`ip/mmu/docs/architecture.md` 的 banner 加命名警示 |
| **ADR-040 v2.0** | §10.5 payload key mode-aware；`array_store` 双缓冲（D10 缓解手段） |
| **ADR-046 / ADR-083** | 多周期 FSM 豁免 / Plugin-style SSOT（CH_MEM 新文件落 `ip/mmu/plugins/`） |
| **ADR-076**（规划中） | ASID vs 全局 TLB 冲刷 —— 与 D09 的 `satp` flush-all 粗粒度限制主题相邻 |

---

## 6. 替代方案评估

### 6.1 lookup：并行查所有层 vs 串行级联（**采纳级联**）

| | 级联（采纳） | 并行（spec 现行） |
|---|---|---|
| 比较器面积 | L0/L1 各一套但**不同时**工作 ⇒ 可共用多路选择器 | 两套**同拍**工作 ⇒ 面积 ×2 |
| L0 命中 | 1 拍 | 1 拍 |
| L1 命中 | 2 拍 | 1 拍 |
| 时序简单性 | 单一 FSM | 需"取最深"仲裁 + 一致性判定 |
| `PTW_ACTIVE` 口径 | L0 miss 同拍为 1 ⇒ 天然满足 AC-FS-9 | 需额外保证"最浅 miss 拍即置位" |

⇒ 代价集中在 L1 命中路径（2 拍 vs 1 拍）⇒ **L0 够大**（8 项全相联）才能让多数访问停在 1 拍。

### 6.2 PTWCache 存叶 vs L1 存叶（**采纳 L1 存叶**）

ADR-085 D04 主张 PTWCache 存**目录项**、TLB 存 leaf。本组另设 L1 存 leaf。
⇒ 二者**不冲突**：PTWCache 省的是"**访存次数**"（目录项命中 ⇒ 少一级 walk），L1 省的是"**查表延迟**"（大页翻译一次命中 512 个 4KiB 页）。
⇒ 代价：两级 leaf 缓存 + 一级目录项缓存 = **3 个缓存** ⇒ 容量预算与一致性面变大 ⇒ 登记为风险 R4。

### 6.3 L0 单粒度 vs 多粒度（**默认多粒度**，§0 Q2）

多粒度 ⇒ L1→L0 回填成立（否则大页访问永远 2 拍）⇒ 代价 = L0 比较器 ×8 掩码（仅 8 槽位）。

### 6.4 L1 直接映射 vs 组相连（**采纳组相连**）

ADR-085 D06 因 CH_MEM 组合替换策略昂贵而选直接映射（对象是 PTWCache）。本组 L1 用**组相连**：
⇒ 8 ways ⇒ 需组合式 victim 选择器（PLRU 每 way 1 bit + 树比较）⇒ 面积 + 逻辑深度。
⇒ 缓解：**本期 P0 只做 16 set × 4 way**（D10）⇒ victim 选择器规模可控；PLRU 延后到 P1/P2。

### 6.5 复活 TLM 侧多级 TLB（**否决**）

用户已定「不做 TLM 侧」⇒ TLM 的 `MultiLevelTLB` 与 `params_schema.json` 的 `levels[]` **保持现状**，CH_MEM 侧独立实现 ⇒ **后端语义分叉登记在案**（§5）。

---

## 7. 后果

### 7.1 正面后果

- ✅ **支持大页**：L1 的 per-way `page_shift` 掩码比较让 4 MiB 页一条 entry 覆盖 1024 个 4 KiB 页 ⇒ 大页工作集的 TLB 压力骤降。
- ✅ **权限检查有落点**：L0/L1 的 entry 都带 `perms`（ADR-085 D13 要求）。
- ✅ **容量可扩展**：`sets`/`ways`/`epl` 三旋钮独立 ⇒ 后续抬升不改结构。
- ✅ **PTW 压力下降**：L1 命中 ⇒ 无需 walk ⇒ walk 频率降为"冷启动级"。
- ✅ **命中延迟可预期**：1 / 2 / 4~6 拍三档（D02）。

### 7.2 负面后果与缓解

| 负面后果 | 缓解 |
|----------|------|
| **L1 命中比 L0 慢 1 拍** | L0 8 项全相联 + L1→L0 回填 ⇒ 热点页回到 1 拍 |
| **比较器面积上升**（L1 8 way 掩码比 + L0 多粒度掩码比） | L0/L1 分摊；L0 仅 8 槽位；P0 阶段 L1 只 4 way |
| **节点数上升**（P2 目标 ~1.6~4.9 万 reg） | D10：entry 打包成 1 个 `ch_uint<64>`（÷3）+ 分阶段落地 |
| **CH_MEM 全量测试时长上升**（基线 833 s/轮） | P0 预估 +5~10%；P2 需重估 CI 时限 |
| **三个缓存的一致性面变大**（L0 / L1 / PTWCache） | D09 定义 4 条失效路径；spec 的 deep-evict 反向失效语义复用 |
| **与 openspec spec 语义分叉** | 登记在 §5 + D11；TLM 侧不动；CH_MEM 侧不用 `MultiLevelTLB` 命名 |

### 7.3 风险登记

| ID | 风险 | 等级 | 缓解 |
|----|------|:----:|------|
| R1 | **§0 Q1 未决**（entry 粒度 / 槽位粒度）⇒ 实现者各选各的 ⇒ 结构分叉 | **高** | 实现前必须裁决 Q1；D05/D06 的常量全部参数化，不预设答案 |
| R2 | **"多页表项 per cacheline"（EPL 8）在 Q1-b 下语义弱化**（每 way 1 entry 时 EPL 恒 1）⇒ 与用户原意的偏差 | 中 | D04 保留 EPL 旋钮；P1 之后按"每 way 多条 line"细化 |
| R3 | **per-way 掩码比较的面积 ×8** 被低估 | 中 | U-L1-06 判据 + 变异对照 ⑦；P0 用 4 way 限幅 |
| R4 | **三个缓存的一致性面** ⇒ 逐出/失效遗漏导致陈旧命中 | 中 | D09 的 4 条路径 + U-L1-07/08 判据 + 变异对照 ⑤/⑥ |
| R5 | **PTW 侧尚未实装**（F3：`PTW_ACTIVE` 恒 0）⇒ 本组实装时 L0/L1 命中与双 miss 路径的对照难以端到端验证 | 中 | 先做 L0/L1 命中路径（可用桩驱动 PTE），PTW 随 ADR-085 的 `chmem-ptw-walk` 落地 |
| R6 | **XOR 折叠退化为常量** ⇒ 分布偏斜（有效容量下降） | 低 | D05 明确"折叠不影响正确性"；分布调优后置 |
| R7 | **命名冲突导致实现者误用 TLM 侧 `L0`/`L1` 语义** | 低 | D11 决策 3 + `ip/mmu/docs/architecture.md` banner 警示 + CH_MEM 侧成员名用 `l0_tlb_`/`l1_tlb_` |

---

## 8. 实施检查清单（分阶段）

| # | 阶段 | 内容 | 判据 |
|---|------|------|------|
| 1 | **P0 规格同步** | 本组 ADR + `ip/mmu/docs/architecture.md` banner 的命名警示；`params_schema.json` 加 `l1` 段（`sets`/`ways`/`epl`/`way_granule`） | D11 |
| 2 | **P1 L0 重构** | L0 entry 加 `perms` / `page_shift` / 显式 tag；`paddr` 按 `page_shift` 参数化合成；支持多粒度（§0 Q2） | U-L1-01/05 |
| 3 | **P2 L1 落地（P0 容量）** | 16 set × 4 way；per-way 掩码比较；set/way index 计算；refill 与 lookup 的 index 一致性 | U-L1-03/06/12 |
| 4 | **P3 级联 + 回填 + 失效** | L0→L1→PTW 级联；`PTW_ACTIVE` 分档；L1→L0 回填；L1 evict 反向失效；`satp` 两级 flush | U-L1-02/04/07/08/09/10 |
| 5 | **P4 验证** | 12 条判据 + 7 项变异对照 + e2e + 5 架构 gate + Verilator 后端行为断言 | D12 |

> **本清单是架构层面的阶段划分，不构成 change 拆分**（change 拆分属 planner 职责）。

---

## 9. 与 change 的关系

**本组不创建 change**。以下是它与既有 `chmem-ptw-walk` 拆解（`openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 的 3a~3g 推迟项 + ADR-085 §9）的关系：

| 既有子项 | 本组的关系 | 偏差 |
|---------|-----------|------|
| 3a PTW FSM 自由函数化 | 无关 | — |
| 3b PTWCache | 无关（本组 L1 存 leaf，不是目录项） | — |
| 3c walk 触发 + refill | **重叠**：本组定义"walk 结果按粒度回填 L0/L1"（D09） | 需与 ADR-085 D15 对齐 |
| 3d `mmu_keys` 剩余 key 模式化 | 正交 | — |
| 3e MEM_ADDR 源接线 | 正交，但**依赖** D09 | — |
| 3f fetch-stall 接线 | **重叠**：`PTW_ACTIVE` 的置位点从"TLB miss"前移到"L0 miss"（D02） | 判据口径不变（AC-FS-9） |
| 3g 协议接线 | 依赖 protocol rev.15（ADR-085 D13） | — |
| — | **新增**：L0 结构重（D08）、L1 全新（D04/D05/D06）、级联与回填（D02/D03）、两级失效（D09） | **≥5 项未列工作** |

⇒ 估时需相应上调（**估时/排期属 planner 职责，本组只负责把范围增量列全**）。

---

## 10. 验证命令

```bash
# 架构门禁（5 个）
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh

# 本组的结构性检查（P0~P4 落地后）
grep -q "page_shift" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: L0/L1 entry 缺 page_shift（D06/D08）"
grep -q "perms"      ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: TLB entry 缺 perms（D07）"
grep -q "l1_tlb"     ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: 缺 L1 TLB 结构（D01/D04）"
grep -qE "sets|ways|epl" ip/mmu/configs/params_schema.json || echo "FAIL: L1 的 sets/ways/epl 未配置化（D04/D10）"
grep -q "satp_changed" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: satp 变更无两级 flush 路径（D09）"

# 回归
./build/bin/chipforge_tests "[mmu]"                            # TLM
./build/bin/chipforge_tests_chmem "[mmu-chmem-basic]"          # CH_MEM
bash tools/run_chipforge_tests.sh --all                        # 全量（CH_MEM ~833s/轮）
```

---

## 11. 参考

- **RISC-V Privileged Spec v1.12**：§5.3.2 虚拟地址翻译过程（step 3 编码/reserved 判据、step 4 叶/指针判据、step 5 叶权限）；§12.3 / §12.3.2（Sv32，VPN 10 bit，**巨页 4 MiB**）
- **上游 CppHDL**：`include/chlib/state_machine.h`（FSM DSL）、`include/core/mem.h:125/153`（aread/sread）
- **ChipForge 内部**：`ip/mmu/plugins/MMUPlugin_chmem.h`（现状 CH_MEM TLB）、`ip/mmu/lib/multi_level_tlb.cpp`（TLM 侧并行查 + 最深 hit + shadow fill）、`ip/mmu/lib/tlb.h`（组相连模板）、`ip/mmu/lib/tlb_factory.h`、`ip/mmu/configs/params_schema.json`、`soc/mmu_minimal.json:21-23`、`openspec/specs/mmu-multi-level-coherence/spec.md`、`.rddf/protocols/fetch_stall_protocol_v0.11.0.md`（AC-FS-9 / AC-FS-15 / AC-FS-16）
- **既有 ADR**：[ADR-085](adr/ADR-085-mmu-ptw-architecture.md)（增量对象）、ADR-040 v2.0、ADR-046、ADR-083、ADR-076（规划中）
- **业界对照**：Rocket Chip（TLB + PTW，串行 walk）、BOOM（TLB + PTW）、CVA6（TLB + PTW + PTW cache）

---

*本决策组由用户设计（2026-10-23 讨论：L0 全相联 / L1 组相连 + XOR interleave / L1→L0 回填 / 目标 256×8 本期小值）驱动。实装开始后，状态从 📝 升到 🚧（设计锁定、实装中）再到 ✅（实装 + 集成测试通过）。同步更新 [`docs/architecture/adr.md`](../../../docs/architecture/adr.md) 主表。*
