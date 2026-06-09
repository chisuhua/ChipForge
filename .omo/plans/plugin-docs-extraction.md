# 插件架构文档抽离与状态修正

| 字段 | 值 |
|------|-----|
| 计划版本 | 1.0 |
| 计划日期 | 2026-06-09 |
| 计划状态 | **Proposed（待用户最终确认）** |
| 触发 | 架构审查发现 `declarative-hybrid-framework.md` §4/§7 与代码严重背离（5 STALE ADRs + "100% not in code" 描述 vs 实际 5/5 P0 完成）|
| 关联决策 | `.omo/drafts/decision-plugin-framework-2026-06-08.md`（事实已 Accepted：Phase 0 代码已落地）|
| 关联计划 | `.omo/plans/plugin-framework-revision-plan.md` v1.0（路线图修订，**已存在但仅 Draft，且不覆盖本次抽离范围**）|
| 预计总工作量 | ~6-9 工作日（纯文档 + 验证脚本更新，零代码改动）|
| 风险等级 | **低**（纯文档，零代码/构建影响；但跨文档引用图变更需谨慎）|

---

## TL;DR

> **Quick Summary**: 把插件架构内容（Plugin 模型 / PipeNode / PipeBuilder / CtrlLink 等 5 个 P0 组件的设计）从 1390 行的 `declarative-hybrid-framework.md` 抽离到 580 行的 `docs/architecture/plugin-framework.md`，同时把 5 条 STALE ADR 状态修正为 ✅ Phase 0 完成，把 `code-framework-mapping.md` §7.3 的"0 行代码"修正为实际数字，把 `verify_adr.sh` 中 3 处脚本缺陷修复。
>
> **Deliverables**:
> - 新建 `docs/architecture/plugin-framework.md`（~580 行，插件架构唯一权威）
> - `declarative-hybrid-framework.md` v2.0.3 → v2.1.0（移除 §4/§7，~400 行 → 留 ~990 行）
> - `code-framework-mapping.md` §7.3 修正（"0 行"→ 6 头文件/667 行/51 测试）
> - `adr.md` ADR-025~033 状态修正（5 STALE → ✅/⚠️）
> - `tools/verify_adr.sh` 修复 3 处脚本缺陷（ADR-027/033/031 误报）
> - `docs/api/cf_plugin.md` 加架构导引链接
>
> **Estimated Effort**: Medium（~6-9 工作日 / 跨 2 周）
> **Parallel Execution**: YES - 4 waves
> **Critical Path**: 1 → 2 → 5 → 9 → 11

---

## Context

### Original Request

用户审查发现 `include/cf/plugin/` 已实现但 `declarative-hybrid-framework.md` 仍标"100% not in code"，询问"是否独立文档来维护和跟踪"。审查结论：建议 Y（抽离），用户已同意。

### Interview Summary

**Key Discussions**:
- 现状：5/5 P0 组件已落地（667 行代码 + 51/51 测试 PASS），但 4 个文档中 3 个严重背离
- 受众：插件架构有 2 个独立读者群（系统架构师看 §1-3/8/11.2；Phase 1-4 业务开发者看 §4/§7）
- 生命周期：§4/§7 在 Phase 1-6 期间会被频繁改写（51 周），其他章节稳定
- 跨引用：必须用 fan-in 模式（plugin-framework.md 是中心，其他文档只指向它）

**Research Findings**:
- Oracle 咨询明确推荐抽离（4 项标准全部成立）
- 已存在 `plugin-framework-revision-plan.md` v1.0 但范围不覆盖本次抽离
- 决策文档 `decision-plugin-framework-2026-06-08.md` 状态 Proposed 但事实已 Accepted（代码已落地）
- `verify_adr.sh` 3 处脚本缺陷需要修复

### Metis Review

**Identified Gaps**（已识别并处理）:
- **Gap 1**: `CtrlLink` 不继承 `PipeLink` 基类（与 ADR-031 措辞冲突）→ 计划中 ADR-031 措辞需修正而非追平代码
- **Gap 2**: `StageLink` / `DirectLink` 完全未实现 → ADR-031 保持 🚧 但措辞明确"Phase 0 仅 CtrlLink"
- **Gap 3**: `verify_adr.sh` 对 `at_stage` / `Phase` / `CtrlLink` 报告 STALE/EXPECTED_MISSING 是脚本缺陷 → 需修复脚本
- **Gap 4**: `plugin-framework-revision-plan.md` v1.0 与本计划范围不重叠 → 保留作为路线图修订的 Draft，本计划专注于抽离

---

## Work Objectives

### Core Objective

把插件架构的设计意图从"混合在 1390 行大文档中"重构为"独立的 580 行权威文档 + 5 处轻量跨引用"，消除文档与代码背离，停止 drift。

### Concrete Deliverables

1. **新建** `docs/architecture/plugin-framework.md`（~580 行）
2. **修改** `docs/architecture/declarative-hybrid-framework.md` v2.0.3 → v2.1.0（移除 §4/§7，~400 行删除 + ~5 行新增 cross-ref）
3. **修改** `docs/architecture/code-framework-mapping.md` §7.3（"0 行" → 6 头文件/667 行/51 测试）
4. **修改** `docs/architecture/adr.md` ADR-025~033（5 STALE → ✅/⚠️，措辞调整）
5. **修改** `tools/verify_adr.sh` 修复 3 处脚本缺陷
6. **修改** `docs/api/cf_plugin.md` 头部加架构导引链接

### Definition of Done

- [ ] `bash tools/verify_adr.sh` 输出 0 STALE、0 EXPECTED_MISSING（9 个仍 EXPECTED_MISSING 的需在脚本中注释为"Phase 6 范围"）
- [ ] `declarative-hybrid-framework.md` v2.1.0 发布头标注"§4/§7 已抽离至 plugin-framework.md"
- [ ] `plugin-framework.md` 含完整设计意图 + 正确状态标注 + ADR 摘要
- [ ] 5 处跨引用全部单向 fan-in（`plugin-framework.md` 是中心）
- [ ] 5 个测试可编译通过：`test_plugin_lifecycle` / `test_payload` / `test_pipe_node` / `test_pipe_builder` / `test_ctrl_link`
- [ ] 文档无 `tick()` 业务代码（`grep -r "void tick()" include/cf/` 仅命中 `plugin_base.h:71` 的 `= delete`）
- [ ] `cf_plugin.md` 头部增加 1 行 "**架构权威**: 详见 [`docs/architecture/plugin-framework.md`](../../architecture/plugin-framework.md)"

