---
name: v0100-bootstrap
description: |
  ChipForge v0.10.0 wave5 session-bootstrap skill (薄壳 wrapper).

  Use this skill at the START of each new session continuing v0.10.0 work, or after a gap ≥1 day.

  Migration (2026-10-09): project-specific skill → thin wrapper around rdd-session-bootstrap (rdd-workflow 通用).
  All state collection / template synthesis logic now lives in:
  - rdd-workflow/skills/rdd-session-bootstrap/SKILL.md (通用协议层)
  - rdd-workflow/skills/rdd-session-bootstrap/rdd-session-bootstrap.sh (通用引擎)
  - .rddf/skill-profiles/session-bootstrap.toml (项目 profile)
  - tools/bootstrap/sections/*.sh (项目 hooks)
  - tools/v0100-bootstrap.sh (薄壳入口, 59 lines vs 旧 622 lines)

  Two modes:
  - /v0100-bootstrap (default): Generate paste-ready prompt
  - /v0100-bootstrap review: Deep state analysis (PoC v0.2 planned)
---

# v0100-bootstrap (薄壳 wrapper, PoC v0.1)

> **Migration 2026-10-09**: This skill is now a thin wrapper around the **rdd-session-bootstrap** skill (rdd-workflow ecosystem, PoC v0.1.0).
> All state collection logic, template synthesis, and audit checklist now live in the rdd-workflow skill.
> This file is reduced from 392 lines to ~80 lines.

## When to use

Invoke in any of these situations:
1. **Starting a new session** to continue v0.10.0 wave5-isa-coverage-and-bp work
2. **Returning after ≥1 day gap** to ChipForge
3. **After major artifact** completion (ADR/archive/change creation)
4. **Before any PR** touching `.rddf/roadmap/` or `openspec/changes/`

## Usage

```bash
# 跑薄壳入口 (调用通用引擎)
bash tools/v0100-bootstrap.sh

# 输出: Layer 3 raw state markdown (来自 rdd-session-bootstrap 引擎)
# Layer 4 paste-ready prompt 需 LLM 用通用 SKILL.md §Template 合成
```

Underlying: `bash tools/v0100-bootstrap.sh` (thin wrapper, 59 lines) → `bash rdd-session-bootstrap.sh` (通用引擎) + `.rddf/skill-profiles/session-bootstrap.toml` (项目 profile) + `tools/bootstrap/sections/*.sh` (项目 hooks).

## 必读 (项目特定, 引用 AGENTS.md)

- `AGENTS.md §路线 / Roadmap 类文档` — 文档导航入口（含 4 类文档职责对照 + 记忆口诀）
- `.rddf/roadmap/strategy.md §6` + `.rddf/roadmap/objectives/objective-*.md §1 Why now` — 立即下一步（v0.10.0-v0.11.0 收官）
- `openspec/changes/*/proposal.md` — active change Why 段

## 按需加载路由 (L3/L4/L6 条件触发)

| # | 触发场景 | 加读文件 |
|---|---------|---------|
| 1 | PoC-1 / MUL/DIV / multi-cycle / FSM | `docs/research/decision-1-plugin-evolution.md §3` + `ADR-082` + `docs/research/poics-and-risks.md §1` |
| 2 | RV32C / 解码 | `docs/research/poics-and-risks.md §1` PoC-2 行 + `ADR-070` |
| 3 | ICache / fence.i | `docs/research/poics-and-risks.md §1` PoC-3 行 + `ADR-072` |
| 4 | BTB / GShare / 分支预测 | `docs/research/decision-1-plugin-evolution.md §1` + `docs/research/poics-and-risks.md §1` |
| 5 | ADR 起草/状态 | `docs/research/adr-matrix.md §1+§2+§7` + `docs/architecture/adr.md` |
| 6 | CH_MEM / 双模 | `docs/lessons/phase-6c-elaboration-substrate.md` + `ADR-040 v2.0` |
| 7 | 对外汇报 / 性能对标 | `docs/research/multi-core-comparison.md §1+§2` |
| 8 | 追溯某版本为什么变 | `CHANGELOG.md` `[Unreleased]` + 目标版本节 |
| 9 | **以上均不涉及** | **零加读** (L1 §3 / L2 §5 / L5 proposal + adr.md) |

## 不要做 (项目特定)

- ❌ backout commit `8a14402` (ADR-049 根基)
- ❌ 新建与 `mfc-cpu-pipeline-multi-cycle-fsm` 并行的 multi-cycle change
- ❌ 触碰已 archive 的 `wave3-*` initiative
- ❌ 在 `cpu-pipeline-multi-cycle` 上加任务（已 superseded-by mfc）
- ❌ 不以 `.omo/drafts/` 内容为决策依据

## 启动清单 (项目特定, 6 步)

1. `openspec list` — 确认 active changes
2. `cat .rddf/roadmap/objectives/objective-v0110-launch.md` — 确认立即下一步
3. `cat openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md` — 列出 phase 头
4. `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` — 确认回归状态
5. **CI 门禁快验**:
   ```bash
   bash tools/verify_adr.sh
   bash tools/verify_plugin_decision.sh
   bash tools/check_plugin_portability.sh
   ```
6. 报告你看到的状态 + 建议下一步

## 通用协议 (引用 rdd-session-bootstrap 通用 core)

详细协议 (Generate/Review/Audit 3 modes, A1-A8 审计清单, env var 契约, self-verification) 见:
- `rdd-workflow/skills/rdd-session-bootstrap/SKILL.md` (通用协议层, ~250 行)
- `rdd-workflow/skills/rdd-session-bootstrap/rdd-session-bootstrap.sh` (通用引擎, ~380 行)
- `rdd-workflow/skills/rdd-session-bootstrap/templates/profile.toml.example` (TOML profile 模板)

## 已知 PoC 限制 (planned v0.2)

- K1: honesty_audit 原语未实装 (v0100 §7 honesty_audit 段未迁移)
- K2: review_mode 逃生舱未实装 (Case A-E 决策树未迁移)
- K3: audit mode 完整实现未实装 (A1-A8 审计清单未集成到引擎)

这些限制在新引擎跑 v0100 风格 profile 时表现为缺失对应 markdown 段 (workspace_health / preflight / hard_prerequisites 段正常输出, honesty_audit / review 段缺失)。

## Migration path (completed 2026-10-09)

| 阶段 | 状态 | 说明 |
|------|------|------|
| PoC v0.1.0 (5 内建 + 1 原语 + 1 逃生舱) | ✅ 完成 | 当前 |
| PoC v0.2.0 (2 原语 + 2 逃生舱全实装) | 📋 planned | 2026-Q4 |
| byte-equal 验证 (vs v0100-bootstrap.sh.bak 旧 622 行) | ⏸ 部分 | 引擎输出与旧 v0100 关键 sections (test_status / hard_prerequisites) 一致; honesty_audit / review 段待 v0.2 |

旧 622 行 v0100-bootstrap.sh 备份: `tools/v0100-bootstrap.sh.bak`
旧 392 行 SKILL.md 备份: `.opencode/skills/v0100-bootstrap/SKILL.md.bak`
