---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
depends_on:
  - cpu-pipeline-canonical-ordering-assert
---

# cpu-pipeline-multi-cycle — MUL/DIV 真多周期 stall

## Why

当前 CPU Pipeline 对 MUL/DIV 指令 (`RISC-V RV32M` 扩展) 的实现是 **single-cycle 完成**，但 RISC-V 规范要求：

| 指令 | 最小 latency | 最大 latency |
|------|-------------|-------------|
| MUL | 1 cycle | — |
| MULH/MULHU/MULHSU | 1 cycle | — |
| DIV | 1 cycle | 35 cycle |
| DIVU | 1 cycle | 35 cycle |
| REM | 1 cycle | 35 cycle |
| REMU | 1 cycle | 35 cycle |

当前实现的"1 cycle 完成"对 DIV/REM 类是**性能正确**（结果对），但**时序错误**（latency=1 而非 35）。后果：

1. **riscv-tests 中 mul/div 测试当前归类为 feature stub**（`soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 早期 FAIL）
2. **DSE baseline 失真**: cache-dse-sweep 无法区分 "1-cycle DIV" vs "35-cycle DIV" 的性能差异
3. **Plugin 框架未验证 LATENCY>1 指令**: 仅通过 LATENCY=1 指令验证，Plugin 框架的 stall/forwarding 是否支持多周期未知

RISC-V RV32M 扩展在 Phase 2 RV64GC 是必需，本 change 是 Phase 2 启动前置。

## What Changes

### 1. Plugin 多周期延迟声明 API

- **位置**: `include/cf/plugin/plugin_base.h` (或新文件 `include/cf/plugin/latency.h`)
- **新增** `cf::plugin::PluginBase::latency_table()` 虚函数 (默认返回 `std::unordered_map<Opcode, uint32_t>{}`)
- Plugin 重写该函数声明自身多周期指令延迟
- **冲突裁决 (Metis 审查修订)**: 当多个 Plugin 重写 `latency_table()` 声明同一 opcode 不同延迟, **框架层取最大值** (保守 stall 方向, 防止漏 stall)
- **依赖**: 不依赖其他 change (独立可启动)

### 2. IntAluPlugin 多周期支持

- **位置**: `ip/cpu/arch/riscv/int_alu.h` (含 CH_MEM 版 `int_alu_chmem.h`)
- **现状**: `at_stage("execute", NORMAL)` 单周期完成
- **目标**:
  - 新增 `latency_table()` 返回 `{DIV→35, DIVU→35, REM→35, REMU→35}` (其他 1)
  - DIV/REM 类用计数器模拟 35 cycle (简化: 全 35 cycle stall)
  - 周期 0: 启动除法
  - 周期 1-34: stall (CtrlLink::halt_when(div_in_progress))
  - 周期 35: 写结果到 RD_DATA

### 3. Plugin 框架 Stall 注入原语

- **位置**: `include/cf/plugin/pipe_builder.h`
- **新增**: `pb.run_with_latency()` 自动读所有 Plugin 的 `latency_table()`, 取每 opcode 的最大延迟, 注入 stall
- 复用 P1#4 cycle-precision 框架的 CURRENT_CYCLE counter
- 复用 v0.1.3 `plugin-framework-stall` 的 CtrlLink::halt_when 机制

### 4. Pipelined Stall 语义

- **核心**: 当 ID 阶段译码到 DIV 指令, **EX/MEM/WB 后续阶段 stall**, **IF/ID 也 stall** (避免 fetch 新指令被错误推进)
- 实现: `CtrlLink::halt_when(div_active)` 注册到所有 stage

### 5. 测试验证 (Metis #4 修订: 含 ELF vendor 前置)

- **前置** (Metis #4 新增): **vendor riscv-tests RV32M (mul/div/divu/rem/remu) ELF 到 tests/cpu/riscv_tests/elf/**
  - `tests/cpu/riscv_tests/build_rv32m.sh` 新增 (复用 v0.2.2 vendor 脚本模板)
  - `gcc -march=rv32im_zicsr` 编译 5 ELF (~20KB)
  - 验证 5 ELF 在 chipforge_tests 中跑通 (`tests/cpu/integration/test_rv32m_runner.cpp` 新建)
- **手动 fallback** (如 vendor 失败):
  - `tests/cpu/manual_elf/mul.S` / `div.S` / `divu.S` / `rem.S` 手写 ASM (复用 `tests/cpu/manual_elf/add.S` 模板)
  - 每条 1 ELF, ~10 行 ASM (immediate + ecall)
- **新建** `tests/cpu/integration/test_multi_cycle_stall.cpp` (Metis #4 vendor 优先):
  - Test 1: 单 DIV 指令 → 35 cycle 完成, RD_DATA 在 cycle 35 正确
  - Test 2: DIV 后立即 LW → stall 后正确读取
  - Test 3: 多 DIV 序列 → 每条 stall 35 cycle, 无 overlap
- **新建** `tests/cpu/test_int_alu_latency.cpp`:
  - Test 1: `latency_table()` 返回正确 map
  - Test 2: MUL (1 cycle) 不 stall
  - Test 3: DIV (35 cycle) 真 stall

### 6. ADR 新增

- **新增** `docs/architecture/adr/ADR-051-plugin-latency-table.md` (~200 LOC):
  - §Context: 解释 MUL/DIV 多周期问题
  - §Decision: Plugin `latency_table()` API + 自动 stall 注入 + **多 Plugin 冲突裁决规则 (max)**
  - §Consequences: 与 v0.1.3 plugin-framework-stall + P1#4 cycle-precision 集成
- **更新** `docs/architecture/adr.md` 表项

### 7. ADR-046 FSM 豁免边界 (Metis 审查修订)

- **算术多周期 (MUL/DIV) 不走 ch_state_machine DSL, 而用 counter + CtrlLink::halt_when**, 3 点 rationale:
  1. **无分支状态**: 算术多周期是线性等待 (IDLE → CALCULATING → DONE 单线), 不像 PTW FSM 有 FAULT 分支
  2. **counter-based 更简洁**: 35 cycle stall 直接用 `int counter = 35; while (counter--) stall;`, FSM DSL 需要额外定义 state + transition, boilerplate 多
  3. **CH_MEM 简化**: `ch_reg<uint<6>> counter` 即可表达, 不需要 FSM 状态机
- ADR-046 §1 加注: "本豁免适用于**协议引擎** (PTW, L1Cache refill, 异常处理路径); 算术多周期不触发本豁免, 走 Plugin `latency_table()` API"

## Capabilities

### New Capabilities

- `cpu-pipeline-multi-cycle`: 定义 Plugin 多周期延迟声明 contract, 含 stall 注入语义

### Modified Capabilities

- `pb-stall-loop`: 新增 "PipeBuilder::run() SHALL consume `Plugin::latency_table()` to inject N-1 stall cycles after multi-cycle opcodes"
- `cpu-pipeline-config-schema`: 加 `cpu.multi_cycle_opcodes: [...]` 字段

## Impact

- **修改代码**:
  - `include/cf/plugin/plugin_base.h` (+15 LOC: latency_table 虚函数)
  - `include/cf/plugin/pipe_builder.h` (+30 LOC: run_with_latency + stall 注入)
  - `ip/cpu/arch/riscv/int_alu.h` (+50 LOC: DIV/REM stall 实现 + latency_table)
  - `ip/cpu/arch/riscv/int_alu_chmem.h` (CH_MEM 版同步改造, +60 LOC)
  - `ip/cpu/plugins/cpu_factory.h` (+5 LOC: 透传 latency_table)
- **新增测试**:
  - `tests/cpu/integration/test_multi_cycle_stall.cpp` (~120 LOC)
  - `tests/cpu/test_int_alu_latency.cpp` (~80 LOC)
- **文档**:
  - `docs/architecture/adr/ADR-051-*.md` (~200 LOC)
  - `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` (+5 LOC: 注释算术多周期不在范围)

## Acceptance

- [ ] `latency_table()` API 实装 (Plugin 默认 + IntAluPlugin 重写)
- [ ] MUL 1 cycle + DIV 35 cycle 实测 (新测试验证)
- [ ] CtrlLink::halt_when 在 stall 期间真生效 (IF/ID/EX/MEM/WB 全 stage stall)
- [ ] `test_multi_cycle_stall` 3/3 PASS
- [ ] `test_int_alu_latency` 3/3 PASS
- [ ] `[riscv-tests]` 40/40 PASS 不回归
- [ ] `[cpu-integration]` 4/4 PASS 不回归
- [ ] 3 门禁全 PASS
- [ ] ADR-051 Accepted + adr.md 注册
- [ ] ADR-046 注释更新（算术多周期不在豁免范围）
- [ ] CHANGELOG v0.8.0 段本 change 条目
- [ ] `openspec archive cpu-pipeline-multi-cycle -y`

## Risk

- **R1 (D2 multi-cycle 优先 5-stage)**: 7-stage superscalar 路径可能与多周期 stall 冲突 → 仅 5-stage 路径实装, superscalar 排除 (Oracle D2 决策)
- **R2 (CH_MEM 同步改造)**: `int_alu_chmem.h` 必须同步改造 → 引入双文件维护风险, 但 `latency_table()` 是 Plugin 虚函数, 业务侧改造一次
- **R3 (依赖 P0#1 canonical-ordering)**: stall 链正确性依赖 P0#1 落地, 否则 stall 时序错 → 强依赖 P0#1 archive
- **R4 (测试 ELF 不可得)**: riscv-tests 中 mul/div 测试可能未 vendor → fallback 写手工测试 (不阻塞)
- **R5 (Latency 35 cycle 估值)**: 真实硬件 MUL/DIV 延迟可低至 1-3 cycle (Karatsuba, SRT 除法), 但 RTL 综合前用保守估值, Phase 6d 后实际测量替换
