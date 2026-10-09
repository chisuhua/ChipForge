---
id: mfc-extract-fsm-h
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P1
manual_deps: [mfc-cpu-pipeline-multi-cycle-fsm]
review_by: 2026-10-23
---

# mfc-extract-fsm-h

## 1. Why

### 项目上下文（v0.11.0 Wave 5 收官窗口）

v0.11.0 是 Wave 5 ISA 覆盖 + 分支预测收官版本。`objective-v0110-launch.md §2` 锁定两条 Track A 硬指标：

| 判据 | 当前 | 目标 |
|------|------|------|
| [riscv-tests] | 40/48 (rv32um 0/8) | **48/48** (rv32um 8/8) |
| DMIPS/MHz | 0.0351 | **≥ 1.4** (硬门禁) |

两条指标都落在 `ip/cpu/arch/riscv/mul_div_fsm.h` 上：

1. **rv32um 0/8 根因**：`mul_div_fsm.h:475-500` `advance_fsm` 实装是 ad-hoc busy counter（`busy_cycles_` 计数到 33 后直接调 `compute_mul_div`），不是真 radix-2 iterative。文件头部注释自证 "Phase A 仅满足部分 ad-hoc enum switch 是过渡实现"。
2. **dhrystone livelock**：`[cpu-integration][dhrystone]` 60s+ timeout（commit 76ac53f "确认 livelock"），DMIPS/MHz 0.0351 距硬门禁 1.4 差 40x。

### 本 change 要解决的架构债

`mfc-cpu-pipeline-multi-cycle-fsm` Phase D.1 落地后，CH_MEM DSL 代码（`create_fsm` / `opcode_sig_` / `counter_reg_` / `result_reg_` / `busy_cycles_reg_`）仍内联在 TLM 文件 `mul_div_fsm.h` 的 `#ifdef CF_PLUGIN_USE_CH_MEM` 块（line 276-454，~180 LOC）。三个问题：

1. **违反 ADR-040 v2.0 双文件分离精神**：TLM 文件含 `ch_uint`/`ch_reg`/`ch_state_machine`（虽在 `#ifdef` 块内，`check_plugin_portability.sh` 已豁免 `_chmem.h` 但 TLM 文件内 `#ifdef` 是技术债，AC-4 要求 grep `ch_uint|ch_reg|ch::core` = 0 hits）。
2. **复用难**：业务 Plugin（L1Cache refill FSM / Hazard detection / Branch prediction）想复用 ch_state_machine DSL 模板需 include 整个 `mul_div_fsm.h`，不是干净 reusable header。
3. **真 radix-2 落点被堵**：`advance_fsm` 重写为真 radix-2 iterative 需要干净的 FSM 边界；不先提取 `multi_cycle_fsm.h` 框架，真 radix-2 与 DSL 内联代码纠缠，无法独立验证。

> 本段只给缺口摘要与项目上下文，完整设计/任务明细见 [`openspec/changes/mfc-extract-fsm-h/proposal.md`](../openspec/changes/mfc-extract-fsm-h/proposal.md) + [`tasks.md`](../openspec/changes/mfc-extract-fsm-h/tasks.md)（Phase A/B/C），本文件不重复。

## 2. What Changes

### 启动时机约束（对齐 proposal §1.2）

必须等 `mfc-cpu-pipeline-multi-cycle-fsm` Phase E（rv32um 8/8 验证）+ Phase G（Dhrystone baseline）+ Phase H（archive）全部完成；当前 change 是 `manual_deps` 硬前置。

### 变更范围

- **新增** `include/cf/plugin/multi_cycle_fsm.h`（框架级）：`FsmBase<StateEnum, NStates>` 模板，TLM 模式 ad-hoc `enum class` + `switch`，CH_MEM 模式 `ch_state_machine` DSL，必须提供 `negotiate()` capability（ADR-082）。
- **重构** `ip/cpu/arch/riscv/mul_div_fsm.h`：移除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块（line 276-454，~180 LOC），改为继承 `FsmBase<State, 4>`；`advance_fsm` 改为真 radix-2 iterative（替代 `busy_cycles_` ad-hoc counter）。
- **重接** `ip/cpu/plugins/mul_div_fsm_chmem.h`：删除内联 DSL，重新 include 框架版。
- **迁移测试 include 路径**：`tests/framework/test_chmem_multi_cycle_fsm.cpp` + `tests/framework/test_mul_div_fsm_cycle_parity.cpp`（proposal/tasks 简称 `test_cycle_parity.cpp`）。

### 非目标（Out of Scope）

- 不改 `objective-v0110-launch.md` / 任何 `feat-*.md`（上游 SSOT）。
- 不改 `openspec/changes/mfc-extract-fsm-h/` 下任何文件（不同层级）。
- 不引入 CSR plugin、不做 dual-issue（`objective-v0110-launch.md §4` 反模式 R1：先真 radix-2 + fetch stall + BP 协同三件套）。

## 3. Acceptance

