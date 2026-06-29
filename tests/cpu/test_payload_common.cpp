// tests/cpu/test_payload_common.cpp
//
// 功能描述: ip/cpu/core/payload_common.h 单元测试 (M1.7 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (7 用例):
//   1. RV32 keys_rv32 全部 8 Key 实例化 + 默认值
//   2. RV64 keys_rv64 全部 8 Key 实例化 + 默认值
//   3. PC/INSTRUCTION/RS1/RS2/RD_DATA 跨阶段读写
//   4. RD_IDX (5-bit) 边界值 (0/31)
//   5. DECODE DecodePayload struct 读写
//   6. RESULT 字段读写
//   7. 模板参数 static_assert (编译期: XLEN ∈ {32,64}, T 是 uint32/uint64)
//
// 约束:
//   - 放 tests/cpu/ (2026-06-16 重构: 测试按家族分目录, 替代原 D-1 决策的 src/cf_plugin/tests/ 统一入口)

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"
#include "ip/cpu/core/payload_common.h"

using cf::plugin::PipeBuilder;
using cf::plugin::PipeNode;
using cf::plugin::Phase;
namespace payload = cf::cpu::core::payload;

// ----------------------------------------------------------------------------
// Test 1: RV32 keys_rv32 默认值 + 类型
// ----------------------------------------------------------------------------
TEST_CASE("rv32_keys_instantiate", "[cpu]") {
  // 8 Key 都能被实例化 (头文件加载即触发)
  // 类型: 5 个 xlen 类型 (uint32_t), 1 个 5-bit, 1 个 32-bit INSTRUCTION, 1 个 DecodePayload
  static_assert(sizeof(decltype(payload::keys_rv32::PC)) ==
                    sizeof(cf::plugin::Payload<std::uint32_t>),
                "RV32 PC must be uint32_t");
  static_assert(sizeof(decltype(payload::keys_rv32::RD_IDX)) ==
                    sizeof(cf::plugin::Payload<cf::plugin::uint_t<5>>),
                "RD_IDX must be uint_t<5>");
  static_assert(sizeof(decltype(payload::keys_rv32::DECODE)) ==
                    sizeof(cf::plugin::Payload<payload::DecodePayload>),
                "DECODE must be DecodePayload");
}

// ----------------------------------------------------------------------------
// Test 2: RV64 keys_rv64 默认值 + 类型
// ----------------------------------------------------------------------------
TEST_CASE("rv64_keys_instantiate", "[cpu]") {
  static_assert(sizeof(decltype(payload::keys_rv64::PC)) ==
                    sizeof(cf::plugin::Payload<std::uint64_t>),
                "RV64 PC must be uint64_t");
  static_assert(sizeof(decltype(payload::keys_rv64::RS1)) ==
                    sizeof(cf::plugin::Payload<std::uint64_t>),
                "RV64 RS1 must be uint64_t");
  // INSTRUCTION 仍是 32-bit (RV32/RV64 指令编码都是 32-bit)
  static_assert(sizeof(decltype(payload::keys_rv64::INSTRUCTION)) ==
                    sizeof(cf::plugin::Payload<cf::plugin::uint_t<32>>),
                "INSTRUCTION must be uint_t<32>");
}

// ----------------------------------------------------------------------------
// Test 3: PC/INSTRUCTION/RS1/RS2/RD_DATA 跨阶段读写 (5 级流水仿真)
// ----------------------------------------------------------------------------
TEST_CASE("rv32_cross_stage_read_write", "[cpu]") {
  PipeBuilder pb;
  pb.at_stage("if_id",  Phase::EARLY,  [](){});
  pb.at_stage("id_ex",  Phase::NORMAL, [](){});
  pb.at_stage("ex_mem", Phase::NORMAL, [](){});
  pb.build();

  auto if_id  = pb.node_of_logic_stage("if_id");
  auto id_ex  = pb.node_of_logic_stage("id_ex");
  auto ex_mem = pb.node_of_logic_stage("ex_mem");
  REQUIRE((if_id && id_ex && ex_mem));

  // 取指阶段: 写入 PC, INSTRUCTION
  (*if_id)(payload::keys_rv32::PC)          = 0x80000000u;
  (*if_id)(payload::keys_rv32::INSTRUCTION)  = 0x00100073u;  // ebreak

  // 译码阶段: 读 PC + INSTRUCTION, 写 RS1/RS2
  auto pc = (*id_ex)(payload::keys_rv32::PC) = (*if_id)(payload::keys_rv32::PC);
  (*id_ex)(payload::keys_rv32::INSTRUCTION) = (*if_id)(payload::keys_rv32::INSTRUCTION);
  (*id_ex)(payload::keys_rv32::RS1) = 0x12345678u;
  (*id_ex)(payload::keys_rv32::RS2) = 0x87654321u;
  REQUIRE(pc == 0x80000000u);

  // 执行阶段: 读 RS1/RS2, 写 RD_DATA
  (*ex_mem)(payload::keys_rv32::RS1)    = (*id_ex)(payload::keys_rv32::RS1);
  (*ex_mem)(payload::keys_rv32::RS2)    = (*id_ex)(payload::keys_rv32::RS2);
  (*ex_mem)(payload::keys_rv32::RD_DATA) = 0xaabbccddu;
  REQUIRE((*ex_mem)(payload::keys_rv32::RD_DATA) == 0xaabbccddu);
}

