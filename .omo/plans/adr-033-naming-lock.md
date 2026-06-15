# 锁定 ADR-033 命名冲突 + 清理废弃文档 (Phase 1 启动前 Quick 准备)

## TL;DR

> **Quick Summary**: 将 ADR-033 (CtrlLink 4-control-API) 从 🚧 提升为 ✅ Accepted，绑定 D6 共存决策（halt_when / throw_when / flush_when / bypass 4 个命名冲突），并删除已废弃的 `phase-1-foundation.md`，为 Phase 1 (L1CachePlugin) Large plan 解除前置阻塞。
>
> **Deliverables**:
> - `docs/architecture/adr.md` 中 ADR-033 主表行 + ADR-033 详细记录段 状态由 🚧 → ✅ Accepted，D6 共存方案显式绑定
> - 删除 `docs/roadmap/phases/phase-1-foundation.md`（git rm 保留历史）
> - 修正 5 处反向引用指向 `phase-1-tlm-foundation.md`
> - `docs/roadmap/roadmap-status.md` PA-3 状态推进 + 活动日志追加
>
> **Estimated Effort**: Quick (1h)
> **Parallel Execution**: NO（顺序依赖：adr.md 必先于 roadmap 同步）
> **Critical Path**: 1 → 2 → 3 → 4 → 5

---

## Context

### Original Request
Oracle 推荐：在 Phase 1 (L1CachePlugin) Large plan 启动前，先用 1h Quick 准备锁定 ADR-033 命名冲突 + 删除废弃文档，避免 Phase 1 编码时撞命名冲突被迫返工。

### Interview Summary

**Key Discussions**:
- 5 个候选下一步中，Oracle 推荐路径："先 Quick 1h（ADR-033 锁定 + 删废弃文档），再 Large 1.5w 实施 Phase 1（内部吸收 JSON 重写）"
- 用户选择"拆分为两个 plan"——本 plan 仅覆盖 Quick 准备段
- ADR-033 绑定范围决策：用户选择"绑定 4 个冲突"（halt_when / throw_when / flush_when / bypass），一次性闭环
- 废弃文档处理决策：用户选择"git rm + 修正所有引用"，零坏链
- 实际 5 处反向引用（4 在 `ip/*/README.md` + 1 在 `roadmap-status.md`）—— Oracle phase-1 gate 校正了初判（plugin-framework-revision-plan.md L250 实际在 diff 示例代码块内，非活链接）

**Research Findings**:
- `.omo/drafts/decision-plugin-framework-2026-06-08.md` §3.5 + §4.2 已完整记录 D6-D9 决策
- `adr.md` 中 ADR-033 已存在（status: 🚧，属 §3.H 流水线抽象分组，非 §3.G）
- `phase-1-foundation.md` 已有废弃标记（roadmap-status.md PA-3，2026-06-08）
- Phase 0 脚手架（51/51 tests PASS）已 2026-06-08 提交，ADR-033 锁定不需伴随代码改动

### Metis Review

**Identified Gaps** (addressed):
- **Q1 ADR-033 绑定范围** → 用户已确认绑定 4 个冲突
- **Q2 草稿来源** → `.omo/drafts/decision-plugin-framework-2026-06-08.md` 是唯一权威源
- **Q3 删除方式** → 用户已确认 `git rm`（保留 blame 历史）
- **Q4 5 处反向引用** → Oracle phase-1 gate 校正：第 5 处在 `roadmap-status.md`（自然随 PA-3 更新一并修正），非 `plugin-framework-revision-plan.md`（该 L250 在 diff 代码块内为历史记录，保留）
- **Q5 ADR status 字段约定** → 仓库用 "✅ 已实现" / "⚠️ 部分实现" / "🚧 Phase 提案"，采用 ✅ Accepted（用 "Accepted" 而非 "Locked" 与现有 §1 状态图例一致）

---

## Work Objectives

### Core Objective
在 1h 内完成 ADR-033 命名冲突决策的归档级锁定（绑定 4 个冲突），并清理已废弃的 `phase-1-foundation.md` 及其 5 处反向引用，使 Phase 1 L1CachePlugin Large plan 启动时 CtrlLink API 表面已锁定。

