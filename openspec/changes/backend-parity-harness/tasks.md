---
change: backend-parity-harness
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
---

# tasks — backend-parity-harness（阶段 A2b）

> 每条任务末尾 `[S-x]` = 它落地的契约单元。

## 1. 比对器（✅ 已落）

- [x] 1.1 `tools/parity/parity_gate.py` 骨架：清单驱动 + 逐项比对 + 三态退出码 [S-9, S-10]
- [x] 1.2 `judge()` 判定与执行**分离**（纯函数）⇒ 可离线自检 [S-9]
- [x] 1.3 确定性前置校验（`node_count` + `names_hash` 不等 ⇒ ERROR，**先于**观测量比对）[S-2]
- [x] 1.4 观测量齐备性（缺一 ⇒ ERROR，**MUST NOT** 跳过或填 0）[S-1, S-9]
- [x] 1.5 逐项比对 + `first_divergence`（metric + 两侧值）[S-9, S-14]
- [x] 1.6 三态退出码（0/1/2）+ `error_kind` 分类 [S-10]
- [x] 1.7 白名单校验：未知字段拒收 / 容差字段拒收 / 必填字段 / `issue_url` / `justification` / 过期失效 / 常驻合成例外缺失 ⇒ ERROR [S-11]
- [x] 1.8 白名单规模护栏：真实例外 > 10 ⇒ WARNING（**不阻断**）[S-12]
- [x] 1.9 语料集合断言（清单 ⊄ harness ⇒ ERROR；harness ⊄ 清单 ⇒ WARNING）[S-13]
- [x] 1.10 报告 `build/parity/report.json`（契约 S-14 字段齐备）[S-14]
- [x] 1.11 分层执行（`--layer pr|night|all` + 清单 `layer` 列）[S-15]
- [x] 1.12 `--self-test` 鉴别力自检 **9/9 PASS** [S-9, S-10, S-11]

## 2. 数据文件（✅ 已落）

- [x] 2.1 `tests/cpu/parity/corpus.txt`：**单一来源**（47 ELF + 4 branchbench，`layer` 列分 PR/夜间）[S-13]
- [x] 2.2 `tests/cpu/parity/exceptions.json`：**只有常驻合成例外** ⇒ 真实例外数 0 [S-11]

## 2b. fail-closed 负向验证（✅ 6/6）

- [x] N1 白名单未知字段 ⇒ ERROR(2)
- [x] N2 白名单容差字段（`tolerance`）⇒ ERROR(2)
- [x] N3 缺常驻合成例外 ⇒ ERROR(2)
- [x] N4 清单里有、harness 没有 ⇒ ERROR(2) `corpus_mismatch`
- [x] N5 清单格式坏（3 字段）⇒ ERROR(2)
- [x] N6 过期例外不豁免（判定层）⇒ FAIL(1)

## 3. 验收

- [x] 3.1 `--self-test` 9/9 PASS（判定逻辑的鉴别力，离线可证）
- [x] 3.2 真实 case 端到端：`rv32ui-p-add` ⇒ PASS + 报告落盘
- [x] 3.3 PR 层（8 case）全量跑通 —— ★ **实测 8/8 PASS**，`verdict=PASS`，报告 `build/parity/report-pr.json`
- [x] 3.4 ★ **契约 S-15 的缓存条款更正**（实测：Verilog 按 ELF 而非按 config 变化 ⇒ 跨 ELF 缓存不成立）⇒ 契约 v1.4 已改「每 corpus 一次编译」+ 重算时长预算（PR ≈10 min / 夜间 ≈1 h）+ 开放项 O-5 ⇒ ★ **待 architect 确认**
- [ ] 3.5 夜间层全量（51 case）⇒ **归 A3**
- [x] 3.6 「S ⇒ 实现/测试」覆盖矩阵交付 —— 已填实到契约 **§7**（16 条全落点；S-12 的「commit message 机检」如实标 ⚠️ 待 A4）
