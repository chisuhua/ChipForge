---
id: feat-rv32um-m-extension
kind: feature
status: active
phase_refs: [phase-4]
主题: RV32UM M 扩展真实 radix-2 覆盖 (rv32um 8/8)
---

## 概述
v0.10.0 PoC-1 收官：riscv-tests rv32um 8 个 ELF（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）全部 PASS。当前 0/8 FAIL（ip/cpu/arch/riscv/mul_div_fsm.h:475-500 advance_fsm 是 ad-hoc busy counter 非真 radix-2 iterative），需 mfc Phase H + mfc-extract-fsm-h 真 radix-2 实装。

## 跨阶段拆分

### phase-4 (v0.10.0 收官期)
- 真 radix-2 iterative advance_fsm（替代 ad-hoc busy counter）
- 共享 feat-mfc-multi-cycle-fsm 的 ch_state_machine FSM 框架
- 8 个 rv32um ELF 端到端 PASS：mul / mulh / mulhsu / mulhu / div / divu / rem / remu

## 验收标准
- [ ] [riscv-tests] rv32um 8/8 PASS（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）
- [ ] advance_fsm 真 radix-2 iterative 实装（与 cycle 数一致，1c/3c/33c 三模板）
- [ ] 间接：DMIPS/MHz ≥ 1.4（与 feat-dhrystone-dmips-gate 联动）
- [ ] 间接：[cpu-integration][dhrystone] PASS（修 livelock）
- [ ] 0 退化：[riscv-tests] rv32ui 40/40 维持

## 关联
- ADR-046: 多周期 FSM 豁免（CF_PLUGIN_USE_FSM_EXEMPT 机制）
- ADR-082: negotiate 集成（mul_div_fsm capability advertisement）
- PoC-1: MUL/DIV 1c/33c rv32um 100% (v0.10.0 硬指标)
- R1 BTB 风险：本 feature 收官后开启 v1.0.0 BP 窗口
