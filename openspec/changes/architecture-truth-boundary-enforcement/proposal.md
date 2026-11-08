---
initiative: wave7-architecture-governance
priority: P1
version_target: governance-line
status: proposed
created_from: docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md
adr:
  - ADR-095
  - ADR-089
  - ADR-088
depends_on:
  - architecture-truth-boundary
---

# architecture-truth-boundary-enforcement —— 4 道门禁落地 + 顶层净化

> ★ **本 change = 门禁面 + 内容面**：★ 落地 4 道机制门禁（★ 让「顶层干净」**可持续**而非一次性）+ ★ 净化顶层残留 + ★ 回滚验证。
> ★ ★ **前置硬依赖**：`architecture-truth-boundary` MUST 先完成（★ 门禁① 在迁移前落地 ⇒ 顶层 9 份里 4 份含实施态 ⇒ **必然报红** ⇒ 门禁与迁移的顺序被**锁死**）。
> ★ ★ **AC 权威 = 本文件的 `## Acceptance Criteria` 段**（★ 与 `rdd-verifier` 提取源一致：★ `rdd-verifier/SKILL.md:148`）⇒ ★ **`tasks.md` 的 checklist = 执行镜像**（★ builder 落）。
> ★ ★ **段头必须用 `## Acceptance Criteria`**（★ 正则只认 `验收标准|Acceptance Criteria`：★ `rdd-verifier/SKILL.md:153`）⇒ ★ ★ **不得用 `## Acceptance`**（★ 现有 7 个 change 正是因此 **verifier 提取为空**）。

## Why

★ ADR-095 的三机制（★ **晋升 / 下沉 / 归档**）只有**分类规则**没有**执行装置** ⇒ ★ 顶层清完一次后，**没有任何机制阻止它再次变脏**。

★ ★ **4 道门禁各自的缺口**（★ 现状实测）：

| 门禁 | ★ 缺口（实测） | ★ 本 change 的落点 |
|---|---|---|
| ★ ① **顶层纯净 lint** | ★ **不存在任何脚本**检查顶层是否混入实施态（★ 全仓 grep 零命中）⇒ ★ 顶层脏了无人知道 | ★ `tools/check_arch_truth.sh`（★ 新建） |
| ★ ② **链接完整性扩围** | ★ `tools/doc_link_check.sh` 的 `PATH_CHECK_DOCS` 只覆盖**根级 3 文档**（★ `docs/architecture/` **不在内**）⇒ ★ 本仓已有的 ≥16 处历史死/错引用（★ 含 `src/cf_plugin/tests/` 11 处 + `extern/CppTLM` 剩余 2 处）**从未被检查** | ★ 扩围 `PATH_CHECK_DOCS` |
| ★ ③ **ADR 孤儿检查** | ★ `tools/verify_adr.sh` **无**「`adr/` ↔ `adr.md` 双向」检查 ⇒ ★ 新增 ADR 若忘改 `adr.md` ⇒ **静默失联** | ★ `verify_adr.sh` 加双向检查 |
| ★ ④ **子目录引用非权威标注** | ★ 顶层文档引用 `mixed/` / `archive/` 时**无**强制标注 ⇒ ★ 读者可能把历史派生源当权威 | ★ lint 规则 + 人工核销 |

★ ★ **另有一个内容缺口**：★ `overview.md` 正文仍有**实施态残留**（★ 含「当前状态（Phase 6c/6d, 2026-10-08)」段 ⇒ ★ 与本轮删掉的头部快照**同款同日期**）⇒ ★ 门禁① 落地后会**报红** ⇒ ★ **必须先净化**（★ 本 change 的 **AC-109c**）。

## What Changes

**In Scope**:

