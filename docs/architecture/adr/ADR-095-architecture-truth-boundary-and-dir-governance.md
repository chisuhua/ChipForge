# ADR-095 — `docs/architecture/` 真相源边界与目录治理

- **Status**: ✅ **Accepted v1.0**（2026-11-08 · architect；★ ★ **用户签认「批准」** ⇒ 决策已定）
- **签认记录**: ★ **2026-11-08 · 用户签认「批准」** ⇒ ★ ① 本 ADR 的决策（★ 顶层结构 · 三机制 · `arch_paths` 目录化 · 4 项门禁）**已定**；★ ② ★ **角色边界变更（★ architect 新增可写 6 份）已签认生效**（★ 见 §2.4）
- **Date**: 2026-11-08
- **Author**: architect
- **Parent（上游）**: 无（★ 本 ADR 是**本仓内部**的文档治理决策，★ 不涉及上游 CppHDL）
- **关联 change**: ★ **尚未立项**（★ 目录重构属**实施面** ⇒ 归 **planner 立项 / builder 执行**）⇒ ★ 登记 backlog：★ `.rddf/reviews/architecture-docs-governance-recommendations.md` §9.1 **A1 / A2 / G7**（★ 本 ADR 是那三项的**决策层**）★ ⇒ ★ **立项后回填**（★ 立项时须把 §5 的 V-101~V-110 拆成该 change 的 AC）
- **AC 映射**: ★ **尚未立项** ⇒ ★ AC 权威（change 的 `tasks.md`）暂缺 ⇒ ★ 立项后回填（★ 本 ADR 的 §5 Verification 段即草案 ⇒ ★ **§5 即立项时 AC 的来源**）
- **决策台账**: `.rddf/reviews/architecture-docs-governance-recommendations.md`（★ §1~§8 诊断 · **§9 治理路线图**）
- **审稿记录**: `.rddf/reviews/ADR-095-architecture-truth-boundary-and-dir-governance-review.md`（★ 本 ADR 的 oracle + critic 审稿记录与逐条处置）

---

## 1. 背景与问题

★ 用户目标（2026-11-08 裁决）：★ **`docs/architecture/` 下的 markdown 文件保持干净的「真相源架构 + 架构决策导航入口」；★ 其他文档放入子目录；★ 会归档目录。**

★ 现状（2026-11-08 实测）：

| 事实 | 实测 |
|---|---|
| 顶层文件 | **18 份** markdown（+ `adr/` **19 份**（★ 含本 ADR）· `plugin-research/`） |
| ★ **混合态占比** | ★ **7 / 18（39%）**（★ 目标态文档里混着实施态快照 / 历史残留） |
| ★ **零全局索引** | ★ 非 ADR 架构文档**无任何全局索引**（★ 3 个索引层都不管它们） |
| ★ **可写范围** | ★ `arch_paths` 命中 **7 份顶层** + `adr/` + ★ **IP 级 ADR 目录 2 处**（★ `ip/mmu/docs/adr` **35 份** + `ip/cache/docs/adr` **1 份** = **36 份** IP 级决策）；★ 其余 **10 份既不在范围内也不在 `exclude_paths`**（★ 治理盲区） |
| ★ **执行机制** | ★ `arch-docs.json` **无任何执行脚本** ⇒ ★ 「可写范围」是**无闸约定** |
| ★ **索引完整性** | ★ 零机检（★ `doc_link_check.sh` 检查 2 仅覆盖根级 3 文档） |
| ★ **账实不符** | ★ `adr.md` §2.4 合计 48 vs §2 标题合计 60（★ 两套口径并存）· §2.3 实为「追加残段」（★ 6 次注册 5 次不搬 ⇒ **人工搬行方案已被证伪**） |
| ★ **内容层错误** | ★ `performance-guide.md` **大面积虚构** · `error-handling.md` 的 `MemRespBundle` 是**死名** · `extern/CppTLM` 5 处**错径** |

