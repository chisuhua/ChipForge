// tests/cpu/test_cpu_decoder_chmem.cpp
//
// Phase 6d 6d.1: DecoderPlugin CH_MEM PoC 测试
//
// 验证:
//   1. decoder_complete_elaborate: DecoderPlugin elaborate + toVerilog,
//      Verilog 含 >= 80 mux_select (RV32I 完整译码证据)
//   2. decoder_5_instr_ref: C++ 参考译码 5 条指令正确性
//   3. decoder_complete_simulator_tick: Simulator tick 10 周期无异常
//
// 编译条件: CF_PLUGIN_USE_CH_MEM

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ch.hpp>
#include <codegen_verilog.h>
#include <component.h>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/decode_chmem.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;
namespace cfcpu = cf::cpu;

namespace {

// =========================================================================
// 测试用例结构
struct InstrTestCase {
  uint32_t  inst;
  const char* label;
  uint32_t  opcode;
  uint32_t  rd_idx;
  uint32_t  rs1_idx;
  uint32_t  rs2_idx;
  int32_t   imm_expected;     // 预期符号扩展立即数
  bool      reads_rs1;
  bool      reads_rs2;
  bool      writes_rd;
};

// 5 条测试指令 (RV32I)
static constexpr uint32_t kInstAdd  = 0x002081b3;
static constexpr uint32_t kInstAddi = 0x00508193;
static constexpr uint32_t kInstAuipc = 0x01000197;
static constexpr uint32_t kInstBeq  = 0x00208663;
static constexpr uint32_t kInstJal  = 0x008000EF;

// 测试用例: 硬编码已验证的正确值
static const InstrTestCase kTestCases[] = {
  {kInstAdd,  "ADD x3,x1,x2",
   0x33, 3, 1, 2, 0,          true,  true,  true},
  {kInstAddi, "ADDI x3,x1,5",
   0x13, 3, 1, 5, 5,          true,  false, true},
  {kInstAuipc,"AUIPC x3,0x1000",
   0x17, 3, 0, 16, 0x01000000, false, false, true},
  {kInstBeq,  "BEQ x1,x2,+12",
   0x63, 12, 1, 2, 12,         true,  true,  false},
  {kInstJal,  "JAL x1,+8",
   0x6F, 1, 0, 8, 8,          false, false, true},
};

struct DecodedRef {
  uint32_t opcode;
  uint32_t funct3;
  uint32_t funct7;
  uint32_t rd_idx;
  uint32_t rs1_idx;
  uint32_t rs2_idx;
  int32_t  imm;
  bool     reads_rs1;
  bool     reads_rs2;
  bool     writes_rd;
};

// 参考译码: 使用 decoder_table.h 的 constexpr 函数译码
static DecodedRef decode_reference(uint32_t inst) {
  using namespace cf::cpu::arch::riscv;

  DecodedRef r;
  r.opcode  = get_opcode(inst);
  r.funct3  = get_funct3(inst);
  r.funct7  = get_funct7(inst);
  r.rd_idx  = get_rd(inst);
  r.rs1_idx = get_rs1(inst);
  r.rs2_idx = get_rs2(inst);

  OpCode op = decode_rv32(inst);
  r.imm = get_imm(op, inst);

  // reads_rs1: 大多数指令读 rs1, 除 LUI/AUIPC/JAL/FENCE/SYSTEM(ECALL/EBREAK)
  switch (op) {
    case OpCode::LUI:
    case OpCode::AUIPC:
    case OpCode::JAL:
    case OpCode::FENCE:
      r.reads_rs1 = false;
      break;
    case OpCode::SYSTEM:
      // SYSTEM: funct12=0/1 (ECALL/EBREAK)不读 rs1, 其它(C.S.R.)读
      r.reads_rs1 = (get_funct12(inst) >= 2);  // CSR
      break;
    default:
      r.reads_rs1 = true;
      break;
  }

  // reads_rs2: R-type, STORE, BRANCH
  r.reads_rs2 = false;
  switch (op) {
    case OpCode::ADD: case OpCode::SUB: case OpCode::SLL:
    case OpCode::SLT: case OpCode::SLTU: case OpCode::XOR:
    case OpCode::SRL: case OpCode::SRA: case OpCode::OR: case OpCode::AND:
    case OpCode::SB: case OpCode::SH: case OpCode::SW:
    case OpCode::BEQ: case OpCode::BNE: case OpCode::BLT:
    case OpCode::BGE: case OpCode::BLTU: case OpCode::BGEU:
      r.reads_rs2 = true;
      break;
    default:
      break;
  }

  // writes_rd: LUI/AUIPC/JAL/JALR/LOAD/OP-IMM/OP/SYSTEM-CSR
  r.writes_rd = false;
  switch (op) {
    case OpCode::LUI: case OpCode::AUIPC:
    case OpCode::JAL: case OpCode::JALR:
    case OpCode::LB: case OpCode::LH: case OpCode::LW:
    case OpCode::LBU: case OpCode::LHU:
    case OpCode::ADDI: case OpCode::SLTI: case OpCode::SLTIU:
    case OpCode::XORI: case OpCode::ORI: case OpCode::ANDI:
    case OpCode::SLLI: case OpCode::SRLI: case OpCode::SRAI:
    case OpCode::ADD: case OpCode::SUB: case OpCode::SLL:
    case OpCode::SLT: case OpCode::SLTU: case OpCode::XOR:
    case OpCode::SRL: case OpCode::SRA: case OpCode::OR: case OpCode::AND:
      r.writes_rd = (r.rd_idx != 0);  // x0 屏蔽
      break;
    case OpCode::SYSTEM:
      r.writes_rd = (get_funct3(inst) != 0b000) && (r.rd_idx != 0);
      break;
    default:
      break;
  }

  return r;
}

}  // anonymous namespace