- ★ **AC-109c** `overview.md` 正文深度净化（★ 8 处）★ + `interface-design.md` 正文残留（★ 1 处）
- ★ **AC-110** 门禁① 顶层纯净 lint（★ 新建 `tools/check_arch_truth.sh`）
- ★ **AC-111** 门禁② `doc_link_check.sh` 扩围
- ★ **AC-112** 门禁③ `verify_adr.sh` 加 ADR 孤儿检查
- ★ **AC-113** 门禁④ 顶层引用子目录须标非权威
- ★ **AC-114** 回滚验证（★ 两批实测 `git revert`）
- ★ **AC-115a**（★ 跨 change）★ AC ↔ V **静态标签规则**
- ★ **AC-115b**（★ 跨 change）★ AC ↔ V **并集校验**（★ 只在本 change 执行）
- ★ **AC-119**（★ 本 change 特有）★ **门禁① 的误伤收敛证据**（★ fixture + 三类真实样本）
- ★ **AC-120**（★ 本 change 特有）★ **既有 10 个 change 的 AC 段头统一**（★ 7 个 `## Acceptance` ⇒ verifier 提取为空）

**Out of Scope**:

- ★ **顶层 4 份中已净化的部分**（★ `overview.md` 头部 · `interface-design.md` §1.0 · `error-handling.md` · `background-and-goals.md` 订正块 ⇒ ★ **已完成**，★ **不在此重做**）
- ★ **目录移动**（★ 归姊妹 change `architecture-truth-boundary`）
- ★ **CI 门禁接线**（★ 属阶段 A4 `backend-parity-ci` ⇒ ★ 归 `feat-backend-parity-gate.md`）
- ★ **`check_plugin_portability.sh` 单模化改版**（★ 属阶段 C 前置⑤ ⇒ 归 `ADR-093`）
- ★ **改已签批 ADR 的决策文字**（★ 归 architect · ★ 本 change 只消费不改）

### ★ ★ 落地顺序（★ 依赖被锁死 · ★ 不可重排）

```
★ AC-110（门禁① 工具 + fixture）──→ AC-109c（内容净化）──→ AC-110 终验（顶层 0 命中）
★ AC-111（门禁② 扩围）★ MUST 在姊妹 change 完成迁移之后（★ 此时代码面已 0 broken）
★ AC-112（门禁③）★ 独立
★ AC-113（门禁④）★ 依赖 AC-110 的语法上下文豁免规则
★ AC-114（回滚验证）★ 依赖全部 AC-101~AC-113
★ AC-115b（并集校验）★ 依赖姊妹 change 完成 + 本 change 的 AC 标签全在位
```

★ ★ **★ 循环依赖已解除**：★ `AC-109c` 的兜底判据曾是「门禁① 词表全文扫描 0 命中」，★ 而门禁① 的判据是「顶层命中即报红」（★ 同一份未净化顶层）⇒ ★ **互为前提**。★ ⇒ ★ **修正**：★ **AC-110 的落地判据 = 「工具在 fixture 上能红 + 三类真实样本不误报」**（★ 不依赖顶层已净化）；★ **AC-109c 的终态判据 = 「门禁① 对顶层扫 0 命中」**（★ 依赖门禁① 已可用）。

### ★ ★ 门禁① 的语法上下文豁免（★ **MUST 是语法规则 · 禁按路径豁免**）

★ ADR-095 §2.5 明禁「按路径豁免」⇒ ★ 豁免 MUST 由**语法位置**判定，★ 4 类（★ 每类 MUST 有 fixture）：

| # | 语法位置 | 判据（★ 可机检） |
|---|---|---|
| ★ (a) | 文件头 `---` 块内 | 行首为 `---` 且处于文件前 20 行 |
| ★ (b) | 状态列 | 表格行首单元格以 `✅` / `🚧` / `⚠️` / `⏳` 开头 |
| ★ (c) | 注释块内 | 行在 `<!--` 与 `-->` 之间 |
| ★ (d) | dated 块引用 | 行以 `> **` 开头且同一行含 `YYYY-MM-DD` |

