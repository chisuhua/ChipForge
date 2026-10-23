# ADR-086-D01 — L0 / L1 两级 **leaf 翻译缓存**的职责划分

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始 —— 归属未立项 change `chmem-ptw-walk` 的后续增量） |
| 父 ADR | 无（本组自成一组）；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [architecture.md](../architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D01` |
| 关联验证判据 | U-L1-01、U-L1-03 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h`（新增 L1 结构）、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

## 上下文

用户设计的 L0 TLB 与 L1 TLB **两者都缓存 leaf 页表项**（翻译结果），差别只在容量、组织方式与延迟；L0 miss 后再向 L1 TLB 发请求。这与 ADR-085 的 `TLBCache`（leaf）+ `PTWCache`（**目录项**）拆分**不同源** —— 本设计里 PTWCache 没有对应物。

## 决策正文

**决策**：MMU 的翻译缓存分**两级**，两级都只缓存 **leaf 翻译结果**（叶 PTE 派生的 `{paddr, perms, page_shift}`）：

| | L0 TLB | L1 TLB |
|---|---|---|
| 容量 | **8 项**（小） | **256 set × 8 way**（大） |
| 组织 | **全相联** | **组相连**（set-assoc） |
| 粒度 | **4 KiB**（`page_shift = 12`）+ **2 MiB**（`page_shift = 21`）| **2 MiB** 为主（`page_shift = 21`），4 KiB 需 `M=512` 本期不落地 |
| 延迟 | 1 拍（组合比较） | 2 拍（级联第二级） |
| 角色 | 热点页的**快路径** | 容量扩展 + 大页翻译 |
| 命中后 | — | **回填 L0**（见 D03） |

**三级级联**：`L0 miss → L1 → PTW walk`（PTW 由 ADR-085 定义，本组不改其结构）。

**平台前提（Sv39）**：本组的粒度集合依赖 **Sv39**（2 MiB 页合法）⇒ 需 **64-bit XLEN**；全仓当前是 32-bit（5 处 `kXlenBits = 32`）⇒ **平台升级是前置条件**，登记在 [总架构 §4](../architecture.md) 与 R1。

**术语裁决（规范性）**：本组内 `L0 TLB` / `L1 TLB` 一律指**本决策定义的 leaf 翻译缓存两级**。
⚠ 这与仓库旧约定冲突 —— `ip/mmu/lib/multi_level_tlb.h:25` 写的是 "L0 最快最小, Ln-1 最慢最大"（TLB **深度**轴），且 `soc/mmu_minimal.json:21-23` 的 `levels[].name` 就叫 `"L0"` / `"L1"`。
⇒ **同名不同义** ⇒ 处置见 D11（TLM 侧不动 + 显式登记分叉；CH_MEM 侧实现**不得**复用 `MultiLevelTLB` 类名）。

**与 ADR-085 的关系**：ADR-085 的 `TLBCache`（D02/D14/D17）在本设计下**分裂为 L0 与 L1 两个结构** ⇒ D02 的 entry 组织、D14 的 `page_shift` 掩码比较、D17 的容量表**均需按两级扩充**（delta 表见 D11）。

## 交叉引用

- 架构文档：[architecture.md](../architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)
- 相关决策：[D02](ADR-086-D02-cascaded-lookup-latency-tiers.md)、[D04](ADR-086-D04-l1-organization-params.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D08](ADR-086-D08-l0-organization.md)、[D11](ADR-086-D11-relation-to-adr085-and-spec.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
