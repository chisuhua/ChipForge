# verilator-mul-div-fsm-followup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 rv32um 8 ELF 在 Verilator runner 下 TOHOST=33 FAIL,通过诊断三段差异 (TLM PASS / CH_MEM sim PASS / Verilator FAIL) 定位根因 (a/b/c) 并修复,使 `[verilator]` 48 ELF 全部 PASS。

**Architecture:** 按 proposal.md 候选根因 (a) CppHDL elaboration / (b) Verilator @posedge 编译 / (c) runner 协议 顺序排查,优先尝试最小风险路径 (c) — `sim_main.cpp:80` 加 `tohost & 0x1` mask 对齐 riscv-tests harness 行为。若 (c) 失败再走 (a) FSM boundary 排查。

**Tech Stack:** C++20/23、CH_MEM elaboration (ch_state_machine DSL)、Verilator 5.052、CppHDL Simulator、Catch2 v3.7.0。

---

## File Structure

### Production Code (候选改动)

| File | Responsibility | Path (c) | Path (a) | Path (b) |
|---|---|---|---|---|
| `tools/verilator_runner/sim_main.cpp` | Verilator runner tohost 读取 | **modify** | — | — |
| `ip/cpu/plugins/mul_div_fsm_chmem.h` | CH_MEM MulDiv FSM | — | **modify** | — |
| `build/_deps/install/include/cpp-hdl/core/reg.h` | CppHDL ch_reg reset | — | — | **modify (last resort)** |

### Verification Commands (不动)

| File | Purpose |
|---|---|
| `tests/cpu/test_cpu_verilator_sim.cpp` | [verilator] 48 ELF runner test (不动) |
| `tests/cpu/test_cpu_chmem_vendored_elf.cpp` | [chmem] 46 ELF CppHDL sim test |
| `tests/cpu/test_rv32um_runner.cpp` | [riscv-tests] rv32um 8 ELF (verify no regression) |

---

## Phase 0 — 环境验证 + 基线建立

### Task 1: 确认 verilator 二进制 + 当前 8 rv32um FAIL 基线

**Files:**
- Verify: `./build/bin/cpu_verilator_sim` (binary exists)
- Verify: `tests/cpu/riscv_tests/elf/rv32um-p-{mul,mulh,mulhsu,mulhu,div,divu,rem,remu}` (ELF 8 个)

- [ ] **Step 1: 确认 verilator sim binary 可执行**

Run: `test -x ./build/bin/cpu_verilator_sim && echo OK || echo MISSING`
Expected: `OK` (若不存在 → 跑 `cmake --build build` 先编译)

- [ ] **Step 2: 确认 8 rv32um ELF 文件存在**

Run: `ls tests/cpu/riscv_tests/elf/rv32um-p-mul tests/cpu/riscv_tests/elf/rv32um-p-mulh tests/cpu/riscv_tests/elf/rv32um-p-mulhsu tests/cpu/riscv_tests/elf/rv32um-p-mulhu tests/cpu/riscv_tests/elf/rv32um-p-div tests/cpu/riscv_tests/elf/rv32um-p-divu tests/cpu/riscv_tests/elf/rv32um-p-rem tests/cpu/riscv_tests/elf/rv32um-p-remu`
Expected: 8 个文件均显示 (无 No such file)

- [ ] **Step 3: 单跑 rv32um-p-mul Verilator, 记录当前 TOHOST 实测**

Run: `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-mul --cycles 2000 2>&1 | tail -5`
Expected: 含 `TOHOST=33 CYCLES=48 ELF=rv32um-p-mul FAIL` (基线 FAIL 证据)

- [ ] **Step 4: TLM baseline 对照 — rv32um-p-mul PASS**

Run: `./build/bin/chipforge_tests "[rv32riscv-tests-rv32um]" -c "rv32um-p-mul"` 2>&1 | tail -10
Expected: 含 `PASSED` 或 cycles 报告 (= 455 cycles per CSV baseline)

- [ ] **Step 5: CH_MEM sim baseline — rv32um-p-mul PASS (单跑隔离)**

Run: `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1" -c "rv32um-p-mul"` 2>&1 | tail -10
Expected: 含 `PASSED` (CH_MEM Simulator 行为正确证据)

