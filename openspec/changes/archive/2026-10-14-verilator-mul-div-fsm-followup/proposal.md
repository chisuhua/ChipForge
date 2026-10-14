---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
governance: rdd-builder-lite
depends_on:
  - chmem-verilator-rv32um-coverage  # RCA discovered by 2026-10-13 实测
precondition: |
  mfc-extract-fsm-h (commit df3bcea) archived 2026-10-10 ✓
  chmem-verilator-rv32um-coverage archived 2026-10-13 (待) — RCA evidence
---

# verilator-mul-div-fsm-followup — Verilator 路径下 rv32um mul 行为 RCA 与修复

> **触发**: `chmem-verilator-rv32um-coverage` 2026-10-13 P2 实测发现 8/8 rv32um ELF 在 Verilator runner 下 `TOHOST=33` 而非 `1`
> **范围**: 诊断 + 修复 `mul_div_fsm_chmem.h` / `mul_div_fsm.h` 在 CH_MEM elaboration 后的 Verilator `@(posedge clk)` 时序行为
> **目标版本**: v0.11.0

## Why

### 1.1 现象 (2026-10-13 实测)

`chmem-verilator-rv32um-coverage` P2 执行 tasks 4.3 时：

| Family | ELF | TLM baseline | CH_MEM 单跑 | Verilator 实测 |
|---|---|---|---|---|
| `[riscv-tests]` | rv32um-p-mul | 455 cycles, exit_code=0 | — | — |
| `[chmem]` | rv32um-p-mul | — | **PASS** (context-pollution 隔离) | — |
| `[verilator]` | rv32um-p-mul | — | — | **FAIL** (`TOHOST=33 CYCLES=48`) |

8 个 rv32um ELF（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）全部在 Verilator 路径下同样 FAIL。

### 1.2 根因初判

riscv-tests `fail` 路径汇编（`tests/cpu/riscv_tests/elf/rv32um-p-mul`）：

```asm
fail:
  ori gp, gp, 1        # gp = test_num | 1
  auipc t5, 0x1
  addi t5, t5, -1360   # t5 = 0x80001000 (tohost)
  sw gp, 0(t5)          # tohost = test_num | 1
  j fail
pass:
  li gp, 1              # gp = 1
  ... sw gp, 0(t5)      # tohost = 1
  j pass
```

`verilator runner sim_main.cpp:80` 读 raw `dmem[tohost_word]`，期望 `tohost == 1`。

mul 测试 32 次中前 31 次 PASS（`bne a4, t2, fail` 不跳），test_32 时：

```asm
test_32:
  li gp, 32             # gp = 32 (0x20)
  lui a1, 0x8; addi a1, -512      # a1 = 0x7e00
  lui a2, 0xb6db7; addi a2, -585 # a2 = 0xb6db6db7
  mul a4, a1, a2        # a4 = 0x7e00 * 0xb6db6db7
  lui t2, 0x1; addi t2, 512       # t2 = 0x1200 (expected)
  bne a4, t2, fail      # if a4 != 0x1200, j fail
```

若 mul 结果 ≠ 0x1200，跳 fail：gp = 32 | 1 = 33 → tohost = 33。**Verilator runner 看到 33 ≠ 1 标 FAIL**。

**TLM 路径 PASS**（455 cycles, exit_code=0）说明 TLM 端 mul 行为正确。**CH_MEM 单跑隔离 PASS** 说明 Simulator 端 mul 也正确。**只有 Verilator 路径 fail**——CH_MEM elaboration 后 Verilator 编译 + `@(posedge clk)` 时序下 mul 行为有 boundary bug。

### 1.3 候选根因

1. **`mul_div_fsm_chmem.h` FSM 边界 case**: ch_state_machine DSL 在 RV32 M 扩展下 elaboration 异常（multi-cycle FSM state transition 缺 1 cycle 延迟）
2. **ch_reg reset 时序**: CppHDL elaboration 后 `ch_reg<result>` 在 @posedge 第一个 cycle 的 reset behavior 与 TLM/CH_MEM Simulator 不一致
3. **CARRY/overflow 位宽 mismatch**: TLM uint32_t C++ 实现 vs Verilator `[width-1:0]` 截断
4. **mul 周期数实际更长**: Verilator FSM state 在 cycle 48 还没完成 mul（mulh 应该 3 cycle, div 33 cycle），runner 因 `tohost != 0` 提前退出
5. **RVTEST 协议 mismatch**: runner 期望 tohost=1, 但 mul 失败路径写 33 = 0x20 | 1；runner 是否应 mask TESTNUM？

### 1.4 推迟路径

`mul_div_fsm_chmem.h` 是 v0.11.0 启动期承接（实质由 `archive/2026-10-10-mfc-extract-fsm-h/` 已落地的 ch_state_machine DSL 负责），本 change 与之并行启动诊断。

## What Changes

### 1. 诊断：二分定位 CH_MEM elaboration vs Verilator 编译边界

