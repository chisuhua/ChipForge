# Change: `plugin-framework-stall` — 声明式控制流原语消费 + PTW/Hazard 真实消费者

> **Schema**: spec-driven
> **Date**: 2026-09-15
> **Status**: PROPOSED
> **Priority**: 19/20 (Oracle 评估, top 1 of 3)
> **Depends on**: cpu-pipeline-stubs-replace (v0.1.2, archived 2026-09-15)
> **Unblocks**: soc-cpu-l1-mmu-demo · cache-dse-sweep · riscv-tests-rv32ui · cpu-exception-handling · cpu-pipeline-multi-cycle

## 1. Why (背景与动机)

### 1.1 当前状态（2026-09-15, post-cpu-pipeline-stubs-replace）

**所有声明式控制流原语已实现但 0 消费**：

| 原语 | 位置 | 状态 | 消费者 |
|------|------|------|--------|
| `CtrlLink::halt_when` | `ctrl_link.h:34` | ✅ 已实现 | ❌ 0 |
| `CtrlLink::throw_when` | `ctrl_link.h:40` | ✅ 已实现 | ❌ 0 |
| `CtrlLink::flush_when` | `ctrl_link.h:46` | ✅ 已实现 | ❌ 0 |
| `CtrlLink::bypass` | `ctrl_link.h:52` | ✅ 已实现 | ❌ 0 |
| `PipeNode::State` 5 态 | `pipe_node.h:36-42` | ✅ 已实现 | ❌ 0（仅 reset） |
| `PipeArbitration::arb_` | `pipe_node.h:128-129` | ✅ 已实现 | ❌ 0 |
| `commit_storages()` | `pipe_builder.h:186-188` | ✅ 已实现 | ⚠️ cache L1 用，但 CPU 端未用 |

**5 个隐藏 stall 缺口**（pipeline 真跑后浮现，cpu-pipeline-stubs-replace commit E 验证后暴露）：

| 缺口 | 现象 | 来源 |
|------|------|------|
| **PTW busy → fetch 无 stall** | `MMUPlugin.cpp:57` 写 `PTW_ACTIVE=1` 后 fire-and-forget，IBusPlugin cycle N 拿不到真 PADDR 仍读 stale 0 | `ip/mmu/STATUS.md:39` |
| **Hazard RAW → execute 无 stall** | `hazard.h:165` 注释："M2 阶段: 仅记录冒险, 不实际阻塞 (M4 集成后通过 CtrlLink 阻塞)" | `hazard.h:167` 显式 TODO |
| **Branch mispredict → 无 flush** | 整个 mispredict-squash 原语 0 消费者 | `pipe_builder.h:171-172` 注释点名 |
| **Multi-cycle mul/div → 无 stall** | `RiscvMulPlugin` LATENCY param 默认 1，>1 无 stall | `mul.h` |
| **Cache miss → fetch 无 stall** | L1CachePlugin hit/miss 处理无 stall 传播 | `cache/tlm/` |

### 1.2 为什么是 Now

**已完成**：`cpu-pipeline-stubs-replace` (v0.1.2, 6 commits) 让 `cpu_sim --elf add.elf` 首次真跑 Plugin pipeline → `tohost=1`。

**DSE 文档原文**（`dse_architecture_v2_design_research.md:219-225`）：

> "框架脊柱...**不需要改**。`commit_storages` hook **已经是** OoO commit 原语。`flush_when` **已经是** mispredict squash 原语。"

**但 DSE 文档自承缺口**（`plugin-style-design-methodology-v1.md:118-123`）：

> "B3-D4.2: `pb.run()` 无 cycle 精度：`pb.run()` 遍历 `stages_` 向量依次执行所有 at_stage 回调，**无 cycle 0/1/2 区分**。"

**冲突点**：DSE 文档既说"原语已够"又说"无 cycle 精度"。本研究判定：**API 已就绪、缺消费者 + 框架消费点**。`pb.run()` 当前是单 pass，每个 callback 都跑——即使 CtrlLink `should_halt()=true`，callback 仍会执行。这是本 change 要修的核心。

### 1.3 战略价值

**Oracle 评估**（来自 `bg_10cac111`，总分 20）：

| 维度 | 得分 |
|------|------|
| Architectural Value | 5/5（声明式控制流原语的兑现） |
| Risk | 4/5（框架改动小，318 测试 + 4 门禁兜底） |
| ROI | 5/5（3-7 天） |
| Strategic Fit | 5/5（D4 试金石，Phase 5 RTL 转换前必须验证） |
| **Total** | **19/20** |

**SpinalHDL/VexRiscv 证明**：`stage.arbitration.haltItself` 是 SpinalHDL 流水线的核心，每条 `at_stage` 闭包都会调用。ChipForge 必须有等价物。