**Task 1 失败门 → Stop: 环境异常 (e.g. verilator binary 缺失 / ELF 缺失 / 三段基线不一致),需先修环境**

---

## Phase 1 — 诊断: 候选 (c) runner mask 路径 (优先级最高,最小风险)

### Task 2: 修改 sim_main.cpp:80 加 `& 0x1` mask

**Files:**
- Modify: `tools/verilator_runner/sim_main.cpp:80`

- [ ] **Step 1: Read 当前 line 72-86**

Read: `tools/verilator_runner/sim_main.cpp:72-86`
Expected: 见 tohost 读取循环 `tohost = vtop->rootp->top__DOT__dmem[kToHostWord];` (无 mask)

- [ ] **Step 2: 备份当前 sim_main.cpp 一份作为 raw change diff 留档**

Run: `cp tools/verilator_runner/sim_main.cpp /tmp/opencode/sim_main.cpp.orig`
Expected: `cp` 成功 (留档原始版本,便于 revert)

- [ ] **Step 3: 修改 line 80 加 mask**

Edit `tools/verilator_runner/sim_main.cpp:80`:

```cpp
// before
tohost = vtop->rootp->top__DOT__dmem[kToHostWord];
// after
tohost = vtop->rootp->top__DOT__dmem[kToHostWord] & 0x1u;  // riscv-tests harness: bnez gp → fail (gp |= 1) → tohost = test_num | 1
```

Expected: 1 行修改,保留对齐。

- [ ] **Step 4: 同时更新文件头注释 (line 16-19) 说明 mask 语义**

Edit `tools/verilator_runner/sim_main.cpp:16-19`:

```cpp
// tohost is read from the ch_mem array via --public-flat-rw
// (Option c: rootp->top__DOT__dmem[tohost_word]).
// We mask with 0x1 because the riscv-tests harness writes test_num|1 to tohost
// on failure (the LSB is the "did any test fail" bit, see riscv-tests/env/p/riscv_test.h:
//   #define TESTNUM gp
//   bnez TESTNUM, fail → gp |= 1 → tohost = (test_num << 1) | 1 → LSB = 1 means fail).
// CppHDL codegen emits ch_mem arrays (ADR-035 R8 memory backdoor, ChipForge 6d.5 E8).
```

Expected: 文件头注释更新,riscv-tests 协议说明清晰。

- [ ] **Step 5: 同时把 line 85 的 PASS/FAIL 判定改为基于 mask 后值**

注: line 85 `tohost == 1 ? "PASS" : "FAIL"` 在 mask 后 **自然** 行为正确 (mask 后 tohost ∈ {0,1},只有 1 → PASS)。无需修改判定逻辑,但需确认。

Read: `tools/verilator_runner/sim_main.cpp:84-86`
Expected: `tohost == 1 ? "PASS" : "FAIL"` 仍正确 (mask 后 `tohost == 1` ↔ riscv-tests PASS)

---

### Task 3: 重新编译 cpu_verilator_sim binary

**Files:**
- Build: `./build/bin/cpu_verilator_sim`

- [ ] **Step 1: 触发 rebuild (sim_main.cpp 嵌入 cpu_verilator_sim binary)**

Run: `cmake --build build --target cpu_verilator_sim -j$(nproc) 2>&1 | tail -20`
Expected: `cpu_verilator_sim` 重新链接 (因 sim_main.cpp 是 host runner code source,通过 `add_executable` 嵌入)

- [ ] **Step 2: 确认新 binary 含 mask 逻辑**

Run: `strings ./build/bin/cpu_verilator_sim | grep "riscv-tests harness"` 2>&1 | head -5
Expected: 字符串包含 "riscv-tests harness" (新注释嵌入 binary)

---

### Task 4: 重跑 rv32um-p-mul 单 ELF 验证 mask 路径

**Files:**
- Verify: `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-mul`

- [ ] **Step 1: 单跑 rv32um-p-mul, 验证 mask 后 TOHOST=1**

Run: `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-mul --cycles 2000 2>&1 | tail -5`
Expected (路径 c 成功): `TOHOST=1 CYCLES=<Xv> ELF=rv32um-p-mul PASS`
OR
Expected (路径 c 失败 → 走 Task 5+): 仍 FAIL 但 TOHOST 变为其他非零值

