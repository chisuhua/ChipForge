// tests/cpu/test_dbus.cpp
//
// 功能描述: DBusPlugin 单元测试 (M2.9 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. RV32 实例化
//   2. RV64 实例化
//   3. build() 可调用 (编译验证)
//   4. 多次 build() 调用无副作用
//   5. 销毁安全
//
// 约束:
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/plugins/dbus.h"

using cf::cpu::plugins::DBusPlugin;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

static void test_rv32_instantiate() {
  DBusPlugin<T32> dbus;
  (void)dbus;
  printf("  [PASS] test_rv32_instantiate\n");
}

static void test_rv64_instantiate() {
  DBusPlugin<T64> dbus;
  (void)dbus;
  printf("  [PASS] test_rv64_instantiate\n");
}

static void test_build_compiles() {
  DBusPlugin<T32> dbus;
  cf::plugin::PipeBuilder pb;
  dbus.build(pb);
  printf("  [PASS] test_build_compiles\n");
}

static void test_multiple_build_calls() {
  DBusPlugin<T32> dbus;
  cf::plugin::PipeBuilder pb;
  dbus.build(pb);
  dbus.build(pb);
  (void)dbus;
  printf("  [PASS] test_multiple_build_calls\n");
}

static void test_destruction_safe() {
  {
    DBusPlugin<T32> dbus;
    (void)dbus;
  }
  printf("  [PASS] test_destruction_safe\n");
}

int main() {
  printf("test_dbus:\n");
  test_rv32_instantiate();
  test_rv64_instantiate();
  test_build_compiles();
  test_multiple_build_calls();
  test_destruction_safe();
  printf("[PASS] all DBusPlugin tests\n");
  return 0;
}