### Concrete Deliverables
- `docs/architecture/adr.md` ADR-033 主表行：🚧 → ✅ Accepted
- `docs/architecture/adr.md` ADR-033 详细记录段：补充 D6 决策依据 + 4 个冲突的共存规则
- `docs/roadmap/phases/phase-1-foundation.md` 文件被删除（`git rm`）
- 4 处 `ip/*/README.md` 链接 `phase-1-foundation.md` → `phase-1-tlm-foundation.md`
- 1 处 `docs/roadmap/roadmap-status.md` 链接同步（自然融入 PA-3 更新）
- `docs/roadmap/roadmap-status.md` PA-3 状态推进 + §6 活动日志追加 2026-06-09 行
- 1 个 git commit（`docs(adr): lock ADR-033 + remove deprecated phase-1-foundation`）

### Definition of Done
- [ ] `bash verify_adr033.sh` 8 项断言全部 PASS（见 TODO 1 验收标准）
- [ ] `git diff --stat HEAD~1 HEAD` 仅显示预期的 6 个文件改动（adr.md / phase-1-foundation.md 删除 / 4 ip README / roadmap-status.md）
- [ ] 负向测试：`git diff --stat HEAD~1 HEAD -- 'bundles/*' 'soc/*' 'ip/*/tlm/*'` 输出为空（Phase 1 路径未被触碰）
- [ ] `git log -1 --format=%s` 显示 `docs(adr): lock ADR-033 + remove deprecated phase-1-foundation`

### Must Have
- ADR-033 主表行 + ADR-033 详细记录段两处状态同步更新
- D6 决策对 4 个冲突（halt_when / throw_when / flush_when / bypass）显式绑定
- `.omo/drafts/decision-plugin-framework-2026-06-08.md` 保持原状（历史记录）
- 所有 5 处活链接修复为指向 `phase-1-tlm-foundation.md`
- 单个原子 commit

### Must NOT Have (Guardrails)
- **不触碰** `bundles/mem_bundles.h` / `soc/riscv_virt.json` / `ip/*/tlm/*` / L1CachePlugin 相关代码（属于 Phase 1 plan 范围）
- **不创建** 新的 ADR-034/035/036（不在本 plan 范围）
- **不修改** D7/D8/D9 内容（即使发现需细化也 defer 到 Phase 1 plan）
- **不重命名** `.omo/drafts/decision-plugin-framework-2026-06-08.md`
- **不修改** `plugin-framework-revision-plan.md` L250 的 diff 示例代码块（历史记录，保留）
- **不修改** ADR-033 内容文本（仅状态推进；如需修订则需新 ADR 取代，不在 plan 范围）
- **不运行** cmake build / ctest（纯文档 + git 操作）

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** - ALL verification is agent-executed. No exceptions.
> 接受标准仅依赖 `git` / `grep` / `test` 命令，零编译/测试运行时。

### Test Decision
- **Infrastructure exists**: N/A（本次不涉及 C++ 代码）
- **Automated tests**: 不适用（纯 docs + git 操作）
- **Framework**: N/A
- **Agent QA**: bash 一次性脚本验证

### QA Policy
每个 TODO 必须包含 grep/git 一行命令的 acceptance criteria。证据保存到 `.omo/evidence/task-{N}-{slug}.log`（命令输出 captured stdout）。

- **文档/状态**: 用 Bash（grep + git log/diff）—— 验证状态字段、文件存在性、commit 历史
- **链接修复**: 用 Bash（grep -r）—— 确认旧链接全部消失
- **负向测试**: 用 Bash（git diff --stat）—— 确认 Phase 1 路径未被触碰

---

## Execution Strategy

### Parallel Execution Waves

> 本 plan 为 Quick 1h 顺序工作流，所有 TODO 顺序依赖（前一个 commit 必是后一个 commit 的 base），不并行。
> 5 个 TODO 全部在单一 Wave 内顺序执行。

```
Wave 1 (Quick - 1h, sequential, single commit):
├── Task 1: ADR-033 状态推进 (adr.md 主表行 + ADR-033 详细记录段)
├── Task 2: git rm phase-1-foundation.md
├── Task 3: 修正 4 处 ip/*/README.md 活链接
├── Task 4: 更新 roadmap-status.md (PA-3 + 活动日志)
└── Task 5: 单个原子 commit + 负向测试验证
```