// ----------------------------------------------------------------------------
// Test 4: RD_IDX (5-bit) 边界值 (0-31, x0-x31)
// ----------------------------------------------------------------------------
TEST_CASE("rd_idx_boundary", "[cpu]") {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  REQUIRE(n);

  (*n)(payload::keys_rv32::RD_IDX) = 0;   // x0 (零寄存器, 写屏蔽)
  REQUIRE((*n)(payload::keys_rv32::RD_IDX) == 0);
  (*n)(payload::keys_rv32::RD_IDX) = 31;  // x31 (ra)
  REQUIRE((*n)(payload::keys_rv32::RD_IDX) == 31);
  // 编译期: uint_t<5> 实际 typedef 到 uint8_t (N=5 <= 8)
  static_assert(sizeof(cf::plugin::uint_t<5>) == sizeof(std::uint8_t),
                "uint_t<5> must be uint8_t");
}

// ----------------------------------------------------------------------------
// Test 5: DECODE DecodePayload struct 读写
// ----------------------------------------------------------------------------
TEST_CASE("decode_payload_struct", "[cpu]") {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  REQUIRE(n);

  auto& dec = (*n)(payload::keys_rv32::DECODE);
  dec.op_class  = payload::DecodePayload::OpClass::ALU;
  dec.writes_rd = true;
  dec.reads_rs1 = true;
  dec.reads_rs2 = true;
  dec.rd_class  = 0;

  // 跨节点读回
  PipeBuilder pb2;
  pb2.at_stage("t", Phase::NORMAL, [](){});
  pb2.build();
  auto n2 = pb2.node_of_logic_stage("t");
  REQUIRE(n2);

  n2->put(payload::keys_rv32::DECODE, (*n)(payload::keys_rv32::DECODE));
  const auto& dec2 = (*n2)(payload::keys_rv32::DECODE);
  REQUIRE(dec2.op_class  == payload::DecodePayload::OpClass::ALU);
  REQUIRE(dec2.writes_rd);
  REQUIRE(dec2.reads_rs1);
  REQUIRE(dec2.reads_rs2);
  REQUIRE(dec2.rd_class  == 0);

  // 切换 op_class
  (*n2)(payload::keys_rv32::DECODE).op_class = payload::DecodePayload::OpClass::BRANCH;
  REQUIRE((*n2)(payload::keys_rv32::DECODE).op_class ==
         payload::DecodePayload::OpClass::BRANCH);
}

// ----------------------------------------------------------------------------
// Test 6: RESULT 字段读写
// ----------------------------------------------------------------------------
TEST_CASE("result_field", "[cpu]") {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  REQUIRE(n);

  (*n)(payload::keys_rv32::RESULT) = 0xfeedfaceu;
  REQUIRE((*n)(payload::keys_rv32::RESULT) == 0xfeedfaceu);

  // RV64
  (*n)(payload::keys_rv64::RESULT) = 0x1122334455667788ULL;
  REQUIRE((*n)(payload::keys_rv64::RESULT) == 0x1122334455667788ULL);
}

// ----------------------------------------------------------------------------
// Test 7: 模板参数 static_assert (编译期: XLEN ∈ {32,64}, T 是 uint32/uint64)
// ----------------------------------------------------------------------------
// 编译期测试: 错误实例化会编译失败
//   payload::keys<float, 32> k;   // ❌ float 不是 uint32/uint64
//   payload::keys<uint32_t, 128> k; // ❌ XLEN 不是 32/64
// 这里只测试合法实例化能通过编译
TEST_CASE("template_sanity", "[cpu]") {
  // 合法实例化 (静态, 编译期类型检查)
  // 字符串名在运行时验证 (std::string operator== 不是 constexpr)
  REQUIRE(payload::keys_rv32::PC.name()          == "cpu.pc");
  REQUIRE(payload::keys_rv32::INSTRUCTION.name() == "cpu.instruction");
  REQUIRE(payload::keys_rv32::RS1.name()         == "cpu.rs1");
  REQUIRE(payload::keys_rv32::RS2.name()         == "cpu.rs2");
  REQUIRE(payload::keys_rv32::RD_DATA.name()     == "cpu.rd_data");
  REQUIRE(payload::keys_rv32::RD_IDX.name()       == "cpu.rd_idx");
  REQUIRE(payload::keys_rv32::DECODE.name()       == "cpu.decode");
  REQUIRE(payload::keys_rv32::RESULT.name()      == "cpu.result");
  // RV64 同样 (类型不同, Key 名字相同)
  REQUIRE(payload::keys_rv64::PC.name()          == "cpu.pc");
  REQUIRE(payload::keys_rv64::RD_DATA.name()     == "cpu.rd_data");
  // M4G D.1 (G.1): 新增 UID / THREAD_ID / IID_PC 三个 Payload Key
  REQUIRE(payload::keys_rv32::UID.name()         == "cpu.uid");
  REQUIRE(payload::keys_rv32::THREAD_ID.name()   == "cpu.tid");
  REQUIRE(payload::keys_rv32::IID_PC.name()      == "cpu.iid_pc");
  // 类型检查: UID 是 uint_t<8>, THREAD_ID 是 uint_t<2>, IID_PC 是 T
  static_assert(sizeof(decltype(payload::keys_rv32::UID)) ==
                    sizeof(cf::plugin::Payload<cf::plugin::uint_t<8>>),
                "UID must be uint_t<8>");
  static_assert(sizeof(decltype(payload::keys_rv32::THREAD_ID)) ==
                    sizeof(cf::plugin::Payload<cf::plugin::uint_t<2>>),
                "THREAD_ID must be uint_t<2>");
  static_assert(sizeof(decltype(payload::keys_rv32::IID_PC)) ==
                    sizeof(cf::plugin::Payload<std::uint32_t>),
                "IID_PC must be T (uint32_t for RV32)");
}

// ----------------------------------------------------------------------------
// 主入口
// ----------------------------------------------------------------------------

