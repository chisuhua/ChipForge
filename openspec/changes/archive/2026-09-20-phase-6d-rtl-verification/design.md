# Phase 6d: 5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM — Design

> 详细设计文档

## 6d.1 DecoderPlugin 完整 CH_MEM 设计

### RV32I 译码空间

| 字段 | 位宽 | 说明 |
|------|------|------|
| opcode | 7 bit | 主 opcode (inst[6:0]) |
| funct3 | 3 bit | inst[14:12] |
| funct7 | 7 bit | inst[31:25] |
| rd_idx | 5 bit | inst[11:7] |
| rs1_idx | 5 bit | inst[19:15] |
| rs2_idx | 5 bit | inst[24:20] |

总空间 17 bit, RV32I 有效指令约 80 项 (R/I/B/S/U/J-type)。

### DecoderPlugin 实现骨架

```cpp
class DecoderPlugin : public PluginBase {
  void setup(PipeBuilder& pb) override {
    pb.at_stage("decode", Phase::NORMAL, [this, &pb] { this->decode(pb); });
  }
  
  void decode(PipeBuilder& pb) {
    auto* n = pb.node_of_logic_stage("decode").get();
    const auto& fetch = n->operator()(KeyType::INSTRUCTION);
    
    // 提取字段
    ch_uint<7> opcode = fetch.bits<6, 0>();
    ch_uint<3> funct3 = fetch.bits<14, 12>();
    ch_uint<7> funct7 = fetch.bits<31, 25>();
    ch_uint<5> rd = fetch.bits<11, 7>();
    ch_uint<5> rs1 = fetch.bits<19, 15>();
    ch_uint<5> rs2 = fetch.bits<24, 20>();
    
    // 写 DECODE Payload
    auto& dec = n->operator()(KeyType::DECODE);
    dec.opcode = opcode;
    dec.funct3 = funct3;
    dec.funct7 = funct7;
    dec.rd_idx = rd;
    dec.rs1_idx = rs1;
    dec.rs2_idx = rs2;
    dec.reads_rs1 = ...;  // select 树 (~80 项)
    dec.reads_rs2 = ...;
    dec.writes_rd = ...;
    dec.imm = ...;  // 立即数提取 (I/S/B/U/J-type 不同)
  }
};
```

## 6d.2 Branch + Hazard 完整版 设计

### BranchPlugin 6-op B-type

```cpp
ch_bool take_branch(ch_uint<3> funct3, ch_uint<32> rs1, ch_uint<32> rs2) {
  // funct3 编码: 000=BEQ, 001=BNE, 100=BLT, 101=BGE, 110=BLTU, 111=BGEU
  return select(funct3 == ch_uint<3>(0x0_d), rs1 == rs2,         // BEQ
         select(funct3 == ch_uint<3>(0x1_d), rs1 != rs2,         // BNE
         select(funct3 == ch_uint<3>(0x4_d), signed_lt(rs1, rs2),  // BLT
         select(funct3 == ch_uint<3>(0x5_d), signed_ge(rs1, rs2),  // BGE
         select(funct3 == ch_uint<3>(0x6_d), rs1 < rs2,          // BLTU
         select(funct3 == ch_uint<3>(0x7_d), rs1 >= rs2,         // BGEU
                ch_bool(false))))));
}
```

### HazardPlugin 9 条件 OR-merge

```cpp
ch_bool raw_hazard_complete(
    ch_uint<5> id_rs1, ch_uint<5> id_rs2,
    ch_uint<5> ex_rd, ch_uint<5> mem_rd, ch_uint<5> wb_rd) {
  // 数据 RAW 检测
  ch_bool ex_match  = (id_rs1 == ex_rd)  || (id_rs2 == ex_rd);
  ch_bool mem_match = (id_rs1 == mem_rd) || (id_rs2 == mem_rd);
  ch_bool wb_match  = (id_rs1 == wb_rd)  || (id_rs2 == wb_rd);
  
  // x0 屏蔽 (写 x0 是 noop)
  ch_bool ex_valid  = (ex_rd != ch_uint<5>(0_d));
  ch_bool mem_valid = (mem_rd != ch_uint<5>(0_d));
  ch_bool wb_valid  = (wb_rd != ch_uint<5>(0_d));
  
  return (ex_match && ex_valid) || (mem_match && mem_valid) || (wb_match && wb_valid);
}
```

## 6d.3 CpuFactoryChmem 5-stage 集成 设计

### 5-stage 拓扑

```
IF  →  ID  →  EX  →  MEM  →  WB
       │      │
       │      └─→ BranchPlugin (branch_decision)
       │      └─→ IntAluPlugin (compute_result)
       │      └─→ HazardPlugin (stall_gate)
       ├─→ DecoderPlugin
       └─→ RegFilePlugin (read_rs1/rs2)
              │
              └─→ writeback (write_rd)
```

