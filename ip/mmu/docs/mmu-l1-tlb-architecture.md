# MMU L1 TLB 架构设计 — 256 set × 8 way 组相连的容量级

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 上级文档 | [architecture.md](architecture.md) —— **MMU 总架构**（跨 L0/L1/PTW 数据流 + 外部对接）|
| 决策组 | [ADR-086](adr/ADR-086-l0-l1-tlb-architecture.md)（D01/D02/D03/D04/D05/D06/D07/D09/D10/D12 与 L1 相关部分）|
| 平台 | **Sv39**（需 64-bit XLEN，见 [总架构 §4](architecture.md)）|
| 范围 | **仅 CH_MEM 侧**（TLM 侧不动）|

> **L1 的定位**：MMU 翻译路径的**容量级**。L0 miss ⇒ 向 L1 发请求（级联）；L1 命中 ⇒ 2 拍出 `paddr` 并**回填 L0**；L1 miss ⇒ 交 PTW walk。

---

## 0. 待确认项

### Q-L1-1 —— `2M` 在 Sv39 下**合法**（✅ 前提已满足）

用户原始设计用 "2M = 大页 size"。**Sv39 的 2 MiB 页是合法的**（`VPN[1]` 级的 leaf，`page_shift = 21`）⇒ 之前的平台冲突（32-bit 平台只有 4 MiB 巨页）**因平台改为 Sv39 而消解**。

| 粒度 | 叶 PTE 所在级 | `page_shift` | VA 跨度 | 本期 |
|------|---------------|-------------:|--------|:----:|
| **4 KiB** | `VPN[0]`（叶表） | 12 | `vaddr[11:0]` | ✅ |
| **2 MiB** | `VPN[1]`（中间表） | 21 | `vaddr[20:0]` | ✅ |
| 1 GiB | `VPN[2]`（根表） | 30 | `vaddr[29:0]` | ❌ 本期不考虑（用户已定）|

### Q-L1-2 —— "per-entry 粒度" vs "per-way 粒度"（本组的口径裁决）

用户表述：**"each entry can be 2MiB or 4KiB"** ⇒ 字面是 **per-entry**；但同一句的 **"a line can be 16MiB or 32KiB"** 只有在 **per-way**（一条 line 内 8 个 entry 同粒度）时才成立。
⇒ **裁决：per-way 粒度**（保留 ADR-086 D06 的 B 方案，比较器 8 份掩码而非 64 份）。
⇒ 精确表述：**每个 way 固定一种粒度**；8 个 way 可分别选 2 MiB 或 4 KiB ⇒ 同一个 L1 内可**混合两种粒度**。

---

## 1. 职责与边界

| 项 | 内容 |
|----|------|
| **缓存什么** | **leaf 翻译** `{paddr, perms, page_shift}`（**多粒度**）|
| **不缓存什么** | 页目录项（PTWCache 的职责，ADR-085 D04）|
| **组织** | **256 set × 8 way 组相连** |
| **延迟** | **2 拍**（L0 判定 + L1 判定，级联）|
| **填充来源** | ① L1 自身命中（2 MiB / 4 KiB）② PTW walk 的 2 MiB 结果 |
| **粒度混合** | per-way 固定粒度；2 MiB way 与 4 KiB way 可共存 |

---

## 2. 目标结构

```
L1 = 256 sets × 8 ways

每 way:
  · 固定粒度 page_shift（2MiB=21 或 4KiB=12）
  · M 条 line，M = 16MiB / (8 entry × 2^page_shift)
        2MiB 粒度 ⇒ M = 16MiB / 16MiB = 1
        4KiB 粒度 ⇒ M = 16MiB / 32KiB = 512
  · 每 line = 8 entry × 12B = 96B

每 way 覆盖 VA 跨度 = 16MiB        ← set interleave 的基准
每 set 覆盖 = 8 × 16MiB = 128MiB
256 sets 的"名义"覆盖 = 32GiB；实际 VA 空间 = 2^39 = 512GiB ⇒ 需 XOR interleave 折叠
```

**三个参数的分离（ADR-086 D04）**：

| 参数 | 目标 | 含义 |
|------|-----:|------|
| `sets` | 256 | set 数（`2^8`）⇒ **决定 VA 折叠的桶数** |
| `ways` | 8 | 组相连度 ⇒ 同一 set 内候选 line 数 ⇒ **决定冲突率与替换逻辑成本** |
| `epl` | 8 | 每 line 的 entry 数 ⇒ **决定 line 宽度** |
| `cacheline_size` | 96 B | = `epl × entry_size` = 8 × 12B |
| `entry_size` | 12 B | 见 [L0 文档 §0 Q-L0-1](mmu-l0-tlb-architecture.md)（含 asid 预留）|
| `lines_per_way` (`M`) | 1（2MiB 粒度）/ 512（4KiB 粒度）| 由 `16MiB / (epl × 2^page_shift)` 导出 |

⚠ **`ways` 与 `epl` 正交**：`ways` 是"同一 set 里有几条候选 line"，`epl` 是"一条 line 里有几个 entry"。

---

