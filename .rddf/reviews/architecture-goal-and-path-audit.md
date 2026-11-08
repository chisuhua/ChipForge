# 架构目标—路径—现状盘点（★ 2026-11-08）

> ★ **本文件 = 架构盘点**（★ 「我们要去哪 / 为什么去 / 怎么去 / 现在到哪」）⇒ ★ **不是新 ADR**（★ **不含新决策**）⇒ ★ **不含实施步骤**（★ 实施步骤在 ADR-095 §10 + 各 change 的 `tasks.md`）。
> ★ ★ **口径声明**：★ 本文每条断言**带 `file:line` 或标注「实测」** ⇒ ★ **数字均为 2026-11-08 实测** ⇒ ★ **不引用历史快照**。
> ★ ★ **本轮动机**：★ 用户指令「切 architect 对完成和后续任务进行梳理，检查下我们要实现的目标、为什么要实现这个目标、怎么做、为什么要这么做，目前的任务是什么」⇒ ★ 本文件即该梳理的**可核版本**。

---

## 1. ★ 目标（★ 终态一句话 + 4 支柱）

> ★ **终态**：★ **一个模型（CH_MEM）· 两个后端（CppHDL Simulator / Verilator）· 一道双后端差分 parity 门禁 · 一面干净的真相源文档区**。

| # | 支柱 | 「该长什么样」 | ★ 权威载体 |
|---|---|---|---|
| **①** | ★ **模型单一化** | 业务 IP 只维护**一份** Plugin 代码；`cf::plugin::uint_t<N>` 恒等于 `ch::core::ch_uint<N>`；TLM 模式在阶段 C 删除 | `ADR-083`（单一 source of truth）· `ADR-040`（★ 文件 **v3.0 Accepted**；★ **v2.0 决策**引入「CH_MEM 是新正道」）· `ADR-093`（★ **🚧 Proposed v0.1** · TLM 移除 Phase-2） |
| **②** | ★ **双后端差分证据** | 同一份 elaboration DAG ⇒ CppHDL Simulator 与 Verilator **同 corpus 同 ELF 逐位比对**；「一致性证据」由**计数代理**换成**差分执行** | `ADR-089`（Accepted v1.4）· `ADR-088` v1.4 §10（目标态与三段迁移路径） |
| **③** | ★ **层次化可交付装配** | 层 2 单元成**独立 module** ⇒ 可单独综合 / 单独交付；★ **但不是** parity 门禁输入 | `ADR-094` D-1~D-6（★ **🚧 Proposed v0.1** · 决策已闭合、**关联 change 未立项**）· `ADR-091` v1.16 · 上游 `CppHDL` ADR-058~062 |
| **④** | ★ **真相源文档区**（★ **治理维度** · ★ 非功能维度） | `docs/architecture/` 顶层 = 真相源 + 决策导航入口；实施态内容下沉 / 归档 | `ADR-095`（Accepted v1.0）· `.rddf/config/arch-docs.json` |

★ **支柱间关系（★ 为什么是这 4 条而不是别的）**：★ ①② = **可验证性**（缺了就瞎）· ③ = **可交付性**（做出来了能不能交）· ④ = **可维护性**（架构会不会腐）。★ ①②③ 是**功能主线**（⇒ roadmap objectives/features）· ④ 是**治理主线**（⇒ ADR-095 + 本文件）。

---

## 2. ★ 为什么（★ 每条支柱的问题—代价—不做的后果）

### 2.1 支柱① 模型单一化

| 项 | 内容 | ★ 取证 |
|---|---|---|
| ★ **问题** | 业务 IP 曾分别维护 TLM 副本与 RTL 副本 ⇒ **两份语义、两条漂移路径** | `ADR-083`（Plugin-style 单一 source of truth）· `ADR-040`（★ v2.0 决策） |
| ★ **代价** | ★ TLM 模式是**第三条腿**：它提供亚秒级快环，但**不参与** parity 门禁 ⇒ 保留它 = 长期维护**第三条语义** | `ADR-088` v1.4 §5.2 删除判据 · `ADR-092`（TLM 引用冻结） |
| ★ **不做的后果** | ★ 「删掉 TLM 但没有 parity 门禁」⇒ ★ **一致性证据归零** ⇒ ★ CI 零保护（`architecture-gates.yml` 只跑 8 个脚本、**不构建项目**） | ★ `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:38`（§1.1 表第 6 行） |

### 2.2 支柱② 双后端差分证据

