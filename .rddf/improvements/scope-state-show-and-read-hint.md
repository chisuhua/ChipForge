---
id: scope-state-show-and-read-hint
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
manual_deps: [scope-matrix-proposal]
review_by: 2026-12-31
---

# scope-state-show-and-read-hint

> 在仓库 AGENTS.md §Build Agent Work Scope Matrix 顶部加 §0.5 (per-turn read+output hint) + §10 (slash command `/scope-show`),让 LLM 不靠自觉，自动化读 role.json + scope.json + 输出开场报告。
>
> **本文件是 rdd-planner 起的 5 段草稿**，下一步由 rdd-builder P0 落地 openspec `proposal.md` + `tasks.md` + spec.md。

## 1. Why

### 项目上下文

scope-matrix-proposal change (2026-10-19) 落了 §Build Agent Work Scope Matrix (156 行 AGENTS.md 新 section) + `.rddf/state/scope.json`, 但**没有任何"自动触发"机制**——纯靠 LLM 自觉在每回合开始读 scope.json + 输出开场报告。

用户直接观察到的现象:
- 本会话"切换到 ip/cpu"是用户主动发 trigger 命令 + LLM 收到后写 scope.json + 输出开场报告
- **没有"每回合响应前自动注入"的 prompt 机制**
- 跨 session / 跨 scope 工作时,容易 cognitive drift

### 用户反馈 (2026-10-19 直接引用)

> "可以路径1, 2都做吗?"
>
> 路径1: 在 system prompt 注入 scope read hint
> 路径2: 增加 `/scope-show` 命令

### 本 change 要解决的架构债

让 scope matrix 从"docs 规范"升级到"build agent runtime 行为约束":
1. **路径 1** - 在 AGENTS.md §Build Agent Work Scope Matrix 顶部加 §0.5 硬性 read+output 规范,让 LLM 知道"每回合必做",不再靠自觉
2. **路径 2** - 在 AGENTS.md §Build Agent Work Scope Matrix 增 §10 (slash command `/scope-show`) + §11 (`/scope-history`) + §12 (`/scope-reset`) 三个 user-facing command 文档

### 非问题（非本 change 范围）

- **不动 `~/.config/opencode/AGENTS.md`**(用户全局文件,Out of Scope by design)
- **不动 build agent SKILL.md 实现**(仅落文档)
- **不动 `.rddf/state/scope.json` schema**(保持 v1)
- **不动 `.rddf/state/role.json`**(保持兼容)
- **不动 openspec/specs/scope-matrix-proposal/spec.md** (本 change 是 scope-matrix-proposal 的 minor 增强,不开新 spec)
- **不动 `tools/` 任何脚本**(slash command 文档化足够,不动 tool impl)

## 2. What Changes

### In Scope

#### 路径 1 - Per-turn read+output hint (§0.5 段落)

在仓库 AGENTS.md §Build Agent Work Scope Matrix 顶部 (紧接 H2 title 后) 加 §0.5 "Per-turn read+output hard constraint" 段, 包含:

- **Read 必做清单** (3 步):
  1. 读 `<cwd>/.rddf/state/role.json` (项目级) → fallback `~/.config/opencode/AGENTS.md` §1 → fallback `general`
  2. 读 `<cwd>/.rddf/state/scope.json` (项目级) → fallback `governance/governance` (本 session 入口默认)
  3. 读最近 1 个 git log 行 (隐式 trigger: 自动 suggest scope)
- **Output 必做清单** (2 步):
  1. 在回合响应**最顶部**输出 `【角色: <X> | scope: <Y>】` 格式 (一行机读 + 一句人话)
  2. 如果 scope 与上回合不同,显式声明 "已切换 scope: <Y>"

#### 路径 2 - Slash command 文档 (§10 + §11 + §12)

| § | 命令 | 用途 | 输出 |
|---|------|------|------|
| §10 | `/scope-show` | 显示当前 scope + 完整 state.json | `cat .rddf/state/scope.json` 全文 + 1 行 human summary |
| §11 | `/scope-history` | 显示最近 N 次 scope 切换 | `history[]` 倒序打印,附 trigger reason |
| §12 | `/scope-reset` | 重置回 `governance/governance` (清空 history) | 写新 state, history 增 1 项 `{from: <old>, to: governance/governance, by: user, trigger: "user reset"}` |

> **Note**: 这 3 个 slash command 在 opencode 内**不是系统级真 slash command** (opencode runtime 不解析 `/scope-show`),而是**用户→LLM 触发的口语口令** (类似 `切换到 X`). 文档把它们叫"slash command" 是为了对称 opencode 的 `/role-show` 用法 (实际也是口令).

### Out of Scope

- **不动 `~/.config/opencode/AGENTS.md`** (用户全局, parallel 独立)
- **不动 build agent SKILL.md 实现** (opencode runtime 不能/不应在 build agent 上 patch)
- **不动 `role.json` schema** (保持兼容)
- **不动 `scope.json` schema** (保持 v1)
- **不动 4 architecture gate** (纯 docs change, 0 业务代码)
- **不动 openspec/specs/scope-matrix-proposal/spec.md** (在原 change 上 minor 增强,不开新 spec)
- **不动 tools/ 任何脚本** (slash command 文档化, 靠 LLM 解析)

