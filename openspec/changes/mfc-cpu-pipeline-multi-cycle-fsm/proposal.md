---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.0
depends_on:
  - cpu-pipeline-multi-cycle
  - plugin-framework-cycle-precision
---

# mfc-cpu-pipeline-multi-cycle-fsm — MUL/DIV FSM 化与 ADR-082 negotiate 集成（v0.10.0 收官版）

> **`depends_on` 声明 vs 实际消费差异**（2026-09-29 Step 4 决策 (b)）：
> - Frontmatter `depends_on` 含 `plugin-framework-cycle-precision` 是 openspec 模板**声明性元数据**, **非执行阻塞**。
> - 实际消费仅在 **Phase F**（mfc `tasks.md:201` 依赖图: cycle-precision → Phase F → E → D → C → B → A）, **Phase A 启动不依赖**（`tasks.md:11` 明示 "不依赖 `plugin-framework-cycle-precision` change（其尚未 archive, 0/25 tasks）"）。
> - **决策采纳 (b)**: cycle-precision 降级为 Phase F optional。Phase F 时点路径选择:
>   - **(b)-lite**: 直接读 `tools/cpu_sim/main.cpp:230-242` 的 `actual_cycles`（wall-clock 计数, **非** framework cycle 精度, 仅作 cycle 精度不足时的兜底）
>   - **(a)**: Phase F 启动前实装 cycle-precision（解锁真 cycle 精度能力, 当前 0/25 NOT STARTED）
> - **(a) 路径保留** — 若 Phase F 提前进入或 DSE/VexiiRiscv 对拍门禁触发真实消费者需求, 可重新启用

> **承接关系**：本 change **supersedes** `cpu-pipeline-multi-cycle`（P1#5，2026-09 已作为 P1 启动，当前 0/35 tasks，未 archive）。
>
> **处置关系**：
> - 本 change archive 时，`cpu-pipeline-multi-cycle` 头部加 `superseded-by: mfc-cpu-pipeline-multi-cycle-fsm` 标注
> - 本 change 包含 `cpu-pipeline-multi-cycle` 的全部完成项 + 增量（FSM 化 + ADR-082 negotiate 集成）
> - 不并行推进（避免 double-accounting）；本 change 是唯一权威版本
>
> **技术差异**（本 change vs 原 P1#5）：
> - 引入 ADR-046 多周期 FSM 豁免（`ch_state_machine` DSL）—— 原 P1#5 用 ad-hoc stall 计数器
> - 引入 ADR-047 Result 范式（elaboration 期 fail-fast）—— 原 P1#5 缺
> - 引入 ADR-082 `Plugin::negotiate()` capability 协商（v0.10.0 必须）—— 原 P1#5 无
> - 收敛 12 个 PoC 中的 PoC-1 成功标准（rv32um 100% + cycle precision）
> - CH_MEM 模式 `mul_div_fsm_chmem.h` 配对（ADR-040 v2.0 双模同语义）
>
> **命名约定**：`mfc` = Multi-cycle + FSM + CH_MEM（本 change 三大技术特征首字母）。

## Why

`cpu-pipeline-multi-cycle`（P1#5）已完成 multi-cycle stall 的最小骨架，但：

1. **FSM 表达力不足**：当前实现是 ad-hoc stall 计数器（v0.7.0 已有），无法表达状态机协议（如"stall → flush → drain → restart"序列）
2. **与 ADR-046/047 未集成**：现有 multi-cycle 路径绕过 `ch_state_machine` DSL，缺乏 elaboration 期校验
3. **与 ADR-082 未集成**：Plugin 间 capability 协商（`provides` / `requires`）未声明，配置错误在运行时才发现
4. **riscv-tests rv32um PASS 率未达 PoC-1 标准（100%）**：当前是 stub，需要真 FSM 实现

后果：v0.10.0 PoC-1（`MUL=1c, DIV=33c, rv32um 100%, stall 期间 IPC 回归 ≤5%`）无法达成 → v1.0.0 信誉期（CoreMark ≥2.3）无法启动。

## What Changes

### 1. `MulDivFsmPlugin`（新）— FSM 化 MUL/DIV 流水线

- 在 `ip/cpu/plugins/mul.h` / `mul_div_fsm_chmem.h` 实现 `MulDivFsmPlugin`
- 使用 `ch_state_machine` DSL（ADR-046 豁免）描述状态：`IDLE → MULTIPLY(1c) | DIVIDE(33c) → WRITE_BACK`
- `stall()` 走 CtrlLink（ADR-045），其他 Plugin（Branch/Hazard）通过 port 监听
- **CH_MEM 双模配对**：TLM 侧 `mul.h`（功能等价，POD cycle 计数）+ CH_MEM 侧 `mul_div_fsm_chmem.h`（emit `always_ff @(posedge)`）

### 2. `Plugin::negotiate()` capability 声明（ADR-082 落地）

`MulDivFsmPlugin::negotiate(CapabilityTable& cap)`：

```cpp
void MulDivFsmPlugin::negotiate(CapabilityTable& cap) {
    cap.provides = {
        .multi_cycle_fsm = true,
        .fsm_exemption_kind = FSM_EXEMPTION::MULTI_CYCLE_STALL,
    };
    cap.requires = {
        .flush_broadcaster = "CtrlLink flush subscriber",  // from BranchPlugin
        .writeback_arbiter = "LaneArbiter or HazardPlugin",  // from v0.10.0+
    };
}
```