### Plugin at_stage 注册顺序

```cpp
void CpuFactoryChmem::build_cpu(PipeBuilder& pb) {
  // 1. EARLY-stage payload pre-population (沿用 v0.3.1 M6 fix)
  pb.at_stage("fetch", Phase::EARLY, [this, &pb] { /* pre-populate PC, INSTRUCTION */ });
  pb.at_stage("decode", Phase::EARLY, [this, &pb] { /* pre-populate DECODE */ });
  pb.at_stage("execute", Phase::EARLY, [this, &pb] { /* pre-populate EX result */ });
  pb.at_stage("memory", Phase::EARLY, [this, &pb] { /* pre-populate MEM data */ });
  pb.at_stage("writeback", Phase::EARLY, [this, &pb] { /* pre-populate WB data */ });
  
  // 2. Plugin 注册
  auto ibus = std::make_unique<IBusPlugin>();
  auto decoder = std::make_unique<DecoderPlugin>();
  auto int_alu = std::make_unique<IntAluPlugin>();
  auto branch = std::make_unique<BranchPlugin>();
  auto hazard = std::make_unique<HazardPlugin>();
  auto reg_file = std::make_unique<RegFilePlugin>();
  
  // 3. Stage linking (M2S connection)
  pb.register_stage_payload_connector<ch_uint<32>>("decode", KeyType::PC, ...);
  pb.register_stage_payload_connector<ch_uint<32>>("decode", KeyType::INSTRUCTION, ...);
  pb.register_stage_payload_connector<...>("execute", KeyType::DECODE, ...);
  // ... (5 stage × 多个 payload)
  
  // 4. Hazard → CtrlLink wiring
  pb.register_ctrl_link("decode", std::make_shared<CtrlLink>(
      halt_when(hazard->raw_hazard_complete(...))));
  
  // 5. Plugin 注册
  pb.register_plugin(std::move(ibus));
  pb.register_plugin(std::move(decoder));
  // ...
}
```

## 6d.4 riscv-tests RV32I 端到端 设计

### ELF 编译流程

```bash
# 编译 5 指令 ELF (依赖 Phase 6d prereq #1 riscv64 工具链)
for test in add addi auipc jal beq; do
  riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -o ${test}.elf \
    riscv-tests/isa/rv32ui/${test}.S
done
```

### CppHDL Simulator 端到端测试

```cpp
TEST_CASE("riscv_tests_rv32ui_add_tohost_1") {
  // 1. 加载 add.elf 到 PicolibcHostMemory
  PicolibcHostMemory mem;
  mem.load_elf("/tmp/add.elf");
  
  // 2. CpuFactoryChmem 5-stage 集成
  ch::core::context ctx("riscv_tests_ctx");
  ctx.set_as_current_context();
  PipeBuilder pb(&ctx);
  CpuFactoryChmem factory;
  factory.build_cpu(pb, &mem);
  
  // 3. 仿真 100 cycle
  pb.elaborate(ctx);
  pb.to_verilog("/tmp/cpu_riscv_tests.v");
  ch::Simulator sim(&ctx);
  sim.reset();
  
  for (int cycle = 0; cycle < 100; ++cycle) {
    sim.tick();
  }
  
  // 4. 验证 tohost = 1
  uint32_t tohost = mem.read_word(0x80000000);
  REQUIRE(tohost == 1);
}
```

## 6d.5 Verilator 后端集成 设计

### Verilator build 命令

```bash
verilator --cc cpu_5stage.v --exe main.cpp --build \
  --top-module TopModule \
  -Iinclude -Ibuild/_deps/install/include
```

### VL1Cache + VRegFile 编译产物

```
obj_dir/
├── VL1Cache.h
├── VL1Cache.cpp
├── VL1Cache___024unit.h
└── VTopModule.mk
```

### Verilator sim trace 对比

```cpp
TEST_CASE("verilator_sim_byte_equal_cppHDL_sim") {
  // 1. 跑 CppHDL Simulator 100 cycle, dump trace
  auto cpp_trace = cppHDL_simulate(add_elf, /* n_cycles */ 100);
  
  // 2. 跑 Verilator sim 100 cycle (separate binary), dump trace
  auto v_trace = verilator_simulate(add_elf, /* n_cycles */ 100);
  
  // 3. Byte-equal
  for (size_t i = 0; i < cpp_trace.size(); ++i) {
    REQUIRE(cpp_trace[i] == v_trace[i]);
  }
}
```

## 6d.6 MMU/PTW 多周期 FSM 设计 (ADR-046 豁免, Oracle 修正: sv32)

### ch_state_machine DSL

