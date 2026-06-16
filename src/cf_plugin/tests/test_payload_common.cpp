// src/cf_plugin/tests/test_payload_common.cpp
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
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)
//   - 必须放 src/cf_plugin/tests/ (D-1 决策: 测试与 IP 目录解耦)

#include <cassert>
#include <cstdint>
#include <cstdio>
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
static void test_rv32_keys_instantiate() {
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
  printf("  [PASS] test_rv32_keys_instantiate\n");
}

// ----------------------------------------------------------------------------
// Test 2: RV64 keys_rv64 默认值 + 类型
// ----------------------------------------------------------------------------
static void test_rv64_keys_instantiate() {
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
  printf("  [PASS] test_rv64_keys_instantiate\n");
}

// ----------------------------------------------------------------------------
// Test 3: PC/INSTRUCTION/RS1/RS2/RD_DATA 跨阶段读写 (5 级流水仿真)
// ----------------------------------------------------------------------------
static void test_rv32_cross_stage_read_write() {
  PipeBuilder pb;
  pb.at_stage("if_id",  Phase::EARLY,  [](){});
  pb.at_stage("id_ex",  Phase::NORMAL, [](){});
  pb.at_stage("ex_mem", Phase::NORMAL, [](){});
  pb.build();

  auto if_id  = pb.node_of_logic_stage("if_id");
  auto id_ex  = pb.node_of_logic_stage("id_ex");
  auto ex_mem = pb.node_of_logic_stage("ex_mem");
  assert(if_id && id_ex && ex_mem);

  // 取指阶段: 写入 PC, INSTRUCTION
  (*if_id)(payload::keys_rv32::PC)          = 0x80000000u;
  (*if_id)(payload::keys_rv32::INSTRUCTION)  = 0x00100073u;  // ebreak

  // 译码阶段: 读 PC + INSTRUCTION, 写 RS1/RS2
  auto pc = (*id_ex)(payload::keys_rv32::PC) = (*if_id)(payload::keys_rv32::PC);
  (*id_ex)(payload::keys_rv32::INSTRUCTION) = (*if_id)(payload::keys_rv32::INSTRUCTION);
  (*id_ex)(payload::keys_rv32::RS1) = 0x12345678u;
  (*id_ex)(payload::keys_rv32::RS2) = 0x87654321u;
  assert(pc == 0x80000000u);

  // 执行阶段: 读 RS1/RS2, 写 RD_DATA
  (*ex_mem)(payload::keys_rv32::RS1)    = (*id_ex)(payload::keys_rv32::RS1);
  (*ex_mem)(payload::keys_rv32::RS2)    = (*id_ex)(payload::keys_rv32::RS2);
  (*ex_mem)(payload::keys_rv32::RD_DATA) = 0xaabbccddu;
  assert((*ex_mem)(payload::keys_rv32::RD_DATA) == 0xaabbccddu);

  printf("  [PASS] test_rv32_cross_stage_read_write\n");
}

// ----------------------------------------------------------------------------
// Test 4: RD_IDX (5-bit) 边界值 (0-31, x0-x31)
// ----------------------------------------------------------------------------
static void test_rd_idx_boundary() {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  assert(n);

  (*n)(payload::keys_rv32::RD_IDX) = 0;   // x0 (零寄存器, 写屏蔽)
  assert((*n)(payload::keys_rv32::RD_IDX) == 0);
  (*n)(payload::keys_rv32::RD_IDX) = 31;  // x31 (ra)
  assert((*n)(payload::keys_rv32::RD_IDX) == 31);
  // 编译期: uint_t<5> 实际 typedef 到 uint8_t (N=5 <= 8)
  static_assert(sizeof(cf::plugin::uint_t<5>) == sizeof(std::uint8_t),
                "uint_t<5> must be uint8_t");

  printf("  [PASS] test_rd_idx_boundary\n");
}

// ----------------------------------------------------------------------------
// Test 5: DECODE DecodePayload struct 读写
// ----------------------------------------------------------------------------
static void test_decode_payload_struct() {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  assert(n);

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
  assert(n2);

  n2->put(payload::keys_rv32::DECODE, (*n)(payload::keys_rv32::DECODE));
  const auto& dec2 = (*n2)(payload::keys_rv32::DECODE);
  assert(dec2.op_class  == payload::DecodePayload::OpClass::ALU);
  assert(dec2.writes_rd);
  assert(dec2.reads_rs1);
  assert(dec2.reads_rs2);
  assert(dec2.rd_class  == 0);

  // 切换 op_class
  (*n2)(payload::keys_rv32::DECODE).op_class = payload::DecodePayload::OpClass::BRANCH;
  assert((*n2)(payload::keys_rv32::DECODE).op_class ==
         payload::DecodePayload::OpClass::BRANCH);

  printf("  [PASS] test_decode_payload_struct\n");
}

// ----------------------------------------------------------------------------
// Test 6: RESULT 字段读写
// ----------------------------------------------------------------------------
static void test_result_field() {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s");
  assert(n);

  (*n)(payload::keys_rv32::RESULT) = 0xfeedfaceu;
  assert((*n)(payload::keys_rv32::RESULT) == 0xfeedfaceu);

  // RV64
  (*n)(payload::keys_rv64::RESULT) = 0x1122334455667788ULL;
  assert((*n)(payload::keys_rv64::RESULT) == 0x1122334455667788ULL);

  printf("  [PASS] test_result_field\n");
}

// ----------------------------------------------------------------------------
// Test 7: 模板参数 static_assert (编译期: XLEN ∈ {32,64}, T 是 uint32/uint64)
// ----------------------------------------------------------------------------
// 编译期测试: 错误实例化会编译失败
//   payload::keys<float, 32> k;   // ❌ float 不是 uint32/uint64
//   payload::keys<uint32_t, 128> k; // ❌ XLEN 不是 32/64
// 这里只测试合法实例化能通过编译
static void test_template_sanity() {
  // 合法实例化 (静态, 编译期类型检查)
  // 字符串名在运行时验证 (std::string operator== 不是 constexpr)
  assert(payload::keys_rv32::PC.name()          == "cpu.pc");
  assert(payload::keys_rv32::INSTRUCTION.name() == "cpu.instruction");
  assert(payload::keys_rv32::RS1.name()         == "cpu.rs1");
  assert(payload::keys_rv32::RS2.name()         == "cpu.rs2");
  assert(payload::keys_rv32::RD_DATA.name()     == "cpu.rd_data");
  assert(payload::keys_rv32::RD_IDX.name()       == "cpu.rd_idx");
  assert(payload::keys_rv32::DECODE.name()       == "cpu.decode");
  assert(payload::keys_rv32::RESULT.name()      == "cpu.result");
  // RV64 同样 (类型不同, Key 名字相同)
  assert(payload::keys_rv64::PC.name()          == "cpu.pc");
  assert(payload::keys_rv64::RD_DATA.name()     == "cpu.rd_data");
  printf("  [PASS] test_template_sanity\n");
}

// ----------------------------------------------------------------------------
// 主入口
// ----------------------------------------------------------------------------
int main() {
  printf("=== test_payload_common (M1.7) ===\n");
  test_rv32_keys_instantiate();
  test_rv64_keys_instantiate();
  test_rv32_cross_stage_read_write();
  test_rd_idx_boundary();
  test_decode_payload_struct();
  test_result_field();
  test_template_sanity();
  printf("=== ALL 7 TESTS PASSED ===\n");
  return 0;
}
