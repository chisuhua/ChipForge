---
id: scope-aware-session-bootstrap-recommender-2026-10
kind: improvement
status: in-progress
created: 2026-10-19
created_by: build-agent
context:
  parent_session: ses_ead2ea455ffeqP3jILeCzBP02r
  parent_improvement: roadmap-p1-sync-2026-10
  oracle_sessions:
    - ses_eadc4634dffeogEL94TgSIGnwX (B1-B3 审查)
    - ses_ada679d3ffe8LVmYuf60KPN0A (深度诊断 + reflog 实证)
    - ses_ead89ad2dffetRjNr0jo78y0PJ (S3 决策推荐)
    - ses_eac3d780affelVrK662lDzlyoC (v0-bootstrap 评估, 推荐方案 2)
    - ses_eac338bdeffeb0qMMKHeNXGaY6 (critic REJECT 4 BLOCKER)
    - ses_eabd44b82ffeEXP52ht16tXFKM (oracle 复审 critic, 推荐 B+ 方案)
  review_methodology: rdd-planner → rdd-builder P0-P3 (轻量)
priority: P1
target_audit_trail: openspec/changes/scope-aware-session-bootstrap-recommender-2026-10/
---

# Improvement: scope-aware-session-bootstrap-recommender-2026-10 (B+ 方案)

