# ADR-086: MMU L0 / L1 TLB 架构 — 伞索引（D01–D12）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始 —— 归属未立项 change `chmem-ptw-walk` 的后续增量）<br>注：全局注册表 `docs/architecture/adr.md` 只有 `✅/⚠️/🚧` 三态、无 📝 ⇒ 本 ADR 在那张表里登记为 **🚧 + 备注"设计提案"** |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡 ⇒ 归 `ip/mmu/docs/adr/`） |
| 架构文档 | **[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)** —— 设计正文 |
| 决策文件 | 本目录下 `ADR-086-D01` … `ADR-086-D12`（**12 个，一决策一文件**） |
| 增量对象 | [ADR-085](ADR-085-mmu-ptw-architecture.md)（MMU_PTW 架构）—— 本组**不回改**它，delta 见 D11 |
| 来源 | 用户设计（2026-10-23 讨论）：L0/L1 都是 leaf 翻译缓存，L0 全相联小而快，L1 组相连大容量，级联查询 |

---

## 0. 如何阅读

| 文件 | 内容 | 何时读 |
|------|------|--------|
| **架构设计文档**（`../mmu-l0-l1-tlb-architecture.md`） | 设计正文：现状事实 / 职责边界 / **决策索引（§3）** / 数据流与时序 / 与现有架构的关系（含 spec 分叉）/ 替代方案 / 后果 + 风险 / 实施清单 / 验证命令 | **先读这个** |
| **12 个子 ADR**（`ADR-086-D01..D12`） | 每个决策一个文件：上下文 / 决策正文 / 交叉引用 | 按需精读 |

**全局注册表只登记本条**（ADR-086），子决策用 `ADR-086-Dnn` 子号，不占全局号段。

**引用约定**：`§0`–`§11` 指架构文档的章节（`§3` 为决策索引）；`§D0x.y` 指对应子 ADR 内的子节；其它文档的章节号自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

---

## 1. 决策清单（D01–D12）

| 编号 | 决策标题 | 一句话要点 | 关联判据 |
|------|---------|-----------|-----------|
| [D01](ADR-086-D01-l0-l1-leaf-cache-split.md) | L0 / L1 两级 **leaf 翻译缓存**的职责划分 | 两级都只存 leaf 翻译；L0 = 8 项全相联 / 4KiB；L1 = 256 set × 8 way / 大页；级联 L0→L1→PTW | U-L1-01/03 |
| [D02](ADR-086-D02-cascaded-lookup-latency-tiers.md) | 级联 lookup 与 hit latency 分档 | 串行级联；L0 命中 1 拍 / L1 命中 2 拍 / PTW 4~6 拍；**与 openspec spec 的并行查分叉** | U-L1-02/10 |
| [D03](ADR-086-D03-l1-hit-backfill-l0.md) | L1 命中回填 L0 | 回填是 L1 容量收益兑现的前提；**但大页 entry 无法降级写成 L0 的 4KiB entry** ⇒ 需 L0 支持多粒度（§0 Q2） | U-L1-04 |
| [D04](ADR-086-D04-l1-organization-params.md) | L1 的组织参数 | `256 sets × 8 ways × EPL 8` = 16384 entry；`sets`/`ways`/`epl` 三旋钮独立；**ways 与 epl 正交** | U-L1-11 |
| [D05](ADR-086-D05-set-index-and-xor-interleave.md) | set index 与 XOR interleave | `way = vaddr[G+2:G]`；`set = fold(vaddr[31:G+3])`；XOR 折叠**仅在 entry 粒度 < 槽位粒度时**才有意义，否则退化为常量 | U-L1-12 |
| [D06](ADR-086-D06-multi-granularity-page-shift-mask.md) | entry 粒度选择 + per-way `page_shift` 掩码比较 | **单 bank + per-way 掩码**（比较器面积 ×8）；**4KiB 归 L0 / 大页归 L1**；跨 bank 假命中由级联顺序裁决 | U-L1-06 |
| [D07](ADR-086-D07-leaf-entry-fields-and-width.md) | leaf entry 字段与 8B 宽度 | `{valid, tag(20b), pfn(22b), perms(4b), asid, global, reserved}` = 60 bit ⇒ 8B；**与 ADR-085 D05 的 8B 目录项 entry 是不同实体** | U-L1-05 |
| [D08](ADR-086-D08-l0-organization.md) | L0 的组织 | 8 项全相联；`paddr` 按 `entry.page_shift` 参数化合成；本期默认 **L0 单粒度 4KiB**（比较器最简） | U-L1-01 |
| [D09](ADR-086-D09-coherence-and-invalidation.md) | 一致性与失效 | 4 条路径：L1 evict ⇒ 反向失效 L0（复用 spec MUST）/ `satp` 变更 ⇒ 两级 flush / `sfence.vma` 推迟 / walk 结果按粒度落 L0 或 L1 | U-L1-07/08/09 |
| [D10](ADR-086-D10-capacity-and-node-budget.md) | 容量与节点预算 | 目标 256×8（~1.6~4.9 万 reg）**不在本期**；本期 P0 = 16×4×8 = 512 entry；缓解手段：entry 打包成 1 个 `ch_uint<64>`（节点 ÷3） | U-L1-11 |
| [D11](ADR-086-D11-relation-to-adr085-and-spec.md) | 与 ADR-085 / openspec spec 的关系 | **13 条 delta 表**（D02/D12/D14/D15/D17/D19/D20 受影响）；spec 分叉登记；命名冲突处置 | — |
| [D12](ADR-086-D12-verification-criteria.md) | 验证判据 | `U-L1-01` … `U-L1-12` + **7 项变异对照** + e2e + 5 门禁 | 全部 |

