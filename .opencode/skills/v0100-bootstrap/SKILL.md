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
| `## honesty_audit` | AGENTS.md/CHANGELOG.md 声明 vs ctest/doc_link_check/ip/cpu/test/add.elf 实测对账 |
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

### Recent commits (last 5)
```
{{RECENT_COMMITS}}
```

### Workspace health
- working_tree: {{WORKING_TREE_STATUS}}
- orphan_changes: {{ORPHAN_CHANGES_STATUS}}

### Hard prerequisites (v0.10.0 launch gates)
| # | Item | Status |
|---|------|--------|
| 1 | [cpu-l1-mmu-demo] 6/6 | {{DEMO_BLOCKER}} |
| 2 | plugin-framework-cycle-precision 25/25 | {{CF_BLOCKER}} |

### Active OpenSpec changes
{{ACTIVE_CHANGES_LIST}}

### Honesty audit (声称 vs 实测, K3 fix 2026-09-30)
| 指标 | 声称来源 | 声称数字 | 实测 | 一致? |
|------|---------|---------|------|------|
| [cpu] | {{CLAIM_CPU_SRC}} | {{CLAIM_CPU}} | {{CPU_MEASURED}} | {{CPU_HONESTY}} |
| [cpu-integration] | {{CLAIM_CPUINT_SRC}} | {{CLAIM_CPUINT}} | {{CPUINT_MEASURED}} | {{CPUINT_HONESTY}} |
| [cpu-l1-mmu-demo] | {{CLAIM_DEMO_SRC}} | {{CLAIM_DEMO}} | {{DEMO_MEASURED}} | {{DEMO_HONESTY}} |
| doc_link_check | (无显式声称) | — | {{BROKEN_COUNT}} broken | {{DOC_HONESTY}} |

## 必读 (静态, 见 AGENTS.md):
- `AGENTS.md §路线 / Roadmap 类文档` — 文档导航入口（首次必读，含 4 类文档职责对照 + 记忆口诀）
- `soc/cpu/docs/roadmap/execution-roadmap.md §5` — 立即下一步
- `openspec/changes/*/proposal.md` — active change Why 段

## 按需加载路由 (L3/L4/L6 条件触发)

按本会话意图匹配触发场景，决定加读哪些文件。**反例行（行 9）：无触发场景则不加读——"惰性"是默认**。

| # | 触发场景（关键词） | 加读文件（限 section） | 优先级 vs L1/L2/L5 | 与默认"不读"的区别 |
|---|---|---|---|---|
| 1 | PoC-1 / MUL/DIV / multi-cycle / FSM / negotiate 实装 | `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md §3` + `docs/architecture/adr/ADR-082-plugin-negotiate-capability.md` §Context+Decision + `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-1 行 | **高于 L5** | 不读 → negotiate 设计契约靠猜 |
| 2 | RV32C / 解码 / 跨页 fetch | `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-2 行 + `ADR-070`（若已起草） | 与 L5 同级 | 不读 → 成功标准（rv32uc ≥95%）缺失 |
| 3 | ICache / fence.i / 取指一致性 | `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-3 行 + `ADR-072`（若已起草） | 与 L5 同级 | 不读 → fence.i ≥10 断言要求漏 |
| 4 | BTB / GShare / 分支预测 / mispredict | `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md §1` + `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-6 行 | 与 L5 同级 | 不读 → 漏 Port 抽象纪律（CI 第 11 条） |
| 5 | ADR 起草/状态变更/编号 | `soc/cpu/docs/roadmap/references/adr-matrix.md §1+§2+§7` + `docs/architecture/adr.md` 注册表 | **最高**（先于一切写操作）| 不读 → 撞号（050/051 事故重演） |
| 6 | CH_MEM / 双模 / elaborate / ch_state_machine / 对拍 | `docs/lessons/phase-6c-elaboration-substrate.md` 陷阱清单节 + `ADR-040 v2.0` Decision 节 | 与 L2 同级 | 不读 → 15 类已知陷阱逐个重踩 |
| 7 | 对外汇报 / 性能对标 / 决策 3 / chip-selector 定位 | `soc/cpu/docs/roadmap/references/multi-core-comparison.md §1+§2` | 低于 L5（仅此类会话需要）| 不读无执行损失，仅汇报失真 |
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

