---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0
depends_on:
  - cpu-sim-add-elf-tohost-fix
---

# cpu-pipeline-tick-rca — 诊断 cpu_sim CLI 路径 pipeline 不 tick 根因

> **Pre-existing FAIL 范围** (继承自 `openspec/changes/cpu-sim-add-elf-tohost-fix/`):
> - `test_cpu_sim_real_tohost` (在 `[cpu]`)
> - 4 个 `test_{3,5,7,10}stage_riscv::end_to_end` (在 `[cpu-integration]`)
> - 实测对比: C++ API 路径 `test_rv32ui_runner.cpp` 跑同 `rv32ui-p-add` ELF **48/48 PASS**
> - 唯一已知行为差异: cpu_sim CLI 用 `build_cpu_impl` (Result 包装) + `auto_throw` + `std::unique_ptr<PipeBuilder>` (但此差异本身可能不是真因, 见 §Why)
>
> **本 change scope**: **纯诊断**, 不动 product code 修复. 产出诊断报告 (3-4 调查线结论 + 最可能根因 + 后续 `cpu-pipeline-tick-fix` change 实施建议). 后续 `cpu-pipeline-tick-fix` change 才会真正修复.

## Why

**Phase 1 真因证据** (从 `cpu-sim-add-elf-tohost-fix` change 实测获得):

1. **C++ API 路径 48/48 PASS** (`test_rv32ui_runner.cpp`):
   - 直接调 `CpuFactory<uint32_t>::build_cpu(cfg, &mem)`, 不用 Result 包装
   - `cfg.enable_mmu = false` 显式设置
   - `pb->run()` 循环, `mem.exited()` 检测

2. **cpu_sim CLI 路径 100% FAIL** (`tools/cpu_sim/main.cpp`):
   - 调 `auto_throw(build_cpu_impl(cfg, &mem))`, `pb` 是 `std::unique_ptr<PipeBuilder>`
   - `cfg = load_config(config_path)` 从 JSON 加载
   - `cfg.isa = "rv32i"` 显式 pin
   - 同样 `pb->run()` 循环

3. **实测症状** (workaround change 调试期间获得):
   - PC 通过 writeback +4 推进 (证明 `pb->run()` 调了)
   - memory 节点 payload (DECODE / INSTRUCTION) 全 0 (证明 decode/memory 阶段 at_stage 闭包未触发或未传播)
   - tohost 永远 0, cycles 满预算

4. **真差异候选 (Metis + Oracle 双复审 2026-10-12 一致识别)**:
   - `load_config()` **不解析 `enable_mmu` 字段** (只解析 7 字段: isa/stages/mul_*/dispatch/n_lanes/icache/dcache/n_threads)
   - `cfg.enable_mmu` 保留 `CPUConfig` struct 默认值 = **`true`** (cpu_factory.h:136)
   - 启用了 MMU → `MMUPlugin` + `RiscvMMUPlugin` 注册 → `tlb_lookup_ifetch` 节点存在 → fetch CtrlLink `halt_when` 评估 `PTW_ACTIVE`
   - 而 runner 显式 `cfg.enable_mmu = false`, 不注册任何 MMU plugin, fetch 闭包走 vaddr 直接 path
   - **此 config 分歧是迄今未被原始 3 调查线覆盖的最高优先候选**

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

**Affected docs**:
- `openspec/changes/cpu-pipeline-tick-rca/{proposal,design,tasks}.md` (本 change artifacts)
- `openspec/changes/cpu-pipeline-tick-rca/specs/cpu-pipeline-tick-rca/spec.md` (诊断方法约定)
- `AGENTS.md` §known test status (更新 5 FAIL 描述为 "5 FAIL 根因调查进行中, see openspec/changes/cpu-pipeline-tick-rca/")
- `ip/cpu/plugins/mmu_exception_handler_chmem.h:26` (注释从 "defer Phase 6d.6+" 改为 "defer Phase 6d.6+ cpu-pipeline-tick-fix change, 诊断中 see cpu-pipeline-tick-rca")

**Affected tests**:
- `[riscv-tests]` 48/48 PASS 保持 (硬约束)
- `[cpu-l1-mmu-demo]` 6/6 PASS 保持
- `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS 保持
- `[cpu]` 128/129 (1 FAIL cpu_sim_real_tohost) **保持**, 不试图修复
- `[cpu-integration]` 30/35 (4 FAIL + 1 SIGTERM) **保持**, 不试图修复

**Affected code**: 无 (纯诊断)

**Out of scope** (避免 scope creep):
- 不修 `cfg.enable_mmu` 默认值 (留给 `cpu-pipeline-tick-fix`)
- 不修 `load_config()` JSON 解析 (留给 `cpu-pipeline-tick-fix`)
- 不改 `auto_throw` / `build_cpu_impl` (除非 L1 调查发现真因在此)
- 不动 `mmu-chmem-pipeline-integration` 共享头文件 (避免干扰在途 change)
- 不重写 `StageLinkPlugin` (HIGH 风险, 仅 L1/L2/L4 排除后才考虑)
- 不修复 `test_dhrystone` SIGTERM timeout (独立 issue, 属 `feat-dhrystone-dmips-gate` initiative)

**Dependencies**:
- `cpu-sim-add-elf-tohost-fix` (前置, 2026-10-12 archive) — workaround 已就位, 本 change 在其基础上诊断
- `phase-6d-prerequisites` (并行, 可在 archive 前完成) — CI 集成 + ADR-037 v2.0 验证
- `mfc-extract-fsm-h` (v0.11.0 推迟 scope) — 不直接相关, 但避免修改 `mul_div_fsm.h`
- `mmu-chmem-pipeline-integration` (在途) — 本 change 须避免修改共享头文件, 不动 `_chmem.h` 文件

**Risk**:
- **Low**: 仅诊断, 不动 product code. 唯一风险是临时 debug log 残留, 用 `git status --porcelain` + 架构 gate 兜底
- **Medium**: 调查线 L2 涉及 fetch stall 语义, 如不当修改 `halt_when` lambda 可能误判已通过测试. **本 change 不修改**, 只读 + log
- **High** (已规避): 调查线 L3 StageLink 重写, **本 change 不重写**, 只读 + log

## Open Questions (从 cpu-sim-add-elf-tohost-fix 继承并更新)

**OQ1**: pipeline 不 tick 的真根因是什么?
- **状态**: 转移至本 change 调查
- **预计产出**: design.md §3 含 4 调查线结论 + 修复建议

**OQ2**: 后续修复 `cpu-pipeline-tick-fix` change 的优先级与范围?
- **状态**: 待本 change 完成后, 基于诊断结论定
- **预计启动**: Phase 6d.6+ 启动期, 与 `mmu-chmem-pipeline-integration` 协调

**OQ3**: `cfg.enable_mmu` 默认值是否应该改?
- **状态**: 待 L4 调查结论
- **如果 L4 confirmed**: 建议在 `cpu-pipeline-tick-fix` change 中改 `CPUConfig` 默认 + 改 `load_config()` 解析 enable_mmu
- **如果 L4 excluded**: 改 cpu_sim CLI 显式 pin `cfg.enable_mmu = false` 作为 workaround
