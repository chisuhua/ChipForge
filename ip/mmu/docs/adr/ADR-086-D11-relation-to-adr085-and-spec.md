# ADR-086-D11 — 与 ADR-085 / openspec spec 的关系（delta 表 + 分叉登记）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [architecture.md](../architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D11` |
| 关联验证判据 | — |
| 受影响文件 | `docs/architecture/adr.md`、`ip/mmu/docs/README.md`、`ip/mmu/docs/architecture.md` |

## 上下文

本组与已提交的 ADR-085 高度相关（同一个 MMU、同一个 CH_MEM 前向架构），且与已 archive 的 openspec spec `mmu-multi-level-coherence` 有语义分叉。必须逐条登记，避免两份文档互相矛盾。

## 决策正文

**决策 1 — 不改 ADR-085 原文，单向增量**：ADR-085 已 commit 且过了 3 轮 critic ⇒ **不回改**。本组以 **delta 表**声明对 ADR-085 的影响。

**delta 表（规范性）**：

| ADR-085 条款 | 原内容 | 本组的影响 | 处置 |
|---|---|---|---|
| **D01**（MMU 拆 TLB+PTW） | MMU_TLB + MMU_PTW 两单元 | **不变** | — |
| **D02**（TLBCache entry 与 cacheline） | 单个 TLBCache，8 项全相联，entry `{valid,asid,tag,pfn,perms,page_shift,global}` | **TLBCache 分裂为 L0（8 项全相联）+ L1（256×8 组相连）** | 本组 D01/D04/D07/D08 细化；ADR-085 D02 的字段集**沿用**（D07 只补位宽与 `pfn` 粒度说明） |
| **D04**（PTWCache 只存非叶目录项） | PTWCache 存目录项 | **不变**（本组的 L1 存 leaf，PTWCache 仍是目录项缓存，两者共存但职责不同） | ⚠ 需在 ADR-085 的索引里加一行说明，避免"两套 leaf 缓存"的误读 |
| **D05**（entry 固定 8B） | PTWCache 目录项 entry 8B | **不变**；另立 leaf entry 8B（D07），**两者是不同实体** | D07 明确区分 |
| **D06**（替换策略 / 直接映射） | PTWCache CH_MEM 用直接映射 | **不变**（本组 L1 用组相连，对象不同） | — |
| **D07**（`start_level`） | PTWCache 命中 ⇒ start_level | **不变** | — |
| **D12**（`PTW_ACTIVE` 组合派生） | `sv_active && !paddr_valid` | **扩展**：miss 检出现在发生在 **L0**（更早）⇒ `PTW_ACTIVE` 仍须在 **L0 miss 同拍**为 1（L1 miss 时保持 1） | 本组 D02 定义分档；**判据口径不变**（AC-FS-9 仍成立） |
| **D14**（superpage 两侧） | TLB entry 加 `page_shift` + 掩码 tag 比较 | **扩展**：粒度从 per-entry 变为 **per-way**；新增粒度归属规则（4KiB→L0，大页→L1） | 本组 D06；ADR-085 D14 的机制不推翻 |
| **D15**（walk 只回填 TLB） | 结果回填 TLB 的对应 entry | **扩展**：回填目标随粒度而变（4KiB→L0，大页→L1） | 本组 D09 |
| **D17**（容量上限） | CH_MEM：TLB 1×8×1 / PTWCache 1×1×4 | **扩展**：加 L1 的 per-mode 默认（本期 16×4×8）+ 目标 256×8 | 本组 D10 |
| **D19**（验证判据 U1–U11） | 11 条判据 | **扩展**：本组加 U1–U12（见 D12），其中 U11 与 ADR-085 的 U11 同名不同义 ⇒ **本组改用 `U-L1-11` 记法** | D12 记录 |
| **D20**（`satp` flush） | flush-all TLBCache | **扩展**：flush 范围含 L1 | 本组 D09 |

**决策 2 — 与 `openspec/specs/mmu-multi-level-coherence/spec.md` 的分叉（必须登记）**：

| 维度 | 该 spec（TLM 侧 `MultiLevelTLB`） | 本组（CH_MEM 侧） | 关系 |
|---|---|---|---|
| lookup | **并行**查所有层，取**最深** hit | **串行级联** L0→L1 | **语义分叉** |
| shadow fill | 深命中 → 填浅层 | 同向（L1 命中 → 填 L0） | **一致** |
| deep evict | 反向失效浅层 | 同 | **一致** |
| asid / all 失效 | 全层 | 全层 | **一致** |

⇒ **不是违约，是后端分叉**：该 spec 约束的是 TLM 侧 C++ 类 `MultiLevelTLB`（`ip/mmu/lib/multi_level_tlb.cpp`），用户已明确「**不做 TLM 侧**」⇒ spec 仍成立。
⇒ **约束**：CH_MEM 侧实现**不得**复用 `MultiLevelTLB` 类名 / `multi_tlb_` 成员名 ⇒ 避免与 spec 术语撞车。本组用 `l0_tlb_` / `l1_tlb_`。
⇒ **登记位置**：本组架构文档 §5「与现有架构的关系」显式列出该分叉。

**决策 3 — 命名冲突的处置**：`L0 TLB` / `L1 TLB` 在本组内指 leaf 翻译缓存两级（D01）；而 `ip/mmu/lib/multi_level_tlb.h:25` 与 `soc/mmu_minimal.json:21-23` 的 `L0`/`L1` 指 **TLM 侧 TLB 深度层**（两套并存、语义不同）。
⇒ 处置：① **TLM 侧不动**（用户已定）⇒ 不改 `multi_level_tlb.h` 注释与 `params_schema.json` 的 `levels[].name`；② 本组文档内一律写全「**L0 TLB**」「**L1 TLB**」并注明"leaf 翻译缓存"；③ **在 [总架构文档](../architecture.md) §0/§5 与 [TLM 侧文档](../mmu-tlm-architecture.md) 顶部加命名警示**（2026-10-23 文档重组时已落地）；④ **文档职责重组**：`ip/mmu/docs/architecture.md` 改为**总架构文档**（跨 L0/L1/PTW 数据流 + 外部对接），原 TLM 侧内容外移到 `ip/mmu/docs/mmu-tlm-architecture.md`，L0/L1 各自成文。

## 交叉引用

- 架构文档：[architecture.md](../architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D02](ADR-086-D02-cascaded-lookup-latency-tiers.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)
