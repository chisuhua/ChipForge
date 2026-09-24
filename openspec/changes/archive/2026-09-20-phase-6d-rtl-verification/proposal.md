# Phase 6d: 5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM

> **关联 OpenSpec**: `openspec/changes/phase-6d-rtl-verification/`
> **关联 phase doc**: [docs/roadmap/phases/phase-6d-rtl-verification.md](../../docs/roadmap/phases/phase-6d-rtl-verification.md)
> **关联 prereqs**: `openspec/changes/phase-6d-prerequisites/` (必先 archive)
> **关联 PoC follow-up**: `openspec/changes/poc-follow-up-fixes/` (必先 archive)

## Why

Phase 6c `plugin-elaboration-substrate` 完成 (v0.3.0 + v0.3.1, 2026-09-20) 证明 `cf::plugin` 可生成 Verilog + 跑 CppHDL native Simulator PoC。但 Phase 6c 是"框架能力证明", **业务代码端到端跑通 riscv-tests RV32I 5 指令 `tohost=1` + Verilator 集成 + 多周期协议引擎实装** 是 Phase 6d 目标。

Phase 6d 兑现 README "CppTLM + CppHDL-based" HDL 侧**端到端承诺**:
- riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) CppHDL sim `tohost=1`
- Verilator 后端集成 (生成 Verilog → Verilator 编译 → VL1Cache/VRegFile 替代 C++ sim)
- MMU/PTW 多周期 FSM (`chlib::ch_state_machine` + `CF_PLUGIN_USE_FSM_EXEMPT` ADR-046 豁免)
- L1Cache refill FSM (同 ch_state_machine)
- Harness 迁移 (pb.run() → CppHDL sim runner / Verilator)

**Why now**: Phase 6c 框架能力已 PoC 实证, Phase 6d prereqs (工具链 + ADR-037 v2.0 + PoC follow-up) 闭环后立即启动。

## What Changes

### 6d.1: DecoderPlugin 完整 CH_MEM (Oracle 修正: 1.5 周, 从零建)

- **新建** `ip/cpu/plugins/decode_chmem.h` (Oracle 修正: 原"重启用 `.disabled`"措辞错误, 该文件**不存在**)
- 参考 `ip/cpu/arch/riscv/decode.h` (147 行 TLM 模板类) + `decoder_table.h` (400 行 constexpr 查表) 翻译为 CH_MEM 风格
- 完整 RV32I 主 opcode 7 bit + funct3 3 bit + funct7 7 bit 译码 (17 bit 总空间, ~80 项有效指令)
- 输出: `DECODE` Payload (含 `opcode`, `funct3`, `funct7`, `rd_idx`, `rs1_idx`, `rs2_idx`, `imm`, `reads_rs1`, `reads_rs2`, `writes_rd`)
- 与 `IntAluPlugin` + `BranchPlugin` 通过 Payload 接口

### 6d.2: BranchPlugin + HazardPlugin 完整 CH_MEM (1.5 周)

- `branch_chmem.h`: 6-op B-type 完整 (BEQ/BNE/BLT/BGE/BLTU/BGEU) + `branch_target = pc + imm`
- `hazard_chmem.h`: RAW 完整检测 (id_rs1/rs2 vs ex/mem/wb rd 9 条件 OR-merge, Phase 6c PoC 仅 6 条件)
- `CtrlLink::halt_when(ch_bool)` 接到 ID stage stall 门控

### 6d.3: CpuFactoryChmem 完整 5-stage 集成 (1.5 周)

- `cpu_factory_chmem.h::build_cpu()` 完整 5-stage elaboration (IF/ID/EX/MEM/WB)
- 4+ 个 Plugin 正确 stage wiring: IBusPlugin + DecoderPlugin + IntAluPlugin + BranchPlugin + HazardPlugin + RegFilePlugin
- EARLY-stage payload pre-population (v0.3.1 M6 fix 沿用)
- Stage linking connectors (M2/Spike-1..7)