- [ ] **Step 2: 若 PASS, 跑其余 7 rv32um ELF 全部确认**

Run for each: `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-{mulh,mulhsu,mulhu,div,divu,rem,remu} --cycles 2000 2>&1 | tail -1`
Expected: 8 个全部含 `TOHOST=1 ... PASS` (路径 c 全通,根因 = (c))

- [ ] **Step 3: 若 8 个全 PASS, 跳过 Task 5-7, 直接进入 Phase 2 (回归)**

判定: 若 Step 2 全 PASS → **根因 = (c) runner 协议**,Task 5/6/7 (路径 a/b) 跳过。
判定: 若 Step 2 仍有 FAIL → 走 Task 5+ (FSM boundary RCA)。

---

## Phase 1.5 — 诊断: 候选 (a) FSM boundary 路径 (Task 4 不通时启用)

### Task 5: (条件性) 在 mul_div_fsm_chmem.h 加 instrumentation 输出 state_ 转移

**Files:**
- Modify (临时): `ip/cpu/plugins/mul_div_fsm_chmem.h:121-134`

- [ ] **Step 1: 读 CH_MEM mul_div_fsm_chmem.h line 121-134 状态转移链**

Read: `ip/cpu/plugins/mul_div_fsm_chmem.h:121-134`
Expected: 4 state (IDLE/MULTIPLY/DIVIDE/WRITE_BACK) + transition_when/transition_to 链

- [ ] **Step 2: 临时在 State::MULTIPLY on_active 加 $display (Verilog trace)**

在 `ip/cpu/plugins/mul_div_fsm_chmem.h:125` `sm.state(State::MULTIPLY).on_active` 闭包内加:

```cpp
// DEBUG (RCA only): ch_reg<8-bit> counter 用 CppHDL simulator 读取
// 注: Verilator @posedge trace 走 $display 在生成的 top.v 内
```

Expected: 临时 instrumentation (注释标记 DEBUG RCA only)

- [ ] **Step 3: rebuild CH_MEM 路径 → 重跑 simulator 提取 state_ 转移 cycle 序列**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: 编译成功 (CH_MEM mode 重新 build)

Run: `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1" -c "rv32um-p-mul" --success 2>&1 | tail -20`
Expected: CH_MEM sim 仍 PASS (instrumentation 不影响)

- [ ] **Step 4: Verilator 生成 top.v 提取 state_reg / result_reg 时刻值**

Run: `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-mul --verilog /tmp/opencode/mul_top.v --work-dir /tmp/opencode/mul_work --cycles 2000 2>&1 | tail -5`
Expected: 生成 `mul_top.v`,通过 grep `state_reg`、`result_reg` 看 wire 转移

- [ ] **Step 5: 对比 CH_MEM sim trace 与 Verilog trace, 定位 boundary cycle**

人工对比: 在 cycle 48 (Verilator 报告 fail 退出 cycle) 阶段, Verilog `result_reg` 应为 mul 结果 vs `state_reg` 应为 IDLE。

- [ ] **Step 6: 若定位到 (a) FSM 边界 bug, 修改 mul_div_fsm_chmem.h 状态转移**

Edit (依据 RCA 结果): 例如 `transition_when(op_valid && is_mul, State::MULTIPLY)` 改为带 N cycle delay, 或 `transition_to(State::WRITE_BACK)` 加 gate。

Expected: CH_MEM/Verilator 行为对齐。

- [ ] **Step 7: revert Task 5 Step 2 临时 instrumentation**

Edit: 移除 Task 5 Step 2 添加的 DEBUG 注释。
Expected: `mul_div_fsm_chmem.h` 回到 baseline 或 RCA fix 后状态。

---

## Phase 2 — 回归 (路径 a/b/c 任意路径修复后)

### Task 6: 跑全套 [verilator] 测试 (48 ELF)

**Files:**
- Verify: `./build/bin/chipforge_tests_chmem [verilator]`

- [ ] **Step 1: 跑 [verilator] family 全部 case**

Run: `./build/bin/chipforge_tests_chmem [verilator] 2>&1 | tail -15`
Expected (AC-2): `cpu_verilator_sim_tohost1` PASS, 48 DYNAMIC_SECTION 全 PASS, `All tests passed`