★ ★ **实测 Day-1 误伤样本（★ MUST 不误报）**：★ ① `adr.md` 的状态列 · ② `roadmap-evolution.md` 的图例行「进行中」· ③ 目标态前提句（★ 如「★ 本文件 = 目标架构（真相源）」）。

### ★ ★ 词表冻结（★ 单一真源 · ★ 禁开放词表）

★ 实施态时点语汇词表 MUST **冻结在脚本内**（★ 单一真源）⇒ ★ 变更须走 PR + 审查（★ 禁运行时读外部文件 ⇒ 禁「随便加词」）。★ 初始词表（★ 来自 architect 实测命中）：

★ `当前状态` · `当前实现` · `当前实际状态` · `已落地` · `已完成` · `已实现` · `Phase N 完成` · `是 POD` · `PASS`（★ 须排除**代码块**与**表格状态列**）★ ★ **词表 + 语法上下文豁免 = 完整判据**（★ 缺一 ⇒ 门禁不可用）。

## Capabilities

- ★ **可持续性**：★ 三机制（★ 晋升 / 下沉 / 归档）获得**执行装置** ⇒ ★ 顶层不会二次变脏
- ★ **链接完整性**：★ `doc_link_check.sh` 覆盖 `docs/architecture/`（★ 含子目录）⇒ ★ ≥16 处历史死/错引用入检
- ★ **ADR 不失联**：★ `adr/` ↔ `adr.md` 双向检查 ⇒ ★ 孤儿即报红
- ★ **内容达标**：★ 顶层 4 份 + `interface-design.md` 正文**无实施态断言**
- ★ **可回滚**：★ 两批实测 `git revert` ⇒ 引用与 `arch_paths` 复原

## Impact

- ★ **新增脚本**：`tools/check_arch_truth.sh`（★ 门禁①）
- ★ **修改脚本**：`tools/doc_link_check.sh`（★ 门禁② 扩围）· `tools/verify_adr.sh`（★ 门禁③ 孤儿检查）
- ★ **修改文档**：`docs/architecture/overview.md`（★ AC-109c）· `docs/architecture/interface-design.md`（★ AC-109c）· ★ 顶层引用 `mixed/`/`archive/` 处的非权威标注（★ AC-113）
- ★ **修改既有 change**：★ 10 个有 `tasks.md` 的 change 中 **9 个**有 Acceptance 段（★ **7 个段头不合规** ⇒ 提取为空）⇒ ★ 统一为 `## Acceptance Criteria`（★ AC-120）
- ★ **不改**：★ 已签批 ADR 的**决策文字**（★ 只读引用）· ★ 代码 · ★ IP 级 ADR 目录
- ★ ★ **CI 归一**（★ **本 change 不做**）：★ 门禁脚本落地后**是否接入 CI** ⇒ ★ 归 `architecture-gates.yml`（★ 但该 workflow **不构建项目** ⇒ 完整回归仍属阶段 A4）⇒ ★ **本 change 只保证脚本本地可跑 + 0 误伤**
- ★ ★ **归档 change 的历史引用**：★ `doc_link_check.sh` 的 `SCAN_PATHS` 不含 `openspec/` ⇒ ★ **显式 out of scope**（★ 否则读者会误以为全仓 0 broken）

## Acceptance Criteria

> ★ ★ **本段 = AC 权威**（★ `rdd-verifier` 从此段提取）⇒ ★ ★ **段头必须用 `## Acceptance Criteria`**。
> ★ ★ **AC-115a 规则**：★ 每条 AC 显式带 `覆盖 V:` 标签；★ **无孤儿 AC**（★ 每条至少盖 1 个 V）；★ 本 change 声明的 V 均被覆盖。

