## Context

**当前状态**:
- `ip/cpu/docs/dse_architecture.md` v1.0 (2026-06-17, 🟡 Accepted) §9 列出 Phase A+B 共 11 个 M4-DSE 任务，是事实上的实施路线图
- `ip/cpu/docs/dse_architecture_v2_locks.md` v2.0-locks (2026-06-17, 🟢 Accepted for Phase 1, Oracle 评审通过) §6.3 列出"从设计研究文档删除的内容"——明确否决 3 类决策 + 推迟 4 章节到 Phase 5+
- 两文档**同一天诞生、同一天评审、同一项目**，但 §9 任务清单未与 §6.3 同步

**冲突清单** (11 任务逐条对照 v2.0-locks §6.3):

| v1.0 任务 | v1.0 内容 | v2.0-locks 结论 | 状态 |
|---|---|---|---|
| A.1 | `PluginBase::setup_with_config(pb, const void*)` 加 3 行 | D.7 推迟 Phase 5 | ❌ 删除 |
| A.2 | `PipeBuilder::merge_stage(name, parent)` 新增方法 | 超出 D.1-D.4 范围 | 🟡 需重设计 |
| A.3 | `CpuFactory::build_cpu` 替换 stub,真实 register 11 个 plugin | E.6 显式允许（"M4-DSE 将填充"） | ✅ 允许 |
| A.4 | 修复 `reg_file.cpp:40` 双重定义 bug | §1.2 隐含未禁止（Bug 修复） | ✅ 允许 |
| A.5 | `parse_config(json_text)` + `validate_config(cfg)` | D.1-D.4 不涉及 | ✅ 允许 |
| A.6 | test_cpu_factory 升级 (断言 plugin_count / stage_names / 拓扑) | 文档未禁止 | ✅ 允许 |
| B.1 | `BranchPredictorPlugin` 模板参数化 + 10 个显式实例化 | D.2 已完成默认参数化；10 显式实例化属 sweep 范围 | 🟡 部分允许 |
| B.2 | `BranchPredictorPlugin::create(cfg)` 工厂方法 | §6.3 显式删除 BranchPredictorFactory | ❌ 删除 |
| B.3 | `HazardPlugin` `setup_with_config` 接受 `use_strict_scoreboard` | D.7 推迟 Phase 5 | ❌ 删除 |
| B.4 | `IBusPlugin` / `DBusPlugin` 接受 `icache/dcache_latency` | §1.3 #6 未列入 Phase 1 | 🟡 需重设计 |
| B.5 | 测试: `btb_entries = 16/64/256` 跑出不同 BTB 命中分布 | 文档未禁止（测试驱动） | ✅ 允许 |

**统计**: 11 任务 → ✅ 5 允许 / 🟡 3 需重设计 / ❌ 3 删除

**约束**:
- 0 行代码改动（纯文档对齐）
- 不修改 `dse_architecture_v2_locks.md` §1-§6（Oracle 评审通过的冻结区）
- 不修改 `dse_architecture.md` v1.0 §1-§8（设计意图不变）
- 不得引入新 ADR（裁剪表本身不是决策）
- 完成后 `tools/verify_adr.sh` 仍 PASS, `ctest` 36/36 不退化

**利益相关方**:
- M4-DSE 实施者：需要 v1.0 §9 + v2.0-locks 双重指引，避免违反 Oracle 评审
- 评审者：需要单点权威裁剪源
- 项目管理者：status.md §4.1 的 M4.12-M4.19 任务状态需要正确的"前置"标记

## Goals / Non-Goals

**Goals:**
- 在 `dse_architecture_v2_locks.md` 新增 §7 "§9 任务与 Oracle 评审对齐表"，包含 11 任务逐条裁剪（✅/🟡/❌ + 理由）
- 在 `dse_architecture.md` §9 Phase A 段头加 ⚠️ 提示，指向上面 §7 表
- 在 `dse_architecture.md` §9 Phase B 段头加 ⚠️ 提示，指向上面 §7 表
- 在 `status.md` §4.1 顶部加 1 行 link，引用 v2.0-locks §7 作为 M4-DSE 硬前置
- 0 行代码改动，36/36 ctest 保持绿

