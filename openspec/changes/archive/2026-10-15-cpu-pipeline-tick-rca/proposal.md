---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0
depends_on:
  - cpu-sim-add-elf-tohost-fix
---

# cpu-pipeline-tick-rca — 诊断 cpu_sim CLI 路径 pipeline 不 tick 根因

> **⚠️ Naming Caveat (2026-10-15 P0 重做, Oracle + Metis + Critic 三方复审)**:
> 标题 "cpu-pipeline-tick-rca" 与早期叙事 "pipeline 不能 tick" 是**事实性错误**。`pb->run()` 在持续推进 (证据: `ibus.h:103-113` writeback PC 无条件 +4, 循环永远不停); **真实症状**是 fetch stage 永远 stall (CtrlLink `halt_when(PTW_ACTIVE)` 持续 true), 导致 decode/memory 阶段从未被调度, payload 全 0、tohost 永不写。诊断目标是 "**fetch 永 stall** 的根因", 不是 "pipeline 不能 tick"。原因: 本 change 已被 commit `5403718` 用 `chore(openspec): cpu-pipeline-tick-rca 诊断 change` 命名前归档, 目录改名需追改 ≥8 处外部引用 (AGENTS.md / CHANGELOG / `mmu-chmem-pipeline-integration` tasks:237 / `csr-chmem-writeback-complete` tasks:614,668 / `openspec/specs/cpu-sim-elf-base-tohost-override/spec.md:94-105` 等), 而结论未变。按**最小破坏面**原则保留目录名, 仅在 artifact 文本与外部 caveat 中澄清事实。
>
> **Pre-existing FAIL 范围** (继承自 `openspec/changes/cpu-sim-add-elf-tohost-fix/`):
> - `test_cpu_sim_real_tohost` (在 `[cpu]`)
> - 4 个 `test_{3,5,7,10}stage_riscv::end_to_end` (在 `[cpu-integration]`)
> - 实测对比: C++ API 路径 `test_rv32ui_runner.cpp` 跑同 `rv32ui-p-add` ELF **48/48 PASS** (证明 `pb->run()` + payload 传播机制本身可达; 问题在 CLI 路径输入侧)
> - 同 ELF 同 `enable_mmu=true` 在 C++ API 路径下是否也 FAIL 仍是**未验证对照** (见 §Why F4 + §OQ4), 本 change §1 control experiment 必须补跑
>
> **本 change scope**: **纯诊断**, 不动 product code 修复. 产出诊断报告 (复合根因段落 + 5-测试独立基线台账 + 2×2 control experiment 矩阵 + 修复建议方向). 后续 `cpu-pipeline-tick-fix` change 才会真正修复.

## Why

**复合根因 (三层, Metis + Oracle + Critic 三方 2026-10-15 一致识别, P0 重做)**:

证据链 (3 个 code facts + 1 个待验证对照):

