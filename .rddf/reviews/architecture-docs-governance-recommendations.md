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

★ ★ **architect 倾向**：★ **区分对待** —— ★ `error-handling.md` / `background-and-goals.md`（★ **目标态** + 仅缺 dated 注）**值得扩**；★ `plugin-framework.md` / `code-framework-mapping.md` / `tech-selection.md`（★ **历史态 / 映射态**）**不值得扩**（★ 扩了也是让 architect 去改历史文档 ⇒ 正确处置是 dated 注 + 整体重写归 backlog）⇒ ★ **需你点头「扩 2 份」这个口径**（★ 上表已补该栏）。

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
★ ★ **未做（★ 需你决策）**：★ 补登 `adr.md` §2.4（★ 会固化第二套分类 ⇒ 见选项 3）· ★ 建 `docs/architecture/README.md`（★ 见选项 1）· ★ 扩 `arch_paths`（★ 见选项 2）· ★ 写门禁脚本（★ 归 builder）· ★ frontmatter 批量标注（★ 依赖选项 1）。
