// tests/framework/test_plugin_lifecycle.cpp
//
// 功能描述: PluginBase 单元测试 (Phase 0 P0 #1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 测试覆盖:
//   1. PluginBase 是抽象类 (不能直接实例化)
//   2. 派生类必须实现 build() (否则仍为抽象)
//   3. 派生类正确实现 build() 后可实例化
//   4. setup() 有默认空实现, 派生类可选 override
//   5. setup() 和 build() 按顺序被 PipeBuilder 调用 (此处用直接调用模拟)
//   6. uint_t<N> 类型切换正确 (uint8/16/32/64)
//
// 测试框架: 纯 main() + assert (无外部依赖)
// Phase 0 暂不引入 Catch2; 测试即 main(), 失败时返回非零退出码

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/uint_t.h"

// ----------------------------------------------------------------------------
// Test 1: PluginBase 是抽象类 (不能直接实例化)
// 编译期检查: 注释掉下面这一行会编译失败
//   PluginBase pb;  // ERROR: cannot allocate abstract class
// ----------------------------------------------------------------------------
static void test_plugin_base_is_abstract() {
  static_assert(!std::is_default_constructible<cf::plugin::PluginBase>::value,
                "PluginBase must NOT be default-constructible (abstract)");
  printf("  [PASS] test_plugin_base_is_abstract\n");
}

// ----------------------------------------------------------------------------
// Test 2: 未实现 build() 的派生类仍为抽象 (不能实例化)
// ----------------------------------------------------------------------------
struct BadPlugin : cf::plugin::PluginBase {
  // 故意不实现 build() —— 应保持抽象
};
static void test_derived_must_implement_build() {
  static_assert(!std::is_default_constructible<BadPlugin>::value,
                "Derived class without build() must remain abstract");
  printf("  [PASS] test_derived_must_implement_build\n");
}

// ----------------------------------------------------------------------------
// Test 3: 实现 build() 后可实例化; setup() 可选 override
// ----------------------------------------------------------------------------
struct GoodPlugin : cf::plugin::PluginBase {
  bool setup_called = false;
  bool build_called = false;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override { setup_called = true; }

  void build(cf::plugin::PipeBuilder& /*pb*/) override { build_called = true; }
};
static void test_good_plugin_works() {
  GoodPlugin gp;
  cf::plugin::PipeBuilder pb;
  assert(!gp.setup_called);
  assert(!gp.build_called);

  gp.setup(pb);
  assert(gp.setup_called);
  assert(!gp.build_called);

  gp.build(pb);
  assert(gp.setup_called);
  assert(gp.build_called);
  printf("  [PASS] test_good_plugin_works\n");
}

// ----------------------------------------------------------------------------
// Test 4: setup() 有默认空实现 (派生类不 override 也能编译)
// ----------------------------------------------------------------------------
struct MinimalPlugin : cf::plugin::PluginBase {
  void build(cf::plugin::PipeBuilder& /*pb*/) override {
    // 不 override setup(), 走基类默认空实现
  }
};
static void test_setup_default_empty() {
  MinimalPlugin mp;
  cf::plugin::PipeBuilder pb;
  mp.setup(pb);  // 不应崩溃
  mp.build(pb);  // 不应崩溃
  printf("  [PASS] test_setup_default_empty\n");
}

// ----------------------------------------------------------------------------
// Test 5: setup() 和 build() 调用顺序记录 (验证生命周期)
// ----------------------------------------------------------------------------
struct OrderPlugin : cf::plugin::PluginBase {
  int call_order = 0;
  int setup_marker = -1;
  int build_marker = -1;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {
    setup_marker = ++call_order;
  }
  void build(cf::plugin::PipeBuilder& /*pb*/) override {
    build_marker = ++call_order;
  }
};
static void test_setup_before_build() {
  OrderPlugin op;
  cf::plugin::PipeBuilder pb;
  op.setup(pb);
  op.build(pb);
  assert(op.setup_marker == 1);
  assert(op.build_marker == 2);
  assert(op.setup_marker < op.build_marker);
  printf("  [PASS] test_setup_before_build\n");
}

// ----------------------------------------------------------------------------
// Test 6: uint_t<N> 编译期类型切换
// ----------------------------------------------------------------------------
static void test_uint_t_width() {
  static_assert(sizeof(cf::plugin::uint_t<8>) == 1, "uint_t<8> must be 1 byte");
  static_assert(sizeof(cf::plugin::uint_t<16>) == 2, "uint_t<16> must be 2 bytes");
  static_assert(sizeof(cf::plugin::uint_t<32>) == 4, "uint_t<32> must be 4 bytes");
  static_assert(sizeof(cf::plugin::uint_t<64>) == 8, "uint_t<64> must be 8 bytes");
  static_assert(std::is_same<cf::plugin::uint_t<32>, uint32_t>::value,
                "uint_t<32> must be uint32_t");
  static_assert(std::is_same<cf::plugin::bool_t, bool>::value,
                "bool_t must be bool");
  printf("  [PASS] test_uint_t_width\n");
}

// ----------------------------------------------------------------------------
// Test 7: 禁止拷贝 (每个 Plugin 应独立注册)
// ----------------------------------------------------------------------------
static void test_no_copy() {
  static_assert(!std::is_copy_constructible<GoodPlugin>::value,
                "Plugin must not be copyable");
  static_assert(!std::is_copy_assignable<GoodPlugin>::value,
                "Plugin must not be copy-assignable");
  printf("  [PASS] test_no_copy\n");
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
  printf("=== PluginBase Lifecycle Tests (Phase 0 P0 #1) ===\n");
  test_plugin_base_is_abstract();
  test_derived_must_implement_build();
  test_good_plugin_works();
  test_setup_default_empty();
  test_setup_before_build();
  test_uint_t_width();
  test_no_copy();
  printf("=== All PluginBase tests passed ===\n");
  return 0;
}
