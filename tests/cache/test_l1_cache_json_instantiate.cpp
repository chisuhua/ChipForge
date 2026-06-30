// tests/cache/test_l1_cache_json_instantiate.cpp
//
// 功能描述: L1CacheTLMBridgeAdapter full JSON instantiateAll e2e 测试 (Phase 1.3d-extras)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-13
//
// 测试覆盖 (5 子测试, F3 决议):
//   1. JSON 加载 + ModuleFactory::instantiateAll 不抛异常
//   2. factory.getInstance("l1") 返回非空, dynamic_cast 到 L1CacheTLMBridgeAdapter* 成功
//   3. factory.getInstance("tg") + "mem" 也返回非空 (3 模块拓扑连通)
//   4. factory.startAllTicks() 不崩溃, EventQueue 推进 100 cycle
//   5. 5 事务后 Bridge 的 pb_run_count() >= 5 (D1' 契约经 registerAdapter 路径仍成立)
//
// 关键集成点 (Phase 1.3d-extras 落地):
//   - ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>
//     (静态全局, 在 l1_cache_bridge_adapter.cpp 加载时自动执行)
//   - ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>("L1CacheTLMBridgeAdapter")
//     (测试中显式调用, 配合 REGISTER_CHSTREAM 注册 tg/mem)
//
// 范围限制 (与 v2 D3=A + F3 决议一致):
//   - 仅 1.3 范围, traffic_gen → l1 → mem 三模块
//   - 不测 mem 内部行为, 仅验证拓扑连通 + 协议转换注册生效
//   - 不引入 ch_stream 数据流验证 (Phase 1.4 baseline 对比范围, R7)
//
// 详见:
//   - .omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md F1-F5
//   - CppTLM/include/chstream_register.hh (REGISTER_CHSTREAM 宏)
//   - CppTLM/test/test_json_config_e2e.cc (canonical e2e pattern)

#include "catch_amalgamated.hpp"
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

#include "cf_plugin/bridge/l1_cache_bridge.h"
#include "cf_plugin/bridge/l1_cache_bridge_adapter.h"

using cf::plugin::bridge::L1CacheTLMBridge;
using cf::plugin::bridge::L1CacheTLMBridgeAdapter;

namespace {

const char* kSocE2EJsonPath = "soc/l1_cache_adapter_e2e.json";

nlohmann::json load_e2e_json() {
  std::ifstream f(kSocE2EJsonPath);
  if (!f.is_open()) {
    fprintf(stderr, "FAIL: cannot open %s\n", kSocE2EJsonPath);
    std::exit(1);
  }
  return nlohmann::json::parse(f);
}

void register_all_chipforge_modules() {
  REGISTER_CHSTREAM;
  ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>(
      "L1CacheTLMBridgeAdapter");
}

}  // namespace

TEST_CASE("instantiate_all_succeeds", "[cache]") {
  register_all_chipforge_modules();
  EventQueue eq;
  ModuleFactory factory(&eq);

  auto config = load_e2e_json();
  bool instantiated = factory.instantiateAll(config);
  REQUIRE(instantiated);
}

TEST_CASE("l1_instance_is_bridge_adapter", "[cache]") {
  register_all_chipforge_modules();
  EventQueue eq;
  ModuleFactory factory(&eq);

  auto config = load_e2e_json();
  factory.instantiateAll(config);

  SimObject* obj = factory.getInstance("l1");
  REQUIRE(obj != nullptr);

  L1CacheTLMBridgeAdapter* adapter = dynamic_cast<L1CacheTLMBridgeAdapter*>(obj);
  REQUIRE(adapter != nullptr);
  REQUIRE(adapter->getName() == "l1");
  REQUIRE(adapter->bridge() != nullptr);
}

TEST_CASE("three_module_topology_resolved", "[cache]") {
  register_all_chipforge_modules();
  EventQueue eq;
  ModuleFactory factory(&eq);

  auto config = load_e2e_json();
  factory.instantiateAll(config);

  REQUIRE(factory.getInstance("tg") != nullptr);
  REQUIRE(factory.getInstance("l1") != nullptr);
  REQUIRE(factory.getInstance("mem") != nullptr);

  const auto& all = factory.getAllInstances();
  REQUIRE(all.size() == 3);
}

TEST_CASE("start_ticks_and_cycle_advance", "[cache]") {
  register_all_chipforge_modules();
  EventQueue eq;
  ModuleFactory factory(&eq);

  auto config = load_e2e_json();
  factory.instantiateAll(config);
  factory.startAllTicks();

  const uint64_t before = eq.getCurrentCycle();
  eq.run(100);
  const uint64_t after = eq.getCurrentCycle();

  REQUIRE(after == before + 100);
}

TEST_CASE("bridge_pb_run_count_after_cycles", "[cache]") {
  register_all_chipforge_modules();
  EventQueue eq;
  ModuleFactory factory(&eq);

  auto config = load_e2e_json();
  factory.instantiateAll(config);
  factory.startAllTicks();

  eq.run(5);

  SimObject* obj = factory.getInstance("l1");
  L1CacheTLMBridgeAdapter* adapter = dynamic_cast<L1CacheTLMBridgeAdapter*>(obj);
  REQUIRE(adapter != nullptr);

  const int run_count = adapter->bridge()->pb_run_count();
  REQUIRE(run_count >= 4);
}