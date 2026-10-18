# scope-state-show-and-read-hint Specification

## Purpose

Document the contract for scope-state-show-and-read-hint: a minor enhancement to the existing AGENTS.md §Build Agent Work Scope Matrix (added by scope-matrix-proposal) which adds:
- §0.5 per-turn read+output hard constraint (per-turn 必做 read+output, 升级 LLM 自觉到硬规范)
- §10/§11/§12 slash command docs (`/scope-show`, `/scope-history`, `/scope-reset`)

0 business code change. Pure docs enhancement.

## ADDED Requirements

### Requirement: agents-md-section-zero-point-five
The system SHALL add `### 0.5. Per-turn read+output 硬性规范` as the FIRST sub-section under `## Build Agent Work Scope Matrix (POC)` (immediately after H2 title), containing:
- Read checklist (3 必做 read):
  1. `<cwd>/.rddf/state/role.json` (项目级) → fallback `~/.config/opencode/AGENTS.md` §1 → fallback `general`
  2. `<cwd>/.rddf/state/scope.json` (项目级) → fallback `governance/governance`
  3. `git log -1 --oneline` (last commit, optional implicit trigger for auto scope suggest)
- Output checklist (2 必做 output):
  1. 在回合响应最顶部输出 `【角色: <X> | scope: <Y>】` 格式 (一行机读 + 一句人话)
  2. 如果 scope 与上回合不同, 显式声明 "已切换 scope: <Y>" (新切换 mark)
- "Before vs After" 对照表 (让 reviewer 一眼看懂自觉→硬规范差异)

#### Scenario: section 0.5 visible
- **WHEN** `awk '/^## Build Agent Work Scope Matrix/,/^### 1\./' AGENTS.md` runs
- **THEN** output includes `### 0.5. Per-turn read+output 硬性规范` heading + content

#### Scenario: section 0.5 not 破坏 parent change
- **WHEN** `awk '/^## Build Agent Work Scope Matrix/,/^---$/' AGENTS.md` runs
- **THEN** output still contains §0.5 + §1-9 from parent scope-matrix-proposal (all 9 sections preserved, 10 sections total)

### Requirement: slash-command-scope-show
The system SHALL add `### 10. /scope-show` section at the END of §Build Agent Work Scope Matrix, containing:
- Trigger expression (user spoken: "scope?", "scope status", "/scope-show")
- LLM 期望动作 (3 步):
  1. `cat .rddf/state/scope.json`
  2. parse current_scope + history (last 5)
  3. output formatted: `【scope-show】\n  current: <category>/<target>[/<sub_target>]\n  updated: <at>\n  history (last 5):\n    <list>`
- 期望输出示例 (concrete example with governance/governance + 1 history entry)

#### Scenario: /scope-show section visible
- **WHEN** `grep -A1 '^### 10\. /scope-show' AGENTS.md` runs
- **THEN** section heading + 期望输出示例 visible

#### Scenario: /scope-show trigger is user→LLM口语口令, not opencode runtime slash
- **WHEN** reader inspects §10
- **THEN** section contains explicit clarification: "opencode runtime 不解析真 slash command; 这是 user→LLM 触发的口语口令 (类似 opencode /role-show 风格)"

### Requirement: slash-command-scope-history
The system SHALL add `### 11. /scope-history` section, containing:
- Trigger: "scope history", "/scope-history"
- LLM 动作: print `history[]` array in reverse-chronological order, with trigger reason
- 示例输出 (含 `from/to/at/by/trigger` 5 字段 table)

#### Scenario: /scope-history section visible
- **WHEN** `grep -A1 '^### 11\. /scope-history' AGENTS.md` runs
- **THEN** section heading + 示例 visible

### Requirement: slash-command-scope-reset
The system SHALL add `### 12. /scope-reset` section, containing:
- Trigger: "scope reset", "回到 governance", "/scope-reset"
- LLM 动作 (3 步):
  1. 读 current scope, save to `from: <old>`, `to: governance/governance`
  2. 写新 history entry with `by: user, trigger: "user reset"`
  3. output 确认: `【scope-reset】已重置回 governance/governance`
- 示例输出

#### Scenario: /scope-reset section visible
- **WHEN** `grep -A1 '^### 12\. /scope-reset' AGENTS.md` runs
- **THEN** section heading + 示例 visible

### Requirement: no-business-code-change
The system SHALL NOT modify any business code (.cpp/.h in `ip/`, `include/`, `src/cf_plugin/`, `tests/`). Pure docs change.

#### Scenario: 0 business diff
- **WHEN** `git diff --name-only origin/main..HEAD | grep -E '^(ip|include|src|tests|tools)/' | grep -v '\.md$'` runs after archive
- **THEN** empty output (only .md files + .json + openspec/ docs changed)

### Requirement: regression-zero
The system SHALL pass 4 architecture gates + TLM regression unchanged vs scope-matrix-proposal baseline (458/459 PASS).

#### Scenario: gates green
- **WHEN** 4 gate scripts + `bash tools/run_chipforge_tests.sh` run after archive
- **THEN** all gates PASS, 458/459 unchanged (1 pre-existing fail retained)

### Requirement: openspec-archive-and-sync
The system SHALL complete:
- `openspec archive scope-state-show-and-read-hint` succeeds
- `openspec/specs/scope-state-show-and-read-hint/spec.md` D3 spec-delta generated (4-6 Requirement)
- `.rddf/roadmap/strategy.md §7` auto-derived via `bash .rddf/sync_strategy_status.sh`

#### Scenario: archive atom
- **WHEN** openspec archive executes
- **THEN** change moved to openspec/changes/archive/ + spec auto-updated + strategy.md reflects DONE