### Must Have

- ✅ `plugin-framework.md` §1（设计动机）+ §2（5 个 P0 组件详细 API 摘要）+ §3（生命周期）+ §4（与 CppTLM/CppHDL 关系）+ §5（Phase 0/1/6 路线图锚点）
- ✅ `declarative-hybrid-framework.md` 移除 §4/§7 后,**保留** §4.8（Plugin 与 ChStreamModuleBase 关系，因为这是跨抽象层的关系，不属于插件架构内部）
- ✅ ADR-025/026/027/028/030/032/033 状态改为 ✅ Phase 0 P0 #N
- ✅ ADR-031 保持 🚧 但措辞改为"Phase 0 仅 CtrlLink，StageLink/DirectLink/PipeLink 基类推迟到 Phase 6"
- ✅ ADR-029（ImplMode）保持 🚧 Phase 6（与本次抽离无关）

### Must NOT Have (Guardrails)

- ❌ **不动代码**（include/cf/plugin/*.h 与 src/cf_plugin/tests/*.cpp 全部保留原状）
- ❌ **不动 CppTLM/CppHDL 任何文件**（本次仅 ChipForge 仓库范围）
- ❌ **不修改 D1-D5 决策**（仅承认事实上的 Accepted，不重开决策讨论）
- ❌ **不删除 plugin-framework-revision-plan.md v1.0**（用户未决定是否执行，保留 Draft 状态）
- ❌ **不创建 Plugin-style 教程**（仅架构规范，how-to 留给 docs/guides/ 后续 Phase）
- ❌ **不引入 Doxygen 自动化**（`cf_plugin.md` 已经是手动维护，本计划不改变）
- ❌ **不实现 BundleMapper / CompareDriver / ScoreBoard / RTL 生成**（明确推迟到 Phase 6）
- ❌ **不做 6 头文件大改**（仅修改 1 个状态注释：`plugin_base.h:71` 的 `= delete tick()` 保持）

### Spec Framework Integration

> *Omit this section entirely if no SDD framework is detected in the target repository.*

- **Detected Framework**: None（未检测到 OpenSpec / Spec Kit / BMAD 框架目录）
- **本计划不涉及 SDD 框架集成**

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** - ALL verification is agent-executed.

### Test Decision

- **Infrastructure exists**: YES（51/51 单元测试已存在，本次不动）
- **Automated tests**: Tests-after（51 个测试在 Phase 0 已写完，零 TODO 残留）
- **Framework**: 自定义 `main() + assert`（无 GoogleTest 依赖）
- **If TDD**: 不适用（本次纯文档）

### QA Policy

每项修改任务 MUST 包含 agent-executed QA 场景（详见各 TODO）。

主要 QA 工具：
- **文档链接检查**：使用 `markdown-link-check`（如可用）或 `grep` 验证 cross-reference 完整性
- **脚本验证**：`bash tools/verify_adr.sh` 输出必须符合预期
- **代码未触动**：`git diff --stat` 确认 `include/cf/plugin/` 与 `src/cf_plugin/tests/` 仅注释级变化（应为 0 行）
- **状态数字一致**：`grep -c` 验证 ADR 表中"已实现"与"🚧"数量加和 = 38

Evidence 保存到 `.omo/evidence/plugin-docs-extraction-task-{N}.log`。

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately - foundation, no dependencies):
├── Task 1: Create plugin-framework.md skeleton (6 sections, ~580 lines)
├── Task 2: Fix verify_adr.sh script (3 defects: ADR-027/031/033)
└── Task 3: Snapshot pre-change state (git baseline, evidence baseline)

Wave 2 (After Wave 1 - content creation + cross-references):
├── Task 4: Fill plugin-framework.md §1-2 (design rationale + 5 P0 API summaries)
├── Task 5: Fill plugin-framework.md §3-5 (lifecycle, CppTLM/CppHDL relation, roadmap)
└── Task 6: Update cf_plugin.md header (1-line architecture pointer)

Wave 3 (After Wave 2 - dependent edits, MAX PARALLEL):
├── Task 7: Update declarative-hybrid-framework.md (remove §4/§7, bump to v2.1.0)
├── Task 8: Update code-framework-mapping.md §7.3 (fix "0 行" claim)
└── Task 9: Update adr.md ADR-025~033 (status correction)

Wave 4 (After Wave 3 - validation):
├── Task 10: Run verify_adr.sh + manual cross-reference check
├── Task 11: Update plugin-framework-revision-plan.md v1.0 (mark superseded by this plan, or add Section 11 linking to new plan)
└── Task 12: Generate final report + evidence bundle

Wave FINAL (After Wave 4 — 4 parallel reviews):
├── Task F1: Plan compliance audit (oracle) — read all 6 changed files, verify scope
├── Task F2: Documentation quality review (unspecified-high) — check links, no broken refs
├── Task F3: Real manual QA — run verify_adr.sh, grep cross-refs, confirm 0 code changes
└── Task F4: Scope fidelity check (deep) — confirm no creep into CppTLM/CppHDL
-> Present results -> Get explicit user okay

Critical Path: Task 1 → Task 4 → Task 7 → Task 10 → F1-F4 → user okay
Parallel Speedup: ~40% faster than sequential
Max Concurrent: 3 (Waves 1 & 3)
```

### Dependency Matrix

- **1**: - - 4, 5
- **2**: - - 10
- **3**: - - 10
- **4**: 1 - 7, 9
- **5**: 1 - 7, 9
- **6**: - - 10
- **7**: 4, 5 - 10, 11
- **8**: - - 10
- **9**: 4, 5 - 10
- **10**: 2, 3, 6, 7, 8, 9 - 11, 12, F1-F4
- **11**: 10 - 12
- **12**: 10, 11 - F1-F4

### Agent Dispatch Summary

- **Wave 1**: 3 tasks (`quick` for skeleton/script, `git` for snapshot)
- **Wave 2**: 3 tasks (`writing` for §1-2, `writing` for §3-5, `quick` for cf_plugin.md)
- **Wave 3**: 3 tasks (`writing` × 2, `quick` for ADR status)
- **Wave 4**: 3 tasks (`quick` × 2, `unspecified-high` for cross-ref check)
- **FINAL**: 4 reviews (`oracle` + `unspecified-high` + `unspecified-high` + `deep`)

---

## TODOs

> Implementation + Test = ONE Task. Never separate.
> **FORMAT**: Task labels use bare numbers: `1.`, `2.`, `3.` — NOT `T1.`, `Task 1.`, `Phase 1:`.
> Final Wave labels use `F1.`, `F2.` format.

- [x] 1. 创建 `plugin-framework.md` 骨架

  **What to do**:
  - 新建 `docs/architecture/plugin-framework.md`，文件头含：版本/日期/状态/关联决策/适用范围
  - 创建 6 个章节锚点（无内容）：§1 设计动机 / §2 5 个 P0 组件 / §3 生命周期 / §4 与 CppTLM/CppHDL 关系 / §5 路线图锚点 / §6 文档维护规则
  - 章节大纲（占位行）

  **Must NOT do**:
  - 不写实质性内容（留给 Task 4-5）
  - 不复制 `declarative-hybrid-framework.md` §4/§7 原文（要重写+修正状态）

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 文档骨架创建，纯结构任务
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1（与 Task 2/3 并行）
  - **Blocks**: Task 4, Task 5
  - **Blocked By**: None

  **References**:
  - **Pattern References**:
    - `docs/architecture/declarative-hybrid-framework.md:26-44` — 文档结构范例（章节编号 + 锚点）
  - **External References**:
    - VexRiscv `Plugin.scala` 25 行 — Plugin 极简接口范本
  - **WHY Each Reference Matters**:
    - declarative-hybrid-framework.md 提供文档结构约定（章节编号、目录、状态标注）
    - VexRiscv Plugin.scala 是设计哲学的灵感来源（必须出现在 §1）

  **Acceptance Criteria**:
  - [ ] 文件存在且非空
  - [ ] 含 6 个章节锚点（`## 1.` 到 `## 6.`）
  - [ ] 头部含版本号/日期/状态字段
  - [ ] 包含"关联决策"链接指向 `.omo/drafts/decision-plugin-framework-2026-06-08.md`

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 骨架文件存在且结构正确
    Tool: Bash (file checks)
    Preconditions: 工作目录 = /workspace/project/ChipForge
    Steps:
      1. test -f docs/architecture/plugin-framework.md
      2. grep -c "^## " docs/architecture/plugin-framework.md >= 6
      3. grep -E "版本|日期|状态" docs/architecture/plugin-framework.md | head -3
    Expected Result: 3 步全部 PASS
    Failure Indicators: 文件缺失/章节数 < 6/缺头部字段
    Evidence: .omo/evidence/plugin-docs-extraction-task-1-skeleton.log
  ```

  **Commit**: YES
  - Message: `docs(adr): create plugin-framework.md skeleton`
  - Files: `docs/architecture/plugin-framework.md`

- [x] 2. 修复 `tools/verify_adr.sh` 3 处脚本缺陷

  **What to do**:
  - **缺陷 1**: ADR-027 (`enum class Phase`) 脚本期望"找不到 = EXPECTED_MISSING"，但 `pipe_builder.h:38` 实际已定义。修复：脚本应区分"枚举在正确命名空间（`cf::plugin::`）"与"在其他位置"
  - **缺陷 2**: ADR-031 (StageLink/CtrlLink/DirectLink) 脚本检查"class StageLink|CtrlLink|DirectLink"，但只有 `CtrlLink` 实现。修复：拆分为 3 个独立检查，CtrlLink 标记 STALE（应改为 ✅），其他 2 个保持 EXPECTED_MISSING
  - **缺陷 3**: ADR-033 (CtrlLink 4 API) 脚本搜索路径不包含 `include/cf/plugin/ctrl_link.h`。修复：扩展搜索路径

  **Must NOT do**:
  - 不修改其他 ADR 的检查逻辑（仅这 3 处）
  - 不修改脚本的输出格式

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 脚本缺陷修复，单文件多行修改
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**:
  - **Pattern References**:
    - `tools/verify_adr.sh:300-340` — ADR-031/033 检查代码段
    - `include/cf/plugin/pipe_builder.h:38-42` — `enum class Phase` 实际位置
    - `include/cf/plugin/ctrl_link.h:24-89` — `class CtrlLink` 实际位置
  - **API/Type References**: 不适用
  - **External References**: 不适用
  - **WHY Each Reference Matters**:
    - 脚本行号定位缺陷位置
    - pipe_builder.h / ctrl_link.h 验证修复后脚本能识别

  **Acceptance Criteria**:
  - [ ] 修复后 `bash tools/verify_adr.sh` 输出 ADR-027 为 🚧 EXPECTED_MISSING（实际仍 EXPECTED_MISSING 因为 enum Phase 是局部命名空间，脚本可能不检查）
  - [ ] 修复后 ADR-031 输出：CtrlLink → ✅ Phase 0, StageLink → 🚧, DirectLink → 🚧
  - [ ] 修复后 ADR-033 输出 ✅ Phase 0
  - [ ] 总 STALE 数从 5 降到 0

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 脚本修复后 STALE 归零
    Tool: Bash
    Preconditions: 工作目录 = /workspace/project/ChipForge
    Steps:
      1. cp tools/verify_adr.sh tools/verify_adr.sh.bak
      2. (执行 3 处修复)
      3. bash tools/verify_adr.sh 2>&1 | grep -c "STALE"
    Expected Result: 0
    Failure Indicators: 仍 > 0
    Evidence: .omo/evidence/plugin-docs-extraction-task-2-script.log

  Scenario: 修复后 CtrlLink 状态正确
    Tool: Bash
    Preconditions: 脚本已修复
    Steps:
      1. bash tools/verify_adr.sh 2>&1 | grep "ADR-031\|ADR-033"
    Expected Result: ADR-031 显示 3 个子项（CtrlLink ✅, StageLink 🚧, DirectLink 🚧）；ADR-033 显示 ✅
    Failure Indicators: 仍显示 STALE
    Evidence: .omo/evidence/plugin-docs-extraction-task-2-script.log
  ```

  **Commit**: YES
  - Message: `chore(scripts): fix verify_adr.sh 3 defects (ADR-027/031/033)`
  - Files: `tools/verify_adr.sh`

- [x] 3. 快照变更前基线状态

  **What to do**:
  - 记录当前 `verify_adr.sh` 输出到 `state/plugin-docs-baseline.txt`
  - 记录 `git status` 干净状态确认
  - 记录当前 ADR 数量（38 条）和状态分布
  - 创建证据目录 `.omo/evidence/plugin-docs-extraction/`

  **Must NOT do**:
  - 不 commit 任何 state 文件（仅本地记录）

  **Recommended Agent Profile**:
  - **Category**: `git`
    - Reason: 状态快照，无修改
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**: 不适用

  **Acceptance Criteria**:
  - [ ] `state/plugin-docs-baseline.txt` 存在
  - [ ] 含 `verify_adr.sh` 完整输出
  - [ ] `.omo/evidence/plugin-docs-extraction/` 目录已创建

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 基线快照完整
    Tool: Bash
    Steps:
      1. test -f state/plugin-docs-baseline.txt
      2. wc -l state/plugin-docs-baseline.txt
    Expected Result: 文件存在且 >= 50 行
    Evidence: .omo/evidence/plugin-docs-extraction-task-3-baseline.log
  ```

  **Commit**: NO（state/ 在 .gitignore 中）

- [x] 4. 填充 `plugin-framework.md` §1 设计动机 + §2 5 个 P0 组件 API 摘要

  **What to do**:
  - §1 设计动机：解释为什么需要 Plugin 范式（参考决策文档 §2.1-2.3），从"实现风格替代"的视角
  - §2 5 个 P0 组件 API 摘要（**不是 API 完整参考**，那是 `cf_plugin.md` 的职责）：
    - §2.1 PluginBase（85 行摘要）— 来自 `include/cf/plugin/plugin_base.h`
    - §2.2 Payload<T> + PayloadStore（80 行摘要）— 来自 `payload.h`
    - §2.3 PipeNode（90 行摘要）— 来自 `pipe_node.h`
    - §2.4 PipeBuilder + Phase（90 行摘要）— 来自 `pipe_builder.h`
    - §2.5 CtrlLink（80 行摘要）— 来自 `ctrl_link.h`
    - §2.6 uint_t<N> / bool_t（30 行摘要）— 来自 `uint_t.h`
  - 每个组件包含：设计意图 + 公共 API 列表（不含完整方法签名）+ 头文件位置 + 单元测试位置
  - 标注：完整 API 见 `docs/api/cf_plugin.md`

  **Must NOT do**:
  - 不复制 `cf_plugin.md` 全文（会重复）
  - 不重写代码（仅引用）

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 文档内容撰写
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 7, Task 9
  - **Blocked By**: Task 1

  **References**:
  - **Pattern References**:
    - `docs/api/cf_plugin.md:1-290` — 完整 API 参考（§2 应**摘要**而非复制）
    - `.omo/drafts/decision-plugin-framework-2026-06-08.md:60-105` — 设计动机论述
  - **API/Type References**:
    - `include/cf/plugin/plugin_base.h:48-72` — PluginBase 公共 API
    - `include/cf/plugin/payload.h:71-87` — Payload<T> 公共 API
    - `include/cf/plugin/pipe_node.h:29-122` — PipeNode 公共 API
    - `include/cf/plugin/pipe_builder.h:53-133` — PipeBuilder 公共 API
    - `include/cf/plugin/ctrl_link.h:24-89` — CtrlLink 公共 API
    - `include/cf/plugin/uint_t.h:27-46` — uint_t<N> / bool_t 定义
  - **WHY Each Reference Matters**:
    - cf_plugin.md 是 API 权威（应**指向**而非**重复**）
    - 决策文档是 §1 设计动机的源材料
    - 6 个头文件是 §2.1-2.6 摘要的源

  **Acceptance Criteria**:
  - [ ] §1 含 5 个关键论点（替换 tick()、类型安全、声明式、生命周期、Phase 调度）
  - [ ] §2.1-2.6 全部存在且每个引用对应头文件
  - [ ] §2 末尾有 "完整 API 见 cf_plugin.md" 链接
  - [ ] §2 总行数约 450-500 行

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: §1 含关键论点
    Tool: Bash (grep)
    Preconditions: plugin-framework.md 已含 §1
    Steps:
      1. awk '/^## 1\./,/^## 2\./' docs/architecture/plugin-framework.md > /tmp/s1.md
      2. for kw in "tick()" "类型安全" "声明式" "生命周期" "Phase"; do
           grep -q "$kw" /tmp/s1.md || { echo "MISSING: $kw"; exit 1; }
         done
    Expected Result: 5 关键词全部存在
    Evidence: .omo/evidence/plugin-docs-extraction-task-4-s1.log

  Scenario: §2 5 个组件全部覆盖
    Tool: Bash
    Steps:
      1. awk '/^## 2\./,/^## 3\./' docs/architecture/plugin-framework.md > /tmp/s2.md
      2. for sec in "2.1 PluginBase" "2.2 Payload" "2.3 PipeNode" "2.4 PipeBuilder" "2.5 CtrlLink" "2.6 uint_t"; do
           grep -q "^### $sec" /tmp/s2.md || { echo "MISSING: $sec"; exit 1; }
         done
    Expected Result: 6 子章节全部存在
    Evidence: .omo/evidence/plugin-docs-extraction-task-4-s2.log
  ```

  **Commit**: YES
  - Message: `docs(plugin): fill §1-2 design + 5 P0 API summaries`
  - Files: `docs/architecture/plugin-framework.md`

- [x] 5. 填充 `plugin-framework.md` §3-6 (lifecycle + CppTLM 关系 + roadmap + 维护规则)

  **What to do**:
  - §3 生命周期: 5 个 P0 组件的完整生命周期时序图 (ctor → setup → build → run),含 HelloPlugin 示例
  - §4 与 CppTLM/CppHDL 关系:
    - §4.1 与 `cpptlm::ChStreamModuleBase` 的正交性 (重写原 §4.8 关系表)
    - §4.2 与 `ch::Component` 的正交性
    - §4.3 与 CppHDL chlib 命名共存 (halt_when vs stream_halt_when,引用 D6 决策)
  - §5 路线图锚点: Phase 0 (已完) / Phase 1 (L1CachePlugin Hello World) / Phase 6 (完整 PipeBuilder)
  - §6 文档维护规则: 版本号规则、状态变更流程、ADR 同步规则

  **Must NOT do**:
  - 不写完整代码示例 (指向 `cf_plugin.md` 和 `test_*.cpp`)

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 设计文档主体撰写
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 7, Task 9
  - **Blocked By**: Task 1

  **References**:
  - **Pattern References**:
    - `declarative-hybrid-framework.md:506-549` — §4.8 Plugin 与 ChStreamModuleBase 关系 (重写为新文档的 §4.1)
    - `declarative-hybrid-framework.md:1219-1386` — §12 路线图 (摘取路线图锚点到 §5)
    - `docs/roadmap/phases/phase-0-plugin-scaffolding.md:160-189` — 接口稳定性承诺 (摘取到 §5)
  - **External References**:
    - VexRiscv Plugin.scala — 生命周期范例
  - **WHY Each Reference Matters**:
    - §4.8 是正交性论述的源
    - §12 是路线图的源
    - phase-0 文档是接口承诺的源

  **Acceptance Criteria**:
  - [ ] §3 含时序图 (ctor → setup → build → run)
  - [ ] §4.1 包含与 ChStreamModuleBase 正交性表 (5 维度对比)
  - [ ] §4.3 明确 CtrlLink::halt_when 与 chlib::stream_halt_when 共存 (D6 决策)
  - [ ] §5 含 3 个 Phase 状态 (0 = ✅, 1 = 🚧, 6 = 🚧)
  - [ ] §6 含 4 条维护规则
  - [ ] 总行数达 ~580

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: §3 生命周期时序图存在
    Tool: Bash (grep)
    Steps:
      1. awk '/^## 3\./,/^## 4\./' docs/architecture/plugin-framework.md > /tmp/s3.md
      2. for kw in "ctor" "setup" "build" "run"; do
           grep -q "$kw" /tmp/s3.md || { echo "MISSING: $kw"; exit 1; }
         done
    Expected Result: 4 关键词全部存在
    Evidence: .omo/evidence/plugin-docs-extraction-task-5-s3.log

  Scenario: §4.3 命名共存已说明
    Tool: Bash
    Steps:
      1. grep -A 2 "D6 决策\|stream_halt_when.*CtrlLink" docs/architecture/plugin-framework.md
    Expected Result: 含共存说明
    Evidence: .omo/evidence/plugin-docs-extraction-task-5-s4.log
  ```

  **Commit**: YES
  - Message: `docs(plugin): fill §3-6 lifecycle + CppTLM relation + roadmap + maintenance`
  - Files: `docs/architecture/plugin-framework.md`

- [x] 6. 在 `cf_plugin.md` 头部加架构导引链接

  **What to do**:
  - 在 `docs/api/cf_plugin.md` 第 7 行后 (版本字段之后) 添加一行:
    ```
    > **架构权威**: 详见 [`docs/architecture/plugin-framework.md`](../../architecture/plugin-framework.md) (设计意图、生命周期、ADR 摘要)。本文档仅描述 API 签名。
    ```

  **Must NOT do**:
  - 不修改 `cf_plugin.md` 其他内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 1 行修改
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**:
  - **Pattern References**:
    - `docs/api/cf_plugin.md:1-10` — 头部结构
  - **WHY Each Reference Matters**:
    - 头部是插入点

  **Acceptance Criteria**:
  - [ ] `cf_plugin.md` 含 "架构权威" 链接
  - [ ] 文件总行数 +1

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 架构导引已添加
    Tool: Bash (grep)
    Steps:
      1. grep "架构权威" docs/api/cf_plugin.md
      2. grep "plugin-framework.md" docs/api/cf_plugin.md
    Expected Result: 两次 grep 均命中
    Evidence: .omo/evidence/plugin-docs-extraction-task-6-pointer.log
  ```

  **Commit**: YES
  - Message: `docs(api): add architecture pointer to cf_plugin.md`
  - Files: `docs/api/cf_plugin.md`

- [x] 7. 从 `declarative-hybrid-framework.md` 移除 §4/§7 并升级到 v2.1.0

  **What to do**:
  - 删除 §4 全部 (4.1-4.8, ~250 行)
  - 删除 §7 全部 (7.1-7.6, ~200 行)
  - 在原 §4 位置替换为 1 段 cross-reference (指向 plugin-framework.md)
  - 在原 §7 位置加同样 cross-reference
  - 文档头版本号: 2.0.3 → 2.1.0
  - 文档头日期: 2026-06-08 → 2026-06-09
  - 文档头 "v2.0.3 变更" 行下新增 "v2.1.0 变更" 行

  **保留**:
  - §4.8 (Plugin 与 ChStreamModuleBase 关系) — 跨抽象层关系,不属于插件架构内部
  - §11.1 ADR 决策执行状态表 — 仍需更新 ADR 状态
  - §12.0 路线图 — 仍需引用 plugin-framework.md

  **Must NOT do**:
  - 不重写 §1-3/§5-6/§8-12 内容
  - 不修改 §4.8

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 大型文档删除 + 替换
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: Task 10
  - **Blocked By**: Task 4, Task 5

  **References**:
  - **Pattern References**:
    - `declarative-hybrid-framework.md:411-549` — §4 全文 (待删除)
    - `declarative-hybrid-framework.md:869-931` — §7 全文 (待删除)
    - `declarative-hybrid-framework.md:506-549` — §4.8 (**保留**)
  - **WHY Each Reference Matters**:
    - 定位删除范围
    - 识别保留边界

  **Acceptance Criteria**:
  - [ ] 文档总行数从 1390 降到约 990 行 (删除 ~400 行)
  - [ ] `grep -c "100% not in code" declarative-hybrid-framework.md` = 0
  - [ ] 文档头版本号 = v2.1.0
  - [ ] §4.8 内容保留

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: §4/§7 已替换为 cross-reference
    Tool: Bash (grep)
    Preconditions: plugin-framework.md 已存在
    Steps:
      1. grep -c "100% not in code" docs/architecture/declarative-hybrid-framework.md
      2. grep -c "Plugin 架构已抽离" docs/architecture/declarative-hybrid-framework.md
      3. wc -l docs/architecture/declarative-hybrid-framework.md
    Expected Result: 第 1 步 = 0; 第 2 步 ≥ 2; 第 3 步 ≈ 990
    Evidence: .omo/evidence/plugin-docs-extraction-task-7-extract.log

  Scenario: §4.8 内容保留
    Tool: Bash
    Steps:
      1. grep "Plugin 不替代 SimObject" docs/architecture/declarative-hybrid-framework.md
    Expected Result: 命中
    Evidence: .omo/evidence/plugin-docs-extraction-task-7-section48.log
  ```

  **Commit**: YES
  - Message: `docs(arch): extract §4/§7 to plugin-framework.md (v2.1.0)`
  - Files: `docs/architecture/declarative-hybrid-framework.md`

- [x] 8. 修正 `code-framework-mapping.md` §7.3 插件实现状态

  **What to do**:
  - 定位 §7.3 表格 "PipeNode / PipeLink / PipeBuilder" 行
  - 修改为: `cf::plugin 5 个 P0 组件` + 6 头文件/667 行/51 测试 + ✅ Phase 0 已完成
  - 在 §7.4 (文档与代码背离) 下新增一行条目: **已修复** (2026-06-09)
  - §7.5 "建议的修正优先级" 第 5 项标记为 **已执行**

  **Must NOT do**:
  - 不修改 §1-6 内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 表格修改 + 1 处追加
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**:
  - **Pattern References**:
    - `code-framework-mapping.md:391-398` — §7.3 原表
    - `code-framework-mapping.md:399-409` — §7.4 原表
    - `code-framework-mapping.md:410-417` — §7.5 原表

  **Acceptance Criteria**:
  - [ ] §7.3 含 "✅ Phase 0 已完成 (2026-06-08)" 状态
  - [ ] §7.4 含新增的 "已修复" 行 (2026-06-09)
  - [ ] §7.5 第 5 项标记 "已执行"
  - [ ] 数字正确: 6 头文件 / 667 行 / 51 测试

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: §7.3 状态已修正
    Tool: Bash (grep)
    Steps:
      1. grep "cf::plugin 5 个 P0" docs/architecture/code-framework-mapping.md
      2. grep "0 行代码, 0 头文件" docs/architecture/code-framework-mapping.md
    Expected Result: 第 1 步命中; 第 2 步 = 0
    Evidence: .omo/evidence/plugin-docs-extraction-task-8-mapping.log
  ```

  **Commit**: YES
  - Message: `docs(mapping): fix §7.3 plugin implementation status`
  - Files: `docs/architecture/code-framework-mapping.md`

- [x] 9. 修正 `adr.md` ADR-025~033 状态

  **What to do**:
  - 修正表 2.1 (已实现决策): 追加 7 条新行 (ADR-025/026/027/028/030/032/033 均为 ✅; 029/031 仍 🚧)
  - 修正表 2.3 (Phase 1 提案决策): 删除 7 条行
  - 修正表 2.4 统计: ✅ 数从 23 升到 30; 🚧 数从 14 降到 8; 实现率 79%
  - 修正 §3 详细记录中各 ADR 的 "状态" 字段
  - 修正 ADR-031 详细记录: 状态保持 🚧,但措辞改为 "Phase 0 仅 CtrlLink 已完成;StageLink/DirectLink/PipeLink 基类推迟到 Phase 6"
  - 修正 ADR-033 详细记录: 状态改为 ✅ Phase 0 P0 #5
  - 修正 ADR-031/033 的 "代码锚点" 小节,添加 `include/cf/plugin/ctrl_link.h` 路径
  - 在每条 ✅ ADR 的 "决策依据" 添加指针: "完整设计见 plugin-framework.md §2.N"

  **特殊处理**:
  - ADR-029 (ImplMode) 保持 🚧 Phase 6
  - ADR-007 (TLM↔RTL 通用桥接) 保持 🚧
  - ADR-024 (Bundle 三层分层) 保持 ⚠️

  **Must NOT do**:
  - 不修改 ADR-001~024 内容
  - 不修改 ADR-034~039 内容
  - 不修改 ADR-037 内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 表格 + 状态字段批量修改
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: Task 10
  - **Blocked By**: Task 4, Task 5

  **References**:
  - **Pattern References**:
    - `adr.md:77-103` — 表 2.1 已实现决策
    - `adr.md:111-128` — 表 2.3 Phase 1 提案决策
    - `adr.md:130-146` — 表 2.4 统计
    - `adr.md:758-963` — ADR-025~033 详细记录

  **Acceptance Criteria**:
  - [ ] 表 2.1 含 30 条行 (含 7 条新 ✅)
  - [ ] 表 2.3 仅含 8 条行
  - [ ] 表 2.4 统计: ✅ = 30, ⚠️ = 1, 🚧 = 8, 合计 = 38
  - [ ] 实现率 = 30/38 = 79%
  - [ ] §3 详细记录中 5 条状态改为 ✅, 1 条措辞调整 (ADR-031)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: ADR 状态分布正确
    Tool: Bash (awk + grep)
    Steps:
      1. awk '/^### 2.1/,/^### 2.2/' docs/architecture/adr.md | grep -c "^| ADR-"
      2. awk '/^### 2.3/,/^### 2.4/' docs/architecture/adr.md | grep -c "^| ADR-"
      3. grep "实现率" docs/architecture/adr.md
    Expected Result: 第 1 步 = 30; 第 2 步 = 8; 第 3 步 = 79%
    Evidence: .omo/evidence/plugin-docs-extraction-task-9-adr.log

  Scenario: ADR-031 措辞已修正
    Tool: Bash
    Steps:
      1. awk '/^#### ADR-031/,/^#### ADR-032/' docs/architecture/adr.md > /tmp/adr031.md
      2. grep -c "Phase 0 仅 CtrlLink" /tmp/adr031.md
    Expected Result: ≥ 1
    Evidence: .omo/evidence/plugin-docs-extraction-task-9-adr031.log
  ```

  **Commit**: YES
  - Message: `docs(adr): correct ADR-025~033 status (Phase 0 done)`
  - Files: `docs/architecture/adr.md`

- [ ] 10. 运行 `verify_adr.sh` + 手动 cross-reference 完整性检查

  **What to do**:
  - `bash tools/verify_adr.sh 2>&1 | tee state/post-change-adr-verify.log`
  - 验证 STALE 数 = 0
  - 验证 EXPECTED_MISSING 数 ≤ 9
  - grep 5 个 cross-reference 链接都存在
  - 51/51 单元测试通过

  **Must NOT do**:
  - 不修改任何文档 (仅验证)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 综合验证 + 报告
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4 (独立)
  - **Blocks**: Task 11, Task 12
  - **Blocked By**: Task 2, Task 3, Task 6, Task 7, Task 8, Task 9

  **References**:
  - **Pattern References**:
    - `tools/verify_adr.sh:1-100` — 脚本入口
    - `state/plugin-docs-baseline.txt` — 变更前基线

  **Acceptance Criteria**:
  - [ ] `bash tools/verify_adr.sh` 退出码 = 0
  - [ ] STALE 数 = 0
  - [ ] 5 个文件含 plugin-framework.md 引用
  - [ ] 51/51 单元测试通过

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: ADR 验证脚本零 STALE
    Tool: Bash
    Steps:
      1. bash tools/verify_adr.sh 2>&1 | tee /tmp/adr.log
      2. grep -c "STALE" /tmp/adr.log
    Expected Result: 第 2 步 = 0
    Evidence: .omo/evidence/plugin-docs-extraction-task-10-adr.log

  Scenario: 跨引用图完整
    Tool: Bash
    Steps:
      1. for f in docs/architecture/declarative-hybrid-framework.md \
                 docs/architecture/code-framework-mapping.md \
                 docs/architecture/adr.md \
                 docs/api/cf_plugin.md \
                 docs/architecture/plugin-framework.md; do
           grep -q "plugin-framework.md" "$f" || { echo "MISSING in: $f"; exit 1; }
         done
    Expected Result: 5 个文件均 PASS
    Evidence: .omo/evidence/plugin-docs-extraction-task-10-crossref.log
  ```

  **Commit**: NO (state/ 在 .gitignore 中)

- [ ] 11. 在 `plugin-framework-revision-plan.md` 添加 Section 11 指向本计划

  **What to do**:
  - 在 `plugin-framework-revision-plan.md` 末尾添加 Section 11 "相关计划",链接到本计划

  **Must NOT do**:
  - 不修改 plan v1.0 Draft 状态
  - 不重写 plan 其他章节

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 1 节追加
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: Task 12
  - **Blocked By**: Task 10

  **References**:
  - **Pattern References**:
    - `plugin-framework-revision-plan.md:421-431` — 原 Section 10 (计划状态变更历史)

  **Acceptance Criteria**:
  - [ ] Section 11 存在且含链接到 plugin-docs-extraction.md
  - [ ] 不修改 plan 其他内容

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Section 11 已添加
    Tool: Bash (grep)
    Steps:
      1. grep -A 1 "^## 11\." .omo/plans/plugin-framework-revision-plan.md
    Expected Result: 含 "相关计划" 标题
    Evidence: .omo/evidence/plugin-docs-extraction-task-11-pointer.log
  ```

  **Commit**: YES
  - Message: `docs: update revision plan to reference new plan`
  - Files: `.omo/plans/plugin-framework-revision-plan.md`

- [x] 12. 生成最终报告 + 证据包

  **What to do**:
  - 收集 12 个任务的 evidence 文件到 `.omo/evidence/plugin-docs-extraction/`
  - 生成 `state/final-report-plugin-docs-extraction.md` 总结:
    - 变更文件清单 (6 个) + 行数变化
    - STALE 数从 5 → 0
    - 实现率从 63% → 79%
    - 51/51 测试仍通过
  - 更新 `.omo/drafts/decision-plugin-framework-2026-06-08.md` 状态历史: Proposed → Accepted

  **Must NOT do**:
  - 不修改决策文档的 "决策内容" 或 "影响" 部分 (仅追加变更历史行)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 总结报告 + 状态更新
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4 (独立)
  - **Blocks**: F1-F4
  - **Blocked By**: Task 10, Task 11

  **References**:
  - **Pattern References**:
    - `.omo/drafts/decision-plugin-framework-2026-06-08.md:382-388` — 状态变更历史

  **Acceptance Criteria**:
  - [ ] `state/final-report-plugin-docs-extraction.md` 存在
  - [ ] 12 个 evidence 文件全部存在
  - [ ] 决策文档状态从 Proposed → Accepted

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 最终报告完整
    Tool: Bash
    Steps:
      1. test -f state/final-report-plugin-docs-extraction.md
      2. ls .omo/evidence/plugin-docs-extraction/ | wc -l
      3. grep "Accepted" .omo/drafts/decision-plugin-framework-2026-06-08.md
    Expected Result: 第 1 步 PASS; 第 2 步 = 12; 第 3 步 ≥ 1
    Evidence: .omo/evidence/plugin-docs-extraction-task-12-final.log
  ```

  **Commit**: YES
  - Message: `docs: final evidence bundle for plugin docs extraction`
  - Files: `state/final-report-plugin-docs-extraction.md`, `.omo/drafts/decision-plugin-framework-2026-06-08.md`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE.

- [ ] F1. **Plan Compliance Audit** — `oracle`
  读取所有 6 个变更文件，验证：
  - `plugin-framework.md` §1-5 完整存在
  - `declarative-hybrid-framework.md` §4/§7 已删除，cross-ref 存在
  - `code-framework-mapping.md` §7.3 数字正确（6 头文件/667 行/51 测试）
  - `adr.md` 5 条 STALE 已修正
  - `cf_plugin.md` 1 行架构导引存在
  - `verify_adr.sh` 3 处缺陷已修复
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Documentation Quality Review** — `unspecified-high`
  检查文档质量：
  - 所有跨引用可点击（`grep -E "\\[.*\\]\\([^\\)]+\\)"` 在每个文件中能找到目标）
  - 无断链（`markdown-link-check` 或 grep 验证）
  - 状态数字一致（ADR 38 = 23 ✅ + 1 ⚠️ + 14 🚧，但 5 STALE 已修复意味着新的"已实现"集合）
  - 表格行数与文中描述一致
  Output: `Links [N/N valid] | Consistency [PASS/FAIL] | VERDICT`

- [ ] F3. **Real Manual QA** — `unspecified-high`
  执行端到端验证：
  - `bash tools/verify_adr.sh` 输出符合预期
  - `git diff --stat` 确认 `include/cf/plugin/` 与 `src/cf_plugin/tests/` 0 行变化
  - `grep -c "100% not in code"` 在 `declarative-hybrid-framework.md` 应为 0
  - `grep -c "0 行代码, 0 头文件"` 在 `code-framework-mapping.md` 应为 0
  - 51/51 单元测试仍通过（`ctest --test-dir build --output-on-failure`）
  Output: `Scenarios [N/N pass] | VERDICT`

- [ ] F4. **Scope Fidelity Check** — `deep`
  范围合规：
  - 6 个变更文件之外的文件 0 行变化
  - 没有 CppTLM/CppHDL 仓库的文件被触碰
  - 没有引入 Doxygen 自动化（Must NOT Have 第 6 项）
  - 没有重写任何头文件实现（仅注释级或状态级）
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N] | VERDICT`

---

## Commit Strategy

每个 Wave 完成后独立 commit（便于回滚）：

- **Wave 1** (基础):
  - `docs(adr): create plugin-framework.md skeleton` — `plugin-framework.md`
  - `chore(scripts): fix verify_adr.sh 3 defects (ADR-027/031/033)` — `tools/verify_adr.sh`
  - `chore: snapshot pre-change state` — `state/plugin-docs-baseline.txt`
- **Wave 2** (内容):
  - `docs(plugin): fill §1-2 design + 5 P0 API summaries` — `plugin-framework.md`
  - `docs(plugin): fill §3-5 lifecycle + CppTLM relation + roadmap` — `plugin-framework.md`
  - `docs(api): add architecture pointer to cf_plugin.md` — `docs/api/cf_plugin.md`
- **Wave 3** (联动):
  - `docs(arch): extract §4/§7 to plugin-framework.md (v2.1.0)` — `declarative-hybrid-framework.md`
  - `docs(mapping): fix §7.3 plugin implementation status` — `code-framework-mapping.md`
  - `docs(adr): correct ADR-025~033 status (Phase 0 done)` — `adr.md`
- **Wave 4** (验证):
  - `docs: update revision plan to reference new plan` — `plugin-framework-revision-plan.md`
  - `docs: final evidence bundle for plugin docs extraction` — `state/evidence-plugin-docs.txt`

每个 commit 独立可回滚（cherry-pick revert）。

---

## Success Criteria

### Verification Commands

```bash
# 1. 验证 ADR 验证脚本输出无 STALE
bash tools/verify_adr.sh 2>&1 | grep -E "STALE|EXPECTED_MISSING" | head -5
# 预期: EXPECTED_MISSING 仅 9 个（ADR-007/029/031/034/035/036/037/039 + ImplMode 相关）
# 预期: STALE 0 个

# 2. 验证 §4/§7 已从 declarative-hybrid-framework.md 移除
grep -c "100% not in code\|Plugin 模型设计" /workspace/project/ChipForge/docs/architecture/declarative-hybrid-framework.md
# 预期: 0（不再有"100% not in code"）

# 3. 验证 code-framework-mapping.md §7.3 数字
grep "Phase 0" /workspace/project/ChipForge/docs/architecture/code-framework-mapping.md | head -3
# 预期: 提及 6 头文件 / 667 行 / 51 测试

# 4. 验证 plugin-framework.md 存在且不为空
test -s /workspace/project/ChipForge/docs/architecture/plugin-framework.md && \
  wc -l /workspace/project/ChipForge/docs/architecture/plugin-framework.md
# 预期: ~500-650 行

# 5. 验证代码零变化
git diff --stat HEAD -- include/cf/plugin/ src/cf_plugin/tests/
# 预期: 0 files changed

# 6. 验证 51/51 测试仍通过
cmake --build /workspace/project/ChipForge/build --target test_plugin_lifecycle test_payload test_pipe_node test_pipe_builder test_ctrl_link 2>&1 | tail -5
# 预期: 5/5 targets built

ctest --test-dir /workspace/project/ChipForge/build --output-on-failure -R "plugin"
# 预期: 5/5 tests passed
```

### Final Checklist

- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent（零代码改动）
- [ ] `verify_adr.sh` 输出符合预期（0 STALE）
- [ ] 51/51 单元测试通过
- [ ] 跨引用图无断链
- [ ] 5 处 fan-in 引用全部正确
- [ ] `plugin-framework.md` 状态标注正确（Phase 0 5/5 完成，Phase 6 待定）
- [ ] `plugin-framework-revision-plan.md` v1.0 已添加引用本计划的链接
- [ ] 证据文件齐全（`.omo/evidence/plugin-docs-extraction-task-*.log`）
- [ ] 用户对 F1-F4 报告显式认可