**Non-Goals:**
- 不修改 `dse_architecture_v2_locks.md` §1-§6（Oracle 评审通过的冻结区）
- 不修改 `dse_architecture.md` v1.0 §1-§8 的设计内容
- 不解决 v1.0 §9 任务本身的执行（仅做"哪些可执行、哪些需重设计、哪些删除"裁剪）
- 不为被 ❌ 删除的任务提供替代方案（替代方案在 v2.0-locks §6.3 已说明）
- 不修改 `dse_architecture_v2_design_research.md`（Phase 5+ 参考，不在 Phase 1 范围）
- 不创建新 OpenSpec capability（这是元 change）

## Decisions

### Decision 1: 裁剪表放 v2.0-locks §7，不放 v1.0 §9 表格

**选择**: 在 `dse_architecture_v2_locks.md` §6 (附录) 之后新增 §7 章节，承载 11 任务对齐表。

**理由**:
- v2.0-locks 是"决策文档"，权威性高于 v1.0（v1.0 是"实施路线图"）
- v1.0 §9 表格格式是"任务/文件/验收"，加状态列会污染
- v2.0-locks §6.3 已建立"删除/推迟"列表格式，§7 自然延续

**替代方案**:
- 在 v1.0 §9 表格加状态列：污染表格格式，破坏 1.0 文档可读性
- 单独建 `dse_architecture_v2_3_reconciliation.md` 新文档：碎片化，违反"决策入口单点"原则

### Decision 2: v1.0 §9 不删，仅加 ⚠️ 提示

**选择**: `dse_architecture.md` §9 Phase A 和 Phase B 段开头各加 1 段 ⚠️ 提示（~5 行/段），指向上面 §7 表。**不删除任何任务行**。

**理由**:
- v1.0 §9 仍含 5 个 ✅ 任务和 3 个 🟡 任务的有效实施细节（文件路径、估时、验收）
- 全删会丢失有价值信息
- 状态由读者对照 §7 表读取
- "v1.0 §9 表格 = 实施参考" + "v2.0-locks §7 = 裁剪权威源" = 双指针

**替代方案**:
- 全删 v1.0 §9 任务行：丢失 5+3=8 任务的有用信息
- 在 v1.0 §9 表格每行加 ✅/🟡/❌ 列：状态会随 Oracle 评审演化，需频繁更新两份文档

### Decision 3: status.md §4.1 加 link 而非重新分类

**选择**: `ip/cpu/docs/status.md` §4.1 顶部加 1 行 "前置: v2.0-locks §7 裁剪表"，M4.12-M4.19 任务状态保留 🔵 待启动。

**理由**:
- 本 change 不实际解决 v1.0 §9 与 status.md §4.1 的任务映射（那是下一个 change 的事）
- 本 change 只确保"读 status.md 的人能看到前置条件"
- 避免 status.md §4.1 状态误标（M4.12 任务名与 v1.0 A.1 不同，但内容重叠）

**替代方案**:
- 在 status.md §4.1 把 M4.12-M4.19 任务按 v1.0 §9 → v2.0-locks §7 重命名：超出本 change 范围
- 删除 status.md §4.1 的 8 个 🔵 任务：丢失计划性，下一个 change 无起点

### Decision 4: 创建狭义新 capability `dse-scope-alignment-v2-locks`(非泛化政策)

**选择**: 本 change 在 `openspec/specs/dse-scope-alignment-v2-locks/` 创建**狭义** capability，专门锁定本 Oracle 评审(2026-06-17)对 v1.0 §9 任务的裁剪契约；不创建泛化政策 capability。

**理由**:
- 文档裁剪表本身是一个**可验证的契约**(3 个 Requirement + 6 个 Scenario)，值得独立 capability 记录
- capability 名锁定**具体评审范围**(`v2-locks` + `Oracle 2026-06-17`),避免"漂移到任何未来评审"
- 现有 7 个 capability(4 旧 + 3 M4G 新)职责分明,本 capability 不与 `arch-doc-consistency-baseline` 冲突(后者管 ghost class + 配置残留,前者管任务级 Oracle 评审对齐)
- M4-DSE 真正实施时,新 `m4-dse-*` capability 引用本 capability 作为前置,形成依赖链

