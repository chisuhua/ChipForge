---
change: backend-parity-corpus
priority: P1
initiative: wave5-isa-coverage-and-bp
adr:
  - ADR-089
  - ADR-088
plan: docs/architecture/parity-gate-contract.md
status: in-progress
depends_on:
  - parity-gate-spec
  - backend-parity-observability
  - backend-parity-harness
---

# tasks — backend-parity-corpus（阶段 A3）

> 每条任务末尾 `[S-x]` = 它落地的契约单元。

## 1. 语料接入（✅ 已落）

- [x] 1.1 `rv32ui-p-simple` 补进 Simulator 侧 `kCases`（1 行）⇒ 两侧 48 == 48 [S-13, ADR-089 D4]
- [x] 1.2 清单扩到 **52 case = 48 ELF + 4 branchbench**（`layer` 列 + PR 层 8 条）[S-13, S-15]
- [x] 1.3 新增 case 端到端 PASS（`rv32ui-p-simple` ⇒ 两侧 5 观测量逐位相等）
- [x] 1.4 语料集合断言双向干净（清单 ⊆ 两侧 ∧ 两侧 ⊆ 清单 ⇒ 零 WARNING）[S-13]
- [x] 1.5 `[vendored-elf]` 从 47 → 48 case，新增 case MUST PASS（零回归）

## 2. 门禁增强（✅ 已落；★ 3 个真缺陷）

- [x] 2.1 ★ **partial 语义**（S-9a）：两侧 `done=false` ⇒ `tohost`/`cycles` 标 vacuous；承重判据 = `regs`/`memsig`/`pchash` [S-9]
- [x] 2.2 ★ `completion` 不一致 ⇒ **FAIL** / `completion_mismatch`（真分歧，不是 ERROR）；`done` 缺失 ⇒ ERROR [S-9, S-10]
- [x] 2.3 ★ **per-case cycle cap**：清单第 5 列 `max_cycles` + 按 `kind` 默认（elf=3000 / branchbench=20000）[S-15]
- [x] 2.4 ★ **Verilator work-dir/verilog per-case 隔离**（修并行覆盖 `top.v` 的静默错测）[S-14]
- [x] 2.5 ★ 绝对路径修复（`cpu_verilator_sim` 内部 `cd work-dir` ⇒ 相对路径失效）[S-14]
- [x] 2.6 **并行执行** `--jobs`（默认 4）+ 报告按清单顺序重排 [S-15]
- [x] 2.7 **pre-flight**：清单引用的 ELF MUST 存在（branchbench 4 个是构建产物 ⇒ 不在版本控制内）[S-13]
- [x] 2.8 `--self-test` **13/13**（含 partial 四情形）[S-9, S-10]
- [x] 2.9 PR 层 **8/8 PASS**（实测 6.5 min，`jobs=4`）

## 3. 契约更新（✅ 已落）

- [x] 3.1 ★ **契约 v1.4 → v1.5**：新增 **S-9a partial 语义** [S-9]
- [x] 3.2 S-13 清单格式加第 5 列 `max_cycles` + 归属更新（A3 落「扩到 52 + 首跑」）[S-13]
- [x] 3.3 S-15 per-case cap + ★ 预算实测更正（PR ≈6.5 min / 夜间 52 case ≈45~60 min，`jobs=4`）[S-15]
- [x] 3.4 ★ S-15 branchbench 条款更正：**cap 截断 ⇒ partial**（不是「仅 Verilator 侧可跑」的豁免 —— 两侧都能跑 partial）[S-9, S-15]

## 4. ★ 首跑 + 分歧清单（本 change 核心）

- [x] 4.1 52 case 全量差分跑（`--layer night`，`jobs=4`）⇒ 报告落盘 `build/parity/report-night.json`
- [x] 4.2 ★ **产出既存后端分歧清单** —— ★ **结果：清单为空（52/52 PASS，零后端分歧）**
      ⇒ 「既存后端分歧」这一**假设被实测证伪**：两后端在 48 ELF + 4 branchbench 上
      **5 项观测量逐位相等**（含 `pchash` 逐拍 PC 流 hash）
- [x] 4.3 ★ 每条分类 —— **N/A（无分歧条目 ⇒ 无需分类）**；★ 但登记了 **2 项「未覆盖」**（见下）
- [x] 4.4 处置登记 —— **N/A（无分歧 ⇒ 无白名单登记、无上游 issue、无修复 change）**
      ⇒ ★ 真实例外数保持 **0**（`exceptions.json` 只有常驻合成例外）
- [x] 4.5 契约 §7 覆盖矩阵补 A3 行 + roadmap（A3 勾选 + G-A3 输入）

### ★ 首跑的 2 项**未覆盖**（诚实标注 —— MUST NOT 假装已覆盖）

| # | 未覆盖项 | 原因 | 影响 | 归属 |
|---|---|---|---|---|
| **U-1** | branchbench 4 档**只跑到 20k 拍**（未跑到 done） | 完整需 ~86k 拍 ⇒ Simulator 侧 9~10 拍/s ⇒ **61 h/档**，不可行 | 该 4 档的 `tohost`（= 完整校验和）**未验证**；承重判据 = `regs`+`memsig`+`pchash`（前 20k 拍逐位一致） | O-5（把 ELF 移出 elaboration 期）⇒ 才能让 Simulator 侧跑完 |
| **U-2** | **BP 相关路径未覆盖** | GShare（增量 3）的 WIP 被 stash（净负收益 + D8/D10 未签批）⇒ 本次跑的是 **Bimodal + uncond BTB** 状态 | ★ 增量 3 落地后 MUST **重跑**（BP 逻辑变更 ⇒ 后端行为可能变） | BP 增量 3 定稿后 |

### ★ 首跑顺带抓到的第 4 个真缺陷（分层语义反了）

- `--layer night` 原只选 `layer=night` ⇒ **漏掉 PR 层的 8 条** ⇒ 夜间层选集（44）**比 PR 层（8）还小**
  ⇒ **语义反了** ⇒ 首跑报告只有 44 case 而非契约要求的 52。
- ⇒ 修正：`pr` = 快子集（8）· `night` = **全量**（52）· `all` = 同 `night`（契约 v1.6）。
- ⇒ ★ 顺带新增两个工具：`tools/parity/parity_report.py`（报告 → 分歧清单 + 三分类**建议**）
  与 `tools/parity/parity_replay.py`（用已落盘的 runner 结果**重放**判定 ⇒ 门禁改动后不必重跑 50 min；
  ★ 但重放**不是**跑过 ⇒ 报告 `schema_version` 带 `-replay` 后缀以区分）。

## 5. 关联（★ 非本 change scope）

- [ ] 5.1 ★ **BP 增量 3 的量收益跑批**：1.2 → 1.1 三件套未落地 → 1.0 的 D10 未签 ⇒ **被架构签批卡住**
      ⇒ 按 R5 兜底「放弃合并、分开跑」处理；★ 双产物接口已就位（门禁的 `cycles` + branchbench 条目）
- [ ] 5.2 ★ S-12 的「commit message 引 `ADR-089`」机检 ⇒ 归 A4
