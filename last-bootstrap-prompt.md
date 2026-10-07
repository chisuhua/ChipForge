[延续 v0.10.0 wave5-isa-coverage-and-bp 会话 — bootstrap 2026-10-07 11:30]

项目根: `/workspace/project/ChipForge`
HEAD: `0229612` (chore(openspec): archive mfc-defer-v0.11.0) — ⚠️ DO NOT REUSE, regenerate each session

## Live state (from /v0100-bootstrap):

### Test status
- [cpu-l1-mmu-demo]: ✅ 6/6 PASS (40 assertions) — target: 6/6
- [riscv-tests]: 🟡 40/48 PASS (rv32um 8 fail, defer v0.11.0) — target: 40/40

### Recent commits (last 5)
```
0229612 chore(openspec): archive mfc-defer-v0.11.0 (Oracle D' defer 决策 ship)
0652499 docs(strategy): sync §7 with current openspec state (auto-derived, 2026-10-07)
cc2c25d docs(changelog): v0.10.x interim — mfc-defer-v0.11.0 entry (Oracle D' defer 决策)
5bd83d7 docs(agents): §已知测试状态 加 [riscv-tests] 40/48 诚实性 NOTE
c6a0275 docs(dse): rv32um-baseline-matrix — Phase E.5 partial advance evidence
```

### Workspace health
- working_tree: ✅ clean
- orphan_changes: ✅ 无 archive 候选

### Hard prerequisites (v0.10.0 launch gates; #2 软门禁 — per `openspec/changes/plugin-framework-cycle-precision/proposal.md §状态` 2026-09-29 Oracle 核实, 决策: 采纳 (b) 不阻塞 mfc Phase A)
| # | Item | Status |
|---|------|--------|
| 1 | [cpu-l1-mmu-demo] 6/6 (hard gate) | ✅ PASS |
| 2 | plugin-framework-cycle-precision 25/25 **(soft gate, Phase F optional)** | 🟡 soft (defer to v0.11.0/wave5-bp) |

### Active OpenSpec changes (9 active, 1 archived this session)
```
mfc-cpu-pipeline-multi-cycle-fsm     48/60 tasks  IN_PROGRESS (partial advance, defer v0.11.0)
mmufault-verilator-sv32-e2e-flip     0/36 tasks   TODO (Part b, deps cpu-pipeline-mmufault-handler ✓)
mfc-extract-fsm-h                    0/20 tasks   TODO (v0.11.0 follow-up placeholder)
plugin-framework-cycle-precision     0/25 tasks   TODO (soft gate, defer v0.11.0)
phase-1.5-wave-4                     0/51 tasks   TODO (wave4 placeholder)
verilator-l1cache-e2e-coverage       0/52 tasks   TODO (wave4 P2 placeholder)
mmu-config-json-driven               No tasks     TODO (待 proposal/tasks 起草)
cache-phase1.5-4way                  0/43 tasks   TODO (wave4 placeholder)
vexii-riscv-parity-poc               0/74 tasks   TODO (wave6 placeholder, v0.11.0+ 启动)
```

### Honesty audit (声称 vs 实测, K3 fix 2026-10-07 ✅ ALL PASS)
| 指标 | 声称来源 | 声称数字 | 实测 | 一致? |
|------|---------|---------|------|------|
| [cpu-l1-mmu-demo] | AGENTS.md §已知测试状态 | 6/6 PASS | 6/6 PASS | ✅ |
| [riscv-tests] | AGENTS.md §已知测试状态 | **40/48** PASS + rv32um 0/8 FAIL (defer v0.11.0) | 40/48 + 0/8 FAIL | ✅ |
| [mmu] | AGENTS.md §已知测试状态 | 53/53 PASS | (CHANGELOG v0.10.4 声明, 不再跑) | ✅ |
| [cpu-integration] | AGENTS.md §已知测试状态 | 81/81 PASS | (CHANGELOG v0.10.4 声明) | ✅ |
| doc_link_check | (无显式声称) | — | 0 broken (105 markdown files) | ✅ |

**Honesty audit 增补 NOTE (Metis + Oracle 审查, 2026-10-07)**:

