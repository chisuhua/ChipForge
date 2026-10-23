# ADR-086-D07 — leaf entry 的字段与 8B 宽度

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md)（D05 固定 8B entry） |
| 架构文档 | [mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D07` |
| 关联验证判据 | U-L1-05、U-L1-11 |
| 受影响文件 | `ip/mmu/lib/tlb_entry.h`、`ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

用户的容量公式里出现 `/8B` ⇒ 沿用 ADR-085 D05 的「entry 固定 8B」口径。但 D05 的 8B 是给 **PTWCache 目录项** 定的（`{level, parent_ppn, asid, next_ppn, perms}`），**不是** leaf 翻译 entry —— 两者字段集不同，必须显式区分。

## 决策正文

**决策**：leaf entry（L0 / L1 共用同一结构）字段集：

| 字段 | 宽度 | 说明 |
|------|-----:|------|
| `valid` | 1 | 有效位 |
| `tag` | 20 | **4KiB 粒度的 VPN**（`vaddr[31:12]`）；比较时按粒度掩码（D06）⇒ 存全 VPN 可让**不同粒度共用同一 tag 字段**（掩码后比较） |
| `pfn` | 22 | 物理页号，**按该 entry 自身页粒度**存（ADR-085 D14 §D14.1 第 4 条的坑：`pfn` 必须 = `paddr_base >> page_shift`，不能存 4KiB 粒度 PPN 再配大 `page_shift`） |
| `perms` | 4 | R/W/X/U（ADR-085 D13 口径；A/D 预留位见 D07.1） |
| **合计** | **47** | 1 + 20 + 22 + 4 = 47 bit ⇒ **8B（64 bit）够，且余 17 bit** |

**D07.1 余量用途（规范性）**：余下的 17 bit 分配给：
- `asid`：**位宽预留**（当前 `asid_bits=0` ⇒ 分配 0 bit，但**结构上保留 9 bit** 供 ASID 支持，ADR-085 D05 同样的"位宽现在不定死"纪律）
- `global`：1 bit（RISC-V Global 位）
- `page_shift`：**per-way**（D06）⇒ **不进 entry** ⇒ 但为将来"per-entry 粒度"预留 3 bit
⇒ 分配：`asid` 9 + `global` 1 + `reserved` 3 = 13 bit ⇒ 47 + 13 = 60 bit ⇒ 8B 内仍余 4 bit。

**D07.2 与 ADR-085 D05 的 8B 固定宽度是不同实体（规范性）**：

| | ADR-085 D05 的 8B | 本决策的 8B |
|---|---|---|
| 实体 | **PTWCache 目录项** entry | **leaf 翻译** entry |
| 字段 | `{level, parent_ppn, asid, next_ppn, perms}` | `{valid, tag, pfn, perms, asid, global, reserved}` |
| 缓存内容 | 页**目录项** | 页**翻译结果** |
| 所属 | ADR-085 的 `PTWCache` | 本组的 `L0 TLB` / `L1 TLB` |

⇒ 两者**不共享结构**、**不共享缓存**，但**恰好都是 8B** ⇒ 实现时可用同一个 `ch_uint<64>` cell 容器，但**字段语义完全不同** ⇒ 文档与代码注释必须区分（防止"8B 就一样"的错误合并）。

**D07.3 `pfn` 的粒度坑（复述 ADR-085 D14 的教训）**：4MiB entry 的 `pfn` = 4MiB 页基址 PPN（12 bit），**不是** 4KiB 粒度的 PPN（22 bit）。存错 ⇒ `paddr` 静默左移 10 位 ⇒ 译出错误地址且无任何报错。

## 交叉引用

- 架构文档：[mmu-l0-l1-tlb-architecture.md](../mmu-l0-l1-tlb-architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D05 8B 固定宽度、D14 §D14.1 `pfn` 粒度）
- 相关决策：[D04](ADR-086-D04-l1-organization-params.md)、[D06](ADR-086-D06-multi-granularity-page-shift-mask.md)、[D08](ADR-086-D08-l0-organization.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)
