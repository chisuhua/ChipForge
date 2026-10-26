---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: in-progress
created_from: docs/architecture/parity-gate-contract.md
adr:
  - ADR-089
  - ADR-088
depends_on:
  - parity-gate-spec
  - backend-parity-observability
---

# backend-parity-harness

> **阶段 A2b（比对器）**：把两侧的观测量片段变成**一个可判定的门禁**。
> ★ 契约锚：`docs/architecture/parity-gate-contract.md` **S-9 ~ S-16**（本 change 每条 task 引 S 编号）。
> ★ 上游依赖：**A2a 已完成**（两侧 5 项观测量 + 确定性判据，5/5 corpus 实测逐位相等）。

## Why

A2a 只证明了「两侧采得到、且采出来一样」。但**判定本身**还没有实现：

- 没有一个地方把两份片段**逐项比对**并给出 PASS / FAIL / ERROR
- 没有**白名单**（已知分歧的受控登记）⇒ 分歧只能靠人看
- 没有**语料单一来源**的强制 ⇒ 两侧各维护一份 `kCases`（已实测漂移：Verilator 侧多一个 `rv32ui-p-simple`）
- 没有**三态退出码** ⇒ 「门禁自己坏了」与「后端真的不一致」混在一起 ⇒ 分歧清单会被污染
- 没有**报告** ⇒ CI 无法回读结果

⇒ 没有这些，A3 的「首跑产出既存后端分歧清单」根本无从谈起。

## What Changes

| 件 | 内容 | 状态 |
|---|---|---|
| 比对器 | `tools/parity/parity_gate.py`（纯 stdlib Python）：清单驱动 · 逐项比对 · 三态退出码 · 白名单校验 · 语料集合断言 · 报告 · `--self-test` 鉴别力自检 | ✅ **已落** |
| 语料清单 | `tests/cpu/parity/corpus.txt`（**单一来源**，47 ELF + 4 branchbench，`layer` 列分 PR/夜间） | ✅ **已落** |
| 白名单 | `tests/cpu/parity/exceptions.json`（**只有常驻合成例外** ⇒ 真实例外数为 0） | ✅ **已落** |
| 报告 | `build/parity/report.json`（契约 S-14 字段） | ✅ **已落** |

## Acceptance

- [x] `--self-test` 鉴别力自检 **9/9 PASS**（全等 PASS / 不等 FAIL / 缺失 ERROR / 确定性不符 ERROR / 例外豁免 / 过期失效 / 作用域不匹配）
- [x] 真实 case 端到端跑通（`rv32ui-p-add` ⇒ PASS + 报告落盘）
- [x] PR 层（8 case）全量跑通
- [x] 语料集合断言生效（清单缺 ⇒ ERROR；harness 多 ⇒ WARNING）
- [ ] 夜间层全量（51 case）跑通 ⇒ **属 A3**（首跑 + 分歧清单）

## Impact

| 面 | 影响 |
|---|---|
| 代码 | 新增 `tools/parity/parity_gate.py`（≈480 行，纯 stdlib）+ 语料/白名单两个数据文件 |
| 测试 | 既有基线**零影响**（不改任何既有 harness 行为） |
| 成本 | ★ PR 层 8 case ≈ **10 min**（Simulator 8×~53 s + Verilator 8×~20 s）；夜间 51 case ≈ **1 h** |
