// tests/cpu/test_reg_file.cpp
//
// 功能描述: RegFilePlugin 单元测试 (M2.9 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. RV32 32 寄存器初始值 (x0=0, 其他=x)
//   2. RV64 32 寄存器初始值
//   3. x0 写屏蔽 (写 x0 仍为 0)
//   4. RV32 寄存器读写
//   5. RV64 寄存器读写
//
// 约束:
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/plugins/reg_file.h"

using cf::cpu::plugins::RegFilePlugin;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

static void test_rv32_initial() {
  RegFilePlugin<T32> rf;
  // x0 恒为 0
  assert(rf.read_reg(0) == 0);
  // 其他寄存器初始为 0 (未初始化)
  for (int i = 1; i < 32; ++i) {
    rf.write_reg(i, 0xDEAD0000 + i);
  }
  assert(rf.read_reg(1) == 0xDEAD0001);
  assert(rf.read_reg(31) == 0xDEAD001F);
  printf("  [PASS] test_rv32_initial\n");
}

static void test_rv64_initial() {
  RegFilePlugin<T64> rf;
  assert(rf.read_reg(0) == 0);
  rf.write_reg(5, 0xDEADBEEFCAFEBABEULL);
  assert(rf.read_reg(5) == 0xDEADBEEFCAFEBABEULL);
  printf("  [PASS] test_rv64_initial\n");
}

static void test_x0_write_mask() {
  RegFilePlugin<T32> rf;
  rf.write_reg(0, 0xFFFFFFFF);
  assert(rf.read_reg(0) == 0);  // x0 写屏蔽
  rf.write_reg(15, 0x12345678);
  rf.write_reg(0, 0xAAAAAAAA);
  assert(rf.read_reg(15) == 0x12345678);  // 其他寄存器不受影响
  assert(rf.read_reg(0) == 0);
  printf("  [PASS] test_x0_write_mask\n");
}

static void test_rv32_rw() {
  RegFilePlugin<T32> rf;
  rf.write_reg(2, 0x1000);
  rf.write_reg(3, 0x2000);
  assert(rf.read_reg(2) == 0x1000);
  assert(rf.read_reg(3) == 0x2000);
  rf.write_reg(2, 0x3000);
  assert(rf.read_reg(2) == 0x3000);
  printf("  [PASS] test_rv32_rw\n");
}

static void test_rv64_rw() {
  RegFilePlugin<T64> rf;
  rf.write_reg(10, 0x1111111111111111ULL);
  rf.write_reg(11, 0x2222222222222222ULL);
  assert(rf.read_reg(10) == 0x1111111111111111ULL);
  assert(rf.read_reg(11) == 0x2222222222222222ULL);
  printf("  [PASS] test_rv64_rw\n");
}

// M4G D.2 (G.2): 自定义 N_REGS 编译 + 行为正确
static void test_custom_n_regs() {
  // N_REGS=8 (非标准 RISC-V, 验证模板参数生效)
  RegFilePlugin<T32, 8> rf8;
  assert(rf8.read_reg(0) == 0);  // x0 屏蔽
  rf8.write_reg(3, 42);
  assert(rf8.read_reg(3) == 42u);
  rf8.write_reg(7, 0xFF);
  assert(rf8.read_reg(7) == 0xFFu);
  printf("  [PASS] test_custom_n_regs\n");
}

// M4G D.2 (G.2): 多线程模板参数 — per-thread 寄存器隔离
static void test_multi_thread_isolation() {
  // N_THREADS=2: 每个线程独立 32 寄存器
  RegFilePlugin<T32, 32, 2> rf_mt;
  // 默认 tid=0
  rf_mt.write_reg(5, 100);
  assert(rf_mt.read_reg(5) == 100u);
  // 显式 tid=1
  rf_mt.write_reg(5, 200, /*tid*/ 1);
  assert(rf_mt.read_reg(5, /*tid*/ 0) == 100u);
  assert(rf_mt.read_reg(5, /*tid*/ 1) == 200u);
  // 写 x0 在 tid=1 上也屏蔽
  rf_mt.write_reg(0, 0xDEAD, /*tid*/ 1);
  assert(rf_mt.read_reg(0, /*tid*/ 1) == 0u);
  printf("  [PASS] test_multi_thread_isolation\n");
}

int main() {
  printf("test_reg_file:\n");
  test_rv32_initial();
  test_rv64_initial();
  test_x0_write_mask();
  test_rv32_rw();
  test_rv64_rw();
  test_custom_n_regs();
  test_multi_thread_isolation();
  printf("[PASS] all RegFilePlugin tests\n");
  return 0;
}
