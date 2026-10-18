---
id: scope-matrix-proposal
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
manual_deps: []
review_by: 2026-12-31
---

# scope-matrix-proposal — Work Scope Matrix for Build Agent

> 在 AGENTS.md 引入 **Build Agent Work Scope Matrix**，与 `~/.config/opencode/AGENTS.md` 的 **Build Agent Role Matrix** 平行（**正交独立**）。
>
> 灵感来源：用户观察到本次 2026-10-18 session 实际经历了 4 个 scope 切换（governance → ip/cache → tests/soc → tests/cpu）但无显式状态信号，每次切换无法靠 prompt context 自我校准。
>
> **本文件是 rdd-planner 起的 5 段草稿**，下一步由 rdd-builder P0 落地 openspec `proposal.md` + `tasks.md` + spec.md。

## 1. Why

### 项目上下文（governance wave 治理窗口，main HEAD 干净）

`git log --oneline -5` 显示 main HEAD 当前状态：3 个 governance change（cache-tlm-to-plugins-rename + tests-soc-topology-restructure + tests-cpu-configs-to-schema-rename）已 DONE (archived)，12 commits ahead of origin/main。
本 session 主要时间花在**对项目目录归属的梳理**（`/tmp/opencode/code-test-ownership-report.md` §F.1-F.5 + `/tmp/opencode/cpu-docs-ownership-report.md`），最终把所有 governance 5 项命名漂移全部修了 4 处（剩 §F.3 placeholder IPs + §F.4 tests/bundles/）。

### 用户需求（直接引用用户原话）

> "借鉴 `~/.config/opencode/AGENTS.md` 的 Build Agent 角色矩阵的思路，我可以切换当前会话工作的内容（是某个 ip， 还是 soc, 还是框架），并可以在对话里可以让用户知道处于什么工作内容？"

需求本质：
1. **类型层**：区分 framework / ip / soc / governance / tools 5 类
2. **状态层**：当前会话焦点在哪
3. **UI 层**：在对话里显式声明当前 scope

### 与 opencode Role Matrix 的关系

| 维度 | opencode Role Matrix | 本 Work Scope Matrix |
|------|---------------------|---------------------|
| 目的 | agent 行为约束（who I am） | agent 工作焦点（what I'm working on） |
| 状态位置 | `.rddf/state/role.json` | `.rddf/state/scope.json` (并行但不冲突) |
| 取值 | `arch \| planner \| builder \| verifier \| no-role \| general` | `framework \| ip/<name> \| soc/<name> \| governance \| tools/<name>` |
| 副作用 | 控制 agent 允许/禁止动作（commit/edit/branch） | 控制 agent 建议焦点 + grep 范围 + 反向引用优先级 |
| UI | 开场报告 `【角色: builder】` | 开场报告 `【scope: ip/cpu】` |

**关键差异**：role 是 *enforcement*（角色约束），scope 是 *advisory*（焦点提示）。两者正交独立，**叠加使用** = `role=builder + scope=ip/cpu` 完全有效。

### 本 change 要解决的认知问题

1. **Cognitive drift in long sessions**: agent 经过多轮对话后容易"忘了现在在哪"（cognitive context loss）
2. **Implicit focus drift**: git diff 中跨 scope 操作时 agent 难以自我校准建议优先级
3. **User 无法从对话表面判断 agent 当前焦点**: 每次切换无显式信号

### 非问题（非本 change 范围）

- **不改 `role.json` schema**（保持兼容）
- **不动 build agent 实现的 SKILL.md**（本 change 仅落 AGENTS.md + state schema）
- **不强制切换**（scope 是 advisory，不阻断操作）
- **不改其他 governance 文档**（`/tmp/opencode/code-test-ownership-report.md` 仍作为背景资料）

## 2. What Changes

### 变更范围

1. **AGENTS.md 新增 section**（在 `## 构建与测试` 之前 or `## 代码组织` 之后插入）：Work Scope Matrix 定义 + scope 取值列表 + Mermaid 图
2. **`.rddf/state/scope.json` 初始状态**：写入 scope-matrix-proposal change 自身在 `governance` scope 下
3. **Build Agent 开场报告格式**：扩展现有 `【角色: <X>】` 为 `【角色: <X> | scope: <Y>】`

### In Scope

- **AGENTS.md section**（约 80-100 行）：
  - Work Scope Matrix 介绍段落（生效范围 + 与 role 关系 + advisory 而非 enforcement 声明）
  - 状态来源 schema（参照 `role.json`）
  - 取值列表（5 categories + targets 矩阵表）
  - 切换触发 + 写入规则
  - 开场报告格式 example
  - 默认行为
  - **可选**：Mermaid 关系图示

- **`.rddf/state/scope.json` 初始 schema v1**：
  ```json
  {
    "schema_version": "scope-v1",
    "current_scope": {
      "category": "governance",
      "target": "governance",
      "sub_target": null
    },
    "updated_at": "2026-10-19T...",
    "updated_by": "user",
    "session_id": "...",
    "history": [{"from": null, "to": "governance/governance", "at": "...", "by": "user", "trigger": "scope-matrix-proposal change initial"}]
  }
  ```