⇒ ★ ★ **核心矛盾**：★ 「真相源」与「非真相源」**混在同一层** ⇒ ★ ① 读者无法分辨哪份是权威（★ ⇒ 「入口不可达」）★ ② 治理只能靠**逐份登记**（★ `arch_paths` 白名单）⇒ ★ **治理成本随文档数线性增长** ★ ③ 混合态文档的**实施态污染**无结构性隔离 ⇒ ★ 真相源随时被现状污染。

---

## 2. 决策

### 2.1 ★ 目录结构（★ 顶层 = 真相源 + 决策导航入口）

```
docs/architecture/
├── README.md                          ← ★ **NEW** · 目录级导航（真相源清单 + 阅读路径 + 子目录说明）
├── adr.md                             ← ★ **架构决策导航入口**（注册表 · ★ 76 处引用 ⇒ 绝不动）
├── adr/                              ← ★ **架构决策**（19 份 ADR · 决策的本体 · ★ 含 ADR-095）
├── overview.md                        ← ★ 总体架构（★ 拆 stale 实施态快照 ⇒ 纯 target）
├── interface-design.md                ← ★ 接口设计（★ 拆 Phase 1 当前态 ⇒ 纯 target）
├── roadmap-evolution.md               ← ★ 架构演进 SSOT（★ 版本节点目标 = 目标态的时间轴）
├── parity-gate-contract.md            ← ★ 门禁面契约（★ 纯 target）
├── error-handling.md                  ← ★ 错误处理（★ 纯 target · ★ 修 C++17/死名）
├── background-and-goals.md            ← ★ 背景与目标（★ 纯 target）
├── hierarchical-plugin-architecture.md ← ★ 层次化 Plugin 主题（★ 真相源）
├── mixed/                            ← ★ **混合态**（含实施态污染 / 未净化 · ★ 可被 architect 净化）
│   ├── plugin-framework.md
│   ├── declarative-hybrid-framework.md
│   ├── code-framework-mapping.md
│   ├── tech-selection.md
│   └── testing-and-dse.md
├── archive/                          ← ★ **归档**（历史 / 台账 / 已定案计划 / PR 描述 · ★ 只读）
│   ├── tlm-coverage-parity-ledger.md
│   ├── tlm-retirement-b1-capacity-plan.md
│   ├── m4-dse-cpufactory-real-pr-description.md
│   └── performance-guide.md
└── plugin-research/                  ← ★ 研究（★ 已是子目录）
```

★ ★ **`ip-catalog.md` 的特例**：★ 它是 **IP 级索引**（★ 管 `ip/<name>/docs/`）⇒ ★ **语义上不属于架构文档** ⇒ ★ **移出至 `ip/ip-catalog.md`**（★ 保留文件名；★ `ip/README.md` 章节**不重叠** ⇒ ★ **不合并**）⇒ ★ 引用改 16 处（★ 机械替换）。

### 2.2 ★ 逐文件归属（★ 依据 = 「是否纯 target」+「移动成本」双判据）