## 启动清单 (新会话必做的前 6 步):
1. `openspec list` — 确认 active changes
2. `cat soc/cpu/docs/roadmap/execution-roadmap.md | sed -n '/## 5\./,/## 6\./p'` — 确认立即下一步
3. `cat openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md | grep -E "^## Phase |^### Phase "` — 列出所有 phase 头, 确认当前 phase
4. `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` — 确认回归状态（若 FAIL 优先 debug change）
5. **CI 门禁快验**（PR 阻塞门禁, 任一 FAIL 必须先修/报告才能继续）:
   ```bash
   bash tools/verify_adr.sh                  # ADR 注册表与代码实现对齐
   bash tools/verify_plugin_decision.sh      # D4 Plugin-style 业务代码静态检查
   bash tools/check_plugin_portability.sh    # ADR-040 移植性约束
   ```
   - 三脚本任一 ❌ → **立即报告 + 停止**; 不要绕过门禁继续 implement
   - `tools/doc_link_check.sh` 不在本步（虽推荐但非 PR 阻塞; 仅当步骤 6 报告 doc 链接异常时单独跑）
6. 报告你看到的状态 + 建议下一步
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

### Case E: `## honesty_audit` 失配 (声明数字 vs 实测) — 2026-09-28 新增

触发条件: 任何一条"声称 vs 实测 对账表"行 = ❌。意味着 AGENTS.md / CHANGELOG.md 数字与当前 main HEAD ctest 实测不一致。

```markdown
🔴 **🟡 文档诚实性失配** (Metis 评审驱动, 2026-09-28)

`## honesty_audit.声称 vs 实测 对账` 出现 ❌:
- [cpu]: 声称 117/117 → 实测 116/117
- [cpu-integration]: 声称 81/81 → 实测 77/81
- [cpu-l1-mmu-demo]: 声称 6/6 → 实测 1/6
- doc_link_check: 17 broken (无显式声称)

