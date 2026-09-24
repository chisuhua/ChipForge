## ADDED Requirements

### Requirement: DecoderPlugin 完整 CH_MEM

`ip/cpu/plugins/decode_chmem.h` MUST 完整 RV32I 主 opcode 7 bit + funct3 3 bit + funct7 7 bit 译码, 17 bit 总空间, ~80 项有效指令。

#### Scenario: DecoderPlugin 完整 RV32I 译码

- **WHEN** `decode_chmem.h::DecoderPlugin::decode(instruction)` 跑
- **THEN** 输出 DECODE Payload (含 opcode, funct3, funct7, rd_idx, rs1_idx, rs2_idx, imm, reads_rs1, reads_rs2, writes_rd), 覆盖 RV32I 基础指令 (LUI/AUIPC/JAL/JALR/B-type/I-type/R-type/LOAD/STORE/SYSTEM)

#### Scenario: DecoderPlugin CH_MEM elaborate + toVerilog

- **WHEN** `tests/cpu/test_cpu_decoder_chmem.cpp::decoder_complete_elaborate` 跑
- **THEN** DecoderPlugin 能 elaborate + `toVerilog("decoder.v", ctx)` 输出含 ~80 项 mux_select (RV32I ~80 指令)

### Requirement: BranchPlugin + HazardPlugin 完整 CH_MEM

`branch_chmem.h` + `hazard_chmem.h` MUST 完整实装, 与 5-stage 集成配合。

#### Scenario: BranchPlugin 6-op B-type 完整

- **WHEN** `branch_chmem.h::BranchPlugin::take_branch(funct3, rs1, rs2)` 跑
- **THEN** 返回 6-op B-type select 树 (BEQ/BNE/BLT/BGE/BLTU/BGEU)

#### Scenario: HazardPlugin 完整 RAW 检测 9 条件

- **WHEN** `hazard_chmem.h::HazardPlugin::raw_hazard(id_rs1, id_rs2, ex_rd, mem_rd, wb_rd)` 跑
- **THEN** 返回 9 条件 OR-merge (6 数据 + 3 x0 屏蔽)

#### Scenario: Branch + Hazard 与 CtrlLink 集成

- **WHEN** `pb.register_ctrl_link("decode", std::make_shared<CtrlLink>(halt_when(hazard.raw_hazard(...))));`
- **THEN** HazardPlugin halt 信号接到 ID stage stall, CppHDL Simulator 跑通无 SEGV

### Requirement: CpuFactoryChmem 完整 5-stage 集成

`cpu_factory_chmem.h::build_cpu()` MUST 完整 5-stage (IF/ID/EX/MEM/WB) elaboration, 4+ Plugin 注册 + 正确 stage wiring。

#### Scenario: 5-stage 集成 elaboration

- **WHEN** `CpuFactoryChmem::build_cpu(pb, mem)` 跑
- **THEN** 5 个 stage (IF/ID/EX/MEM/WB) + 4+ Plugin (IBus + Decoder + IntAlu + Branch + Hazard + RegFile) 全部 at_stage 注册

#### Scenario: 5-stage 集成 toVerilog

- **WHEN** `pb.to_verilog("cpu_5stage.v")` 跑
- **THEN** `cpu_5stage.v` 含 5 个 stage always_ff + ~80 项 decoder mux + 11 项 ALU mux + 6 项 branch mux + 9 项 hazard mux + 32 项 regfile select

### Requirement: riscv-tests RV32I 端到端 `tohost=1`

`tests/cpu/test_cpu_chmem_riscv_tests.cpp` 5 指令 add/addi/auipc/jal/beq 端到端 CppHDL sim `tohost=1` MUST PASS。

#### Scenario: add.elf tohost=1 端到端

- **WHEN** 编译 riscv-tests rv32ui-p-add ELF + CppHDL Simulator 跑 (10+ cycle)
- **THEN** tohost=1 (内存写入 0x80000000 = 1)

#### Scenario: addi.elf tohost=1 端到端

- **WHEN** 同上 addi ELF
- **THEN** tohost=1

#### Scenario: auipc.elf / jal.elf / beq.elf tohost=1

- **WHEN** 同上 5 指令全部
- **THEN** 全部 tohost=1

### Requirement: Verilator 后端集成

