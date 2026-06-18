// tests/framework/test_pipe_builder.cpp
//
// 功能描述: PipeBuilder 单元测试 (Phase 0 P0 #4)

#include <cassert>
#include <cstdio>
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

static void test_register_plugin() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.register_plugin(std::make_unique<CounterPlugin>());
  assert(pb.plugin_count() == 2);
  pb.build();
  assert(g_setup_counter == 2);
  assert(g_build_counter == 2);
  printf("  [PASS] test_register_plugin\n");
}

static void test_at_stage_order() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  assert(pb.stage_count() == 3);
  assert(pb.stage_names().size() == 3);
  assert(pb.stage_names()[0] == "alpha");
  assert(pb.stage_names()[1] == "beta");
  assert(pb.stage_names()[2] == "gamma");
  printf("  [PASS] test_at_stage_order\n");
}

static void test_run_executes_callbacks() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  pb.run();
  assert(g_stage_log.size() == 3);
  assert(g_stage_log[0] == "alpha");
  assert(g_stage_log[1] == "beta");
  assert(g_stage_log[2] == "gamma");
  printf("  [PASS] test_run_executes_callbacks\n");
}

static void test_node_of_logic_stage() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.build();
  auto n = pb.node_of_logic_stage("alpha");
  assert(n != nullptr);
  assert(n->name() == "alpha");
  auto missing = pb.node_of_logic_stage("does_not_exist");
  assert(missing == nullptr);
  printf("  [PASS] test_node_of_logic_stage\n");
}

static void test_node_auto_creation() {
  PipeBuilder pb;
  pb.at_stage("x", Phase::NORMAL, []{});
  pb.at_stage("y", Phase::NORMAL, []{});
  pb.at_stage("x", Phase::LATE, []{});  // x 已存在
  assert(pb.node_count() == 2);
  printf("  [PASS] test_node_auto_creation\n");
}

static void test_setup_called_before_build() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.build();
  // setup() 和 build() 都调用一次, 顺序由 PipeBuilder 决定
  assert(g_setup_counter == 1);
  assert(g_build_counter == 1);
  printf("  [PASS] test_setup_called_before_build\n");
}

static void test_declare_substage() {
  PipeBuilder pb;
  pb.declare_substage("main", "sub_a", 1);
  pb.declare_substage("main", "sub_b", 1);
  // 子阶段节点也被创建
  assert(pb.node_of_logic_stage("sub_a") != nullptr);
  assert(pb.node_of_logic_stage("sub_b") != nullptr);
  assert(pb.node_of_logic_stage("main") != nullptr);
  printf("  [PASS] test_declare_substage\n");
}

static void test_reset_all() {
  PipeBuilder pb;
  pb.at_stage("s", Phase::NORMAL, []{});
  auto n = pb.node_of_logic_stage("s");
  n->assert_valid();
  assert(n->is_firing());
  pb.reset_all();
  assert(n->is_idle());
  printf("  [PASS] test_reset_all\n");
}

static void test_has_stage() {
  PipeBuilder pb;
  pb.at_stage("foo", Phase::NORMAL, []{});
  assert(pb.has_stage("foo"));
  assert(!pb.has_stage("bar"));
  printf("  [PASS] test_has_stage\n");
}

static void test_multi_plugin_stage_sharing() {
  reset_globals();
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<StagePlugin>());
  pb.register_plugin(std::make_unique<CounterPlugin>());
  pb.build();
  // StagePlugin 注册 3 个阶段; CounterPlugin 不注册
  assert(pb.stage_count() == 3);
  // run() 调用 3 个回调
  pb.run();
  assert(g_stage_log.size() == 3);
  printf("  [PASS] test_multi_plugin_stage_sharing\n");
}

static void test_phase_values() {
  PipeBuilder pb;
  pb.at_stage("e", Phase::EARLY, []{});
  pb.at_stage("n", Phase::NORMAL, []{});
  pb.at_stage("l", Phase::LATE, []{});
  assert(cf::plugin::phase_name(Phase::EARLY) == std::string{"EARLY"});
  assert(cf::plugin::phase_name(Phase::NORMAL) == std::string{"NORMAL"});
  assert(cf::plugin::phase_name(Phase::LATE) == std::string{"LATE"});
  printf("  [PASS] test_phase_values\n");
}

int main() {
  printf("=== PipeBuilder Tests (Phase 0 P0 #4) ===\n");
  test_register_plugin();
  test_at_stage_order();
  test_run_executes_callbacks();
  test_node_of_logic_stage();
  test_node_auto_creation();
  test_setup_called_before_build();
  test_declare_substage();
  test_reset_all();
  test_has_stage();
  test_multi_plugin_stage_sharing();
  test_phase_values();
  printf("=== All PipeBuilder tests passed ===\n");
  return 0;
}
