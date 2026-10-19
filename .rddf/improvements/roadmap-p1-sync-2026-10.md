---
id: roadmap-p1-sync-2026-10
kind: improvement
status: draft
created: 2026-10-19
created_by: build-agent
context:
  parent_change: roadmap-honesty-audit-2026-10 (commit 3a33611, archived 2026-10-19)
  oracle_sessions:
    - ses_eadc4634 (B1-B3 审查)
    - ses_ada679d (深度诊断 + reflog 实证)
priority: P1
---

# Improvement: roadmap-p1-sync-2026-10

## 1. Why（背景）

路线图审计（commit 3a33611 roadmap-honesty-audit-2026-10, AC 7/7 PASS）已完成 4 处最小诚实修正：
1. CHANGELOG.md :562-565 PTW TDD red marker 失实删除
2. test_ptw_real_memory.cpp 4 处过期注释清理（[red] → [stub-guard]/[api]）
3. AGENTS.md :63 baseline-known FAIL 列表 dhrystone/DMIPS 移除
4. AGENTS.md 顶部 feature 表 feat-dhrystone-dmips-gate → done（auto-gen）

但**仍有 6 处 P1 文档漂移未修复**——本 improvement 承接之：

### 漂移清单（来自 oracle ses_ada679d 诊断）

| # | 漂移 | 严重度 |
|---|---|---|
| P1-1 | strategy.md §3 wave4 仍"exploring (占位)"但 2/3 change 已 archive | SHOULD_FIX |
| P1-2 | strategy.md §6 时间表未刷新：v0.8.0 (2026-12 中旬) 实际 2026-10-08 已 archive / v0.9.0 (2027-02) wave4 2/3 done / v0.10.0 (2027 Q1) v0.10.0-interim tag 已发 | SHOULD_FIX |
| P1-3 | strategy.md §10 v0.11.0 Go/No-Go 行含 "rv32um 01-13 仍有 FAIL → hotfix" 已失实分支 | SHOULD_FIX |
| P1-4 | phase-2-ip-mmu.md "🟡 部分收官" + phase-3-ip-cache.md "⏸ deferred 未启动" 与 strategy §3 "✅ 已收官" 矛盾 | SHOULD_FIX |
| P1-5 | feat-rv32um-m-extension / feat-mfc-multi-cycle-fsm / feat-phase-6d-rtl-verification 3 个 feature frontmatter 仍 status: active（archive 已发生） | SHOULD_FIX |
| P1-6 | objective-soc-cpu-v0110-launch.md §2 数字表 "当前 [riscv-tests] 40/48 (rv32um 0/8)" + §1 驱动诊断 #1 "rv32um 0/8 FAIL" 已失实；§8 Sprint 1 (mfc-extract-fsm-h) 已 archive 后台账缺补 3 条 | SHOULD_FIX |

### 战略层决策（待 user 拍板）

| # | 决策点 | 我的推荐 | 论据 |
|---|---|---|---|
| **S3** | BP-BTB / CoreMark ≥1.9 是否 v0.11.0 硬门禁？ | **BP 不进 v0.11.0** | DMIPS 1.43 已由 commit `f95b2d0` (dhrystone harness 修复) 实现，**未靠 BP**；strategy §10 现行立场是不含 BP；oracle 评估"R-BTB-1 砍分叉"风险已变；让 BP 走 v1.0.0 窗口（已规划 2027 Q3） |
| **S5** | [mmu-verilator] 目标数 5/5 vs 4/4 不一致 | 选 5/5（objective-v0110 + strategy §10 多数派）| 与 mmu-chmem-pipeline-integration HANDOFF §14 一致 |

## 2. What Changes（范围）

