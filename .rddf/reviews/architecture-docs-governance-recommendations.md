# `docs/architecture/` 文档治理建议（★ 待决策材料 · 2026-11-08）

> ★ **本文件 = 治理建议材料**（★ **不是**架构决策、★ **不是**真相区内容）⇒ ★ 落 `.rddf/reviews/`（★ 过程/建议载体）★ ⇒ ★ **权威真相**在 ADR / 架构文档；★ **落地与否待治理决策**。
> ★ **诊断来源**：★ oracle 治理诊断轮（★ 材料轮 · 2026-11-08）· ★ **落地裁决**：architect。
> ★ **口径前提**（★ 用户 2026-11-08 裁决）：★ **真相 = 架构决策文档 + 架构文档（目标态）**；★ **代码 = 朝目标实施的过程态**。

---

## 1. ★ 三个最严重的治理问题（★ 排序 + 依据）

| # | 问题 | 依据 | 状态 |
|---|---|---|---|
| **①** | ★ **17 份非 ADR 架构文档零全局索引** + ★ `arch-docs.json` **无执行脚本、无读者** ⇒ ★ 治理机制建了但**不可达** | ★ `adr.md` 只收 ADR（60 行）；★ `ip-catalog.md` §相关文档 共 4 个 bullet、★ 其中属 `docs/architecture/` 的**仅 2 份**（★ overview + interface-design，★ 另 2 份是 SoC 指南与 roadmap）；★ 层次化 §0 自述只导航本主题；★ 全仓 grep `arch_paths` **无脚本引用** | ⚠️ **待决策**（§4 选项 1） |
| **②** | ★ **第一真相文档（`overview.md`）头置 stale 实施态快照** ⇒ ★ 违反「真相=目标态」；★ 且该纪律**全仓仅 1 份文档制度化** | ★ `overview.md:3`（2026-10-08 ⇒ 1 个月 stale）；★ 层次化 §0.1 + M-8 是唯一样板 | ⏳ **过渡处置（本轮已加 dated 定性横幅 + 指针）** ★ ★ **但首屏污染仍在 ⇒ 拆分待决策** |
| **③** | ★ **索引完整性零机检** ⇒ ★ 全景表条目失效 / ADR 孤儿 / 统计漂移 **均不会被发现** | ★ `doc_link_check.sh`：★ 检查 2 **仅根级 3 文档** · 检查 1 只认 markdown 链接 · `SCAN_PATHS` 不含 `.rddf/`；★ M-4 自证「零机检覆盖」 | ⚠️ **待 builder**（★ §5 清单；★ ★ 但 #3 / #6 依赖 frontmatter 先行〔⇒ 选项 1〕· #4 依赖选项 3 的 A · #5 需 architect 先做显式标注） |

---

## 2. ★ 文档类型分类法（★ 8 类 · 建议作仓库级规范）

| 类 | 定义 | 本仓实例 |
|---|---|---|
| ★ **target** | 目标态（★ 理想架构） | `parity-gate-contract.md` · `background-and-goals.md` · `error-handling.md` · `performance-guide.md` |
| ★ **mixed** | ★ 目标态与实施态**混在一份** | ★ **7 份（39%）**：`overview.md` · `interface-design.md` · `code-framework-mapping.md` · `tech-selection.md` · `plugin-framework.md` · `declarative-hybrid-framework.md` · `testing-and-dse.md` |
| ★ **historical** | 历史态（已归档/已被取代） | `m4-dse-cpufactory-real-pr-description.md` |
| ★ **index** | 索引层 | `adr.md` · `ip-catalog.md` |
| ★ **ledger** | 台账 | `tlm-coverage-parity-ledger.md` |
| ★ **plan** | 规划 / 演进计划 | `roadmap-evolution.md` · `tlm-retirement-b1-capacity-plan.md` |
| ★ **research** | 研究态 | `plugin-research/external-plugin-architectures.md` |
| ★ **reference** | 参考 / 映射（★ 锚代码路径 ⇒ 天然随上游漂移） | `code-framework-mapping.md`（★ 可兼属 mixed） |

★ ★ **「兼属」规则**：★ 分类法允许一份文档兼属两类（★ 如 `code-framework-mapping.md` = reference 兼 mixed）⇒ ★ **兼属时取主类（首个）**为 `doc_type` ⇒ ★ **门禁 #6 的 mixed 豁免判定才有确定依据**（★ 否则非单值 ⇒ 豁免歧义）。

★ ★ **「mixed」是问题本身（★ 与「真相=目标态」口径冲突）**：★ 混合态文档 MUST 二选一 ——
★ **(a) 内部分区**（★ 目标态导航 + dated 实施态快照 ⇒ ★ `hierarchical-plugin-architecture.md` 是**范例**）
★ **(b) 逐出真相区 + 头部横幅**（★ `declarative-hybrid-framework.md` 模式）。

### 2.1 逐份处置建议（★ 混合态 7 份）

