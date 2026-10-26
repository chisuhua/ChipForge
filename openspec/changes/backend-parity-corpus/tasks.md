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

- [ ] 4.1 52 case 全量差分跑（`--layer night`，`jobs=4`）⇒ 报告落盘
- [ ] 4.2 ★ **产出既存后端分歧清单**（每条含：case / 观测量 / 两侧值 / 首个分歧点）
- [ ] 4.3 ★ 每条分类：**上游待对齐**（⇒ 上游 issue + 收敛期限）/ **有意例外**（⇒ 白名单 5 必填字段 + 作用域）/ **真 bug**（⇒ 修复 change）
- [ ] 4.4 处置登记：白名单条目 OR 上游 issue OR 修复 change ⇒ **分歧清单归零或全部显式登记**（= G-A3 判据）
- [ ] 4.5 契约 §7 覆盖矩阵补 A3 行 + roadmap（A3 勾选 + G-A3 输入）

## 5. 关联（★ 非本 change scope）

- [ ] 5.1 ★ **BP 增量 3 的量收益跑批**：1.2 → 1.1 三件套未落地 → 1.0 的 D10 未签 ⇒ **被架构签批卡住**
      ⇒ 按 R5 兜底「放弃合并、分开跑」处理；★ 双产物接口已就位（门禁的 `cycles` + branchbench 条目）
- [ ] 5.2 ★ S-12 的「commit message 引 `ADR-089`」机检 ⇒ 归 A4