| 目标位置 | 文档 | ★ 理由 | ★ **关注度**（★ 外部引用文件数 · ★ 口径 = 仓外唯一 md 文件提及数 · ★ **分批依据**，★ **非断链数**） |
|---|---|---|---|
| ★ **顶层（真相源）** | `adr.md` | ★ 决策导航入口 | **76** ★ 绝不动 |
| ★ **顶层** | `adr/`（19 份 · ★ 含 ADR-095） | ★ 决策本体 | — |
| ★ **顶层** | `overview.md` | ★ 总体架构（★ 拆快照后纯 target） | **41** ★ 移动成本最高 ⇒ 必留 |
| ★ **顶层** | `interface-design.md` | ★ 接口形态（★ 拆当前态段后纯 target） | **23** |
| ★ **顶层** | `roadmap-evolution.md` | ★ 架构演进 SSOT（★ 目标态时间轴） | **22** |
| ★ **顶层** | `parity-gate-contract.md` | ★ 门禁面契约（★ 已纯 target） | **22** |
| ★ **顶层** | `error-handling.md` | ★ 错误处理（★ 修 C++17 / 死名 ⇒ 纯 target） | **6** |
| ★ **顶层** | `background-and-goals.md` | ★ 背景与目标（★ 修 C++17 ⇒ 纯 target） | **2** |
| ★ **顶层** | `hierarchical-plugin-architecture.md` | ★ 层次化 Plugin 主题（★ 真相源） | **4** |
| ★ **`mixed/`** | `plugin-framework.md` · `declarative-hybrid-framework.md` · `code-framework-mapping.md` · `tech-selection.md` · `testing-and-dse.md` | ★ **混合态**（★ 实施态污染 / 历史残留 / 映射态）⇒ ★ **不是真相源** | 14 / 10 / 11 / 4 / 9 |
| ★ **`archive/`** | `tlm-coverage-parity-ledger.md` · `tlm-retirement-b1-capacity-plan.md` · `m4-dse-*.md` · `performance-guide.md` | ★ **台账 / 已定案计划 / PR 描述 / 虚构文档** ⇒ ★ 已被取代 | 22 / 3 / **3** / 2 |
| ★ **移出** | `ip-catalog.md` → `ip/ip-catalog.md` | ★ **IP 级索引** ⇒ 不属架构文档 | 16 |

⇒ ★ ★ **双判据的裁决规则（★ 已知冲突）**：★ 「该留顶层」的引用数 **2~77**（★ 移动成本高）★ 「该下沉/归档」的引用数 **3~22**（★ 可控）⇒ ★ ★ **冲突确实存在**（★ `tlm-coverage-parity-ledger.md` **22** 处却归档 · `ip-catalog.md` **18** 处却移出）⇒ ★ ⇒ ★ **裁决规则 = 以语义为准**（★ 「是不是真相源」优先），★ **迁移按关注度升序分批**（★ 冲突批次的引用改写单独成批）⇒ ★ **本表不宣称二者「不冲突」**。

### 2.3 ★ 三个机制（★ 让「顶层干净」**可持续**，★ 而非一次性重整）

| 机制 | 规则 | ★ 机检 |
|---|---|---|
| ★ **晋升（promotion）** | ★ 一份文档进顶层，MUST 是**纯 target**：★ ① 无实施态时点语汇（「已落地」「当前状态」「当前实际状态」「Phase N 进行中」…）★ ② 无 dated 实施态快照段 ★ ③ 无历史残留表述（★ 如「旧版写的…」） | ★ **顶层纯净 lint**（★ §2.4 门禁 ①） |
| ★ **下沉（demotion）** | ★ 顶层文档一旦混入实施态 ⇒ **立即下沉**到 `mixed/`（★ 不得就地打横幅糊弄）⇒ ★ ★ **注**：★ 「就地加 dated 横幅」是**过渡处置**、**不是终态**（★ 横幅只防「引用」、★ 不防「首屏误导」） | ★ 同上 lint（★ 命中即红） |
| ★ **归档（archive）** | ★ `mixed/` 中的文档 ★ **被取代 / 失效 / 历史完成**（★ 台账 · 已定案计划 · PR 描述）⇒ 进 `archive/`（★ **只读** ⇒ 不再维护）⇒ ★ `archive/` 内**允许**保留实施态与过时内容（★ 那正是它的性质） | ★ `archive/` **豁免** lint |

