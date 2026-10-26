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
  - backend-parity-harness
---

# backend-parity-corpus

> **阶段 A3（语料接入 + 首跑）**：把「两侧交集」落成**可跑的语料**，并**第一次**把全量差分跑起来。
> ★ 契约锚：`docs/architecture/parity-gate-contract.md` **S-1 ~ S-16**（v1.5）。

## Why

A2b 交付了**判定器**，但判定器从未在**真实语料**上跑过全量。缺口有三层：

1. **语料不完整**：Verilator 侧 `kCases` 48 个、Simulator 侧 47 个 ⇒ `rv32ui-p-simple` 只在一侧
   ⇒ 差分比对**缺一侧** ⇒ 该 case 无法参与门禁（ADR-089 D4 明确要求补齐）。
2. **branchbench 4 档从未进过门禁**：它们是分支密集负载（最可能出现后端分歧的地方），
   且是 BP 量收益的同一批负载 ⇒ 缺失 = 门禁覆盖了「ALU/load/store」但**没覆盖「分支」**。
3. ★ **从未全量跑过 ⇒ 「既存后端分歧」是未知的** ⇒ ADR-089 的整个价值主张（照妖镜）
   **至今没有一份实测清单**。今天两边都 `tohost==1`，但**内部状态是否逐位相同** —— 未验证。

⇒ A3 的核心交付物不是「门禁能跑」，而是**那份既存后端分歧清单**（每条分类：上游待对齐 /
有意例外 / 真 bug）⇒ ★ 这是**阶段 A 最重要的产出物**，也是 ADR-088 §5.2 删除判据 6 的唯一来源。

## What Changes

| 件 | 内容 | 状态 |
|---|---|---|
| 语料补齐 | `rv32ui-p-simple` 补进 Simulator 侧 `kCases`（1 行）⇒ 两侧 48 == 48 | ✅ **已落** |
| 清单扩展 | `tests/cpu/parity/corpus.txt` ⇒ **52 case = 48 ELF + 4 branchbench** | ✅ **已落** |
| ★ partial 语义 | 门禁新增 **S-9a**：cap 截断 ⇒ `tohost`/`cycles` **vacuous**；两侧 `done` 不一致 ⇒ FAIL | ✅ **已落** |
| per-case cap | 清单第 5 列 `max_cycles` + 按 `kind` 默认 cap（ELF 3k / branchbench 20k） | ✅ **已落** |
| 并行 | 门禁 `--jobs`（两侧 runner 是独立子进程 ⇒ 线程池安全）⇒ 52 case 从 ≈3 h 降到 ≈45 min | ✅ **已落** |
| pre-flight | 清单引用的 ELF MUST 存在（branchbench 4 个是 CMake 构建产物 ⇒ 不在版本控制内） | ✅ **已落** |
| **首跑 + 分歧清单** | 52 case 全量差分 ⇒ **既存后端分歧清单**（每条分类 + 处置） | 🔄 **本 change 的核心** |

## Acceptance

- [x] 两侧 `kCases` 集合相等（48 == 48，交集 48，**零漂移**）
- [x] 清单扩到 52 case（48 ELF + 4 branchbench），PR 层 8 条
- [x] `--self-test` **13/13**（含 ★ partial 四情形：两侧截断 / partial 下 pchash 不等 /
      completion 不一致 / completion 缺失）
- [x] PR 层 **8/8 PASS**（实测 6.5 min，`jobs=4`）
- [x] 新增 case `rv32ui-p-simple` 端到端 PASS（两侧 5 观测量逐位相等）
- [x] ★ **门禁自身 3 个真 bug 修复**（见 design.md §「跑批前抓到的 3 个缺陷」）
- [ ] ★ **52 case 全量首跑** ⇒ 产出**既存后端分歧清单**
- [ ] ★ 分歧清单每条分类（上游待对齐 / 有意例外 / 真 bug）+ 处置登记

## Impact

| 面 | 影响 |
|---|---|
| 代码 | `tools/parity/parity_gate.py`（+partial 语义 +per-case cap +并行 +pre-flight）· `tests/cpu/test_cpu_chmem_vendored_elf.cpp`（+1 行） |
| 既有测试 | ★ **零回归**（`kCases` 只增不改 ⇒ `[vendored-elf]` 从 47 → **48** case，新增 case 必须 PASS） |
| 机时 | 52 case ≈ 45~60 min（`jobs=4`）⇒ ★ **落在夜间窗口内** |
| 契约 | **v1.4 → v1.5**（S-9a partial 语义 + S-13 第 5 列 + S-15 per-case cap + 预算实测更正） |
