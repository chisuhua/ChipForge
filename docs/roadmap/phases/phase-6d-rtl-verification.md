# Phase 6d：5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM

> **Status**: Not Started (待启动)
> **拆分日期**: 2026-09-20 (从 Phase 6c v2.0.2 拆分, 详见 [`phase-6-declarative.md`](phase-6-declarative.md) §3)
> **里程碑**: M6d - riscv-tests RV32I 5 指令 `tohost=1` 端到端 RTL sim + Verilator 集成 + 多周期协议引擎实装
> **Depends on**: Phase 6c ✅ 完成; Phase 6d prereqs (`openspec/changes/phase-6d-prerequisites/`)
> **决策依据**: `docs/research/phase6c-elaboration-pattern-study.md` §10 + `ADR-040 v2.0` + `ADR-046`
> **目标版本**: ChipForge 0.4.x

**目标**：在 Phase 6c 框架能力（elaboration → Verilog → Simulator）已实证的基础上，端到端验证 5-stage RISC-V CPU Pipeline (CH_MEM 完整版) + 集成 Verilator 综合验证 + 实装 MMU/PTW 与 L1Cache refill 多周期 FSM。

> **范围纪律**: Phase 6c 仅证明"框架能生成 Verilog + 跑 CppHDL native Simulator PoC"。Phase 6d 是"业务代码端到端跑通" — 5-stage CPU riscv-tests RV32I 5 指令 `tohost=1` + Verilator 验证 + MMU/PTW FSM 真实可用。

---

## 1. Phase 6d 触发条件 (v2.0.3, 2026-09-20)

满足以下**全部**前置条件启动 Phase 6d:

### 1.1 Phase 6c 已完成 ✅

- ✅ M1-M5 + M6 全部落地, v0.3.0 + v0.3.1 已发布
- ✅ `cf::plugin` 框架双模化 (uint_t/payload/pipe_builder/storage/ctrl_link)
- ✅ 端到端 PoC 实证 (`pipeline2_stall_matrix` 16/16 PASS + M3 PoC + M4 PoC)
- ✅ D4 + ADR-040 静态检查 8/8 PASS
- ✅ ADR-040 v2.0 + ADR-046 已 Accepted

### 1.2 Phase 6d prereqs 已完成 (新增 prereqs OpenSpec change)

- ⏸ **riscv64-unknown-elf-gcc** 工具链安装到 build env (Phase 6c 缺失, 阻塞 riscv-tests 编译)
- ⏸ **Verilator** ≥ 5.020 + **Yosys** + **iverilog** 综合验证工具链 (Phase 6c 缺失, 阻塞综合验证)
- ⏸ **ADR-037 v2.0 修订**: D4 范式在 elaboration 语义下兑现 (推迟自 Phase 6c W9, Phase 6d 启动前必做)
- ⏸ **PoC follow-up fixes**: M3/W6 byte-equal #95 完整版 + chbool context pollution 测试隔离 (Phase 6c 已知 issue, 不阻塞 Phase 6c 收官但 Phase 6d 启动前必须闭环)
- ⏸ **M3 HazardPlugin 完整 CH_MEM 版** (Phase 6c 仅骨架 + PoC, Phase 6d 端到端验证需要完整 RAW 检测)

---

## 2. Phase 6d 范围拆分 (v2.0.3, 2026-09-20 拟定)

| 子阶段 | 内容 | 工时 | 依赖 |
|--------|------|------|------|
| **6d.1** | DecoderPlugin 完整 CH_MEM (Phase 6c 仅 ALU 内嵌) | 1 周 | 6d prereqs |
| **6d.2** | BranchPlugin + HazardPlugin 完整 CH_MEM | 1.5 周 | 6d prereqs + 6d.1 |
| **6d.3** | CpuFactoryChmem 完整 5-stage 集成 (IF/ID/EX/MEM/WB 全 5 阶段 + 4 plugin 正确 stage wiring) | 1.5 周 | 6d.1 + 6d.2 |
| **6d.4** | riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) 端到端 `tohost=1` CppHDL sim 验证 | 2 周 | 6d.3 + riscv64 工具链 |
| **6d.5** | Verilator 后端集成 (Verilog → Verilator 编译 → VL1Cache/VRegFile 替代 C++ sim) | 1.5 周 | 6d.4 + Verilator |
| **6d.6** | MMU/PTW 多周期 FSM (`chlib::ch_state_machine` + `CF_PLUGIN_USE_FSM_EXEMPT`) | 1 周 | 6d.5 + ADR-046 |
| **6d.7** | L1Cache refill FSM (同 ch_state_machine) | 0.5 周 | 6d.6 |
| **6d.8** | Harness 迁移 (pb.run() → CppHDL sim runner / Verilator) + `tools/cpu_sim/main.cpp` 切换 | 1 周 | 6d.4 |
| **总计** | | **10-12 周** | — |

