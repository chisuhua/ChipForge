# 修复计划 Design: 文档同步 + DRIFT 闭环 + CI 接入

> **作者**: Prometheus (planning consultant)
> **日期**: 2026-06-12
> **状态**: Proposed (待用户批准)
> **触发源**: 2026-06-12 架构对齐审计 (5 处文档不一致 + 3 处 DRIFT + 2 处轻微不一致)
> **审计源**: 上文"ChipForge 架构-状态对齐最终审计报告 (增强版)"

---

## 1. 目标 (Goals)

通过 4 个独立 OpenSpec change 闭环 2026-06-12 审计发现的所有 5 处文档不一致 + 3 处 DRIFT + 2 处轻微不一致,达到:

1. **文档-代码 100% 对齐**: roadmap / overview / interface-design / decision drafts 全部与 `bundles/` + `ip/cache/tlm/` + `src/cf_plugin/bridge/` 代码现实一致
2. **架构 DRIFT 0 个**: Bundle 双轨制 (DRIFT-1) + ch_stream 协议转换语义 (DRIFT-2) + ImplMode 悬挂 (DRIFT-3) 全部文档化消解
3. **CI 强制门禁**: 3 个验证脚本 (`verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh`) 接入 `.github/workflows/`,作为 PR 守门员
4. **可执行可回滚**: 每个 change 独立 archive/spec-sync, 失败可回滚到审计前状态 (commit `17782f4`)

---

## 2. 非目标 (Non-Goals)