- ⚠️ **`mmu_chmem.h` owner 真空** (新增, 流程教训): `cpu_factory_chmem.h:84` 源码级注释自证 *"sv32 translation 实现 owner 尚未分配"*, `cpu_verilator_sim.cpp:137` 明确 `--enable-mmu --mmu-mode sv32` 是 no-op stub. `phase-6d-fsm-chmem` archive 仅交付 PTW FSM PoC (`mmu_ptw_chmem.h` 314 LOC), 不含 CH_MEM pipeline 集成. 当前状态 = CH_MEM MMU 集成层 scope **无 change 持有**. 这与 v0.10.4 hotfix 同类流程教训 (文档/change 范围与代码状态不一致).
- ✅ **git push 状态修正**: 上版 claim "5 commits ahead of origin/main" 与实测不符, `git rev-list --count origin/main..HEAD = 0`, 5 commits **已同步到 origin/main**. 无 push 决策需求.

## 必读 (静态, 见 AGENTS.md):
- `AGENTS.md §路线 / Roadmap 类文档` — 文档导航入口（首次必读，含 4 类文档职责对照 + 记忆口诀）
- `soc/cpu/docs/roadmap/execution-roadmap.md §5` — 立即下一步
- `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/proposal.md` — partial advance + defer v0.11.0 (Why 段)

## 按需加载路由 (L3/L4/L6 条件触发)

按本会话意图匹配触发场景，决定加读哪些文件。**反例行（行 9）：无触发场景则不加读——"惰性"是默认**。

| # | 触发场景（关键词） | 加读文件（限 section） | 优先级 vs L1/L2/L5 | 与默认"不读"的区别 |
|---|---|---|---|---|
| 1 | PoC-1 / MUL/DIV / multi-cycle / FSM / negotiate 实装 / mfc-extract-fsm | `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md §3` + `docs/architecture/adr/ADR-082-plugin-negotiate-capability.md` §Context+Decision + `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-1 行 | **高于 L5** | 不读 → negotiate 设计契约靠猜 |
| 2 | RV32C / 解码 / 跨页 fetch | `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-2 行 + `ADR-070`（若已起草） | 与 L5 同级 | 不读 → 成功标准（rv32uc ≥95%）缺失 |
| 3 | ICache / fence.i / 取指一致性 | `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-3 行 + `ADR-072`（若已起草） | 与 L5 同级 | 不读 → fence.i ≥10 断言要求漏 |
| 4 | BTB / GShare / 分支预测 / mispredict / wave5-bp | `soc/cpu/docs/roadmap/references/decision-1-plugin-evolution.md §1` + `soc/cpu/docs/roadmap/references/poics-and-risks.md §1` PoC-6 行 | 与 L5 同级 | 不读 → 漏 Port 抽象纪律（CI 第 11 条） |
| 5 | ADR 起草/状态变更/编号 | `soc/cpu/docs/roadmap/references/adr-matrix.md §1+§2+§7` + `docs/architecture/adr.md` 注册表 | **最高**（先于一切写操作）| 不读 → 撞号（050/051 事故重演） |
| 6 | CH_MEM / 双模 / elaborate / ch_state_machine / 对拍 / `mul_div_fsm.h` ch_reg 实例化 WARN | `docs/lessons/phase-6c-elaboration-substrate.md` 陷阱清单节 + `ADR-040 v2.0` Decision 节 | 与 L2 同级 | 不读 → 15 类已知陷阱逐个重踩 |
| 7 | 对外汇报 / 性能对标 / 决策 3 / chip-selector 定位 / DMIPS 0.0351 报告 | `soc/cpu/docs/roadmap/references/multi-core-comparison.md §1+§2` | 低于 L5（仅此类会话需要）| 不读无执行损失，仅汇报失真 |
| 8 | 追溯 mfc partial advance 历史（如 722eca6 / 76ac53f / c4035e5 / 9e83222 性质） | `CHANGELOG.md` v0.10.3 + v0.10.x mfc-defer-v0.11.0 interim entry（**不读全文**） | L6 叙事层 | 不读无执行损失，仅缺历史叙事 |
| 9 | **以上均不涉及** | **零加读**——仅 L1 §3 / L2 §5 / L5 proposal + adr.md | — | **显式默认"惰性 opt-in"**，防清单广播 |

### 链路纪律（ADR 引用追踪）

- **depth ≤ 2**：单条 ADR 只读 `## Context` + `## Decision`（约 40% 篇幅）；`Consequences` / `Risks` 仅在做回退决策时读
- **路径不可达即停**：ADR-070~081 等规划中编号尚未起草，agent 沿链接走到不存在文件时应停止，**禁止脑补**（"未起草" ≠ "内容为空"）
- **升级触发（客观）**：ADR 总数 >15 或首次观测到链读事故时，本节升级为 P0 硬门禁