- **F1 — config 保真度缺陷**: `tools/cpu_sim/main.cpp:83-113` `load_config()` 只解析 10 个 JSON 字段 (`name/isa/pipeline_stages/mul_latency/mul_impl/dispatch_width/n_lanes/icache_latency/dcache_latency/n_threads`), 静默忽略 **`enable_mmu / mmu_mode / satp_ppn / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / OoO 宽度 / ghr_bits / ext_*`**.
- **F2 — 测试 config 意图缺陷**: 4/5 FAIL 测试 config 显式 `enable_mmu: true` (`tests/cpu/configs/cpu_default.json:9` 5-stage; `cpu_superscalar.json:16` 7-stage; `cpu_deep_pipeline.json:18` 10-stage), 1/5 (`cpu_embedded.json:9` 3-stage) 写 `false` 但因 F1 被静默忽略。意图上, 3/5/7/10-stage `*_add_elf_end_to_end` 测试目标只是验证 pipeline 拓扑跑 `add` 指令, **本就是裸机烟测**, 根本不该开 MMU。spec 背书见 `openspec/specs/riscv-tests-fixture/spec.md:74-80` "Fixture SHALL use `enable_mmu=false` … Wave 1 runs riscv-tests in M-mode bare-metal".
- **F3 — MMU 瞬态语义 + v0.10.4 Bare-shortcut 严格化放大**: `enable_mmu=true` + satp 未编程 → `ip/cpu/cpu_factory.h:444` `make_satp_value(Sv39, 0)` 自动编码 `8<<60` → `ip/mmu/tlm/MMUPlugin.cpp:109` Bare shortcut 严格判定 (MODE=8 ≠ Bare=0, 不走 identity translation) → `do_lookup` 走真翻译 → TLB miss → `PTW_ACTIVE=1` → `ip/cpu/plugins/ibus.h:62-67` fetch CtrlLink `halt_when` 持续 true → **fetch stage 永 stall**, decode/memory 节点 payload 全 0; writeback 阶段不受 fetch stall 影响, PC 仍 +4 推进, 与 "`pb->run()` 在跑" 的表象矛盾.
- **F4 — 反向对照 (待验证)**: 上面三层解释了 CLI 路径 FAIL, 但若同 ELF + `enable_mmu=true` 在 C++ API 路径 (`test_rv32ui_runner.cpp`) 也跑, 是 PASS 还是 FAIL? 若 FAIL 则三层复合根因 confirmed; 若 PASS 则**还需追加第 4 层 (CLI/API 路径差异 × enable_mmu 交互)**, 调查线 L2 (`ibus.h` halt_when lambda 与 register 顺序的边界条件) 升为主要嫌疑。**本 change §1 control experiment 必须先回答 F4**。

**对照基线**:

1. **C++ API 路径 48/48 PASS** (`test_rv32ui_runner.cpp`):
   - 直接调 `CpuFactory<uint32_t>::build_cpu(cfg, &mem)`, `cfg.enable_mmu = false` 显式
   - `pb->run()` 循环跑通

2. **cpu_sim CLI 路径 100% FAIL** (`tools/cpu_sim/main.cpp`):
   - `cfg = load_config(config_path)` (F1 静默丢 enable_mmu)
   - `cfg.isa = "rv32i"` 显式 pin
   - 同样 `pb->run()` 循环

3. **症状签名** (`cpu-sim-add-elf-tohost-fix` change 调试期获得):
   - PC +4 推进 (`pb->run()` 在调)
   - fetch stage 永 stall (decode/memory 不被调度)
   - tohost 永远 0, cycles 满预算

**为什么不是 framework-level bug**: C++ API 路径 48/48 PASS 已证 pipeline tick 机制本身可达; 差异只来自输入侧 (F1+F2+F3 复合)。原 `AGENTS.md:61` "framework-level bug" 表述**已被本 change §Why 复合根因段修正** (推迟到 §6.4 archive 时同步更新, 引用本 caveat)。

**为什么是 now**:
- 5 FAIL 阻塞 v0.10.0 final launch 全绿 (但已诚实记录于 `AGENTS.md`, 非硬阻塞)
- `cpu-sim-add-elf-tohost-fix` change (v0.10.1 hotfix, 2026-10-12) 落地工具层 workaround, 根因未修
- **真实驱动力在 v0.11.0 + wave5 传导**: `tools/dse/sweep_driver.py` 依赖 cpu_sim 消费 JSON config (F1 静默丢字段意味着 DSE/sweep 全矩阵静默污染), `feat-dhrystone-dmips-gate` (DMIPS/MHz ≥1.4) 与 `feat-bp-btb-gshare` (CoreMark ≥1.9) 的**测量底座是坏的**, 不修即无法取证达标
- **诊断必须** 独立 change (避免与 "修复尝试" 耦合) 才能输出可信结论, 后续 `cpu-pipeline-tick-fix` change 才能基于此设计

