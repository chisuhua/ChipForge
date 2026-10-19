## Tasks

> B+ 方案 (oracle 推荐 + critic REJECT 反馈修订). governance meta change (修 session bootstrap helper).
> 3 commit, 0.5-1 天, openspec 轻量 P0-P3.

### Phase 0 — Pre-flight (基线锁定)

- [ ] **Task 0.1**: 跑 `bash tools/verify_adr.sh` → 锁定 0 fail baseline
- [ ] **Task 0.2**: `bash tools/verify_plugin_decision.sh` → 8/8 baseline
- [ ] **Task 0.3**: `bash tools/check_plugin_portability.sh` → 12/12 baseline
- [ ] **Task 0.4**: `bash tools/doc_link_check.sh` → 0 fail baseline
- [ ] **Task 0.5**: `bash tools/roadmap-r9-check.sh` → 10 fragment pass baseline
- [ ] **Task 0.6**: 6 处漂移 grep 旧路径 → 锁定 baseline (改前快照)

### Phase A — 漂移修复 (commit 1, 纯文本)

- [ ] **Task A.1**: 修 SKILL.md:78 (`objective-v0110-launch.md` → `objective-soc-cpu-v0110-launch.md`)
- [ ] **Task A.2**: 修 SKILL.md:79 (删/改 mfc tasks.md 行, 改历史措辞)
- [ ] **Task A.3**: 修 SKILL.md:82-85 (步骤 5 补 `doc_link_check.sh` + `roadmap-r9-check.sh`)
- [ ] **Task A.4**: 修 SKILL.md:70 (不要做段 mfc 引用刷新)
- [ ] **Task A.5**: 修 `tools/bootstrap/sections/gates.sh:56` (删 `(target: 40/40)` 硬编码标签)
- [ ] **Task A.6**: 修 AGENTS.md:99 mfc 段 ("锁定不 archive" → 已 archive 事实)
- [ ] **Task A.7**: 跑 `git diff --stat` 确认只触 SKILL.md + gates.sh + AGENTS.md (3 文件, 0 业务代码)
- [ ] **Task A.8**: 跑 5 架构 gate + R9 check = 6 gate 全绿 (AC-7 验证)
- [ ] **Task A.9**: grep 旧路径 0 命中 (AC-3 验证)
- [ ] **Task A.10**: 跑 `git add` (3 文件) + commit "roadmap: fix 6 drift in SKILL.md + gates.sh + AGENTS.md"

### Phase B — scope_snapshot.sh 最小版 (commit 2)

- [ ] **Task B.1**: 起草 `tools/bootstrap/sections/scope_snapshot.sh` (~60-80 行, bash 4+)
  - 首行 `cd "$(git rev-parse --show-toplevel)"` (S7 CWD 防御)
  - 4 项确定性输出 (B1 规避 / B2 修复 / B4 删 item 5):
    1. `cat .rddf/state/scope.json` (缺失时输出 `(missing), default=governance/governance`)
    2. `ls .rddf/roadmap/{phases,objectives,features}/` 全量
    3. `git log -5 --oneline` 全量
    4. 5 gate exit code 收集 (`bash gate.sh >/dev/null 2>&1; echo "$g: $?"`)
  - 脚本自身恒定 `exit 0` (B2 修复)
  - 颜色输出仅 tty 时启用 (与 roadmap-r9-check.sh 一致)
- [ ] **Task B.2**: `chmod +x tools/bootstrap/sections/scope_snapshot.sh`
- [ ] **Task B.3**: 自测 3 场景 (AC-4 验证):
  - 场景 1: 正常 (从 repo 根跑) → 4 项输出 + exit 0
  - 场景 2: 缺 scope.json (`mv .rddf/state/scope.json /tmp/opencode/scope.bak && bash tools/bootstrap/sections/scope_snapshot.sh`) → 退化输出 `(missing)` + exit 0
  - 场景 3: 非 repo 根 (`cd /tmp/opencode && bash /workspace/project/ChipForge/tools/bootstrap/sections/scope_snapshot.sh`) → 4 项输出 + exit 0