| 项 | 内容 | ★ 取证 |
|---|---|---|
| ★ **问题** | ★ **两个后端跑的是不同 corpus、不同 oracle，从不互比** ⇒ 「两边都 `tohost == 1` 但内部状态不同」的模型**今天完全不可见** | ★ `ADR-089` §Context §1（★ `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md:16-21`） |
| ★ **替代方案（被否决）** | ★ **计数台账**（现状）：数「有没有对应 case 名」⇒ ★ **已在漂**：头部声明 partial **292** vs 条目级实测 **282** | ★ `ADR-089` §Context §2（★ 同文件 `:26`） |
| ★ **代价** | ★ 差分执行**比台账贵**（CH_MEM Simulator vendored 47 ≈ **1594s**）⇒ ★ 需分层 corpus + 单元级快车道 | ★ `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:37`（§1.1 表第 5 行） |
| ★ **不做的后果** | ★ 门禁形同虚设 ⇒ ★ 阶段 C「删 TLM」的前置不成立 ⇒ ★ **目标态整体不可达** | ★ `ADR-089` 阶段 C 5 硬前置 |

### 2.3 支柱③ 层次化可交付装配

| 项 | 内容 | ★ 取证 |
|---|---|---|
| ★ **问题** | 扁平单一 Verilog ⇒ 层 2 单元**不可单独综合 / 不可单独交付** | ★ `ADR-094` D-4（交付定位） |
| ★ **边界（★ 关键）** | ★ **层次化产物 = 交付路径，不是 parity 门禁输入** ⇒ ★ 门禁**仍消费扁平 `top.v`** | ★ `ADR-094` D-4（★ `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md:84` 起） |
| ★ **代价** | ★ 装配契约 + 端口纪律 = **新约束面**（r.1~r.16）⇒ ★ 需 oracle→critic 逐条收敛 | ★ `ADR-094` §Verification V-27~V-43 |
| ★ **不做的后果** | ★ SoC 级交付（v1.1.0~v1.3.0 objectives）**缺装配契约** | ★ `.rddf/roadmap/objectives/objective-soc-cpu-v120-launch.md` 等 3 个 objective |

### 2.4 支柱④ 真相源文档区

| 项 | 内容 | ★ 取证 |
|---|---|---|
| ★ **问题** | ★ 顶层 18 份 md **混**有：台账（`tlm-coverage-parity-ledger.md`）· 已定案计划（`tlm-retirement-b1-capacity-plan.md`）· PR 描述（`m4-dse-*.md`）· **虚构文档**（`performance-guide.md`）· 混合态框架文档（`plugin-framework.md` 等 5 份） | ★ 实测 `ls docs/architecture/*.md` = **18 份** |
| ★ **不做的后果** | ★ 读者把「现状」读成「合理」⇒ ★ 决策漂移无人察觉；★ 且**实施态快照**（`overview.md` 头部 2026-10-08 版）会持续被当权威引用 | ★ `docs/architecture/hierarchical-plugin-architecture.md` §0.1（真相只有两类） |
| ★ **代价** | ★ 移动成本（引用改写）+ 内容净化（逐份过审） | ★ `ADR-095` §2.2 双判据 · §10 M1~M6 |

---

## 3. ★ 怎么做（★ 为什么这么这么做 —— 逐条取舍）

### 3.1 ★ 为什么「一个模型」而不是「两份模型」⇒ ★ **分歧类有界**

★ **差分执行 = 同一 elaboration ⇒ 两后端**：★ 分歧**只可能来自后端** ⇒ ★ **后端 bug 小类**；★ 而「TLM ≡ CH_MEM」比的是**两条 ISA 语义**（M 扩展 radix-2 多周期 vs 组合式 1 拍 · CSR stub vs 实现）⇒ ★ **无界类**。
⇒ ★ **所以选「同模型跨后端」而非「跨模型对比」**：★ 前者把比对面**收敛到可控**；★ 后者会把「建模差异」与「实现 bug」混在一个失败集合里，无法判读。
★ **实证支撑**：★ 2026-10-21 上游 `c27543d` 修 `ch_mem::aread` 1 拍滞后 ⇒ 10 个 rv32ui load/store ELF 失败 ⇒ ★ **那是后端时序不一致，不是建模错误**。
★ 取证：`ADR-089` §Context §3（★ `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md:29-40`）。

### 3.2 ★ 为什么「差分执行」而不是「计数台账」⇒ ★ **度量对象换了**

★ 台账量的是**名字有没有**；★ 差分执行量的是**状态一不一样**。★ ★ **可判定的强弱关系**：★ 「名字都有」是「状态一致」的**必要非充分条件**（★ 台账全绿 ⇒ 仍可能状态不一致）⇒ ★ 反之「状态一致」⇒ 名字必然都有。★ ⇒ ★ 且台账**已漂** ⇒ ★ 保留台账只能当**覆盖边界规格**（阶段 B 冻结），**不能当一致性证据**。
★ 取证：`ADR-089` §Context §2（★ 同文件 `:26`）· `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md` §2.1（★ `:54-64`）。