CI 第 8 条门禁起效（`build()` 内 `dynamic_cast` = 0，强制走 negotiate 拿 handle）。

### 3. `MulDivResult` Result 范式（ADR-047 落地）

`MulDivFsmPlugin::build()` elaboration 期校验：

```cpp
auto mul_status = MulDivResult::ok();
if (cfg.xlen != 32) mul_status = MulDivResult::err("MulDivFsmPlugin requires RV32");
if (!cap.has_provider("flush_broadcaster")) mul_status = MulDivResult::err("missing flush_broadcaster");
if (!mul_status) throw PluginException(mul_status.error());  // ADR-047 fail-fast
```

### 4. `riscv-tests rv32um` 8/8 PASS（PoC-1 硬指标）

| 测试 | 当前 | 目标 |
|------|------|------|
| rv32um-p-mul | stub | PASS |
| rv32um-p-mulh | stub | PASS |
| rv32um-p-mulhsu | stub | PASS |
| rv32um-p-mulhu | stub | PASS |
| rv32um-p-div | stub | PASS (33 cycle) |
| rv32um-p-divu | stub | PASS (33 cycle) |
| rv32um-p-rem | stub | PASS (33 cycle) |
| rv32um-p-remu | stub | PASS (33 cycle) |

### 5. Cycle precision 验证（P1#4 闭环）

- 利用 `plugin-framework-cycle-precision` 的 `pb.run(cycle_count=N)` API（**Phase F optional 消费点, 不阻塞 Phase A 启动** — 见 [`plugin-framework-cycle-precision/proposal.md`](../plugin-framework-cycle-precision/proposal.md) §NOT STARTED 标头 + 本 change §实施窗口决策 (b)）
- 验证 MUL=1 cycle、DIV=33 cycle、stall 期间 IPC 回归 ≤5%
- 通过 `busy-cycles` Payload Key 在 idle 周期数 ≤ total cycles × 5%

## Acceptance Criteria

- [ ] `[cpu-integration] multi_cycle_*` 测试全部 PASS
- [ ] `[riscv-tests] rv32um-p-*` 8/8 PASS
- [ ] `[cpu-l1-mmu-demo]` 5 FAIL 修复（如未修则 v0.10.0 不能 launch）
- [ ] TLM 模式 cycle 数与 CH_MEM 模式 elaboration 后的 Verilog 在 Verilator 仿真下 cycle-equal（PoC-9 双模对拍前置）
- [ ] CI 第 8 条门禁（`dynamic_cast` = 0）通过
- [ ] CI 第 10 条门禁（核内禁 `#ifdef FPGA`）通过（无 FPGA 平台特定代码）
- [ ] DHrystone ≥1.4 DMIPS/MHz（CH_MEM + Verilator 实测）— v0.10.0 hard gate

## ADR 锚点

- **ADR-046**（已落地）：多周期 FSM 豁免 D4
- **ADR-047**（已落地）：静态配置 Result 范式
- **ADR-045**（已落地）：CtrlLink stall/flush 契约
- **ADR-070**（待起草）：RV32C 解码（v0.10.0 内嵌，本 change 仅引用）
- **ADR-075**（待起草）：BP 分级（v1.0.0，本 change 不涉及）
- **ADR-082**（待起草）：`Plugin::negotiate()` capability 协商（v0.10.0 内嵌，本 change 是首个消费方）

## 失败 → 砍分叉动作

- DIV >40 cycle（实测）→ 换 radix-4 SRT（+2 周）；仍不达标 → M 扩展软乘除库兜底，HW M 推迟 v1.0.0
- rv32um PASS <8/8（30 个 sprint 内）→ M 扩展降级为"软乘除"，HW M 推迟 v1.0.0
- `dynamic_cast` 出现 >0 处（ADR-082 落地失败）→ 重做 negotiate API 单点
- stall 期间 IPC 回归 >10% → 修 forward 网络（priority）→ 仍不达标 → 砍 multi-issue 分叉，single-issue 路线

## 实施窗口

- **启动条件**: `[cpu-l1-mmu-demo]` 6/6 PASS（**唯一 hard prerequisite**）
  - `plugin-framework-cycle-precision` 不再是启动条件: mfc `tasks.md:11` 明示"不依赖", frontmatter `depends_on` 是声明性元数据（实际消费在 Phase F, 见 mfc `tasks.md:178-182` + 本 proposal §5 line 91-95）
  - **Phase F 消费点**: 通过 framework cycle-precision 验证 MUL=1c / DIV=33c / IPC 回归 ≤5%（详见 §5 line 91-95）
  - **Phase F 时点路径选择**: (b)-lite — 直接读 `tools/cpu_sim/main.cpp:230-242` `actual_cycles`（无需 framework API）；(a) — Phase F 启动前实装 cycle-precision。2026-09-29 默认采纳 (b)-lite
- **估时**：3-4 周（与 P1#6 mmu-config-json-driven 并行）
- **顺序**：先实现 FSM → 再 integrate ADR-082 → 最后 rv32um PASS 验证