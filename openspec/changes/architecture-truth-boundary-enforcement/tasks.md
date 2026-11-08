---
change: architecture-truth-boundary-enforcement
priority: P1
initiative: wave7-architecture-governance
adr:
  - ADR-095
plan: .rddf/plans/architecture-truth-boundary-enforcement.md
status: in-progress
depends_on:
  - architecture-truth-boundary
---

# tasks — architecture-truth-boundary-enforcement（4 道门禁 + 内容净化）

> ★ **AC 权威 = `proposal.md` 的 `## Acceptance Criteria` 段**（`rdd-verifier` 提取源）。
> 本文件 = 执行清单 + 勾选态 + **裁决/偏差登记**。
> 证据目录：`evidence/`（`gate1-*` · `gate2-negative-test.txt` · `gate3-orphan-test.txt` · `ac114-revert-test.md` · `ac120-extraction.txt`）

## 1. ★ 开工前的三项裁决（2026-11-08 · 用户）

实测发现 proposal 的判据与 ADR 的现实存在冲突（28 份文档 **201 处**原始命中），three 项交用户裁决：

| # | 问题 | 实测依据 | 裁决 |
|---|---|---|---|
| Q1 | 词表含 `PASS` / `是 POD` | `PASS` 贡献 **89/201** 命中，但它是 S-契约的 verdict 取值（`0 = PASS` / `verdict`(PASS\|FAIL\|ERROR)）+ 里程碑表测试结果 ⇒ **零**实施态语义；`是 POD` 是类型不变量，且子串命中否定句「`ch_uint` 在 TLM 编译下**不是 POD**」（ADR-092:51）⇒ 语义相反 | **移除两词**，保留 6 词 + Phase-N 正则 |
| Q2 | 门禁① 范围含 `adr/` | `adr/` 侧经全部语法豁免后仍剩 **76 处**，逐条核查**全是 ADR 历史本体**：决策时点 before-state（ADR-087:17「当前实现 = RV32IM」）、交付位置（ADR-040:118）、残留清单（ADR-091:502）⇒ 清零须改写 19 份 Accepted ADR 正文 | **范围限定顶层 9 份**；`adr/` 由门禁③ + Status frontmatter 治理 |
| Q3 | `hierarchical-plugin-architecture.md` §1「实施态快照（… 非目标态）」 | 与 ADR-094 M-8（主题文档内制度化「真相区 / 实施区」分离）相容，与 ADR-095「顶层只放真相源」冲突 | **保留该节**；门禁① 增加语法豁免「自标注「非目标态」的节」 |

★ 三项裁决使 proposal 的 AC-110① / AC-110④ / AC-113 判据**字面**发生变化 ⇒
  本文件记录为**判据修正**（而非实施偏差）；proposal 的修订归 planner/architect。

## 2. ★ 门禁①（AC-110）

- [x] **2.1** 新建 `tools/check_arch_truth.sh`（shell 入口）+ `tools/check_arch_truth.py`（判据本体）
      ★ 词表冻结在 `.py`（单一真源 · 禁运行时读外部文件 · 禁 CLI 传入）
- [x] **2.2** 8 类**语法上下文豁免**（禁按路径豁免）：
      (a) frontmatter（仅第 1 行 `---`）· (b) 表格状态单元格 · (c) HTML 注释块 ·
      (d) dated 横幅 · (e) 代码围栏 · (f) 表头行 · (g) 日期日志行 · (h) 自标注「非目标态」节
- [x] **2.3** fixture 自检：`--self-test` ⇒ **9/9**（8 类豁免 + 1 真阳性 · 真阳性期望 5 处）
- [x] **2.4** 顶层扫描 ⇒ **0 命中**（AC-109c 的兜底判据）
- [x] **2.5** `mixed/` `archive/` 不在扫描范围（非真相源）⇒ 天然豁免

★ ★ 实现中修掉的两个自身缺陷（均为实测命中）：
  ① 把任意 `---` 当 frontmatter 开关 ⇒ **104 处**真实命中被误判豁免（**假绿**）⇒ 改为「仅第 1 行」
  ② Phase 正则漏匹配 `Phase 6d 完成`（字母后缀）⇒ 改为 `Phase\s*\d+[a-z]?(\.\d+)?\s*完成`

## 3. ★ 门禁②（AC-111）

- [x] **3.1** `doc_link_check.sh` 的检查 2（反引号路径）扩入 `docs/architecture/`（含子目录）
- [x] **3.2** 落闸实测：**100 处断链**（此前完全不可见）
- [x] **3.3** **42 处机械错路径 → 修源文本**
      L1-A 迁移的 6 份新位置 16 处 · 代码/测试早先移动 9 处 · 8 个已归档 change 的日期前缀路径 17 处
      ★ 根因：L1-A 的 `migrate_arch_docs.py` 只改 markdown 链接，**未改反引号路径**