### 3.3 ★ 为什么「层次化」作为**交付**路径而不是**门禁**路径 ⇒ ★ 两个门禁吃不同产物

| 路径 | 产物 | 用途 | ★ 依据 |
|---|---|---|---|
| 扁平 | `ch::toVerilog(ctx)` → 单 `.v` | ★ **parity 门禁输入** | ★ `ADR-094` D-4 |
| 层次化 | `ch::toVerilogHierarchical(...)` → 单文件多 module | ★ **交付路径**（层 2 可单独综合） | ★ `ADR-094` D-4 · 上游 `ADR-058` v2.17 |

★ **为什么必须分开**：★ 门禁要**单一扁平网表**才能双后端逐位比对；★ 交付要**模块边界**才能单独综合。★ 两者产物形态**互斥** ⇒ ★ 强行合并 = 门禁网表被交付结构污染。
★ 取证：`ADR-094` D-4 · `docs/architecture/overview.md` 头部「两条 RTL 出口」条（2026-11-08 净化时保留）。

### 3.4 ★ 为什么文档面要**分层**（真相源 / `mixed/` / `archive/`）⇒ ★ 治理成本 O(n)→O(1)

| 分区 | 语义 | 谁能写 | ★ 治理动作 |
|---|---|---|---|
| ★ **顶层** | 真相源（架构决策 + 架构文档） | architect | ★ **晋升**（进顶层 MUST 是纯 target） |
| ★ **`mixed/`** | 混合态（有实施态污染） | architect（可净化） | ★ **下沉**（顶层混入实施态 ⇒ 立即下沉） |
| ★ **`archive/`** | 历史 / 台账 / 已定案计划 / PR 描述 / 虚构 | **只读**（owner） | ★ **归档**（被取代 / 失效 / 历史完成） |

★ **为什么不是「逐份扩围 `arch_paths`」**：★ 逐份扩围 = 每新增一份文档都要改一次配置 ⇒ ★ **O(n)**；★ 按目录表达职责 = 一次配置覆盖一类文档 ⇒ ★ **O(1)**。
★ ★ **可判定的口径（★ 不夸大）**：★ 「O(n)→O(1)」指的是**「新增同类文档时是否需再改配置」**这一维度；★ ★ **但仍有残余维护成本**：★ ① `exclude_paths` 与 `arch_paths` 的目录语义须**互斥不重叠**（★ `archive/` 在 exclude、`mixed/` 将进 arch）★ ② 目录内文件**移动到别处**时旧条目会成**死路径**（★ ⇒ V-106 要求「无死路径」检）⇒ ★ **粒度放粗成立，复杂度归零不成立**。
★ 取证：`ADR-095` §2.4（`arch_paths` 改按目录表达）。

### 3.5 ★ 为什么存量走「**就地净化**」而不是「先下沉再晋升」⇒ ★ 移动成本

★ 「下沉」是**移动**（改路径 ⇒ 全仓引用改写 + 历史重排）；★ 「就地净化」是**改写内容**（不动路径）。★ 存量文档**已经在顶层被引用**（`adr.md` 76 · `overview.md` 41 · `interface-design.md` 23 …）⇒ ★ 移动成本远高于净化成本。
★ ★ **口径**：★ **「就地净化」是唯一的存量路径**（★ 下沉路径**只给新增文档**）⇒ ★ 两者并存会造成「同一文档两种处置」的解释歧义。
★ ★ **「就地净化」≠「净化后无条件留顶层」**：★ 存量文档**必须先过晋升检验**才留在顶层（★ **判据 = V-108**）⇒ ★ 净化是**手段**，**晋升检验**才是**门槛**（★ ⇒ 未通过者下沉 `mixed/`）⇒ ★ **故「就地净化」与「顶层只含真相源」不矛盾**。
★ 取证：`ADR-095` §2.3 ②（就地净化）· §5 V-108。

### 3.6 ★ 为什么拆 **2 个** change 而不是合批 ⇒ ★ **审查粒度隔离**

| change | 范围 | ★ 性质 | ★ 为什么能拆 |
|---|---|---|---|
| ★ `architecture-truth-boundary` | 位置调整 + 引用改写（AC-101~AC-108） | ★ **纯机械** | ★ 可批量快跑 |
| ★ `architecture-truth-boundary-gates` | 门禁落地 + 顶层净化（AC-109~AC-115） | ★ **含内容决策** | ★ 需逐份过审 |

★ **合批的坏处**：★ 内容审查（逐份净化）与机械改动（批量移动）混在一批 ⇒ ★ 审查者无法区分「这条 diff 是搬家还是改内容」⇒ ★ 审查失焦。
★ 取证：★ `ADR-095` §5 V-101~V-110 · `.rddf/reviews/architecture-docs-governance-recommendations.md` §11.2 决策点 2。