`tools/verilator_runner/main.cpp` MUST 新建, Verilator 编译 + sim MUST 与 CppHDL sim byte-equal。

#### Scenario: Verilator --cc --exe 编译

- **WHEN** `verilator --cc cpu_5stage.v --exe main.cpp --build` 跑
- **THEN** 退出码 0, 生成 VL1Cache/VRegFile shared library

#### Scenario: Verilator sim 跑 add.elf

- **WHEN** Verilator sim 跑 add.elf (10+ cycle)
- **THEN** tohost=1, 与 CppHDL sim 行为 byte-equal

#### Scenario: Verilator sim 与 CppHDL sim byte-equal

- **WHEN** 5 指令 5 个 ELF 在两个 simulator 各跑 20 cycle
- **THEN** trace (PC + register state) byte-equal

### Requirement: MMU/PTW 多周期 FSM

`ip/cpu/plugins/mmu_ptw_chmem.h` MUST 用 `chlib::ch_state_machine<PTW_State, 6>` DSL 实装 5 状态 sv39 walk FSM。

#### Scenario: ch_state_machine DSL 5 状态

- **WHEN** `ch_state_machine<PTW_State, 6>` 构造 (PTW_State 枚举 IDLE/L0_WAIT/L1_WAIT/L2_WAIT/DONE/FAULT)
- **THEN** 6 个 state + set_entry + build() 全部执行, emit Verilog 含 state_reg

#### Scenario: CF_PLUGIN_USE_FSM_EXEMPT 豁免

- **WHEN** `mmu_ptw_chmem.h` 顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`
- **THEN** `check_plugin_portability.sh` Check 5 (`if(ch_bool)`) 跳过该 Plugin

#### Scenario: Verilator sim 跑 sv39 walk

- **WHEN** MMU enable=true, 触发 page fault + sv39 3-level walk
- **THEN** Verilator sim 跑通 IDLE → L0_WAIT → L1_WAIT → L2_WAIT → DONE 状态转移

### Requirement: L1Cache refill FSM

`ip/cache/tlm/l1_cache_refill_fsm_chmem.h` MUST 用 `chlib::ch_state_machine<CacheState, 4>` 实装 4 状态 FSM。

#### Scenario: ch_state_machine 4 状态

- **WHEN** `ch_state_machine<CacheState, 4>` 构造 (IDLE/LOOKUP/MISS/REFILL_WAIT)
- **THEN** 4 个 state + emit Verilog 含 state_reg

### Requirement: Harness 迁移

`tools/cpu_sim/main.cpp` MUST 从 `pb.run()` 迁到 `pb.elaborate()` + `ch::Simulator::tick()` / Verilator, MUST 保留 CLI 兼容。

#### Scenario: --elf flag 兼容

- **WHEN** `./cpu_sim --elf add.elf` 跑
- **THEN** 同 TLM baseline 行为, tohost=1 (与 `[cpu-l1-mmu-demo]` 5 用例对齐)

#### Scenario: --mode chmem flag

- **WHEN** `./cpu_sim --mode chmem --elf add.elf` 跑
- **THEN** 切到 CH_MEM 模式, 跑 CppHDL sim (替代 TLM sim)

### Requirement: CI 门禁保留 + 扩展

`check_plugin_portability.sh` MUST 从 8/8 扩展到 9/9 PASS (新增 Check 9 CF_PLUGIN_USE_FSM_EXEMPT 标注), `verify_adr.sh` MUST 维持 0 FAILED, TLM baseline MUST 0 回归。

#### Scenario: check_plugin_portability.sh 9/9 PASS

- **WHEN** Phase 6d 6d.6 完成后 `bash tools/check_plugin_portability.sh` 跑
- **THEN** 9/9 PASS (含新增 Check 9 CF_PLUGIN_USE_FSM_EXEMPT grep 验证)

#### Scenario: verify_adr.sh 0 FAILED

- **WHEN** ADR-040 v3.0 (Verilator 集成段) + ADR-037 v2.0 + ADR-046 全部更新后
- **THEN** `bash tools/verify_adr.sh` 仍 0 FAILED

#### Scenario: TLM baseline chipforge_tests 0 回归

- **WHEN** `./bin/chipforge_tests` 全量跑
- **THEN** 391 PASS / 12 baseline 已知 FAIL 保持不变, 不新增回归
