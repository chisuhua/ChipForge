# 归档前验收报告 —— 5 个已完成 change（wave5 parity + wave7 治理）

> **角色**：verifier（★ 只验不改 · ★ 本轮未改动任何实现代码）
> **日期**：2026-11-08
> **范围**：`parity-gate-spec` · `backend-parity-observability` · `backend-parity-ci` · `architecture-truth-boundary` · `architecture-truth-boundary-enforcement`
> **用途**：★ 这 5 个 change 的 tasks **全部 36/36 / 29/29 / 36/36 / 11/11 / 41/41 勾完**，但仍**滞留工作区**
> ⇒ ★ 它们让 `chmem-hierarchical-assembly` 的 `depends_on: backend-parity-ci` 指向一个「进行中」的 change
> ⇒ ★ 归档前做一次**本地验收**（★ `rdd-verifier` 分流条件②不满足 —— `.rddf/state/iteration.json` 不存在 ⇒ 走本地 1–5 步）

---

## 1. 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | 用户本次指令 | 「按你建议的顺序实施」+「@metis + @critic 审查上述实施路径建议通过后实施」+「继续推进」 |
| 2 | 各 change 的 `proposal.md` 的 `## Acceptance Criteria` 段 | ★ **AC 权威**（ADR-094 v0.2 起 = `proposal.md`，★ 非 `tasks.md`） |
| 3 | 各 change 的 `tasks.md` | 执行清单（★ 派生自 proposal 的 AC） |
| 4 | ADR-088 §5.2 判据 4/7 · ADR-089 V-1~V-6 · ADR-095 §5 V-101~V-110 · ADR-094 §Verification | 上游契约 |

---

## 2. 逐条判定 + 证据

### 2.1 验收 1 —— 静态检查

| 检查 | 结果 | 证据 |
|---|---|---|
| 门禁① 顶层纯净 lint | ✅ 0 命中 | `bash tools/check_arch_truth.sh` ⇒ 「门禁① 结果: 0 命中 ✓」 |
| 门禁④ 子目录引用非权威 | ✅ 0 命中 | 同上 ⇒ 「门禁④ 结果: 0 命中 ✓」 |
| 门禁①④ self-test | ✅ 11/11 | `bash tools/check_arch_truth.sh --self-test` |
| 门禁② doc_link | ✅ 0 broken | `bash tools/doc_link_check.sh` ⇒ 「All markdown cross-references valid (0 broken)」+「All backtick path references valid (0 broken)」 |
| 门禁③ ADR 孤儿 | ✅ 0 FAILED / 0 ORPHANS | `bash tools/verify_adr.sh` ⇒ 「All ✅ ADRs pass verification」 |
| D4 插件风格 | ✅ 8/8 | `bash tools/verify_plugin_decision.sh` |
| ADR-040 移植性 | ✅ 13/13 | `bash tools/check_plugin_portability.sh` |
| openspec 结构 | ✅ 5/5（本次范围） | `openspec validate <change> --type change` ⇒ 5 个全 `is valid` |
| AC↔V 机检 | ✅ 28/28 · 0 孤儿 · 0 未授权重复 | `python3 tools/check_ac_v_coverage.py`（★ 本轮扩到 L4 两 change） |

### 2.2 验收 2 —— 单元测试

| 套件 | 结果 | 证据 |
|---|---|---|
| 门禁单测 | ✅ **34 passed** | `python3 -m pytest tools/test_arch_truth_gates.py -q`（★ 24 → 34，★ 本轮 +3 侧面感知 + 修 3 个因 V 全集扩容而失配的 fixture） |
| parity 时长 + 回读 | ✅ 合计 58 passed | `pytest tools/test_arch_truth_gates.py tools/test_parity_timings.py tools/test_read_back_verdict.py -q` |
| parity 门禁自检 | ✅ 16/16 | `python3 tools/parity/parity_gate.py --self-test` |
| ★ S-12 provenance 自检 | ✅ 5/5 | `python3 tools/parity/check_exception_provenance.py --self-test`（★ 含负向对照 + merge commit 跳过用例） |
| ★ AC↔V 自检 | ✅ 8/8 | `python3 tools/check_ac_v_coverage.py --self-test`（★ 本轮 3/6 → 8/8） |

### 2.3 验收 3 —— 端到端（★ 真实 CI，非本机模拟）