### 3.7 ★ 为什么 AC-115 必须**自豁免**⇒ ★ 否则判据被自己架空

★ 若 AC-115（「AC ↔ V 机检相等」）**参与**覆盖并集计算 ⇒ ★ 并集**恒等于** V 全集 ⇒ ★ 判据**恒真** ⇒ ★ **零判别力**。
⇒ ★ 判据改为：★ **AC-101~AC-114 的「覆盖 V」并集 == V-101~V-110 全集**（★ AC-115 自身不参与）。
★ 取证：`.rddf/reviews/architecture-docs-governance-recommendations.md` §11 AC-115 行。

---

## 4. ★ 目前的任务是什么（★ 现状快照）

| 域 | ★ 目标 | ★ 当前状态（★ 实测） | ★ 缺口 | ★ owner |
|---|---|---|---|---|
| ★ **支柱② 门禁** | 阶段 A（A1 契约 → A2 观测量 → A3 语料首跑 → A4 门禁接线） | ★ A1 `parity-gate-spec` **32/32** ✓ · A2a `backend-parity-observability` **29/29** ✓ · A2b `backend-parity-harness` **25/26**（★ 1 项归 A3）· A3 `backend-parity-corpus` **23/25**（★ 2 项待跑批/待归 A4）· ★ **A4 门禁接线 + CI = 未立项** | ★ ★ **A4**（门禁接线 + 分层 corpus + CI）+ ★ `backend-parity-open-core` **40/61**（★ 21 项：★ 含 **A5-2 ELF 加载** / **A5-3 核↔从设备集成** = ★ **真实缺口**） | builder |
| ★ **支柱① 退役** | 阶段 B 台账冻结 → 阶段 C 删 TLM | ★ ★ **阶段 C 未启动**（★ 5 硬前置含「① 差分门禁已上线且全绿」⇒ **被 A4 卡住**）★ ★ **阶段 B 未启动但不被 A4 卡**（★ 前置④ 台账冻结 = 纯规格工作） | ★ **C 依赖 A4**；★ **B 可与 A4 并行** | builder |
| ★ **支柱③ 层次化** | 装配契约落地 + 首批成层单元 | ★ ADR-094 = 🚧 **Proposed v0.1**（★ 决策已闭合，★ **关联 change 未立项** ⇒ 未转 Accepted）· ★ **2 个 change 不存在** | ★ ★ **立项**（`chmem-hierarchical-assembly` / `chmem-hierarchical-unit-landing`） | planner |
| ★ **支柱④ 文档** | 顶层干净 + 4 道门禁 | ★ ADR-095 **Accepted v1.0** ✓ · ★ **顶层仍 18 份**（★ 目标 9 份）· ★ `mixed/` `archive/` **不存在** · ★ `README.md` **不存在** · ★ 顶层 4 份**已净化**（★ 2 份改写 + 2 份订正块）· ★ `arch_paths` **16 条**（★ 文件级；★ 目标目录级） | ★ ★ **M1~M6 + 4 道门禁**（★ 全未执行） | planner → builder |
| ★ **SoC 交付** | v0.11.0 → v1.3.0 | ★ 5 个 objective 均 `active` · ★ `mmu-chmem-pipeline-integration` **29/173** · ★ `mmu-chmem-phase-b-c-launch` **10/33** · ★ `chmem-bp-btb-gshare` **4/45**（★ defer v1.0.0）· ★ `phase-1.5-wave-4` **0/51**（★ placeholder）· ★ `vexii-riscv-parity-poc` **0/74**（★ v1.0.0 远期） | ★ 见 §6 任务总表 | builder |

★ ★ **实测命令**（★ 可复现）：★ `for d in openspec/changes/*/; do grep -c "^\s*- \[x\]" $d/tasks.md; done` · ★ `ls docs/architecture/*.md | wc -l` = **18**。

---

## 5. ★ 实测偏差清单（★ 公开账核对 —— 本轮最重要的产出）

### 5.1 ★ 四处「摘要 / 既有表述 vs 实测」硬偏差

| # | 对象 | ★ 既有表述 | ★ **实测（2026-11-08）** | ★ 后果 |
|---|---|---|---|---|
| **B-1** | ★ **ADR-094 状态** | ★ 「Accepted v1.16 · 620 行」 | ★ **`🚧 Proposed v0.1` · 180 行**（★ `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md:3` + `wc -l`） | ★ ★ **重大**：★ ADR-094 **未转 Accepted** ⇒ ★ 不得对外宣称「已 Accepted」；★ 其 `:3` 自陈「仅剩治理收口（关联 change 立项 + AC 映射回填）⇒ ADR 完成的标志第 4 条未满足」⇒ ★ 文档**自洽**，是**外部摘要**错 |
| **B-2** | ★ **ADR-095 行数** | ★ 「196 行」 | ★ **199 行** | ★ 无害（快照漂移），但**摘要不得复用旧数字** |
| **B-3** | ★ **「真断链 ≈31」** | ★ ≈31 处 | ★ **29**（★ 口径 = 全仓 md **链接形态** 且 basename ∈ 待移动集）· ★ **oracle 独立复算得 46 处链接 / 23 个唯一文件** ⇒ ★ **口径未锁定** ⇒ ★ 当前**已断链 = 0** | ★ ★ **三个口径互斥**（★ 29 / 46 / 旧 31）⇒ ★ **基线导出须以脚本实测为准**（★ M2.5）⇒ ★ **旧数字与本文 29 均不得作判据** |
| **B-4** | ★ **`adr/` 份数** | ★ governance-summary 记 18 | ★ **19**（★ 含 ADR-095） | ★ summary 快照过期 |

