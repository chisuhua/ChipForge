# MMU L0 TLB 架构设计 — 8 项全相联的多粒度快路径

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 上级文档 | [architecture.md](architecture.md) —— **MMU 总架构**（跨 L0/L1/PTW 数据流 + 外部对接）|
| 决策组 | [ADR-086](adr/ADR-086-l0-l1-tlb-architecture.md)（D01/D02/D03/D08/D09/D12 与 L0 相关部分）|
| 平台 | **Sv39**（需 64-bit XLEN，见 [总架构 §4](architecture.md)）|
| 范围 | **仅 CH_MEM 侧**（TLM 侧不动）|

> **L0 的定位**：MMU 翻译路径的**第一站**与**唯一 1 拍路径**。所有访问先查 L0；L0 命中 ⇒ 1 拍出 `paddr`；L0 miss ⇒ 同拍置 `PTW_ACTIVE=1` 并向 L1 发请求（级联，见 [总架构 §2.2](architecture.md)）。

---

## 0. 待确认项

### Q-L0-1 —— leaf entry 的宽度不再是 8B（Sv39 带来的变化）

ADR-086 D07（初版）按 **sv32** 算：tag 20 + pfn 22 + perms 4 + valid 1 = 47 bit ⇒ 8B 够。
**Sv39 下**：

| 字段 | 宽度 | 说明 |
|------|-----:|------|
| `valid` | 1 | |
| `global` | 1 | RISC-V Global 位 |
| `perms` | 4 | R/W/X/U（A/D 位预留） |
| `tag` | **27** | `vaddr[38:12]`（Sv39 VPN 27 bit） |
| `pfn` | **44** | Sv39 PPN（4 KiB 粒度）；**按 entry 自身页粒度存**（ADR-085 D14 §D14.1 第 4 条） |
| `asid` | **16** | Sv39 ASID 位宽（本期只预留，不参与比较 ⇒ ASID=0 时可裁掉 16 bit ⇒ 61 bit ⇒ 8B 够） |
| `reserved` | 3 | 预留 per-entry `page_shift` 等 |
| **合计** | **96 bit = 12B** | 含 asid；**61 bit = 8B**（不含 asid） |

⇒ **默认取 12B（含 asid 预留）**，代价是 64B cacheline 装不下整数个 ⇒ **cacheline 调整为 96B = 8 × 12B**（EPL 仍为 8）。
⇒ **与 ADR-085 D05 的 8B 不冲突**：那是 **PTWCache 目录项** entry（字段集不同，是另一个实体，见 ADR-086 D07）。

### Q-L0-2 —— L0 的多粒度支持（已裁决：支持）

用户已定「**L0 支持多粒度**」⇒ 比较器为 **per-entry 掩码比较**（8 份掩码）。
⇒ 好处：L1 命中回填 L0 时，**大页 entry 也能回填**（否则大页访问永远 2 拍，ADR-086 D03）。
⇒ 代价：8 路比较器各带自己的 `page_shift` 掩码 ⇒ 面积 ×8（仅 8 个槽位，远小于 L1）。

---

## 1. 职责与边界

| 项 | 内容 |
|----|------|
| **缓存什么** | **leaf 翻译** `{paddr, perms, page_shift}`（4 KiB / 2 MiB）|
| **不缓存什么** | 页目录项（那是 PTWCache 的职责，ADR-085 D04）|
| **组织** | 8 项**全相联**（1 set × 8 ways）|
| **延迟** | **1 拍**（组合比较，无级联开销）|
| **端口** | 2 个（ifetch / loadstore）**共享同一存储体** ⇒ 比较器 2 份 ⇒ 面积 ×2（已知代价）|
| **填充来源** | ① PTW walk 结果（4 KiB 粒度）② L1 命中回填（多粒度）|

---

## 2. 现状与差距

| 项 | 现状（`ip/mmu/plugins/MMUPlugin_chmem.h`） | 目标 |
|----|------------------------------------------|------|
| 项数 | 8 全相联 | 8 全相联（**不变**）|
| entry | `{valid, vaddr, paddr}`（3 个 `ch_reg`/项 = 24 reg）| `{valid, global, perms, tag, pfn, asid, reserved}`（12B/项）|
| tag | 隐式 = `vaddr[31:12]` 直接比 | 显式 `tag` 字段 + **per-entry 掩码比较** |
| `perms` | **无** ⇒ 权限检查无处落 | 有（ADR-085 D13 要求）|
| `page_shift` | **无** ⇒ 恒按 4 KiB 合成 `paddr` | 有 ⇒ 2 MiB 也能缓存 |
| `paddr` 合成 | `(pfn << 12) \| (vaddr & 0xFFF)` | `(pfn << page_shift) \| (vaddr & mask)` |
| 命中输出 | `PADDR` / `PADDR_VALID` | 同 + `perms`（供权限检查）|

---

## 3. 结构与数据通路

