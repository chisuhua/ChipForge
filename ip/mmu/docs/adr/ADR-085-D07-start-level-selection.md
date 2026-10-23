# ADR-085-D07 — PTWCache 命中 ⇒ `start_level` 选择（"从哪一级开始 walk"）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D07`（父 ADR-085 的 D7） |
| 关联验证判据 | U3 |
| 受影响文件 | `ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

这是用户提案的核心机制，也是修正项 M2。跨级地址串行依赖 ⇒ 物理上不可能「多级同时发访存」；可实现的形态是同拍串联 hit test + `start_level` 决定剩余访存次数。本决策给出物理约束证明、三种可实现并行与规范性选择算法。

## 决策正文

**这是用户提案的核心机制，也是本决策修正 M2 的地方。**

#### D7.1 物理约束（必须先说清）

sv32 的两级 walk：

```
pte1_addr = (pt_base.ppn << 12) + VPN[1] * 4      ← 恒可计算（pt_base 已知）
pte1      = mem[pte1_addr]
pte0_addr = (pte1.ppn  << 12) + VPN[0] * 4          ← 需要 pte1 先到
pte0      = mem[pte0_addr]
```

⇒ **`pte0_addr` 依赖 `pte1` 的内容** ⇒ **不同 level 的访存地址无法同时计算**。任何"多 level 同时发访存"的实现，除非投机，否则**物理上不可能**。

这是**串行依赖**，不是实现技巧问题。业界所有 RISC-V 核的 PTW 都是串行的（BOOM/Rocket/CVA6 皆然），优化手段是"**减少串行步数**"而不是"并行发出"。

#### D7.2 可实现的三种并行

| # | 并行 | 范围 | 状态 |
|---|------|------|------|
| **P1** | **cacheline 内 EPL 路 tag 比较并行** | 硬件必然，零成本 | ✅ 采纳 |
| **P2** | **同一拍内串联多级 hit test**（`test(k)` 命中 ⇒ `next_ppn` 是组合值 ⇒ `test(k-1)` 的地址当拍可算） | **只在"连续命中前缀"内成立**：一旦某级 miss，必须等物理读回来（`sread` 1 拍），其后的 test 落在后续拍 | ✅ 采纳（这是"从哪一级开始 walk"的正确形式） |
| **P3** | **投机并行**（用历史 base 预测深层 level 的地址，同时发出多级访存） | 需 base predictor + 取消逻辑 | ❌ 不采纳（复杂度 vs 收益，见 §6.2） |

#### D7.3 `start_level` 的定义与选择算法（规范性）

**level 编号（sv32）**：`L1` = 根页表（基址 = `pt_base`，由 `VPN[1]` 索引）；`L0` = 叶表（由 `VPN[0]` 索引，其**表项即叶 PTE**）。

> 注：sv32 **只有 L1 / L0 两级**。rev.1 曾写"resume level 2（最深）"，在 sv32 下不存在该级 —— rev.2 已改为下述 `start_level` 表述。

**`start_level`（派生量，不是状态）**：walk 过程中**第一次需要发物理读**的那一级。

**算法**（记当前级为 `k`，其表基址已知）：

1. **同拍**做 `test(k)` = PTWCache 查找 `(level=k, parent_ppn=base(k), index=VPN[k])`。
2. `test(k)` **命中** ⇒ 取命中项的 `next_ppn` ⇒ **不发生访存**；若 `k > 0`，`base(k-1)` 当拍已知 ⇒ **同一拍继续第 1 步（`k ← k-1`）**；若 `k == 0`，命中项即叶 ⇒ 直接完成。
3. `test(k)` **未命中** ⇒ 若这是**首次** miss，则 `start_level := k`；发物理读（`sread`）；数据返回后进入 `k-1` 的第 1 步。
4. 循环至叶或 fault。

⇒ **`start_level` = 连续命中前缀之后的那一级** ⇒ **剩余访存次数 = `start_level` + 1**（sv32 下 ∈ {1, 2}）。

**举例（sv32，按 [D4](ADR-085-D04-ptwcache-no-leaf-pte.md)「叶不入 PTWCache」）**：

| 情形 | test 结果 | `start_level` | 访存次数 |
|------|-----------|---------------|---------|
| 冷（PTWCache 空） | `test(L1)` = miss（首次） | **L1** | **2**（L1 目录项 + L0 叶 PTE） |
| L1 目录项已缓存 | `test(L1)` = hit ⇒ 当拍 `test(L0)` 必 miss（叶不入，见 D7.4） | **L0** | **1**（只读叶 PTE） |
| 叶也在 PTWCache（**仅 Q1 改判时**） | `test(L1)`=hit, `test(L0)`=hit | 无 | 0 |

#### D7.4 与"叶 PTE 不入 PTWCache"（[D4](ADR-085-D04-ptwcache-no-leaf-pte.md)）的交互 —— **sv32 下 P2 的并行度为零**

叶 PTE 不在 PTWCache ⇒ **`test(L0)` 恒 miss**。因此在 sv32 下：

1. **连续命中前缀最长只有 1 级（L1）** ⇒ P2 的"同拍串联"**在 sv32 上没有第二个 test 可以串** ⇒ **多级同时 test 机制在 sv32 上并行度为 0**。
2. **PTWCache 的唯一收益 = 每次 walk miss 省 1 次访存**（2 → 1）。
3. 对 sv39/sv48（3~4 级）才出现真实的连续命中前缀（可省 1~2 次）⇒ P2 的价值在那些 mode 上才兑现。

⇒ **架构结论（本决策明确记录）**：**PTWCache 在 sv32（当前唯一支持的 mode）上是"可选优化"，不是 walk 正确性的必要条件**。因此：

- **P0~P3 的实装顺序不允许被 PTWCache 阻塞**：先做对 walk（[D13](ADR-085-D13-pte-encoding-and-permission.md) 编码 / [D14](ADR-085-D14-superpage-two-sided.md) superpage / [D15](ADR-085-D15-walk-refill-tlb-only.md) 回填 / [D16](ADR-085-D16-ptw-physical-read-port.md) 物理读端口），PTWCache **作为独立可配置项，默认关闭**（`ptw_cache_entries = 0` ⇒ 直接走内存）。
- 开启时必须满足 [D19](ADR-085-D19-verification-criteria.md) 的 U3 判据（"L1 命中 ⇒ 只读 1 次"）⇒ 收益是可测的，不是估算的。
- 若将来实装 sv39/48，再把 PTWCache 默认打开（那时 Q1 也应重新评估，见 §6.3）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D04 — PTWCache 只缓存**非叶**目录项（PDE），不缓存叶 PTE](ADR-085-D04-ptwcache-no-leaf-pte.md)
  - [D05 — PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配](ADR-085-D05-ptwcache-entry-width-epl.md)
  - [D16 — PTW 的物理内存读端口（绕过翻译）](ADR-085-D16-ptw-physical-read-port.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