## 3. Acceptance

### 必达 (Hard)

- [ ] AGENTS.md §Build Agent Work Scope Matrix 顶部新增 §0.5 (per-turn read+output 规范)
- [ ] AGENTS.md §Build Agent Work Scope Matrix 新增 §10 `/scope-show` (含示例输出)
- [ ] AGENTS.md §Build Agent Work Scope Matrix 新增 §11 `/scope-history`
- [ ] AGENTS.md §Build Agent Work Scope Matrix 新增 §12 `/scope-reset`
- [ ] 4 architecture gate unchanged (12/12 + 8/8 + 0 fail + 0 doc_link)
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression (458/459 baseline unchanged)
- [ ] openspec archive `scope-state-show-and-read-hint` 成功
- [ ] `.rddf/roadmap/strategy.md §7` 经 sync 后含 `scope-state-show-and-read-hint: DONE (archived)` 行

### 应达 (Soft)

- [ ] §0.5 段含"凭 LLM 自觉"vs"硬规范"对照表, 让 reviewer 一眼看懂升级前/后差异
- [ ] §10-12 每个 slash command 含 trigger 表达式 + LLM 期望动作 + 期望输出示例 3 件套
- [ ] §10-12 注明"opencode runtime 不解析真 slash, 这些是用户→LLM 触发的口语口令"

### 治理 (Meta)

- [ ] `openspec/specs/scope-state-show-and-read-hint/spec.md` 4-6 Requirement 落地
- [ ] proposal frontmatter 含 `initiative: wave5-isa-coverage-and-bp` + `manual_deps: [scope-matrix-proposal]`

## 4. Capabilities

### MUST

- 仓库 AGENTS.md §Build Agent Work Scope Matrix 顶部 §0.5 hard constraint
- 仓库 AGENTS.md §Build Agent Work Scope Matrix 增 §10-12 slash command docs

### MUST NOT

- 不动 `~/.config/opencode/AGENTS.md` (用户全局)
- 不动 build agent SKILL.md
- 不动 `.rddf/state/{role,scope}.json` schema
- 不动 openspec/specs/scope-matrix-proposal/spec.md

详见 `.rddf/improvements/scope-matrix-proposal.md §4` (parent change 同样的 capability 集合, 本 change 是 minor 增强).

## 5. Impact

### 受影响文件

| 文件 | 动作 | 大小预估 |
|------|------|---------|
| `AGENTS.md` | §Build Agent Work Scope Matrix 顶部加 §0.5 (10-15 行) + 末尾加 §10-12 (40-60 行) | +60-80 行 |
| `.rddf/improvements/scope-state-show-and-read-hint.md` | 本文件 | 0 (已写) |
| `openspec/changes/scope-state-show-and-read-hint/{proposal,tasks}.md` | 新建 | ~3000 字 |
| `openspec/changes/scope-state-show-and-read-hint/specs/.../spec.md` | 新建 | ~1500 字 |
| `openspec/specs/scope-state-show-and-read-hint/spec.md` (D3) | openspec archive 自动衍生 | ~1500 字 |

**总计: 0 业务代码改动, docs-only change (~60-80 行 AGENTS.md 增量)**.

### 风险

- **R-COGNITIVE-1** (用户担心 LLM 靠自觉不可靠) → 缓解: §0.5 hard constraint 明确写"build agent 每回合必做", 接近 opencode role matrix §3 的"用户触发, build agent 必须遵守" 风格
- **R-SLASH-1** (slash command 不是 opencode 真 slash) → 缓解: §10-12 明确注明"是用户→LLM 触发的口语口令", 避免歧义
- **R-CONFLICT-1** (与 role matrix 的 `/role-show` 命名冲突) → 缓解: 命名采用 `/scope-show` (与 opencode `/role-show` 对称但不冲突), 实际触发是 "scope?" / "scope status" 之类口语
- **R-SCHEMA-1** (`scope.json` schema 改动) → 缓解: 严格保持 v1, 仅修改 `current_scope/history` 内容
- **R-AGENT-1** (其他 agent 误用) → 缓解: §0.5 明确 "生效范围: 仅 build agent" (同 role matrix §0 模式)
- **回退**: git revert 单 commit 即可 (AGENTS.md 还原)

### 下游依赖

- **上游**: scope-matrix-proposal (本 change 是 minor 增强, parent change 必先 archive)
- **下游**: 未来 build agent runtime patch (opencode 框架可能升级支持真 slash command) — Out of Scope by design

### 不解决

- 跨 session scope persistence (fresh clone 永远 `governance/governance` fallback) — Out of Scope by design
- 自动 detect scope 切换机制 (git diff path → scope suggestion) — Out of Scope (scope-matrix §3 已定义为"可选", 留 future work)
- `~/.config/opencode/AGENTS.md` Role Matrix 同步增强 — Out of Scope (用户全局, 由用户决策)