| 文档 | 建议 | 理由 |
|---|---|---|
| `overview.md` | ★ **拆**（★ 快照段移出 ⇒ 归 `roadmap-evolution.md` / `STATUS.md`） | ★ 第一真相文档被实施态污染代价最大；★ 快照已 1 个月 stale；★ **本轮已先加 dated 定性横幅**（★ 整段移出需用户点头）★ ★ **但横幅 ≠ 终态**：★ `overview.md` 是读者最可能首读的架构总览，★ 第 3 行起即 10+ 行实施态清单 ⇒ ★ **横幅只防「引用」，不防「首屏误导」** ⇒ ★ **首屏污染仍在** ⇒ ★ 拆分仍待决策 |
| `testing-and-dse.md` | ★ **拆**（★ `§当前实际状态` 段 dated 横幅或移出） | ★ 2026-06-17 实证已 5 个月 stale；★ **本轮已加 dated 定性横幅** |
| `interface-design.md` | ★ **分区 + 注** | ★ 目标是 Bundle 形态主契约，当前态占比小；★ **本轮已加 dated 定性横幅** |
| `code-framework-mapping.md` | ★ **定性为 reference**（★ 非真相区） | ★ 它锚**代码路径** ⇒ 天然随上游漂移 ⇒ 不适合作目标态；★ 保留其自带「版本敏感性声明」 |
| `tech-selection.md` | ★ owner 加 dated 注 + §8 补 TLM 退役行 | ★ 残留 ⑯ 已登记 |
| `plugin-framework.md` | ★ owner dated 注 + backlog 整体重写 | ★ 残留 ⑮；★ 局部改写治标不治本（★ 该文自证 Phase 0/1 历史态） |
| `declarative-hybrid-framework.md` | ★ **维持逐出真相区** + 头部横幅明写「背景参考」 | ★ 已是最优处置；★ 补个正式分类标签即可 |

---

## 3. ★ 索引层分工（★ 现状 4 层并存 ⇒ 建议明确边界，★ 不新增第 5 层）

| 层 | 管什么 | 状态 |
|---|---|---|
| `adr.md` | ★ **决策索引**（★ 只收 ADR · 60 行 · ADR-001~094 跨仓库） | ✅ 职责清楚（★ 但 §2.4 统计口径脱节 ⇒ 见 §4 选项 3） |
| `hierarchical-plugin-architecture.md` §0 | ★ **该主题的导航**（★ 真相全景表 + 阅读路径 + 权威分布 + M-1~M-8） | ✅ 职责清楚 |
| `ip-catalog.md` §文档层次导航 | ★ **IP 级**文档树（★ 管 `ip/<name>/docs/`）⇒ ★ 与 `docs/architecture/` **无重叠** | ✅ 职责清楚 |
| 各 ADR §Related | ★ 反向链接 | ✅ 残留 ⑨ 已闭合 |

★ ⇒ ★ **建议**：新增 `docs/architecture/README.md` 作**目录级编目**（★ 只做「类型 / 状态 / owner / 时效 + 一句职责」⇒ **不做主题导航** ⇒ 避免第 5 个索引层）★ ⇒ ★ **前置条件 = frontmatter 落地**（★ §4）⇒ ★ 否则 README 的总表会立刻腐化。

---

## 4. ★ 需治理决策的 3 项（★ 选项 + 代价）

### 选项 1：建不建 `docs/architecture/README.md`？

| | 收益 | 代价 |
|---|---|---|
| ★ **建** | ★ 闭合「17 份零全局索引」缺口；★ 给 `arch-docs.json` 一个**读者**；★ 分类法 + 元数据 + 纪律的**唯一落点** | ★ 1 份**生成式**文档（★ 总表 MUST 由 frontmatter 派生 + 机检相等 ⇒ 否则会腐）+ ★ frontmatter 标注一轮 ~18 份 |
| ★ **不建** | 零成本 | ★ 缺口维持 ⇒ 读者定位文档靠 `ls` 本能；★ 治理机制继续「不可达」 |

★ ★ **architect 倾向**：★ **建**（★ 但 MUST 生成式 + 前置 frontmatter 落地）⇒ ★ 需你点头。

### 选项 2：`arch_paths` 扩不扩（★ 治理盲区）

★ **现状**：18 份架构文档中**仅 7 份**在 `arch_paths` 内；★ 其余 **10 份既不在范围内也不在 `exclude_paths`**（★ 唯一被 `exclude_paths` 覆盖的是 `tlm-coverage-parity-ledger.md`）⇒ ★ 治理盲区。
★ ★ **本轮已做**：★ 删除死路径 `ip/cpu/docs/adr`（★ 该目录不存在）★ 并在 `description` 写明「无执行脚本 ⇒ 可写范围是无闸约定」。

| | 收益 | 代价 |
|---|---|---|
| ★ **扩入 5 份相关文档**（`plugin-framework` / `tech-selection` / `code-framework-mapping` / `error-handling` / `background-and-goals`） | ★ architect 可直接修 ⇒ 残留 ⑮~⑱ 闭合；★ 治理盲区收敛 | ★ architect 可写面扩大 ⇒ 需重审边界（★ 且这些文档多处是**历史态** ⇒ 写了会不会「用新决策改历史文档」？） |
| ★ **扩 2 份**（★ `error-handling.md` + `background-and-goals.md` —— ★ **目标态**且仅缺 dated 注） | ★ 这 2 份的残留 ⑱ 可由 architect 直接闭合；★ 不触碰历史态文档 | ★ 可写面小幅扩大（★ 需重审边界） |
| ★ **维持现状走 owner 注记** | 边界不变 | ★ 长期双轨（★ 4~5 份文档长期 stale ⇒ 读者读到过时目标态） |