- [ ] **AC-109c** ★ **`overview.md` 正文深度净化 + `interface-design.md` 正文残留**（★ 本轮不动 ⇒ 本 change 执行）⇒ ★ 判据：★ ① `overview.md` 的 8 处（含 `:255-261` 的「当前状态（Phase 6c/6d, 2026-10-08)」段 · `:190` · `:193` · `:210-212` · `:313` · `:382` · `extern/` 目录树）**全无实施态断言**；★ ② `interface-design.md:305`（★ `ImplMode` 已废弃）的实施态措辞**已改形态定性**；★ ③ ★ **兜底 = 门禁① 词表对顶层全文扫描 0 命中**（★ 含语法上下文豁免逐条登记）⇒ ★ **防止枚举不完备时误判通过**；★ ④ **须单独过审查**（★ 属内容决策 ⇒ 不与机械改动混批）。★ **覆盖 V: V-108**
- [ ] **AC-110** ★ **落地门禁①（顶层纯净 lint）**：★ 新建 `tools/check_arch_truth.sh` ⇒ ★ 判据：★ ① 词表**冻结在脚本内**（★ 单一真源 · ★ 禁开放词表）；★ ② **语法上下文豁免**按 §「4 类语法位置」实现（★ **禁按路径豁免**）；★ ③ **fixture 单测**：★ 4 类豁免各有 1 个 fixture（★ **必过**）+ ★ 3 个真实样本（★ `adr.md` 状态列 · `roadmap-evolution.md` 图例 · 目标态前提句）**不误报**；★ ④ 顶层或 `adr/` 命中实施态语汇 ⇒ **报红**；★ ⑤ `mixed/` `archive/` 同类文本 ⇒ **豁免**。★ **覆盖 V: V-103**
- [ ] **AC-111** ★ **落地门禁②**（★ `doc_link_check.sh` 的 `PATH_CHECK_DOCS` 扩入 `docs/architecture/` ⇒ 含子目录）⇒ ★ 判据：★ ① 覆盖范围含 `docs/architecture/`；★ ② **≥16 处历史死/错引用入检**（★ 含 `src/cf_plugin/tests/` 11 处 + `extern/CppTLM` 剩余 2 处）⇒ ★ **须同时修或显式登记豁免**（★ 否则落闸即爆）。★ **覆盖 V: V-104**
- [ ] **AC-112** ★ **落地门禁③**（★ `verify_adr.sh` 加 ADR 孤儿检查：`adr/` ↔ `adr.md` 双向）⇒ ★ 判据：★ ① `adr/` 每份在 `adr.md` §2 有行；★ ② `adr.md` 每行本地链接存在；★ ③ ★ **报错信息含可执行指引**（★ 「新增 ADR 请在 `adr.md` §2 加行」+ 同步责任人 = architect）⇒ ★ **只检不引导 ⇒ 门禁会持续制造孤儿 ⇒ 逼人加 allowlist**（★ 须避免）；★ ④ 实测零误伤 ⇒ 无 baseline 负担。★ **覆盖 V: V-105**
- [ ] **AC-113** ★ **落地门禁④**（★ 顶层文档引用 `mixed/` / `archive/` ⇒ MUST 标明「★ **非权威 · 仅作背景**」）⇒ ★ 判据：★ ① 顶层引用子目录处**均带非权威标注**；★ ② 门禁① 或人工核销二选一（★ 判据写死）。★ **覆盖 V: V-101, V-103**
- [ ] **AC-114** ★ **回滚验证**：★ **首批**（★ `code-framework-mapping.md` 等 3 节点环批）与**最大批**两批实测 `git revert` ⇒ ★ 判据：★ ① revert 后**引用与 `arch_paths` 全部复原**；★ ② ★ **revert 后三个检查全绿**（★ `doc_link_check.sh` 0 broken · `verify_adr.sh` 0 FAILED · `arch_paths` liveness 无死条目）；★ ③ 其余批由「单 commit 可 revert」结构性保证。★ **覆盖 V: V-110**
- [ ] **AC-115a** ★ ★ **AC ↔ V 静态标签规则**（★ 跨 change）⇒ ★ 判据：★ ① **本 change + 姊妹 change 的每条 AC 都带 `覆盖 V:` 标签**；★ ② **无孤儿 AC**（★ 每条至少盖 1 个 V）；★ ③ 每个被声明的 V 至少被 1 条 AC 覆盖。★ **覆盖 V: 全部（★ 本条自豁免于并集计算）**
- [ ] **AC-115b** ★ ★ **AC ↔ V 并集校验**（★ 跨 change · ★ **只在本 change 的最后一条 task 执行**）⇒ ★ 判据：★ ① 脚本（★ `tools/check_ac_v_coverage.py`）判定 **两个 change 的 AC 覆盖 V 并集 == V-101~V-110 全集**；★ ② **无孤儿 V**（★ 每个 V 至少被 1 条 AC 覆盖）；★ ③ **无重复分配**（★ 同一 V 被两条 AC 覆盖须为**显式允许**的交叉（如 V-104 被 AC-104/105/106/111 覆盖）⇒ ★ 脚本须区分「交叉覆盖」与「重复分配」）。★ **覆盖 V: 全部（★ 本条自豁免）**
- [ ] **AC-119** ★ ★ **门禁① 的误伤收敛证据**（★ 本 change 特有）⇒ ★ 判据：★ ① `evidence/` 含 **4 类豁免 fixture** 的运行输出；★ ② 含 **3 类真实样本**（★ `adr.md` 状态列 · `roadmap-evolution.md` 图例 · 目标态前提句）的**不误报**输出；★ ③ 含 **1 个真阳性**（★ 顶层某处植入实施态语汇 ⇒ 报红）的输出。★ **覆盖 V: V-103**
- [ ] **AC-120** ★ ★ **既有 change 的 AC 段头统一**（★ 本 change 特有）⇒ ★ 判据：★ ① 10 个有 `tasks.md` 的 change 中，★ **9 个有 Acceptance 段的全部改用 `## Acceptance Criteria`**（★ 现状 **7 个用 `## Acceptance`** ⇒ **verifier 提取为空**）；★ ② `mmu-chmem-pipeline-integration` 的 `proposal.md` **补 AC 段**（★ 现状无）；★ ③ **改后逐个跑一遍 AC 提取** ⇒ **提取非空**。★ **覆盖 V: V-104**

