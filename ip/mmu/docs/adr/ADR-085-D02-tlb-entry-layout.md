# ADR-085-D02 — TLBCache 的 entry 与 cacheline 组织

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D02`（父 ADR-085 的 D2） |
| 关联验证判据 | U1 |
| 受影响文件 | `ip/mmu/lib/tlb_entry.h`、`ip/mmu/plugins/MMUPlugin_chmem.h` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

TLB 需要缓存叶 PTE 的翻译结果并携带权限位。本决策定义 entry 字段集、cacheline 组织方式与 EPL 路并行比较形态。

## 决策正文

**决策**：

| 项 | 值 | 理由 |
|---|---|---|
| entry 字段 | `{valid, asid, tag, pfn, perms, page_shift, global}` | 复用 `lib/tlb_entry.h:49-61` 的 `TLBEntry`（F5：**`perms` 字段已存在**）⇒ struct 只需**扩 `page_shift`**。`perms` 的真实缺口在 **CH_MEM 插件侧的存储**（`MMUPlugin_chmem.h:291-293` 只有 valid/vaddr/paddr），需补存 |
| tag | VPN（页号），sv32 = vaddr[31:12] | 页内偏移不进 TLB |
| perms | R/W/X/U **4 位有效**；容器 `uint_t<8>`（与 `lib/tlb_entry.h:58` 一致），高 4 位**预留给 A/D**（[D13](ADR-085-D13-pte-encoding-and-permission.md)） | **RISC-V 要求权限检查**；现有 CH_MEM TLB **没有 perms**（`MMUPlugin_chmem.h:291-293` 只有 valid/vaddr/paddr）⇒ 必须补 |
| cacheline | 1 条 line = **EPL 个 entry**，EPL 可配；**per-mode 默认值见 [D17](ADR-085-D17-chmem-capacity-limit.md) 的表**（TLM 默认 8 / CH_MEM 默认 1） | 用户提案的 "cacheline 存多个 PTE" |
| 组织 | 现 8 项全相联 = **1 line × 8 ways**（degenerate case） | ⇒ 迁移平滑：现有实现已经是"cacheline 化的 TLB"，只是没有 line 的概念 |
| line 内比较 | **EPL 路 tag 比较并行** + OR 归约 | 硬件必然；与现有 `hit = hit \|\| hit_lane[i]` 形态一致（`MMUPlugin_chmem.h:247-250`） |

**并发/端口**：TLB 有 2 个查表端口（ifetch / loadstore），**共享同一存储体**（现有实现已如此，`MMUPlugin_chmem.h:191-207`）⇒ 组合上要 2 份比较逻辑 ⇒ 面积 ×2。这是已知代价，记录在 §7。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D01 — MMU 分解为 MMU_TLB + MMU_PTW（两个协作单元，同一 MMU 插件内）](ADR-085-D01-mmu-tlb-ptw-split.md)
  - [D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配](ADR-085-D05-ptwcache-entry-width-epl.md)
  - [D14 — Superpage（巨页）拼接（PTW 侧 + TLB 侧）](ADR-085-D14-superpage-two-sided.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