### 必做（6 文件纯 docs）
1. `.rddf/roadmap/strategy.md` §3 + §6 + §10 三处更新
2. `.rddf/roadmap/phases/phase-2-ip-mmu.md` 转 completed + `phases/phase-3-ip-cache.md` 勾 P2#7
3. `.rddf/roadmap/features/feat-rv32um-m-extension.md` / `feat-mfc-multi-cycle-fsm.md` / `feat-phase-6d-rtl-verification.md` frontmatter status → done
4. `.rddf/roadmap/objectives/objective-soc-cpu-v0110-launch.md` §2 + §1 数字刷新 + §11 台账补 3 条
5. CHANGELOG.md :551 抬头加快照标注（"e9850d2 实测 vs f95b2d0/c1cb8ec 继承"）
6. S3 决策落到 strategy §6/§10 + objective §9.2 + phase-4-soc-cpu.md（按 user 决定方向）

### 治理改进（结构性防漂移）
7. `.rddf/sync_strategy_status.sh` 加 lint mode 进架构 gate：扫描 features/objectives/phases 引用的 change 名 vs `openspec/changes/archive/` 状态，drift → error
9. `tools/v0100-bootstrap.sh review` 的 honesty_audit 段加 grep 规则：CHANGELOG 新增 "TDD red" 声明时强制要求附当时 runner 输出原文

## 3. Acceptance（验收）

- [ ] AC-1: strategy.md §3 + §6 + §10 与 §7 派生表无漂移
- [ ] AC-2: phase-2 转 completed + phase-3 勾 P2#7 + 不再含已 archive 的 change 待办
- [ ] AC-3: 6 个 feature frontmatter status 与 archive 状态 100% 对齐（5 done + 1 active feat-bp-btb-gshare）
- [ ] AC-4: objective-v0110 §2 数字表反映真实状态（48/48, 59/59, 6/6, 3/3 plumbing） + §11 台账含 mfc-extract-fsm-h archive / rv32um 8/8 / DMIPS 1.43 三条
- [ ] AC-5: CHANGELOG :551 抬头加 snapshot 标注
- [ ] AC-6: sync_strategy_status.sh lint mode 实装 + 跑通（feat 漂移 0）
- [ ] AC-7: S3 决策落地（strategy §6 / §10 + objective §9.2 + phase-4 收尾判据）一致

## 4. Capabilities（能力增量）

- 路线图文档与代码状态的**单向一致性**（archive event → 自动刷新 features/objectives/phases/AGENTS.md feature 表）
- honesty_audit 原则**结构化 enforcement**（CHANGELOG TDD red 声明必须附 runner 输出）
- v0.11.0 launch gate **真实阻塞路径清晰**（剩 1 项 mmu-chmem Phase B-E）

## 5. Impact（影响）

### 正面
- 路线图文档**从此与代码状态自动对齐**，未来类似漂移不再发生
- v0.11.0 launch gate **真实阻塞路径只剩 1 项**，决策可行性高
- CHANGELOG 诚实记录原则得到结构化保护

### 风险
- 路线图文档**版本节点时间表可能需要重排**（v0.8.0/v0.9.0 实际进展快于规划）
- sync_strategy_status.sh 加 lint mode 是**架构 gate 增强**，需 PR 评审
- 涉及 6 个文件 + 1 个工具脚本，scope 中等（~150 行改动）

### 反模式（Anti-pattern）
- ❌ "一次性手动 grep 全部 markdown 文件" — 应走 sync 自动化
- ❌ "改 sync_strategy_status.sh 同时改多个文件" — 拆成 sync lint + docs 同步两个独立 change

## 6. 关联

- 父 change: `roadmap-honesty-audit-2026-10` (commit 3a33611, 2026-10-19)
- Oracle 审计 session: `ses_eadc4634` (B1-B3 审查) + `ses_ada679d` (深度诊断)
- 当前活跃 initiative: `wave5-isa-coverage-and-bp` (v0.11.0 owner)
- 决策门禁: `objective-soc-cpu-v0110-launch.md §9.2 Go/No-Go`
- ADR: 暂无新增 ADR 需求；现有 ADR-040 v2.0 + ADR-046 + ADR-082 已覆盖