- [ ] `bash tools/run_chipforge_tests.sh --all` 0 regression（TLM 432/432 + CHMEM 48/48 + verilator + gate 全绿）
- [ ] `[riscv-tests] rv32um 8/8 PASS`（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）
- [ ] `ip/cpu/arch/riscv/mul_div_fsm.h` `advance_fsm` 实现真 radix-2 iterative（no `busy_cycles_` ad-hoc counter，与 1c/3c/33c 三模板 cycle 数一致）
- [ ] `[cpu-integration][dhrystone]` PASS 无 livelock，cycle ≤ 1M
- [ ] dmips/mHz ≥ 1.4（硬门禁 via `feat-dhrystone-dmips-gate`）
- [ ] AC-4 落地：TLM-only 文件 grep `ch_uint|ch_reg|ch::core` = 0 hits（`check_plugin_portability.sh` 12/12 PASS）
- [ ] AC-5：ADR-082 negotiate API 仍工作（`MulDivFsmPlugin::negotiate` 经 `FsmBase::negotiate` 提供 `multi_cycle_fsm` capability）
- [ ] `openspec archive mfc-extract-fsm-h` 成功

## 4. Capabilities

新增能力（对齐 proposal §2.3 + tasks Phase A.2）：

- **框架级 `multi_cycle_fsm.h` 双模式模板**：`FsmBase<StateEnum, NStates>` 一套 FSM 描述同时服务 TLM 与 CH_MEM 两种编译模式；业务 Plugin 继承后 **0 业务代码感知双模式**——TLM 模式编译为 ad-hoc `enum class State` + `switch`（ADR-048 兼容），CH_MEM 模式编译为 `chlib::ch_state_machine` DSL（`#ifdef CF_PLUGIN_USE_CH_MEM` 内提供 `create_chmem_fsm()` / `state_out()`）。业务侧只写一次状态转移表。
- **可复用性**：L1Cache refill FSM / Hazard detection / Branch prediction 等业务 Plugin 可直接 include 框架头，无需再借道 `mul_div_fsm.h`。
- **ADR-082 negotiate 契约**：`FsmBase::negotiate(CapabilityTable&)` 提供 `multi_cycle_fsm` capability，`MulDivFsmPlugin` override 接续 `flush_broadcaster` / `writeback_arbiter` 资源协商，CH_MEM 与 TLM 行为一致。
- **真 radix-2 iterative 可独立验证**：`advance_fsm` 重写与 FSM 框架提取解耦，rv32um 8 ELF（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）cycle 数可逐指令断言。

## 5. Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `ip/cpu/arch/riscv/mul_div_fsm.h` | 重构 | 移除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块（~180 LOC extraction）+ `advance_fsm` 真 radix-2 重写；TLM 行为向后兼容（`[cpu]` 125/125 维持） |
| `ip/cpu/plugins/mul_div_fsm_chmem.h` | 重接 | 删除内联 DSL 代码，重新 include 框架版 `multi_cycle_fsm.h`（保留 `has_chmem_support` symbol 供 cycle parity 测试入口） |
| `include/cf/plugin/multi_cycle_fsm.h` | **新增** | 框架级新头文件，`cf::plugin::multi_cycle_fsm` namespace |
| `tests/framework/test_chmem_multi_cycle_fsm.cpp` | 迁移 | include 路径 `ip/cpu/arch/riscv/mul_div_fsm.h` → `cf/plugin/multi_cycle_fsm.h` |
| `tests/framework/test_mul_div_fsm_cycle_parity.cpp` | 迁移 | 同上（proposal/tasks 简称 `test_cycle_parity.cpp`） |
| `docs/architecture/adr/ADR-040-v2.md` | 文档 | tasks C.4 添加 `multi_cycle_fsm.h` 引用（ADR-040 v2.0 §2.1） |

### 风险（来自 `objective-v0110-launch.md §5`）

- **R-RADIX-1**：mfc-extract-fsm-h 1-2 周估时是 Oracle 经验值，实装时可能发现 ch_state_machine DSL 边界 case → 缓解：Sprint 1 后段留 1 周 buffer，bp-btb 启动期（本 change `review_by: 2026-10-23` 即 Sprint 1 窗口终点）。
- **R-RADIX-2**：mfc team 双 P1（`mfc-cpu-pipeline-multi-cycle-fsm` 50/60 + 本 change 15）资源冲突 → 缓解：owner 区分——Phase H archive 归前者，真 radix-2 归本 change，共享 `MulDivFsmPlugin::negotiate()` 接续。
- **API 兼容性破坏（proposal R1）**：`create_fsm()` / `state_out()` / `opcode()` 签名变化 → 提供 typedef 兼容层，deprecate 1 version 后删除。
- **测试 include 批量改动（proposal R2）**：5+ 测试文件改 include → 批量改 + 立即编译 + 立即测试。
- **回退**：独立 commit，`git revert` 即可，不与其他 change 交织。

### 下游依赖

- `feat-mfc-multi-cycle-fsm` / `feat-rv32um-m-extension` / `feat-dhrystone-dmips-gate`：本 change 的 FSM 提取是真 radix-2 + rv32um 8/8 + DMIPS ≥1.4 的架构前置。
- `objective-mmu-chmem-phase-b-e §9.1`：mmu-chmem Phase C（`ibus_chmem.h`）依赖 fetch stall framework 扩展，而 fetch stall 扩展依赖本 change 的 FSM 边界清晰化。