## 不要做 (静态, 见 AGENTS.md):
- ❌ backout commit `8a14402` (ADR-049 根基)
- ❌ backout commits `722eca6` / `76ac53f` / `c4035e5` / `9e83222` / `0229612` (mfc partial advance + defer 决策证据链)
- ❌ 新建与 `mfc-cpu-pipeline-multi-cycle-fsm` 并行的 multi-cycle change
- ❌ archive `mfc-cpu-pipeline-multi-cycle-fsm` (维持 partial advance 证据, 等 v0.11.0 重启)
- ❌ 触碰已 archive 的 `wave3-*` initiative + `2026-10-07-mfc-defer-v0.11.0`
- ❌ 在 `cpu-pipeline-multi-cycle` 上加任务（已 superseded-by mfc）
- ❌ implement `vexii-riscv-parity-poc` (wave6 placeholder, v0.11.0+ 启动)
- ❌ 不以 `.omo/drafts/` 内容为决策依据（历史草案，已被 ADR/OpenSpec 吸收或废弃，反 SSOT）
- ❌ 修改 AGENTS.md §已知测试状态 / CHANGELOG.md v0.10.x interim 数字（K3 fix 已 ship, 仅在实测变化时同步）

**不要做 (Metis + Oracle 审查新增, 2026-10-07)**:
- ❌ **不把 Phase 6d.6 (`mmu_chmem.h`) 实装塞进 `mmufault-verilator-sv32-e2e-flip`** — 该 change Out of Scope 已声明 "MMU/PTW FSM 实装 → 由 Phase 6d.6 独立 change 覆盖". 扩容会污染 acceptance criteria, 且该 change 36 tasks 估算基于"翻译语义已存在"的错误假设. 必须 scope 隔离.
- ❌ **release notes 不显式标注 rv32um 0/8 FAIL + DMIPS 0.0351** — 与 AGENTS.md §honesty_audit K3 修复冲突, 是诚实性硬要求 (即使 rv32um 已记录在 AGENTS.md / CHANGELOG.md, release tag 的语义化版本注释也必须再写一遍).
- ❌ **跳过 release tag v0.10.0 release notes 决策** — K3 修复漂移风险, 必须用户决策 tag 语义 (launch / interim / 不打).
- ❌ **archive `mfc-extract-fsm-h`** — 该 change 阻塞 (depends_on mfc Phase E+G+H 全完成, mfc 已 defer 48/60), 维持 active 占位等 v0.11.0 重启.
- ❌ **新建 `mfc-extract-fsm-restore`** — 与现有 `mfc-extract-fsm-h` 命名混淆 + 同阻塞依赖, 仅在 v0.11.0 launch 期间由 `mfc-defer-v0.11.0 §3.1` 触发创建, v0.10.0 期内不要建.
- ❌ **implement `mmu_chmem.h` 集成层在 v0.10.0 期内** — 该 scope 估时 1-3 周 (vs proposal 自述 <400 LOC 严重低估), 必须 defer v0.11.0 与 `wave5-bp-btb` 同窗口规划.

## Smart recommendations (Metis + Oracle 双审查修订, 2026-10-07):

✅ **v0.10.0 启动硬前置全过** + **mfc 已 partial advance + defer v0.11.0** + **诚实性 K3 fix ✅** + **git push 已完成 (0 commits ahead of origin/main)**