| 项 | 结果 | 证据 |
|---|---|---|
| parity 门禁在 CI 执行 + 回读 | ✅ | CI run **42473424285**（`workflow_dispatch` · layer=pr · ref=main）⇒ **conclusion = success** · **非 success/skipped 的 step 数 = 0**（13 步全过） |
| artifact 回读 | ✅ | `parity-report-pr` ⇒ `verdict=PASS` · `error_kind=null` · `layer=pr` · `timings.count=8` · `p95=30.851s` · `jobs=4` · `commit=c10041a` |
| ★ CI 首次 run 的两次失败 | ✅ 已归因并修复 | ① deps 未构建（`No rule to make target libcpptlm_core.a`）② Verilator 静默未装（按发行版精确匹配分流 ⇒ 两分支都 skip）⇒ 均已修，第三次 run 全绿；★ 证据落 `openspec/changes/backend-parity-ci/ci-first-run-evidence.md` |

### 2.4 验收 4 —— 边界与错误路径（★ 每条都有负向对照）

| 场景 | 期望 | 实测 |
|---|---|---|
| S-12 白名单扩张 + message 无 ADR-089 | RED | ✅ 「扩张 + message 不含 ADR-089 ⇒ RED」 |
| ★ S-12 弱判定边界 | ★ 如实登记 | ✅ 「message 提到 ADR-089 即算引用」⇒ **辅助闸**，★ 非「architect 已审」的证明 ⇒ 已登记在脚本 docstring + 契约 §S-12 |
| 语料**选中项**缺失 | ERROR / exit 2 | ✅ `verdict=ERROR error_kind=corpus_missing_files` |
| 语料**未选中项**缺失 | WARNING（不改 verdict） | ✅ PR 层 = PASS + 4 条 WARNING（night 专属 branchbench ELF 缺失不再拖红 PR 层） |
| 顶层布局越界（新子目录） | RED | ✅ `RED-LAYOUT docs/architecture/notes [unexpected-dir]` |
| 围栏内假标题 `# 非目标态` | **MUST 仍报红**（不得静默开豁免） | ✅ `k_fence_fake_heading.md (findings=2/2)` |
| 「非目标态」但无日期 | 不豁免 | ✅ `m_nontarget_no_date.md (findings=1/1)` |
| 真阳性 | 5 处逐条报红 | ✅ `z_true_positive.md (findings=5/5)` |

### 2.5 验收 5 —— 遗留清扫（★ 本轮范围）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK | ✅ **0**（唯一命中 = `architecture-truth-boundary/tasks.md:62` 的清扫表格行「本 change 范围内 TODO / 半截实现 / 临时日志 \| 无」，★ **是登记不是残留**） |
| 注释掉的代码 / 临时调试日志 | ✅ 0（`print` 命中全为 CLI 正常输出） |
| 被 skip 的测试 | ✅ 0 |
| 未替换的 mock / 临时 fallback | ✅ 0 |
| ★ 临时产物清理 | ✅ 临时 worktree（5 批 × 2 次重放）已 `worktree remove`；★ 临时分支 `tmp/s12-negative` 已删；★ 负向探针注入的 `exceptions.json` 条目已随分支删除；★ `build/branchbench_m*.elf` 4 个已恢复 |

### 2.6 逐 change 的 AC 判定

| change | AC | tasks | 判定 | 依据 |
|---|---|---|---|---|
| `parity-gate-spec` | **7/7 全勾** | 32/32 | ✅ **PASS** | ★ AC-4 的字面要求「`reset_mode = none`」**已被实测推翻**（C-2 = ctor-init）⇒ 按**已裁决口径**判定，并在 proposal 内注明权威出处（`parity-gate-contract.md:614`） |
| `backend-parity-observability` | **6/6 全勾** | 29/29 | ✅ **PASS** | 4 条补勾的依据 = tasks 2.5 / 3.5 / 1.10 / 4.1（★ 均已勾且含实测数字） |
| `backend-parity-ci` | **5/5 全勾** | 36/36 | ✅ **PASS** | ★ 依据 = CI run 42473424285 的 step 7~13 + artifact 回读 + `run_chipforge_tests.sh` 的实测接线点（行号已落 proposal） |
| `architecture-truth-boundary` | **11/11 全勾** | 11/11 | ✅ **PASS** | ★ AC-103 用**可机检补交**闭合（worktree 重放 5 批父 commit）⇒ 产物 `evidence/ac103-reference-baseline.txt`：起点 **84** 条 ⇒ 峰值 **100** 条 ⇒ 迁移期间净增 **+16** ⇒ 「原有断链」与「迁移新增断链」已逐条区分 |
| `architecture-truth-boundary-enforcement` | **10/10 全勾** | 41/41 | ✅ **PASS** | ★ 4 道门禁（①④ ② ③）+ 顶层净化 + 覆盖台账，全部实测 0 命中 / 0 broken / 0 FAILED / 0 ORPHANS |

