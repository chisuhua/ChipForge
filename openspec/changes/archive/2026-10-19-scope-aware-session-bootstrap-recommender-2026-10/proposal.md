---
initiative: governance
priority: P1
version_target: v0.11.0+post-launch
manual_deps: []
revision_history:
  - date: 2026-10-19
    author: rdd-builder (build agent session ses_ead2ea455ffeqP3jILeCzBP02r)
    summary: |
      Initial draft (B+ 方案, oracle 推荐 + critic REJECT 反馈修订后).

      Oracle session ses_eac3d780affelVrK662lDzlyoC 推荐方案 2 (修漂移 + scope_snapshot.sh 逃生舱 + 建议合成协议 + audit 自检).
      Critic REJECT 4 BLOCKER + 10 SHOULD_FIX + 7 NITS.
      Oracle 复审后推荐 B+ 方案: 修 6 处漂移 (7→6, S9 + #5 改标) + scope_snapshot.sh 4 项确定性输出 (B1 规避 + B2 改写 + B4 删 item 5) + gates.sh:56 删硬编码 target 标签 (S3 零漂移解).

      3 commit, 0.5-1 天, openspec 轻量 P0-P3.
      协议段 + skill-drift-check.sh + 分类法映射表 + CI 挂接 + K1-K3 接口钉住 留 P1 governance change 承接.

      manual_deps: [] (无前置, 与已落 commit 9af06e8 feat scope 字段 / 766c15b primary scope 改名 / 115c7dd R9 check gate 协同).
---

# scope-aware-session-bootstrap-recommender-2026-10 — 修 SKILL.md 6 处漂移 + scope_snapshot.sh 4 项输出 (B+ 方案)

## Why

`.opencode/skills/v0100-bootstrap/SKILL.md` 5 处文本漂移导致**每个新 LLM session 都被指令 cat 不存在/已 archive 的文件** (活性错误信息). 本 session oracle × 2 轮才搞清状态, improvement §1 痛点 2 自述.

**实证漂移清单 (6 处, 全部成立)**:

| # | 漂移 | 实证 |
|---|---|---|
| 1 | SKILL.md:78 引 `objective-v0110-launch.md` | 2026-10-19 commit `766c15b` git mv → `objective-soc-cpu-v0110-launch.md` |
| 2 | SKILL.md:79 引 `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md` | 2026-10-16 archive 至 `openspec/changes/archive/2026-10-16-mfc-cpu-pipeline-multi-cycle-fsm/` |
| 3 | SKILL.md:82-85 步骤 5 漏列 `doc_link_check.sh` + `roadmap-r9-check.sh` | 2026-10-19 commit `115c7dd` 新增 R9 check (5 架构 gate 之一) |
| 4 | ~~SKILL.md:60 路由 #4 BTB 仍 active~~ | **删除** (critic S9 实证: feature 仍 active 是事实状态, 路由表是条件映射非状态断言, **不构成漂移**) |
| 5 | ~~SKILL.md 缺"立即下一步"段~~ | **删除** (critic 实证: SKILL.md:50 必读段已含"立即下一步(v0.10.0-v0.11.0 收官)"字样, 实为**设计缺陷非漂移**, 留 P1 协议段解决) |
| 6 | `gates.sh:56` 硬编码 `(target: 40/40)` | 实际 [riscv-tests] 48/48 (commit `df3bcea`) |
| 7 | AGENTS.md:99 "mfc 主线锁定不 archive" | 实测 mfc 已 archive |

**其他架构性腐朽**:
- `gates.sh` 硬编码 target 数字 (S3 实证: 即便从 CSV 读也是转移漂移非消除)
- SKILL.md 静态文本 vs 实际状态天然漂移 (markdown 塞事实模式已多次实证失败)
- scope 三套分类法 (scope.json `ip/cpu` / features frontmatter `scope` `cpu` / objective phase `primary_scope` `ip-cpu`) 互不兼容 (B1)

详见 `.rddf/improvements/scope-aware-session-bootstrap-recommender-2026-10.md §1`.

## What Changes

### In Scope (本 change 实施, 0.5-1 天)

1. **修 SKILL.md 5 处漂移** (commit 1, 纯文本):
   - L78: `objective-v0110-launch.md` → `objective-soc-cpu-v0110-launch.md`
   - L79: 删/改 mfc tasks.md 行 (已 archive, 改历史措辞)
   - L82-85: 补 `doc_link_check.sh` + `roadmap-r9-check.sh`
   - L70 "不要做" 段 mfc 引用刷新
2. **修 `tools/bootstrap/sections/gates.sh:56`** (commit 1, 1 行): 删 `(target: 40/40)` 硬编码标签 — 零漂移源解 (S3 优先, 比 CSV 解析更简单)
3. **修 AGENTS.md:99 mfc 段** (commit 1): "锁定不 archive" → 已 archive 事实
4. **新建 `tools/bootstrap/sections/scope_snapshot.sh` 最小版** (commit 2, ~60-80 行):
   - 首行 `cd "$(git rev-parse --show-toplevel)"` (S7 CWD 防御)
   - 4 项确定性输出 (B1 规避 — 不过滤 / B2 修复 — 采集 exit code 脚本恒定 exit 0 / B4 删 item 5):
     1. `cat .rddf/state/scope.json` (缺失时输出 `(missing), default=governance/governance`)
     2. `ls .rddf/roadmap/{phases,objectives,features}/` 全量 (B1 规避)
     3. `git log -5 --oneline` 全量
     4. 5 gate exit code 收集 (`bash gate.sh >/dev/null 2>&1; echo "$g: $?"`)
5. **注册 custom_section** (commit 2): `.rddf/skill-profiles/session-bootstrap.toml` 的 `order` 数组插入 `scope_snapshot`, `custom_sections` 注册
6. **improvement draft 升级** (commit 3): 漂移表 7→6 (S9 + #5 改标), AC 重排 (删 AC-4/5/7 归 P1), P1 留档承接

### Out of Scope (留 P1 governance change 承接)

- ❌ 协议段 (oracle Q2-C 混合, 但实施层面 LLM 行为约束软) — P1 留档
- ❌ `tools/skill-drift-check.sh` (S5 复杂度高) — P1 留档
- ❌ 分类法映射表 (B1 完整解, 需要设计 scope.json 6 值枚举 ↔ primary_scope 6 值枚举 ↔ scope 5 值枚举) — P1 留档
- ❌ CI 挂接 (S6: `architecture-gates.yml` 仅 4 gate) — P1 留档
- ❌ 上游 rdd-workflow v0.2 K1-K3 接口钉住 (S8) — P1 留档
- ❌ 路由 #4 (BTB) / #5 (立即下一步段) 设计缺陷 — P1 留档

## Capabilities

### MUST

- AC-1: `tools/bootstrap/sections/scope_snapshot.sh` 存在 + 可执行 + 4 项输出 + 恒定 exit 0
- AC-2: `.rddf/skill-profiles/session-bootstrap.toml` `order` 数组含 `scope_snapshot` + `custom_sections` 注册 (双断言)
- AC-3: 6 处漂移 grep 旧路径 0 命中 (S10 统一)
- AC-4: 3 场景手测 (正常 / 缺 scope.json / 非 repo 根) 贴输出到 commit message
- AC-5: `gates.sh:56` 不含硬编码 target 数字 (S3 零漂移解)
- AC-6: AGENTS.md:99 mfc 段"锁定不 archive" 0 命中
- AC-7: 5 架构 gate + R9 check = 6 gate 全绿 (无回归)
- AC-8: 0 业务代码改动 (`src/`, `ip/`, `tests/`, `bundles/`, `include/` 0 命中)

### MUST NOT

- ❌ 不动上游 rdd-workflow `_lib/roadmap_validate.py` (跨仓库改动, oracle 反对)
- ❌ 不动薄壳 wrapper 设计哲学 (oracle Q1-C 修订: 项目侧新 section hook, 不是独立平行 CLI)
- ❌ 不动 5 架构 gate 行为
- ❌ 不重写 SKILL.md (oracle 反对, 薄壳哲学违反)
- ❌ 不接 CI (本 change 不新增 gate, 留 P1)

## Acceptance

| AC | 内容 | 验证命令 |
|---|---|---|
| AC-1 | scope_snapshot.sh 4 项输出 + 恒定 exit 0 | `bash tools/bootstrap/sections/scope_snapshot.sh; echo $?` |
| AC-2 | profile TOML 双断言 (order + custom_sections) | `grep -c scope_snapshot .rddf/skill-profiles/session-bootstrap.toml` ≥ 2 |
| AC-3 | 6 处漂移 grep 旧路径 0 命中 | `grep -rn "objective-v0110-launch.md\|mfc-cpu-pipeline-multi-cycle-fsm/tasks" --include="*.md" .` 排除 archive |
| AC-4 | 3 场景手测 (正常 / 缺 scope.json / 非 repo 根) | commit message 贴 3 输出 |
| AC-5 | gates.sh:56 无硬编码 target 数字 | `grep "target: 40/40" tools/bootstrap/sections/gates.sh` 0 命中 |
| AC-6 | AGENTS.md:99 mfc 段无"锁定不 archive" | `grep "锁定不 archive" AGENTS.md` 0 命中 |
| AC-7 | 5 架构 gate + R9 check = 6 gate 全绿 | 跑 `bash tools/verify_adr.sh && tools/verify_plugin_decision.sh && tools/check_plugin_portability.sh && tools/doc_link_check.sh && tools/roadmap-r9-check.sh` |
| AC-8 | 0 业务代码改动 | `git diff --name-only | grep -E "^(src\|ip\|tests\|bundles\|include)/"` 空 |

## Tasks

详见 `tasks.md`.

## References

- Improvement draft: `.rddf/improvements/scope-aware-session-bootstrap-recommender-2026-10.md`
- Oracle 评估: `ses_eac3d780affelVrK662lDzlyoC` (Q1-Q7 + 3 方案推荐 B+)
- Critic 审查: `ses_eac338bdeffeb0qMMKHeNXGaY6` (4 BLOCKER + 10 SHOULD_FIX + 7 NITS)
- Oracle 复审 critic: `ses_eabd44b82ffeEXP52ht16tXFKM` (B+ 方案最终推荐)
- 父 improvement: `roadmap-p1-sync-2026-10.md` (commit `fb67a70`)
- 协同 commit:
  - `9af06e8` (feat frontmatter scope 字段, 2026-10-19)
  - `766c15b` (objective/phase 按 primary scope 改名, 2026-10-19)
  - `115c7dd` (R9 check gate, 2026-10-19)
- 上游 rdd-workflow `rdd-session-bootstrap` (薄壳 wrapper + 通用引擎)
- AGENTS.md §"路线图 primary scope 隐式语义" 段 (commit `766c15b` 落地)

## Supersedes / 关联

- **supersedes**: 漂移的 SKILL.md 文本 (本 change 修复)
- **superseded by**: 无 (本 change 是 P1 governance change 的止血部分)
- **manual_deps**: []
- **scope 维度**: governance (per AGENTS.md §Build Agent Work Scope Matrix)