### 5.2 ★ 一处**内部契约冲突**（★ 比数字偏差更严重）

| # | 冲突 | ★ 证据 | ★ 处置 |
|---|---|---|---|
| **B-5** | ★ **「AC 权威 = change 的 `tasks.md`」** ★ **与** ★ **verifier 判据来源 + openspec 实际惯例** ★ **不一致** | ★ ① ★ **成立**：★ `ADR-095:177`「AC 权威 = 立项后 change 的 `tasks.md`」+ ★ `ADR-094:13`「权威 = change 的 `tasks.md` 的 AC 清单」★ ② ★ **成立**：★ **verifier 的 AC 提取段头正则** = `^##\s+(?:验收标准\|Acceptance Criteria)\s*$`（★ `rdd-verifier/SKILL.md:153`）★ 且提取源 = `openspec/changes/<name>/proposal.md`（★ 同文件 `:148`）★ ③ ★ **部分成立（★ 数字须订正）**：★ ★ **10 个有 `tasks.md` 的 change 中，9 个**在 `proposal.md` 有 Acceptance 段（★ 全仓口径：`## Acceptance` **7** + `## Acceptance (待 wave3 后细化)` **1** + `## Acceptance Criteria` **2** = **10**；★ ★ **但其中 `parity-observation-ambiguity-fail-closed` 无 `tasks.md`** ⇒ ★ **落在「有 tasks.md」子集内的是 9**）★ ⇒ ★ **`mmu-chmem-pipeline-integration` 有 `tasks.md` 但 `proposal.md` 无 Acceptance 段**（★ `:21-205` 仅 Why/What Changes/Tasks）★ ⇒ ★ **反向佐证**：★ `parity-observation-ambiguity-fail-closed` **有 `## Acceptance`（`:98`）但无 `tasks.md`**（★ 目录仅 `proposal.md`）★ ⇒ ★ **AC 不在 `tasks.md`**★ ④ ★ **★ 「写入死锁」表述过强（★ 订正）**：★ planner 权限含起草 `proposal.md`（★ 角色矩阵）⇒ ★ **AC 有处可写**；★ ★ **真实问题是「权威口径冲突 + 段头命名不统一」** ⇒ ★ **且更严重**：★ ★ 现有 7 个 `## Acceptance` 段 ★ **提取不到**（★ 正则只认 `Acceptance Criteria` / `验收标准`）⇒ ★ **即使权威订正落地，段头仍须统一** | ★ ★ **口径订正（待 architect 下一轮走 oracle→critic）**：★ **AC 权威 = change 的 `proposal.md` 的 Acceptance 段**；★ **`tasks.md` 的 checklist = 执行镜像**（★ builder 落 · ★ AC 编号一一对应）⇒ ★ **与 verifier 提取源一致** + **解开口径冲突**。★ ★ **A-2 范围必须含「段头规范统一」**：★ 立项时一律用 ★ **`## Acceptance Criteria`**（★ 或 `## 验收标准`）★ ⇒ ★ **否则 verifier 提取为空**。★ **本轮不擅自改已签批 ADR**（★ 列入 §6 任务总表 A-2） |

### 5.3 ★ ADR-095 §2.2「关注度」数字**不可复现**（★ ⇒ 分批依据失效）

★ **问题**：★ §2.2 的关注度列是**分批依据**（AC-102 的升序），但**四种口径复算均对不上**：

| 口径 | `adr.md` | `overview.md` | `interface-design.md` | `parity-gate-contract.md` | 对齐数 |
|---|---|---|---|---|---|
| ★ **A** 排除 `.rddf/` · 文件提及 | 63 | 37 | 20 | 14 | ★ 1/9 |
| ★ **B** 含 `.rddf/` · 文件提及 | 79 | 42 | 24 | 22 | ★ 2/9 |
| ★ **C** 排除 `.rddf/` · md 链接路径提及 | 4 | 16 | 11 | 0 | ★ 0/9 |
| ★ **D** 含 `.rddf/` · md 链接路径提及 | 4 | 17 | 12 | 0 | ★ 0/9 |
| ★ **ADR-095 §2.2 表载** | **76** | **41** | **23** | **22** | — |