## 3. set index 与 XOR interleave

**基准**：**16 MiB**（= 每 way 的 VA 覆盖）。

```
16MiB 块索引  blk = vaddr[38:24]        # 15 bit（Sv39 39-bit VA）
way    (3 bit) = blk[2:0]                # 块在 set 内的 8 个槽位
set    (8 bit) = blk[14:7] XOR {blk[6:3], 4'b0}
```

**参考实现**（可替换，但必须满足下面的 4 条性质）：

```c
uint8_t l1_set_index(uint64_t vaddr) {
    const uint16_t blk = (uint16_t)((vaddr >> 24) & 0x7FFF);   // vaddr[38:24]
    return (uint8_t)((blk >> 7) ^ ((blk >> 3) & 0x0F) << 4);
}
uint8_t l1_way_index(uint64_t vaddr) {
    return (uint8_t)((vaddr >> 24) & 0x07);                    // blk[2:0]
}
```

**规范性性质（4 条，实现必须满足）**：

| # | 性质 | 理由 |
|---|------|------|
| 1 | `set` / `way` 必须是 vaddr 的**纯函数** ⇒ **refill 与 lookup 必须同式** | 否则 refill 写错槽位（判据 `U-L1-12`）|
| 2 | 折叠必须**用满 15 bit**（`blk[14:0]` 全部参与）| 否则信息丢失 ⇒ 冲突集中 |
| 3 | 折叠必须**自反**（XOR 天然可逆）⇒ 同一 VA 恒定映射到同一槽位 | 幂等性 |
| 4 | 相邻 16MiB 块（`blk` 差 1）应落到**不同 set 或不同 way** | 避免同一大页被拆散到多个 set |

**容量核算**：

```
VA 空间       = 2^39 = 512GiB
16MiB 块数    = 2^39 / 2^24 = 2^15 = 32768
槽位数        = 256 sets × 8 ways = 2048
每槽位争用块数 = 32768 / 2048 = 16
```

⇒ **16 块争 1 槽位** ⇒ 组相连 + 替换策略确有必要 ✓（这正是 `ways=8` 的意义）。

**2 MiB 粒度 way 的一个细节**：一条 line 覆盖整个 16MiB 块（8 × 2MiB）⇒ **一个 way 只能缓存 16MiB 块内的 8 个 2MiB 翻译** ⇒ 而一个 16MiB 块内有 8 个 2MiB 页 ⇒ **恰好装满** ✓（无浪费）。
⇒ 而 2 MiB 以下的粒度（4 KiB）需要 `M=512` 条 line ⇒ 节点数 = 8 way × 512 line × 8 entry = 32768 entry/set ⇒ **本期不落地**（见 §6）。

---

## 4. 粒度与掩码比较

**per-way 掩码比较**（ADR-086 D06）：

```
每个 way 带一个 page_shift（3 bit）
mask_i = ~0 << page_shift_i
hit_i  = valid_i && (tag_i == (vaddr & mask_i)) && (asid/global 匹配)
命中项 = tag 命中的 way（多 way 命中时按 way 序号取第一个 ⇒ 规定小粒度优先）
```

**代价**：8 个 way ⇒ **8 份不同掩码** ⇒ 比较器面积 ×8（ADR-085 D14 §D14.1 已指出"比较器不能再直接比全 VPN"）。

**tag 宽度**：存全 VPN（27 bit，`vaddr[38:12]`）⇒ 掩码后比较 ⇒ 不同粒度 entry 可共用同一 tag 字段。

**`pfn` 粒度陷阱**（ADR-085 D14 §D14.1 第 4 条）：2 MiB entry 的 `pfn` = 2 MiB 页基址 PPN（**35 bit**），**不是** 4 KiB 粒度 PPN（44 bit）⇒ 存错 ⇒ `paddr` 静默左移 9 位 ⇒ 译出错误地址且无任何报错 ⇒ 判据 `U-L1-05` 专门锚这一点。

---

## 5. entry 字段

| 字段 | 宽度 | 说明 |
|------|-----:|------|
| `valid` | 1 | |
| `global` | 1 | RISC-V Global 位 |
| `perms` | 4 | R/W/X/U（A/D 位预留）|
| `tag` | 27 | `vaddr[38:12]`（Sv39 VPN），比较时按粒度掩码 |
| `pfn` | 44 | **按 entry 自身页粒度**存（4 KiB ⇒ 44 bit；2 MiB ⇒ 35 bit）|
| `asid` | 16 | Sv39 ASID（本期只预留）|
| `reserved` | 3 | 预留 per-entry `page_shift` 等 |
| **合计** | **96 bit = 12B** | ⇒ `cacheline_size = 96B`（8 × 12B）|

⚠ **与 ADR-085 D05 的 8B 是不同实体**：那是 **PTWCache 目录项** entry（`{level, parent_ppn, asid, next_ppn, perms}`），本表是 **leaf 翻译** entry ⇒ 字段集不同、缓存不同，**只是恰好宽度接近** ⇒ 代码注释必须区分。

---

## 6. 容量与节点预算

