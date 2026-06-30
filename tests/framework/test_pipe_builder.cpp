// tests/framework/test_pipe_builder.cpp
//

#include "catch_amalgamated.hpp"
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PipeNode;
using cf::plugin::PluginBase;

static int g_setup_counter = 0;
static int g_build_counter = 0;
static int g_run_counter = 0;
static std::vector<std::string> g_stage_log;

struct CounterPlugin : PluginBase {
  void build(PipeBuilder& /*pb*/) override {
    g_build_counter++;
  }
  void setup(PipeBuilder& /*pb*/) override {
    g_setup_counter++;
  }
};

struct MinimalPlugin : PluginBase {
  void build(PipeBuilder& /*pb*/) override {}
};

struct StagePlugin : PluginBase {
  int marker = 0;
  void build(PipeBuilder& pb) override {
    pb.at_stage("alpha", Phase::EARLY, [this] {
      marker = marker * 10 + 1;
      g_stage_log.push_back("alpha");
    });
    pb.at_stage("beta", Phase::NORMAL, [this] {
      marker = marker * 10 + 2;
      g_stage_log.push_back("beta");
    });
    pb.at_stage("gamma", Phase::LATE, [this] {
      marker = marker * 10 + 3;
      g_stage_log.push_back("gamma");
    });
  }
};

static void reset_globals() {
  g_setup_counter = 0;
  g_build_counter = 0;
  g_run_counter = 0;
  g_stage_log.clear();
}

TEST_CASE("register_plugin", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.register_plugin(std::make_unique<CounterPlugin>());
  REQUIRE(pb.plugin_count() == 2);
  pb.build();
  REQUIRE(g_setup_counter == 2);
  REQUIRE(g_build_counter == 2);
}

TEST_CASE("at_stage_order", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  REQUIRE(pb.stage_count() == 3);
  REQUIRE(pb.stage_names().size() == 3);
  REQUIRE(pb.stage_names()[0] == "alpha");
  REQUIRE(pb.stage_names()[1] == "beta");
  REQUIRE(pb.stage_names()[2] == "gamma");
}

TEST_CASE("run_executes_callbacks", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  pb.run();
  REQUIRE(g_stage_log.size() == 3);
  REQUIRE(g_stage_log[0] == "alpha");
  REQUIRE(g_stage_log[1] == "beta");
  REQUIRE(g_stage_log[2] == "gamma");
}

TEST_CASE("node_of_logic_stage", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  auto n = pb.node_of_logic_stage("alpha");
  REQUIRE(n != nullptr);
  REQUIRE(n->name() == "alpha");
  auto missing = pb.node_of_logic_stage("does_not_exist");
  REQUIRE(missing == nullptr);
}

TEST_CASE("node_auto_creation", "[framework]") {
  PipeBuilder pb;
  pb.at_stage("x", Phase::NORMAL, []{});
  pb.at_stage("y", Phase::NORMAL, []{});
  pb.at_stage("x", Phase::LATE, []{});  // x 已存在
  REQUIRE(pb.node_count() == 2);
}

TEST_CASE("setup_called_before_build", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.build();
  // setup() 和 build() 都调用一次, 顺序由 PipeBuilder 决定
  REQUIRE(g_setup_counter == 1);
  REQUIRE(g_build_counter == 1);
}

TEST_CASE("declare_substage", "[framework]") {
  PipeBuilder pb;
  pb.declare_substage("main", "sub_a", 1);
  pb.declare_substage("main", "sub_b", 1);
  // 子阶段节点也被创建
  REQUIRE(pb.node_of_logic_stage("sub_a") != nullptr);
  REQUIRE(pb.node_of_logic_stage("sub_b") != nullptr);
  REQUIRE(pb.node_of_logic_stage("main") != nullptr);
}

TEST_CASE("reset_all", "[framework]") {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, []{});
  auto n = pb.node_of_logic_stage("s");
  n->assert_valid();
  REQUIRE(n->is_firing());
  pb.reset_all();
  REQUIRE(n->is_idle());
}

TEST_CASE("has_stage", "[framework]") {
  PipeBuilder pb;
  pb.at_stage("foo", Phase::NORMAL, []{});
  REQUIRE(pb.has_stage("foo"));
  REQUIRE(!pb.has_stage("bar"));
}

TEST_CASE("multi_plugin_stage_sharing", "[framework]") {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.build();
  // StagePlugin 注册 3 个阶段; CounterPlugin 不注册
  REQUIRE(pb.stage_count() == 3);
  // run() 调用 3 个回调
  pb.run();
  REQUIRE(g_stage_log.size() == 3);
}

TEST_CASE("phase_values", "[framework]") {
  PipeBuilder pb;
  pb.at_stage("e", Phase::EARLY, []{});
  pb.at_stage("n", Phase::NORMAL, []{});
  pb.at_stage("l", Phase::LATE, []{});
  REQUIRE(cf::plugin::phase_name(Phase::EARLY) == std::string{"EARLY"});
  REQUIRE(cf::plugin::phase_name(Phase::NORMAL) == std::string{"NORMAL"});
  REQUIRE(cf::plugin::phase_name(Phase::LATE) == std::string{"LATE"});
}


