---
name: v0100-bootstrap
description: |
  Generate a customized session-bootstrap prompt for ChipForge v0.10.0 wave5-isa-coverage-and-bp work.

  Use this skill at the START of each new session continuing v0.10.0 work, or after a gap ≥1 day.

  Two modes:
  - /v0100-bootstrap (default): Generate paste-ready prompt for new session
  - /v0100-bootstrap review: Deep state analysis + recommendations (no prompt)

  Underlying: bash tools/v0100-bootstrap.sh (project-level, tracked in git).
---

# When to use

Invoke this skill in any of these situations:

1. **Starting a new session** to continue v0.10.0 wave5-isa-coverage-and-bp work
2. **Returning after ≥1 day gap** to ChipForge
3. **After major artifact** completion (ADR/archive/change creation)
4. **Before any PR** touching `soc/cpu/docs/roadmap/` or `openspec/changes/`

# Usage

- `/v0100-bootstrap` — default mode: generate paste-ready prompt
- `/v0100-bootstrap generate` — alias for default
- `/v0100-bootstrap review` — analyze state + recommendations (no prompt)
- `/v0100-bootstrap help` — show this help

# Mode: Generate (default)

## Step 1: Invoke bash script to get live state

```bash
cd /workspace/project/ChipForge && bash tools/v0100-bootstrap.sh
```

The script outputs structured markdown with these sections:

| Section | Source |
|---------|--------|
| `## generation_time` | `date '+%Y-%m-%d %H:%M:%S'` + HEAD commit |
| `## test_status` | `[cpu-l1-mmu-demo]` + `[riscv-tests]` pass/fail counts |
| `## active_changes` | `openspec list` |
| `## initiative_status` | `tools/sync_strategy_status.sh --dry-run` |
| `## recent_commits` | `git log --oneline -5` |
| `## hard_prerequisites` | v0.10.0 launch gates table |
| `## preflight_reminders` | cross-file change impact |

## Step 2: Generate session-bootstrap prompt

Use this template (replace `{{PLACEHOLDERS}}` with values from bash script output):

````markdown
[延续 v0.10.0 wave5-isa-coverage-and-bp 会话 — bootstrap {{GENERATION_TIME}}]

项目根: `/workspace/project/ChipForge`
HEAD: `{{HEAD_COMMIT}}` — ⚠️ DO NOT REUSE, regenerate each session

## Live state (from /v0100-bootstrap):

### Test status
- [cpu-l1-mmu-demo]: {{DEMO_STATUS}} (target: 6/6)
- [riscv-tests]: {{RISCV_STATUS}} (target: 40/40)

### Hard prerequisites (v0.10.0 launch gates)
| # | Item | Status |
|---|------|--------|
| 1 | [cpu-l1-mmu-demo] 6/6 | {{DEMO_BLOCKER}} |
| 2 | plugin-framework-cycle-precision 25/25 | {{CF_BLOCKER}} |

### Active OpenSpec changes
{{ACTIVE_CHANGES_LIST}}

## 必读 (静态, 见 AGENTS.md):
- `soc/cpu/docs/roadmap/execution-roadmap.md §5` — 立即下一步
- `docs/roadmap/strategy/a-plus-c-hybrid.md §3` — initiative 状态
- `openspec/changes/*/proposal.md` — active change Why 段
- `docs/architecture/adr.md` — ADR 注册表

## 按需加载路由 (L3/L4/L6 条件触发)

按本会话意图匹配触发场景，决定加读哪些文件。**反例行（行 9）：无触发场景则不加读——"惰性"是默认**。

