# ADR-086-D09 — L0 / L1 一致性与失效路径

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D15 / D20） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D09` |
| 关联验证判据 | U-L1-07、U-L1-08、U-L1-09 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

两级缓存引入**跨级一致性**问题：L1 evict 了某 entry，L0 里可能还有同一 VA 的副本 ⇒ 命中 L0 会拿到**已被逐出的**翻译。ADR-085 D20 定义了 `satp` 变更的失效路径，但只针对单级 TLBCache。

## 决策正文

**决策（4 条失效路径）**：

| # | 触发 | 动作 | 归属 |
|---|------|------|------|
| **1** | **L1 逐出某 entry** | **反向失效 L0** 中同 `(vaddr, asid)` 的副本 | 本决策 |
| **2** | **L0 逐出** | 无需动作（L0 是浅层，被逐出不产生陈旧命中） | — |
| **3** | **`satp` 变更** | **flush-all 两级**（L0 全清 + L1 全清），采用 ADR-085 D20 的 `satp_changed` 影子寄存器比较 | ADR-085 D20 扩展到 L1 |
| **4** | **`sfence.vma`** | 按 `(vaddr, asid)` 精确失效两级；`rs2=x0, rs1=x0` ⇒ 全清 | 推迟到 CSR 批次（`cpu-pipeline-smode-umode`） |

**路径 1 的理由与复用**：`openspec/specs/mmu-multi-level-coherence/spec.md` 已有 MUST 级条款 "MUST reverse-invalidate shallow levels on deep evict"（L2 evict ⇒ L0/L1 匹配项 `valid=false`）⇒ **本决策原样复用该语义**（只把层数从 3 变 2）⇒ 不引入新概念。

**路径 3 的扩展**：ADR-085 D20 的 flush-all 机制（`satp_prev` 影子寄存器 + `satp_changed`）**逐字复用**，只是清零范围从「TLBCache」扩到「TLBCache + L1 TLB」。
⚠ **注意 D20 的已知限制在两级下更明显**：全清是粗粒度 ⇒ 每次 `satp` 写都清空两级 ⇒ 上下文切换后必有一次冷 walk + 冷 L1。登记为已知限制（ASID 粒度失效属 ADR-076 主题，见 ADR-085 D11 的关系表）。

**路径 1 的实现代价**：L1 逐出时需要**反查 L0** 是否有同 VA 副本 ⇒ L0 只有 8 项 ⇒ 组合上做 8 项比较（L0 的比较器可复用）⇒ 代价小。**但** CH_MEM 的逐出是 `ch_reg` 的 next 值写 ⇒ 反向失效要在**同一拍**把 L0 的对应 `valid` 置 0 ⇒ 需要"命中 L0 的哪一项"这一选择信号 ⇒ **可行**（L0 的 8 路命中 lane 已有）。

**walk 结果的落点（与 ADR-085 D15 对齐）**：ADR-085 D15 规定「PTW 不直接写 `PADDR`；结果只回填 TLB」⇒ 两级下：**walk 结果回填 L0**（4KiB 粒度，命中快路径）；**不**同时回填 L1（避免两级同时持有同一 4KiB 翻译，浪费 L1 的大页容量）。
⚠ 若 walk 结果是**大页**（`page_shift > 12`）⇒ 回填 L1（L0 单粒度，见 D08.1）⇒ **两级回填目标随粒度而变**。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D15 只回填 TLB、D20 `satp` flush）
- 相关决策：[D02](ADR-086-D02-cascaded-lookup-latency-tiers.md)、[D03](ADR-086-D03-l1-hit-backfill-l0.md)、[D08](ADR-086-D08-l0-organization.md)、[D11](ADR-086-D11-relation-to-adr085-and-spec.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
