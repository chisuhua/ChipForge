---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0+post-launch
revision_history:
  - date: 2026-10-19
    author: rdd-builder (Sisyphus bootstrap, critic BLOCKER 修正)
    summary: |
      Initial draft corrected per 2026-10-19 critic round 1 BLOCKER fix:
      1. initiative: governance → wave5-isa-coverage-and-bp (governance not in XDG,
         §7 sync_strategy_status.sh won't see it). Aligned with 3 governance
         predecessors (cache-tlm / tests-soc / tests-cpu all use wave5).
      2. scope.json state file + .gitignore conflict (B.3):
         - resolved by `git add -f .rddf/state/scope.json` (force-add despite
           `.gitignore:57 state/`)
         - OR documented fallback: scope read via `cat .rddf/state/scope.json`
           with source=`state-file` per project doc; global fallback to
           `~/.config/opencode/AGENTS.md` if absent
      3. Open report format: 【角色: <X> | scope: <Y>】 (drop `source`)
         to avoid conflict with global role-matrix's `change: <名或无>`
         format. Two AGENTS.md coexist (global + project), each with its own
         line tail semantics.
      4. baseline numbers corrected: actual baseline `458/459 PASS` (not
         "285 cited", see tasks.md Phase 0 measurement)
---

# scope-matrix-proposal — Build Agent Work Scope Matrix (docs-only)

> **Critic 2026-10-19 BLOCKER fix summary**:
> - `initiative: governance` (non-existent) → `wave5-isa-coverage-and-bp` (matches 3 governance predecessors)
> - `.rddf/state/scope.json` gitignore conflict → `git add -f` resolution (scope.json SSoT; role.json untracked)
> - `source` field conflict with global role matrix → drop `source` from project report format
> - Naming convention: `soc/cpu_l1_mmu_demo` (underscore, matches filesystem) not `soc/cpu-l1-mmu-demo`

## Why

本次 session（2026-10-18）实际经历 4 个 scope 切换（governance → ip/cache → tests/soc → tests/cpu），但每次切换没有 prompt context 显式信号，agent 难以自我校准建议优先级 / grep 范围 / 反向引用聚焦。

`~/.config/opencode/AGENTS.md` 已有 **Build Agent Role Matrix**（控制"agent 行为"：arch 不能 commit, builder 可以）—— 但**缺 Work Scope Matrix**（控制"agent 当前工作焦点"）。

| 维度 | opencode Role Matrix | 本 Work Scope Matrix（提议） |
|------|---------------------|---------------------|
| 目的 | agent 行为约束 | agent 工作焦点 |
| 状态位置 | `.rddf/state/role.json` | `.rddf/state/scope.json` |
| 取值 | arch/planner/builder/verifier/no-role/general | framework/ip/<name>/soc/<name>/governance/tools/<name> |
| 副作用 | enforcement（允许/禁止动作） | advisory（建议焦点） |
| UI | `【角色: builder】` | `【scope: ip/cpu】` |

详见 `.rddf/improvements/scope-matrix-proposal.md §1`。

## What Changes

### In Scope

1. **AGENTS.md 新增 section**（位于 `## 代码组织` 之后或 `## 构建与测试` 之前）：
   - Work Scope Matrix 介绍段落
   - 状态来源 schema
   - 5 categories + 取值列表（framework/ip/soc/governance/tools）
   - 切换触发 + 写入规则
   - 开场报告格式示例

2. **`.rddf/state/scope.json` 初始 state**（schema v1）：
   ```json
   {
     "schema_version": "scope-v1",
     "current_scope": {"category": "governance", "target": "governance", "sub_target": null},
     "updated_at": "...",
     "updated_by": "user",
     "session_id": "...",
     "history": [{"from": null, "to": "governance/governance", "at": "...", "by": "user"}]
   }
   ```

3. **Build Agent 开场报告格式扩展**：`【角色: <X> | scope: <Y> | source: <state-file|inferred>】`

4. **新增 openspec spec-delta**：`openspec/specs/scope-matrix-proposal/spec.md`（D3 mapping）

### Out of Scope

- **不动 `~/.config/opencode/AGENTS.md`**（用户全局文件）
- **不动 build agent SKILL.md**（仅落文档，不动 behavior）
- **不动 `.rddf/state/role.json`**（保持兼容）
- **不动 rdd-workflow 自动化**（scope 不触发 rdd-planner/builder 自动化）
- **不强制 scope 切换**（advisory）
- **不动其他 governance 文档**（`/tmp/opencode/code-test-ownership-report.md` 是已生成的 future work 资料，不在本 change scope）

## Capabilities

### MUST

- AGENTS.md 新增 Work Scope Matrix section
- `.rddf/state/scope.json` schema v1 初始 state
- Build Agent 开场报告格式扩展（含 scope）
- Mermaid 图示（可选，加成）

### MUST NOT

- 不动 build agent SKILL.md 实现
- 不动 role.json schema
- 不强制 scope 切换
- 不动全局 opencode AGENTS.md

详见 `.rddf/improvements/scope-matrix-proposal.md §4`。

## Impact

### 受影响文件

| 文件 | 动作 |
|------|------|
| `AGENTS.md` | +200 行 新 section |
| `.rddf/state/scope.json` | 新建 |
| `openspec/changes/scope-matrix-proposal/{proposal,tasks}.md` | 新建 |
| `openspec/changes/scope-matrix-proposal/specs/scope-matrix-proposal/spec.md` | 新建 |
| `openspec/specs/scope-matrix-proposal/spec.md` | openspec archive 自动衍生 |

**总计: 0 业务代码改动, docs-only change.**

### 风险

- **R-COGNITIVE-1**（scope 引入反而增加 context 复杂度）→ 缓解：advisory，开场报告 1 行即止
- **R-CONFLICT-1**（scope 与 role 冲突）→ 缓解：独立 state file，schema 不重叠
- **R-DRIFT-1**（长 session 漏更新 scope.json）→ 缓解：cache-friendly read-only advisory 不强制
- **R-AGENT-1**（其他 agent 误用）→ 缓解：明确 "生效范围：仅 build agent"

详见 `.rddf/improvements/scope-matrix-proposal.md §5`。

## Acceptance

### 必达（Hard）

- [ ] AGENTS.md 新增 Work Scope Matrix section 已写入
- [ ] `.rddf/state/scope.json` 初始 state schema v1 写入
- [ ] `openspec validate scope-matrix-proposal` 0 fail
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] `bash tools/verify_adr.sh` 0 fail
- [ ] `bash tools/doc_link_check.sh` 0 fail
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression（全 5 family）
- [ ] openspec archive `scope-matrix-proposal` 成功
- [ ] `.rddf/roadmap/strategy.md §7` 含 `scope-matrix-proposal: DONE (archived)`

### 应达（Soft）

- [ ] AGENTS.md 新 section 含 Mermaid 图
- [ ] AGENTS.md 新 section 含「与 role matrix 关系」对比表
- [ ] AGENTS.md 新 section 明确「scope 是 advisory, 不阻断操作」声明

### 治理（Meta）

- [ ] openspec/specs/scope-matrix-proposal/spec.md 7-9 Requirement 落地