⇒ ★ ★ **两条豁免规则（★ 消除字面张力）**：★ ① **晋升条件适用于「真相源文档体」**；★ `adr.md` 是**架构决策导航入口**（★ 第二类目）⇒ ★ 按**注册表规则**治理（★ 状态列是决策跟踪的**本性**）⇒ ★ **门禁 ① 对它的豁免是派生机制**，★ 不是例外条款；★ ② **存量豁免**：★ 已在顶层的文档（★ `overview.md` · `interface-design.md` · `error-handling.md` · `background-and-goals.md`）★ **就地净化**（★ 而非「先下沉 `mixed/` 再晋升」）⇒ ★ ★ 理由 = **移动成本**（★ 关注度 40 / 22 / 7 / 2 ⇒ ★ 下沉再晋升会额外产生一轮断链改写）⇒ ★ 但它们**必须先过晋升检验才留在顶层**（★ 判据 = V-108）⇒ ★ ★ **注意**：★ 「就地净化」是**唯一的**存量路径（★ 下沉路径只给**新增**文档用）⇒ ★ 否则「顶层干净」会被推迟到无穷远。
⇒ ★ ★ **这套机制的价值**：★ 「顶层干净」不再靠**人工自觉**，★ 而是**目录位置 + lint 双重保证** ⇒ ★ ⇒ ★ **真相源不可能被现状污染**（★ 这是 ADR-094 D-6「两阶段迁移」在**文档面**的同构做法）。

### 2.4 ★ `arch_paths` 的新形态（★ 与目录结构耦合 ⇒ 职责**物理隔离**）

| 范围 | 内容 | ★ 谁能写 | ★ 理由 |
|---|---|---|---|
| ★ **顶层 + `adr/`** | ★ 真相源架构 + 架构决策 | ★ **architect 独占** | ★ 真相源必须有单一守护者 |
| ★ **`mixed/`** | ★ 待净化的混合态 | ★ **architect 可写**（★ 净化是 architect 的活 ⇒ ★ 晋升路径） | ★ 否则混合态永远无人能净化 ⇒ ★ 长期腐化 |
| ★ **`archive/` + `plugin-research/`** | ★ 归档 / 研究 | ★ **owner**（★ 不在 `arch_paths`） | ★ 归档=只读；★ 研究态由研究 owner 维护 |
| ★ **仓外 · IP 级 ADR** | ★ `ip/mmu/docs/adr`（**35 份**）· `ip/cache/docs/adr`（**1 份**）⇒ ★ **保留 architect 可写** | ★ **architect**（★ 现状即如此） | ★ ★ **保留的理由**：★ IP 级 ADR **也是决策本体 / 真相源** ⇒ ★ 「真相源必须有单一守护者」原则**延伸到它们** ⇒ ★ ★ 且它们**不在 `docs/architecture/` 目录内** ⇒ ★ **不影响「顶层干净」目标** ⇒ ★ ★ **本决策不改变其归属**（★ ⇒ ★ **不构成角色边界变更** ⇒ ★ **无须额外签认**） |
| ★ **仓外 · 其余** | ★ `ip/`（含 `ip-catalog.md`） | ★ IP owner | — |

⇒ ★ ★ **★ 角色边界变更（★ ✅ 2026-11-08 用户签认「批准」⇒ 已生效）**：★ 本形态使 architect **新增可写 6 份**（★ `error-handling.md` · `background-and-goals.md` · `plugin-framework.md` · `declarative-hybrid-framework.md` · `code-framework-mapping.md` · `tech-selection.md` —— ★ 前两份进顶层、后四份进 `mixed/`）⇒ ★ ★ **这是 architect 可写域的实质扩展** ⇒ ★ ⇒ ★ **属角色边界变更 ⇒ MUST 由用户签认**（★ ★ **已于 2026-11-08 签认「批准」⇒ 生效**）
⇒ ★ ★ **这取代了「逐份扩围」**（★ 治理建议 §9.1 A2 的「扩 4 份」）⇒ ★ 一次重构 ⇒ ★ **长期生效** ⇒ ★ **治理成本不再随文档数线性增长**。

### 2.5 ★ 门禁（★ 新增 / 扩展，★ 归 builder）

