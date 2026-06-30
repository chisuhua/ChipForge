// tests/cache/test_l1_cache_plugin_e2e.cpp
//
// 功能描述: L1CachePlugin e2e 测试 (Phase 1.3d)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 测试覆盖:
//   1. L1CacheTLMBridgeAdapter 通过 cpptlm::ModuleFactory 注册
//   2. ModuleFactory::create() 直接实例化 (绕过 instantiateAll 的连接/adapter 管道)
//   3. Adapter 持有 L1CacheTLMBridge (验证桥接正确)
//   4. Adapter::tick() 触发 Bridge::tick() → Bridge::pb.run() (D1' 契约)
//   5. 多事务场景: miss → 验证 lookup 行为
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §3-4 (D1=C + D1')
//   - CppTLM/include/core/module_factory.hh (registerObject + create)
//
// 限制 (Phase 1.3d 范围):
//   - ch_stream 协议转换未实现, 跳过 instantiateAll 的连接/adapter 管道
//   - 测试通过 ModuleFactory::create() 直接实例化 (单模块, 无连接)
//   - 验证 ModuleFactory 发现 + 实例化 + Bridge lifecycle (不验证 ch_stream 数据通路)
//   - Phase 1.3d-extras 范围: ch_stream adapter 注册 + full JSON instantiateAll e2e

#include "catch_amalgamated.hpp"
#include <memory>
#include <string>

#include "bundles/mem_bundles.h"
#include "cf_plugin/bridge/l1_cache_bridge.h"
#include "cf_plugin/bridge/l1_cache_bridge_adapter.h"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

using cf::bundles::CacheReq;
using cf::bundles::CacheResp;
using cf::plugin::bridge::L1CacheTLMBridge;
using cf::plugin::bridge::L1CacheTLMBridgeAdapter;

TEST_CASE("module_factory_recognizes_adapter_type", "[cache]") {
  ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>(
      "L1CacheTLMBridgeAdapter");
  auto types = ModuleFactory::getRegisteredObjectTypes();
  bool found = false;
  for (const auto& t : types) {
    if (t == "L1CacheTLMBridgeAdapter") found = true;
  }
  REQUIRE(found);
}

TEST_CASE("adapter_constructs_with_bridge", "[cache]") {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  REQUIRE(adapter != nullptr);
  REQUIRE(adapter->getName() == "l1");

  auto* bridge = adapter->bridge();
  REQUIRE(bridge != nullptr);
}

TEST_CASE("adapter_tick_triggers_bridge_pb_run", "[cache]") {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  REQUIRE(adapter != nullptr);

  CacheReq req{};
  req.address = 0xDEADBEEFULL;
  req.is_write = false;
  req.id = 1;
  adapter->bridge()->issue_request(req);

  const int before_count = adapter->bridge()->pb_run_count();
  adapter->tick();
  const int after_count = adapter->bridge()->pb_run_count();

  REQUIRE(after_count == before_count + 1);

  CacheResp resp = adapter->bridge()->read_response();
  REQUIRE(resp.hit == false);
}

TEST_CASE("miss_workflow_through_adapter", "[cache]") {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  REQUIRE(adapter != nullptr);
  auto* bridge = adapter->bridge();

  constexpr uint64_t kTestAddr = 0x000012340ABCDE00ULL;

  CacheReq req{};
  req.address = kTestAddr;
  req.is_write = false;
  req.id = 100;
  bridge->issue_request(req);
  adapter->tick();
  CacheResp resp = bridge->read_response();
  REQUIRE(resp.hit == false);
  REQUIRE(resp.id == 100);
}

TEST_CASE("multiple_transactions_via_adapter", "[cache]") {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  REQUIRE(adapter != nullptr);
  auto* bridge = adapter->bridge();

  int hit_count = 0;
  int miss_count = 0;
  for (int i = 0; i < 1000; ++i) {
    CacheReq req{};
    req.address = static_cast<uint64_t>(i % 256) * 64;
    req.is_write = false;
    req.id = static_cast<uint8_t>(i);
    bridge->issue_request(req);
    adapter->tick();
    CacheResp resp = bridge->read_response();
    if (resp.hit) {
      ++hit_count;
    } else {
      ++miss_count;
    }
  }

  REQUIRE(hit_count + miss_count == 1000);
  REQUIRE(miss_count == 1000);
  REQUIRE(hit_count == 0);
}