★ ★ **排除集声明**：★ 上表四口径的「仓外」= **全仓 `*.md` 减去 `docs/architecture/` 顶层 18 份自身**；★ A/B 差异 = **是否含 `.rddf/`**；★ C/D 差异 = 同上（且按**链接路径**计数）；★ **oracle 另测 5 种口径（含「仓外提及」「链接形态全仓排 build」）无一能复现** ⇒ ★ **口径确实未锁定**。
★ **连带失效**：★ §2.2 裁决规则写「该留顶层 **2~77**」·「该下沉/归档 **3~22**」⇒ ★ 按口径 B 实测下沉组最大 = **24**（`plugin-framework.md`）· 归档组最大 = **31**（`tlm-coverage-parity-ledger.md`）⇒ ★ **两条区间都越界**。
★ ★ **两处内部互斥（★ 比「对不上外部口径」更硬）**：★ ① **同一文件内**：★ `ADR-095:95`（§2.3② 移动成本）写 **40 / 22 / 7 / 2**★ ★ 而 `ADR-095:74-79`（§2.2 表）载 **41 / 23 / 6 / 2**（★ `overview.md` / `interface-design.md` / `error-handling.md` / `background-and-goals.md`）⇒ ★ **同一 ADR 自相矛盾**；★ ② **跨文件**：★ `.rddf/reviews/architecture-docs-governance-recommendations.md:274`（AC-102 分批升序）用 **2/3/3/9/10/12/13/18/22** ⇒ ★ 与 §2.2 表载值**逐项不一致**（★ 如 `plugin-framework` 13 vs 14 · `ip-catalog` 18 vs 16 · `testing-and-dse` 9 vs 4）。
★ ★ **处置（★ 双重处置并行 · ★ 二选一不够）**：★ **① 重测并锁定口径**（★ 建议口径 = **B**（含 `.rddf/` · 文件名提及）⇒ ★ 最接近且可由脚本确定性复现）⇒ ★ **② 同时降级**：★ §2.2 的数字改标「★ **分批先验 · 不可作判据**」⇒ ★ **③** AC-102 的分批顺序**改用 M2.5 实测基线**（★ 脚本输出）⇒ ★ **不由历史快照决定**。
★ ★ **为何两者都要（★ oracle 已论证）**：★ **只降级不重测** ⇒ ★ AC-102 分批升序**无准绳**；★ **只重测不降级** ⇒ ★ 已签批 ADR 内的**错数字仍会被后续轮次当判据引用**（★ 本轮已实证其害）。
★ ★ **本轮不改**（★ 改已签批 ADR 的决策内容须走 oracle→critic）⇒ ★ 列入 §6 任务总表 **A-1**。

### 5.4 ★ 其它已核为**一致**的项（★ 不重复劳动）

| 项 | 实测 | ★ 与既有表述 |
|---|---|---|
| ★ `docs/architecture/` 顶层 md | **18** 份 | ★ 与 §2.1 目标（9 份）差 9 份 ⇒ ★ **M1~M6 待做** ✓ |
| ★ `adr/` | **19** 份 | ★ 与 ADR-095 §2.1「19 份」✓ |
| ★ IP 级 ADR | **35 + 1 = 36** 份 | ✓ |
| ★ `arch_paths` | **16 条** · **0 死路径** | ✓（2026-11-08 补登 6 份后） |
| ★ 顶层 4 份净化 | ★ 已做（2 改写 + 2 订正块） | ✓ · ★ 剩余 = **AC-109c** |
| ★ `verify_adr.sh` / `doc_link_check.sh` | 0 STALE / 0 FAILED / 0 broken | ✓ |

---

## 6. ★ 后续任务总表（★ → planner 的直接输入）

### 6.1 ★ architect 侧（★ 下一轮）

| # | 任务 | 前置 | ★ 审查 |
|---|---|---|---|
| ★ **A-1** | ★ **ADR-095 §2.2 关注度重测 + 口径锁定 + 数字降级标注**（★ 数字不可复现 ⇒ 分批依据失效；★ 含 §5.3 两处内部互斥修正） | — | ★ oracle→critic（★ 改已签批 ADR 决策内容） |
| ★ **A-2** | ★ **AC 权威口径订正 + 段头规范统一**（★ B-5）⇒ ★ ① `ADR-095:177` + `ADR-094:13` 改「权威 = `proposal.md` 的 Acceptance 段」★ ② ★ **段头一律用 `## Acceptance Criteria`**（★ 现 7 个 `## Acceptance` 段 **verifier 提取为空** ⇒ 须在既有 10 个 change 内统一 + 新立项用规范段头） | A-1 同批更省预算 | ★ oracle→critic |
| ★ **A-3** | ★ **ADR-094 治理收口**（★ 关联 change 立项后回填 AC 映射 ⇒ 转 Accepted） | ★ planner 立项 2 个 change | ★ 审稿指针 + §Related 更新 |
| ★ **A-4** | ★ 层次化落点登记（★ **归 planner** ⇒ ★ 与 P-5 同一任务，**architect 不重复登记**） | — | ★ 见 §6.2 **P-5** |