> ⚠️ **修订说明**: 上版 (11:30 auto-inferred) 有 2 处前提性事实错误, 已修订:
> - **Rec 1**: 误判 "deps ✓", 实际 `mmufault-verilator-sv32-e2e-flip` 还依赖 Phase 6d.6 (`mmu_chmem.h` 集成层), 该 change 不存在 (`cpu_factory_chmem.h:84` 自证 "owner 尚未分配"), 启动即撞 Spike 1.2/1.3 FAIL. **拆分为 1a (TLM v0.10.x ship) + 1b (mmu-chmem-pipeline-integration defer v0.11.0)**.
> - **Rec 2**: 误判 "5 commits ahead", 实测 `git rev-list --count origin/main..HEAD = 0` (已同步). git push 决策已不存在, 仅剩 **release tag + 诚实 release notes** 决策.

📋 **推荐下一步 (修订版)**:

1. **🟢 v0.10.0 final launch 收尾 (Quick, 0.5-1h)**
   - **git push 不需要**: 5 commits 已在 origin/main
   - **决策点**: release tag `v0.10.0` 打还是不打? tag 语义 (launch vs interim vs 不打)?
   - **必须**显式 release notes 标注: rv32um 0/8 FAIL (defer v0.11.0) + DMIPS 0.0351 vs hard gate 1.4 (Phase G defer, mfc-defer-v0.11.0 §v0.11.0 推迟 scope)
   - 用户决策点 (Metis + Oracle 共识): 详见 §关键问题 1

2. **🟢 mmufault-verilator-sv32-e2e-flip Part a (TLM flip, Short 1-4h ~ 1d, v0.10.x ship)**
   - Scope 拆分: sv32 PTE ELF vendor + **TEST_CASE 7 (TLM `cfg.enable_mmu=true` flip)** + AGENTS.md workaround 行更新
   - TLM 前置全 archive (Part a v1 + cpu-factory-satp-mapping v0.10.2 + v0.10.4 hotfix), 无缺口
   - **不包含 TEST_CASE 6 (Verilator sv32)** — 那个阻塞于 mmu_chmem.h 集成层, 见 #3

3. **🔴 mmu-chmem-pipeline-integration (Medium, 1-3 周, defer v0.11.0)**
   - **新建独立 change** (建议命名) 显式认领 `mmu_chmem.h` owner 真空
   - Scope: CH_MEM MMUPlugin 集成 (TLB+PTW 接入 5-stage CH_MEM pipeline fetch/memory 路径) + factory 接线 + CH_MEM EXCEPTION_CODE 生产侧契约 + Verilator TEST_CASE 6 + sv32 baseline CSV
   - **与 `wave5-bp-btb` 同窗口规划** (共享 fetch stage, 避免 fetch path ownership 冲突)
   - 用户决策点 (Oracle 推荐): 不要扩 `mmufault-verilator-sv32-e2e-flip` scope (会污染 acceptance criteria), 新建独立 change