| # | 触发场景（关键词） | 加读文件（限 section） | 优先级 vs L1/L2/L5 | 与默认"不读"的区别 |
|---|---|---|---|---|
| 1 | PoC-1 / MUL/DIV / multi-cycle / FSM / negotiate 实装 | `references/decision-1-plugin-evolution.md §3` + `ADR-082` 仅 Context+Decision 节 + `references/poics-and-risks.md §1` PoC-1 行 | **高于 L5** | 不读 → negotiate 设计契约靠猜 |
| 2 | RV32C / 解码 / 跨页 fetch | `references/poics-and-risks.md §1` PoC-2 行 + `ADR-070`（若已起草） | 与 L5 同级 | 不读 → 成功标准（rv32uc ≥95%）缺失 |
| 3 | ICache / fence.i / 取指一致性 | `references/poics-and-risks.md §1` PoC-3 行 + `ADR-072`（若已起草） | 与 L5 同级 | 不读 → fence.i ≥10 断言要求漏 |
| 4 | BTB / GShare / 分支预测 / mispredict | `references/decision-1-plugin-evolution.md §1` + `references/poics-and-risks.md §1` PoC-6 行 | 与 L5 同级 | 不读 → 漏 Port 抽象纪律（CI 第 11 条） |
| 5 | ADR 起草/状态变更/编号 | `references/adr-matrix.md §1+§2+§7` + `docs/architecture/adr.md` 注册表 | **最高**（先于一切写操作）| 不读 → 撞号（050/051 事故重演） |
| 6 | CH_MEM / 双模 / elaborate / ch_state_machine / 对拍 | `docs/lessons/phase-6c-elaboration-substrate.md` 陷阱清单节 + `ADR-040 v2.0` Decision 节 | 与 L2 同级 | 不读 → 15 类已知陷阱逐个重踩 |
| 7 | 对外汇报 / 性能对标 / 决策 3 / chip-selector 定位 | `references/multi-core-comparison.md §1+§2` | 低于 L5（仅此类会话需要）| 不读无执行损失，仅汇报失真 |
| 8 | 追溯某版本为什么变（如 8a14402 性质） | `CHANGELOG.md` 仅 `[Unreleased]` + 目标版本节（**不读全文**） | L6 叙事层 | 不读无执行损失，仅缺历史叙事 |
| 9 | **以上均不涉及** | **零加读**——仅 L1 §3 / L2 §5 / L5 proposal + adr.md | — | **显式默认"惰性 opt-in"**，防清单广播 |

### 链路纪律（ADR 引用追踪）

- **depth ≤ 2**：单条 ADR 只读 `## Context` + `## Decision`（约 40% 篇幅）；`Consequences` / `Risks` 仅在做回退决策时读
- **路径不可达即停**：ADR-070~081 等规划中编号尚未起草，agent 沿链接走到不存在文件时应停止，**禁止脑补**（"未起草" ≠ "内容为空"）
- **升级触发（客观）**：ADR 总数 >15 或首次观测到链读事故时，本节升级为 P0 硬门禁

## 不要做 (静态, 见 AGENTS.md):
- ❌ backout commit `8a14402` (ADR-049 根基)
- ❌ 新建与 `mfc-cpu-pipeline-multi-cycle-fsm` 并行的 multi-cycle change
- ❌ 触碰已 archive 的 `wave3-*` initiative
- ❌ 在 `cpu-pipeline-multi-cycle` 上加任务（已 superseded-by mfc）
- ❌ 不以 `.omo/drafts/` 内容为决策依据（历史草案，已被 ADR/OpenSpec 吸收或废弃，反 SSOT）

## Smart recommendations (auto-inferred from hard_prerequisites):

{{SMART_RECOMMENDATIONS}}

## 启动清单 (新会话必做的前 5 步):
1. `openspec list` — 确认 active changes
2. `cat soc/cpu/docs/roadmap/execution-roadmap.md | sed -n '/## 5\./,/## 6\./p'` — 确认立即下一步
3. `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` — 确认回归状态（若 FAIL 优先 debug change）
4. `bash tools/verify_plugin_decision.sh` — 确认 CI 门禁
5. 报告你看到的状态 + 建议下一步
````

## Step 3: Inject smart recommendations

Based on parsed `## hard_prerequisites` table, generate `{{SMART_RECOMMENDATIONS}}`:

### Case A: `[cpu-l1-mmu-demo]` FAIL + cycle-precision incomplete
```markdown
🔴 **PoC-1 启动被双重阻塞**:
- [cpu-l1-mmu-demo] 1/6 FAIL → 执行 `debug-cpu-l1-mmu-demo-paddr-regression/`
- plugin-framework-cycle-precision 0/25 → P1#4 owner 收官

📋 **推荐下一步**: 先修回归（按优先级），再收官 cycle-precision
```