### 6d.4: riscv-tests RV32I 端到端 (2 周, Oracle 修正: 4 ELF 待编)

- `tests/cpu/test_cpu_chmem_riscv_tests.cpp.disabled` 重启用
- **5 指令 (add/addi/auipc/jal/beq) ELF 准备** (Oracle 修正: 仓库无 riscv-tests submodule; 现有 `tests/cpu/manual_elf/` 仅有 add/and/or/sll/srli/sub, 缺 addi/auipc/jal/beq 4 条)
  - 选项 A: 为 4 条缺失指令编写 `tests/cpu/manual_elf/*.S` (沿用 add.S 的 tohost 模式)
  - 选项 B: vendor riscv-tests submodule (大仓库, 慎选)
  - 推荐选项 A (与现有 pattern 一致)
- 5 指令 add/addi/auipc/jal/beq 端到端 `tohost=1` PASS
- CppHDL Simulator 跑 (10+ tick)
- **硬证据**: Phase 6d 端到端验证 (与 Phase 6c PoC 区别)

### 6d.5: Verilator 后端集成 (1.5 周)

- `tools/verilator_runner/main.cpp` 新建 (类似 `cpu_sim` 但调 Verilator VL1Cache)
- `verilator --cc cpu.v --exe main.cpp` 编译 + 跑 sim
- Verilator sim 与 CppHDL sim byte-equal 对比
- **硬证据**: "可综合 RTL" (vs Phase 6c "可仿真 RTL")

### 6d.6: MMU/PTW 多周期 FSM (Oracle 修正: 改 sv32 对齐现有 MMUPlugin)

- `ip/cpu/plugins/mmu_ptw_chmem.h` 新建
- **sv32 2-level walk FSM** (Oracle 修正: 与 `ip/mmu/` 现有 sv32 MMUPlugin 对齐, 避免 sv39 范围扩大; AGENTS.md 载明 `enable_mmu=true, sv32`；状态: IDLE/L0_WAIT/L1_WAIT/DONE/FAULT)
- 用 `chlib::ch_state_machine<PTW_State, 5>` DSL (sv32 = 2-level, 不是 sv39 3-level)
- 标记 `#define CF_PLUGIN_USE_FSM_EXEMPT` 豁免 ADR-040 Tier-1 Check 5
- 必须 Verilator 验证 (ch_state_machine 简化实现不能在 CppHDL sim 跑 cycle-accurate)

### 6d.7: L1Cache refill FSM (0.5 周)

- `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 新建
- 4 状态 FSM (IDLE/LOOKUP/MISS/REFILL_WAIT)
- 同 6d.6 ch_state_machine + CF_PLUGIN_USE_FSM_EXEMPT

### 6d.8: Harness 迁移 (1 周)

- `tools/cpu_sim/main.cpp` 从 `pb.run()` 迁到 `pb.elaborate()` + `ch::Simulator::tick()` / Verilator
- 保留 CLI 兼容性 (`--elf` flag 等)
- `[cpu-integration]` 测试 (`test_*stage_riscv.cpp`) 同步迁移

## Capabilities

### New Capabilities

- `decoder-chmem-complete`: DecoderPlugin 完整 CH_MEM (RV32I ~80 指令译码)
- `branch-hazard-chmem-complete`: Branch + Hazard Plugin 完整 CH_MEM (B-type + RAW 检测)
- `cpu-factory-chmem-5stage`: CpuFactoryChmem 完整 5-stage 集成
- `riscv-tests-rv32ui-tohost-1`: riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` 端到端 CppHDL sim
- `verilator-backend-integration`: Verilator 后端集成 (Verilog → VL1Cache/VRegFile → sim)
- `mmu-ptw-fsm-chmem`: MMU PTW 5 状态 FSM (`ch_state_machine` + `CF_PLUGIN_USE_FSM_EXEMPT`)
- `l1-cache-refill-fsm-chmem`: L1Cache refill 4 状态 FSM
- `harness-migration`: `pb.run()` → CppHDL sim runner / Verilator 迁移

