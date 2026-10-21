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
- **闭环 change**：`openspec/changes/chmem-dmem-read-latency-aligned/`
- ✅ **状态：已闭环（2026-10-21）** —— 上游 CppHDL 已提交修复并归档
- **违约方（已更正）**：**上游 CppHDL**（非 ChipForge）。原判断"上游行为 by-contract、
  违约方是 ChipForge"已被上游修复推翻 —— 上游把 `async_read` 归组合 + 建 `read_port →
  data_proxy` 数据流边后，Simulator 与生成 Verilog 同为 0-cycle，CH_MEM 侧无需任何改动
- **根因（2026-10-21 变异对照精化）**：上游 `mem-read-timing` R3 要求 sync+async 读端口都
  归 `sequential_instr_list_` ⇒ Simulator 的 `ch_mem::aread` 相对生成 Verilog 的连续
  `assign` 滞后 1 拍。CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage
  寄存器）⇒ regfile 在同一次 `eval_sequential()` 内先于读端口锁存（regfile node id 702 <
  `load_read` 1707）⇒ **load 的结果 = 上一个访存地址的内容**。
  **变异对照结论**：上游修复含两个独立成分 ——
  ① `read_port → data_proxy` 的 **src 边**（提供拓扑序）才是修好 ELF 失败面的**承重**件：
     只撤分类（边保留）⇒ ELF 仍全绿；边与分类都撤 ⇒ 精确重现 10 个 ELF FAIL；
  ② **分类**（async 归组合）决定与 Verilog 的 0-cycle 对齐，即 `[mem-drift]` pin 检测的对象。
- **失败面（历史，已消除）**：39 个 rv32ui 中 10 个 FAIL（`lb`/`lbu`/`ld_st`/`lh`/`lhu`/`lw`/
  `sb`/`sh`/`sw`/`st_ld`），其余 29 个 + 8 个 rv32um 全 PASS（反汇编逐 ELF 统计 load 数：
  FAIL 组 19~193，PASS 组全 0）
- **跨后端**：`[verilator]` 同批 48 ELF 全 PASS ⇒ 差异在后端相位分类，不在插件逻辑
- **回归锚**：`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`（`[mem-drift]`）—— 已从
  "钉死 1 拍漂移" 反转为 "**锁住 0-cycle 对齐**"（每拍观测值 == 当前地址内容，且 != 上一拍
  地址内容；变异对照：上游 async 端口改回 sequential ⇒ 本测试 FAIL）
- **修复方向（结论）**：**方案 B（上游改）是唯一"零 ChipForge 数据通路改动"的解法** ——
  上游 `async_read` 归组合 + 数据流边 ⇒ 实测 ChipForge `[chmem]` **10 → 0 FAIL**、
  47 个 vendored ELF 全 PASS（16410 assertions）、`[verilator]` 不变（122 assertions）、
  TLM 不受影响、**ChipForge 业务代码零改动**。
  ⚠️ 方案 A（dmem 改 `sread`）单独实施**反而恶化**（10 → 11 FAIL，10 个 ELF 依旧全红），
  且必须伴随 CH_MEM 数据通路级改造（load-use stall + RMW 写通路）⇒ **放弃**。
  方案 C（上游加显式 backend 开关）保留为"上游只能小步改"时的备选。
- **上游已落地**：CppHDL `c27543d`（`fix(sim): align ch_mem::aread with generated Verilog
  0-cycle assign`）+ `5b37b7d` archive `sim-mem-read-timing-align-2026-10` +
  `eccf58f` 跨后端 parity + `cba0571` archive `dual-backend-mem-read-timing-parity-2026-10`
- **实测（2026-10-21，升级上游 deps 后）**：`[chmem]` **73 case / 73 PASS / 0 failed**
  （19389 assertions）、vendored ELF 47/47 全 PASS、`[verilator]+[mmu-verilator]` 4 case /
  122 assertions PASS、TLM 480/481（唯一失败 = 已知 `test_7stage_riscv.cpp:124` infra drift）
- **阻塞**：~~E13（`chipforge_tests_chmem` 完整 5-stage + riscv-tests + Verilator 全 PASS）~~
  ⇒ **前置条件已满足**（2026-10-21 上游落地后 `[chmem]` 全绿），待本 feature 自身收官

## 验收标准
- [ ] E1-E14 全部 PASS（详见 docs/architecture/roadmap-evolution.md §1）
- [ ] E3: riscv-tests RV32I 5 指令 tohost=1 CppHDL sim 端到端 PASS
- [ ] E4: 生成 cpu.v 含 5-stage 完整 Verilog
- [ ] E5: 8/8 check_plugin_portability.sh PASS（含新增 Check 9: _chmem.h 必须有 CF_PLUGIN_USE_FSM_EXEMPT 标注 if FSM）
- [ ] E8: Verilator sim 跑 riscv-tests add.elf tohost=1 与 CppHDL sim byte-equal
- [ ] E12: chipforge_tests TLM baseline 0 回归
- [ ] E13: chipforge_tests_chmem 完整 5-stage + riscv-tests + Verilator 全 PASS
      （**前置阻塞已解除** —— 上游 `c27543d` 落地后 `[chmem]` 73/73 PASS，
      见上"已知问题（已闭环）"）
- [ ] E14: CHANGELOG v0.10.0/v0.11.0 entry + ADR-037 v2.0 Accepted 状态 + ADR-040 v3.0 (Verilator 集成段)
- [ ] 间接：与 feat-rv32um-m-extension 共享 MUL/DIV 路径
- [ ] 间接：v0.10.0 PoC-2 RV32C ≥95% + PoC-3 ICache ≥90%