**为什么是 now**:
- 5 FAIL 阻塞 v0.10.0 final launch 全绿 (但已诚实性记录于 `AGENTS.md` §known test status)
- `cpu-sim-add-elf-tohost-fix` change (v0.10.1 hotfix, 2026-10-12) 落地工具层 workaround 但根因未修
- **诊断必须** 独立 change (避免与"修复尝试"耦合) 才能输出可信结论, 后续 `cpu-pipeline-tick-fix` change 才能基于此设计

## What Changes

### 1. 新增 openspec change workspace

`openspec/changes/cpu-pipeline-tick-rca/` (本 change):
- `proposal.md` (本文件): Why/What/Capabilities/Impact
- `design.md`: 4 调查线详细分析 + control experiment 设计 + 修复建议
- `tasks.md`: 5 阶段 16+ checkbox (per-line × 步 + stop-and-document)
- `specs/cpu-pipeline-tick-rca/spec.md`: 诊断方法约定 (可复现性 + 验证路径 + 排除清单)

### 2. 不动 product code

**严格不动**: `tools/cpu_sim/main.cpp`, `ip/cpu/cpu_factory.h`, `ip/cpu/plugins/*`, `src/cf_plugin/CMakeLists.txt`, 任何 framework 文件.

**例外**: 临时 debug log 允许 (含条件):
- 必须用 `git stash` 或独立 patch 文件管理, **禁止 commit 到 HEAD**
- 验证完必须 revert (tasks.md 显式 checkbox)
- archive 前必须 `git status --porcelain` 与 `git diff HEAD~N` 校验, 仅有 openspec artifacts

### 3. 4 调查线 + 1 control experiment

| 线 | 候选根因 | 排序 | 主要方法 |
|---|---|---|---|
| **L4** (新加) | `cfg.enable_mmu` config 分歧 (CLI 默认 true vs runner 显式 false) | **最高** | **control experiment** (临时加 `--no-mmu` flag 验证) + 静态 diff `cpu_default.json` vs 测试 config |
| **L2** | `ibus.h:54-67` fetch CtrlLink `halt_when` 在 `tlb_lookup_ifetch` 节点存在时是否误判 | 第 2 | 静态读 halt_when lambda + 临时 log `PipeBuilder::run()` stall 分支 + `pb->build()` 后 dump `ctrl_link_count(stage)` |
| **L3** | `stage_link.h` EARLY 闭包在 5 段链上是否正确连接, payload 跨阶段传播 | 第 3 | 静态读注册顺序 + 临时 log EARLY 闭包入口 fetch/decode PC+INSTRUCTION |
| **L1** (降级) | `build_cpu_impl` Result 包装 + `auto_throw` 解包 + `std::unique_ptr<PipeBuilder>` | **最后** | 5 分钟静态排除 (无 tick 差异机制) |

每条调查线必须有 **stop-and-document 结论块** (continue / pivot / confirmed / excluded 判定), 否则视为 incomplete.

### 4. Verification 阶段

- **架构 gate 必跑**: `bash tools/verify_adr.sh` + `tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` — 0 FAIL (兜底 debug log 残留检测)
- **`[riscv-tests]` 必跑**: 48/48 PASS (0 regression 硬约束)
- **`[cpu]` + `[cpu-integration]` 必跑**: 5 FAIL 状态**保持** (change 不修复, 只诊断)
- **`[cpu-l1-mmu-demo]` 必跑**: 6/6 PASS (mmu 相关测试不受影响)
- **`[cpu-integration][mul-div-fsm-integration]` 必跑**: 4/4 PASS (框架回归)

## Capabilities

### New Capabilities

- `cpu-pipeline-tick-rca-methodology`: 诊断 cpu_sim CLI 路径 pipeline 推进缺陷的 4 调查线 + control experiment 方法学 (含可复现性 + 验证步骤 + 排除清单). 后续 `cpu-pipeline-tick-fix` change 可引用此 capability 作为诊断 baseline.

### Modified Capabilities

无. (`cpu-sim-elf-base-tohost-override` capability 不变, 仅 work-around 链路保持.)

## Impact

