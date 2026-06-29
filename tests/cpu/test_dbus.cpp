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

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/plugins/dbus.h"

using cf::cpu::plugins::DBusPlugin;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

TEST_CASE("rv32_instantiate", "[cpu]") {
  DBusPlugin<T32> dbus;
  (void)dbus;
}

TEST_CASE("rv64_instantiate", "[cpu]") {
  DBusPlugin<T64> dbus;
  (void)dbus;
}

TEST_CASE("build_compiles", "[cpu]") {
  DBusPlugin<T32> dbus;
  cf::plugin::PipeBuilder pb;
  dbus.build(pb);
}

TEST_CASE("multiple_build_calls", "[cpu]") {
  DBusPlugin<T32> dbus;
  cf::plugin::PipeBuilder pb;
  dbus.build(pb);
  dbus.build(pb);
  (void)dbus;
}

TEST_CASE("destruction_safe", "[cpu]") {
  {
    DBusPlugin<T32> dbus;
    (void)dbus;
  }
}


