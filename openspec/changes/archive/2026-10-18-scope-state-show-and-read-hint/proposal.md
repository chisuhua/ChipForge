---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0+post-launch
manual_deps: [scope-matrix-proposal]
revision_history:
  - date: 2026-10-19
    author: rdd-builder (Sisyphus bootstrap, scope-matrix-proposal 增强)
    summary: |
      Initial draft. scope-matrix-proposal 的 minor 增强 (路径 1+2):
      1. AGENTS.md §Build Agent Work Scope Matrix 顶部加 §0.5 per-turn hard constraint
         (read role.json + scope.json + 输出开场报告, 强制非自觉)
      2. AGENTS.md §Build Agent Work Scope Matrix 加 §10 /scope-show, §11 /scope-history,
         §12 /scope-reset slash command 文档 (口语口令触发, 不是 opencode runtime slash).
      0 业务代码改动. manual_deps: scope-matrix-proposal (parent 必先 archive).
      Pre-emptive frontmatter + manual_deps 已加.
---

# scope-state-show-and-read-hint — Scope Matrix runtime hint + slash command docs

## Why

scope-matrix-proposal (2026-10-19) 落了 §Build Agent Work Scope Matrix (156 行) + `.rddf/state/scope.json`, 但**没有任何"自动触发"机制**——纯靠 LLM 自觉在每回合开始读 scope.json + 输出开场报告。

用户直接观察 (2026-10-19):
> "可以路径1, 2都做吗?"
> 路径1: 在 system prompt 注入 scope read hint
> 路径2: 增加 `/scope-show` 命令

本 change 是 scope-matrix-proposal 的 minor 增强, 在已落的 AGENTS.md §Build Agent Work Scope Matrix 内**追加 §0.5 + §10-12** (不破坏 parent change 的 156 行内容).

详见 `.rddf/improvements/scope-state-show-and-read-hint.md §1`.

## What Changes

### In Scope

1. **AGENTS.md §0.5 Per-turn read+output hard constraint** (10-15 行, 插在 H2 title 之后)
2. **AGENTS.md §10 /scope-show** (15-20 行, 含示例输出)
3. **AGENTS.md §11 /scope-history** (10-15 行)
4. **AGENTS.md §12 /scope-reset** (10-15 行)

### Out of Scope

- 不动 `~/.config/opencode/AGENTS.md` (用户全局, parallel)
- 不动 build agent SKILL.md
- 不动 `.rddf/state/{role,scope}.json` schema
- 不动 openspec/specs/scope-matrix-proposal/spec.md (parent spec 仍 valid, 本 change 是 minor doc enhancement)
- 不动 tools/ 脚本
- 不动 4 architecture gate 行为

## Capabilities

### MUST

- AGENTS.md §Build Agent Work Scope Matrix 顶部 §0.5 (per-turn hard constraint)
- AGENTS.md §Build Agent Work Scope Matrix 末尾 §10-12 (slash command docs)
- 0 业务代码改动

### MUST NOT

- 不动 `~/.config/opencode/AGENTS.md`
- 不动 build agent SKILL.md
- 不动 `.rddf/state/{role,scope}.json` schema

## Impact

### 受影响文件

| 文件 | 动作 |
|------|------|
| `AGENTS.md` | §0.5 (top) + §10-12 (bottom) 追加, +60-80 行 |
| `openspec/changes/scope-state-show-and-read-hint/{proposal,tasks}.md` | 新建 |
| `openspec/changes/scope-state-show-and-read-hint/specs/.../spec.md` | 新建 |
| `openspec/specs/scope-state-show-and-read-hint/spec.md` (D3) | openspec archive 自动衍生 |

**总计: 0 业务代码改动, +60-80 行 AGENTS.md 增量**.

### 风险

- **R-SLASH-1**: opencode runtime 不解析真 slash, 这些是 user→LLM 口语口令 → 缓解: §10-12 明确注明
- **R-CONFLICT-1**: 与 `/role-show` 命名风格冲突 → 缓解: 严格采用 `/scope-X` 前缀
- **R-AGENT-1**: 其他 agent 误用 → 缓解: §0.5 明确 "生效范围: 仅 build agent"
- **回退**: git revert 单 commit 即可 (AGENTS.md 还原)

详见 `.rddf/improvements/scope-state-show-and-read-hint.md §5`.

## Acceptance

### 必达 (Hard)

- [ ] AGENTS.md §Build Agent Work Scope Matrix 顶部新增 §0.5
- [ ] AGENTS.md 新增 §10 /scope-show
- [ ] AGENTS.md 新增 §11 /scope-history
- [ ] AGENTS.md 新增 §12 /scope-reset
- [ ] 4 architecture gate 12/12 + 8/8 + 0 fail + 0 doc_link
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression
- [ ] openspec archive `scope-state-show-and-read-hint` 成功
- [ ] strategy.md §7 含 `scope-state-show-and-read-hint: DONE (archived)`
- [ ] manual_deps: scope-matrix-proposal 已 DONE archived (per state.json `current_phase=phase-3-archive`)

### 应达 (Soft)

- [ ] §0.5 含"靠 LLM 自觉 vs 硬规范"对照表
- [ ] §10-12 每个含 trigger 表达式 + LLM 动作 + 期望输出 3 件套
- [ ] §10-12 注明"opencode runtime 不解析真 slash"

### 治理 (Meta)

- [ ] openspec/specs/scope-state-show-and-read-hint/spec.md 4-6 Requirement
- [ ] proposal frontmatter 含 `initiative: wave5-isa-coverage-and-bp` + `manual_deps: [scope-matrix-proposal]` (已加)
