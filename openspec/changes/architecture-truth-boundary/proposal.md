---
initiative: wave7-architecture-governance
priority: P1
version_target: governance-line
status: proposed
created_from: docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md
adr:
  - ADR-095
  - ADR-083
  - ADR-040
depends_on: []
---

# architecture-truth-boundary —— `docs/architecture/` 位置调整 + 引用改写

> ★ **本 change = 位置面**（★ 纯机械）：★ 顶层只留真相源 + 决策导航入口，其余下沉 `mixed/` / 归档 `archive/` / `ip-catalog.md` 移出至 `ip/`；★ **所有引用同步改写** ⇒ ★ 批批可绿。
> ★ ★ **姊妹 change**：`architecture-truth-boundary-enforcement`（★ 门禁落地 + 顶层净化）⇒ ★ ★ **本 change MUST 先于它完成**（★ 门禁① 在迁移前落地必然报红 ⇒ 依赖被锁死）。
> ★ ★ **AC 权威 = 本文件的 `## Acceptance Criteria` 段**（★ 与 `rdd-verifier` 提取源一致：★ `rdd-verifier/SKILL.md:148` 读 `proposal.md`）⇒ ★ **`tasks.md` 的 checklist = 执行镜像**（★ builder 落）。
> ★ ★ **AC 编号沿用 ADR-095 §5 的 V 映射**（★ 源自 `.rddf/reviews/architecture-docs-governance-recommendations.md` §11）。

## Why

★ **`docs/architecture/` 顶层混装了 4 类非真相源内容**（★ 实测顶层 **18** 份 md）：

| 类 | 实例（实测） | ★ 为什么不能留顶层 |
|---|---|---|
| ★ **混合态** | `plugin-framework.md` · `declarative-hybrid-framework.md` · `code-framework-mapping.md` · `tech-selection.md` · `testing-and-dse.md`（★ **5** 份） | ★ 含实施态污染 ⇒ 读者把「现状」读成「合理」 |
| ★ **台账 / 计划 / PR 描述 / 虚构** | `tlm-coverage-parity-ledger.md` · `tlm-retirement-b1-capacity-plan.md` · `m4-dse-cpufactory-real-pr-description.md` · `performance-guide.md`（★ **4** 份） | ★ 台账已在漂（★ 头部 292 vs 实测 282）· ★ `performance-guide.md` 含**虚构内容** |
| ★ **IP 级索引** | `ip-catalog.md`（★ 语义上是 `ip/<name>/docs/` 的索引） | ★ **不属架构文档** ⇒ 应归 IP owner |
| ★ **顶层真相源**（★ 目标 **9** 份） | `README.md`（★ NEW）· `adr.md` · `overview.md` · `interface-design.md` · `roadmap-evolution.md` · `parity-gate-contract.md` · `error-handling.md` · `background-and-goals.md` · `hierarchical-plugin-architecture.md` | ★ **保持** |

★ ⇒ ★ **顶层 = 真相源 + 决策导航入口**（★ `ADR-095` §2.1）⇒ ★ 治理成本从 **O(n)**（每加一份改一次配置）降到 **O(1)**（按目录表达职责）。

★ ★ **架构授权**：`ADR-095`（✅ Accepted v1.0 · 用户签认「批准」2026-11-08）· ★ **角色边界变更同批生效**（★ architect 新增可写 6 份）。

## What Changes

**In Scope**:

- ★ **AC-101** 建 `docs/architecture/README.md`（★ 目录级导航）
- ★ **AC-102** 移动 **10** 份文档（★ 按 SCC 分批 · ★ 每批一个 commit · ★ `git mv` 保留历史）
- ★ **AC-103** 导出「引用基线清单」（★ **每批移动「之前」执行**）
- ★ **AC-104** 改写全部引用（★ 仓外 + `adr/` + 顶层留守 + 根级 + ★ **76 处文档内部相对链接**）
- ★ **AC-105** `adr.md` 三处「源文档」指针改写
- ★ **AC-106** 根级文档同步（★ `AGENTS.md` · `.cursorrules`）
- ★ **AC-107** `ip-catalog.md` ↔ `ip/README.md` 互链 + 单一维护点
- ★ **AC-108** `arch-docs.json` 改目录级表达（★ 分两步 · ★ 避免中间态失配）
- ★ **AC-116**（★ 本 change 特有）★ **批批可绿**的机械保证（★ SCC 分批 + 同批内互链同时改写 + 每批 `doc_link_check` 证据留存）
- ★ **AC-117**（★ 本 change 特有）★ **76 处内部相对链接**逐条改写与核销
- ★ **AC-118**（★ 本 change 特有）★ **`ip-catalog.md` 的 owner 交接**（★ 移出 architect 可写域 ⇒ 须 IP owner 接收）