★ ★ **architect 倾向**：★ **区分对待** —— ★ `error-handling.md` / `background-and-goals.md`（★ **目标态** + 仅缺 dated 注）**值得扩**；★ `plugin-framework.md` / `code-framework-mapping.md` / `tech-selection.md`（★ **历史态 / 映射态**）**不值得扩**（★ 扩了也是让 architect 去改历史文档 ⇒ 正确处置是 dated 注 + 整体重写归 backlog）⇒ ★ **需你点头「扩 2 份」这个口径**（★ 上表已补该栏）。★ ★ **2026-11-08 收口深挖后上调为「扩 4 份」**（★ 新增 `performance-guide.md`〔大面积虚构 ⇒ 需整体标历史态〕· `ip-catalog.md`〔索引层〕⇒ ★ **完整清单见 §9.1 A2**）⇒ ★ **本表「扩 2 份」栏为第一轮口径**，★ ★ **以 §9.1 A2 的 4 份清单为准**（★ 算术：2 + 2 = 4）。

### 选项 3：`adr.md` §2.4 统计口径怎么收？

★ **诊断（★ 本轮实测，比原残留 ⑧ 更深）**：★ §2.4 统计表（**合计 48** / A~M 十三类）与 §2.1 主表（★ **实测 41 行** / 另一套类别词表「Plugin / 验证基建 / 架构治理 / IP 架构…」）★ **是两套体系且已脱节** ⇒ ★ ★ **按主表派生统计这条路当前走不通**（★ 词表不同）⇒ ★ ★ **补登 ADR-087~094 会固化第二套分类** ⇒ ★ **architect 不单方面补登**。★ ★ **另实测到两层更深的账实不符**：★ ① `adr.md` **自身**节标题与行数对不上（★ §2.1 标题「40 条」/ **实测 41 行** · §2.3 标题「9 条」/ **实测 18 行**）⇒ ★ **统计与标题两处都不自洽**；★ ② §2.4 类别表有 **M 类（IP 架构）** 但 §3 详细记录**只有 A~L 十二节**（★ 无 M 节 · 目录 TOC 亦无）⇒ ★ **类别体系断链**。

| | 收益 | 代价 |
|---|---|---|
| ★ **A. 统一词表**（★ 归一 §2.1 主表 **41 行**的类别列 ⇒ §2.4 可派生） | ★ 根治；★ 与 ADR-094 D-16「生成镜像 + 机检相等」哲学一致 | ★ ★ **工程量此前被低估**：★ 不止 §2.1 的 41 行词表归一 —— ★ 还需 ② **§2.3 标题「9 条」⇒ 实测 18 行** ★ ③ **§2.4 的 M 类在 §3 详细记录无对应节**（★ §3 只有 A~L · TOC 亦无 M）⇒ ★ **类别体系断链** ⇒ ★ ④ 写派生脚本 |
| ★ **B. 扩展 A~M 类别**（★ 把「验证基建 / 架构治理」纳入 A~M ⇒ 补登 7 条） | ★ 改动局部 | ★ ★ **仍与 §2.1 词表不一致** ⇒ 两套分类长期并存 |
| ★ **C. 保留 backlog + 口径显式化**（★ §2.4 顶部写明「本表按 §3 的 A~M；§2.1 类别列是简写；ADR-087~094 未纳入」） | ★ 零风险 | ★ 统计仍落后 ⇒ 残留 ⑧ 不闭合；★ ★ **注：★ C 已部分落地**（★ `adr.md:159` 已有「统计口径（2026-11-07 显式登记）」注记）⇒ ★ C 不是「零风险待做」而是「**已做一半**」 |

★ ★ **architect 倾向**：★ **A**（★ 根治且与本仓既有哲学一致）★ ⇒ **需你点头 + planner/owner 配合**。★ ★ **若求稳**：★ 选 **C**（★ **已半落地**）★ + 把「★ §3 无 M 节」与「★ §2.1/§2.3 标题与行数不符」**另登记为新残留**（★ 不与 ⑲ 混）★ 是合理退路。

---

## 5. ★ 可机检门禁清单（★ 按成本/收益排序）