**Phase 6d 估时说明**: 比 Phase 6c (9 周) 略长, 因为:
- 业务代码完整化 (非 PoC)
- 工具链首次集成 (Verilator + riscv64)
- 多周期 FSM 首次实装 (ADR-046 豁免后)
- Harness 迁移是 W9 推迟项的兑现

---

## 3. 退出标准 (Phase 6d 完整版)

### 3.1 最小退出标准 (M6d.1, "5-stage Pipeline CH_MEM 端到端")

| # | 标准 | 验证命令 |
|---|------|----------|
| E1 | `m4_poc_5stage_simulator_tick` 升级为完整 5-stage 版本 | `./bin/chipforge_tests_chmem "m4_poc_5stage_full_simulator"` |
| E2 | DecoderPlugin/BranchPlugin/HazardPlugin 完整 CH_MEM 实现 | `git log` + `tests/cpu/test_cpu_5stage.cpp` PASS |
| E3 | riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` CppHDL sim 端到端 PASS | `./bin/chipforge_tests_chmem "riscv_tests_rv32ui_*"` |
| E4 | 生成 `cpu.v` 含 5-stage 完整 Verilog | `ch::toVerilog("cpu.v", ctx)` 输出含 5 个 stage always_ff |
| E5 | 8/8 `check_plugin_portability.sh` 仍 PASS (含新增 Check 9: `_chmem.h` 必须有 `CF_PLUGIN_USE_FSM_EXEMPT` 标注 if FSM) | `bash tools/check_plugin_portability.sh` |
| E6 | `verify_adr.sh` 0 FAILED + `verify_plugin_decision.sh` PASS | 同上 |

### 3.2 标准退出标准 (M6d.2, "+ Verilator + MMU FSM")

| # | 标准 | 验证命令 |
|---|------|----------|
| E7 | Verilator 编译 `cpu.v` 无 error | `verilator --lint-only cpu.v` |
| E8 | Verilator sim 跑 riscv-tests add.elf `tohost=1` 与 CppHDL sim byte-equal | TLM↔Verilog 对比 trace |
| E9 | MMU PTW 5 状态 FSM (`chlib::ch_state_machine`) 实装 + `CF_PLUGIN_USE_FSM_EXEMPT` 标记 | `ip/cpu/plugins/mmu_ptw_chmem.h` |
| E10 | L1Cache refill FSM (4 状态) 实装 | `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` |
| E11 | Harness 切换 (`tools/cpu_sim/main.cpp` → CppHDL sim runner) + 保留 CLI 兼容 | `tools/cpu_sim --elf add.elf` 跑 CppHDL sim |
| E12 | `chipforge_tests` TLM baseline 0 回归 (391 PASS / 12 known FAIL 保持不变, 不新增 FAIL) | `ctest -R chipforge_tests` |
| E13 | `chipforge_tests_chmem` 完整 5-stage + riscv-tests + Verilator 全 PASS | `ctest -R chipforge_tests_chmem` |
| E14 | CHANGELOG v0.4.x 发布 + ADR-037 v2.0 Accepted + ADR-040 v3.0 (Verilator 集成段) | `CHANGELOG.md` + `docs/architecture/adr.md` |

---

## 4. 与 Phase 6c 推迟项的对应关系

| Phase 6c 推迟项 (v0.3.1 §M5) | Phase 6d 子阶段 |
|------|------|
| riscv-tests RV32I 5 指令 `tohost=1` 端到端 | **6d.4** |
| Verilator / Yosys / iverilog 综合验证集成 | **6d.5** |
| M3/W6 byte-equal 完整版 (#95) | **PoC follow-up OpenSpec change** (Phase 6d 启动前) |
| 5-stage DecoderPlugin + BranchPlugin + HazardPlugin 完整 CH_MEM | **6d.1 + 6d.2 + 6d.3** |
| MMU/PTW 多周期 FSM | **6d.6** |
| L1Cache refill FSM | **6d.7** |
| Harness 迁移 | **6d.8** |

---

## 5. 决策可追溯

Phase 6d 的所有设计决策来源于:
- **Phase 6c 收官条目**: 本目录 [`phase-6-declarative.md` §2](phase-6-declarative.md)
- **ADR-040 v2.0** (2026-09-17): `CH_MEM 是新正道`, `array_store` 双缓冲, `CtrlLink::halt_when(ch_bool)`
- **ADR-046** (2026-09-16): 多周期协议引擎豁免 (`CF_PLUGIN_USE_FSM_EXEMPT` 机制, `chlib::ch_state_machine` DSL)
- **Phase 6c 研究文档**: [`docs/research/phase6c-elaboration-pattern-study.md`](../../research/phase6c-elaboration-pattern-study.md) §11-13 (SpinalHDL/VexRiscv/CppHDL elaboration 原理 + cf::plugin 映射)
- **DECISION-2026-06-13-02 F1.A** (Phase 1.4): L1CachePlugin 设计方法学基线
- **待修订**: ADR-037 v2.0 (D4 范式在 elaboration 语义下兑现, 推迟自 Phase 6c W9, 6d 启动前必做)

任何对 Phase 6d 范围/接口的修改, **必须**同步更新决策记录 + ADR。

---

## 6. 详细任务清单

> 详细 tasks 在 OpenSpec change `openspec/changes/phase-6d-rtl-verification/tasks.md` 中维护 (4/4 artifacts complete, openspec validate PASS)。
> 本文件仅提供 phase-level 范围 + 退出标准, 实施时按 OpenSpec 工作流细化。

### 6.1 6d.1 - DecoderPlugin 完整 CH_MEM (Oracle 修正: 1.5 周, 从零建)

- **新建** `ip/cpu/plugins/decode_chmem.h` (Oracle 修正: 原"重启用 `.disabled`"措辞错误, 该文件**不存在**)
- 参考 `ip/cpu/arch/riscv/decode.h` (147 行 TLM 模板类) + `decoder_table.h` (400 行 constexpr 查表) 翻译为 CH_MEM 风格
- 完整 RV32I 主 opcode 7 bit + funct3 3 bit + funct7 7 bit 译码 (17 bit 总空间, ~80 项有效指令)
- 输出: `DECODE` Payload (含 `opcode`, `funct3`, `funct7`, `rd_idx`, `rs1_idx`, `rs2_idx`, `imm`, `reads_rs1`, `reads_rs2`, `writes_rd` 等)
- 与 `IntAluPlugin` + `BranchPlugin` 通过 Payload 接口
- D4 合规: 无 tick(), 无状态机, at_stage 注册

### 6.2 6d.2 - BranchPlugin + HazardPlugin 完整 CH_MEM (1.5 周)

- `branch_chmem.h`: 6-op B-type 完整实现 (BEQ/BNE/BLT/BGE/BLTU/BGEU) + `branch_target = pc + imm`
- `hazard_chmem.h`: RAW 检测完整版 (id_rs1/rs2 vs ex/mem/wb rd 完整检测, 当前 PoC 仅 6 条件 OR)
- 与 `CtrlLink::halt_when(ch_bool)` 接到 ID stage stall 门控

### 6.3 6d.3 - CpuFactoryChmem 完整 5-stage 集成 (1.5 周)

- `cpu_factory_chmem.h::build_cpu()` 完整 5-stage elaboration (IF/ID/EX/MEM/WB 全 5 阶段)
- 4 个 Plugin 正确 stage wiring: IBusPlugin + DecoderPlugin + IntAluPlugin + BranchPlugin + HazardPlugin + RegFilePlugin
- EARLY-stage payload pre-population (v0.3.1 M6 fix 沿用)
- Stage linking connectors (M2/Spike-1..7)

### 6.4 6d.4 - riscv-tests RV32I 端到端 (2 周)

- `tests/cpu/test_cpu_chmem_riscv_tests.cpp.disabled` 重启用
- 5 指令 add/addi/auipc/jal/beq 端到端 `tohost=1` PASS
- CppHDL Simulator 跑 (10+ tick)
- 这是 Phase 6d 端到端验证的**硬证据**

### 6.5 6d.5 - Verilator 后端集成 (1.5 周)

- `tools/verilator_runner/main.cpp` 新建 (类似 `cpu_sim` 但调 Verilator VL1Cache)
- `verilator --cc cpu.v --exe main.cpp` 编译 + 跑 sim
- Verilator sim 与 CppHDL sim byte-equal 对比
- 这一步是"可综合 RTL"硬证据 (vs "可仿真 RTL")

### 6.6 6d.6 - MMU/PTW 多周期 FSM (1 周, Oracle 修正: 改 sv32 对齐现有 MMUPlugin)

- `ip/cpu/plugins/mmu_ptw_chmem.h` 新建
- **sv32 4 状态 walk FSM** (Oracle 修正: 与 `ip/mmu/` 现有 sv32 MMUPlugin 对齐, 避免 sv39 范围扩大; AGENTS.md 载明 `enable_mmu=true, sv32`；状态: IDLE / L0_WAIT / L1_WAIT / DONE / FAULT 共 5 个含 FAULT)
- 用 `chlib::ch_state_machine<PTW_State, 5>` DSL (Oracle 修正: sv32 = 2-level, 不是 sv39 3-level)
- 标记 `#define CF_PLUGIN_USE_FSM_EXEMPT` 豁免 ADR-040 Tier-1 Check 5
- 必须 Verilator 验证 (ch_state_machine 简化实现不能在 CppHDL sim 跑 cycle-accurate)

