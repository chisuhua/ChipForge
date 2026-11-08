# `docs/architecture/` 目录导航

> ★ **本文件的唯一职责 = 目录级导航**（★ 「要查什么 ⇒ 去哪篇」）⇒ ★ **不承载任何决策与结论**（★ 结论一律在 ADR / 主题文档）。
> ★ ★ **不复制 `adr.md` 的注册表内容**（★ `adr.md` 是架构决策导航入口，★ 单一权威）⇒ ★ 本文件只给**分区说明 + 阅读路径**。
> ★ **治理依据**：`docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md`（✅ Accepted v1.0）

---

## 1. 分区说明

| 分区 | 语义 | 谁能写 | 治理动作 |
|---|---|---|---|
| **顶层 `*.md`** | **真相源**（架构决策 + 架构文档）—— 描述「该长什么样」 | architect | **晋升**：进顶层 MUST 是纯 target |
| **`adr/`** | **架构决策的本体**（ADR 全文） | architect | 新增 ADR MUST 同步 `adr.md` |
| **`mixed/`** | 混合态（含实施态污染 / 未净化） | architect（可净化） | **下沉**：顶层混入实施态 ⇒ 立即下沉 |
| **`archive/`** | 历史 / 台账 / 已定案计划 / PR 描述 / 被取代内容 | **只读**（owner） | **归档**：被取代 / 失效 / 历史完成 |
| **`plugin-research/`** | 研究材料（非真相源） | 研究 owner | — |

★ **三条纪律**（ADR-095 §2.3）：
1. **顶层只放真相源** —— 实施态内容（时点化的现状描述 / 进度快照）**不得**进顶层
2. **引用子目录须标非权威** —— 顶层文档引用 `mixed/` / `archive/` 时 MUST 标「非权威 · 仅作背景」
3. **存量走就地净化** —— 已在顶层的文档**就地改写**（不移动）；下沉路径只给**新增**文档

---

## 2. 顶层真相源清单

★ ★ **本清单由目录实测派生**（★ 新增顶层文档 MUST 同步本表；★ 与 `ls docs/architecture/*.md` 机检相等）。

| 文档 | 内容 | 读它当你想知道 |
|---|---|---|
| [`overview.md`](./overview.md) | 总体架构 | 系统由哪些部分组成、Plugin-style 两模式怎么分工 |
| [`interface-design.md`](./interface-design.md) | 接口形态 | Bundle / 端口 / 形态演进（早期形态 vs 设计目标） |
| [`error-handling.md`](./error-handling.md) | 错误处理 | 错误分类、响应方式、与 CH_MEM 错误范式的关系 |
| [`background-and-goals.md`](./background-and-goals.md) | 背景与目标 | 项目为什么存在、目标与非目标 |
| [`roadmap-evolution.md`](./roadmap-evolution.md) | 架构演进 SSOT | 各版本节点的架构图与跨版本对比 |
| [`parity-gate-contract.md`](./parity-gate-contract.md) | 门禁面契约 | 双后端差分门禁的 S-1~S-17 条款、GN-1 总则 |
| [`hierarchical-plugin-architecture.md`](./hierarchical-plugin-architecture.md) | 层次化 Plugin 主题 | 层次化装配目标形态、残留清单、导航层（§0） |
| [`adr.md`](./adr.md) | **架构决策导航入口** | 全部 ADR 的注册表 + 状态 + 验证方式 |

★ ★ **IP 级索引不在本节** —— 它已移至 [`ip/ip-catalog.md`](../../ip/ip-catalog.md)（★ 语义上管 `ip/<name>/docs/`，不属架构文档）。

---

## 3. 按意图的阅读路径

| 你想… | 读这几篇（按顺序） |
|---|---|
| **第一次接触本项目架构** | 本文件 → [`overview.md`](./overview.md) → [`adr.md`](./adr.md)（§2 决策主表） |
| **查某条决策为什么这么定** | [`adr.md`](./adr.md) §2 → 对应 `adr/ADR-*.md` 的 `## Decision` / `## Options considered` |
| **写一个层 2 单元** | [`hierarchical-plugin-architecture.md`](./hierarchical-plugin-architecture.md) §3 → `adr/ADR-094` §Decision → 上游写作指南 |
| **改门禁 / 看一致性证据怎么算** | [`parity-gate-contract.md`](./parity-gate-contract.md)（S-1~S-17 · GN-1）→ `adr/ADR-089` |
| **判断某条规则归谁管** | [`hierarchical-plugin-architecture.md`](./hierarchical-plugin-architecture.md) §0.5（权威分布） |
| **查「实施到哪了 / 离目标多远」** | ★ **不在本目录** ⇒ [`.rddf/roadmap/`](../../.rddf/roadmap/) + [`roadmap-evolution.md`](./roadmap-evolution.md) |
| **查未决 / 已知边界** | 对应文档的残留 / 开放项节（如 `parity-gate-contract.md` 的 O-* 表） |

---

## 4. 子目录

| 目录 | 份数 | 说明 |
|---|---|---|
| [`adr/`](./adr/) | 19 | ADR 全文。★ **注册表在 [`adr.md`](./adr.md)**（新增 ADR MUST 在 §2 加行） |
| [`mixed/`](./mixed/) | 5 | 混合态（`plugin-framework` / `declarative-hybrid-framework` / `code-framework-mapping` / `tech-selection` / `testing-and-dse`）⇒ ★ **非权威 · 含实施态污染** |
| [`archive/`](./archive/) | 4 | 归档（`tlm-coverage-parity-ledger` 台账 / `tlm-retirement-b1-capacity-plan` 已定案计划 / `m4-dse-*` PR 描述 / `performance-guide`）⇒ ★ **非权威 · 只读** |
| [`plugin-research/`](./plugin-research/) | — | 研究材料 |

★ ★ **引用 `mixed/` 或 `archive/` 时 MUST 标明「非权威 · 仅作背景」**（ADR-095 §2.5 门禁 ④）。

---

## 5. 本目录的治理门禁

| 门禁 | 内容 | 载体 |
|---|---|---|
| ① **顶层纯净 lint** | ★ **顶层**（`docs/architecture/*.md`，**非递归**）不得出现实施态时点语汇（★ **`adr/` 不在范围** · `mixed/` / `archive/` 豁免）⇒ ★ 词表 + 8 类语法上下文豁免（ADR-095 v1.1 §2.5.1） | `tools/check_arch_truth.sh` |
| ② **链接完整性** | `doc_link_check.sh` 覆盖 `docs/architecture/`（含子目录） | `tools/doc_link_check.sh` |
| ③ **ADR 孤儿检查** | `adr/` 每份在 `adr.md` §2 有行；`adr.md` 每行本地链接存在 | `tools/verify_adr.sh` |
| ④ **子目录引用非权威标注** | 顶层引用 `mixed/` / `archive/` 下的**文件**时须标「非权威」 | 门禁 ① / 人工核销 |

★ 门禁的落地与验证：`openspec/changes/archive/2026-11-09-architecture-truth-boundary-enforcement/`