| # | 判据 | 落点 | 成本 | 误伤风险 | 归属 |
|---|---|---|---|---|---|
| **1** | ★ **ADR 孤儿检查**（★ `adr/` 每个文件 MUST 在 `adr.md` §2 有行；§2 每行的本地链接 MUST 存在） | `verify_adr.sh` 加两检查 | 低 | ★ ★ **实测零误伤**：★ `adr/` **18/18** 在 §2 有行 · §2 全部 **42 个本地链接 0 broken**（★ 脚本实测）⇒ ★ **可直接进 `verify_adr.sh`，无 baseline 负担** | ★ **builder** |
| **2** | ★ **索引表条目路径存在性**（★ §0.3 等表的反引号路径 MUST 存在） | `doc_link_check.sh` 检查 2 的 `PATH_CHECK_DOCS` 扩入该文件 + `SCAN_PATHS` 补 `.rddf/` | 低（★ 残留 ⑩ 已给方案） | ★ 低（★ 需放宽无 `.md` 后缀的条目匹配） | ★ **builder** |
| **3** | ★ **元数据四字段存在性 + `last_reviewed` 超期 WARN** | 新 lint | 低 | ★ 中（★ **必须先完成 frontmatter 标注**，★ 否则 4 份无元数据 + 历史文档批量误报） | ★ 依赖 ★ **frontmatter 先行** |
| **4** | ★ **§2.4 统计 = §2.1 主表派生**（★ 生成镜像 + 机检相等） | 小脚本 | 低 | ★ 零 | ★ 依赖 ★ **选项 3 的 A** |
| **5** | ★ **真相全景表漏收** | ★ **显式标记法**：★ 真相区文档头部加 `nav: hierarchical`（或 HTML 注释标记）⇒ ★ 检查「带标记 ⇒ MUST 在 §0.3 表内」 | 中（★ 一次性标注 ~10 份） | ★ 近零 | ★ **architect**（★ 标注）+ builder（★ 脚本） |
| | | ★ ★ **关键词法不可行** —— ★ 同词异义已实证（★ 「层次化」在 `performance-guide.md` 指 StatGroup 统计组）⇒ ★ 自动 fail 会恒红 ⇒ ★ 逼人为消红而扩张排除清单 ⇒ ★ **重蹈 `baseline_known_failures` 反模式** ⇒ ★ 退而求其次可做 **WARN 级嫌疑清单**（grep 命中但不在表内 ⇒ 打印供人工 triage） | | | |
| **6** | ★ **真相区纪律 lint**（★ `doc_type: target` 的文档正文禁出现「实现状态快照 / 当前实际状态 / 已落地 / 当前状态是」等实施态标记） | 新 lint（★ 或并入 `doc_link_check.sh` 检查 3） | 中 | ★ 可控（★ mixed/historical 天然豁免 ⇒ 依赖 frontmatter 先行） | ★ **builder** |
| **7** | ★ **命名规范** | ★ **不建议做闸** | — | ★ `docs/architecture/` 顶层**已统一为纯主题风格**（★ 18 份零例外）；★ `adr/` 亦**零例外**（★ `ADR-NNN-主题.md`）；★ 日期前缀风格只在上游与其他目录 ⇒ ★ 跨目录统一收益 ≈ 0 |

---

## 6. ★ 元数据规范（★ 建议的最小模板 · **兼容本仓惯例**）

```yaml
---
doc_type: target | mixed | historical | index | ledger | plan | research | reference
status:   active | draft | frozen | stale | superseded
owner:    <角色或具体 owner>        # architect / builder / planner
last_reviewed: YYYY-MM-DD          # ★ 上次核对日（对齐 M-6「时效」哲学：dated 越久越不可信）
---
```

★ ★ **必填 4 字段**（★ 选填 `supersedes: <path>` 用于 ADR 取代链）⇒ ★ **不引入版本号字段** —— ★ 本仓已有两套版本习惯（★ ADR 的 `vX.Y` · `adr.md` 的文档版本号）⇒ ★ 统一进 frontmatter 会引发存量迁移争议 ⇒ ★ **版本留在正文 Status 行，frontmatter 只管「判读四问」**（★ 可信吗 / 谁负责 / 何时写的 / 是什么类型）。
★ ★ **元数据现状（★ 实测）**：★ **4 份完全无元数据**（`background-and-goals` / `error-handling` / `interface-design` / `performance-guide`；★ 另有 3 份边缘 = 有章节级注记、无文档级字段）· ★ **格式 6 套并存**（表格式 3 份 / Status 行式 4 份 / 列表式 1 份 / 散文注记式 2 份 / 快照式 1 份 / PR 描述式 1 份）· ★ **无一份有维护人**（★ 仅 `roadmap-evolution.md` 有 Owner）⇒ ★ **当前读者做不到「10 秒判读」**。★ ★ **口径注**：★ 「完全无元数据」指**文档级**头部字段块；★ `tech-selection.md` / `ip-catalog.md` / `testing-and-dse.md` 三份**有章节级注记**但**无文档级**字段 ⇒ ★ 边缘情况，归入待标注。

---

## 7. ★ 命名规范（★ 结论：★ **不动**）

★ ★ **实测修正（★ architect 先前判断有误，★ 此处更正）**：★ `docs/architecture/` 顶层 **17 份纯主题风格 + 1 份例外**（★ 例外 = `m4-dse-cpufactory-real-pr-description.md`，★ 且它本质是 PR 描述、★ 不应驻留架构目录）· ★ `adr/` 18 份**全部** `ADR-NNN-主题.md`（★ 零例外）⇒ ★ ★ **「三种命名风格并存」在 `docs/architecture/` 内不成立** ⇒ ★ 日期前缀风格只存在于**上游 CppHDL** 与本仓**其他目录**（★ `docs/build-reports/` · `docs/superpowers/plans/` · `.rddf/plans/`）⇒ ★ **各服务各自读者** ⇒ ★ **不引入日期前缀**（★ 日期信息归 `last_reviewed` 字段）⇒ ★ **不跨目录统一命名**。