```
      vaddr(39 bit)              entry_i (12B)
          │                          │
          ├─► vaddr[38:12] (tag)  ───┼──► 掩码比较（per-entry page_shift）
          │        │                │
          │        └─► & mask_i ────┘   mask_i = ~0 << page_shift_i
          │                              │
          │  hit_i = valid_i && (tag_i == (vaddr & mask_i)) && (asid/global 匹配)
          │
          ├─► hit = OR(hit_i)
          └─► paddr = select(hit_i, (pfn_i << page_shift_i) | (vaddr & mask_i))
```

**关键点**：

1. **tag 存全 VPN（27 bit）** ⇒ 比较时按该 entry 自己的 `page_shift` 掩码 ⇒ **不同粒度的 entry 可以共存于同一组比较器**。
2. **命中不唯一时**：同一 VA 可能被 4 KiB entry 与 2 MiB entry 同时覆盖 ⇒ **优先级规则**：先命中者由 `select` 链的顺序决定 ⇒ **规定小粒度优先**（4 KiB 在前），因为 4 KiB 映射是"更具体"的翻译。
3. **2 端口共享存储体** ⇒ 2 份比较逻辑 ⇒ 面积 ×2（已知代价，ADR-085 D02 已记录）。

---

## 4. 回填与失效（L0 侧）

| 事件 | 动作 | 决策 |
|------|------|------|
| PTW walk 成功（4 KiB） | 回填 L0 | ADR-086 D09 |
| PTW walk 成功（2 MiB） | 回填 **L1**（L0 也可存，但优先 L1 以免占用小而快的 L0） | ADR-086 D09 |
| L1 命中 | 回填 L0（多粒度 ⇒ 大页也能回填） | ADR-086 D03 |
| L1 evict | **反向失效** L0 中同 `(vaddr, asid)` 的副本 | ADR-086 D09 |
| `satp` 变更 | L0 + L1 **两级 flush-all** | ADR-085 D20 + ADR-086 D09 |
| L0 满 | 固定槽位 / 轮转（**本期不引入组合替换逻辑**，理由同 ADR-085 D06） | ADR-086 D08 |

---

## 5. 决策映射

| 决策 | 与 L0 相关的部分 |
|------|-----------------|
| [D01](adr/ADR-086-D01-l0-l1-leaf-cache-split.md) | L0 = 8 项全相联 / 4 KiB 起 / 1 拍 |
| [D02](adr/ADR-086-D02-cascaded-lookup-latency-tiers.md) | L0 miss ⇒ 同拍 `PTW_ACTIVE=1` ⇒ 向 L1 发请求 |
| [D03](adr/ADR-086-D03-l1-hit-backfill-l0.md) | L1 命中 ⇒ 回填 L0（依赖 L0 多粒度支持）|
| [D08](adr/ADR-086-D08-l0-organization.md) | L0 的完整组织（entry 字段 / 多粒度比较 / 替换）|
| [D09](adr/ADR-086-D09-coherence-and-invalidation.md) | L0 侧的 4 条失效路径 |
| [D12](adr/ADR-086-D12-verification-criteria.md) | `U-L1-01/02/04/05/07/08/09/10` |

---

## 6. 验证判据（L0 侧子集）

| # | 判据 |
|---|------|
| `U-L1-01` | L0 命中 ⇒ **1 拍**产出 `PADDR`（`PADDR_VALID=1`）且 `perms` 正确；`PTW_ACTIVE=0` |
| `U-L1-02` | L0 miss ⇒ **同拍** `PTW_ACTIVE=1`（protocol AC-FS-9，不靠桩）且 L1 被查 |
| `U-L1-04` | L1 命中 ⇒ **回填 L0** ⇒ 下次同 VA **1 拍**命中 L0 |
| `U-L1-05` | **大页 `pfn` 粒度正确**：2 MiB entry 的 `pfn` = 2 MiB 页基址 PPN（35 bit），**不是** 4 KiB 粒度 PPN（44 bit）⇒ 断言 `paddr` 与 4 KiB 路径**不同**且落在预期页内 |
| `U-L1-07` | L1 evict ⇒ L0 中同 `(vaddr, asid)` 副本 `valid=0` |
| `U-L1-08` | `satp` 变更 ⇒ L0/L1 **两级全清** |
| `U-L1-10` | hit latency 分档：L0 命中 1 拍 / L1 命中 2 拍 |

**变异对照（L0 相关）**：① L0 命中不置 `PTW_ACTIVE=0` ⇒ `U-L1-01` RED；③ 大页 `pfn` 存 4 KiB 粒度 PPN ⇒ `U-L1-05` RED；⑤ L1 evict 不反向失效 L0 ⇒ `U-L1-07` RED；⑥ `satp` 变更只清 L0 不清 L1 ⇒ `U-L1-08` RED。

---

## 7. 验证命令

```bash
# 架构门禁（5 个）
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh

# L0 结构性检查
grep -q "page_shift" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: L0 entry 缺 page_shift（D08）"
grep -q "perms"      ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: L0 entry 缺 perms（D07/D08）"

# 回归
./build/bin/chipforge_tests_chmem "[mmu-chmem-basic]"          # CH_MEM
bash tools/run_chipforge_tests.sh --all                        # 全量（CH_MEM ~833s/轮）
```