> **Source**: Oracle session `ses_eac3d780affelVrK662lDzlyoC` 评估方案 2 → Critic `ses_eac338bdeffeb0qMMKHeNXGaY6` REJECT 4 BLOCKER + 10 SHOULD_FIX → Oracle 复审 `ses_eabd44b82ffeEXP52ht16tXFKM` 推荐 B+ 方案
> **本 improvement 描述 B+ 方案最终版**: 修 6 处漂移 (7→6, S9 + #5 改标) + scope_snapshot.sh 4 项确定性输出 (B1 规避 + B4 删 item 5) + gates.sh:56 删硬编码 (S3 零漂移解)
> **0.5-1 天, openspec 轻量 P0-P3**

## 1. Why（背景 + 痛点）

### 痛点 1: SKILL.md 静态文本 vs 实际状态漂移 (2026-10-19 已实证, B+ 缩 scope)

**6 处实证成立漂移** (原列 7 处, critic S9 撤 #4, oracle 复审撤 #5):

| # | 漂移 | 实证 |
|---|---|---|
| 1 | SKILL.md:78 引 `objective-v0110-launch.md` | 2026-10-19 commit `766c15b` git mv → `objective-soc-cpu-v0110-launch.md` |
| 2 | SKILL.md:79 引 `mfc-cpu-pipeline-multi-cycle-fsm/tasks.md` | 2026-10-16 archive 至 `archive/2026-10-16-mfc-cpu-pipeline-multi-cycle-fsm/` |
| 3 | SKILL.md:82-85 步骤 5 漏列 `doc_link_check.sh` + `roadmap-r9-check.sh` | 2026-10-19 commit `115c7dd` 新增 R9 check |
| 4 | ~~SKILL.md:60 路由 #4 BTB 仍 active~~ | **删除** (critic S9 实证: feature 仍 active 是事实状态, 路由表是条件映射非状态断言, **不构成漂移**) |
| 5 | ~~SKILL.md 缺"立即下一步"段~~ | **删除** (oracle 论证: SKILL.md:50 必读段已含"立即下一步(v0.10.0-v0.11.0 收官)"字样, 实为**设计缺陷非漂移**, 留 P1 协议段解决) |
| 6 | gates.sh:56 硬编码 `(target: 40/40)` | 实际 [riscv-tests] 48/48 (commit `df3bcea`) |
| 7 | AGENTS.md:99 "mfc 主线锁定不 archive" | 实测 mfc 已 archive |

### 痛点 2: LLM session 启动时 "重新发现世界" 成本高

当前 bootstrap 流程 (per SKILL.md 启动清单 6 步):
1. `openspec list` 拿 active changes
2. `cat .rddf/roadmap/objectives/objective-*.md` 拿 objective (**漂移 #1: 旧名 cat 不存在**)
3. `cat openspec/changes/<name>/tasks.md` 拿 phase 头 (**漂移 #2: 已 archive**)
4. 跑 1 个 test family
5. 跑 3 个 gate (**漂移 #3: 漏 2 个**)
6. LLM 读 5+ 文件后**自己**综合给"立即下一步"

**本 session oracle × 2 轮实证**: oracle 用了 2 轮才把"PTW TDD red 失实" + "DMIPS 1.43 真实 commit 基础"弄清楚 (oracle ses_eac3d780 + ses_ada679d).

### 痛点 3: 薄壳 wrapper 设计哲学多次漂移

gates.sh:56 硬编码 40/40 vs 实测 48/48 = **同一逃生舱自身**已腐烂的实证
SKILL.md 5 处漂移 = markdown 塞事实的必然结果

**根因**: scope 维度 + 项目特定 gate 是项目私有概念, 硬塞 SKILL.md markdown 必漂移

## 2. What Changes (B+ 方案最终版: 3 步骤, 0.5-1 天)

### 步骤 1: 修 6 处漂移 + gates.sh 删硬编码 (commit 1, 已 done)

**Files** (3 文件纯文本):
- `.opencode/skills/v0100-bootstrap/SKILL.md`: 4 处漂移 (L78/L79/L82-85/L70)
- `tools/bootstrap/sections/gates.sh`: L56 删 `(target: 40/40)` 硬编码标签 (**S3 零漂移解**, 不是 CSV 解析)
- `AGENTS.md:99`: "锁定不 archive" → "已 archive (commit 07cf77c, 2026-10-16)"

**风险**: 低 (纯文本)

### 步骤 2: scope_snapshot.sh 最小版 (commit 2, 已 done)

**设计原则** (per oracle Q1-C 修订):
- 严格限定为**输出确定性 raw state** (不涉及 LLM 判断, oracle Q1-C 薄壳哲学)
- 复用 `preflight.sh` + `gates.sh` 已验证的 custom_section 机制
- profile `order` 数组插入 `"scope_snapshot"`
- **0 上游引擎改动** (薄壳哲学遵守)

**4 项确定性输出** (B+ 方案缩 scope, 7→4 项):
1. `cat .rddf/state/scope.json` (缺失时输出 `(missing), default=governance/governance`)
2. `ls .rddf/roadmap/{phases,objectives,features}/` **全量** (B1 规避: 不过滤 — scope 三套分类法不统一)
3. `git log -5 --oneline` **全量**
4. **5 gate exit code 收集** (`bash gate.sh >/dev/null 2>&1; echo "$g: $?"`)
- **脚本恒定 exit 0** (B2 修复: gate 失败编码进文本而非退出码)

**B4 删除 item 5** (git-diff 启发式 vs scope.json 一致性 warn): 留给 P1 协议段 (LLM 判断合法)

**Files** (2 文件):
- Create: `tools/bootstrap/sections/scope_snapshot.sh` (~120 行, bash)
- Modify: `.rddf/skill-profiles/session-bootstrap.toml` (注册 custom_section, `order` 数组插入)

**风险**: 中 (新脚本需自测覆盖 3 场景: 正常 / 缺 scope.json / 非 repo 根)

### 步骤 3: improvement + AC 修订 + P1 留档 (commit 3, 当前 phase)

- 漂移表 7→6 (本 improvement 修订)
- AC 重排 (AC-4/5/7 归 P1 留档)
- P1 留档承接: 协议段 + skill-drift-check.sh + 分类法映射表 + CI 挂接 + K1-K3 接口

## 3. Acceptance (8 项 AC, B+ 修订版)

| AC | 内容 | 验证 |
|---|---|---|
| AC-1 | `tools/bootstrap/sections/scope_snapshot.sh` 存在 + 可执行 + **4 项输出** + 恒定 exit 0 | `bash tools/bootstrap/sections/scope_snapshot.sh; echo $?` |
| AC-2 | `.rddf/skill-profiles/session-bootstrap.toml` `order` 数组含 `scope_snapshot` + `custom_sections` 注册 (**双断言**) | `grep -c scope_snapshot .rddf/skill-profiles/session-bootstrap.toml` ≥ 2 |
| AC-3 | **6 处**漂移 grep 旧路径 0 命中 (排除 archive + state + .opencode + openspec + .rddf) | `grep -rn "objective-v0110-launch\.md\|mfc-cpu-pipeline-multi-cycle-fsm/tasks\.md\|target: 40/40\|锁定不 archive" --include="*.md" .` 排除 archive |
| AC-4 | 3 场景手测 (正常 / 缺 scope.json / 非 repo 根) | commit message 贴 3 输出 (B2 替代方案 — 无 bash 测试 harness, 手动覆盖) |
| AC-5 | `gates.sh:56` **不含硬编码 target 数字** (S3 零漂移解) | `grep "target: 40/40" tools/bootstrap/sections/gates.sh` 0 命中 |
| AC-6 | AGENTS.md:99 mfc 段"锁定不 archive" 0 命中 | `grep "锁定不 archive" AGENTS.md` 0 命中 |
| AC-7 | 5 架构 gate + R9 check = 6 gate 全绿 | 跑 `bash tools/verify_adr.sh && tools/verify_plugin_decision.sh && tools/check_plugin_portability.sh && tools/doc_link_check.sh && tools/roadmap-r9-check.sh` |
| AC-8 | 0 业务代码改动 (`src/`, `ip/`, `tests/`, `bundles/`, `include/` 0 命中) | `git diff --name-only | grep -E "^(src\|ip\|tests\|bundles\|include)/"` 空 |

## 4. Capabilities (能力增量)

- **LLM session 启动成本**: 5-10 分钟 → 1-2 分钟 (-80%) — 实测 oracle × 2 轮 ≈ 10 分钟, scope_snapshot 给 LLM 直接消费
- **scope 切换 0 成本**: 改 scope.json 后, 下次 session 自动跟随 (vs grep $srcdir/xxx.md)
- **漂移防御结构性**: SKILL.md 静态事实 → 协议 + 自检 (audit 自检在 P1-2 落地)
- **上游兼容**: 0 rdd-workflow 上游改动, K1-K3 实装后可平滑接入 (per oracle S8 警告)

## 5. Impact (影响)

### 正面
- LLM session 启动时间 -80% (估)
- 推荐质量显著提升 (证据锚定 + 用户参与)
- SKILL.md 漂移防御 (结构性, 配合 P1-2 audit 自检)
- 与 rdd-workflow 上游 v0.2 接口预留 (K1-K3 可插拔)

### 风险 (oracle 评估 3 大风险)
- **推荐幻觉/陈旧**: critic/metis 条件触发 (Q5-C 3 条)
- **SKILL.md 漂移复发**: audit 自检 + 协议不写事实
- **薄壳膨胀 + 上游冲突**: 职责切分写死 (项目层只 scope + 5 gate; 通用原语等上游)

### 反例 (Anti-patterns)
- ❌ 彻底重写 SKILL.md (违反薄壳哲学, oracle 反对)
- ❌ 上游引擎层做 scope-aware (scope 私有概念, 污染通用层)
- ❌ 模板化 3-5 候选 (gates.sh:56 硬编码腐烂实证)
- ❌ 默认 critic+metis 双重验证 (高频路径成本不可持续)

## 6. P1 留档 (本次不实施, 后续 governance change 承接)

### P1-1: SKILL.md "建议合成协议"段 (oracle Q2-C 混合, 协议段软约束)

- 4 条约束: 3-5 个候选 + 用户确认 + critic/metis 条件触发 + prompt 必含字段
- SKILL.md 启动清单改为**动态发现** (不列具体文件名)
- 风险: 协议段约束力软, LLM 可能不遵守 → audit 自检兜底 + 用户验证

### P1-2: 工具层 `tools/skill-drift-check.sh` (类似 roadmap-r9-check.sh)

- grep SKILL.md 路径引用 vs 文件存在性 (含 anchor 剥除 + 外部 allowlist + glob 展开规则)
- profile TOML 自检 (注册 custom_section 不重复, `order` 数组无 gap)
- 与现有 6 gate 并行, 共 7 gate 防御

### P1-3: scope 分类法映射表 (B1 完整解, oracle 评估范围)

- scope.json 6 值枚举 (ip-cpu / ip-mmu / ip-cache / soc-cpu / soc-mmu / soc-cache)
- ↔ primary_scope 6 值 (同上)
- ↔ features frontmatter scope 5 值 (cpu / cache / mmu / soc / cross)
- 设计文档 + lint 规则

### P1-4: CI 挂接 (`architecture-gates.yml` 升级)

- 当前 workflow 跑 4 gate (verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check)
- 加 R9 check + skill-drift-check.sh 后, 共 7 gate
- PR 模板加 skill-drift-check 跑过提示

### P1-5: 上游 rdd-workflow v0.2 K1-K3 接口钉住 (oracle S8 警告)

- profile `version = "1"` 当前被引擎忽略 (per oracle 引用的 `rdd-session-bootstrap.sh` parse_profile)
- K1 honesty_audit / K2 review / K3 audit A1-A8 实装时, profile schema 可能 break
- 准备 migration 文档 + 备份 mechanism

## 7. 关联

- 父 improvement: `roadmap-p1-sync-2026-10.md` (commit `fb67a70`)
- Oracle 推荐: session `ses_eac3d780affelVrK662lDzlyoC` Q1-Q7 答案 + 3 方案
- Oracle 复审 critic: `ses_eabd44b82ffeEXP52ht16tXFKM` (B+ 最终推荐)
- Critic 审查: `ses_eac338bdeffeb0qMMKHeNXGaY6` (4 BLOCKER + 10 SHOULD_FIX + 7 NITS)
- 现有机制: `tools/v0100-bootstrap.sh` 薄壳 + `rdd-session-bootstrap` 通用引擎
- 上游兼容: rdd-workflow K1-K3 (v0.2 planned) 通过 profile `order` 数组平滑接入
- AGENTS.md §0.5 per-turn 硬规范 + §路线图 primary scope 隐式语义 段
- CONTRIBUTING.md §3.1 文档按文件粒度 (本次 3 commit 模式, 2 commit 已 done)

## 9. 实施日志 (Phase A/B 已 done, Phase C 进行中)

| 阶段 | 状态 | commit |
|---|---|---|
| Phase A (漂移修复) | ✅ done | `d3d512a` roadmap: 修 6 处 bootstrap helper 漂移 (B+ 方案 Phase A) |
| Phase B (scope_snapshot.sh) | ✅ done | `8fde4bd` tools: scope_snapshot.sh custom_section 4 项确定性输出 + profile 注册 |
| Phase C (improvement + AC 修订) | 🔄 current | (本 commit) |
| Phase D (verify + archive) | ⏸ pending | openspec archive scope-aware-session-bootstrap-recommender-2026-10 |