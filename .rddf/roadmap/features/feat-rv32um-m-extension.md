---
id: feat-rv32um-m-extension
kind: feature
status: active
scope: cpu
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

### CH_MEM datapath 侧（v0.11.0, `chmem-mext-comb-datapath`）
- **owner 缺口已补齐**：CH_MEM 路径（`-DCF_PLUGIN_USE_CH_MEM`）此前**完全没有** RV32M 实现 —— `ip/cpu/arch/riscv/int_alu_chmem.h` 只有 11 层 RV32I select 树，不识别 `funct7=0x01`；`MulDivFsmPlugin` 未注册到 `cpu_factory_chmem.h`。
- **落地方案 = 组合式**：在 `int_alu_chmem.h` 追加 8 层 M-op select（合计 19 层），用 `ch_uint` + sign-magnitude（CppHDL 无 `ch_int`）实现全部 8 条 M-op；除/模用 safe divisor 显式 guard 除零（CppHDL 除零默认返回 0，RISC-V 要求 DIV/DIVU → 全 1、REM/REMU → 被除数）。
- **不注册 MulDivFsmPlugin、不做 FSM**：CH_MEM 是全组合通路，无 stage 寄存器 → 组合（1 拍）与多周期（N 拍）之间无中间态；cycle-accurate 版本移交 follow-up `chmem-mext-fsm-cycle-parity`（blocked on CtrlLink 句柄扩展）。
- **实测**（2026-10-20）：`[chmem] chmem_vendored_elf_tohost1` 8 个 rv32um ELF 全部 `tohost == 1`（真 PASS，非 mask 假绿）；cycle 数 mul/mulh/mulhsu/mulhu = 472、div = 94 / divu = 95 / rem = 94 / remu = 94 —— 与 TLM radix-2 baseline（455/504/380/381/380/380）**不同构，不可比**。

## 验收标准
- [x] [riscv-tests] rv32um 8/8 PASS（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）
- [x] advance_fsm 真 radix-2 iterative 实装（与 cycle 数一致，1c/3c/33c 三模板）
- [x] CH_MEM 侧 8 rv32um ELF 全 PASS（`chmem-mext-comb-datapath`，组合式实现）
- [ ] 间接：DMIPS/MHz ≥ 1.4（与 feat-dhrystone-dmips-gate 联动）
- [ ] 间接：[cpu-integration][dhrystone] PASS（修 livelock）
- [ ] 0 退化：[riscv-tests] rv32ui 40/40 维持
- [ ] CH_MEM cycle parity（移交 `chmem-mext-fsm-cycle-parity`）

## 关联
- ADR-046: 多周期 FSM 豁免（CF_PLUGIN_USE_FSM_EXEMPT 机制）
- ADR-082: negotiate 集成（mul_div_fsm capability advertisement）
- PoC-1: MUL/DIV 1c/33c rv32um 100% (v0.10.0 硬指标)
- R1 BTB 风险：本 feature 收官后开启 v1.0.0 BP 窗口