### Dependency Matrix

- **1**: - - 2, 3, 4
- **2**: 1 - 3, 4, 5
- **3**: 2 - 4, 5
- **4**: 1, 2, 3 - 5
- **5**: 1, 2, 3, 4 -

### Agent Dispatch Summary
- 5 个 TODO 全部走 `quick` agent（轻量 grep + git 操作）

---

## TODOs

- [x] 1. ADR-033 状态推进 (adr.md 主表行 + ADR-033 详细记录段)

  **What to do**:
  - 打开 `docs/architecture/adr.md`，定位 ADR-033 主表行（约 L121-125，§2.3 Phase 1 提案决策表内）和 ADR-033 详细记录段（`#### ADR-033：CtrlLink 四种控制 API`，约 L926-953，§H 流水线抽象分组）
  - 主表行第 3 列 `状态` 由 `🚧` 改为 `✅ Accepted`，第 4 列备注追加 `D6 共存方案已绑定 (4 conflicts)`
  - ADR-033 详细记录段：在"决策内容"小节后追加"**决策依据**"子节，引用 `.omo/drafts/decision-plugin-framework-2026-06-08.md` §3.5 表第 1 行 + §4.2 D6，明确 4 个冲突（halt_when / throw_when / flush_when / bypass）均为"方案 C：两者共存，明确层级差异"
  - 验证命令字段保留 `bash tools/verify_adr.sh ADR-033`（脚本中已存在该 case，无需新增）
  - **执行前**：`mkdir -p .omo/evidence`（证据目录可能不存在）

  **Must NOT do**:
  - 不修改 ADR-033 已有的"决策内容"小节文字
  - 不触碰 ADR-025~032 / 034~039 的任何行
  - 不重命名 ADR-033 编号
  - 不在 ADR-033 详细记录段 之外的其他 §3 段添加内容

  **Recommended Agent Profile**:
  - **Category**: `quick`（小范围单文件 markdown 编辑）
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `cpp-architecture`: 不适用（无 C++ 代码改动）
    - `cmake-manage`: 不适用
    - `using-superpowers`: 不触发（编辑任务无歧义）

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 1 Sequential (1st)
  - **Blocks**: 2, 3, 4, 5
  - **Blocked By**: None (可立即开始)

  **References**:

  **Pattern References**:
  - `docs/architecture/adr.md:926-953` - ADR-033 当前行（待改 🚧→✅）
  - `docs/architecture/adr.md` ADR-033 详细记录段位置
  - `.omo/drafts/decision-plugin-framework-2026-06-08.md:184-191` - D6-D9 决策原文
  - `.omo/drafts/decision-plugin-framework-2026-06-08.md:165` - §3.5 表第 1 行 halt 命名方案 C 共存

  **External References**:
  - `docs/architecture/adr.md` §1 状态图例 - "✅ 已实现" / "⚠️ 部分实现" / "🚧 Phase 提案" 三态约定

  **Acceptance Criteria**:

  ```
  # Acceptance 1: ADR-033 主表行状态已变 Accepted
  grep -E "^\| ADR-033 \|" /workspace/project/ChipForge/docs/architecture/adr.md
  # 期望输出含 "✅ Accepted" 且不再含 "🚧"

  # Acceptance 2: ADR-033 详细记录含 D6 引用（h4 锚点范围，文件实际用 `#### ADR-033` + `### I.` 边界）
  awk '/^#### ADR-033/,/^### I\./' /workspace/project/ChipForge/docs/architecture/adr.md | grep -c "D6\|chlib::stream_halt_when\|CtrlLink::halt_when"
  # 期望输出 ≥ 2（决策依据子节显式绑定 D6）

  # Acceptance 3: 4 个冲突符号全部出现
  awk '/^#### ADR-033/,/^### I\./' /workspace/project/ChipForge/docs/architecture/adr.md | grep -oE "halt_when|throw_when|flush_when|bypass" | sort -u | wc -l
  # 期望输出 = 4
  ```

  **Evidence to Capture**:
  - `.omo/evidence/task-1-adr033-status.log`（3 个 grep 命令的完整 stdout + exit code）

  **Commit**: NO（独立 commit 见 Task 5）

- [x] 2. git rm phase-1-foundation.md

  **What to do**:
  - 执行 `git rm docs/roadmap/phases/phase-1-foundation.md`（保留 git blame 历史）
  - 确认 `git status` 显示 `D  docs/roadmap/phases/phase-1-foundation.md`
  - **执行前**：先用 `grep -rn "phase-1-foundation" . --include="*.md" --include="*.sh" --include="*.py" --exclude-dir=.git` 列出全部引用（已知 5 处活链接：`ip/cache/README.md:3`、`ip/memory/README.md:3`、`ip/interconnect/README.md:3`、`ip/peripheral/README.md:3`、`docs/roadmap/roadmap-status.md:16,23,96,117`），确认 Task 3 + Task 4 会处理所有引用
  - **注**: `.omo/plans/plugin-framework-revision-plan.md:250` 的 `phase-1-foundation` 引用位于 ```diff 代码块内（历史示例），属活链接假阳性，**保留不修改**

  **Must NOT do**:
  - 不使用 `rm` 手动删除（必须 `git rm` 保留历史）
  - 不在 task 2 阶段执行 commit（commit 留到 Task 5）
  - 不修改任何 README 引用（那是 Task 3 + 4 的工作）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `using-git-worktrees`: 1 个 commit 规模，过度设计

  **Parallelization**:
  - **Can Run In Parallel**: NO（必须等 Task 1 完成后才能改 adr.md 后续行）
  - **Parallel Group**: Wave 1 Sequential (2nd)
  - **Blocks**: 3, 4, 5
  - **Blocked By**: 1

  **References**:

  **Pattern References**:
  - `git log --diff-filter=D --name-only | grep phase-1-foundation` - 删除历史模式参考
  - `git rm --help` 文档（`--cached` 选项不要用，本任务需从工作区删除）

  **Acceptance Criteria**:

  ```
  # Acceptance 1: 文件从工作区消失
  test ! -f /workspace/project/ChipForge/docs/roadmap/phases/phase-1-foundation.md && echo "OK: file removed"
  # 期望: OK: file removed

  # Acceptance 2: git index 标记为删除
  git -C /workspace/project/ChipForge status --short docs/roadmap/phases/phase-1-foundation.md
  # 期望: "D  docs/roadmap/phases/phase-1-foundation.md"（注意 D 前是两个空格）

  # Acceptance 3: git 历史保留（可回溯）
  git -C /workspace/project/ChipForge log --all --oneline -- docs/roadmap/phases/phase-1-foundation.md | head -3
  # 期望: 至少 1 行 commit 记录（旧 commit 中存在该文件）
  ```

  **Evidence to Capture**:
  - `.omo/evidence/task-2-git-rm.log`（3 个命令的 stdout）

  **Commit**: NO