### Modified Capabilities

- `cpu-real-fetch-and-memory` (已存在): Harness 迁移可能需要更新
- `cpu-pc-update-on-writeback` (已存在): 5-stage 集成后 PC 更新路径需验证

## Impact

- **影响文件**:
  - `ip/cpu/plugins/decode_chmem.h` (6d.1 重启用)
  - `ip/cpu/plugins/branch_chmem.h` (6d.2 完整化)
  - `ip/cpu/plugins/hazard_chmem.h` (6d.2 完整化, 依赖 PoC follow-up Fix #3)
  - `ip/cpu/cpu_factory_chmem.h` (6d.3 5-stage 完整集成)
  - `tests/cpu/test_cpu_chmem_riscv_tests.cpp` (6d.4 重启用)
  - `tools/verilator_runner/main.cpp` (6d.5 新建)
  - `ip/cpu/plugins/mmu_ptw_chmem.h` (6d.6 新建)
  - `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` (6d.7 新建)
  - `tools/cpu_sim/main.cpp` (6d.8 Harness 迁移)
  - `tests/cpu/integration/test_*stage_riscv.cpp` (6d.8 同步迁移)
  - `CHANGELOG.md` (v0.4.x)
- **测试**: 全部现有 PASS + 5 指令 tohost=1 PASS + Verilator byte-equal
- **依赖**:
  - `phase-6d-prerequisites` (必先 archive, 工具链 + ADR-037 v2.0)
  - `poc-follow-up-fixes` (必先 archive, #95 + chbool + Hazard)
- **不破坏**:
  - TLM baseline `chipforge_tests` 0 回归
  - `check_plugin_portability.sh` 8/8 PASS (可能扩到 9/9 with CF_PLUGIN_USE_FSM_EXEMPT 检查)
  - `verify_adr.sh` 0 FAILED

## Tasks

(详细 task list 见 OpenSpec `tasks.md`, 由 Phase 6d 启动后细化)

**估时汇总** (Oracle 2026-09-20 修订: 6d.1 从 1 周 → 1.5 周, 6d.3 显式吸收 memory model +0.5 周):

| 子阶段 | 估时 | 备注 |
|------|------|------|
| 6d.1 Decoder | **1.5 周** | Oracle 修正: 原 1 周与 §6.1 标题自相矛盾; 从零建 (~80 指令译码) 合理估时 |
| 6d.2 Branch + Hazard | 1.5 周 | 完整 9 条件 RAW + 6 B-type |
| 6d.3 CpuFactoryChmem 5-stage | **2 周** | Oracle 关键发现: 显式新增 CH_MEM fetch/memory model 任务 (~0.5 周), 6d.3 内吸收 |
| 6d.4 riscv-tests tohost=1 | 2 周 | 5 指令端到端; 接受路径放宽为"~8-9 条指令路径 (含 lui/sw/bne)" |
| 6d.5 Verilator | 1.5 周 | E8 降级"tohost=1 一致" 替代 "trace byte-equal" (trace 对比基础设施不存在) |
| 6d.6 MMU PTW FSM (sv32) | 1 周 | sv32 5 状态 (Oracle 修正, 不是 sv39 3-level) |
| 6d.7 L1Cache refill FSM | 0.5 周 | 4 状态 |
| 6d.8 Harness 迁移 | 1 周 | 显式排除 7stage superscalar config (预存 segfault 干扰) |
| **总计** | **~11.5-12.5 周** | Oracle 修订后 |

## 风险与回退

| 风险 | 回退 | 状态 |
|------|------|------|
| 6d.4 riscv-tests 端到端超时 (5 指令) | **回退**: 6d.4 降级为 PoC (单指令 `add.elf`) + 完整 5 指令推迟 Phase 6e | 🔴 关键监控 |
| 6d.5 Verilator 编译失败 | **回退**: Phase 6d 仅交付"可仿真 RTL" (与 Phase 6c 一致), 推迟"可综合 RTL" 到 Phase 6e | 监控中 |
| 6d.6 MMU PTW Verilator sim 不工作 | **回退**: 仅交付 PoC, 推迟 MMU FSM 完整化到 Phase 6e | 监控中 |
| 6d.8 Harness 迁移影响 CPU 业务代码 | **回退**: 保留 TLM 入口 (deprecated), 仅在 `--mode chmem` 时切 CppHDL sim | 监控中 |
| **🔴 [Oracle 关键风险] CH_MEM memory model 完全缺失** (`cpu_factory_chmem.h:147-151` `T* memory` unused, `ibus_chmem.h` 不存在) | **缓解 (Oracle 2026-09-20 必做)**: 6d.3 显式新增 task 3.0, 新建 `ibus_chmem.h` + `dmem_chmem.h` + 独立 PoC, 先于 6d.4 完成 | 🔴 关键监控 |
| **🟠 [Oracle 验证级联] 6d.5→6d.6/6d.7 验证路径缺失** (若 6d.5 触发"仅可仿真"回退, 6d.6/6d.7 零验证路径) | **缓解 (Oracle 2026-09-20 必做)**: 6d.5 先做最小 Verilator smoke (lint + 小 FSM); 6d.6/6d.7 验收改为"Verilator lint + 定向状态转移 testbench", 不依赖全 CPU Verilator byte-equal | 🟠 高监控 |
| **🟡 [Oracle 内部矛盾] tasks.md 6d.6 sv39/L2_WAIT 描述** (原 task 6.5 写"L2_WAIT", 6.7 写"跑通 sv39 walk", 与 sv32 修正矛盾) | **回退**: 6d.6 任务已修订为 sv32 5 状态, 不再存在 | 🟢 已修正 |
| **🟡 [Oracle 提案不一致] proposal.md 6d.1 时间盒 1 周 vs §6.1 标题 1.5 周** | **回退**: 时间盒已统一为 1.5 周 | 🟢 已修正 |

## 时间盒

| 子阶段 | 任务 | 估时 | 状态 |
|------|------|------|------|
| 6d.1 | DecoderPlugin 完整 | 1 周 | ⏸ 待启动 |
| 6d.2 | Branch + Hazard 完整 | 1.5 周 | ⏸ 待启动 |
| 6d.3 | CpuFactoryChmem 完整集成 (含 memory model) | **2 周** | ⏸ 待启动 |
| 6d.4 | riscv-tests 端到端 (~8-9 指令路径) | 2 周 | ⏸ 待启动 |
| 6d.5 | Verilator 集成 (E8 tohost=1 一致) | 1.5 周 | ⏸ 待启动 |
| 6d.6 | MMU/PTW FSM (sv32 5 状态) | 1 周 | ⏸ 待启动 |
| 6d.7 | L1Cache refill FSM | 0.5 周 | ⏸ 待启动 |
| 6d.8 | Harness 迁移 (排除 7stage superscalar) | 1 周 | ⏸ 待启动 |
| **总计** | — | **~11.5-12.5 周** | — |

## Phase 6 全阶段完成定义

**Phase 6c ✅ + Phase 6d =** `cf::plugin` 从"每周期仿真器"→ "elaboration DSL" → "端到端 RISC-V RTL sim" 完整 SpinalHDL 风格 RTL 生成方法学闭环。

**Phase 6a + Phase 6b =** 调度自动化 + TLM↔RTL 对比验证 (Phase 6d 端到端验证后启动)。

---

*本文件为 Phase 6d main change 的 proposal。详细 design + specs + tasks 在 OpenSpec change 内维护。*