- [x] **3.4** **35 个 distinct 目标（58 处）→ 显式登记豁免**（`tools/doc_link_check_ignore.txt`，按组注明理由）
- [x] **3.5** 顺带修 ignore 清单语义 bug：原只做**精确相等**匹配 ⇒ 清单里的目录条目对子路径**从未生效**
      改为支持**目录前缀**
- [x] **3.6** 负向验证：临时加一个不存在的反引号路径 ⇒ 报 `BROKEN_PATH`（`evidence/gate2-negative-test.txt`）
- [x] **3.7** 实测 **0 broken**

## 4. ★ 门禁③（AC-112）

- [x] **4.1** `verify_adr.sh` 加 `check_adr_orphans()`：`adr/` ↔ `adr.md` 双向
- [x] **4.2** 报错含**可执行指引**（三行：加行位置 / 同 PR 落地 / 责任人 = architect）
- [x] **4.3** Summary 增列 `ORPHANS`；孤儿计入 FAILED ⇒ 退出码 1
- [x] **4.4** 实测零误伤：19 份 ADR 全部有行 · 42 链接 0 broken ⇒ `ORPHANS 0`
- [x] **4.5** 负向验证两个方向（孤儿文件 / `adr.md` 死链）各抓一次（`evidence/gate3-orphan-test.txt`）

## 5. ★ 门禁④（AC-113）

- [x] **5.1** 实装进门禁① 同一入口：顶层引用 `mixed/` `archive/` 下的**文件**时，同行 MUST 出现「非权威」
- [x] **5.2** 只认 `.md` 文件引用，**不认「仅命名目录」**（分区表里的 `mixed/` 不要求标注）
- [x] **5.3** 补标 **10 处**（`adr.md` ×7「完整设计见 mixed/plugin-framework.md」· `error-handling.md` ×1 · `hierarchical` ×2）
- [x] **5.4** fixture：`i_subdir_ref_unmarked.md`（期望 2 命中）/ `j_subdir_ref_marked.md`（期望 0）
- [x] **5.5** 实测 **0 命中**

## 6. ★ 内容净化（AC-109c）

- [x] **6.1** `overview.md`：「当前状态（Phase 6c/6d, 2026-10-08）」整块 5 条快照 → **指针**
      （IP 状态 → `ip/ip-catalog.md` · 阶段进度 → `.rddf/roadmap/` · 演进 → `roadmap-evolution.md`）
      ★ 该块自 2026-10-08 起已与 roadmap / ip-catalog 重复，且描述已退役的「TLM 模式」
- [x] **6.2** `interface-design.md`：**`ImplMode` 整体下线**（ADR-093 Phase-2 单模化后已废弃）——
      enum 块标为「历史形态 · MUST NOT 当作现行 API」；SoC 组装示例去掉 `ImplMode` 参数与 `mode_` 成员
- [x] **6.3** 其余措辞级修订（`adr.md` 列名与 4 处正文 · `hierarchical` 对照表列名 ·
      `README` / `error-handling` / `parity-gate-contract` / `roadmap-evolution` 各 1 处）
- [x] **6.4** 门禁① 对顶层 **0 命中**（改前 25 处）
- [x] **6.5** ★ **不改任何决策内容**（ADR 决策本体一字不动；全部为措辞/位置调整）

## 7. ★ 回滚验证（AC-114）

- [x] **7.1** 批 1（含 3 节点环 · 也是最大批 · `00e239a`）在自身提交点 revert ⇒ **三检查全绿**，
      顶层 12 → 18 份，`arch_paths` 16 条**全部复原**
- [x] **7.2** 批 5（`f4361ab`）在自身提交点 revert ⇒ `doc_link` 0 broken · `verify_adr` 0 FAILED ·
      位置复原；`arch_paths` 的 6 条死条目**是该点既有**（配置改版 AC-108 在更晚的 `28b01db`）
- [x] **7.3** 证据：`evidence/ac114-revert-test.md`

★ ★ **AC-114③ 的字面表述被实测推翻**：原文写「其余批由『单 commit 可 revert』结构性保证」。
  实测在 `28b01db` 上单独 revert 批 5 ⇒ **4 处断链**，全部来自**更晚的 commit**
  （`docs/architecture/README.md`（`c12679f` 新建）与 `ip/README.md`（`fc7f017` 改指针）
  都假定 `ip/ip-catalog.md` 已存在）。
  ⇒ **单 commit 可 revert 是「时点性质」不是全局性质**；中间点单独 revert 早期移动批，
    后续依赖它的 commit 必然断链 —— 这是迁移顺序的固有性质，不是缺陷。

## 8. ★ AC ↔ V 覆盖校验（AC-115a / AC-115b）

- [x] **8.1** 新建 `tools/check_ac_v_coverage.py`（AC-115b 指名的脚本）
- [x] **8.2** 三条判据：无标签 AC · 孤儿 V · 未授权重复分配（`CROSS_ALLOWED` 白名单冻结在脚本内，每条带理由）
- [x] **8.3** `--self-test` ⇒ **6/6**（全绿 / 缺标签 / 孤儿 V / 未授权重复 / 已授权交叉 / self-exempt）
- [x] **8.4** 实测：AC 21 条（11 + 10）· 无标签 0 · **并集覆盖 10/10** · 无孤儿 V · 无未授权重复