---

## 8. ★ 本轮已落地的修订（★ architect 单方可做 + 零争议）

| # | 修订 | 落点 | 闭合 |
|---|---|---|---|
| 1 | ★ `overview.md` 头部 stale 实施态快照 → **加 dated 定性横幅**（★ 「本段 = 实施态 · MUST NOT 被当作目标态引用」+ 指针到 `roadmap-evolution.md` 与纪律样板） | `overview.md:3` | ★ 问题 ② 的一半（★ 整段移出待选项 2/选项 1） |
| 2 | ★ `testing-and-dse.md` §当前实际状态（2026-06-17）→ **加 dated 定性横幅** | `testing-and-dse.md:94` | ★ 问题 ② 的一半 |
| 3 | ★ `interface-design.md` §1.0「Phase 1 当前状态是 POD」→ **加 dated 定性横幅**（★ 明写「目标态以右列为准」） | `interface-design.md:19` | ★ 问题 ② 的一半 |
| 4 | ★ `.rddf/config/arch-docs.json` → **删死路径** `ip/cpu/docs/adr`（★ 目录不存在）+ `description` 写明「**无执行脚本** ⇒ 可写范围是无闸约定」+ 覆盖缺口（★ 18 份中仅 7 份在范围内） | `.rddf/config/arch-docs.json` | ★ 部分（★ 扩不扩见选项 2） |
| 5 | ★ 主题文档 §7 残留 +2（⑲ 统计口径脱节 · ⑳ `arch_paths` 治理盲区） | 主题文档 §7 | ★ 登记（★ 决策见 §4） |

★ ★ **本自身的 mixed 属性（★ 诚实标注）**：★ ★ **本文件所在的 `hierarchical-plugin-architecture.md` §7 本身就是「mixed 内容」的实例**（★ 跨主题残留台账 · 含治理条目 ⇒ 与「真相 = 目标态」口径张力）⇒ ★ ★ **沿 ⑮~⑱ 的既有先例**（★ 该节已是「跨主题残留台账」形态）⇒ ★ 不构成新问题、★ 但**是本仓纪律尚未仓级化的又一处证据**。
---

## 9. ★ 治理路线图（★ 2026-11-08 收口深挖后）

> ★ §1~§8 是**第一轮**诊断与建议；★ ★ **§9 是收口深挖后的完整路线图**（★ 缺口清单 A~G + 新挖 G1~G9 + 优先级 + 后果 + 依赖图）。

### 9.1 ★ 完整缺口清单（★ 按「性质」分级）