**替代方案**:
- **不创建任何 capability** (原稿): 与实际 artifacts(已有 `specs/dse-scope-alignment-v2-locks/spec.md`)矛盾,已否决
- 创建 `dse-reconciliation-policy` 泛化 capability: 名字暗示"通用政策",实际只覆盖本评审;会误导未来读者假设其他 Oracle 评审也走此契约
- 在 `arch-doc-consistency-baseline` 加 scenario: 该 capability 管 ghost class + 配置残留,与任务级裁剪职责正交;强行合并会让该 capability 失去焦点

## Risks / Trade-offs

### Risk 1: 裁剪表本身可能误判 v1.0 任务意图
- **缓解**: 表格的每行 ✅/🟡/❌ 都附 1-2 句引用 v2.0-locks §6.3 原文，便于读者复核；如有偏差，读者可走 ADR 流程修订
- **概率**: 低（11 任务与 §6.3 一一对应，结构清晰）

### Risk 2: 实施者忽略 ⚠️ 提示，直接按 v1.0 §9 开工
- **缓解**: v1.0 §9 段头 ⚠️ 提示用粗体 + 感叹号开头；v2.0-locks §7 表格加 "权威裁剪源" 说明；status.md §4.1 顶部加 link
- **概率**: 中（文档对齐依赖人类遵守约定）
- **回退**: 若未来有违反案例，开 ADR 强制要求所有 v1.0 §9 实施前必须引用 §7 表

### Risk 3: v2.0-locks §7 与 §6.3 后续演化分裂
- **缓解**: §7 表格每行标注"§6.3 第 N 条"，让读者能溯源；未来若 §6.3 修订，同步更新 §7
- **概率**: 中（Oracle 评审是冻结点，但 Phase 5+ 准备时可能重新评估）
- **回退**: §6.3 修订时同时检查 §7 表格，必要时重开本 change

### Risk 4: 文档大小膨胀
- **缓解**: v2.0-locks §7 表格控制在 ~50 行；v1.0 §9 提示每段 ~5 行；status.md +2 行
- **影响**: 文档总增长 ~60 行 markdown，可忽略

## Migration Plan

无代码改动，无需 migration / rollback：

1. **本 change 实施**（30 分钟）:
   - 编辑 `dse_architecture_v2_locks.md` 加 §7 章节
   - 编辑 `dse_architecture.md` §9 Phase A/B 段头加 ⚠️
   - 编辑 `status.md` §4.1 顶部加 link
   - 验证：`git grep "v2.0-locks §7" dse_architecture.md status.md` ≥ 3
   - 验证：`grep -c "✅\|🟡\|❌" dse_architecture_v2_locks.md` 增量 = 11+（11 任务每行至少 1 状态符号）
   - 验证：`ctest` 36/36 仍 PASS
2. **commit 1**：`docs(dse): reconcile dse_architecture.md §9 with v2.0-locks §6.3`（3 文件改动，~60 行）
3. **归档本 change**
4. **下一个 change**（M4-DSE 真正实施）按 §7 表格的 ✅ 任务开工，🟡 任务需先重设计 ADR，❌ 任务不实施

**回滚策略**: 纯 git revert 即可，0 行为影响。

## Open Questions

1. **v1.0 §9 Phase A 表格的"1 周"估时是否需要更新？** — 当前决策：保留原估时，§7 表只标 ✅/🟡/❌ 不重新估时；若后续发现 🟡 任务估时偏差，单独开 ADR。
2. **status.md §4.1 的 8 任务编号（M4.12-M4.19）是否需要按 v1.0 §9 重命名？** — 当前决策：本 change 不动，下一个 change 实施时按 §7 表的 ✅ 任务重新编号。
3. **dse_architecture_v2_design_research.md 是否也需要加 ⚠️ 提示？** — 当前决策：暂不。该文档明确为"Phase 5+ 参考"（README.md 第 4 行），不是 Phase 1 实施起点，不构成误导风险。
