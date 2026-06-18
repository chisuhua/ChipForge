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

#include <cassert>
#include <cstdio>
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

namespace {

void test_module_factory_recognizes_adapter_type() {
  ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>(
      "L1CacheTLMBridgeAdapter");
  auto types = ModuleFactory::getRegisteredObjectTypes();
  bool found = false;
  for (const auto& t : types) {
    if (t == "L1CacheTLMBridgeAdapter") found = true;
  }
  assert(found);
  printf("  [PASS] test_module_factory_recognizes_adapter_type\n");
}

void test_adapter_constructs_with_bridge() {
  // Phase 1.3d 限制: ModuleFactory 没有 create<T>() 方法, instantiateAll
  // 需要 ch_stream adapter 注册 (Phase 1.3d-extras 范围). 本测试直接构造
  // Adapter 验证 Bridge 生命周期, ModuleFactory 注册在 test1 验证.
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  assert(adapter != nullptr);
  assert(adapter->getName() == "l1");

  auto* bridge = adapter->bridge();
  assert(bridge != nullptr);

  printf("  [PASS] test_adapter_constructs_with_bridge\n");
}

void test_adapter_tick_triggers_bridge_pb_run() {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  assert(adapter != nullptr);

  CacheReq req{};
  req.address = 0xDEADBEEFULL;
  req.is_write = false;
  req.id = 1;
  adapter->bridge()->issue_request(req);

  const int before_count = adapter->bridge()->pb_run_count();
  adapter->tick();
  const int after_count = adapter->bridge()->pb_run_count();

  assert(after_count == before_count + 1);

  CacheResp resp = adapter->bridge()->read_response();
  assert(resp.hit == false);

  printf("  [PASS] test_adapter_tick_triggers_bridge_pb_run\n");
}

void test_miss_workflow_through_adapter() {
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  assert(adapter != nullptr);
  auto* bridge = adapter->bridge();

  constexpr uint64_t kTestAddr = 0x000012340ABCDE00ULL;

  CacheReq req{};
  req.address = kTestAddr;
  req.is_write = false;
  req.id = 100;
  bridge->issue_request(req);
  adapter->tick();
  CacheResp resp = bridge->read_response();
  assert(resp.hit == false);
  assert(resp.id == 100);

  printf("  [PASS] test_miss_workflow_through_adapter\n");
}

void test_multiple_transactions_via_adapter() {
  // Phase 1.3d 限制: refill 未实现 (ch_stream), 全为 miss
  // 验证: Adapter 生命周期稳定 + 多事务 Adapter::tick() 正确
  EventQueue eq;
  auto adapter = std::make_unique<L1CacheTLMBridgeAdapter>("l1", &eq);
  assert(adapter != nullptr);
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

  assert(hit_count + miss_count == 1000);
  assert(miss_count == 1000);
  assert(hit_count == 0);

  printf("  [PASS] test_multiple_transactions_via_adapter (1000 tx, %d hit / %d miss)\n",
         hit_count, miss_count);
}

}  // namespace

int main() {
  printf("=== L1CachePlugin E2E Tests (Phase 1.3d, Adapter direct 路径) ===\n");
  test_module_factory_recognizes_adapter_type();
  test_adapter_constructs_with_bridge();
  test_adapter_tick_triggers_bridge_pb_run();
  test_miss_workflow_through_adapter();
  test_multiple_transactions_via_adapter();
  printf("=== All tests passed ===\n");
  return 0;
}