- [x] 3. 修正 4 处 ip/*/README.md 活链接

  **What to do**:
  - 修改以下 4 个文件，将 `phase-1-foundation.md` 替换为 `phase-1-tlm-foundation.md`：
    - `ip/cache/README.md` L3
    - `ip/memory/README.md` L3
    - `ip/interconnect/README.md` L3
    - `ip/peripheral/README.md` L3
  - 4 处均为活链接（`[Phase 1 roadmap](../../docs/roadmap/phases/phase-1-foundation.md)`），edit 替换路径尾段

  **Must NOT do**:
  - 不修改 README 中其他文本（"Status: Planning" 等保留）
  - 不触碰 `ip/cpu/README.md`（grep 已确认无该链接）
  - 不触碰 `plugin-framework-revision-plan.md` L250（diff 代码块内为历史示例，保留）
  - 不为这 4 个 README 创建 commit（统一在 Task 5）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `cpp-architecture`: 不适用

  **Parallelization**:
  - **Can Run In Parallel**: NO（4 个 edit 顺序执行可避免 git 状态污染）
  - **Parallel Group**: Wave 1 Sequential (3rd)
  - **Blocks**: 4, 5
  - **Blocked By**: 2

  **References**:

  **Pattern References**:
  - `ip/cache/README.md:3` - 第一个修改样本（其余 3 个结构相同）
  - 现有 4 处链接均使用 `../../docs/roadmap/phases/phase-1-foundation.md` 格式

  **Acceptance Criteria**:

  ```
  # Acceptance 1: 4 个 README 中无残留旧链接
  grep -rn "phase-1-foundation" /workspace/project/ChipForge/ip/ --include="*.md"
  # 期望: 空输出（exit code 1）

  # Acceptance 2: 4 个 README 全部含新链接
  grep -rln "phase-1-tlm-foundation" /workspace/project/ChipForge/ip/ --include="*.md" | sort
  # 期望: 4 行（cache / interconnect / memory / peripheral）
  ```

  **Evidence to Capture**:
  - `.omo/evidence/task-3-link-fix.log`（2 个 grep 命令的 stdout）

  **Commit**: NO

- [x] 4. 更新 roadmap-status.md (PA-3 状态推进 + 活动日志)

  **What to do**:
  - 打开 `docs/roadmap/roadmap-status.md`
  - §3 PA-3 行（L96）：备注列从 `✅ 已完成 (2026-06-08, 添加废弃标记)` 改为 `✅ 已完成 (2026-06-09, 文件已 git rm)`
  - §6 活动日志（L160 附近）：在 2026-06-09 现有行**之前**插入新行，描述本次 Quick plan 交付（ADR-033 锁定 + 废弃文档删除 + 4 处链接修正 + 1 个原子 commit）
  - §1 状态总览：Phase 1* 行（L16 "M1 (legacy, 已取代)"）备注从 "处置 phase-1-foundation.md" 改为 "已删除 (2026-06-09)"（同步消除 PA-3 引用）
  - §5 建议 1（L117）整段删除（"处置 phase-1-foundation.md" 已执行完毕）
  - §2 Phase 0 详情块中"退出标准"等行：保持不变

  **Must NOT do**:
  - 不修改 §1 中 Phase 0/1-6 其他行
  - 不重写 §6 活动日志格式（保持 `<date> | <event>` 表格行）
  - 不为这 1 个文件创建 commit（统一 Task 5）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `cpp-architecture`: 不适用

  **Parallelization**:
  - **Can Run In Parallel**: NO（必须等 Task 3 完成以避免 git 状态污染）
  - **Parallel Group**: Wave 1 Sequential (4th)
  - **Blocks**: 5
  - **Blocked By**: 1, 2, 3

  **References**:

  **Pattern References**:
  - `docs/roadmap/roadmap-status.md:96` - PA-3 行当前内容
  - `docs/roadmap/roadmap-status.md:160` - 活动日志 2026-06-09 现有行（作为新行插入位置参考）
  - `docs/roadmap/roadmap-status.md:117` - 建议 1 段（待删除）

  **Acceptance Criteria**:

  ```
  # Acceptance 1: PA-3 行状态推进
  grep -E "^\| PA-3 \|" /workspace/project/ChipForge/docs/roadmap/roadmap-status.md | grep -c "2026-06-09"
  # 期望: 1（备注列含 2026-06-09）

  # Acceptance 2: 活动日志含本次会话行
  grep -c "ADR-033" /workspace/project/ChipForge/docs/roadmap/roadmap-status.md
  # 期望: ≥ 1（活动日志新行引用 ADR-033）

  # Acceptance 3: 建议 1 段已删除
  awk '/^### 建议 1:处置 `phase-1-foundation.md`/,/^### 建议 2:/' /workspace/project/ChipForge/docs/roadmap/roadmap-status.md | wc -l
  # 期望: 0 或 1（仅锚点行，无内容行；文件实际 header 含 backticks）

  # Acceptance 4: Phase 1* 行同步
  grep -E "^\| Phase 1\* \|" /workspace/project/ChipForge/docs/roadmap/roadmap-status.md | grep -c "已删除"
  # 期望: 1
  ```

  **Evidence to Capture**:
  - `.omo/evidence/task-4-roadmap-update.log`（4 个命令的 stdout）

  **Commit**: NO

- [x] 5. 单个原子 commit + 负向测试验证

  **What to do**:
  - `git add docs/architecture/adr.md docs/roadmap/phases/phase-1-foundation.md ip/cache/README.md ip/memory/README.md ip/interconnect/README.md ip/peripheral/README.md docs/roadmap/roadmap-status.md`
  - `git commit -m "docs(adr): lock ADR-033 + remove deprecated phase-1-foundation

  - ADR-033 (CtrlLink 4-control-API): 🚧 → ✅ Accepted, D6 共存方案绑定 (halt_when / throw_when / flush_when / bypass)
  - 删除 docs/roadmap/phases/phase-1-foundation.md (git rm 保留历史)
  - 修正 4 处 ip/{cache,memory,interconnect,peripheral}/README.md 链接指向 phase-1-tlm-foundation.md
  - 更新 roadmap-status.md: PA-3 状态推进 + 活动日志追加 + Phase 1* 行同步

  Phase 1 (L1CachePlugin) Large plan 启动前的前置解锁。

  Refs: .omo/drafts/decision-plugin-framework-2026-06-08.md §3.5 + §4.2 D6
  Refs: docs/architecture/adr.md ADR-033 详细记录段
  Refs: .omo/plans/adr-033-naming-lock.md"`
  - 提交后执行负向测试：`git diff --stat HEAD~1 HEAD -- 'bundles/*' 'soc/*' 'ip/*/tlm/*' 'src/cf_plugin/*'` 期望为空
  - 执行整体验收：`bash .omo/plans/adr-033-naming-lock.md` 内 8 项 grep 断言（分散记录在 Task 1-4 证据中）

  **Must NOT do**:
  - 不使用 `git add .` 或 `git add -A`（必须显式列文件）
  - 不创建分支（直接在当前分支提交）
  - 不 push
  - 不 amend
  - 不修改 commit message 模板（除 msg + body 外不附加 trailer 除非引用了 Co-Authored-By 等）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `using-git-worktrees`: 单 commit 不需 worktree

  **Parallelization**:
  - **Can Run In Parallel**: NO（最终步骤）
  - **Parallel Group**: Wave 1 Sequential (5th, final)
  - **Blocks**: 无
  - **Blocked By**: 1, 2, 3, 4

  **References**:

  **Pattern References**:
  - `git log -1 --format=%B` - 当前最新 commit message 格式参考（应保持 conventional commit 风格）

  **Acceptance Criteria**:

  ```
  # Acceptance 1: 单 commit 已创建
  git -C /workspace/project/ChipForge log -1 --oneline
  # 期望: 1 行新 commit，message 以 "docs(adr): lock ADR-033" 开头

  # Acceptance 2: commit 改动文件数符合预期
  git -C /workspace/project/ChipForge diff --stat HEAD~1 HEAD | tail -1
  # 期望: "6 files changed"（adr.md 改 + phase-1-foundation.md 删除 + 4 README 改 + roadmap-status.md 改）

  # Acceptance 3: 负向测试 - Phase 1 路径未被触碰
  git -C /workspace/project/ChipForge diff --stat HEAD~1 HEAD -- 'bundles/*' 'soc/*' 'ip/*/tlm/*' 'src/cf_plugin/*'
  # 期望: 空输出

  # Acceptance 4: 整体 grep 验收套件（汇总 Task 1-4 标准）
  bash -c '
  set -e
  grep -E "^\| ADR-033 \|" /workspace/project/ChipForge/docs/architecture/adr.md | grep -q "Accepted" || { echo "FAIL: ADR-033 status"; exit 1; }
  test ! -f /workspace/project/ChipForge/docs/roadmap/phases/phase-1-foundation.md || { echo "FAIL: phase-1-foundation still exists"; exit 1; }
  ! grep -rn "phase-1-foundation" /workspace/project/ChipForge/ip/ --include="*.md" || { echo "FAIL: ip/* still has old link"; exit 1; }
  grep -E "^\| PA-3 \|" /workspace/project/ChipForge/docs/roadmap/roadmap-status.md | grep -q "2026-06-09" || { echo "FAIL: PA-3 not updated"; exit 1; }
  echo "ALL PASS"
  '
  # 期望: "ALL PASS"
  ```

  **Evidence to Capture**:
  - `.omo/evidence/task-5-commit-and-verify.log`（4 个命令的 stdout + 最终 "ALL PASS"）

  **Commit**: YES
  - Message: `docs(adr): lock ADR-033 + remove deprecated phase-1-foundation`
  - Files: `docs/architecture/adr.md`, `docs/roadmap/phases/phase-1-foundation.md` (D), `ip/{cache,memory,interconnect,peripheral}/README.md`, `docs/roadmap/roadmap-status.md`
  - Pre-commit: 无（无 C++ 编译/测试）

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. Verify:
  - `docs/architecture/adr.md` ADR-033 行确实为 ✅ Accepted（grep 主表 + ADR-033 详细记录段两处）
  - `docs/roadmap/phases/phase-1-foundation.md` 已被 git rm（`git log --diff-filter=D --name-only | grep phase-1-foundation` 有记录）
  - 4 个 ip/README.md 不再含 `phase-1-foundation` 链接
  - 1 个 commit 含全部 6 个文件改动
  - 5 处证据文件存在（`.omo/evidence/task-{1,2,3,4,5}-*.log`）
  Output: `ADR-033 [Accepted/Locked] | Doc Removal [Done/Not-Done] | Links [Fixed/Broken] | Commit [Single/Multiple] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  N/A for this plan（无 C++ 代码改动）。改为验证：
  - 6 个文件改动的 markdown 格式正确（无 broken table、行号连续、引用链接 valid）
  - `docs/architecture/adr.md` 中 ADR-033 主表行 + ADR-033 详细记录段两处状态字段同步一致
  - `roadmap-status.md` §1 + §3 + §6 三处对 PA-3 / Phase 1* / 活动日志的引用一致
  Output: `Markdown [Valid/Broken] | Sync [Consistent/Inconsistent] | VERDICT: APPROVE/REJECT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  重跑 Task 1-5 全部 acceptance commands（共 15+ 个 grep/test/git 命令），记录所有 stdout 到 `.omo/evidence/final-qa.log`。验证：
  - 15+ 命令全部 exit code 0（或预期的非零）
  - Task 1-4 evidence 文件 5 个齐全
  Output: `Commands [N/N pass] | Evidence [N/N present] | VERDICT: APPROVE/REJECT`

- [x] F4. **Scope Fidelity Check** — `deep`
  验证 `git diff HEAD~1 HEAD`：
  - 包含全部 6 个预期文件
  - 不包含 bundles/ / soc/ / ip/*/tlm/ / src/cf_plugin/ 任何文件
  - 不包含 `.omo/drafts/` 内容修改（保留历史）
  - 不包含 `.opencode/`、`tools/`、`soc/riscv_virt.json` 等 Phase 1 准备路径
  Output: `In-Scope [N/N] | Out-of-Scope [CLEAN/N issues] | VERDICT: APPROVE/REJECT`

