---
id: feat-dhrystone-dmips-gate
kind: feature
status: active
phase_refs: [phase-4]
主题: Dhrystone DMIPS/MHz ≥1.4 硬门禁 (mfc Phase G)
---

## 概述
v0.10.0 收官的硬门禁：Dhrystone benchmark DMIPS/MHz ≥ 1.4（与 VexiiRiscv single-issue 官方 ~1.4 对拍）。当前 DMIPS/MHz = 0.0351（差 40x），需 mfc Phase G 真 radix-2 + fetch stall + BP 协同达成。

## 跨阶段拆分

### phase-4 (v0.10.0 收官期)
- mfc Phase G 真 radix-2 iterative 实装（依赖 mfc-extract-fsm-h）
- fetch stall framework 扩展（依赖 mfc-extract-fsm-h）
- BP 协同：pc_reg mux / stall / fetch stage 协议
- Dhrystone benchmark 集成到 `[dhrystone]` test family
- 当前 [cpu-integration][dhrystone] 60s+ timeout (commit 76ac53f "确认 livelock")，需先修

## 验收标准
- [ ] [cpu-integration][dhrystone] PASS（无 livelock，cycle ≤ 1M）
- [ ] DMIPS/MHz ≥ 1.4（与 VexiiRiscv single-issue 偏差 ≤ 10%）
- [ ] [riscv-tests] rv32um 8/8 PASS（MUL/DIV 真 radix-2 间接验证）
- [ ] 4 architecture gates 0 error
- [ ] `bash tools/run_chipforge_tests.sh --all` 全 PASS

## 风险（关联 R5 CI 豁免爆发）
- R5 触发信号: `CF_PLUGIN_USE_FSM_EXEMPT` + waiver 标记 > 20 处
- 缓解: MUL/DIV FSM 使用 `ch_state_machine` DSL（ADR-046 豁免白名单）不计入豁免数
