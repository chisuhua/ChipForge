## Tasks

> docs-only change. 18 tasks, 2-5 分钟粒度.

### Phase 0 — Pre-flight (基线锁定)

- [ ] ### Task 0.1: 跑 `bash tools/run_chipforge_tests.sh` → 锁定 458/459 baseline
- [ ] ### Task 0.2: `bash tools/check_plugin_portability.sh` → 12/12 baseline
- [ ] ### Task 0.3: `bash tools/verify_plugin_decision.sh` → 8/8 baseline
- [ ] ### Task 0.4: `bash tools/verify_adr.sh` → 0 fail baseline
- [ ] ### Task 0.5: `bash tools/doc_link_check.sh` → 0 fail baseline

### Phase A — AGENTS.md §0.5 Per-turn 硬性规范 (路径 1)

- [ ] ### Task A.1: 起草 `### 0.5. Per-turn read+output 硬性规范` 段 (10-15 行, 含 3-step read + 2-step output + Before/After 对照表)
- [ ] ### Task A.2: 起草段写入临时文件 `/tmp/opencode/scope-section-0.5.md`
- [ ] ### Task A.3: 用 Python 插入到 AGENTS.md `## Build Agent Work Scope Matrix (POC)` H2 之后, `### 1. 状态来源` 之前
- [ ] ### Task A.4: 验证 `awk '/^## Build Agent Work Scope Matrix/,/^### 1\./' AGENTS.md` 输出含 §0.5 标题
- [ ] ### Task A.5: 验证 §1-9 内容 (parent change 的 9 段) 完整保留未破坏

### Phase B — AGENTS.md §10/§11/§12 slash command 文档 (路径 2)

- [ ] ### Task B.1: 起草 `### 10. /scope-show` 段 (15-20 行: trigger / 3-step action / 示例输出)
- [ ] ### Task B.2: 起草 `### 11. /scope-history` 段 (10-15 行)
- [ ] ### Task B.3: 起草 `### 12. /scope-reset` 段 (10-15 行)
- [ ] ### Task B.4: 起草 3 段写入临时文件 `/tmp/opencode/scope-slash-commands.md`
- [ ] ### Task B.5: 用 Python 追加到 AGENTS.md `## Build Agent Work Scope Matrix (POC)` 段尾, 现有 `---` 之前 (与 §0.5 + §1-9 一起, 整个 section 完整)

### Phase C — 验证 (硬 gate)

- [ ] ### Task C.1: `git diff --name-only origin/main..HEAD | grep -E '^(ip|include|src|tests|tools)/' | grep -v '\.md$'` 应空 (0 业务代码改)
- [ ] ### Task C.2: `git diff --stat AGENTS.md` 应只显示 `+60-80 lines` (本 change 增量), 不破坏 parent change 内容
- [ ] ### Task C.3: `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] ### Task C.4: `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] ### Task C.5: `bash tools/verify_adr.sh` 0 fail
- [ ] ### Task C.6: `bash tools/doc_link_check.sh` 0 fail
- [ ] ### Task C.7: `bash tools/run_chipforge_tests.sh` 0 regression (458/459 baseline)

### Phase D — openspec archive + sync

- [ ] ### Task D.1: 确认 `openspec/changes/scope-state-show-and-read-hint/{proposal,tasks}.md` 已写
- [ ] ### Task D.2: 确认 `openspec/changes/scope-state-show-and-read-hint/specs/.../spec.md` 已写 (7 Requirement)
- [ ] ### Task D.3: 确认 proposal.md frontmatter 含 `initiative: wave5-isa-coverage-and-bp` + `manual_deps: [scope-matrix-proposal]` (已加)
- [ ] ### Task D.4: `openspec validate scope-state-show-and-read-hint` 0 fail
- [ ] ### Task D.5: `openspec archive scope-state-show-and-read-hint -y` 成功
- [ ] ### Task D.6: 跑 `bash .rddf/sync_strategy_status.sh` 检查 strategy.md §7 派生

### Phase E — git commit + final state

- [ ] ### Task E.1: 单 commit `git add -A && git commit -m "governance: enhance scope matrix with per-turn hint + slash commands"`
- [ ] ### Task E.2: 写 builder handoff final state (current_phase=phase-3-archive, approval_status=archived)
- [ ] ### Task E.3: 验证 `git status` empty (working tree clean)
- [ ] ### Task E.4: 跑 `openspec list` 验证 2 个 change 全部 archive (scope-matrix-proposal + scope-state-show-and-read-hint)
