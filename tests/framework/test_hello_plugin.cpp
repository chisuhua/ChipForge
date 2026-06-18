// tests/framework/test_hello_plugin.cpp
//
// 功能描述: 最小验证 Plugin (Phase 0 退出标准 §2.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 验证:
//   - 一个 ~10 行的 Plugin 能在 PipeBuilder 下端到端跑通
//   - 演示 Plugin-style 设计的最小可行模式:
//     1. 派生 PluginBase
//     2. 声明 Payload 静态 Key
//     3. build() 中 at_stage() 注册回调
//     4. PipeBuilder 编译 + 运行
//   - 验证多次运行结果一致 (调度确定性)

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

using cf::plugin::Payload;
using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;

static Payload<uint32_t> g_greet_count{"greet_count"};

struct HelloPlugin : PluginBase {
  void build(PipeBuilder& pb) override {
    pb.at_stage("greet", Phase::NORMAL, [&pb] {
      auto node = pb.node_of_logic_stage("greet");
      node->operator()(g_greet_count) = node->operator()(g_greet_count) + 1;
    });
  }
};

static int run_hello_pipeline() {
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<HelloPlugin>());
  pb.build();
  pb.run();
  return pb.node_of_logic_stage("greet")->operator()(g_greet_count);
}

static void test_hello_plugin_runs() {
  int count = run_hello_pipeline();
  assert(count == 1);
  printf("  [PASS] test_hello_plugin_runs\n");
}

static void test_determinism_multiple_runs() {
  std::vector<int> results;
  for (int i = 0; i < 5; ++i) {
    results.push_back(run_hello_pipeline());
  }
  for (std::size_t i = 1; i < results.size(); ++i) {
    assert(results[i] == results[i - 1]);
  }
  for (int r : results) {
    assert(r == 1);
  }
  printf("  [PASS] test_determinism_multiple_runs\n");
}

static void test_determinism_independent_instances() {
  int count_a = run_hello_pipeline();
  int count_b = run_hello_pipeline();
  int count_c = run_hello_pipeline();
  assert(count_a == count_b);
  assert(count_b == count_c);
  assert(count_a == 1);
  printf("  [PASS] test_determinism_independent_instances\n");
}

int main() {
  printf("=== HelloPlugin + Determinism Tests (Phase 0 退出标准) ===\n");
  test_hello_plugin_runs();
  test_determinism_multiple_runs();
  test_determinism_independent_instances();
  printf("=== All HelloPlugin tests passed ===\n");
  return 0;
}
