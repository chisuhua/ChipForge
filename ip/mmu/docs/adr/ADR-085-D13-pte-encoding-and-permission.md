# ADR-085-D13 — 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 父 ADR | [ADR-085 — MMU_PTW 架构（伞索引）](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [mmu-ptw-architecture.md](../mmu-ptw-architecture.md) |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策编号 | `D13`（父 ADR-085 的 D13） |
| 关联验证判据 | U4、U5 |
| 受影响文件 | `ip/mmu/lib/pte_decode.h`（新）、`ip/mmu/plugins/mmu_ptw_chmem.h`、`.rddf/protocols/fetch_stall_protocol_v0.11.0.md`、`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` |

> **引用约定**：本文件沿用父 ADR-085 的章节编号。`§0`–`§11` 指 [架构文档 `mmu-ptw-architecture.md`](../mmu-ptw-architecture.md) 的对应章节（其中 `§3` 是决策索引）；`§D7.x` / `§D14.1` 指**编号对应子 ADR 内**的子节（`§D7.x` 在 D07 文件里、`§D14.1` 在 D14 文件里）。**其它文档**的章节号保持原样并自带文档名（如 RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

## 上下文

**现存真实缺陷**：`mmu_ptw_chmem.h:251` 的 `is_reserved = r && w` 会把合法 RW 叶判成页错误（TLM 侧 `lib/ptw.cpp:79` 本来就对）。本决策以 spec §5.3.2 翻译算法原文为唯一权威，抽出 `lib/pte_decode.h` 单点实现，并要求 protocol 升 rev.15。

## 决策正文

**决策**：严格按 **RISC-V Privileged Spec 的地址翻译算法**（§5.3.2 Sv39 算法，§12.3.2 Sv32 同构）实现，**不自己编编码表**。

| step | spec 原文条件 | 判定式 | 动作 |
|:----:|--------------|--------|------|
| 2 | `pte = mem[a + vpn[i] × PTESIZE]` | — | 发物理读 |
| 3a | `pte.v == 0` | `!v` | **页错误** |
| 3b | `pte.r == 0 && pte.w == 1` | `!r && w` | **页错误**（reserved 编码） |
| 3c | 保留位/保留编码被置位 | 非叶 PTE 的 `D`/`A`/`U` MUST 为 0（v1.12：非叶 PTE 的 D/A/U 保留） | **页错误** |
| 4a | `pte.r == 1 \|\| pte.x == 1` | `r \|\| x` | **叶** ⇒ step 5 |
| 4b | 否则 | `!r && !x` | **指针** ⇒ `i ← i−1`；`i < 0` ⇒ **页错误** |
| 5 | 叶权限检查 | `R/W/X` vs `access_type`；`U` vs 特权级；`MXR` | 不满足 ⇒ **页错误** |

> ⚠ **实现顺序**：3c 的"非叶"判定依赖 4a/4b 的分类结果 ⇒ 实现上必须**先按 `r || x` 分出叶/指针，再对指针套用 3c**。反过来会把**叶 PTE 的合法 D/A/U**（`A=1` 是很常见的状态）误判成 reserved ⇒ 全体页 fault。

⇒ **`is_reserved = !r && w`**（**唯一**保留编码判据；`v=0` 已由 3a 先行 fault，故无需前置 `v` —— 与 3b 行的判定式保持字面一致）。由此：

| 编码 | 语义 |
|------|------|
| `V=1, R=1, W=0, X=0` | **合法只读叶**（read-only page；`.rodata` 常态） |
| `V=1, R=1, W=1, X=0` | **合法 RW 叶** |
| `V=1, R=0, W=0, X=1` | **合法 X-only 叶**（`MXR` 位只对叶生效，其存在即证明 X-only 合法） |
| `V=1, R=0, W=1, X=*` | **reserved** ⇒ 页错误 |
| `V=1, R=0, W=0, X=0` | 指针（非叶） |

**两个真实缺陷（分属两侧，方向相反）**：

