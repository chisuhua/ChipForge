# Improvement: verilator-mul-div-fsm-followup

> **Status**: 📋 PROPOSED (rdd-builder-lite governance)
> **Initiative**: wave5-isa-coverage-and-bp
> **Priority**: P1
> **Version Target**: v0.11.0
> **Created**: 2026-10-13
> **Depends on**: chmem-verilator-rv32um-coverage (commit 0adeaa6 archived ✓)

---

## Why

`chmem-verilator-rv32um-coverage` 2026-10-13 P2 实测发现:8 个 rv32um ELF 在 Verilator runner 下
`TOHOST=33 CYCLES=48`,而非期望的 `TOHOST=1`。

| 路径 | 状态 |
|------|------|
| TLM (`test_rv32um_runner.cpp`) | 48/48 PASS (mul=455/mulh=504/div=380 cycles 与 CSV 基线 1:1) |
| CH_MEM Simulator (`chmem_vendored_elf_tohost1`) | 8/8 PASS (单跑隔离) |
| Verilator (`cpu_verilator_sim_tohost1`) | 8/8 **FAIL** (TOHOST=33) |

riscv-tests `fail` 路径 `ori gp, gp, 1; ... sw gp, 0(tohost) → tohost = test_num | 1`,mul 失败 (test_32 测试
`0x7e00 * 0xb6db6db7 != 0x1200`) 写 `gp = 32 | 1 = 33` 到 dmem。runner 期望 `tohost == 1`,看到 `33` 标 FAIL。

**TLM PASS + CH_MEM Simulator PASS + Verilator FAIL 三段对比** 暗示根因在 CH_MEM elaboration 后
Verilator `@(posedge clk)` 时序下 mul 行为有 boundary bug,或 runner 协议不严格遵循 riscv-tests
harness 实际行为(mask 0x1 vs raw `== 1`)。

---

## What Changes

### 1. 诊断阶段 (P2 中段)

**输入**: rv32um-p-mul 三段差异 (TLM PASS / CH_MEM sim PASS / Verilator FAIL)

**目标**: 区分是 (a) CppHDL elaboration bug、(b) Verilator @posedge 编译 bug、还是 (c) runner 协议 bug

**方法** (按优先级):
- **(c) 路径** — **最快、最小风险**:
  在 `tools/verilator_runner/sim_main.cpp:80` 改 `tohost = vtop->rootp->top__DOT__dmem[kToHostWord]`
  为 `tohost = vtop->rootp->top__DOT__dmem[kToHostWord] & 0x1`,严格按 riscv-tests harness
  `bnez gp, fail` 协议只看 LSB。重跑 8 rv32um ELF。

- **(a) 路径** — 若 (c) 不通:
  在 `ip/cpu/plugins/mul_div_fsm_chmem.h:121-134` 的 `sm.transition_when` / `transition_to` 链
  加 cycle-counter instrumentation,跑 Verilator 后读 `top.v` 中 `state_reg` / `result_reg` 时刻值,
  对比 CH_MEM Simulator 时序,定位 boundary cycle。

- **(b) 路径** — 最后手段 (影响面大):
  改 CppHDL `ch_reg` reset 行为(`build/_deps/install/include/cpp-hdl/core/reg.h`),
  库层修改需 CI 全面回归。

### 2. 修复阶段 (按诊断结果分支)

- **(c) 路径**: 改 `sim_main.cpp:80` mask `tohost = raw & 0x1`,1 行修复。
- **(a) 路径**: 改 `mul_div_fsm_chmem.h` 状态转移,CH_MEM 行为对齐 TLM radix-2 iterative。
- **(b) 路径**: 改 CppHDL `ch_reg` reset 行为 + 全面回归测试。

### 3. 文档 + 验证

- 更新 `AGENTS.md` `## 已知测试状态` 段 — 数字绑定实测 commit
- 更新 `CHANGELOG.md` v0.11.0 段 — 修复 entry

---

## How

### 步骤 1 — Improvement 创建 + governance rdd-builder-lite

生成此 5 段 improvement 文档;governance 选 rdd-builder-lite 因为改动域极小 (≤ 3 文件候选)。

### 步骤 2 — Plan 生成 (`.rddf/plans/verilator-mul-div-fsm-followup.md`)

参考 `.rddf/plans/mfc-extract-fsm-h.md` 模板,生成 TDD 5-step plan。

### 步骤 3 — Execute

按 (c) 路径先 try,失败 fallback (a)。

### 步骤 4 — Verify

- [chmem] chmem_vendored_elf_tohost1: 46 DYNAMIC_SECTION 全 PASS (不退化)
- [verilator] cpu_verilator_sim_tohost1: 48 DYNAMIC_SECTION 全 PASS (8 rv32um + 40 rv32ui)
- [riscv-tests] 48/48 PASS (不退化)
- 架构门禁 3 件套全 PASS

### 步骤 5 — Archive

`openspec archive verilator-mul-div-fsm-followup --yes`

---

## Acceptance

- [ ] **AC-1**: 根因明确定位 (a/b/c 之一),附 Verilog/Simulator log 证据
- [ ] **AC-2**: 修复后 [verilator] 48 DYNAMIC_SECTION 全 PASS (含 8 rv32um)
- [ ] **AC-3**: [chmem] 46 DYNAMIC_SECTION 全 PASS (不退化)
- [ ] **AC-4**: [riscv-tests] 48/48 PASS (不退化)
- [ ] **AC-5**: 架构门禁 verify_adr 0 fail / decision 8/8 / portability 12/12
- [ ] **AC-6**: AGENTS.md + CHANGELOG 数字绑定实测 commit

---

## Capabilities

### MUST
- 修复后 8 rv32um ELF 在 Verilator runner 下 `TOHOST=1`
- TLM / CH_MEM sim / Verilator 三路径行为契约对齐 (ADR-040 v2.0 双模同语义)

### MUST NOT
- 不修改 CppHDL 库 `ch_reg` reset 行为 (除非 (a)/(c) 都失败且 (b) 是唯一根因)
- 不放宽 `[riscv-tests]` 48/48 PASS 基线
- 不改 runner 协议 raw `== 1` 为 `== 1 || == 33` 等或运算 — 必须用 mask 对齐 riscv-tests harness 行为

---

## Impact

- **代码** (按诊断分支):
  - `(c)` 路径: `tools/verilator_runner/sim_main.cpp` (1 行修复)
  - `(a)` 路径: `ip/cpu/plugins/mul_div_fsm_chmem.h` (state transition 修复)
  - `(b)` 路径: `build/_deps/install/include/cpp-hdl/core/reg.h` (库层,影响面大)
- **测试**: `tests/cpu/test_cpu_verilator_sim.cpp` (不动) + `test_cpu_chmem_vendored_elf.cpp` (不动)
- **docs**: `AGENTS.md` `## 已知测试状态` 段刷新 (8 rv32um FAIL → 8 rv32um PASS)
- **runtime**: [verilator] family 单 case 30-60s (全套 ~6m+)
- **risk**:
  - R1: 若走 (b) 路径,影响 CppHDL 库所有 `ch_reg` 用户(全项目)
  - R2: 候选根因 RCA 不够深入,可能错过 boundary cycle
  - R3: mask `& 0x1` 是协议变更,需在 runner 注释中说明 riscv-tests harness 语义