| # | 缺口 | ★ 性质 | 证据 | ★ 建议处置 | 归属 |
|---|---|---|---|---|---|
| **D** | ADR-094 未登记 V-28/V-29 机制锚（★ `ch::hier_eq`） | ★ **待办 · 零依赖** | ★ 上游 helper 2026-11-06 落地 · 本仓 **0 引用** | ✅ **本轮已做**（★ ADR-094 V-28 补机制锚注） | ★ architect |
| **D2** | ADR-089 / 契约文档未登记「层次化产物适用面」 | ★ **待办 · 阻塞 §3.5 升 MUST** | ★ 两文 grep「层次化」**零命中** | ★ 起草为 ADR-089 修订 + 契约文档补面 ⇒ ★ **须经 oracle → critic 审**（★ 属新决策） | ★ architect（待你点头） |
| **B1** | ★ `plugin-framework.md` **17 处过时 / 死链**（★ 10 组内容过时 + 7 处死引用） | ★★ **实际错误** | ★ `:4` Phase 1.5 进行中（已 archive）· `:207` 「get 缺失默认构造」**与 ADR-040 v3.0 Check 8 fail-fast 直接矛盾** · `:669-671` 双副本（**与 ADR-083 矛盾**）· `:673-694` 「Plugin→lnode 转换器」（ADR-037 v2.0：无转换器）· `:156,220,318,388,456,568,573` **7 处 `src/cf_plugin/tests/` 死引用**（目录已删） | ★ **顶部标「Phase 0/1 历史快照」+ §4 / §5 整节替换为指针**（ADR-037 v2.0 / ADR-083 / ADR-091 v1.16）★ **局部 dated 注不可行**（★ 冲突遍布 §4/§5 + 死链 ⇒ 给读者「其余可信」的错觉）⇒ ★ 整体重写归 backlog | ★ owner（★ **不在 `arch_paths`**） |
| **B4** | ★ `error-handling.md`：`MemRespBundle` **死名** + COMPARE **从未存在** + `LOG_ERROR/LOG_WARN` **无此宏** | ★★ **实际错误** | ★ 真名 `cf::bundles::MemResp`（`bundles/mem_bundles.h:59`）· 开放核 `ip/cpu/core/bus_iface_chmem.h:27` 明禁该名 · COMPARE 已被 Sim↔Verilator parity 取代 | ★ dated 注 + 改死名 + §4.3 标「从未实现」 | ★ owner |
| **B5** | ★ `background-and-goals.md`：**C++17 事实错误** | ★★ **实际错误** | ★ `:20`「C++17 / C++20」⇒ ★ 实际 **C++23** 全 target（AGENTS.md + ADR-047） | ★ 立即改 C++23 | ★ owner（★ 或先扩 `arch_paths`） |
| **B6** | ★★ `performance-guide.md`：**大面积虚构** | ★★ **实际错误（最重）** | ★ §2.1 DMI JSON **无解析**（CppTLM 仅 `is_dmi_allowed() const { return false; }` 桩）· §2.2 `quantum_ns` 仅 GPU board shell 常量 · §2.4「CPU ISS 基本块缓存 2-5x」**零代码命中** · §3.3 速度表把 TLM_ONLY/RTL_ONLY/COMPARE **当真配置**（COMPARE/SHADOW 从未存在）· §4.1/§4.2 SHADOW/ImplMode 切换 JSON **虚构** ⇒ ★ **全文无一处真实实测性能数据** | ★ **整体标历史态**（★ ADR-088 §10 单模后其指导价值归零）⇒ ★ **NOT** 逐条修（★ 修成本 > 重写） | ★ owner |
| **B3** | `code-framework-mapping.md`：`extern/CppTLM` 错径 + §3.1 缺层次化符号 | ★ **实际错误** | ★ `:75` 错径（★ 已在 ADR-083 / `overview.md` 同批订正）· §3.1 缺 `create_child` / `hier_eq` / `toVerilogHierarchical`，★ 且列了 `ch_module` / `ch_device`（★ ADR-091 G-1b 禁用于层 2） | ★ 改错径 + 补行（★ 按其自带「版本敏感性声明」） | ★ owner |
| **B2** | `tech-selection.md`：数字快照腐化 + C++20 | ★ 定时炸弹 | ★ `:63,64,113,115` 快照数字 · `:124`「C++20」 | ★ 数字去敏（★ AGENTS.md「不记快照」纪律）+ dated 注 | ★ owner |
| **G7** | ★★ `adr.md` §2 **分段语义错位**（★ 本轮核心新证） | ★ **定时炸弹** | ★ §2.3（★「Phase 1 提案决策（🚧）」段）实测 **18 行 = 🚧 12 + ✅ 6**（★ ✅ 6 = ADR-045/087/088/089/091/092）⇒ ★ 该段实为**追加残段**、非按状态分段（★ 046/047/048/049/082/083 被放进 §2.1 · ★ 045/087~092 没搬）⇒ ★ **6 次注册 5 次不搬** ⇒ ★ ★ **「按状态分三段 + 人工搬行」已被证伪** ⇒ ★ 唯一出路 = **单表 + 状态列 + 机检** | ★ **随 A3 一并裁决**（★ ★ 人工搬行方案排除） | ★ **待决策** |
| **G1** | `adr.md` §3 节标题计数腐化 | ★ 定时炸弹 | ★ C「5 条」实 **6** · K「2 条」实 **1** · L「1 条」实 **2** · TOC G=5（节标题 7） | ✅ **本轮已修**（★ 含 TOC anchor 同步 —— ★ anchor 含条数 ⇒ 不同步即断链） | ★ architect |
| **G8** | `adr.md` §2.4 M 类静默漏 ADR-049 | ★ 效率损失（★ §2.4 已有「不得引用为全部 ADR 的分布」声明对冲 ⇒ 实际危害偏低） | ★ M=3 只数 044/085/086 · 漏 049（✅ · 2026-09-25 · ≤086） | ★ **随 A3 一并**（★ 单点改会再动那套脱节统计） | ★ **待决策** |
| **G3** | ADR-042 决策被现实超越 | ★ 定时炸弹 | ★ §2.1 行 ✅，★ 但 MMU 已在 Phase 6d 实装（`ip/mmu/` · `[mmu]` · `src/cf_plugin/bridge/mmu_bridge.h`） | ✅ **本轮已加 dated 注**（★ 推迟语义仅对 FPU/Exception 成立） | ★ architect |
| **G4** | ADR-091 文件 Status 行版本滞后 | ★ 效率损失 | ★ 文件 Status 写 v1.2/v1.9，★ 正文与 §2.1 行已是 v1.16 | ★ 随下次修订 | ★ architect |
| **G2** | ★ ADR-083（**Accepted**）内 `extern/CppTLM` 错径 ×2 | ★★ **实际错误** | ★ `extern/` 目录**实测不存在** | ✅ **本轮已修**（★ + `overview.md` 1 处 ⇒ 3/5 处） | ★ architect |
| **G5** | `docs/lessons` / `docs/methodology` 各含 `src/cf_plugin/tests/` 死引用（×3） | ★ 定时炸弹 | ★ 目录已删 | ★ 随 C3 扩围**自动进检** | ★ builder / owner |
| **G6** | `background-and-goals.md:55`「与 CppTLM 共享同一套 Bundle 类型系统」过度陈述 | ★ 实际错误（弱） | ★ `bundles/` 用 `cf::plugin::uint_t`，★ TLM 下是 POD | ★ dated 注 | ★ owner |
| **G9** | `error-handling.md` **全文无 CH_MEM 错误范式** | ★ 缺口 | ★ ADR-047 `Result` / PayloadStore fail-fast 全文缺席 | ★ owner 补节 或 标历史态 | ★ owner |
| **A3** | `adr.md` §2.4 统计口径（48 vs 60 · 两套词表 · M 类断链） | ★ 定时炸弹 | ★ 见 §7 | ★ **单表化 + 派生 + 机检相等**（★ 与 ADR-094 D-16 同一哲学） | ★ **待决策** |
| **A2** | `arch_paths` 仅覆盖 **7 份顶层文件 / 18 份**（★ 另含 1 个目录 `adr/`）⇒ ★ **盲区 10 份**（★ 口径：18 顶层 − 7 命中 − 1 排除 = 10） | ★ 归属问题 | ★ `.rddf/config/arch-docs.json` | ★ **扩围**（★ 换 architect 直接修的权利）★ ⇒ ★ **建议扩 4 份（★ 逐一列名，★ 不含历史态 / 映射态文档）**：★ ① `error-handling.md`（★ 目标态 · 仅缺 dated 注 + 死名 `MemRespBundle`）· ② `background-and-goals.md`（★ 目标态 · C++17 事实错误）· ③ `performance-guide.md`（★ **大面积虚构** ⇒ 需整体标历史态 ⇒ ★ **须 architect 写**）· ④ `ip-catalog.md`（★ 索引层 · §相关文档 只链 2 份架构文档）★ ★ **明确排除 3 份历史态 / 映射态文档**（★ `plugin-framework.md` · `code-framework-mapping.md` · `tech-selection.md`）⇒ ★ ★ **扩围 = 2 + 2 = 4 份（★ 算术闭合）** | ★ **待决策** |
| **A1** | 无 `docs/architecture/README.md` | ★ 效率损失 | ★ 18 份靠 AGENTS.md §5 四行表 | ★ 建（★ MUST 生成式 + frontmatter 派生 + 机检相等） | ★ **待决策** |
| **C1** | ADR 孤儿检查未建 | ★ 效率损失 | ★ `verify_adr.sh` 不做 | ★ builder（★ **实测零误伤** ⇒ 可直接落闸） | ★ builder |
| **C2** | 索引条目路径存在性未建 | ★ 效率损失 | ★ 残留 ⑩ | ★ builder | ★ builder |
| **C3** | ★ `doc_link_check.sh` 检查 2 仅根级 3 文档 | ★ **定时炸弹** | ★ `doc_link_check.sh:14-15,56` · ★ 实测漏报 **≥16 处**死/错引用 | ★ **先扩 `docs/architecture/`**（18 份）★ ⇒ 跑顺后再评估全量（★ 脚本注释明说全量噪声过高） | ★ builder |
| **E** | 层次化落点未登记进 `objective-soc-cpu-v0110-launch.md` | ★ 待办 | ★ 该文件 grep「层次化」**零命中** | ★ planner | ★ planner |
| **F** | 层次化主题 §7 的 20 条残留（★ ⑨已闭合） | ★ 混合 | ★ 主题文档 §7 | ★ 按各条归属（★ ⑭ 已由 D 闭合） | ★ 混合 |
| **B7** | ★ `m4-dse-cpufactory-real-pr-description.md` **不应驻留架构目录**（★ PR 描述非架构文档） | ★ 效率损失 | ★ 该文自述「OpenSpec change 已归档」 | ★ 移出（★ 归 `openspec/changes/archive/` 旁或 `docs/` 历史区） | ★ owner |