---

## Commit Strategy

- **1** (Task 5): `docs(adr): lock ADR-033 + remove deprecated phase-1-foundation`
  - Files: `docs/architecture/adr.md`, `docs/roadmap/phases/phase-1-foundation.md` (D), `ip/{cache,memory,interconnect,peripheral}/README.md`, `docs/roadmap/roadmap-status.md`
  - Pre-commit: N/A (docs only)

---

## Success Criteria

### Verification Commands

```bash
# 1. ADR-033 status lock
grep -E "^\| ADR-033 \|" /workspace/project/ChipForge/docs/architecture/adr.md
# 期望: 含 "✅ Accepted"

# 2. Phase 0 doc removed
test ! -f /workspace/project/ChipForge/docs/roadmap/phases/phase-1-foundation.md && echo OK
# 期望: "OK"

# 3. Inbound links fixed
! grep -rn "phase-1-foundation" /workspace/project/ChipForge/ip/ /workspace/project/ChipForge/docs/roadmap/ --include="*.md"
# 期望: 空输出（exit 1）

# 4. PA-3 state updated
grep -E "^\| PA-3 \|" /workspace/project/ChipForge/docs/roadmap/roadmap-status.md | grep "2026-06-09"
# 期望: 1 行

# 5. Single commit
git -C /workspace/project/ChipForge log -1 --oneline | head -1
# 期望: 以 "docs(adr): lock ADR-033" 开头

# 6. Negative test
git -C /workspace/project/ChipForge diff --stat HEAD~1 HEAD -- 'bundles/*' 'soc/*' 'ip/*/tlm/*' 'src/cf_plugin/*'
# 期望: 空输出
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent (Phase 1 路径未被触碰)
- [ ] ADR-033 状态 🚧 → ✅ Accepted
- [ ] phase-1-foundation.md 已 git rm
- [ ] 5 处活链接已修正
- [ ] 6 个文件单 commit 提交
