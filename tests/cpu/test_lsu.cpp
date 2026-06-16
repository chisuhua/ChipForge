// tests/cpu/test_lsu.cpp
//
// 功能描述: RiscvLsuPlugin 单元测试 (M3.10 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (4 用例):
//   1. RV32 实例化
//   2. RV64 实例化
//   3. build() 可调用
//   4. 销毁安全
//
// 约束:
//   - 纯 main() + assert

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/arch/riscv/lsu.h"

using namespace cf::cpu::arch::riscv;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

static void test_rv32_instantiate() {
  RiscvLsuPlugin<T32> lsu;
  (void)lsu;
  printf("  [PASS] test_rv32_instantiate\n");
}

static void test_rv64_instantiate() {
  RiscvLsuPlugin<T64> lsu;
  (void)lsu;
  printf("  [PASS] test_rv64_instantiate\n");
}

static void test_build_compiles() {
  RiscvLsuPlugin<T32> lsu;
  cf::plugin::PipeBuilder pb;
  lsu.build(pb);
  printf("  [PASS] test_build_compiles\n");
}

static void test_destruction_safe() {
  {
    RiscvLsuPlugin<T32> lsu;
    (void)lsu;
  }
  printf("  [PASS] test_destruction_safe\n");
}

int main() {
  printf("test_lsu:\n");
  test_rv32_instantiate();
  test_rv64_instantiate();
  test_build_compiles();
  test_destruction_safe();
  printf("[PASS] all RiscvLsuPlugin tests\n");
  return 0;
}
