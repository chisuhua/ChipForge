# ADR-086: MMU L0 / L1 TLB 架构 — 伞索引（D01–D12）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始 —— 归属未立项 change `chmem-ptw-walk` 的后续增量）<br>注：全局注册表 `docs/architecture/adr.md` 只有 `✅/⚠️/🚧` 三态、无 📝 ⇒ 本 ADR 在那张表里登记为 **🚧 + 备注"设计提案"** |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡 ⇒ 归 `ip/mmu/docs/adr/`） |
| 架构文档 | **[architecture.md](../architecture.md)** —— **MMU 总架构**（跨 L0/L1/PTW 数据流 + 外部对接）|
| 分册 | [L0 文档](../mmu-l0-tlb-architecture.md) · [L1 文档](../mmu-l1-tlb-architecture.md) · [PTW 文档](../mmu-ptw-architecture.md)（ADR-085）|
| 决策文件 | 本目录下 `ADR-086-D01` … `ADR-086-D12`（**12 个，一决策一文件**）|
| 增量对象 | [ADR-085](ADR-085-mmu-ptw-architecture.md)（MMU_PTW 架构）—— 本组**不回改**它，delta 见 D11 |
| 平台前提 | **Sv39**（3 级页表：VPN[2]/VPN[1]/VPN[0] 各 9 bit + 12 bit 页内偏移）⇒ **需 64-bit XLEN**，见 [总架构 §4](../architecture.md) |
| 来源 | 用户设计（2026-10-23 讨论）：L0/L1 都是 leaf 翻译缓存，L0 全相联小而快，L1 组相连大容量，级联查询 |

---

## 0. 如何阅读

| 文件 | 内容 | 何时读 |
|------|------|--------|
| **[总架构文档](../architecture.md)** | 跨 L0/L1/PTW 的数据流与时序、三单元职责边界、外部对接（satp / fetch-stall / 物理内存 / SoC JSON）、风险登记 | **先读这个** |
| [L0 文档](../mmu-l0-tlb-architecture.md) | L0 内部结构（8 项全相联 + 多粒度 + entry 字段 + 回填失效） | 改 L0 时 |
| [L1 文档](../mmu-l1-tlb-architecture.md) | L1 内部结构（256×8 组相连 + set index + 粒度 + 容量预算） | 改 L1 时 |
| **12 个子 ADR**（`ADR-086-D01..D12`） | 每个决策一个文件：上下文 / 决策正文 / 交叉引用 | 按需精读 |

**全局注册表只登记本条**（ADR-086），子决策用 `ADR-086-Dnn` 子号，不占全局号段。

**引用约定**：`§0`–`§11` 指各架构文档的章节；`§D0x.y` 指对应子 ADR 内的子节；其它文档的章节号自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、`openspec/specs/mmu-multi-level-coherence/spec.md`）。

---

## 1. 决策清单（D01–D12）

| 编号 | 决策标题 | 一句话要点 | 归属文档 | 关联判据 |
|------|---------|-----------|---------|-----------|
| [D01](ADR-086-D01-l0-l1-leaf-cache-split.md) | L0 / L1 两级 **leaf 翻译缓存**的职责划分 | 两级都只存 leaf 翻译；L0 = 8 项全相联；L1 = 256 set × 8 way 组相连；级联 L0→L1→PTW | 总架构 §1 | U-L1-01/03 |
| [D02](ADR-086-D02-cascaded-lookup-latency-tiers.md) | 级联 lookup 与 hit latency 分档 | 串行级联；L0 命中 1 拍 / L1 命中 2 拍 / PTW 4~6 拍；**与 openspec spec 的并行查分叉** | 总架构 §2 | U-L1-02/10 |
| [D03](ADR-086-D03-l1-hit-backfill-l0.md) | L1 命中回填 L0 | 回填是 L1 容量收益兑现的前提；**前提 = L0 支持多粒度**（用户已定） | 总架构 §2.4 | U-L1-04 |
| [D04](ADR-086-D04-l1-organization-params.md) | L1 的组织参数 | `256 sets × 8 ways × EPL 8`；`cacheline 96B` / `entry 12B` / `lines_per_way` 导出；**ways 与 epl 正交** | L1 §2 | U-L1-03/11 |
| [D05](ADR-086-D05-set-index-and-xor-interleave.md) | set index 与 XOR interleave | 基准 **16MiB**：`blk = vaddr[38:24]`(15b)，`way = blk[2:0]`，`set = blk[14:7] ^ {blk[6:3],4'b0}` | L1 §3 | U-L1-12 |
| [D06](ADR-086-D06-multi-granularity-page-shift-mask.md) | entry 粒度 + per-way `page_shift` 掩码比较 | Sv39：**4 KiB / 2 MiB**（1 GiB 本期不考虑）；**单 bank + per-way 掩码**（比较器 ×8） | L1 §4 | U-L1-05/06/11 |
| [D07](ADR-086-D07-leaf-entry-fields-and-width.md) | leaf entry 字段与 **12B** 宽度 | `{valid, global, perms(4), tag(27), pfn(44), asid(16), reserved(3)}` = 96 bit；**与 ADR-085 D05 的 8B 目录项 entry 是不同实体** | L1 §5 | U-L1-05/11 |
| [D08](ADR-086-D08-l0-organization.md) | L0 的组织 | 8 项全相联 + **多粒度（per-entry 掩码）** + `perms` + `page_shift` | L0 §3 | U-L1-01/04/05 |
| [D09](ADR-086-D09-coherence-and-invalidation.md) | 一致性与失效 | 4 条路径：L1 evict ⇒ 反向失效 L0 / `satp` 变更 ⇒ 两级 flush / `sfence.vma` 推迟 / walk 结果按粒度落 L0 或 L1 | 总架构 §2.4 | U-L1-07/08/09 |
| [D10](ADR-086-D10-capacity-and-node-budget.md) | 容量与节点预算 | 目标 256×8（~1.6 万 reg，entry 打包成 1 个 `ch_uint<96>`）**不在本期**；本期 P0 = 16×4×8 = 512 entry | L1 §6 | U-L1-11 |
| [D11](ADR-086-D11-relation-to-adr085-and-spec.md) | 与 ADR-085 / openspec spec 的关系 | **13 条 delta 表**；spec 分叉登记；命名冲突处置 | 总架构 §5 | — |
| [D12](ADR-086-D12-verification-criteria.md) | 验证判据 | `U-L1-01` … `U-L1-12` + **7 项变异对照** + e2e + 5 门禁 | L0 §6 / L1 §8 | 全部 |