📋 **推荐下一步**:
1. 🔴 立即停止推进 hotfix (`debug-cpu-l1-mmu-demo-paddr-regression`) — 文档未诚实声明的"已修复"会误导后续会话
2. 在 AGENTS.md §已知测试状态 加 ⚠️ HONESTY NOTE (5 分钟) — 声明数字是 v0.7.0 archive 时快照
3. 在 CHANGELOG.md v0.8.0 §Verification 修订 — 改为实测数字
4. 跑 `bash tools/v0100-bootstrap.sh review` 看 `## review_recommendations` 段获取修 hotfix 的执行步骤
5. hotfix Phase F archive 后, 回填实测数字
```

**Case E 升级触发**: 4 项对账中 ≥2 项 ❌ 应**拒绝推进 PoC**，直到 AGENTS.md/CHANGELOG.md 数字与 ctest 一致。

**为什么是 Case E 不是 Case A**: Case A 是 `[cpu-l1-mmu-demo]` FAIL — 这是**代码 bug**。Case E 是**文档与代码不一致** — 这是**流程失败**（P1#3 归档验证时没跑集成测试）。两者都阻塞 PoC-1 启动，但 Case E 优先级更高（修文档 5 分钟 vs 修 hotfix 1-2 周）。

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

## 当文档结构变化时同步本 skill

> **触发条件**: 任何 roadmap 文档的新增/删除/重命名/section 编号变更（如 `execution-roadmap.md §3.5 新增`、`AGENTS.md §路线 / Roadmap 类文档 新增`）。

**同步步骤**:
1. **跑链接检查**: `bash tools/doc_link_check.sh` — 验证 §必读 / 路由表中的路径仍可解析
2. **检查 §必读 引用**: 若 `AGENTS.md §路线 / Roadmap 类文档` 等导航节的内容变化,本 skill §必读 第 1 条的描述要保持同步
3. **检查路由表路径**: 若 `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md` / `ADR-082` / `soc/cpu/docs/roadmap/references/poics-and-risks.md` 等路径变化,路由表具体路径必须更新
4. **检查 MIGRATION_LOG**: 若本次变化足够大(新增/合并/重命名文档),在 `docs/MIGRATION_LOG.md` 追加变更记录

**反向警示**: 避免"因为变了所以同步修所有引用"的反射。文档结构变化时,先问"这个变化是否影响 SKILL.md 的 §必读 / 路由表 / #Maintenance 三处",影响才动;不影响则不动(如 `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md §1+§2` 内容调整不影响 skill 路径)。

## Known issues in `tools/v0100-bootstrap.sh` (修订时发现, 2026-09-30)

> **触发背景**: 2026-09-30 修订 `last-bootstrap-prompt.md` 时实测 bash script 输出, 发现以下非阻塞腐化。**tools/ scope, 不在 v0.10.0 wave5 scope**, 报告 surface 在此供未来 sessions 知情。

| # | Issue | 症状 | 绕过方法 |
|---|-------|------|---------|
| K1 | `:442 BROKEN_COUNT unbound variable` | `set -u` 触发 unbound 错误, `doc_link_check` 行渲染空白（实际全 PASS, exit-code 路径正确） | 输出含 `[WARN]` 标记但不传播; honest audit `doc_link_check` 行显示空 broken 数, 需看 K2 状态 |
| K2 | bash script 与 SKILL.md 模板**双重权威** | bash script 输出 schema（test_status/hard_prerequisites/honesty_audit 等）≠ SKILL.md 模板全文; 完整 prompt（含按需加载路由/不要做/smart recs/启动清单）需 agent 手工合成 | 接受现状: bash script 提供 state, agent 用 SKILL.md 模板合成最终 prompt |
| K3 | CHANGELOG.md baseline 漂移不自检 | bash script 不主动 diff `CHANGELOG.md §Verification` vs 实测数字 | Case E 触发时人工对比; 长期修法: bash script 加 `CHANGELOG.md` 行级 regex 对账 |
| K4 | "不要做" 列表静态, 不随 active changes 演化 | 如 wave6 启动后 vexii-riscv-parity-poc 应进"不要 implement" 列表, 但 skill 模板不感知 | 未来 wave 切换时, agent 手工加条目 (上次 prompt rev 5 加了 5 条) |

**Skill 维护动作**: 若修 K1/K3, 同步更新 SKILL.md §启动清单 step 6 报告模板字段。

## 审查已保存的 prompt（"audit mode", 2026-09-30 新增）

> **触发场景**: 用户问"审查 `last-bootstrap-prompt.md` 是否适合启动"或类似审计类需求 — 既不是 generate 也不是 review mode。

**审计清单** (顺序检查, 任一 FAIL 给出明确改进建议):

| # | 检查项 | 通过标准 | FAIL 行动 |
|---|--------|---------|---------|
| A1 | 时间戳新鲜度 | timestamp ≤ 4 小时前 | 建议 `bash tools/v0100-bootstrap.sh > last-bootstrap-prompt.md` regenerate |
| A2 | HEAD 一致性 | prompt 内 HEAD = `git rev-parse --short HEAD` | regenerate |
| A3 | 路由表路径可达 | 所有 `soc/cpu/docs/roadmap/references/*.md` 存在 + ADR-082 等存在 | 标 ❌ + 给修复建议 (见 K2) |
| A4 | 启动清单完整 | 6 步齐 (openspec list / §5 read / tasks phase / chipforge_tests / 3 CI scripts / report) | 标 ❌ + 补缺失步骤 |
| A5 | "不要做" 防御层 | ≥ 10 条 + 含 archive/vexii/tools-scope/CHANGELOG-stale 等关键禁止 | 补缺 |
| A6 | Honesty audit 全 ✅ | 4 项对账全 ✅ (无 ❌) | Case E 触发: 报告失配 + 不 implement |
| A7 | Smart recs 与 hard_prerequisites 一致 | demo PASS + cycle-precision 降级 → 推荐主路径 = mfc Phase A | 不一致: 重写 |
| A8 | 修订 metadata 存在 | 文件末尾 "修订要点 N 处" + 来源 + 日期 | 加 metadata |

**输出格式** (给用户):

```markdown
# last-bootstrap-prompt.md 审计报告

**生成时间**: <文件 timestamp>
**HEAD**: <文件 HEAD> vs `<实际 HEAD>` (一致/不一致)
**审计结果**: ✅ 适合启动 / 🟡 适合但建议 N 项改进 / 🔴 不适合, 需 regenerate

## 改进项 (按 A1-A8 顺序, 标 FAIL 的)
1. A3: 路由表行 5 引用 `references/poics-and-risks.md` 缺前缀 → 加 `soc/cpu/docs/roadmap/`
2. A4: 启动清单缺 CI 门禁 step → 补 3 个 CI 脚本

## 已知 issues (引用 K1-K4)
- K1: bash script BROKEN_COUNT unbound → 输出 `doc_link_check` 行渲染空白但实际全 PASS (exit-code 路径正确)
```

**不替代 regenerate**: 审计只 surface 问题, 不自动修。若用户想修, 推荐 regenerate + 手工合成完整 prompt (bash script 输出 + SKILL.md 模板 + 手动 rev notes)。



# Output to user

## Per-mode output contract

- **Generate mode** outputs a single markdown code block containing the complete session-bootstrap prompt (header + Live state + 必读 + 按需加载路由 + 不要做 + Smart recs + 6-step 启动清单), ready for copy-paste as the first message of the new session.
- **Review mode** outputs a structured review report (state summary + hard prereqs + recent changes + risk assessment + recommendations + questions), NOT a session-bootstrap prompt.
- **Audit mode** outputs an audit report on an existing saved prompt (freshness / HEAD / paths / checklist completeness / honesty / smart-recs / metadata), NOT a session-bootstrap prompt.

**Self-verification after composition**: before emitting, confirm `HEAD` in output matches `git rev-parse --short HEAD` and timestamp is current; otherwise regenerate.