### 6.2 ★ planner 侧（★ 本轮下一步）

| # | 任务 | 产物 | ★ 依赖 |
|---|---|---|---|
| ★ **P-1** | ★ **立项 `architecture-truth-boundary`** | `openspec/changes/<name>/proposal.md`（★ **含 `## Acceptance Criteria` 段 = AC-101~AC-108**）· ★ `design.md` + `tasks.md` **待 builder** | ★ **软依赖 A-2**（★ AC 权威口径 + 段头规范）⇒ ★ **不阻塞立项**（★ 段头规范可由 builder 在 tasks 落定前统一），★ 但**最终态须一致** |
| ★ **P-2** | ★ **立项 `architecture-truth-boundary-gates`** | 同上（★ AC-109~AC-115） | ★ P-1 |
| ★ **P-3** | ★ **立项层次化 2 个 change**（★ `chmem-hierarchical-assembly` / `chmem-hierarchical-unit-landing`） | `proposal.md` + AC | ★ ADR-094 §Decision |
| ★ **P-4** | ★ **立项 A4 门禁接线 + CI**（★ `backend-parity-ci`） | `proposal.md` + AC | ★ **★ 关键路径**：★ 阶段 C（删 TLM）的硬前置 |
| ★ **P-5** | ★ **层次化落点登记进 `objective-soc-cpu-v0110-launch.md`**（★ = A-4，★ **单一登记点在此**） | 改 `.rddf/roadmap/objectives/objective-soc-cpu-v0110-launch.md` | — |

### 6.3 ★ builder 侧（★ 依序）

| # | 任务 | ★ 判据 |
|---|---|---|
| ★ **B-1** | ★ **M2.5 引用基线导出**（★ **每批移动「之前」执行**） | ★ 基线清单随 change 存档 |
| ★ **B-2** | ★ **M1~M6 迁移执行**（★ `git mv` · 关注度升序 · 每批一个 commit） | ★ 顶层 = 9 份 · `mixed/` 5 · `archive/` 4 |
| ★ **B-3** | ★ **门禁 ①~④ 落地** | ★ ①顶层纯净 lint（★ 冻结词表 + 语法上下文豁免）②`doc_link_check` 扩围 ③ADR 孤儿检查 ④子目录引用标非权威 |
| ★ **B-4** | ★ **AC-109c**（★ `overview.md` 正文 8 处 + `interface-design.md:305`） | ★ 须单独过审查 |
| ★ **B-5** | ★ **A5-2 ELF 加载 + A5-3 核↔从设备集成**（★ `backend-parity-open-core` 的**真实缺口**） | ★ RED+GREEN |
| ★ **B-6a** | ★ **阶段 B：台账冻结为覆盖边界规格**（★ 前置④） | ★ `ADR-088` §10.3 ④ · ★ **实质判据 = `ADR-089` V-6**（★ 冻结脚本须新增「头部计数 == 条目级实测」等式校验，★ 且**脚本 PASS 本身不构成证据**）· ★ **不被 A4 卡**（★ 可并行） |
| ★ **B-6b** | ★ **阶段 C 前置③：TLM 参考实现语义抽规格**（★ BP GShare 语义 · M 扩展 radix-2 · CSR stub · MMU PTW walk ⇒ 当前唯一可参照实现） | ★ `ADR-088` §10.3 ③（★ 同文件 `:299`） |
| ★ **B-6c** | ★ **阶段 C 前置⑤：门禁体系自身改版**（★ `check_plugin_portability.sh` Check 2/5/6/7/13 全部建立在**双模前提** ⇒ 单模后须改） | ★ `ADR-088` §10.3 ⑤（★ 同文件 `:301`）· ★ **载体 = `ADR-093`（Phase-2）** |
| ★ **B-6d** | ★ **阶段 C：删 TLM 模式** | ★ 5 硬前置全绿（★ `ADR-088` §10.3） |
| ★ **B-6e** | ★ **A3 收尾**（★ `backend-parity-corpus` 剩 2 项）：★ 5.1 BP 增量 3 的量收益跑批（★ **被架构签批卡住**）· ★ 5.2 S-12「commit message 引 ADR-089」机检（★ **归 A4**） | ★ `openspec/changes/backend-parity-corpus/tasks.md:76-78` |

