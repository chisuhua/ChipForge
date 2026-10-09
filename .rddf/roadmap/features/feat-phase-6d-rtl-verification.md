---
id: feat-phase-6d-rtl-verification
kind: feature
status: active
phase_refs: [phase-4]
主题: Phase 6d 5-stage Pipeline CH_MEM + Verilator + MMU/PTW FSM
---

## 概述
Phase 6d 端到端：把 Phase 6c 框架能力（CH_MEM elaboration + Verilog 生成 + Simulator）在真实业务代码上完整化。8 子阶段（6d.1-6d.8），10-12 周，是 v0.8.0/v0.10.0/v0.11.0 多个版本节点的硬前置。

## 跨阶段拆分

### phase-4 (v0.10.0 收官期)
- 6d.1 DecoderPlugin 完整 CH_MEM（1 周）
- 6d.2 BranchPlugin + HazardPlugin 完整 CH_MEM（1.5 周）
- 6d.3 CpuFactoryChmem 完整 5-stage 集成 + ibus_chmem.h / dmem_chmem.h（1.5 周）
- 6d.4 riscv-tests RV32I 5 指令（add/addi/auipc/jal/beq）端到端 tohost=1 CppHDL sim PASS（2 周）
- 6d.5 Verilator 后端集成（Verilog → Verilator 编译 → VL1Cache/VRegFile 替代 C++ sim，1.5 周）
- 6d.6 MMU/PTW FSM（ch_state_machine + CF_PLUGIN_USE_FSM_EXEMPT，sv32 5 状态 IDLE→L0_WAIT→L1_WAIT→DONE→FAULT，1 周）
- 6d.7 L1Cache refill FSM（4 状态 IDLE→LOOKUP→MISS→REFILL_WAIT，0.5 周）
- 6d.8 Harness 迁移（pb.run() → CppHDL sim runner / Verilator，1 周）

## 验收标准
- [ ] E1-E14 全部 PASS（详见 docs/architecture/roadmap-evolution.md §1）
- [ ] E3: riscv-tests RV32I 5 指令 tohost=1 CppHDL sim 端到端 PASS
- [ ] E4: 生成 cpu.v 含 5-stage 完整 Verilog
- [ ] E5: 8/8 check_plugin_portability.sh PASS（含新增 Check 9: _chmem.h 必须有 CF_PLUGIN_USE_FSM_EXEMPT 标注 if FSM）
- [ ] E8: Verilator sim 跑 riscv-tests add.elf tohost=1 与 CppHDL sim byte-equal
- [ ] E12: chipforge_tests TLM baseline 0 回归
- [ ] E13: chipforge_tests_chmem 完整 5-stage + riscv-tests + Verilator 全 PASS
- [ ] E14: CHANGELOG v0.10.0/v0.11.0 entry + ADR-037 v2.0 Accepted 状态 + ADR-040 v3.0 (Verilator 集成段)
- [ ] 间接：与 feat-rv32um-m-extension 共享 MUL/DIV 路径
- [ ] 间接：v0.10.0 PoC-2 RV32C ≥95% + PoC-3 ICache ≥90%