**Out of Scope**:

- ★ **顶层 4 份的净化**（★ `overview.md` + `interface-design.md` 已完成；★ `error-handling.md` + `background-and-goals.md` 已完成；★ **`overview.md` 正文深度残留** ⇒ 归姊妹 change 的 **AC-109c**）
- ★ **4 道门禁的落地**（★ 归姊妹 change）
- ★ **回滚验证**（★ 归姊妹 change 的 **AC-114** · ★ 依赖本 change 完成）
- ★ **已归档 change 的历史引用**（★ `doc_link_check.sh` 的 `SCAN_PATHS` 不含 `openspec/` ⇒ **显式 out of scope**）
- ★ **改已签批 ADR 的决策文字**（★ 归 architect）

### ★ ★ 批次方案（★ **按 SCC 划分** · ★ 由内部链接图实测导出）

★ ★ **为什么必须按 SCC 而不是按关注度**：★ 待移动 10 份内部共 **76 处**相对链接（★ 实测），★ 其中存在 **2 个环** ⇒ ★ 单文件粒度移动 ⇒ **中间态必然断链** ⇒ ★ 「每批一个 commit」与「批批可绿」**不可兼得**（★ 除非环内同批）。

| 批 | 文件 | 目标 | ★ 同批内互链（★ 必须同时改写） |
|---|---|---|---|
| ★ **批 1** | `code-framework-mapping.md` · `declarative-hybrid-framework.md` · `plugin-framework.md` | `mixed/` | ★ **3 节点环**（★ `plugin-framework` ↔ `declarative-hybrid-framework` ↔ `code-framework-mapping`） |
| ★ **批 2** | `performance-guide.md` · `testing-and-dse.md` | `archive/` + `mixed/` | ★ **2 节点环**（★ `performance-guide` ↔ `testing-and-dse`） |
| ★ **批 3** | `tech-selection.md` | `mixed/` | ★ 无环（★ 但被批 1 的 `code-framework-mapping` / `declarative-hybrid-framework` 引用 ⇒ 须在批 1 之后） |
| ★ **批 4** | `tlm-coverage-parity-ledger.md` | `archive/` | ★ 无环 |
| ★ **批 5** | `tlm-retirement-b1-capacity-plan.md` | `archive/` | ★ 无环 |
| ★ **批 6** | `m4-dse-cpufactory-real-pr-description.md` | `archive/` | ★ 无环 |
| ★ **批 7** | `ip-catalog.md` | `ip/ip-catalog.md` | ★ 无环（★ 但其 18 处链接全为 `../../ip/...` / `](../../soc/...` ⇒ 移到 `ip/` 后**整体降一级重算**） |

★ ★ **每批的原子动作**（★ 一个 commit 内完成，★ ⇒ 该批可单独 `git revert`）：
1. ★ `git mv` 本批文件
2. ★ 改写**指向本批文件**的仓外引用
3. ★ 改写**本批文件内部**的相对链接（★ 指向已移动者 ⇒ 新路径；★ 指向未移动者 ⇒ 保持相对可达）
4. ★ 跑 `bash tools/doc_link_check.sh` ⇒ **0 broken** ⇒ **输出存进 `evidence/`**

### ★ ★ `ip-catalog.md` 移出的**降一级重算**

★ `ip-catalog.md` 现有 **18** 处链接全部形如 `../../ip/<x>/...` / `../../soc/...`（★ 相对 `docs/architecture/`）⇒ ★ 移到 `ip/` 后 ⇒ 深度从 `docs/architecture/`（2 级）变为 `ip/`（1 级）⇒ ★ **须去掉一级 `../`**（★ `../../ip/x` → `../ip/x` ⇒ ★ ★ 目标仍在 `ip/` ⇒ ⇒ 实际变为 `x` 或 `../x`，★ **逐条重算，不可机械替换**）。

## Capabilities

- ★ **顶层纯净**：★ 顶层只剩 **9** 份真相源 md + `adr.md` + `adr/`（★ `adr/` **19** 份 · ★ IP 级 ADR **36** 份**不动**）
- ★ **子目录分流**：`mixed/` **5** 份（★ architect 可净化）· `archive/` **4** 份（★ 只读）· `plugin-research/` 不动
- ★ **链接完整性**：★ 迁移后 `bash tools/doc_link_check.sh` **0 broken**（★ 且检查范围含 `docs/architecture/` ⇒ **门禁②归姊妹 change，本 change 先保证自身绿**）
- ★ **`arch_paths` 目录级**：`arch_paths` = 顶层 9 份 + `adr/` + `mixed/` + `ip/mmu/docs/adr` + `ip/cache/docs/adr`（★ IP 级 ADR 目录**保留 architect 可写**）
- ★ **可回滚**：★ 每批一个 commit ⇒ ★ 单批可 revert