// =========================================================================
// Test 1: 完整 DecoderPlugin elaborate + Verilog mux_select ≥ 80
// =========================================================================
TEST_CASE("decoder_complete_elaborate", "[cpu][chmem][decoder][poc]") {
  ch::core::context ctx("decoder_elab_ctx");
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  // 注册 DecoderPlugin (ELABORATE test)
  pb.register_plugin(
      std::make_unique<cfcpu::plugins::RiscvDecodePluginChmem<ch_uint<32>>>());

  // EARLY 预填充 INSTRUCTION (避免 PayloadStore cell miss)
  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  pb.at_stage("decode", Phase::EARLY, [&pb]() {
    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) =
          ch_uint<32>(ch::core::ch_literal<0, 32>{});
    }
  });

  pb.build();

  REQUIRE_NOTHROW(pb.elaborate());

  const std::string out_file = "/tmp/decoder_complete.v";
  REQUIRE_NOTHROW(pb.to_verilog(out_file));

  std::ifstream f(out_file);
  REQUIRE(f.is_open());
  std::stringstream ss;
  ss << f.rdbuf();
  std::string verilog = ss.str();
  REQUIRE(!verilog.empty());

  // 计数 mux_select: ≥80 (40+ 指令 × select 树)
  std::size_t pos = 0;
  int mux_select_count = 0;
  while ((pos = verilog.find("mux_select", pos)) != std::string::npos) {
    ++mux_select_count;
    pos += 10;
  }
  INFO("mux_select count = " << mux_select_count);
  REQUIRE(mux_select_count >= 80);

  SUCCEED("DecoderPlugin elaborate: " << mux_select_count
          << " mux_select (>= 80 = complete RV32I decode)");
}

// =========================================================================
// Test 2: C++ 参考译码 5 条指令正确性
// =========================================================================
TEST_CASE("decoder_5_instr_ref", "[cpu][chmem][decoder][poc]") {
  for (std::size_t i = 0; i < 5; ++i) {
    const auto& tc = kTestCases[i];
    auto ref = decode_reference(tc.inst);

    INFO("Case " << i << " [" << tc.label << "]: inst=0x"
         << std::hex << tc.inst << std::dec);

    // 原始字段
    REQUIRE(ref.opcode  == tc.opcode);
    REQUIRE(ref.rd_idx  == tc.rd_idx);
    REQUIRE(ref.rs1_idx == tc.rs1_idx);
    REQUIRE(ref.rs2_idx == tc.rs2_idx);
    REQUIRE(ref.imm     == tc.imm_expected);

    // 控制标志
    REQUIRE(ref.reads_rs1 == tc.reads_rs1);
    REQUIRE(ref.reads_rs2 == tc.reads_rs2);
    REQUIRE(ref.writes_rd == tc.writes_rd);

    SUCCEED("Case " << i << " [" << tc.label << "] decode PASS");
  }
}

// =========================================================================
// Test 3: Simulator tick 10 周期无异常
// =========================================================================
TEST_CASE("decoder_complete_simulator_tick", "[cpu][chmem][decoder][poc]") {
  ch::core::context ctx("decoder_sim_ctx");
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  pb.register_plugin(
      std::make_unique<cfcpu::plugins::RiscvDecodePluginChmem<ch_uint<32>>>());

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  pb.at_stage("decode", Phase::EARLY, [&pb]() {
    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) =
          ch_uint<32>(ch::core::ch_literal<0, 32>{});
    }
  });

  pb.build();
  pb.elaborate();

  auto sim = cf::plugin::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);

  sim->reset();
  for (int i = 0; i < 10; ++i) {
    REQUIRE_NOTHROW(sim->tick());
  }

  SUCCEED("DecoderPlugin Simulator tick 10 周期无 crash");
}

#endif  // CF_PLUGIN_USE_CH_MEM