| # | 判据 | 落点 | ★ 误伤控制 |
|---|---|---|---|
| ① | ★ **顶层纯净 lint**：顶层 + `adr/` 的文档禁含实施态时点语汇 | 新 lint（★ 或并入 `doc_link_check.sh` 检查 3） | ★ ★ **词表 MUST 冻结进脚本**（★ 单一真源 · ★ 禁开放词表）★ + ★ **语法上下文豁免**（★ 状态列 / 图例行 / frontmatter / dated 横幅内**不检**）⇒ ★ ★ **否则 Day-1 必误伤**（★ `adr.md` 状态列词汇 · `roadmap-evolution.md` §0 图例「进行中」· 目标态的既成事实前提句） |
| ② | ★ **链接检查扩围**：`doc_link_check.sh` 检查 2 的 `PATH_CHECK_DOCS` 扩入 `docs/architecture/`（★ 含子目录）⇒ ★ 把 ≥16 处死/错引用收进门禁 | `doc_link_check.sh` | ★ 脚本注释明说全量 `docs/` 噪声过高 ⇒ ★ **只扩 `docs/architecture/`**（★ 18+ 份）⇒ 跑顺后再评估 |
| ③ | ★ **ADR 孤儿检查**：`adr/` ↔ `adr.md` 双向 | `verify_adr.sh` | ★ **实测零误伤**（★ 18/18 有行 · 42 链接 0 broken） |
| ④ | ★ **顶层 ↔ 子目录引用完整性**：顶层文档引用 `mixed/` `archive/` 时 MUST 标明「★ 非权威 · 仅作背景」 | 新 lint（★ 可与 ① 合并） | ★ 低 |

### 2.7 ★ 与既有决策的关系（★ 原 §2.6「迁移执行」已按 ADR 契约（★ 实施步骤 MUST NOT 进 ADR 正文）移出 ⇒ 见 `.rddf/reviews/architecture-docs-governance-recommendations.md` **§10**）

| 既有 | 关系 |
|---|---|
| ★ **ADR-094 D-6 两阶段迁移** | ★ ★ **同构**：★ 文档面同样分两步（★ **混合态下沉** + **顶层净化**）⇒ ★ 两步的先后与门禁 ⇒ **见本 ADR §2.3 的三机制** |
| ★ **ADR-094 治理轮固化于 `hierarchical-plugin-architecture.md` §0.6 的 M-8「真相区 / 实施区不混」** | ★ ★ **本 ADR 是它的仓级化**：★ M-8 只在层次化主题内制度化 ⇒ ★ 本 ADR 把它变成**目录结构 + lint** ⇒ ★ 全仓生效 |
| ★ **ADR-091 / ADR-089 / ADR-083 / ADR-092 / ADR-093** | ★ **决策本体一字不动**（★ 本 ADR 只管文档位置与治理机制） |
| ★ **`docs/architecture/adr.md`** | ★ **不动**（★ 76 处引用 + 它就是「架构决策导航入口」） |

---

## 3. 选项对比表