---

## 2. 修订历史

| 版本 | 日期 | 内容 |
|------|------|------|
| rev.1 | 2026-10-23 | 初稿：依据用户 4 项设计答复（L1 组相连 + XOR interleave / per-way 掩码 / L1→L0 回填 / 目标 256×8 本期小值）+ 现状核实，产出 D01–D12 + 架构正文 |

### 2.1 设计过程中发现的 3 处冲突（已写入 §0 待确认项）

| # | 冲突 | 处置 |
|---|------|------|
| 1 | **`2M` 在当前 32-bit 平台不是合法页大小**（sv32 巨页 = 4 MiB；2 MiB 是 sv39 粒度） | §0 **Q1**，给出 3 个自洽候选 + 默认值 |
| 2 | **用户公式 `2M × cacheline_size/8B` 与「256 set × 8 way × EPL 8」不能同时成立**（way 槽位 2MiB < 巨页 4MiB ⇒ 1 entry 跨 2 槽位） | §0 **Q1** |
| 3 | **XOR 折叠要求 entry 粒度 < 槽位粒度**，否则退化为常量 ⇒ 分布偏斜 | D05 明确"折叠不影响正确性"+ §0 **Q1** |

---

## 3. 与既有物的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-085**（MMU_PTW 架构） | **增量对象**，不回改；13 条 delta 见 [D11](ADR-086-D11-relation-to-adr085-and-spec.md)。本组的 L1 存 **leaf**，ADR-085 的 PTWCache 存**目录项** ⇒ 两者共存、职责不同 |
| **`openspec/specs/mmu-multi-level-coherence/spec.md`** | ⚠ **语义分叉**：spec 要求"并行查所有层 + 取最深 hit"，本组用**串行级联**。spec 约束的是 **TLM 侧** C++ 类（用户已定"不做 TLM 侧"）⇒ 不是违约；但 CH_MEM 侧**不得**复用 `MultiLevelTLB` 类名 |
| **`ip/mmu/lib/multi_level_tlb.h:25`** + `soc/mmu_minimal.json:21-23` | ⚠ **命名冲突**：这里的 `L0`/`L1` 指 **TLM 侧 TLB 深度层**，本组的 `L0 TLB`/`L1 TLB` 指 **leaf 翻译缓存两级** ⇒ 处置见 D11 决策 3 |
| **ADR-040 v2.0** | §10.5 payload key mode-aware；`array_store` 双缓冲（D10 缓解手段 3） |
| **ADR-046** | 多周期 FSM 豁免（PTW 侧，ADR-085 已定） |
| **ADR-083** | Plugin-style SSOT ⇒ CH_MEM 新文件落 `ip/mmu/plugins/` |
| **ADR-076**（规划中，ASID vs 全局 TLB 冲刷） | 与 D09 路径 3（`satp` flush-all 的粗粒度限制）主题相邻 |

---

## 4. 门禁

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh
```