### 9.2 ★ 优先级（★ 前 5）

| 序 | 项 | ★ 为什么这个序 |
|---|---|---|
| **1** | ★ **D（机制锚注）** | ★ ★ **零依赖** + ★ **阻塞 ADR-094 治理收口** ⇒ ★ 本轮已做 |
| **2** | ★ **B5 / B4 / B6 的「错误子集」**（C++17 · `MemRespBundle` · `performance-guide` 虚构） | ★ ★ **读者即刻会信的错** ⇒ ★ 改一句话即止血（★ B6 是整体标历史态） |
| **3** | ★ **A2（`arch_paths` 扩围）** | ★ ★ **它决定 B1~B6 谁有权改** ⇒ ★ 它是 B 的**前置** |
| **4** | ★ **G7① §2 单表化 + 两条机检**（★ 节标题数 = 行数 · 行状态 = 节语义） | ★ ★ **拆弹**：★ 单表化后 A3（补登）才有意义、★ C1（孤儿检查的词表）才稳定 |
| **5** | ★ **C3（`doc_link_check` 扩围 `docs/architecture/`）** | ★ ★ 把 **≥16 处**死/错引用**收进门禁** ⇒ ★ G5 自动收敛 |

### 9.3 ★ ★ 不治理的**实际后果**（★ 最严重 3 条）