**VexRiscv Proteus 论文佐证**（Bognár et al., 2023）：Proteus 的 OoO pipeline **完全由插件组合**，所有控制流（hazard/flush/stall）都是 `service` + `CtrlLink`。

## 2. What Changes（变更范围）

### 2.1 框架层（`include/cf/plugin/`）

| 改动 | 位置 | 行数 |
|------|------|------|
| `PipeBuilder::run()` 内 stall loop | `pipe_builder.h:100-122` | +30 |
| `PipeBuilder::run(stop_when)` 公开 API | `pipe_builder.h` (新) | +10 |
| `PipeBuilder::register_ctrl_link(ctrl, stage_name)` 新 API | `pipe_builder.h` (新) | +15 |
| `PipeBuilder::should_stall_stage(name)` 内部查询 | `pipe_builder.h` (新) | +5 |
| 文档化 CtrlLink 消费契约 | `pipe_builder.h` 注释 | +40 |

**Scope 限制**：不改 `PipeNode` 5 态状态机、不改 `PipeArbitration`、不改 `PluginBase`。这些是"未来消费"的原料，本 change 只打通消费链。

### 2.2 框架测试（`tests/framework/`）

| 新测试 | 覆盖 |
|--------|------|
| `test_pipe_builder_stall.cpp` | 新增 12 case：pb.run() 在 CtrlLink halt 时跳过 callback、flush 时清 payload、throw 时异常、空 CtrlLink 不影响、OR 合并、per-stage 绑定 |

### 2.3 第一个真实消费者（验证场景）

**A. PTW-busy → IBus fetch stalled**

| 改动 | 位置 |
|------|------|
| `IBusPlugin::build()` 注册 `pb.register_ctrl_link(fetch_ctrl, "fetch")` | `ip/cpu/plugins/ibus.h` |
| `fetch_ctrl.halt_when([&pb] { return pb.node_of_logic_stage("tlb_lookup_ifetch")->operator()(PTW_ACTIVE) == 1; })` | `ip/cpu/plugins/ibus.h` |
| 注入测试：`tests/mmu/test_ptw_stall_integration.cpp`（3 case：PTW miss → IBus 跳过 → 完成 → IBus 重读） | `tests/mmu/` |
| ~~恢复 `tests/CMakeLists.txt` 中的 mmu 测试排除项~~（R2 fix #2 移除：实测 CMakeLists 无 `list(REMOVE_ITEM)`，GLOB_RECURSE 自动拾取新测试） | — |

**B. HazardPlugin RAW → execute stalled**

| 改动 | 位置 |
|------|------|
| `HazardPlugin::build()` 注册 `pb.register_ctrl_link(execute_ctrl, "execute")` | `ip/cpu/plugins/hazard.h` |
| `execute_ctrl.halt_when([this] { return has_active_hazard(); })` | `ip/cpu/plugins/hazard.h` |
| HazardPlugin 增加 `has_active_hazard()` 公开方法 | `ip/cpu/plugins/hazard.h` |
| 注入测试：`tests/cpu/integration/test_hazard_stall.cpp`（4 case：addi;addi;add → RAW stall 2 cycles，R2 fix #3 统一） | `tests/cpu/integration/` |

### 2.4 throw_when / flush_when 占位消费者（API 演示，非真实语义）

| 改动 | 位置 | 说明 |
|------|------|------|
| `ExceptionPlugin::build()` 注册 throw 演示（不实装 trap delivery，仅 smoke） | `ip/cpu/plugins/exception.h` | 标记 `TODO Phase 5+` |
| `BranchPredictorPlugin::build()` 注册 flush 演示（不实装 mispredict，仅 smoke） | `ip/cpu/plugins/branch_predictor.h` | 标记 `TODO Phase 5+` |

### 2.5 文档（ADR + CHANGELOG + STATUS）

| 文档 | 改动 |
|------|------|
| `docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md` | **新 ADR**：CtrlLink 消费契约 + PipeBuilder::run() stall loop 决策 |
| `docs/architecture/adr.md` | 注册 ADR-045（现有最后一条为 ADR-044，下一为 ADR-045） |
| `CHANGELOG.md` | 新增 `## v0.1.3 (2026-09-XX) - plugin-framework-stall` |
| `ip/mmu/STATUS.md` | "CtrlLink halt_when PTW stall 未实装" 标记为已完成 |
| `ip/cpu/STATUS.md` | 新增"声明式控制流原语激活"段落 |
| `docs/roadmap/README.md` | 更新 next-milestone 状态 |

## 3. Scope Boundaries（不做什么）

