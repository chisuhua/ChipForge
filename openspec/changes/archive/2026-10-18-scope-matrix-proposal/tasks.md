## Tasks

> docs-only change. 每个 task 2-5 分钟粒度。

### Phase 0 — Pre-flight (基线锁定)

- [ ] ### Task 0.1: 跑 `bash tools/run_chipforge_tests.sh` → 锁定 458/459 baseline
- [ ] ### Task 0.2: `bash tools/check_plugin_portability.sh` → 12/12 baseline
- [ ] ### Task 0.3: `bash tools/verify_plugin_decision.sh` → 8/8 baseline
- [ ] ### Task 0.4: `bash tools/verify_adr.sh` → 0 fail baseline
- [ ] ### Task 0.5: `bash tools/doc_link_check.sh` → 0 fail baseline

### Phase A — AGENTS.md 新 section (主实施)

- [ ] ### Task A.1: 看 AGENTS.md 现有结构找插入点 (推荐 `## 代码组织` 后 or `## 构建与测试` 前)
- [ ] ### Task A.2: 起草 `# Build Agent Work Scope Matrix (POC)` section (含 5 categories + 取值表 + 切换触发 + 开场报告格式)
- [ ] ### Task A.3: 加 Mermaid 图（可选 - 加成）画 scope 5-category 关系
- [ ] ### Task A.4: 插入到 AGENTS.md 选定位置
- [ ] ### Task A.5: 验证 `grep -n 'Build Agent Work Scope Matrix' AGENTS.md` 输出 ≥1 命中

### Phase B — .rddf/state/scope.json 初始 state

- [ ] ### Task B.1: 起草 schema v1 初始 JSON (current_scope=governance/governance + history 1 项)
- [ ] ### Task B.2: 写入 `.rddf/state/scope.json`
- [ ] ### Task B.3: 验证 `python3 -c "import json; d=json.load(open('.rddf/state/scope.json')); print(d['schema_version'])"` 输出 `scope-v1`

### Phase C — AGENTS.md section 不能破坏 role.json

- [ ] ### Task C.1: `git status .rddf/state/role.json` 检查无改动
- [ ] ### Task C.2: 验证 `cat .rddf/state/role.json` 仍是 `current_role=builder` (不污染)

### Phase D — validation & tests

- [ ] ### Task D.1: `openspec validate scope-matrix-proposal` 0 fail
- [ ] ### Task D.2: `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] ### Task D.3: `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] ### Task D.4: `bash tools/verify_adr.sh` 0 fail
- [ ] ### Task D.5: `bash tools/doc_link_check.sh` 0 fail
- [ ] ### Task D.6: `bash tools/run_chipforge_tests.sh` 0 regression (458/459 PASS baseline unchanged)

### Phase E — spec-delta + openspec archive

- [ ] ### Task E.1: 确认 `openspec/changes/scope-matrix-proposal/specs/scope-matrix-proposal/spec.md` 已写 (9 Requirement)
- [ ] ### Task E.2: 确认 proposal.md frontmatter 含 `initiative: governance` (已加)
- [ ] ### Task E.3: 跑 `openspec archive scope-matrix-proposal -y` 成功
- [ ] ### Task E.4: 验证 `openspec/specs/scope-matrix-proposal/spec.md` 自动衍生（D3 mapping）
- [ ] ### Task E.5: 跑 `bash .rddf/sync_strategy_status.sh` 检查 strategy.md §7 派生

### Phase F — git commit + push-ready

- [ ] ### Task F.1: 单 commit `git add -A && git commit -m "governance: add Build Agent Work Scope Matrix to AGENTS.md"`
- [ ] ### Task F.2: rdd-workflow artifacts commit (improvement draft + openspec archive + spec-delta + strategy.md auto-sync)
- [ ] ### Task F.3: (推后) `git push` 由用户决策

### Phase G — governance memorial

- [ ] ### Task G.1: 确认 `code-test-ownership-report.md` 的 §F.5 漂移已治理（4/5 完成），无需引用本 change
- [ ] ### Task G.2: 写一个简短的 CHANGELOG entry (follow-up commit) 标记本次 governance 增强