| # | 谁 · 什么时候 | ★ 会发生什么 |
|---|---|---|
| **1** | ★ **下一个 onboarding 的 IP 开发者** | ★ 读 `background-and-goals.md:20` 认定代码库 **C++17**；★ 按 `performance-guide.md` 的 §2.1/§4.1 配 `dmi_enabled` / `impl_mode_config` JSON ⇒ ★ **照做即发现配置无人解析**（★ ADR-029 明写「悬挂引用」）⇒ ★ ★ **onboarding 第一天踩两个虚构/错误事实** |
| **2** | ★ **验证工程师** | ★ 按 `error-handling.md:39-51` 用 **`MemRespBundle.status`** 设计错误注入、按 `:74-77` 做 **COMPARE** 异常验证 ⇒ ★ **编译直接失败**（★ 开放核 `bus_iface_chmem.h:27` 明禁该名）★ 且 **COMPARE 模式从未存在**、parity 语义早已取代 |
| **3** | ★ **架构读者** | ★ 按 `plugin-framework.md:669-671` 得出「同一 IP 可维护 **TLM 版 + RTL 版两份**」⇒ ★ ★ **这正是 ADR-083（2026-10-08 Accepted）要消灭的双副本模式** ⇒ ★ 若据此规划新 IP ⇒ ★ **新项目 Day 1 就违反已签批的 single-source-of-truth** |

### 9.4 ★ 依赖图（★ 文字版）

```
A2（arch_paths 扩围）──► B1~B6（5 份盲区文档的直接修权限）
        │                        └──► C3（扩围后 doc_link_check 才有意义）
        ▼
G7①（§2 单表化 + 机检）──► A3（§2.4 补登 087~094）──► C1（孤儿检查的词表稳定）
        │
        └──► G8（M 类补 ADR-049）· G1（§3 计数，已做）

D（机制锚注）──► ADR-094 治理收口（★ 独立快车道 · 零前置 · ★ 本轮已完成 D1）
D2（适用面登记）──► 主题文档 §3.5 由 SHOULD 升 MUST（★ 须先登记）

E（objective 登记）∥ 全部（★ 可并行）
F 残留 ⑭ ──► D（★ 已闭合）
```

### 9.5 ★ 建议的**决策顺序**（★ 若只能做三件事）

| 顺 | 决策 | ★ 为什么先做它 |
|---|---|---|
| **1** | ★ **A2（`arch_paths` 扩围 · 建议扩 4 份 · ★ 清单见 §9.1 A2）** | ★ 它**解锁 B1~B6 的直接修权限** ⇒ ★ 否则 5 份盲区文档只能长期走 owner 注记（★ 慢且易漏） |
| **2** | ★ **A1（建 `docs/architecture/README.md`）** | ★ 闭合「17 份零全局索引」⇒ ★ 且 MUST 生成式（★ frontmatter 派生 + 机检相等）⇒ ★ 否则 README 自己也会腐 |
| **3** | ★ **A3（§2.4 口径 · 建议在 G7① 单表化之后）** | ★ ★ **顺序有依赖**：★ 先单表化（★ 消除「人工搬行」这个已被证伪的方案）★ 再补登 ⇒ ★ 否则会把两套口径固化得更深（★ 残留 ⑲ 已警示） |

### 9.6 ★ 本轮已做（★ architect 单方可做 + 零风险）

| # | 修订 | 性质 |
|---|---|---|
| 1 | ★ `adr.md` §3 节标题计数修正（C 5→6 · K 2→1 · L 1→2）+ ★ **TOC anchor 同步**（★ anchor 含条数 ⇒ 不同步即断链） | ★ 纯事实修正 |
| 2 | ★ `adr.md` §2.1 ADR-001 行状态：✅ → ★ **⚠️ 部分过时**（★ 依据 §3 记录） | ★ 纯事实修正（★ 消除表内自相矛盾） |
| 3 | ★ `adr.md` §2.1 ADR-042 行加 dated 注（★ MMU 已实装 ⇒ 推迟语义仅对 FPU/Exception 成立） | ★ 事实标注 |
| 4 | ★ `extern/CppTLM` 错径订正 **3 处**（★ `ADR-083` ×2 + `overview.md` ×1）⇒ ★ 实际 = 仓库根 `CppTLM/`（★ `extern/` 实测不存在） | ★ 纯路径事实修正 |
| 5 | ★ **ADR-094 V-28 补 `ch::hier_eq` 机制锚注**（★ 7 项闭合公共面 + fail-closed + 判别力边界 + 「MUST NOT 自建等价比对器」） | ★ **D1**（★ 机制锚的首次登记） |

★ ★ **未做（★ 需你决策）**：★ 补登 `adr.md` §2.4（★ 会固化第二套分类 ⇒ 见选项 3）· ★ 建 `docs/architecture/README.md`（★ 见选项 1）· ★ 扩 `arch_paths`（★ 见选项 2）· ★ 写门禁脚本（★ 归 builder）· ★ frontmatter 批量标注（★ 依赖选项 1）。