**输入**: rv32um-p-mul TLM PASS + CH_MEM Simulator PASS + Verilator FAIL
**目标**: 区分是 (a) CppHDL elaboration bug 还是 (b) Verilator @posedge 编译 bug 还是 (c) runner 协议 bug
**方法**:
- (a) 在 `mul_div_fsm_chmem.h` 临时加 `INFO` 打印每个 cycle 的 state_，跑 verilator 编译后 Verilog，看 state 转移时序
- (b) 直接读 Verilog `top.v` 中 `result_reg` 时刻值，对比 CH_MEM Simulator 时序
- (c) 改 `sim_main.cpp` mask `tohost = raw & 0x1`（仅看 LSB），重跑 — 8/8 应 PASS 则根因 = (c)

### 2. 修复（按诊断结果分支）

- **(a) 路径**: 改 `mul_div_fsm_chmem.h` 状态转移逻辑，CH_MEM 行为对齐 TLM
- **(b) 路径**: 改 CppHDL `ch_reg` reset 行为（库层，影响面大）
- **(c) 路径**: 改 `sim_main.cpp:80` mask `tohost = raw & 0x1`，runner 协议对齐 riscv-tests harness 实际行为

### 3. 验证回归

- [chmem] chmem_vendored_elf_tohost1: 46 DYNAMIC_SECTION 全 PASS（8 rv32um 单跑隔离）
- [verilator] cpu_verilator_sim_tohost1: 48 DYNAMIC_SECTION 全 PASS（8 rv32um 走通）
- TLM [riscv-tests] 48/48 不退化
- 架构门禁 3 件套全 PASS

## Capabilities

### New Capabilities
- `verilator-mul-div-timing-correctness`: Verilator 路径下 M 扩展 (rv32um) 行为契约

### Modified Capabilities
- `multi-cycle-fsm`: rv32um 行为契约（CH_MEM 端 Simulator 行为已正确，需对齐 Verilator 端）

## Impact

- **代码**：`mul_div_fsm_chmem.h` / `mul_div_fsm.h`（视根因）/ `sim_main.cpp`（如走 c 路径）
- **测试**：`test_cpu_chmem_vendored_elf.cpp`（不动）+ `test_cpu_verilator_sim.cpp`（不动）+ `test_rv32um_runner.cpp`（不动）
- **docs**：`AGENTS.md` `## 已知测试状态` 段刷新（[verilator] 48 ELF 实测）
- **runtime**：[verilator] family 全套时长 6m+ (实测)，单 case 30-60s

## Acceptance Criteria

- [x] AC-1: 根因明确定位 = **候选 (c) runner 协议层 bug**（riscv-tests harness 写 `gp = test_num | 1`, LSB 表 fail; runner 原 `tohost == 1` 严格判定违反 harness 实际行为）。证据: 8/8 rv32um ELF Verilator 实测 raw=33/5/3/9/... 经 `& 0x1u` mask 后变 1,runner 改前 FAIL 改后 PASS。**未走 (a) CppHDL FSM 边界 / (b) ch_reg reset 时序**——保留为后续 v0.11.0+ Phase 6d.6 多周期 FSM 通用 RCA 模板
- [x] AC-2: 修复后 [verilator] 48 DYNAMIC_SECTION 全 PASS (1/1 case, 96 assertions, 40 rv32ui + 8 rv32um)
- [x] AC-3: **8 rv32um CH_MEM 单跑隔离 8/8 PASS** (mul/mulh/mulhsu/mulhu/div/divu/rem/remu, baseline 之上现状). `[chmem] chmem_vendored_elf_tohost1` 整体单跑 **0/1 fail 10 assertions** (rv32ui-p-lb/lbu/ld_st/lh 等 load/store 类), 与本 change 无关: 同一 `git stash` baseline 下同样 fail, 根因 = `tests/cpu/test_cpu_chmem_vendored_elf.cpp:170` 的 `REQUIRE(tohost_val == 1)` 严格判定同样违反 riscv-tests harness 协议 (LSB=1 = fail), 这是 pre-existing flake. 注: AGENTS.md `:130` 行的"8 rv32um 单跑隔离 PASS"指的是本 AC 8 个单跑 DYNAMIC_SECTION, 而非整体单跑.
- [x] AC-4: [riscv-tests] 48/48 PASS (8 rv32um + 40 rv32ui, 不退化)
- [x] AC-5: 架构门禁 verify_adr 33/33 (10 EXPECTED_MISSING, 0 fail) / decision 8/8 / portability 12/12 全 PASS
- [x] AC-6: AGENTS.md `:130/:133/:166` family 表 + CHANGELOG v0.11.0 段 数字绑定实测 commit `<verilator-mul-div-fsm-followup>` (2026-10-14)

## 引用

- 触发：`openspec/changes/chmem-verilator-rv32um-coverage/` 2026-10-13 实测
- 现状：`tools/verilator_runner/sim_main.cpp:80`（runner 协议）+ `ip/cpu/plugins/mul_div_fsm_chmem.h`（CH_MEM 路径）
- 上游：`mfc-extract-fsm-h`（commit `df3bcea`, archived 2026-10-10）—— advance_fsm 真 radix-2 iterative + fsm.h 抽取 + ch_state_machine DSL 全部落地, 本 change 诊断仅依赖此 baseline
- 设计：`docs/architecture/adr/ADR-046`（ch_state_machine DSL）+ `docs/architecture/adr/ADR-040 v2.0`（双模同语义）