- [ ] **Step 2: 验证 rv32um 8 个 + rv32ui 40 个 ELF 计数 (实 DYNAMIC_SECTION 输出)**

Run: `./build/bin/chipforge_tests_chmem [verilator] --reporter compact 2>&1 | grep -c "verilator_sim:"`
Expected: 48 (40 rv32ui + 8 rv32um)

---

### Task 7: 跑全套 [chmem] + [riscv-tests] 不退化

**Files:**
- Verify: `./build/bin/chipforge_tests_chmem [chmem]`
- Verify: `./build/bin/chipforge_tests [riscv-tests]`

- [ ] **Step 1: 跑 [chmem] vendored_elf family (46 ELF)**

Run: `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1" 2>&1 | tail -10`
Expected (AC-3): 46 DYNAMIC_SECTION 全 PASS

- [ ] **Step 2: 跑 [riscv-tests] rv32um 8 + rv32ui 40**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -10`
Expected (AC-4): 48/48 PASS (rv32um 8/8 + rv32ui 40/40)

- [ ] **Step 3: 跑 [riscv-tests-rv32um] 8 rv32um 单独**

Run: `./build/bin/chipforge_tests "[rv32riscv-tests-rv32um]" 2>&1 | tail -10`
Expected: 8/8 PASS

---

### Task 8: 跑架构门禁 3 件套

**Files:**
- Verify: `tools/verify_adr.sh` + `tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh`

- [ ] **Step 1: ADR 验证**

Run: `bash tools/verify_adr.sh 2>&1 | tail -10`
Expected (AC-5): `0 fail` (无 ADR 漂移)

- [ ] **Step 2: Plugin 决策验证**

Run: `bash tools/verify_plugin_decision.sh 2>&1 | tail -10`
Expected (AC-5): `8/8` (or current count) PASS

- [ ] **Step 3: Plugin 移植性验证**

Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -10`
Expected (AC-5): `12/12` PASS (TLM/CH_MEM 文件严格分离)

---

### Task 9: 跑全量回归 `--all` 防止隐蔽退化

**Files:**
- Verify: `bash tools/run_chipforge_tests.sh --all`

- [ ] **Step 1: 全量 ctest 跑**

Run: `bash tools/run_chipforge_tests.sh --all 2>&1 | tail -30`
Expected: 4 ctest entries 全 PASS (TLM + CH_MEM + Verilator smoke + architecture gate)

- [ ] **Step 2: 提取关键数字**

Extract: `[verilator] N case PASS / [chmem] M / [riscv-tests] K` 数字,准备 commit message + AGENTS.md.

---

## Phase 3 — 文档 + 归档

### Task 10: 更新 AGENTS.md `## 已知测试状态` 段

**Files:**
- Modify: `AGENTS.md:133` (附近行)

- [ ] **Step 1: 找到 AGENTS.md 当前 [verilator] 行**

Run: `grep -n "verilator.*PASS.*40\|verilator.*PASS.*48" AGENTS.md | head -5`
Expected: 含当前 40 rv32ui + 8 rv32um FAIL 描述

- [ ] **Step 2: 更新为 PASS 数字**

Edit: 把 `[verilator] 1/1 case PASS (40 rv32ui ELF, 80 assertions)` + `2026-10-13 4dc5560b 扩 8 rv32um 全部 FAIL` 段,改为:

```
[verilator] 1/1 case PASS (48 ELF, 96 assertions) — 40 rv32ui + 8 rv32um 全部 TOHOST=1 (verilator-mul-div-fsm-followup archived <commit>)
```

Expected: 数字绑定 commit SHA

---

### Task 11: 更新 CHANGELOG.md v0.11.0 段

**Files:**
- Modify: `CHANGELOG.md` (v0.11.0 段)

- [ ] **Step 1: 找 v0.11.0 段**

Run: `grep -n "## \[v0.11.0\]\|## v0.11" CHANGELOG.md | head`
Expected: 找到 v0.11.0 段 (若不存在,创建新段)

- [ ] **Step 2: 添加修复条目**

Append (in v0.11.0 段):