## 9. ★ 误伤收敛证据（AC-119）

- [x] **9.1** 4 类（实为 8 类）豁免 fixture 运行输出：`evidence/gate1-self-test.txt`（9/9）
- [x] **9.2** 真实样本文档不误报：`evidence/gate1-real-docs.txt`
      （真实豁免分布：`b-status-cell` 36 · `e-fence` 8 · `g-dated-log-row` 3 · `h-self-declared-nontarget` 2 · `f-table-header` 1）
- [x] **9.3** 真阳性：`evidence/gate1-true-positive.txt`（fixture 5 处逐条命中）

★ ★ **AC-119② 的样本替换（如实登记）**：原文指定的 3 类真实样本是
  「`adr.md` 状态列 · `roadmap-evolution.md` 图例 · 目标态前提句」。
  - `adr.md` 状态列 ✅ 仍被豁免（36 处 `b-status-cell`）
  - `roadmap-evolution.md` 图例 ⚠️ **不再是样本**：它命中的是 `PASS` 与「进行中」，
    而 `PASS` 已按 Q1 裁决移出词表 ⇒ 该处**不含任何词表词**，无需豁免
  - 「目标态前提句」⚠️ 同上：净化后顶层不再有「提到当前状态以声明非现状」的句子
  ⇒ 以**真实文档上实际触发的 5 类豁免**替代原 3 类样本（覆盖面不窄于原要求）。

## 10. ★ AC 段头统一（AC-120）

- [x] **10.1** **8 个** `## Acceptance` → `## Acceptance Criteria`
      （`chmem-bp-btb-gshare` 原为 `## Acceptance (待 wave3 后细化)` ⇒ 带尾注，正则不匹配，一并规范）
- [x] **10.2** `mmu-chmem-pipeline-integration` **补 AC 段**（原无）：12 条，逐条对应其 §1~§11
- [x] **10.3** 深度修复 2 个「有段头但提取为空」：
      · `chmem-bp-btb-gshare`：原段只有一句指针 ⇒ AC 正文**移入** proposal.md，tasks.md 改为指针（单一副本）
      · `parity-observation-ambiguity-fail-closed`：AC 是**表格** ⇒ 6 行逐条转 bullet，文本一字不改
- [x] **10.4** 复核：**14/14 change 提取非空**（`evidence/ac120-extraction.txt`）

## 11. ★ 遗留清扫（本 change 范围）

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / 半截实现（新增文件） | 无 |
| 临时调试日志 / `breakpoint()` / pdb | 无 |
| 被 skip 的测试 | 无 |
| 未替换的 mock / 临时 fallback | 无 |
| 工作区临时文件（`.bak` / `__neg_test.md` / `ADR-999-orphan-test.md`） | 全部已删除（负向验证后复原） |
| `tools/migrate_arch_docs.py`（姊妹 change 的产物） | 保留（供新增文档下沉复用） |

★ **范围外发现（只记录，不清理）**：
  - `tools/test_doc_checker.py` 在本机不可运行（同名 `tools` 包被另一 checkout 抢先解析）——
    **pre-existing 环境问题**，非本 change 引入
  - `arch-docs.json` 的 `build` 条目在未构建环境中不存在 —— 环境项，非死条目
  - ★ `mmu-chmem-pipeline-integration` 的 `openspec validate` **失败（pre-existing）**：
    其 `specs/cpu-l1-mmu-demo-regression/spec.md` 的 `MODIFIED` 块漏继承既有 scenario
    （「Fix SHALL NOT modify ADR-049 PADDR consumption contract」的 `ADR-049 contract preserved`）。
    ★ 该 specs/ 目录最近一次改动来自 `308cc08` / `fafadf7`（本 change 之前）⇒ **非本 change 引入**，
    且修它需要判断那条 spec 的语义 ⇒ **不在本 change 范围**。后果：`openspec validate --changes`
    为 **15 通过 / 1 失败**（本 change + 姊妹 change + L4 两个 change 全部 ✓）。

## 12. ★ 阶段边界

| 项 | 本 change |
|---|---|
| 4 道门禁（①②③④） | ✔ |
| 顶层内容净化 + `interface-design.md` 的 `ImplMode` 下线 | ✔ |
| 回滚验证 + 误伤收敛证据 + AC↔V 校验 + AC 段头统一 | ✔ |
| 修订 `ADR-095 §2.5①` 的「顶层 + `adr/`」表述 | ✘ ⇒ **architect**（Q2 裁决已改变判据字面） |
| 修订 `proposal.md` 的词表/范围/豁免描述 | ✘ ⇒ **planner**（本文件已登记判据修正） |
