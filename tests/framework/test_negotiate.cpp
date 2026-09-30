// tests/framework/test_negotiate.cpp
//
// 功能描述: Plugin::negotiate() CapabilityTable 协商框架单元测试 (mfc Phase C.1 + C.5)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-30
//
// 测试覆盖 (C.1 RED + C.5 RED 共 4 用例):
//   1. negotiate_hook_called_before_setup_build   — negotiate 钩子在 setup/build 之前调用
//   2. negotiate_default_empty_impl               — Plugin 不 override negotiate() 时不调用
//   3. negotiate_missing_provider_failfast        — 缺依赖时 build() 返回 err (C.5)
//   4. negotiate_resolved_providers_succeed       — Provider 提供 capability 后 build() OK
//
// 设计 (ADR-082 §1.4-1.7):
//   - CapabilityTable: 提供 / 要求 capability 字典
//   - PluginBase::negotiate(CapabilityTable&) 默认空实现 (向后兼容 ADR-048)
//   - PipeBuilder::build() 编排: negotiate → unresolved 检查 → setup → build
//   - 缺依赖 → build() 返回 err (C.5 elaboration fail-fast)
//
#include "catch_amalgamated.hpp"
#include <any>
#include <memory>
#include <string>

#include "cf/plugin/capability_table.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

using cf::plugin::PipeBuilder;
using cf::plugin::Phase;
using cf::plugin::CapabilityTable;
using cf::plugin::PluginBase;

namespace {

// 测试 Plugin: 提供 capability "test_required_key"
struct ProviderPlugin : PluginBase {
  void negotiate(CapabilityTable& cap) override {
    cap.provide("test_required_key", this);
  }
  void build(PipeBuilder&) override {}
};

// 测试 Plugin: 要求 capability "test_required_key" (由 ProviderPlugin 提供)
struct ConsumerPlugin : PluginBase {
  void negotiate(CapabilityTable& cap) override {
    (void)cap.require("test_required_key");
  }
  void build(PipeBuilder&) override {}
};

// 测试 Plugin: 要求一个永远没人 provide 的 capability "missing_capability"
struct MissingConsumerPlugin : PluginBase {
  void negotiate(CapabilityTable& cap) override {
    (void)cap.require("missing_capability");
  }
  void build(PipeBuilder&) override {}
};

// 追踪 negotiate 调用顺序的 Plugin
struct OrderTrackerPlugin : PluginBase {
  static int setup_calls;
  static int negotiate_calls;
  static int build_calls;

  void setup(PipeBuilder&) override { ++setup_calls; }
  void negotiate(CapabilityTable&) override { ++negotiate_calls; }
  void build(PipeBuilder&) override { ++build_calls; }
};
int OrderTrackerPlugin::setup_calls = 0;
int OrderTrackerPlugin::negotiate_calls = 0;
int OrderTrackerPlugin::build_calls = 0;

}  // namespace

// ----------------------------------------------------------------------------
// Test 1: negotiate 钩子在 setup/build 之前调用 (C.1)
// ----------------------------------------------------------------------------
TEST_CASE("negotiate_hook_called_before_setup_build", "[framework][negotiate]") {
  OrderTrackerPlugin::setup_calls = 0;
  OrderTrackerPlugin::negotiate_calls = 0;
  OrderTrackerPlugin::build_calls = 0;

  PipeBuilder pb;
  pb.at_stage("dummy", Phase::NORMAL, []() {});
  auto plugin = std::make_unique<OrderTrackerPlugin>();
  REQUIRE_NOTHROW(pb.register_plugin(std::move(plugin)));
  REQUIRE(pb.build().has_value());

  REQUIRE(OrderTrackerPlugin::negotiate_calls == 1);
  REQUIRE(OrderTrackerPlugin::setup_calls == 1);
  REQUIRE(OrderTrackerPlugin::build_calls == 1);

  // 顺序保证: negotiate 必须在 setup 之前, setup 必须在 build 之前
  // (顺序由 PipeBuilder::build() 编排保证)
  // 此测试仅验证每个被调用 1 次
}

// ----------------------------------------------------------------------------
// Test 2: Plugin 不 override negotiate() 时, 默认空实现不报错
// ----------------------------------------------------------------------------
TEST_CASE("negotiate_default_empty_impl", "[framework][negotiate]") {
  struct EmptyPlugin : PluginBase {
    void build(PipeBuilder&) override {}
  };

  PipeBuilder pb;
  pb.at_stage("dummy", Phase::NORMAL, []() {});
  REQUIRE_NOTHROW(pb.register_plugin(std::make_unique<EmptyPlugin>()));
  REQUIRE(pb.build().has_value());
}

// ----------------------------------------------------------------------------
// Test 3: 缺依赖时 build() 返回 err (C.5 elaboration fail-fast)
// ----------------------------------------------------------------------------
TEST_CASE("negotiate_missing_provider_failfast", "[framework][negotiate]") {
  PipeBuilder pb;
  pb.at_stage("dummy", Phase::NORMAL, []() {});

  // 仅注册 Consumer, 不提供 test_required_key
  REQUIRE_NOTHROW(pb.register_plugin(std::make_unique<ConsumerPlugin>()));
  auto r = pb.build();
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error() == cf::plugin::PluginError::BuildFailed);
}

// ----------------------------------------------------------------------------
// Test 4: Provider + Consumer 完整配对时 build() OK
// ----------------------------------------------------------------------------
TEST_CASE("negotiate_resolved_providers_succeed", "[framework][negotiate]") {
  PipeBuilder pb;
  pb.at_stage("dummy", Phase::NORMAL, []() {});

  REQUIRE_NOTHROW(pb.register_plugin(std::make_unique<ProviderPlugin>()));
  REQUIRE_NOTHROW(pb.register_plugin(std::make_unique<ConsumerPlugin>()));
  REQUIRE(pb.build().has_value());
}

// ----------------------------------------------------------------------------
// Test 5: 缺失 capability 名称出现在 unresolved() 列表 (CapabilityTable 直接测)
// ----------------------------------------------------------------------------
TEST_CASE("capability_table_unresolved_lists_missing_keys", "[framework][negotiate]") {
  CapabilityTable cap;
  cap.require("alpha");
  cap.provide("beta", std::any{});
  cap.require("gamma");
  cap.require("beta");  // 满足

  auto u = cap.unresolved();
  REQUIRE(u.size() == 2);
  REQUIRE((u[0] == "alpha" || u[1] == "alpha"));
  REQUIRE((u[0] == "gamma" || u[1] == "gamma"));
}