### Case B: `[cpu-l1-mmu-demo]` FAIL + cycle-precision done
```markdown
🔴 **PoC-1 启动被 [cpu-l1-mmu-demo] 回归阻塞**

📋 **推荐下一步**: 执行 `debug-cpu-l1-mmu-demo-paddr-regression/`
   - Phase A: 加 trace 到 `ibus.h` + `picolibc_host_memory.h`
   - Phase B: 二分定位真凶 (4 嫌疑 A/B/C/D)
   - Phase C: 修
   - Phase D: 回归验证
   - Phase E: 清理 trace
   - Phase F: archive
```

### Case C: Both prerequisites PASS
```markdown
✅ **v0.10.0 启动硬前置全过**

📋 **推荐下一步**: 启动 `mfc-cpu-pipeline-multi-cycle-fsm` Phase A
(MulDivFsmPlugin TLM 骨架 + ADR-046/047/082 集成)
```

### Case D: `## workspace_health` non-clean (dirty tree or orphan changes)

**优先于 Case A/B/C** —— 这是唯一会让 prompt 输出**错误事实**的场景（HEAD 显示干净、实际有未提交改动）。Agent 在进入 PoC 推进前必须先复盘脏工作区。

```markdown
🟡 **工作区不干净，先复盘再进 Case A/B/C**

`## workspace_health.working_tree`: ❌ dirty (<N> files, e.g. ?? tools/v0100-bootstrap.sh)
`## workspace_health.orphan_changes`: 🟡 archive 候选 (e.g. <change-name> 25/25)

📋 **推荐下一步**:
1. dirty tree → 跑 `git status` 复盘上次未提交改动；提交或 stash 后重启 v0100-bootstrap
2. orphan changes → 跑 `openspec archive <change-name>` (满足 100% done 的 change)
3. workspace_health clean 后, 重跑 /v0100-bootstrap 进入 Case A/B/C
```

**Case D 升级触发**：dirty tree 永久（或 orphan change 数 >1）应拒绝推进 PoC。

# Mode: Review

Same data collection as Generate mode, but output a structured review report instead of a session prompt.

## Output: Review report

1. **Current state summary** (1 paragraph)
2. **Hard prerequisites status** (table with ✅/❌ markers)
3. **Recent changes** (last 5 commits with what changed)
4. **Risk assessment**:
   - Stale docs (任何 markdown 已 N 天未更新 + 内容腐化风险)
   - Orphan changes (tasks 全 done 但未 archive)
   - Failing CI gates (verify_adr / verify_plugin_decision / check_plugin_portability)
5. **Recommendations** (ordered list 1-N of next actions)
6. **Questions for user** (things requiring human decision, not autonomous)

# What NOT to do

- Do NOT modify any project files (this skill is read-only)
- Do NOT run cmake/build (read-only session)
- Do NOT make any commits
- Do NOT bypass the bash script and hardcode state — always run script for live state
- Do NOT reuse a previous session's prompt — always regenerate (timestamp + HEAD in header enables verification)

# Why this skill exists

Cross-session continuity problem: opencode sessions are stateless. Without this skill, each new session needs to:
1. Run `openspec list` + read state files (5+ commands)
2. Parse and understand current state
3. Determine which hard prerequisite is blocking
4. Construct a session-start prompt manually

This skill automates all 4 steps in one invocation, with intelligent recommendations based on parsed state.

# Underlying tools

| Layer | Tool |
|-------|------|
| Bash script | `tools/v0100-bootstrap.sh` (project-level, git-tracked) |
| State sources | `openspec list`, `bash tools/sync_strategy_status.sh --dry-run`, `./build/bin/chipforge_tests`, `git log`, `git rev-parse HEAD` |
| Hardcoded facts | Only file paths and command names (NO state) — all state is fetched live |

# Maintenance

The skill body contains only pointers (file paths, command names). All state is fetched live from the project. This means:
- Skill does NOT need updates when state changes (✅ no腐化面)
- Skill DOES need updates when file paths change (rare, only on architecture decisions)
- Bash script (`tools/v0100-bootstrap.sh`) needs updates when commands change (e.g., new test family added)

# Output to user

The skill should output a single markdown code block containing the session-bootstrap prompt, ready for copy-paste as the first message of