| 选项 | 结论 | 理由 |
|---|---|---|
| **A. 维持现状 + 逐份扩围 `arch_paths`** | ❌ 不选 | ★ 治理成本**随文档数线性增长**；★ 混合态与真相源**仍同层** ⇒ 「顶层干净」靠自觉 |
| **B. 顶层只留真相源 + 其他全进一个 `archive/`** | ❌ 不选 | ★ **混合态 ≠ 归档**（★ 混合态还没被取代、只是含实施态污染）⇒ ★ 归档 = 只读 ⇒ ★ 混合态进 archive = 宣判它是历史 ⇒ ★ **不准**；★ 且混合态需要**净化路径**（architect 可写）⇒ ★ archive 不满足 |
| **C. 顶层只留真相源 + 决策导航入口；其余按 `mixed/` / `archive/` / `plugin-research/` 三分 + 三机制 + `arch_paths` 物理隔离 + 4 项门禁** | ✅ **采纳** | ★ ① **目录位置本身承载语义**（★ 顶层=权威 ⇒ 读者一眼可辨）★ ② **职责物理隔离**（★ `arch_paths` = 顶层 + `mixed/` ⇒ architect 独占真相源、但能净化混合态）★ ③ **机制可持续**（★ 晋升/下沉/归档 + lint ⇒ 不靠自觉）★ ④ **一次性重构、长期生效**（★ 取代逐份扩围） |
| **D. 新建独立仓 / 独立目录树（★ 如 `docs/arch-truth/`）** | ❌ 不选 | ★ 拆散文档集 ⇒ 跨目录引用激增 ⇒ ★ 断链风险与迁移成本剧增；★ 且与「真相源与决策同处一地、便于交叉引用」相悖 |
| **E. 原地净化 + `README.md` 导航 + lint，★ 不动目录** | ❌ 不选 | ★ ★ **这是「成本最低可逆」的基线**（★ 零断链、零 ≈31 处同步）⇒ ★ 治理收益**大半可得** ⇒ ★ 值得作为对照基线；★ ★ **但它治不了根**：★ 混合态与真相源**仍同层** ⇒ ★ 「顶层一眼可辨」不成立 ⇒ ★ 且 lint 的误伤面**无法靠路径收窄** ⇒ ★ 即便把 4 份已确认可清走的死文档（★ `m4-dse-*` · `performance-guide.md` · `tlm-coverage-parity-ledger.md` · `tlm-retirement-b1-capacity-plan.md`）清走，★ 顶层仍余 **14 份**（★ 含 4 份未净化的混合态）⇒ ★ **仍不满足用户的「顶层干净」裁决** |
| **F. 按主题分子目录**（★ `plugins/` `roadmap/` `verification/` …） | ❌ 不选 | ★ 同样满足「其他文档放入子目录」的字面裁决，★ 且主题导航对读者比纯度导航更友好；★ ★ **但「主题」与「是否真相源」不是同一维度**（★ 例：`roadmap-evolution.md` 既是主题又是真相源 · `error-handling.md` 既非主题也非真相源）⇒ ★ 按主题分 ⇒ **顶层仍混着混合态** ⇒ ★ 用户的裁决是「顶层只留**真相源**与**决策导航入口**」⇒ ★ 按主题分**不满足裁决**；★ ★ **本 ADR 的实质优势正在于「位置 = 治理语义」**（★ 而非位置 = 内容主题） |

---

## 4. 后果与代价

### 4.1 正面

- ★ ★ **「顶层干净」从约定变成结构**：★ 顶层 = 真相源 + 决策导航入口 ⇒ ★ **读者一眼可辨**，★ **不需要读文档头判断可信度**
- ★ ★ **治理成本不再随文档数增长**：★ `arch_paths` 表达**目录级职责**（★ 顶层 / `mixed/` / 归档）⇒ ★ 取代逐份登记
- ★ ★ **真相源不可能被现状污染**：★ lint + 位置双重保证 ⇒ ★ 落实 ADR-094 治理轮固化于主题文档 §0.6 的 **M-8** 的仓级化
- ★ **新文档有明确归属**：★ 「我该写在哪」= ★ 真相源 → 顶层；★ 混合态 → `mixed/`；★ 已取代 → `archive/` ⇒ ★ **无「放不下」的死角**
- ★ **混合态有净化路径**：★ `mixed/` 在 `arch_paths` 内 ⇒ ★ architect 可净化 ⇒ ★ **晋升机制可运转**（★ 否则混合态永远无人能改）

### 4.2 负面 / 代价

- ★ **迁移期断链风险**（★ 最高风险）：★ 移动 10 份文档（★ `mixed/` 5 + `archive/` 4 + 移出 1）⇒ ★ ★ **两个口径**（★ 不可混用）：★ ① **真断链**（★ markdown 链接形态，★ **这才是要改的**）≈ **31 处**（★ 仓外 17 + `adr/` 5 + 留守顶层 9）★ ② **关注度**（★ 仓外唯一 md 文件提及数，★ **分批依据**）= 全部 18 份合计 **287**、★ 其中**移动的 10 份**合计 **95**⇒ ★ ⇒ ★ **缓解 = 按关注度升序分批 + 每批机检 + 每批单 commit 可 revert**
- ★ **owner 协调成本**：★ `archive/` 与 `plugin-research/` 移出 `arch_paths` ⇒ ★ 那些文档的修改**不再由 architect 兜底**（★ 这是**有意的**：★ 归档=只读）
- ★ **顶层 **4** 份须满足晋升条件**（★ `overview.md` 的 dated 实施态快照段 · `interface-design.md` 的「Phase 1 当前状态」段 · `error-handling.md` 与 `background-and-goals.md` 的 C++17 / 死名表述 ⇒ ★ **均属「非目标态内容」⇒ 须先移除或改写**）⇒ ★ **属内容决策** ⇒ ★ 各需相应审查（★ 不与位置调整混批）
- ★ ★ **本 ADR 自身也受本 ADR 约束**（★ 自指）：★ `docs/architecture/adr/` 在 `arch_paths` 内 ⇒ ★ 本 ADR 的**决策文档**也须是**纯 target** ⇒ ★ 本 ADR 正文不含实施态时点语汇 ⇒ ★ ✅ 满足

