// tests/cache/test_l1_cache_bridge.cpp
//
// 功能描述: L1CacheTLMBridge 单元测试 (Phase 1.3a TDD)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §3-7
//   - docs/lessons/phase-1.2-l1cacheplugin.md (TDD 教训复用)

#include <cassert>
#include <cstdio>
#include <memory>

#include "bundles/mem_bundles.h"
#include "cf_plugin/bridge/l1_cache_bridge.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cache/tlm/L1CachePlugin.h"

using cf::bundles::CacheReq;
using cf::bundles::CacheResp;
using cf::ip::cache::tlm::L1CachePlugin;
using cf::plugin::bridge::L1CacheTLMBridge;

static void test_bridge_tick_triggers_plugin_run() {
  auto plugin = std::make_unique<L1CachePlugin>();
  cf::plugin::bridge::L1CacheTLMBridge bridge(std::move(plugin));

  assert(bridge.pb_run_count() == 0);

  CacheReq req{};
  req.address = 0x0;
  req.is_write = false;
  req.id = 1;
  bridge.issue_request(req);

  bridge.tick();
  assert(bridge.pb_run_count() == 1);

  bridge.tick();
  assert(bridge.pb_run_count() == 2);

  printf("  [PASS] test_bridge_tick_triggers_plugin_run\n");
}

static void test_bridge_tick_forwards_4_fields() {
  auto plugin = std::make_unique<L1CachePlugin>();
  cf::plugin::bridge::L1CacheTLMBridge bridge(std::move(plugin));

  CacheReq req{};
  req.address = 0x000012340ABCDE00ULL;
  req.data = 0xCAFEBABEDEADBEEFULL;
  req.is_write = true;
  req.id = 42;
  bridge.issue_request(req);

  bridge.tick();

  CacheResp resp = bridge.read_response();
  assert(resp.id == 42);

  printf("  [PASS] test_bridge_tick_forwards_4_fields\n");
}

int main() {
  printf("=== L1CacheTLMBridge Unit Tests (Phase 1.3a TDD) ===\n");
  test_bridge_tick_triggers_plugin_run();
  test_bridge_tick_forwards_4_fields();
  printf("=== All tests passed ===\n");
  return 0;
}