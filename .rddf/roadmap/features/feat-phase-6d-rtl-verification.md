---
id: feat-phase-6d-rtl-verification
kind: feature
status: active
scope: cross
phase_refs: [phase-4]
主题: Phase 6d 5-stage Pipeline CH_MEM + Verilator + MMU/PTW FSM (跨 cpu/mmu/cache)
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

## 已知问题（owned）

### CH_MEM dmem 读路径 1-cycle 滞后 → 10 个 rv32ui load/store ELF FAIL

- **状态**：RCA 已闭环（2026-10-20），**修复未实施**
- **归位 change**：`openspec/changes/archive/2026-10-20-chmem-dmem-read-latency/`
- **能力 spec**：`openspec/specs/chmem-dmem-read-latency/spec.md`
- **违约方**：**ChipForge**（上游 CppHDL 行为 by-contract，R3/R4 明确记录该 drift）
- **根因**：上游 `mem-read-timing` R3 要求 sync+async 读端口都归 `sequential_instr_list_`
  ⇒ Simulator 的 `ch_mem::aread` 相对生成 Verilog 的连续 `assign` 滞后 1 拍。CH_MEM 数据通路
  全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器）⇒ regfile 在同一次 `eval_sequential()`
  内先于读端口锁存（regfile node id 702 < `load_read` 1707）⇒ **load 的结果 = 上一个访存
  地址的内容**。
- **失败面**：39 个 rv32ui 中 10 个 FAIL（`lb`/`lbu`/`ld_st`/`lh`/`lhu`/`lw`/`sb`/`sh`/`sw`/`st_ld`），
  其余 29 个 + 8 个 rv32um 全 PASS（反汇编逐 ELF 统计 load 数：FAIL 组 19~193，PASS 组全 0）
- **跨后端**：`[verilator]` 同批 48 ELF 全 PASS ⇒ 差异在后端相位分类，不在插件逻辑
- **回归锚**：`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`（`[mem-drift]`）钉死 1 拍漂移的
  **幅度**（观测值 == 上一拍地址内容）
- **修复方向**（2026-10-21 实测后更新）：**方案 B（上游改）已被实测确认为唯一"零 ChipForge
  数据通路改动"的解法** —— 上游 `async_read` 归组合 + 数据流边 ⇒ ChipForge `[chmem]`
  **10 → 0 FAIL**、47 个 vendored ELF 全 PASS、`[verilator]` 不变、TLM 不受影响。
  ⚠️ 方案 A（dmem 改 `sread`）单独实施**反而恶化**（10 → 11 FAIL，10 个 ELF 依旧全红），
  且必须伴随 CH_MEM 数据通路级改造（load-use stall + RMW 写通路）⇒ **放弃**。
  方案 C（上游加显式 backend 开关）保留为"上游只能小步改"时的备选。
- **上游 change**：`CppHDL` 仓库 `openspec/changes/sim-mem-read-timing-align-2026-10/`
  （含 `mem-read-timing` spec delta：RENAMED ×2 + MODIFIED ×2；8 个 task）
- **ChipForge 侧待办**（上游落地后）：升级依赖 → 重跑 `[chmem]` 全套 →
  把 `[mem-drift]` pin 从"钉死已知缺陷"改为"确认 skew 已消除"
- **阻塞**：E13（`chipforge_tests_chmem` 完整 5-stage + riscv-tests + Verilator 全 PASS）

## 验收标准
- [ ] E1-E14 全部 PASS（详见 docs/architecture/roadmap-evolution.md §1）
- [ ] E3: riscv-tests RV32I 5 指令 tohost=1 CppHDL sim 端到端 PASS
- [ ] E4: 生成 cpu.v 含 5-stage 完整 Verilog
- [ ] E5: 8/8 check_plugin_portability.sh PASS（含新增 Check 9: _chmem.h 必须有 CF_PLUGIN_USE_FSM_EXEMPT 标注 if FSM）
- [ ] E8: Verilator sim 跑 riscv-tests add.elf tohost=1 与 CppHDL sim byte-equal
- [ ] E12: chipforge_tests TLM baseline 0 回归
- [ ] E13: chipforge_tests_chmem 完整 5-stage + riscv-tests + Verilator 全 PASS
      （**当前被"CH_MEM dmem 读路径 1-cycle 滞后"阻塞** —— 见上"已知问题"）
- [ ] E14: CHANGELOG v0.10.0/v0.11.0 entry + ADR-037 v2.0 Accepted 状态 + ADR-040 v3.0 (Verilator 集成段)
- [ ] 间接：与 feat-rv32um-m-extension 共享 MUL/DIV 路径
- [ ] 间接：v0.10.0 PoC-2 RV32C ≥95% + PoC-3 ICache ≥90%