> ★ ★ **本 change 的 V 覆盖**：★ **V-101, V-103, V-104, V-105, V-108, V-110**（★ 6 个）★ + **AC-115a/115b** 的**跨 change**并集校验。
> ★ ★ **★ 拆批的必然结果**：★ 本 change 与姊妹 change 的 AC 集合**各自是 V 全集的真子集** ⇒ ★ **单 change 内不可能通过并集校验** ⇒ ★ 这是「拆 2 个 change」的**代价**，★ 已用 `depends_on: [architecture-truth-boundary]` + **AC-115b 只挂本 change** 显式消解（★ 否则姊妹 change 单独 archive 时会永久卡在 P3）。

## References

- ★ `docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md`（★ §2.3 三机制 · §2.5 门禁 · §5 V-101~V-110）
- ★ `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md`（★ 门禁面契约参照）
- ★ `docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（★ §10.3 阶段 C 硬前置⑤「门禁体系自身改版」⇒ **本 change 只做 docs 侧 4 道**）
- ★ `.rddf/reviews/architecture-docs-governance-recommendations.md`（★ §10 M5 · §11 AC 交接件）
- ★ `.rddf/reviews/architecture-goal-and-path-audit.md`（★ §5.2 B-5 AC 权威冲突 · §5.3 关注度不可复现 · §6 任务总表）
- ★ `openspec/changes/architecture-truth-boundary/proposal.md`（★ ★ **前置 change** · ★ 批次方案 = SCC 分批）