**Affected docs** (P0 重做后):
- `openspec/changes/cpu-pipeline-tick-rca/{proposal,design,tasks}.md` (本 change artifacts)
- `openspec/changes/cpu-pipeline-tick-rca/specs/cpu-pipeline-tick-rca/spec.md` (诊断方法约定, 含复合根因 requirement)
- `AGENTS.md` §known test status **2 处改动** (推迟到 §6.4 archive 时同步):
  - 第 61 行 "framework-level bug" 措辞 → 改为 "config 保真度 + 测试 config 意图 + MMU 瞬态语义三层复合, see openspec/changes/cpu-pipeline-tick-rca"
  - "pipeline 不能 tick" 措辞 → 改为 "fetch stage 永 stall (`pb->run()` 持续推进, decode/memory payload 全 0)" (本 caveat 引用)
- `ip/cpu/plugins/mmu_exception_handler_chmem.h:26` (注释从 "defer Phase 6d.6+" 改为 "defer Phase 6d.6+ cpu-pipeline-tick-fix change, 诊断中 see cpu-pipeline-tick-rca")
- `openspec/specs/cpu-sim-elf-base-tohost-override/spec.md:94-105` deferral 引用 (保持, 与本 change 不冲突)

**Affected tests** (硬约束, 不动):
- `[riscv-tests]` 48/48 PASS 保持
- `[cpu-l1-mmu-demo]` 6/6 PASS 保持
- `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS 保持
- `[mmu]` 59/59 PASS 保持
- `[mmu-verilator]` 3/3 PASS 保持
- `[cpu]` 128/129 (1 FAIL cpu_sim_real_tohost) **保持**, 不试图修复
- `[cpu-integration]` 30/35 (4 FAIL + 1 SIGTERM) **保持**, 不试图修复

**Affected code**: 无 (纯诊断)

**Out of scope** (避免 scope creep, **D4 决策对齐**):
- **不**改 `CPUConfig::enable_mmu` 默认值 (保持 true, D4 决策; 留给后续 `cpu-pipeline-tick-fix` 用"测试 config 改 false + cpu_sim --no-mmu override"路径)
- **不**改 `load_config()` JSON 解析 (留给 `cpu-pipeline-tick-fix`)
- **不**改 `auto_throw` / `build_cpu_impl` (除非 L1 调查发现真因在此)
- **不**动 `mmu-chmem-pipeline-integration` 共享 `_chmem.h` 头文件 (避免干扰在途 change; 若 L2 真因需读 `ip/mmu/tlm/MMUPlugin.cpp:109`, tasks.md 白名单显式允许 read-only)
- **不**重写 `StageLinkPlugin` (HIGH 风险, 仅 L1/L2/L3 全部 excluded 后才考虑, 且仍写 change 而非本 RCA)
- **不**修复 `test_dhrystone` SIGTERM timeout (独立 issue, 属 `feat-dhrystone-dmips-gate` initiative)
- **不**做 `tools/dse/sweep_driver.py` 改动 (静默污染是本 RCA 的结论, 修复留给 `cpu-pipeline-tick-fix` 或独立 `dse-config-fidelity` change)
- **不**加 `load_config()` 单测 / CLI-vs-API 对照测试 (留给 `cpu-pipeline-tick-fix`, 见 design.md §Open Questions)

**Dependencies**:
- `cpu-sim-add-elf-tohost-fix` (前置, 2026-10-12 archive) — workaround 已就位, 本 change 在其基础上诊断
- `phase-6d-prerequisites` (并行, 可在 archive 前完成) — CI 集成 + ADR-037 v2.0 验证
- `mfc-extract-fsm-h` (v0.11.0 推迟 scope) — 不直接相关, 但避免修改 `mul_div_fsm.h`
- `mmu-chmem-pipeline-integration` (在途) — 本 change 须避免修改共享头文件, 不动 `_chmem.h` 文件

**Risk**:
- **Low**: 仅诊断, 不动 product code. 唯一风险是临时 debug log 残留, 用 `git status --porcelain` + 架构 gate 兜底
- **Medium**: 调查线 L2 涉及 fetch stall 语义, 如不当修改 `halt_when` lambda 可能误判已通过测试. **本 change 不修改**, 只读 + log
- **High** (已规避): 调查线 L3 StageLink 重写, **本 change 不重写**, 只读 + log

## Open Questions (从 cpu-sim-add-elf-tohost-fix 继承并更新, P0 重做对齐 D4 决策)

**OQ1**: fetch 永 stall 的真根因是什么 (复合 3 层 F1+F2+F3, 还是含 F4 第 4 层)?
- **状态**: §1 control experiment 先答 F4 (反向对照), 再按 D3 跑 4 config × `--no-mmu` 填 5-测试独立基线台账
- **预计产出**: design.md §5 综合诊断 + 5-测试独立台账 + 修复建议方向

**OQ2**: 后续修复 `cpu-pipeline-tick-fix` change 的优先级与范围?
- **状态**: 待本 change 完成后, 基于诊断结论定
- **预计启动**: Phase 6d.6+ 启动期, 与 `mmu-chmem-pipeline-integration` 协调 (改 config 传播路径)
- **D4 硬约束**: **不**改 `CPUConfig::enable_mmu` 默认值 (保持 true); 走 §5.3 候选 (a)+(c) (`load_config` 解析 enable_mmu + 测试 config 改 false + cpu_sim `--no-mmu` CLI override)

**OQ3** (重做, 原 D4 违规版已删): `cfg.enable_mmu` 默认值是否保持 true?
- **D4 决策 (2026-10-15)**: **保持 true**, 不动。理由:
  - v0.10.4 hotfix (`a3afcfa`) 已锁定 Bare shortcut 严格判定, 是 mmufault-handler v1 依赖的稳定语义
  - `cpu_factory.h:444` `make_satp_value(Sv39, 0)` 自动编码与默认 true 配套, 改默认会再次打破 `EnableMMU5StageBuilds` `node_count()==13` 与 `mmu-riscv-isa-adapter/spec.md:95` "MUST return exactly 12 plugins" 等既有 spec 契约
  - `[mmu] 59/59` + `[cpu-l1-mmu-demo] 6/6` + `[mmu-verilator] 3/3` 全 PASS 依赖此默认
- **修复方向**: 改 `load_config()` 解析 `enable_mmu` + cpu_sim CLI 加 `--no-mmu` override + 测试 config 改 `false` (D2 配套)

**OQ4** (新, F4 反向对照): 同 ELF + `enable_mmu=true` 在 C++ API 路径 (`test_rv32ui_runner.cpp`) 跑 PASS 还是 FAIL?
- **前置验证**: §1 control experiment 必须先答; **预期** `{PASS(tohost=1) 时即使开 MMU runner 路径也通, F4 confirmed + 三层复合根因, L2 降为 secondary}` 或 `{FAIL(tohost=0) 时第四层存在, L2 升为主要嫌疑}`
- **可接受结局集合**: `{tohost=0 且 cycles 耗尽 / 进程 terminate with exception}`——后者不算 "实验失败" 而是 "复合根因证据" (Oracle 已记录 `MmuExceptionHandlerPlugin` PTW fault throw 路径有 terminate 风险, design.md:207 自身引用)

**OQ5** (新, 7-stage 同因验证, D3 落地): `cpu_superscalar.json` (dispatch_width=2, n_lanes=2 触发 `cpu_factory.h:367` lane dispatch 路径) 走 cpu_sim 是否与其余 4 个测试同因?
- **前置验证**: §1 control experiment 跑 4 次 `--no-mmu` (每 config 一次), 7-stage 数据进 5-测试独立基线台账
- **预期**: 若 7-stage FAIL 且 cycles/hung 与 5-stage 签名一致 → 同因; 若不一致 → lane dispatch 路径独立 bug, 后续 fix change 需分支处理
