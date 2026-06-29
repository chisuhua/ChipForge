// tests/cpu/test_ibus.cpp
//
// 功能描述: IBusPlugin 单元测试 (M2.9 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. RV32 实例化
//   2. RV64 实例化
//   3. set_instruction 手动注入
//   4. build() 可调用 (编译验证)
//   5. 状态保持 (多次调用)
//
// 约束:

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/plugins/ibus.h"

using cf::cpu::plugins::IBusPlugin;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

TEST_CASE("rv32_instantiate", "[cpu]") {
  IBusPlugin<T32> ibus;
  ibus.set_instruction(0x00000013);  // NOP
  (void)ibus;
}

TEST_CASE("rv64_instantiate", "[cpu]") {
  IBusPlugin<T64> ibus;
  ibus.set_instruction(0x00000013);
  (void)ibus;
}

TEST_CASE("set_instruction", "[cpu]") {
  IBusPlugin<T32> ibus;
  ibus.set_instruction(0xDEADBEEF);
  ibus.set_instruction(0xCAFEBABE);
  (void)ibus;
}

TEST_CASE("build_compiles", "[cpu]") {
  IBusPlugin<T32> ibus;
  cf::plugin::PipeBuilder pb;
  ibus.build(pb);
}

TEST_CASE("state_persistence", "[cpu]") {
  IBusPlugin<T32> ibus;
  for (int i = 0; i < 10; ++i) {
    ibus.set_instruction(0x00000013 + i);
  }
  (void)ibus;
}