- [ ] **Task B.4**: 改 `.rddf/skill-profiles/session-bootstrap.toml`:
  - `order` 数组插入 `"scope_snapshot"`
  - `custom_sections` 注册 `{name: "scope_snapshot", script: "tools/bootstrap/sections/scope_snapshot.sh"}`
- [ ] **Task B.5**: AC-2 验证: `grep -c scope_snapshot .rddf/skill-profiles/session-bootstrap.toml` ≥ 2
- [ ] **Task B.6**: 跑 `bash tools/v0100-bootstrap.sh` (完整 bootstrap 流程) → 验证 `## scope_snapshot` 段出现
- [ ] **Task B.7**: 跑 6 gate 全绿 (AC-7 验证)
- [ ] **Task B.8**: 跑 `git add` (2 文件) + commit "tools: scope_snapshot.sh custom_section 4 项确定性输出 + profile 注册"

### Phase C — improvement + AC 修订 (commit 3)

- [ ] **Task C.1**: 改 `.rddf/improvements/scope-aware-session-bootstrap-recommender-2026-10.md`:
  - 漂移表 7→6 (删 #4 BTB 路由 S9 标注, #5 "立即下一步" 改"设计缺陷/P1 留档")
  - AC 重排 (删 AC-4/5/7 归 P1, 新增 AC-1-8 8 项)
  - 加 §"P1 留档" 段 (协议段 + skill-drift-check.sh + 分类法映射表 + CI 挂接 + K1-K3 接口)
- [ ] **Task C.2**: 写 P1 improvement draft: `.rddf/improvements/scope-aware-session-bootstrap-p1-deferred-2026-10.md` (5 段式, 接续 5 项 P1 留档)
- [ ] **Task C.3**: 跑 6 gate + improvement 验证 (AC-7 + AC-8)
- [ ] **Task C.4**: 跑 `git add` (2 文件) + commit "improvement: B+ 修订 + P1 留档同步"

### Phase D — Verify + Archive (硬 gate)

- [ ] **Task D.1**: `git log --oneline -5` → 3 commit 顺序正确 (漂移修复 → scope_snapshot → improvement)
- [ ] **Task D.2**: `git diff --name-only origin/main..HEAD` → 0 业务代码改动 (AC-8 验证)
- [ ] **Task D.3**: `bash tools/verify_adr.sh` 0 fail
- [ ] **Task D.4**: `bash tools/verify_plugin_decision.sh` 8/8
- [ ] **Task D.5**: `bash tools/check_plugin_portability.sh` 12/12
- [ ] **Task D.6**: `bash tools/doc_link_check.sh` 0 broken
- [ ] **Task D.7**: `bash tools/roadmap-r9-check.sh` 10 fragment pass
- [ ] **Task D.8**: `bash tools/bootstrap/sections/scope_snapshot.sh` → 4 项输出 + exit 0
- [ ] **Task D.9**: `bash tools/v0100-bootstrap.sh` → 验证 `## scope_snapshot` 段出现 + `## hard_prerequisites` 等其他段正常
- [ ] **Task D.10**: `bash tools/run_chipforge_tests.sh` → 0 regression (458/459 baseline 维持)
- [ ] **Task D.11**: `openspec validate scope-aware-session-bootstrap-recommender-2026-10` → pass
- [ ] **Task D.12**: `openspec archive scope-aware-session-bootstrap-recommender-2026-10 -y` → 归档

## Done criteria

- 3 commit 完成
- 6 gate + R9 check 全绿
- `## scope_snapshot` 段在 bootstrap 输出中
- openspec archive 成功
- P1 improvement draft 留档

## Out of scope (P1 留 governance change)

- 协议段 (oracle Q2-C 混合, LLM 行为约束软)
- `tools/skill-drift-check.sh` (S5 复杂度高)
- 分类法映射表 (B1 完整解, scope.json 6 值 ↔ primary_scope 6 值 ↔ scope 5 值)
- CI 挂接 (S6: `architecture-gates.yml` 仅 4 gate)
- 上游 rdd-workflow v0.2 K1-K3 接口钉住 (S8)
- 路由 #4 (BTB) / #5 (立即下一步段) 设计缺陷