```cpp
// ip/cpu/plugins/mmu_ptw_chmem.h
#define CF_PLUGIN_USE_FSM_EXEMPT  // ADR-046: 多周期 FSM 豁免

// Oracle 2026-09-20 修正: sv32 2-level walk (对齐现有 ip/mmu/ MMUPlugin), 不是 sv39 3-level
enum class PTW_State { IDLE, L0_WAIT, L1_WAIT, DONE, FAULT };

class MMUPTWPlugin : public PluginBase {
  void setup(PipeBuilder& pb) override {
    pb.at_stage("execute", Phase::NORMAL, [this, &pb] { this->ptw(pb); });
  }
  
  void ptw(PipeBuilder& pb) {
    ch_state_machine<PTW_State, 5> fsm;  // Oracle: sv32 = 4 状态, 加 FAULT = 5
    
    fsm.state(PTW_State::IDLE)
      .on_active([&] {
        if (ptw_req) fsm.transition_to(PTW_State::L0_WAIT);
      });
    
    fsm.state(PTW_State::L0_WAIT)
      .on_active([&] {
        if (mem_resp_valid && mem_resp_l0) {
          fsm.transition_to(PTW_State::L1_WAIT);
        }
      });
    
    fsm.state(PTW_State::L1_WAIT)
      .on_active([&] {
        if (mem_resp_valid && mem_resp_l1) {
          if (access_fault) fsm.transition_to(PTW_State::FAULT);
          else fsm.transition_to(PTW_State::DONE);
        }
      });
    
    // ... DONE, FAULT
    
    fsm.set_entry(PTW_State::IDLE);
    fsm.build();  // emit state_reg + transitions to Verilog
  }
};
```

**关键约束**: `ch_state_machine` 简化实现不能在 CppHDL Simulator 跑 cycle-accurate, **必须 Verilator 验证**。Oracle 修正: sv32 是项目当前 MMU 模式 (AGENTS.md `enable_mmu=true, sv32`), 与现有 ip/mmu/ 代码 + 测试对齐, 避免范围扩大。

## 6d.7 L1Cache refill FSM 设计 (同 6d.6)

```cpp
enum class CacheState { IDLE, LOOKUP, MISS, REFILL_WAIT };

ch_state_machine<CacheState, 4> fsm;
fsm.state(CacheState::IDLE).on_active([&] { ... });
fsm.state(CacheState::LOOKUP).on_active([&] { ... });
fsm.state(CacheState::MISS).on_active([&] {
  if (mem_resp_valid) fsm.transition_to(CacheState::REFILL_WAIT);
});
fsm.state(CacheState::REFILL_WAIT).on_active([&] {
  // ... write refill data to cache
  fsm.transition_to(CacheState::IDLE);
});
```

## 6d.8 Harness 迁移 设计

### cpu_sim/main.cpp 切换

```cpp
// 旧 (TLM 模式)
pb.run();

// 新 (CH_MEM 模式)
pb.elaborate(ctx);
pb.to_verilog("/tmp/cpu_sim.v");
ch::Simulator sim(&ctx);
sim.reset();
while (!tohost_detected()) {
  sim.tick();
}
```

### CLI 兼容

```bash
# TLM 模式 (保留 deprecated, 兼容性)
./cpu_sim --elf add.elf --mode tlm

# CH_MEM 模式 (新正道)
./cpu_sim --elf add.elf --mode chmem

# Verilator 模式 (Phase 6d.5 验证用)
./cpu_sim --elf add.elf --mode verilator
```

## 风险与缓解 设计

| 风险 | 缓解策略 |
|------|----------|
| 6d.4 riscv-tests 端到端超时 | 降级到单指令 add.elf + 完整 5 指令推迟 Phase 6e |
| 6d.5 Verilator 综合失败 | 仅交付"可仿真 RTL" (与 Phase 6c 一致) |
| 6d.6 MMU PTW Verilator sim 不工作 | 仅交付 PoC, 完整化推迟 Phase 6e |
| 6d.8 Harness 迁移破坏 CPU 业务代码 | 保留 TLM 入口 (deprecated), `--mode` flag 切换 |

## 时间盒汇总

| 子阶段 | 估时 | 关键产出 |
|------|------|----------|
| 6d.1 Decoder | 1 周 | `decode_chmem.h` 完整 + 80 mux_select |
| 6d.2 Branch + Hazard | 1.5 周 | 6+9 mux_select + CtrlLink 集成 |
| 6d.3 5-stage 集成 | 1.5 周 | `cpu_5stage.v` 完整 |
| 6d.4 riscv-tests | 2 周 | 5 指令 tohost=1 |
| 6d.5 Verilator | 1.5 周 | VL1Cache + sim byte-equal |
| 6d.6 MMU FSM | 1 周 | ch_state_machine + Verilator sim |
| 6d.7 L1Cache FSM | 0.5 周 | ch_state_machine |
| 6d.8 Harness | 1 周 | main.cpp 切换 + CLI 兼容 |
| **总计** | **~10-12 周** | **CHANGELOG v0.4.0 + ADR-040 v3.0** |