---

## 3. 失败项与路由建议

| # | 项 | 分类 | 路由 |
|---|---|---|---|
| 1 | ★ `mmu-chmem-pipeline-integration` 的 spec delta 缺 `ADR-049 contract preserved` scenario ⇒ `openspec validate --changes` **15/16** | ★ **pre-existing**（最后改动 `308cc08` / `fafadf7`，★ 与本轮无关） | ★ **不阻断本批归档**；★ 但它是**该 change 自身归档的硬 AC**（其 proposal AC-11 要求「`openspec validate` 0 error」）⇒ ★ 归 `mmu-chmem-pipeline-integration` 自己的 Phase E |
| 2 | ★ `doc_check.yml`（Documentation Health Check）自 2026-10 起**一直 failure**（`ip/cpu/configs/*.json` 的 `description` / `_comment` 不符合 JSON Schema） | ★ **pre-existing** | ★ 归 backlog（★ 与本轮无关；★ 10 月的旧 run 同样红） |
| 3 | ★ `parity-gate` 的 **`pull_request` 触发路径从未跑过**；`architecture-gates.yml` **全部从未跑过**（PR-only，含本轮新增的 S-12 step 与 `check_arch_truth` step） | ★ **零证据**（非失败） | ★ **建议开 draft PR 验证**（★ 用户此前已授权「按建议顺序实施」）⇒ ★ 见 §5 |
| 4 | ★ 夜间层（night）52 case 在 CI **不可跑**（`branchbench_elfs` 需 `riscv32-unknown-elf-g++`，CI 只提供 riscv64） | ★ **开放项 O-A4-1** | ★ 已登记于 `ci-first-run-evidence.md`；★ 夜间层 schedule run 会在门禁阶段 **ERROR**（★ 不会假绿） |

★ **无 `implementation_gap`**（无「实现缺失」）· ★ **无 `proposal_drift`**（无「实现与 AC 不符」）
★ ⇒ **本批 5 个 change 判定 PASS，可归档**。

---

## 4. 已用轮次

| 轮次 | 内容 |
|---|---|
| **第 1 轮** | ★ 本报告（本地验收 1–5 步） |

★ `rdd-verifier` skill **未启用** —— ★ 分流条件②不满足（`.rddf/state/iteration.json` 不存在 ⇒ P3 archive gate 未触发）
★ ⇒ 按 verifier 产出契约走**本地 1–5 步**，★ **未写** `.rddf/state/verifier/*`（那是 skill 的 owns）

---

## 5. 归档后的下一步（★ 已识别、待执行）

1. ★ **开 draft PR** ⇒ 一举验证两条**从未跑过**的 CI 路径：
   - `architecture-gates.yml`（PR-only，含 ★ 本轮新增的 `Check parity exception provenance` step 与 `Check architecture truth purity` step）
   - `parity-gate.yml` 的 **`pull_request`** 触发（★ 它的主要触发方式）
   ⇒ ★ 看完结果即关 PR + 删分支
2. ★ `bash .rddf/sync_strategy_status.sh` ⇒ 重派生 `strategy.md` §7（★ 当前表显示 `architecture-truth-boundary-enforcement` = TODO（实际 41/41）、`backend-parity-ci` 不在表里 ⇒ **漂移**）
3. ★ `feat-backend-parity-gate.md` 的 **G-A3** 勾选（★ 待 night 层重跑证据 ⇒ architect 签认）
4. ★ 夜间层 CI 可跑性（O-A4-1）⇒ 三选一：CI 装 riscv32 工具链 / branchbench ELF 改 vendored / 夜间层改本地 runner