## Impact

- ★ **受影响目录**：`docs/architecture/`（顶层 + 新建 `mixed/` + `archive/`）· `ip/`（新增 `ip/ip-catalog.md`）· ★ 仓外引用方：`AGENTS.md` · `ip/README.md` · `ip/cache/README.md` · `ip/cpu/README.md` · `ip/tilecopy/docs/README.md` · `ip/tilecore/docs/README.md` · `soc/docs/integration-guide.md` · `docs/api/cf_plugin.md` · `docs/templates/IP_TEMPLATE.md` · `ip/cpu/docs/blueprint.md` · `ip/cpu/docs/multi_isa_architecture.md` · `ip/cpu/tlm/README.md` · ★ `docs/architecture/adr/` 内 **5** 处引用
- ★ **历史保留**：★ `git mv` ⇒ 历史不丢（★ 改 `cp`/`mv` 会丢）
- ★ **不改**：★ `ip/mmu/docs/adr/`（35 份）· `ip/cache/docs/adr/`（1 份）· ★ `adr/`（19 份）· ★ 代码
- ★ ★ **中间态风险**：★ `arch_paths` 若提前指向 `mixed/` 而文件未移完 ⇒ ★ **顶层文件短暂不可写**（★ architect 权限失配）⇒ ★ **AC-108 MUST 分两步**（★ 先加目录条目保留顶层单文件条目 ⇒ 移完后再删）
- ★ ★ **`adr.md` 的权威来源**：★ `adr.md` 头部「源文档」指向的两份（★ `code-framework-mapping.md` + `declarative-hybrid-framework.md`）**都要下沉** ⇒ ★ 改写后须明确「★ **`adr.md` 的权威 = `adr/` 自身**；★ 源文档降为**历史派生源**」⇒ ★ 否则注册表失去权威

## Acceptance Criteria

> ★ ★ **本段 = AC 权威**（★ `rdd-verifier` 从此段提取：★ `rdd-verifier/SKILL.md:148,153`）⇒ ★ ★ **段头必须用 `## Acceptance Criteria`**（★ 正则只认 `验收标准|Acceptance Criteria`）⇒ ★ `tasks.md` 的 checklist 是**执行镜像**（★ builder 落 · ★ 编号一一对应）。
> ★ ★ **AC-115a 规则**：★ 每条 AC 显式带 `覆盖 V:` 标签；★ **无孤儿 AC**（★ 每条至少盖 1 个 V）；★ 本 change 声明的 V 均被覆盖。