4. **🟢 v0.11.0 prep 占位 (Quick, 10 min)**
   - `mfc-extract-fsm-h` 已存在 (0/20 tasks, 占位), **勿重复创建** `mfc-extract-fsm-restore` (命名冲突 + mfc Phase E+G+H 未完成, 该 change 也阻塞)
   - 补 `wave5-bp-btb` 占位 change
   - 补 `mmu-chmem-pipeline-integration` 占位 change (与 #3 同步)

5. **⚪ wave4 launch (Later, 等 v0.10.0 收尾)**
   - `cache-phase1.5-4way` + `verilator-l1cache-e2e-coverage` + `phase-1.5-wave-4`
   - L1Cache CH_MEM 同样未实装 (`--enable-cache` fail-fast throws), 与 #3 同类缺口, 不与 MMU 工作争抢 CH_MEM 调试带宽

> 注: plugin-framework-cycle-precision 0/25 soft gate 不阻塞主路径 (per 2026-09-29 决策)

## 启动清单 (新会话必做的前 6 步):
1. `openspec list` — 确认 9 active changes (mfc-defer-v0.11.0 已 archived this session)
2. `cat soc/cpu/docs/roadmap/execution-roadmap.md | sed -n '/## 5\./,/## 6\./p'` — 确认立即下一步
3. `cat openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md | grep -E "^## Phase |^### Phase "` — 列出所有 phase 头, 确认当前 phase (A-G+H, 48/60 done)
4. `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` — 确认回归状态（target 6/6）
5. **CI 门禁快验**（PR 阻塞门禁, 任一 FAIL 必须先修/报告才能继续）:
   ```bash
   bash tools/verify_adr.sh                  # ADR 注册表与代码实现对齐
   bash tools/verify_plugin_decision.sh      # D4 Plugin-style 业务代码静态检查
   bash tools/check_plugin_portability.sh    # ADR-040 移植性约束
   ```
   - 三脚本任一 ❌ → **立即报告 + 停止**; 不要绕过门禁继续 implement
   - `tools/doc_link_check.sh` 不在本步（虽推荐但非 PR 阻塞; 仅当步骤 6 报告 doc 链接异常时单独跑）
6. 报告你看到的状态 + 建议下一步

---

## 修订 metadata (本 prompt vs 上次 rev)

### rev 1 (11:30, 初版)
- **修订要点 6 处**:
  1. HEAD 9e83222 → 0229612 (5 commits ship: 4 doc + 1 archive)
  2. active changes 10 → 9 (mfc-defer-v0.11.0 archived this session)
  3. workspace_health: Case D (dirty) → ✅ clean
  4. honesty audit: Case E (失配) → ✅ ALL PASS (K3 fix 完成)
  5. smart recs: "启动 mfc Phase A" → **"mmufault-verilator-sv32-e2e-flip"** (mfc 已 partial ship)
  6. "不要做" 列表 +3 条 (archive 防回退 + vexii 不 implement + AGENTS.md 数字不动)
- **来源**: 2026-10-07 11:30 bash tools/v0100-bootstrap.sh 输出

### rev 2 (本版, ~12:00, Metis + Oracle 双审查修订)
- **修订要点 4 处** (基于 Metis + Oracle 共识):
  1. **Smart recommendations 段重写**: Rec 1 拆分为 1a (TLM flip, v0.10.x ship) + 1b (`mmu-chmem-pipeline-integration`, defer v0.11.0); Rec 2 前提修正 ("5 commits ahead" → "git push 已完成 0 ahead, 仅剩 release tag + 诚实 release notes 决策"); Rec 4 修正 (删除 "mfc-extract-fsm-restore" 命名混淆, 改为补 `wave5-bp-btb` + `mmu-chmem-pipeline-integration` 占位)
  2. **"不要做" 列表增补 6 条** (Metis + Oracle 共识): scope 隔离 (mmu_chmem.h 不塞进 mmufault change) + release notes 诚实性 (rv32um 0/8 + DMIPS 0.0351 必标) + release tag 决策不能跳过 + `mfc-extract-fsm-h` 不 archive + `mfc-extract-fsm-restore` 不在 v0.10.0 期内建 + `mmu_chmem.h` 集成层不在 v0.10.0 实装
  3. **Honesty audit 段增补 2 NOTE**: `mmu_chmem.h` owner 真空 (与 v0.10.4 hotfix 同类流程教训, `cpu_factory_chmem.h:84` + `cpu_verilator_sim.cpp:137` 自证) + git push 状态修正 (`git rev-list --count origin/main..HEAD = 0` 已同步)
  4. **修订 metadata 段增补**: rev 1 + rev 2 双段并存 (审计可追溯)
- **关键发现摘要** (来自 Metis + Oracle 共识):
  - **共识 1**: "5 commits ahead" 误判 (Rec 2 前提已失效)
  - **共识 2**: "deps ✓" 严重误导 (Rec 1 实际阻塞于 `mmu_chmem.h` 集成层, `phase-6d.6-mmu-ptw-fsm` change 名 dangling reference)
  - **共识 3**: Rec 1 应拆分为 1a (TLM v0.10.x) + 1b (Verilator defer v0.11.0)
  - **共识 4**: Rec 4 `mfc-extract-fsm-restore` 命名混淆, 现有 `mfc-extract-fsm-h` 也阻塞
- **来源**: Metis (bg_b3697d37, 6m 14s) + Oracle (bg_a7e10a20, 2m 53s) 双审查 (2026-10-07 12:00 完成)
- **日期**: 2026-10-07
- **待用户决策**:
  1. release tag v0.10.0 是 launch / interim / 不打?
  2. 接受 Rec 1 拆分 (TLM v0.10.x + Verilator defer v0.11.0)?
  3. `mmu_chmem.h` owner: 新建独立 change vs 扩 mmufault-verilator-sv32-e2e-flip scope?