| 阶段 | `sets` | `ways` | `epl` | `M` | entry 数 | `ch_reg` 估算 | 何时 |
|------|-------:|-------:|------:|----:|---------:|---------------:|------|
| **P0（本期默认）** | 16 | 4 | 8 | 1 | **512** | ~512（每 entry 1 个 `ch_uint<96>`）| 立即 |
| P1 | 64 | 4 | 8 | 1 | 2 048 | ~2 048 | 抬升后 |
| **P2（架构目标）** | **256** | **8** | 8 | 1 | **16 384** | **~16 384** | 目标（不在本期）|

**缓解手段**：

| # | 手段 | 效果 |
|---|------|------|
| 1 | **entry 打包成 1 个 `ch_uint<96>`** | 每 entry 从 3 reg → **1 reg** ⇒ 节点 ÷3 |
| 2 | **本期只做 2 MiB 粒度 way**（`M=1`）| 4 KiB 粒度 way（`M=512`）的节点数是 2 MiB 的 **8 倍/set** ⇒ 本期不落地 |
| 3 | **分阶段落地**（P0/P1/P2）| 本期可控 |

⚠ **CH_MEM 全量测试时长**：基线 **833 s/轮** ⇒ P0（512 entry）预估 **+2~5%**；P2（16 384 entry）预估 **+30~80%** ⇒ P2 落地时需重估 CI 时限。

---

## 7. 决策映射

| 决策 | 与 L1 相关的部分 |
|------|-----------------|
| [D01](adr/ADR-086-D01-l0-l1-leaf-cache-split.md) | L1 = 256 set × 8 way / 多粒度 / 2 拍 |
| [D02](adr/ADR-086-D02-cascaded-lookup-latency-tiers.md) | L0 miss ⇒ 向 L1 发请求；L1 命中 2 拍 |
| [D03](adr/ADR-086-D03-l1-hit-backfill-l0.md) | L1 命中 ⇒ 回填 L0 |
| [D04](adr/ADR-086-D04-l1-organization-params.md) | 组织参数（`sets`/`ways`/`epl`/`cacheline_size`/`M`）|
| [D05](adr/ADR-086-D05-set-index-and-xor-interleave.md) | set index 与 XOR interleave（基准 16MiB）|
| [D06](adr/ADR-086-D06-multi-granularity-page-shift-mask.md) | 粒度归属 + per-way 掩码比较 |
| [D07](adr/ADR-086-D07-leaf-entry-fields-and-width.md) | leaf entry 字段与宽度 |
| [D09](adr/ADR-086-D09-coherence-and-invalidation.md) | L1 侧的失效路径（evict ⇒ 反向失效 L0）|
| [D10](adr/ADR-086-D10-capacity-and-node-budget.md) | 容量与节点预算 |
| [D12](adr/ADR-086-D12-verification-criteria.md) | `U-L1-03/05/06/11/12` |

---

## 8. 验证判据（L1 侧子集）

| # | 判据 |
|---|------|
| `U-L1-03` | L1 命中 ⇒ **2 拍**产出 `PADDR`（2 MiB 拼接正确：`paddr = (pfn << 21) \| (vaddr & 0x1FFFFF)`）|
| `U-L1-05` | **大页 `pfn` 粒度正确**：2 MiB entry 的 `pfn` = 35 bit 页基址 PPN，**不是** 44 bit 的 4 KiB PPN ⇒ 断言 `paddr` 与 4 KiB 路径**不同** |
| `U-L1-06` | **掩码比较**：4 KiB entry 与 2 MiB entry 覆盖同一 VA 时**不得**同时命中 ⇒ 恰好 1 个 way 命中 |
| `U-L1-11` | **容量与组织自检**：`sets`/`ways`/`epl`/`M` 配置生效；entry 总数 = `sets × ways × epl × M`；超容量写入触发逐出且不破坏其它 entry |
| `U-L1-12` | **set index 一致性**：同一 VA 在 **lookup** 与 **refill** 两条路径算出的 `(set, way)` **必须相同** |

**变异对照（L1 相关）**：② L1 命中不回填 L0 ⇒ `U-L1-04` RED；③ 大页 `pfn` 存 4 KiB 粒度 PPN ⇒ `U-L1-05` RED；④ set index 在 refill 与 lookup 用不同公式 ⇒ `U-L1-12` RED；⑦ per-way 掩码退化为全 VPN 直比 ⇒ `U-L1-06` RED。

---

## 9. 验证命令

```bash
# 架构门禁（5 个）
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh

# L1 结构性检查
grep -q "l1_tlb" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: 缺 L1 TLB 结构（D01/D04）"
grep -qE "sets|ways|epl" ip/mmu/configs/params_schema.json || echo "FAIL: L1 的 sets/ways/epl 未配置化（D04/D10）"
grep -q "page_shift" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: L1 entry 缺 page_shift（D06/D07）"

# 回归
./build/bin/chipforge_tests_chmem "[mmu-chmem-basic]"          # CH_MEM
bash tools/run_chipforge_tests.sh --all                        # 全量（CH_MEM ~833s/轮）
```