- [ ] **AC-101** ★ 建 `docs/architecture/README.md`（★ 目录级导航）⇒ ★ **顶层清单由 frontmatter 派生**（★ **不得复制 `adr.md` 的注册表内容** ⇒ ★ 避免第二 SSOT）⇒ ★ 判据：★ ① 文件存在；★ ② 含顶层 9 份清单；★ ③ 清单**与实际目录机检相等**（★ `ls docs/architecture/*.md` 对照）；★ ④ **不含** ADR 条目副本。★ **覆盖 V: V-101**
- [ ] **AC-102** ★ **批量移动 10 份文档**（★ `git mv` · ★ **按 §「批次方案」的 7 批 SCC 顺序** · ★ 每批一个 commit）⇒ ★ 判据：★ ① 顶层 = **9** 份 md + `adr.md` + `adr/`；★ ② `mixed/` = **5** 份；★ ③ `archive/` = **4** 份；★ ④ `ip/ip-catalog.md` 存在且 `docs/architecture/` 内无残留。★ **覆盖 V: V-102, V-109**
- [ ] **AC-103** ★ **导出「引用基线清单」**（★ 脚本导出全部真断链的「原路径 → 新路径」对照表）⇒ ★ ★ **时机 = 每批移动「之前」执行**（★ 否则「原有断链」与「迁移新增断链」混杂 ⇒ V-107 失效）⇒ ★ 判据：★ 清单随 change 存档（★ `evidence/`）+ 含每条断链的旧/新路径。★ **覆盖 V: V-107**
- [ ] **AC-104** ★ **改写全部引用**（★ 仓外 + `adr/` 5 处 + 顶层留守 9 份 + 根级 `AGENTS.md` / `.cursorrules`）⇒ ★ 判据：★ **基线逐条核销完毕**（★ 无遗留）★ 且 `bash tools/doc_link_check.sh` **0 broken**。★ **覆盖 V: V-104, V-107**
- [ ] **AC-105** ★ **`adr.md` 三处「源文档」指针改写**（★ 头部「源文档」字段 + 维护规则 5 + 漂移处理流程）⇒ ★ 改指 `./mixed/...` + 标「★ **非权威 · 历史派生源**」+ 措辞「提炼」→「历史派生」⇒ ★ **并明确「`adr.md` 的权威 = `adr/` 自身」** ⇒ ★ 判据：★ 三处均已改写；★ 顶部真相源文档**不指向 `mixed/` 作为权威**。★ **覆盖 V: V-101, V-104**
- [ ] **AC-106** ★ **根级文档同步**（★ `AGENTS.md` 的「关键文档源」表 + ★ `.cursorrules`）⇒ ★ ★ **实测基线**：★ `AGENTS.md` 命中 **1** 处（★ `plugin-framework.md`）· ★ `.cursorrules` 命中 **0** 处（★ ⇒ 该部分**可能为空任务** ⇒ ★ 以路径为准而非以文件为准）⇒ ★ 判据：★ 根级文档内**无指向已移动文档的失效路径**。★ **覆盖 V: V-104**
- [ ] **AC-107** ★ **`ip-catalog.md` ↔ `ip/README.md` 互链** + ★ **单一维护点 = `ip/ip-catalog.md`** ⇒ ★ ★ **「声明」不足**：★ `ip/README.md` 的 IP 状态表须**改为指针、不保留副本** ⇒ ★ 判据：★ ① 双向链存在；★ ② 单一维护点声明在位；★ ③ **`ip/README.md` 无 IP 状态表副本**。★ **覆盖 V: V-109**
- [ ] **AC-108** ★ **更新 `.rddf/config/arch-docs.json`**（★ **分两步**）⇒ ★ ① **先**：新建 `mixed/` `archive/` 目录 + 把**目录**加入 `arch_paths`（★ **保留顶层单文件条目**）⇒ ★ ② **后**：移完后删顶层单文件条目 + `exclude_paths` += `docs/architecture/archive`（★ ★ **必须写全前缀** ⇒ 写 `archive/` 匹配不到任何路径 ⇒ 门禁永假绿）⇒ ★ 判据：★ 每条 `arch_paths` / `exclude_paths` 项**实际存在**（★ 无死条目）+ ★ **移动过程中架构师权限不失配**。★ **覆盖 V: V-106**
- [ ] **AC-116** ★ ★ **批批可绿的机械保证**（★ 本 change 特有）⇒ ★ 判据：★ ① **7 批**按 SCC 顺序执行；★ ② **每批一个 commit**；★ ③ **每批内**完成「`git mv` + 指向它的引用改写 + 它内部的相对链接改写」三动作（★ **环内同批**）；★ ④ **每批后** `doc_link_check.sh` **0 broken** ⇒ **输出存入 `evidence/batch-N.txt`**；★ ⑤ **任一批可单独 `git revert` 且回到 0 broken**。★ **覆盖 V: V-110**
- [ ] **AC-117** ★ ★ **76 处文档内部相对链接**逐条改写与核销（★ 本 change 特有）⇒ ★ 判据：★ ① 基线清单含**全部 76 处**（★ 逐条列出旧路径 → 新路径）；★ ② 核销完毕后**残留为 0**；★ ③ `ip-catalog.md` 的 18 处**逐条重算**（★ 深度降一级 ⇒ **不可机械替换**）。★ **覆盖 V: V-107**
- [ ] **AC-118** ★ ★ **`ip-catalog.md` 的 owner 交接**（★ 本 change 特有）⇒ ★ 判据：★ ① `ip/ip-catalog.md` 存在且内容完整；★ ② **IP owner 确认接收**（★ 交接记录入 `evidence/`）；★ ③ `ip/ip-catalog.md` 的维护责任**已登记**（★ 位置：★ `ip/README.md` 的单一维护点声明）。★ **覆盖 V: V-106, V-109**

> ★ ★ **本 change 的 V 覆盖**：★ **V-101, V-102, V-104, V-106, V-107, V-109, V-110**（★ 7 个）⇒ ★ **AC-115b（跨 change 并集校验）归姊妹 change**（★ 本 change 的 AC 集合是 V 全集的**真子集** ⇒ ★ **单 change 内无法通过并集校验** ⇒ ★ 这是拆批的必然结果，★ 已在姊妹 change 用 `depends_on` 显式声明）。

## References

- ★ `docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md`（★ §2.1 目录树 · §2.2 归属 · §2.4 `arch_paths` · §5 V-101~V-110）
- ★ `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md`
- ★ `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md`（★ v3.0）
- ★ `.rddf/reviews/architecture-docs-governance-recommendations.md`（★ §10 M1~M6 · §11 AC 交接件）
- ★ `.rddf/reviews/architecture-goal-and-path-audit.md`（★ §6 后续任务总表 · §6.4 关键路径）