- ❌ **不实装 trap delivery**：throw_when 仅作为 API 演示桩，`ExceptionPlugin` 仍是 stub。
- ❌ **不实装 mispredict recovery**：flush_when 仅作为 API 演示桩，`BranchPredictorPlugin` 仍是 stub。
- ❌ **不修改 PipeNode 5 态状态机**：本 change 只激活 `arb_` 字段的 `is_stuck` 推断，5 态方法留 TODO。
- ❌ **不修改 IPC contract**：cpu_keys / mmu_keys / cache_keys 不变。
- ❌ **不实装真实 cycle 精度**：`pb.run()` 仍是单 pass，但每个 stage 内部可 CtrlLink 驱动跳过——真 cycle 精度（`pb.run(cycle_count=N)`）推迟到 `plugin-framework-cycle-precision`。
- ❌ **不恢复 mmu cache 单元测试**：仅恢复新增的 PTW stall integration test。

## 4. Acceptance Criteria（验收标准）

### 4.1 测试
- **baseline 318 → 337 PASS**（+12 framework: test_pipe_builder_stall 12 case; +3 mmu: PTW stall 3 case; +4 cpu-integration: hazard stall 4 case; 总 +19 = **337**；R2 fix #3 统一）
- 5/5 稳定连跑
- 4 architecture gates 全 PASS（verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check）

### 4.2 行为
- CtrlLink `halt_when` 注册后，对应 stage 的 callback 在 `should_halt()=true` 时**不被调用**
- OR 合并语义保持：`pb.should_stall_stage("fetch")` 返回 OR(any fetch_ctrl.halt_when cond)
- 不影响无 CtrlLink 的 stage（空 CtrlLink 完全 no-op）
- 不违反 D4：所有 plugin 仍无 `tick()`、无状态机、无早返

### 4.3 端到端验证
- `IBusPlugin fetch` 在 `PTW_ACTIVE=1` cycle 不读 instruction（PC 不推进）
- `HazardPlugin execute` 在 RAW detected cycle 不写 RESULT
- 真 CPU 真跑：add.elf → tohost=1 仍 PASS（说明 stall 不破坏正常路径）

## 5. Risks & Mitigations（风险）

| Risk | Mitigation |
|------|------------|
| `pb.run()` 加 stall loop 改变全 IP 行为 | 框架层只加"跳过 callback"语义，无新副作用；318 测试兜底 |
| 多 stage 共享 CtrlLink 状态 | `register_ctrl_link` 每 stage 独立 CtrlLink 实例；OR 合并只在同 stage 内 |
| HazardPlugin RAW → stall 改变现有 cpu 测试通过性 | 仅在 RAW detected 时 stall，原有 add.elf 无 RAW 依赖 |
| ADR-045 与 ADR-040 (no early return) 冲突 | stall 用 `if (cond) { /* skip callback */ }` 包裹，不 `return` |

## 6. Out of Scope（推迟到后续 change）

- `plugin-framework-cycle-precision` — `pb.run(cycle_count=N)` 真 cycle 精度
- `cpu-pipeline-multi-cycle` — mul/div LATENCY>1 多周期 stall 实装
- `cpu-pipeline-exception` — trap delivery 实装（throw_when 真实消费者）
- `cpu-pipeline-mispredict` — flush_when 真实消费者
- `cpu-pipeline-cache-miss` — cache miss stall 实装

## 7. References（引用）

### 7.1 内部文档
- `include/cf/plugin/ctrl_link.h:34-96` — CtrlLink API
- `include/cf/plugin/pipe_builder.h:100-122, 128-138, 171-175` — run/commit 钩子
- `ip/mmu/STATUS.md:38-39` — PTW stall TODO
- `ip/cpu/plugins/hazard.h:165-167` — Hazard stall TODO
- `ip/cpu/docs/dse_architecture_v2_design_research.md:219-225` — OoO 原语论证
- `docs/methodology/plugin-style-design-methodology-v1.md:118-123` — pb.run 无 cycle 精度
- `docs/architecture/plugin-framework.md:402-418` — CtrlLink 设计意图

### 7.2 ADR
- ADR-025 Plugin 基类无 tick
- ADR-033 CtrlLink 四种控制 API
- ADR-040 Tier-1 #4 无早返约束
- ADR-044 VIPT L1 Cache

### 7.3 外部参考
- SpinalHDL/VexRiscv `Pipeline.scala` — `stage.arbitration.haltItself` 模式
- SpinalHDL/VexRiscv `HazardSimplePlugin.scala` — RAW hazard 声明式实现
- Bognár et al., "Proteus: An Extensible RISC-V Core", KU Leuven 2023
- Erdem Alkım et al., "ISA Extensions for Finite Field Arithmetic", CHES 2020

## 8. Next Steps

1. ✅ **proposal.md** — 本文件
2. ⏳ `design.md` — 详细设计（API 签名 + 数据流图）
3. ⏳ `specs/{pb-stall-loop,ctrllink-consumption-contract,ptw-fetch-stall-consumer,hazard-execute-stall-consumer}/spec.md`
4. ⏳ `tasks.md` — TDD 5 步结构（write fail → verify fail → implement → verify pass → commit）
5. ⏳ Momus R1 → Oracle R2 → apply → archive