### 4.3 风险

| 风险 | 缓解 |
|---|---|
| ★ **迁移中断 ⇒ 仓库处于「半搬」状态** | ★ ★ `arch_paths` 更新与移动**同批** ⇒ ��� 无「路径变了但可写范围没变」的空窗；★ 每批可 revert |
| ★ **lint 误伤**（★ 如目标态描述里合法出现「已落地」） | ★ lint 只查**顶层 + `adr/`**（★ `mixed/` `archive/` 豁免）★ 且**首轮 WARN 级**、跑稳再升阻塞 |
| ★ **核心顶层文档 Day-1 误伤**（★ `adr.md` 状态列词汇 · `roadmap-evolution.md` §0 图例「进行中」· 目标态的既成事实前提句 ⇒ ★ 三类均会被词面 grep 命中） | ★ 门禁 ① 配「★ **冻结词表 + 语法上下文豁免**」（★ **不得**用「按路径豁免」⇒ 那会把这两份最核心的顶层文档整份豁出）⇒ ★ 首轮 WARN 级 |
| ★ **`adr.md` 自身的源文档指针指向将下沉的文档**（★ 头部的「源文档」字段 + 维护规则 + 漂移处理流程三处 ⇒ 迁移后必断）⇒ ★ 且与门禁 ④「顶层引用 `mixed/` 须标明非权威」产生**语义张力**（★ 「以源文档为准」vs「标为非权威」） | ★ 处置 = **§10 M3** 的三条改写（★ 指针改 `./mixed/...` + 标「非权威 · 历史派生源」+ 维护规则与漂移流程措辞软化）★ + 长期：★ 仍被依赖的决策段**收编进 `adr/`** ⇒ 源文档指针退役 |
| ★ **根级文档（`AGENTS.md` / `.cursorrules`）以全路径链顶层文档** ⇒ 移动后根级断链 ⇒ ★ 且根级**不在门禁 ① 覆盖范围** | ★ 列入 **§10 M3** 同步清单（★ `AGENTS.md` 的关键文档源表 + `.cursorrules`） |
| ★ **顶层 4 份净化的内容决策与位置调整混在一批** | ★ ★ **分两步**：★ 位置调整**不改内容** ⇒ 净化**单独一步**（★ 各走各自审查） |
| ★ **「archive 只读」被违反**（★ 有人继续改归档文档） | ★ `archive/` 不在 `arch_paths` ⇒ ★ architect 不改；★ owner 侧的纪律靠 §2.5 ④ 的引用完整性 lint 部分约束 |

---

## 5. Verification（可勾选验收判据）

> ★ **AC 权威 = 立项后 change 的 `tasks.md`**（★ 本段为草案，★ 立项后回填对应编号）

