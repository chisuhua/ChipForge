# ADR-085-D14 — Superpage（巨页）拼接（PTW 侧 + TLB 侧）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D14`（父 ADR-085 的 D14） |
| 关联验证判据 | U6、U11 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`、`ip/mmu/lib/tlb_entry.h`、`ip/mmu/lib/ptw.cpp` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

**两侧都缺**：CH_MEM 侧是「直传 PPN 不拼接」，TLM 侧是「按最后一层判叶」；且 TLB 的 tag/paddr 合成按 4KiB 写死 ⇒ 只补 PTW 侧会让 4MiB 页的其余子页全错。本决策定参数化 shift（sv32 = 22）与 TLB entry 的 `page_shift` 掩码比较。

## 决策正文

**决策**：叶 PTE 出现在第 `k` 级时，物理地址按 **VPN 位宽参数化**拼接：

```
pteshift = 12 + VPN_BITS * k                 # k = 叶的级数（0 = 4KiB 页）
paddr    = (leaf_ppn_effective << pteshift) | (vaddr & ((1 << pteshift) - 1))

VPN_BITS = 10 (sv32) | 9 (sv39 / sv48 / sv57)
```

| mode | `VPN_BITS` | `k` | 页大小 | `pteshift` | 使用的 PPN 字段 |
|------|-----------:|---:|--------|-----------:|----------------|
| sv32 | 10 | 0 | 4 KiB | 12 | `PPN[1:0]`（22 bit） |
| **sv32** | **10** | **1** | **4 MiB** | **22** | **仅 `PPN[1]`（12 bit）** |
| sv39 | 9 | 1 | 2 MiB | 21 | `PPN[2:1]` |
| sv39 | 9 | 2 | 1 GiB | 30 | 仅 `PPN[2]` |

⇒ **sv32 巨页必须钉死为 `paddr = (PPN[1] << 22) | (vaddr & 0x3FFFFF)`**。

> ⚠ **已知限制（平台假设，非本决策引入）**：sv32 的物理地址实为 **34 位**（`PPN[1]` 12 bit + 22 = 34），而全仓（含 `lib/ptw.cpp:91` 与 CH_MEM 的 `ch_uint<32>` 通路）均按 **32 位 `paddr`** 建模 ⇒ `PA[33:32]` 会被**静默截断**。⇒ 平台假设 **物理内存 < 4 GiB**；打破该假设需把 `paddr` 通路整体升到 34/64 位（独立 change，不在本期）。

> ⚠ **rev.1 的 `<< 21` 是错的（已修正）**：`shift = 12 + 9k` 只对 sv39/48 成立；sv32 的 VPN 是 **10 位**（`VPN[1]=va[31:22]`、`VPN[0]=va[21:12]`）⇒ `pteshift = 12 + 10k`，k=1 时 **22**。写成 21 会让 4MiB 页的物理地址**整体错位 1 bit 且静默**（顶层半页映射到相邻页）。

**必须补（两侧都缺）**：
- **CH_MEM**：`mmu_ptw_chmem.h:82-83` 的注释明写 "megapage (level-1 叶, 4MB) 的 ppn 拼接简化: PoC 直接透传 pte_ppn, 不拼接 vaddr 剩余位 (**Phase 6d.6 完整版补**)" ⇒ **本期必须实装**。
- **TLM**：`lib/ptw.cpp:91-94` 用 `current_level_ + 1 >= max_levels_` 判定"到达叶" ⇒ **非最终级的叶 PTE 被当作指针继续走**（同一 func 的 `result_paddr_ = (pte.ppn << 12) | (vaddr & 0xFFF)` 恒按 4KiB 拼）⇒ **TLM 侧同样不支持 superpage**。本期至少要与 CH_MEM 侧**同判据**（[D13](ADR-085-D13-pte-encoding-and-permission.md) 的 step 4a 是"叶"的唯一判据，与"是否最后一层"无关），superpage 的完整支持登记为 follow-up。

#### D14.1 TLBCache 侧同样必须带 page size（否则 superpage 只对了一半）

PTW 拼对了 `paddr` 还不够：**翻译结果要经 TLB 缓存**，而 TLB 的命中/地址合成是按 4KiB 写死的（`paddr = (pfn << 12) | (vaddr & 0xFFF)`，`MMUPlugin_chmem.h:263-272`）。若把一个 4MiB 叶的结果按 4KiB 缓存：

- **tag 按全 `VPN` 比较** ⇒ 该 entry 只能服务 1 个 4KiB 子页 ⇒ 4MiB 映射要 1024 条 ⇒ **TLB 被冲爆**；
- **`paddr` 按 4KiB 合成** ⇒ 其余子页全错。

⇒ **决策（[D2](ADR-085-D02-tlb-entry-layout.md) 的补充，规范性）**：

1. TLB entry **必须增加 `page_shift` 字段**（sv32 取值 `{12, 22}`，2 bit 够；预留 sv39 的 `{12,21,30}` 需 3 bit ⇒ **定 3 bit**）。
2. **tag 比较用掩码**：`match = (entry.tag == (vaddr >> entry.page_shift))`，即只比较 `vaddr[31:page_shift]`。**asid / global 分量照旧参与**（[D2](ADR-085-D02-tlb-entry-layout.md)/[D5](ADR-085-D05-ptwcache-entry-width-epl.md) 要求），此处只示地址分量的掩码化。
3. **`paddr` 合成参数化**：`paddr = (entry.pfn << entry.page_shift) | (vaddr & ((1 << entry.page_shift) - 1))`。
4. **`entry.pfn` 的粒度必须是"该 entry 自己的页粒度"**，即 `entry.pfn = paddr_base >> entry.page_shift`：
   - 4KiB entry：`pfn = PPN[1:0]`（22 bit，与现状一致）；
   - 4MiB entry：`pfn = PPN[1]`（12 bit）。
   ⚠ **不能存 4KiB 粒度的 PPN 再配 `page_shift=22`**：那会丢掉 `vaddr[21:12]` 的透传（`(PPN[1]<<10) << 22` 把地址整体左移 10 位），译出**静默错误**的 `paddr`。

⇒ §4.1 的 `paddr` 公式据此改为参数化形式（本决策已同步）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D02 — TLBCache 的 entry 与 cacheline 组织](ADR-085-D02-tlb-entry-layout.md)
  - [D13 — 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**）](ADR-085-D13-pte-encoding-and-permission.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