### Out of Scope

- **不改 SKILL.md / build agent 实现**（仅落 AGENTS.md + state schema 文档）
- **不改 rdd-workflow behavior**（scope 不触发 rdd-planner / rdd-builder 自动化）
- **不动 `role.json`**（保持兼容）
- **不动 `~/.config/opencode/AGENTS.md`**（用户全局灵感来源，**不动**）
- **不强制 scope 切换**（advisory）
- **不创 rdd-quick path 入口**（与 cache-tlm/tests-soc/tests-cpu 治理类比，但本 change 是 docs-only，scope=rdd-quick 或 scope=rdd-planner 都不必，需评估）

## 3. Acceptance

### 必达（Hard）

- [ ] `AGENTS.md` 新增 Work Scope Matrix section 已写入（含 5 categories + 取值列表 + 切换触发 + 开场报告格式）
- [ ] `.rddf/state/scope.json` 初始 state 已写入（schema v1, current_scope=governance/governance）
- [ ] `openspec validate scope-matrix-proposal` 0 fail
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] `bash tools/verify_adr.sh` 0 fail
- [ ] `bash tools/doc_link_check.sh` 0 fail
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression
- [ ] openspec archive `scope-matrix-proposal` 成功
- [ ] `.rddf/roadmap/strategy.md §7` 经 sync 后含 `scope-matrix-proposal: DONE (archived)` 行

### 应达（Soft）

- [ ] AGENTS.md 新 section 含 Mermaid 图（visualization 加成）
- [ ] AGENTS.md 新 section 含「与 role matrix 关系」对比表
- [ ] AGENTS.md 新 section 明确：「scope 是 advisory, 不阻断操作」声明

### 治理（Meta）

- [ ] `openspec/specs/scope-matrix-proposal/spec.md` 7-9 Requirement 落地
- [ ] openspec/specs `sections` 收到 `governance` initiative 标记

## 4. Capabilities

新增能力：

### MUST

- 在 build agent 每个回合响应前，prompt 包含 `【角色: <X> | scope: <Y> | source: <state-file|inferred>】`
- 当 scope 变化时，agent 主动 declare "已切换 scope: <X>" 显式说明
- 当 git diff 显示跨 scope 操作时，agent 主动 warn "检测到跨 scope 操作，继续?"
- 当用户问"我们在改什么"时，agent 能 read scope.json + explain 当前位置

### MUST NOT

- scope 不阻断 agent 任何操作（advisory only）
- scope 与 role 不能互相覆盖（role=builder 时 scope 仍可改）
- scope state file 不污染 rdd-workflow state（与 role.json + builder handoff + verifier report 并行独立）

详见 code-test-ownership-report.md §D 归属划分矩阵（以及未来 scope matrix 自身的 reference）。

## 5. Impact

### 受影响文件

| 文件 | 动作 | 字节数预估 |
|------|------|-----------|
| `AGENTS.md` | 新增 section | +200 行 |
| `.rddf/state/scope.json` | 新建 | ~200 字节 |
| `.rddf/improvements/scope-matrix-proposal.md` | 已存在 (本文件) | 0 |
| `openspec/changes/scope-matrix-proposal/{proposal,tasks}.md` | 新建 | ~3000 字 |
| `openspec/changes/scope-matrix-proposal/specs/scope-matrix-proposal/spec.md` | 新建 | ~1500 字 |
| `openspec/specs/scope-matrix-proposal/spec.md` (D3) | openspec archive 自动衍生 | ~1500 字 |

### 风险

- **R-COGNITIVE-1**（scope 引入反而增加 context 复杂度）→ 缓解：scope 是 advisory，开场报告 1 行即止，用户可 reset 回 `general`。
- **R-CONFLICT-1**（scope 与 role 字段冲突）→ 缓解：独立 state file (`.rddf/state/scope.json` vs `.rddf/state/role.json`)，schema 不重叠。
- **R-DRIFT-1**（长 session 中 agent 漏更新 scope.json）→ 缓解：scope state 假定 cache-friendly（任何 prompt 都可重新 read；advisory 不强制）。
- **R-AGENT-1**（其他 agent (explore/oracle/metis/critic/librarian) 误用 scope matrix）→ 缓解：明确标注 "生效范围：仅 build agent"（同 role matrix §0 模式）。
- **回退**：git revert 单 commit 即可恢复（AGENTS.md 还原 + .rddf/state/scope.json 不影响编译）。

### 下游依赖

- 上游: 无
- 下游: 未来 build agent SKILL.md 更新（如果决定把 scope state read 自动化）— **Out of Scope** by design

### 不解决

- 自动 detect scope 切换机制（git diff path → scope suggestion）— Out of Scope by design
- 跨 build agent 同步 scope state（不同 session 之间）— Out of Scope by design
- `~/.config/opencode/AGENTS.md` Role Matrix 更新（与本 scope matrix 平行但全局）— Out of Scope（用户全局配置，由用户决策）
