# ADR-085-D21 — 跨插件物理内存句柄的所有权（rev.2 新增，配合 D16）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D21`（父 ADR-085 的 D21） |
| 关联验证判据 | U3 |
| 受影响文件 | `ip/cpu/plugins/dmem_chmem.h`、`ip/mmu/plugins/mmu_ptw_chmem.h`（新） |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

D16 需要 MMU 持有物理内存的 `ch_mem` 句柄。本决策定所有权归 `DMemPlugin`（创建者）、MMU 只持非拥有引用，且句柄必须在 `setup()` 阶段取（不得在 at_stage 惰性获取）。

## 决策正文

**决策**：`ch_mem` 的所有权归 **`DMemPlugin`**（创建者）；MMU_PTW 只持**非拥有引用**（`ch_mem*`），生命周期由 `PipeBuilder` 保证 —— 两者同属一个 `pb`，`pb` 析构时插件一起销毁，**MMU 不得比 DMem 活得久**。

**规则**：
- 句柄在 **`setup()` 阶段统一获取并缓存**（`ctx_swap` 活跃，满足 `ch_mem` ctor 的"context 必须活跃"要求），**不得在 `at_stage` 闭包里惰性获取**（`("fetch", EARLY)` 早于 DMem 的 `("memory", NORMAL)` ⇒ 拿到空句柄，见 [D16](ADR-085-D16-ptw-physical-read-port.md)）。
- **禁止 MMU 拥有/析构该 `ch_mem`**（`unique_ptr` 不得跨插件转移）。
- 若将来允许多个物理内存（imem/dmem 分离，见 §7 R7），句柄改为**列表/双端口**，所有权规则不变。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D16 — PTW 的物理内存读端口（绕过翻译）](ADR-085-D16-ptw-physical-read-port.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