| 不做 | 理由 |
|------|------|
| 实施 PA-6 (Phase 1.3d-extras ch_stream 协议转换) | 属于新功能开发,需独立 change;本次仅消 DRIFT-2 的**文档层**语义缺失 |
| 实施 PA-7 (Phase 1.4 baseline 对比) | 属于新功能开发,需独立 change |
| 修改 `bundles/mem_bundles.h` 字段类型 | DRIFT-1 解决的是**文档**,代码用 `cf::plugin::uint_t<N>` 是 D4 强制正确行为 |
| 引入新依赖 / 升级 CppTLM/CppHDL | 范围限制在文档 + 脚本 + CI |
| 修改 `verify_adr.sh` 脚本判定逻辑 | 仅接入 CI,不改逻辑 (agent #2 报 ⚠️ STALE 的脚本判定偏差属于 P2 范围外) |
| 修复 R6 (ch_stream 协议转换风险) / R7 (baseline 选型) | roadmap §3.2 已跟踪,不在本次范围 |

---

## 3. 4 阶段 OpenSpec Change 拆分

每个 change 独立,走完整 OpenSpec 流程 (`proposal.md` + `tasks.md` + `design.md` (适用) + `specs/*.md` (适用))。

### Change 1: `fix-roadmap-doc-sync-2026-06-12` (P1, 1-2h)

**目的**: 同步 5 处文档不一致中的 4 处 (审计 #1-#3 + #5 中 2 份决策草案子项)

| 任务 | 文件 | 修改 | 验收 |
|------|------|------|------|
| 1.1 | `docs/roadmap/phases/phase-1-tlm-foundation.md` | L3 Status 改 "In Progress (~65%, Phase 1.3 全部子任务完成, 2026-06-10)"; L19-L82 任务清单 1.1 (4 项) + 1.2 (2 项) + 1.3 (4 项) 任务勾选 [x] 共 10 项; L86-L101 任务清单 1.4 (4 项) + 1.5 (4 项) 保留 [ ]; L3-L4 顶部新增"实施历史"区块 (引用 `bundle-bridging` commits 073402c/e8deacc/26fe7d2..c8d1dd1) | `grep -cE "^- \[x\]" phase-1-tlm-foundation.md` = 10 (1.1/1.2/1.3 全部勾选); `grep "Not Started"` 不出现在 L3-L10; 1.4/1.5 仍为 [ ] |
| 1.2 | `ip/cache/README.md` | L1 banner "(Phase 1.2 + 1.3a + 1.3b + 1.3c + 1.3e, 2026-06-10)" 补 "+ 1.3d"; §9.3 改"Phase 1.3d-extras (ch_stream adapter) 推迟 (PA-6),完整 e2e `instantiateAll` 待启动"; 删除"Phase 1.3d 将完成 ModuleFactory 注册" | L1 banner 含 "1.3d"; §9.3 不含 "Phase 1.3d 将完成" |
| 1.3 | `docs/architecture/overview.md` | L3 快照日期 2026-06-09 → 2026-06-12; L7 改"应用层 Phase 1.3 已落地" + 列举 (`bundles/6 Bundle` + `ip/cache/tlm/L1CachePlugin` + `soc/l1_cache_minimal.json`); 删除"⚠️ 应用层待建设" | L3 含 "2026-06-12"; L7 "应用层待建设" 不出现; L7 列举 3 个落地项 |
| 1.4 | `.omo/drafts/decision-plugin-framework-2026-06-08.md` | L7 状态行 `Proposed (待用户最终确认)` → `Accepted (2026-06-08, 与 ADR-037 Accepted 一致)` | L7 字符串 "Accepted (2026-06-08" 出现; L7 不含 "Proposed (待用户最终确认)" |
| 1.5 | `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` | L7 状态行 `Proposed v2 (待用户最终确认 D1/D1'/D1''/D2/D3)` → `Accepted v2 (2026-06-10, 6 commit 落地 26fe7d2..c8d1dd1, 与 roadmap-status §2 Phase 1.3 v2 决策一致)` | L7 字符串 "Accepted v2" 出现 |

**修复范围限定** (避免范围蔓延):
- **不**修复审计 #5 中 ADR-040 表内不一致 → Change 3 处理
- **不**修复审计 #4 (DRIFT-1 Bundle 双轨制) → Change 2 处理
- **不**修复审计 #4 中其他 2 处 (架构文档与代码不同源) → 已知,本次仅消第 #4

**OpenSpec artifacts**:
- `openspec/changes/fix-roadmap-doc-sync-2026-06-12/proposal.md` (1 Why/2 What Changes/3 Impact)
- `openspec/changes/fix-roadmap-doc-sync-2026-06-12/tasks.md` (5 tasks)
- 无 `design.md` (范围清晰,无架构变更)
- 无 `specs/*.md` (无 delta spec)

**依赖**: 无
**并行**: 可与 Change 2/3/4 并行 (但 Change 1 是 P1, 建议先做)
**回滚**: `git revert <commit>` 即可 (单原子 commit)

### Change 2: `fix-bundle-type-dual-track-2026-06-12` (P1, 1-2h)

**目的**: 消 DRIFT-1 (Bundle 字段类型双轨制, 审计 #4)

| 任务 | 文件 | 修改 | 验收 |
|------|------|------|------|
| 2.1 | `docs/architecture/interface-design.md` | §1 (L1-L75) 顶部新增"Bundle 形态演进"小节 (200-300 字), 明确 3 阶段:<br>- **Phase 1 (TLM)**: `cf::plugin::uint_t<N>` POD struct (D4 强制, 不依赖 CppHDL/CppTLM)<br>- **Phase 5 (RTL 协同)**: BundleMapper 模板将 POD 映射为 `ch_uint<N>` + `bundle_base<Self>`<br>- **Phase 6 (完整框架)**: 自动 codegen 派生两套 Bundle | "Phase 1 (TLM)" + "Phase 5 (RTL)" + "Phase 6" 3 个时段均出现; §1 描述 `ch_uint<bundle_base>` 风格时加交叉引用指向新小节 |
| 2.2 | `docs/architecture/adr.md` ADR-024 详细记录 (L728-754) | 新增"形态切换"小节, 引用 interface-design.md 新小节; 现状 `⚠️ Mapper 模板未实现` 描述保持 | 引用 `interface-design.md` 路径正确 |
| 2.3 | `bundles/README.md` | §2 "设计原则" 表新增一行"Bundle 形态分阶段 (Phase 1 POD → Phase 5/6 升级)" + 引用 interface-design.md 新小节 | 表格行数 +1 |
| 2.4 | `.omo/drafts/bundle-mapper-phase-5-6-decision.md` (新建) | 决策草案: 明确 Phase 5/6 BundleMapper 实施路径, 引用 ADR-024 + interface-design.md 新小节 | 文件存在 ≥ 200 行 |

**OpenSpec artifacts**:
- `openspec/changes/fix-bundle-type-dual-track-2026-06-12/proposal.md`
- `openspec/changes/fix-bundle-type-dual-track-2026-06-12/tasks.md` (4 tasks)
- `openspec/changes/fix-bundle-type-dual-track-2026-06-12/design.md` (Bundle 形态演进设计)
- 无 `specs/*.md` (interface-design.md 在 `docs/architecture/`, 不在 `openspec/specs/`)

**依赖**: 无
**并行**: 可与 Change 1/3/4 并行
**回滚**: `git revert <commit>`

### Change 3: `adr-040-internal-consistency-2026-06-12` (P2, 30min)

**目的**: 修复审计 #5 剩余 (ADR-040 §2.1 ✅ vs §2.3 🚧 表内不一致)

| 任务 | 文件 | 修改 | 验收 |
|------|------|------|------|
| 3.1 | `docs/architecture/adr.md` §2.1 (L112) | `ADR-040 \| TLM→HDL 移植性约束...` 行从"✅ 已实现" → 删除 (此 ADR 仍为 Phase 1 提案, 实质"array_store 已实现, 迁移手册待 Phase 5") | §2.1 不含 ADR-040 |
| 3.2 | `docs/architecture/adr.md` §2.3 (L131) | `ADR-040` 备注列 `🚧（array_store 已实现，迁移手册待 Phase 5 验证）` 保持不变 | 备注文字未变 |
| 3.3 | `docs/architecture/adr.md` §3.L ADR-040 详细记录 (L1153-1196) | 状态字段 `🚧 Phase 1 提案` 保持不变; 内容追加一行"§2.1/§2.3 分类调整 (2026-06-12): §2.1 移除本 ADR, 仅保留 §2.3 Phase 1 提案分类" | 追加行存在 |

**OpenSpec artifacts**:
- `openspec/changes/adr-040-internal-consistency-2026-06-12/proposal.md`
- `openspec/changes/adr-040-internal-consistency-2026-06-12/tasks.md` (3 tasks)
- 无 `design.md` (微小内部一致性)
- 无 `specs/*.md` (无 delta spec)

**依赖**: 无
**并行**: 可与 Change 1/2/4 并行
**回滚**: `git revert <commit>`

### Change 4: `ci-architecture-gates-2026-06-12` (P3, 2-3h)

**目的**: 把 3 个验证脚本接入 GitHub Actions, 作为 PR 守门员

**当前基础设施状态** (2026-06-12 验证):
- 已有 `.github/workflows/doc_check.yml` (文档检查)
- **无** `ci.yml` 或 `architecture-gates.yml`
- 本次新建 `architecture-gates.yml`,与 `doc_check.yml` 并列

| 任务 | 文件 | 修改 | 验收 |
|------|------|------|------|
| 4.1 | `.github/workflows/architecture-gates.yml` (新建) | 新增 job `architecture-gates`, trigger `on: pull_request`, 步骤:<br>1. `actions/checkout@v4`<br>2. `actions/setup-python@v5` (Bash 工具链)<br>3. `run: bash tools/verify_adr.sh` (期望 exit 0)<br>4. `run: bash tools/verify_plugin_decision.sh` (期望 exit 0, D4 业务 3/3)<br>5. `run: bash tools/check_plugin_portability.sh` (期望 exit 0, ADR-040 Tier-1 4/4)<br>6. 任意失败 → exit 1, 阻止 PR merge | yml 文件存在, 3 个 run step, trigger `pull_request`, 失败 exit 1 |
| 4.2 | `.github/workflows/doc_check.yml` | 现有 doc_check job **末尾**增加 step: `run: bash tools/verify_adr.sh --only=ADR-024` (快速 smoke, 不阻塞 PR) | 现有 job 末尾含 verify_adr.sh step, 不设 `continue-on-error: false` (失败不阻塞, 仅提示) |
| 4.3 | `tools/README.md` (新建, 如不存在) | 列出 3 个验证脚本的用途 + 退出码 + CI 集成位置 | 文件存在, 3 脚本均说明, 引用 `.github/workflows/architecture-gates.yml` 路径 |
| 4.4 | `docs/architecture/adr.md` 新增 ADR-041 | 标题: "CI 强制架构门禁 (3 验证脚本 + GitHub Actions 集成)"; 状态: ✅ Accepted; 决策: PR 必须通过 `architecture-gates.yml` 3 步验证; 理由: 防止未来 PR 引入 ADR/DRIFT/D4 漂移; 验证命令: `bash tools/{verify_adr,verify_plugin_decision,check_plugin_portability}.sh` 均 exit 0 | ADR-041 状态 Accepted, 含 3 脚本引用 + 集成 workflow 路径 |

**OpenSpec artifacts**:
- `openspec/changes/ci-architecture-gates-2026-06-12/proposal.md`
- `openspec/changes/ci-architecture-gates-2026-06-12/tasks.md` (4 tasks)
- `openspec/changes/ci-architecture-gates-2026-06-12/design.md` (CI 集成设计 + 失败处理策略)
- 无 `specs/*.md` (CI 配置不在 openspec/specs 范围)

**依赖**: 无 (独立 CI 工作)
**并行**: 可与 Change 1/2/3 并行
**回滚**: 删除新建的 `.github/workflows/architecture-gates.yml` + 还原 `.github/workflows/doc_check.yml` (或 `git revert <change-4 commit>`)

---

## 4. 整体执行顺序与并行性

```
Week 1:
  Day 1: openspec init (一次性, 项目级基础设施)
  Day 1-2 (并行):
    ├─ Change 1: fix-roadmap-doc-sync-2026-06-12 (P1, 1-2h)
    ├─ Change 2: fix-bundle-type-dual-track-2026-06-12 (P1, 1-2h)
    └─ Change 3: adr-040-internal-consistency-2026-06-12 (P2, 30min)
  Day 2-3 (并行或串行):
    └─ Change 4: ci-architecture-gates-2026-06-12 (P3, 2-3h)
```

**Momus loop 流程** (每个 change 严格按以下顺序, 不跳步):
1. **起草** `proposal.md` + `tasks.md` (适用 + `design.md` / `specs/*.md`)
2. **自检**: `openspec validate <change-name>` (Schema 合规性, 必过)
3. **Momus 审查**: `task(subagent_type="momus", prompt=".omo/drafts/<change-name>.md")` — 必须在 **apply 前** 通过 OKAY
4. **若 Momus 报 NO-GO** → 修复 (改 proposal/tasks/design) → 回到 step 2 (loop until OKAY)
5. **若 Momus 报 OKAY** → `/opsx:apply` 实施 (或人工编辑) → 跑验证命令 (§8) → `openspec archive <change-name>` 归档

**Momus 调用硬约束** (从 Prometheus 内部规范):
- `prompt` 参数**只传文件路径字符串**, 不附加解释
- 每次重审**新建 `task(subagent_type="momus", ...)`** 调用, 不复用 `task_id` (避免审查陈旧内容)

**总 Momus loop 预期**: 4 change × 平均 2 轮 = 8 轮 (最坏 16 轮, 取决于每个 change 的方案争议度)

---

## 5. 关键决策 (Key Decisions)

| 决策 | 推荐 | 理由 |
|------|------|------|
| Change 1 是否含决策草案状态行修改 | ✅ 含 (任务 1.4/1.5) | 审计 #5 显式标记为 P1 文档同步; 不修会留长期不一致 |
| Change 2 是否新建决策草案 | ✅ 新建 (任务 2.4) | 为 Phase 5/6 BundleMapper 实施提供 anchor; 但**仅草案**, 不实施 |
| Change 3 是否修改 §2.1 ADR-040 行 | ✅ 移除 (任务 3.1) | 实质"Phase 1 提案" ≠ "✅ 已实现", 删 §2.1 避免误导 |
| Change 4 是否新增 ADR-041 | ✅ 新增 (任务 4.4) | CI 集成是新架构决策, 需 ADR 跟踪 |
| Change 4 是否失败时 exit 1 阻止 merge | ✅ 是 | 高精度门禁, 防止未来 PR 引入 ADR 漂移 |
| 修复后是否回填 roadmap-status.md | ❌ 不在本次范围 | roadmap-status 跟踪文件, 修复完成后由下次 session 统一更新活动日志 |

---

## 6. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| R1: Momus loop 反复要求修改, 总耗时超 1 周 | 中 | 中 | 每个 change 范围小, 4 change 独立 archive, 单 change 失败不影响其他 |
| R2: openspec init 引入新工作流, 学习曲线 | 中 | 低 | openspec CLI 1.3.1 已装, init 仅一次; 4 change 模板可复用 |
| R3: Change 2 新建决策草案被 Momus 拒绝 | 中 | 中 | 草案格式参照 `decision-phase-1.3-bridge-2026-06-10.md`, 减少审查反复 |
| R4: Change 4 CI 配置在本地无法测试, 需 push 后验证 | 高 | 中 | PR dry-run: 推送到分支, 观察 CI run, 失败即修 |
| R5: 修复引入新文档漂移 (修改 interface-design.md 时误改其他章节) | 低 | 中 | 每个 change 单原子 commit; 实施时用 `git diff` 逐行核对 |
| R6: `verify_adr.sh` 报 ⚠️ STALE (Agent #2 发现脚本判定偏差) | 低 | 低 | 已知偏差, 不在本次范围; P2 单独修复 |

---

## 7. 成功标准 (Success Criteria)

- [ ] 4 个 OpenSpec change 全部 `openspec archive` 成功
- [ ] 4 个 change 每个经 Momus 至少 1 轮审查 (verdict=OKAY 才能 archive)
- [ ] 审计 #1-#5 5 处文档不一致全部修复 (每个有 git diff 证据):
  - #1 `phase-1-tlm-foundation.md` Status + 10 个任务勾选
  - #2 `ip/cache/README.md` banner 补 1.3d + §9.3 改写
  - #3 `overview.md` 快照日期 + 列举 3 个落地项
  - #4 (DRIFT-1) `interface-design.md` §1 新增"Bundle 形态演进"小节
  - #5 决策草案状态行同步 Accepted (1.4 + 1.5) + ADR-040 §2.1/§2.3 表内一致 (Change 3)
- [ ] 审计 DRIFT-1/2/3 3 处 DRIFT 全部文档化消解 (DRIFT-1 完全消; DRIFT-2 Change 2 interface-design.md §1 提及 ch_stream 协议转换推迟 + roadmap PA-6 已跟踪; DRIFT-3 Change 2 提及 "ImplMode 推迟 Phase 6" 引用 ADR-029)
- [ ] 14/14 ctest 仍 PASS (无回归)
- [ ] `verify_adr.sh --only=ADR-024` 仍 2/2 PASS (无回归)
- [ ] `verify_plugin_decision.sh` 仍 3/3 PASS (无回归)
- [ ] `check_plugin_portability.sh` 仍 4/4 PASS (无回归)
- [ ] Change 4 实施后, GitHub Actions PR 上 3 个 gate 全部 PASS
- [ ] 4 个 change 的 `archive` 后, `openspec list` 显示 0 个 active change (已清空)

---

## 8. 验证命令 (可重放)

```bash
# 1. 初始化 OpenSpec (一次性)
cd /workspace/project/ChipForge
openspec init .

# 2. 起草 Change 1
mkdir -p openspec/changes/fix-roadmap-doc-sync-2026-06-12
# 写 proposal.md + tasks.md
openspec validate fix-roadmap-doc-sync-2026-06-12

# 3. Momus 审查 (必须 verdict=OKAY 才能实施)
task(subagent_type="momus", prompt=".omo/drafts/fix-roadmap-doc-sync-2026-06-12.md")

# 4. 实施 (用 /opsx:apply 或人工)
# 5. Archive
openspec archive fix-roadmap-doc-sync-2026-06-12

# 6. 重复 Change 2/3/4

# 7. 整体验证
bash tools/run_chipforge_tests.sh  # 14/14 PASS
bash tools/verify_adr.sh --only=ADR-024  # 2/2 PASS
bash tools/verify_plugin_decision.sh  # 3/3 PASS
bash tools/check_plugin_portability.sh  # 4/4 PASS
openspec list  # 0 active
```

---

## 9. 范围与边界 (Scope Boundary)

**IN SCOPE**:
- 4 个 OpenSpec change 的完整生命周期 (proposal → tasks → design (适用) → Momus loop → apply → archive)
- 5 处文档不一致的 git diff 修复
- 3 处 DRIFT 的文档化消解
- CI 集成 (新建 `.github/workflows/architecture-gates.yml` + 改 `.github/workflows/doc_check.yml`)
- 新建 1 个决策草案 (`bundle-mapper-phase-5-6-decision.md`)
- 新建 1 个 ADR (ADR-041 CI 强制门禁)

**OUT OF SCOPE** (留给后续 session):
- 实施 PA-6 (Phase 1.3d-extras ch_stream 协议转换)
- 实施 PA-7 (Phase 1.4 baseline 对比)
- 修改 `bundles/mem_bundles.h` 字段类型
- 修改 `verify_adr.sh` 脚本判定逻辑 (修复 ⚠️ STALE 偏差)
- 修复 R6/R7 (ch_stream 协议转换风险 / baseline 选型)
- 修改 `verify_adr.sh` 引入 Phase 5/6 BundleMapper 主动检查 (新决策, 需独立 change)

---

## 10. 等待用户批准

请确认:

- [ ] 4 个 change 拆分粒度 (Change 1/2/3/4) 是否合理?
- [ ] Momus loop 审查深度 (每个 change OKAY 才能 archive) 是否可接受?
- [ ] 范围边界 (IN/OUT SCOPE) 是否正确?
- [ ] 关键决策 (§5) 是否同意?
- [ ] 成功标准 (§7) 是否完整?

**批准后**: 启动 `writing-plans` skill 写 4 个 change 的实施计划 (4 份 .omo/plans/fix-*.md),然后 `/opsx:apply` 实施。