### 6.4 ★ **关键路径**（★ 决定顺序的依赖链）

```
★ A-2（AC 权威口径 + 段头规范）┄┄▶（★ 软依赖）
   └→ P-1 architecture-truth-boundary ──→ B-1 M2.5 基线 ──→ B-2 M1~M6
                                                          └→ B-3 门禁 ①②③④
★ P-3 层次化立项 ──→ A-3 ADR-094 治理收口（★ 独立可并行）

★ P-4 A4 门禁接线 + CI ★★★ ──┬──→ B-6d 阶段 C（删 TLM）
★ B-6a 台账冻结（阶段 B） ══════╪══╗      │
★ B-6b 语义抽规格 ══════════════╪══╣      │
★ B-6c 门禁体系改版 ════════════╪══╣      │
★ B-6e A3 收尾 ══════════════════╪══╝（经前置①）│
★ B-5 A5-2/A5-3（open-core 主线）═════════════╝（★ 与 A4 **并行**，非其前置）
```

★ ★ **两条独立主线**：★ **文档治理线**（A-2 → P-1 → B-1 → B-2 → B-3）与 ★ **功能主线**（P-4 → B-6d）★ **可并行**（★ ★ **理由 = 不同文件面**（★ 仅 `tools/` 有弱交叉：★ B-3 门禁② 改 `doc_link_check.sh`）· ★ **实施 owner 同为 builder** ⇒ ★ **不构成串行瓶颈**）。
★ ★ **阶段 B 不被 A4 卡**：★ B-6a（前置④）依据 = `ADR-089` 明写「阶段 B（与阶段 C **并行**）」；★ ★ **B-6b / B-6c 是阶段 C 的前置③⑤**（★ 本就不依赖 ①②）⇒ ★ 三者均可与 P-4 并行。
★ ★ **B-5（A5-2/A5-3）不是阶段 C 的硬前置**（★ 5 硬前置逐条核实无此项）⇒ ★ **独立并行**。

---

## 7. ★ 给 planner 的三个待确认项（★ 沿 ADR-095 §11.2）

| # | 决策点 | ★ architect 的建议 |
|---|---|---|
| 1 | ★ change 名 | ★ `architecture-truth-boundary`（★ 位置 + 引用）· `architecture-truth-boundary-gates`（★ 门禁 + 净化） |
| 2 | ★ **是否拆 2 个 change** | ★ **拆**（★ §3.6 的审查粒度隔离理由） |
| 3 | ★ **AC 权威归属** | ★ ★ **【本轮新发现 ⇒ 需修订】** ★ **权威 = `proposal.md` 的 `## Acceptance` 段**（★ 非 `tasks.md`）⇒ ★ 理由见 §5.2 B-5 |

---

## 8. ★ 遗留清扫（本轮范围内）

| 检查项 | 结果 |
|---|---|
| ★ 本文件内 TODO / 半截实现 / 临时日志 / 被 skip 的测试 | ★ **无** |
| ★ 本文件内未核实的数字 | ★ **无**（★ 每条带实测来源；★ §5.1 / §5.3 明确标注「既有表述 vs 实测」） |
| ★ 本轮是否新增门禁 | ★ **无**（★ 门禁脚本属 builder 面） |
| ★ 是否改动已签批 ADR | ★ **无**（★ §5.2 / §5.3 的订正**列入 §6.1 待办**，★ 不在本轮擅改） |
| ★ ★ **本轮审查处置** | ★ **oracle 第 1 轮**：`PASS-WITH-NOTES` · **0 BLOCKER** · ★ **6 SHOULD_FIX + 9 NIT** ⇒ ★ **全部已修**★ **critic 第 1 轮**：`APPROVE_WITH_CONDITIONS` · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** · ★ **1 SHOULD_FIX + 4 NIT** ⇒ ★ **全部已修** |
| ★ ★ **本轮抓出并修净的 4 处自身错误** | ★ ① 「11 个 change」off-by-one（★ critic 抓出 ⇒ **10 个有 tasks.md / 9 个有 Acceptance 段**）★ ② §6.4「B-5 → 阶段 C」**无出处依赖边**（★ oracle 抓出 ⇒ 删）★ ③ A-4 交叉引用「见 B-1」错位（★ oracle 抓出 ⇒ 「见 P-5」）★ ④ 「11 个 change」但自身分项加总 = 10（★ 文档内自相矛盾 ⇒ oracle 抓出 ⇒ 订正） |
| ★ ★ **oracle 抓出的「verifier 段头正则」发现** | ★ 7 个 `## Acceptance` 段 ★ **verifier 提取为空**（★ 正则只认 `Acceptance Criteria` / `验收标准`）⇒ ★ 已登记为 **A-2** 的必含范围（★ **段头规范统一**）⇒ ★ **本轮立项的 change MUST 用规范段头** |
