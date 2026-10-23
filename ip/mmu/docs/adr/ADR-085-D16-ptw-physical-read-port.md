# ADR-085-D16 — PTW 的物理内存读端口（绕过翻译）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D16`（父 ADR-085 的 D16） |
| 关联验证判据 | U3 |
| 受影响文件 | `ip/cpu/plugins/dmem_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

PTW 读的是物理地址 ⇒ 读端口必须绕过 MMU 翻译（防递归）。现状 `DMemPlugin::mem_` 是 private 且惰性创建，MMU 的 `("fetch", EARLY)` 闭包拿不到句柄 ⇒ 需要跨模块 API 变更。

## 决策正文

**决策**：PTW 读的是**物理地址**，其读端口**不经过 MMU 翻译**（防递归）。

- PTW 的读地址 = `pt_base` / `next_ppn` 派生的**物理**地址 ⇒ 直接进物理内存端口。
- CH_MEM 侧：需要一个**绑定到物理内存的 `ch_mem` 句柄**。
  - **现状障碍**：`DMemPlugin::mem_` 是 private（`dmem_chmem.h:72`），且 `get_mem()` 惰性创建（在 `("memory", NORMAL)` 闭包里）；MMU 的 `("fetch", EARLY)` 闭包**早于**它执行 ⇒ MMU 在自己的闭包里拿不到句柄。
  - ⇒ **需要跨模块 API 变更**：`DMemPlugin` 暴露 `ch_mem& mem()`（在 `setup()` 里提前创建；调用方持有 `ctx_swap` ⇒ `ch_mem` ctor 的"context 必须活跃"要求可满足，见 `tests/cpu/test_cpu_chmem_vendored_elf.cpp:135`）。
  - ⇒ **本项不在当前 `chmem-ptw-walk` 的 3a~3g 清单里**（protocol §6.2 / tasks.md §"Phase B 推迟项"）⇒ **必须补进 change 的受影响文件**，否则实现者会在 `("fetch", EARLY)` 拿到空句柄。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D17 — CH_MEM 容量上限（elaboration 节点爆炸）](ADR-085-D17-chmem-capacity-limit.md)
  - [D21 — 跨插件物理内存句柄的所有权（rev.2 新增，配合 D16）](ADR-085-D21-physical-mem-handle-ownership.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
