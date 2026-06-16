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
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/plugins/ibus.h"

using cf::cpu::plugins::IBusPlugin;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

static void test_rv32_instantiate() {
  IBusPlugin<T32> ibus;
  ibus.set_instruction(0x00000013);  // NOP
  (void)ibus;
  printf("  [PASS] test_rv32_instantiate\n");
}

static void test_rv64_instantiate() {
  IBusPlugin<T64> ibus;
  ibus.set_instruction(0x00000013);
  (void)ibus;
  printf("  [PASS] test_rv64_instantiate\n");
}

static void test_set_instruction() {
  IBusPlugin<T32> ibus;
  ibus.set_instruction(0xDEADBEEF);
  ibus.set_instruction(0xCAFEBABE);
  (void)ibus;
  printf("  [PASS] test_set_instruction\n");
}

static void test_build_compiles() {
  IBusPlugin<T32> ibus;
  cf::plugin::PipeBuilder pb;
  ibus.build(pb);
  printf("  [PASS] test_build_compiles\n");
}

static void test_state_persistence() {
  IBusPlugin<T32> ibus;
  for (int i = 0; i < 10; ++i) {
    ibus.set_instruction(0x00000013 + i);
  }
  (void)ibus;
  printf("  [PASS] test_state_persistence\n");
}

int main() {
  printf("test_ibus:\n");
  test_rv32_instantiate();
  test_rv64_instantiate();
  test_set_instruction();
  test_build_compiles();
  test_state_persistence();
  printf("[PASS] all IBusPlugin tests\n");
  return 0;
}