- [ ] **V-101** `docs/architecture/` 顶层**只含**真相源架构文档 + `adr.md` + `adr/`（★ **逐份清单核对** ⇒ 无 mixed / 台账 / 计划 / PR 描述 / 虚构文档）⇒ ★ 变异：把 `mixed/` 任一份复制到顶层 ⇒ **MUST RED**
- [ ] **V-102** `mixed/` 与 `archive/` **存在且各含预期份数**（★ 5 / 4）⇒ ★ 变异：把 `mixed/` 一份移到顶层 ⇒ MUST RED
- [ ] **V-103** ★ **顶层纯净 lint 生效**：★ 顶层 + `adr/` 任一文档含实施态时点语汇 ⇒ **MUST 报红**；★ `mixed/` `archive/` 同类文本 ⇒ **MUST 豁免**（★ 变异：把 `mixed/plugin-framework.md` 的过时段粘进 `overview.md` ⇒ MUST RED）
- [ ] **V-104** ★ **链接完整性**：★ `bash tools/doc_link_check.sh` **0 broken**；★ 且检查 2 的覆盖范围**含 `docs/architecture/`**（★ 变异：把某引用改成不存在的路径 ⇒ MUST RED）
- [ ] **V-105** ★ **ADR 孤儿检查生效**：★ `adr/` 每份在 `adr.md` §2 有行；★ `adr.md` 每行本地链接存在 ⇒ ★ 变异：删 `adr.md` 里一行 ⇒ MUST RED
- [ ] **V-106** ★ **`arch_paths` 与目录一致**：★ `arch_paths` = ★ **顶层 9 份 + `adr/` + `mixed/` + `ip/mmu/docs/adr` + `ip/cache/docs/adr`**（★ IP 级 ADR 目录**保留** ⇒ ★ 见 §2.4）；★ `exclude_paths` 含 `archive/`（★ `plugin-research/` 已在）；★ **无死路径**：★ 每条 `arch_paths` 项**实际存在**；★ ★ 且每条 `exclude_paths` 项**实际存在**（★ 移动后旧条目会成死项）⇒ ★ 变异：加一条不存在的路径 ⇒ MUST RED
- [ ] **V-107** ★ **引用迁移完整**：★ **迁移前基线清单**（★ 由实施侧脚本导出并随 change 存档）中的**全部真断链**（★ ≈31 处）**逐条指向新位置**（★ 逐条对基线核销）⇒ ★ 变异：漏改一处 ⇒ MUST RED
- [ ] **V-108** ★ **顶层 4 份已净化**：★ `overview.md` 无 dated 实施态快照段 · `interface-design.md` 无「Phase 1 当前状态」段 · `error-handling.md` / `background-and-goals.md` 无 C++17 / 死名表述 ⇒ ★ 变异：把旧段贴回 ⇒ MUST RED
- [ ] **V-109** ★ **`ip-catalog.md` 已移至 `ip/ip-catalog.md`** 且 16 处引用全部更新 ⇒ ★ 变异：留一份在 `docs/architecture/` ⇒ MUST RED
- [ ] **V-110** ★ **回滚可验证**：★ **首批**（★ `m4-dse-*`）**与最大批**（★ `tlm-coverage-parity-ledger.md`）**两批实测 `git revert`** ⇒ 引用与 `arch_paths` **全部复原**；★ 其余批由「单 commit 可 revert」结构性保证 ⇒ ★ 变异：revert 后仍断链 ⇒ MUST RED

---

## Related

- **ADR-094**（CH_MEM 开放核的层次化装配契约与端口纪律）—— ★ 本 ADR 是其 **M-8「真相区 / 实施区不混」的仓级化**；★ D-6 两阶段迁移在文档面的**同构**
- **ADR-091 v1.16**（CH_MEM 开放核形态与端口架构）—— ★ 决策本体不动
- `docs/architecture/hierarchical-plugin-architecture.md` —— ★ ★ **层次化 Plugin 主题真相源**（★ 顶层 10 份之一 · ★ 其 §0 导航层是本仓唯一的「主题级真相导航」范例）
- `.rddf/reviews/architecture-docs-governance-recommendations.md` —— ★ **诊断 + 治理路线图**（★ §1~§8 诊断 · **§9 路线图** · ★ 本 ADR 是其 §4 / §9 的**决策层**）
- `.rddf/reviews/architecture-docs-governance-summary.md` —— ★ **治理后现状快照**（★ 本 ADR 落地后需重跑）
- `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md` —— ★ 审稿记录（★ §7l~§7n 治理轮）
