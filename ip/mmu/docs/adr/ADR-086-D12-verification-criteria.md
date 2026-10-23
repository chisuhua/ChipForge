# ADR-086-D12 — 验证判据（U1–U12 + 变异对照）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D19 判据） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D12` |
| 关联验证判据 | U-L1-01 – U-L1-12 |
| 受影响文件 | `tests/mmu/`、`tests/cpu/` |

## 上下文

ADR-085 D19 已给 MMU_PTW 侧的 U1–U11。本组是**两级 leaf 缓存**，需要自己的一组判据。⚠ **编号冲突**：ADR-085 已有 U11（TLB superpage entry），本组的 U11 语义不同 ⇒ **本组用 `U-L1-nn` 记法**（见 D11 决策 1）。

## 决策正文

**决策**：本组的判据集（`U-L1-nn`）：

| # | 判据 |
|---|---|
| `U-L1-01` | **L0 命中** ⇒ 1 拍产出 `PADDR`（`PADDR_VALID=1`）且 `perms` 正确；`PTW_ACTIVE=0` |
| `U-L1-02` | **L0 miss** ⇒ **同拍** `PTW_ACTIVE=1`（protocol AC-FS-9，不靠桩）且 L1 被查 |
| `U-L1-03` | **L1 命中** ⇒ 2 拍产出 `PADDR`（大页拼接正确：`paddr = (pfn << page_shift) \| (vaddr & mask)`） |
| `U-L1-04` | **L1 命中 ⇒ 回填 L0**（4KiB 粒度场景）⇒ 下一次同 VA 访问 **1 拍**命中 L0 |
| `U-L1-05` | **大页 `pfn` 粒度正确**：4MiB entry 的 `pfn` = 4MiB 页基址 PPN（12 bit），**不是** 4KiB 粒度 PPN（22 bit）⇒ 断言 `paddr` 与 4KiB 路径**不同**且落在预期页内 |
| `U-L1-06` | **per-way 掩码比较**：4KiB entry 与大页 entry 覆盖同一 VA 时，**不得**同时命中 ⇒ 断言恰好 1 个 way 命中 |
| `U-L1-07` | **L1 evict ⇒ 反向失效 L0**：L0 中同 `(vaddr, asid)` 的副本 `valid=0` ⇒ 后续该 VA 不得命中 L0 |
| `U-L1-08` | **`satp` 变更 ⇒ 两级全清**：写 `satp` 后同一 vaddr 不得命中旧 L0/L1 entry；新根表的翻译生效（ADR-085 D20 扩展） |
| `U-L1-09` | **walk 期间地址变更** ⇒ 旧 walk 结果**不**回填到新地址（ADR-085 D15 / U7） |
| `U-L1-10` | **hit latency 分档**（D02）：L0 命中 1 拍 / L1 命中 2 拍 ⇒ 逐拍断言拍数，不得多/少 |
| `U-L1-11` | **容量与组织自检**：`sets`/`ways`/`epl` 配置生效；entry 总数 = `sets × ways × epl`；超容量写入触发逐出且不破坏其它 entry |
| `U-L1-12` | **set index 一致性**：同一 VA 在 **lookup** 与 **refill** 两条路径算出的 `(set, way)` **必须相同** ⇒ 断言 refill 写入的槽位 == lookup 比较的槽位 |

**变异对照（项目惯例 ≥3，本组列 7）**：

| # | 变异 | 应 RED 的判据 |
|---|------|-------------|
| ① | L0 命中不置 `PTW_ACTIVE=0` | `U-L1-01` |
| ② | L1 命中不回填 L0 | `U-L1-04` |
| ③ | 大页 `pfn` 存 4KiB 粒度 PPN | `U-L1-05` |
| ④ | set index 在 refill 与 lookup 用不同公式 | `U-L1-12` |
| ⑤ | L1 evict 不反向失效 L0 | `U-L1-07` |
| ⑥ | `satp` 变更只清 L0 不清 L1 | `U-L1-08` |
| ⑦ | per-way 掩码退化为全 VPN 直比 | `U-L1-06` |

**e2e**：真页表（页表内容在内存里）⇒ L0/L1 两级都有条目 ⇒ 走完整级联 ⇒ 非恒等翻译生效。

**门禁**：5 个架构 gate 全 PASS（命令见架构文档 §10）。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D19 判据与 6 项变异对照）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D02](ADR-086-D02-cascaded-lookup-latency-tiers.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)、[D10](ADR-086-D10-capacity-and-node-budget.md)
