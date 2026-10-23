# ADR-085-D18 — 文件组织与双文件分离（ADR-083 / ADR-040）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D18`（父 ADR-085 的 D18） |
| 关联验证判据 | U5 |
| 受影响文件 | `ip/mmu/plugins/`、`ip/mmu/lib/` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

PTW 代码现错放在 `ip/cpu/plugins/`（ADR-083 目录违规 + 「CPU 目录里放 MMU 组件」的归属真空，是 `chmem-ptw-walk` 反复 defer 的根因之一）。本决策定落位与「PTE 判据只允许一份实现」的分层纪律。

## 决策正文

**决策**：

```
ip/mmu/
├── plugins/                              ← ADR-083 §2.4 + CI 可见性（check_plugin_portability Check 2 只扫 plugins/）
│   ├── MMUPlugin_chmem.h                 ← 现有（TLB-only），保留为 MMU_TLB 主体
│   ├── mmu_ptw_chmem.h                   ← 新：MMU_PTW（PTWCache + walk FSM）
│   └── ...
├── tlm/
│   ├── MMUPlugin.{h,cpp}                 ← TLM 侧，不动（本期不改 TLM）
│   └── mmu_keys.h
└── lib/
    ├── ptw.h / ptw.cpp                   ← TLM 纯 C++ PTW，保留
    ├── tlb_entry.h                       ← TLBEntry（复用）
    └── pte_decode.h                      ← 新：PTE 解码 + 编码分类 + 权限检查（共享，见 D13）
```

**规则**：
- **PTE 解码 / 编码分类 / 权限判定**（[D13](ADR-085-D13-pte-encoding-and-permission.md)）**只允许有一份实现**，放 `lib/`（纯 C++、可单测、CH_MEM 可复用）⇒ 消除 TLM 与 CH_MEM 的语义漂移（这正是 [D13](ADR-085-D13-pte-encoding-and-permission.md) 发现问题的根因）。
- `mmu_ptw_chmem.h` 从 `ip/cpu/plugins/mmu_ptw_chmem.h` **迁移**过来（修正 ADR-083 目录违规 + 消除"CPU 目录里放 MMU 组件"的归属真空，这是 `chmem-ptw-walk` 反复 defer 的根因之一）。
- CH_MEM 新文件**必须**落 `plugins/`（不是 `tlm/`）—— `tools/check_plugin_portability.sh` 的 Check 2/7 **只扫 `plugins/`** ⇒ 落 `tlm/` 会逃出 CI 门禁（improvement draft `mmu-chmem-phase-b-c-launch.md:37-44` 已记录该盲区）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D13 — 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**）](ADR-085-D13-pte-encoding-and-permission.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