---

## 2. 修订历史

| 版本 | 日期 | 内容 |
|------|------|------|
| rev.1 | 2026-10-23 | 初稿：依据用户 4 项设计答复（L1 组相连 + XOR interleave / per-way 掩码 / L1→L0 回填 / 目标 256×8 本期小值）+ 现状核实，产出 D01–D12 |
| **rev.2** | 2026-10-23 | ① **平台前提改为 Sv39** ⇒ 2 MiB 页合法 ⇒ 之前"2M 在 32-bit 平台不合法"的冲突**消解**；② **Q1 口径细化**：set interleave 基准 = **16MiB**，line 覆盖 16MiB（2MiB 粒度）/ 32KiB（4KiB 粒度）；③ **粒度口径 per-way**（保留 8 份掩码而非 64 份）；④ **leaf entry 8B → 12B**（Sv39 的 tag 27 + pfn 44 装不下 64 bit）⇒ `cacheline 64B → 96B`；⑤ **文档重组**：`ip/mmu/docs/architecture.md` 改为**总架构文档**，L0 / L1 各自成文，TLM 侧内容外移；⑥ **1 GiB 页本期不考虑**（用户已定） |

### 2.1 设计过程中发现并已消解的冲突

| # | 冲突（rev.1 时期） | 消解方式（rev.2） |
|---|------------------|-----------------|
| 1 | `2M` 在 32-bit 平台不是合法页大小（sv32 巨页 = 4 MiB） | **平台改为 Sv39** ⇒ 2 MiB 合法 ⇒ 冲突消失；`WAY_GRANULE` 参数化保留 |
| 2 | `2M × cacheline_size/8B` 与「256 set × 8 way × EPL 8」不能同时成立 | 用户明确 **interleave 基准 = 16MiB** ⇒ `M = 16MiB / (epl × 2^page_shift)` 导出 ⇒ 自洽（2MiB 粒度 ⇒ M=1） |
| 3 | 「each entry 可 2MiB 或 4KiB」（per-entry）vs 「a line 可 16MiB 或 32KiB」（per-way） | 取 **per-way**（保留 8 份掩码）；精确表述为"**每个 way 选一种粒度**" |
| 4 | XOR 折叠要求 entry 粒度 < 槽位粒度 | 本组槽位粒度 = 16MiB ≫ entry 粒度 ⇒ 折叠始终有效 ✓ |
| 5 | leaf entry 8B（sv32 口径）在 Sv39 下装不下 | 改 **12B**（96 bit）⇒ `cacheline 96B = 8 × 12B`，EPL 仍 8 |

### 2.2 仍未消解、已登记为风险

| # | 项 | 登记位置 |
|---|----|---------|
| 1 | **Sv39 需 64-bit XLEN，而全仓是 32-bit**（5 处 `kXlenBits = 32`）⇒ 平台升级是**前置条件** | 总架构 §4 + **R1（最高）** |
| 2 | 与 `openspec/specs/mmu-multi-level-coherence/spec.md` 的"并行查 + 取最深 hit"**语义分叉** | 总架构 §5 + D11 决策 2 |
| 3 | `L0`/`L1` **同名不同义**（TLM 侧深度层 vs 本组 leaf 缓存级） | 总架构 §0/§5 + D11 决策 3 + TLM 文档顶部警示 |

---

## 3. 与既有物的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-085**（MMU_PTW 架构） | **增量对象**，不回改；13 条 delta 见 [D11](ADR-086-D11-relation-to-adr085-and-spec.md)。本组的 L1 存 **leaf**，ADR-085 的 PTWCache 存**目录项** ⇒ 两者共存、职责不同 |
| **`openspec/specs/mmu-multi-level-coherence/spec.md`** | ⚠ **语义分叉**：spec 要求"**并行**查所有层 + 取**最深** hit"，本组用**串行级联**。spec 约束的是 **TLM 侧** C++ 类（用户已定"不做 TLM 侧"）⇒ 不是违约；shadow fill 与 deep-evict 反向失效**语义一致，可直接复用**。CH_MEM 侧**不得**复用 `MultiLevelTLB` 类名 |
| **`ip/mmu/lib/multi_level_tlb.h` + `soc/mmu_minimal.json`** | ⚠ **命名冲突**：这里的 `L0`/`L1` 指 **TLM 侧 TLB 深度层**，本组指 **leaf 翻译缓存两级** ⇒ 处置见 D11 决策 3 |
| **ADR-040 v2.0** | §10.5 payload key mode-aware；`array_store` 双缓冲（D10 缓解手段 3）|
| **ADR-046** | 多周期 FSM 豁免（PTW 侧，ADR-085 已定）|
| **ADR-083** | Plugin-style SSOT ⇒ CH_MEM 新文件落 `ip/mmu/plugins/` |
| **ADR-076**（规划中，ASID vs 全局 TLB 冲刷）| 与 D09 的 `satp` flush-all 粗粒度限制主题相邻 |

---

## 4. 门禁

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh
```