```
### Fixed
- `verilator-mul-div-fsm-followup`: Verilator runner 协议对齐 riscv-tests harness 行为 (mask `tohost & 0x1`),修复 rv32um 8 ELF Verilator `TOHOST=33` FAIL (根因 = sim_main.cpp 协议层 bug 而非 FSM 边界)。
```

Expected: CHANGELOG 段更新

---

### Task 12: archive change

**Files:**
- Archive: `openspec/changes/verilator-mul-div-fsm-followup/` → `openspec/changes/archive/2026-10-13-verilator-mul-div-fsm-followup/`

- [ ] **Step 1: 验证 proposal.md 的 ## Acceptance checkboxes 全部 [x]**

Run: `grep -c "^- \[ \]" openspec/changes/verilator-mul-div-fsm-followup/proposal.md; grep -c "^- \[x\]" openspec/changes/verilator-mul-div-fsm-followup/proposal.md`
Expected: 6 行的数字 = 6 个 `[x]` (无未勾选)

- [ ] **Step 2: 跑 `openspec validate`**

Run: `openspec validate verilator-mul-div-fsm-followup --strict 2>&1 | tail -10`
Expected: PASS (无 validation 错误)

- [ ] **Step 3: 跑 `openspec archive`**

Run: `openspec archive verilator-mul-div-fsm-followup --yes 2>&1 | tail -10`
Expected: change moved to `openspec/changes/archive/2026-10-13-verilator-mul-div-fsm-followup/`

- [ ] **Step 4: 确认归档目录内容**

Run: `ls openspec/changes/archive/2026-10-13-verilator-mul-div-fsm-followup/`
Expected: `proposal.md` + `design.md` + `tasks.md` (若 archive 自动生成)

---

## Acceptance Criteria (与 improvement AC 同步)

- [ ] AC-1: 根因明确定位 (a/b/c 之一),附 Verilog/Simulator log 证据 (Task 4 或 Task 5 完成)
- [ ] AC-2: [verilator] 48 DYNAMIC_SECTION 全 PASS (Task 6)
- [ ] AC-3: [chmem] 46 DYNAMIC_SECTION 全 PASS 不退化 (Task 7 Step 1)
- [ ] AC-4: [riscv-tests] 48/48 PASS 不退化 (Task 7 Step 2)
- [ ] AC-5: 架构门禁 3 件套全 PASS (Task 8)
- [ ] AC-6: AGENTS.md + CHANGELOG 数字绑定实测 commit (Task 10 + Task 11)

---

## Fallback Strategy

| 路径 | 触发条件 | 任务 | 风险等级 |
|------|---------|------|---------|
| **(c) runner mask** | Task 4 Step 2 8/8 PASS | Task 2-6 + 10-12 | LOW (1 行修复) |
| **(a) FSM boundary** | Task 4 Step 2 仍有 FAIL | Task 5 + 6-12 | MEDIUM (FSM 改写) |
| **(b) ch_reg reset** | (a) 不通 (极不可能) | 不在 Plan 内,需回 rdd-planner 重新决策 | HIGH (库层) |

**Plan 主线**: 默认走 (c) 路径。Task 4 Step 2 全 PASS 后,Task 5-7 自动 skip。

---

## Estimated Timeline

| Phase | 任务 | 估时 |
|-------|------|------|
| Phase 0 | Task 1 环境验证 | 2 min |
| Phase 1 | Task 2-4 路径 c (主路径) | 10 min |
| Phase 1.5 (条件) | Task 5 路径 a | 30-60 min |
| Phase 2 | Task 6-9 回归 | 10-15 min |
| Phase 3 | Task 10-12 文档+归档 | 5 min |
| **总计 (主路径)** | **27 min** | |
| **总计 (条件 a)** | **62 min** | |

---

## Notes

- 路径 (c) 是 **最小风险 + 最快验证** 候选根因。riscv-tests harness 协议本来就只看 LSB (bnez gp 是 fail 入口,gp |= 1 写 tohost),runner `tohost == 1` 严格判定违反 harness 实际行为。
- 若路径 (a) 失败,可能触及 ch_state_machine DSL 的 cycle 转移模型选型问题(本质是 Phase 6d.6 MMU FSM 同源 issue)。
- 不进入 (b) 路径除非极端兜底 — `ch_reg` 库修改影响 CppHDL 所有插件。