### 6.7 6d.7 - L1Cache refill FSM (0.5 周)

- `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 新建
- 4 状态 FSM (IDLE/LOOKUP/MISS/REFILL_WAIT)
- 同 6d.6 ch_state_machine + CF_PLUGIN_USE_FSM_EXEMPT

### 6.8 6d.8 - Harness 迁移 (1 周)

- `tools/cpu_sim/main.cpp` 从 `pb.run()` 迁到 `pb.elaborate()` + `ch::Simulator::tick()` / Verilator
- 保留 CLI 兼容性 (`--elf` flag 等)
- `[cpu-integration]` 测试 (`test_*stage_riscv.cpp`) 同步迁移

---

## 7. 风险与回退

| 风险 | 回退 | 状态 |
|------|------|------|
| 工具链就绪延迟 (riscv64/Verilator) | **回退**: Phase 6d prereqs change 显式跟踪, 阻塞时锁 6d.4-6d.5 | 监控中 |
| DecoderPlugin 完整实现超 1 周估时 | **回退**: 6d.1 拆分为"5 指令子集" + "完整 RV32I" | 监控中 |
| Verilator 编译 `cpu.v` 不可综合 | **回退**: Phase 6d 仅交付"可仿真 RTL" (与 Phase 6c 一致), 推迟"可综合 RTL" 到 Phase 6e | 监控中 |
| MMU PTW Verilator sim 不工作 | **回退**: 仅交付 PoC, 推迟 MMU FSM 完整化到 Phase 6e | 监控中 |
| Harness 迁移影响 CPU 业务代码 | **回退**: 保留 TLM 入口 (deprecated), 仅在 `--mode chmem` 时切 CppHDL sim | 监控中 |
| Phase 6c PoC context pollution (#95) 影响 6d 测试 | **前置修复**: PoC follow-up change 必包含 #95 fix | 🔴 关键监控 |

---

## 8. 时间盒 (v2.0.3 估时)

| 子阶段 | 任务 | 估时 | 状态 |
|------|------|------|------|
| 6d prereqs | 工具链 + ADR-037 v2.0 + PoC follow-up | 1-1.5 周 | ⏸ 待启动 |
| 6d.1 | DecoderPlugin 完整 (Oracle: 从零建, 估时 1.5 周) | 1.5 周 | ⏸ 待启 |
| 6d.2 | Branch + Hazard 完整 | 1.5 周 | ⏸ 待启 |
| 6d.3 | CpuFactoryChmem 完整集成 | 1.5 周 | ⏸ 待启 |
| 6d.4 | riscv-tests 端到端 (Oracle: 4 ELF 待编 `tests/cpu/manual_elf/`) | 2 周 | ⏸ 待启 |
| 6d.5 | Verilator 集成 (Oracle: jammy 22.04 verilator 4.038 不足, 需源码 build) | 1.5 周 | ⏸ 待启 |
| 6d.6 | MMU/PTW FSM (Oracle: 改 sv32 对齐现有 MMUPlugin) | 1 周 | ⏸ 待启 |
| 6d.7 | L1Cache refill FSM | 0.5 周 | ⏸ 待启 |
| 6d.8 | Harness 迁移 | 1 周 | ⏸ 待启 |
| **总计** | — | **10-12 周** | — |

---

## 9. 修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-09-20 | 初版: 从 Phase 6c v2.0.2 拆分, 独立 phase doc |

---

*本文件为 Phase 6d 独立 phase doc。Phase 6c 已完成 ([phase-6-declarative.md §2](phase-6-declarative.md)), Phase 6d 待启动。详细 tasks 在 OpenSpec change `openspec/changes/phase-6d-rtl-verification/tasks.md` 中维护。*