| 位置 | 现状 | 与 spec 的偏差 | 裁决 |
|------|------|---------------|------|
| `ip/cpu/plugins/mmu_ptw_chmem.h:251` | `is_reserved = r_sig && w_sig` | **把合法 RW 叶判为 fault** ⇒ 所有可写页无法翻译 | 改 `!r && w` |
| 同上 `:245-247` 注释 | 声称"任务规范: reserved = R=1 && W=1" | **注释把错误编码制度化** | 与代码同 commit 修 |
| `ip/mmu/lib/ptw.cpp:79-80` | `pte.w && !pte.r` | **与 spec 一致** | **不改** |

> ⚠ **rev.1 的错误已撤回**：rev.1 称"TLM `w && !r` 漏掉 `(R=1,W=0,X=0)` reserved ⇒ 两侧都错"。**错的** —— `(R=1,W=0,X=0)` 不是保留编码。`lib/ptw.cpp:79-80` 的注释还记录了**同款 `r && w` bug 曾被当 "Oracle C1" 修掉** ⇒ **CH_MEM 侧是同一 bug 复发**，不是新缺陷。

**⇒ `lib/` 共享 helper 从"整洁性"升级为"防复发"**（同一判据在 TLM 实现 / CH_MEM 实现 / 两处注释 / protocol 测试向量 4 处出现，已漂移过一次）：

1. **判据只允许一份实现**：`ip/mmu/lib/pte_decode.h` 提供 `classify_pte() -> {invalid, reserved, leaf, pointer}` + `check_leaf_perms(pte, access_type, priv, mxr) -> fault|ok`，纯 C++、可单测。TLM 与 CH_MEM 都调它（CH_MEM 侧在 elaboration 期展开为组合函数 ⇒ 允许）。
2. `mmu_ptw_chmem.h` 的 `is_reserved` 改 `!r && w`，注释同改。
3. **`fetch_stall_protocol_v0.11.0.md` 的 AC-FS-15 必须升 rev.15 + 走 critic** —— 它当前（`protocol:577`）把 `V=1 && R=1 && W=1` 钉为"权威坏 PTE"并注明"以代码为准"⇒ **protocol 正在把 bug 变成验收判据**。rev.15 需改三处：
   - 坏 PTE ② 编码 → **`V=1 && R=0 && W=1`**；
   - 删"引用时以代码为准"的 pin（改指 `lib/pte_decode.h`）；
   - **新增正向判别力**：`V=1,R=1,W=1,X=0` 是合法叶，MUST 翻译成功 —— 否则"把 RW 叶判 fault"的实现**更容易通过**负向门（它对任何 PTE 都 fault ⇒ `PTW_ACTIVE` 恒 1 ⇒ 负向门全绿）。
4. `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 的 `mmu_ptw_fsm_sv32_reserved_encoding_fault`（Test 2）**必须改**：现在喂 `{V=1,R=1,W=1,X=0}` 断言 `fault_code==2`，该编码是合法叶 ⇒ 改喂 `{V=1,R=0,W=1,X=0}`；并**新增一条正例**（`{V=1,R=1,W=1,X=0}` ⇒ DONE）作回归锚。

**A/D 位（accessed / dirty）**：
- 规范行为：**未实现 `Svade` 扩展时**，硬件遇叶 `A=0`（或 store 且 `D=0`）MUST 置位并**写回 PTE**；实现了 `Svade` 则改为抛页错误。
- 两者本项目**均未实现**（TLM 侧同样）⇒ **本期 out of scope**，记为已知限制。
- 但 **TLB entry 的 `perms` 字段（`uint_t<8>`）必须预留 A/D 位** ⇒ 将来实装 A/D 不需要改 entry 格式。

**权限检查的粒度**：**每一级**都要做 step 3c 的保留位检查；step 5 的 `R/W/X` 叶权限检查只对叶做（非叶无权限语义）。

## 交叉引用

- 父 ADR：[ADR-085](ADR-085-mmu-ptw-architecture.md)（伞索引，登记 D01–D21 共 21 条子决策）
- 架构文档：[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)（含数据流与时序、替代方案、后果、实施清单、验证命令）
- 相关决策：
  - [D02 — TLBCache 的 entry 与 cacheline 组织](ADR-085-D02-tlb-entry-layout.md)
  - [D19 — 验证要求（U1–U11 + D19.1 fault 两层语义）](ADR-085-D19-verification-criteria.md)
- 验证判据：见 [D19 — 验证要求](ADR-085-D19-verification-criteria.md)
