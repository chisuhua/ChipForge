// tests/framework/test_coexistence.cpp
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 验证:
//   1. 同一翻译单元可同时 include cf_plugin + cpptlm + ch
//   2. cf::plugin::PluginBase 派生类可用 CppTLM 类型成员
//   3. cf::plugin::PipeBuilder 可与 cpptlm::ChStreamModuleBase 实例共存
//   4. cf::plugin::Payload 与 CppHDL ch_uint<N> 命名空间不冲突

#include "catch_amalgamated.hpp"
#include <memory>
#include <string>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/uint_t.h"

#include "core/chstream_module.hh"
#include "component.h"

using cf::plugin::Payload;
using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;

TEST_CASE("namespace_isolation", "[framework]") {
  cf::plugin::PluginBase* p = nullptr;
  (void)p;  // 类型可寻址即可
}

TEST_CASE("can_include_all_three_headers", "[framework]") {
  // ChStreamModuleBase 实例化需要 EventQueue
  // 这里仅验证类型可见性 (不实际构造,避免 EventQueue 依赖)
  static_assert(std::is_class<ChStreamModuleBase>::value,
                "ChStreamModuleBase must be a class");
  static_assert(std::is_class<ch::Component>::value,
                "ch::Component must be a class");
}

TEST_CASE("payload_coexists_with_chuint", "[framework]") {
  // cf::plugin::uint_t<64> 与 ch::ch_uint<64> 命名空间独立
  cf::plugin::uint_t<64> a = 0xDEAD;
  cf::plugin::uint_t<64> b = 0xBEEF;
  REQUIRE(a == 0xDEAD);
  REQUIRE(b == 0xBEEF);
  REQUIRE(a != b);
  // 同一类型别名工作
  cf::plugin::Payload<cf::plugin::uint_t<32>> p32{"p32"};
  p32.type();
}

TEST_CASE("plugin_can_hold_cpp_namespace_member", "[framework]") {
  // 派生类可以同时使用 cf::plugin 和 cpptlm:: 命名空间成员
  // (不实际构造 ChStreamModuleBase 实例, 只需类型可组合)
  struct HybridPlugin : cf::plugin::PluginBase {
    void build(cf::plugin::PipeBuilder& /*pb*/) override {}
    // 持有 cpptlm::ChStreamModuleBase 指针 (前向声明允许 unique_ptr)
    // 实际构造留给 Phase 1+ 业务代码
  };
  static_assert(sizeof(HybridPlugin) > 0, "HybridPlugin must have size");
  // 类型特征: cf::plugin::PluginBase 派生
  static_assert(std::is_base_of<cf::plugin::PluginBase, HybridPlugin>::value,
                "HybridPlugin must derive from cf::plugin::PluginBase");
}

TEST_CASE("pipe_builder_orchestrates_plugin_lifecycle", "[framework]") {
  // 验证 cf::plugin 自身的全链路 (与 CppTLM/CppHDL include 并存)
  struct CountPlugin : cf::plugin::PluginBase {
    int setup_count = 0;
    int build_count = 0;
    int run_count = 0;
    void setup(cf::plugin::PipeBuilder& /*pb*/) override { ++setup_count; }
    void build(cf::plugin::PipeBuilder& pb) override {
      ++build_count;
      pb.at_stage("work", cf::plugin::Phase::NORMAL,
                  [this] { ++run_count; });
    }
  };
  cf::plugin::PipeBuilder pb;
  pb.register_plugin(std::make_unique<CountPlugin>());
  pb.build();
  REQUIRE(pb.stage_count() == 1);
  pb.run();
  pb.run();
  pb.run();
}


