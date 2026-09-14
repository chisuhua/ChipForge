// tests/cache/test_mmu_cache_integration.cpp (mmu-cache-integration commit 3/9)
//
// 验证 L1CachePlugin VIPT 索引 + PIPT fallback (ADR-044 §3.2 数据流方案 A 消费端契约)
// 不构造 MMUPlugin 实例, 直接 mock pl::MMU_VADDR + pl::PADDR Payload Key
// 镜像 test_l1_cache_plugin_unit.cpp 的隔离测试哲学

#include "catch_amalgamated.hpp"

#include "cf/plugin/pipe_builder.h"
#include "ip/cache/tlm/L1CachePlugin.h"
#include "ip/mmu/tlm/MMUPlugin.h"
#include "ip/mmu/tlm/mmu_keys.h"
#include "bundles/tlb_bundles_tlm.hh"
#include "bundles/tlb_bundles_extension.h"
#include "cf_plugin/bridge/mmu_bridge.h"

namespace cf {
namespace ip {
namespace cache {
namespace tlm {
namespace {

using MMUPlugin = ::cf::ip::mmu::MMUPlugin;

}  // namespace

TEST_CASE("MMUPluginOutputDrivesVIPTIndex", "[cache][MMUCacheIntegration]") {
  // Mock MMU 输出: vaddr=0x4000_07F0 → idx=0x7F (kIdxBits=8, kOffsetBits=4)
  //                 paddr=0x8000_0000 → tag=0x80000 (kTagBits=20, kOffsetBits+kIdxBits=12)
  using mmu_keys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

  cf::plugin::PipeBuilder pb;
  auto plugin = std::make_unique<L1CachePlugin>();
  L1CachePlugin* helper = plugin.get();
  pb.register_plugin(std::move(plugin));
  pb.build();
  auto lookup = pb.node_of_logic_stage("lookup");

  // 预填 set 0x7F, tag=0x80000
  helper->write_set(0x7F, 0x80000, 0xDEADBEEFULL);

  // Mock MMU 输出 (无 issue_request)
  lookup->put(mmu_keys::MMU_VADDR, static_cast<std::uint64_t>(0x400007F0ULL));
  lookup->put(mmu_keys::PADDR,     static_cast<std::uint64_t>(0x80000000ULL));

  pb.run();

  auto resp = helper->read_response(lookup);
  CHECK(resp.hit);
  CHECK(resp.data == 0xDEADBEEFULL);
}

TEST_CASE("NoMMUOutputFallsBackToPIPT", "[cache][MMUCacheIntegration]") {
  // 不写 MMU_VADDR → 走 issue_request → g_addr → PIPT fallback
  // 行为与 mmu-tlb-ptw-impl 前 baseline 等价
  cf::plugin::PipeBuilder pb;
  auto plugin = std::make_unique<L1CachePlugin>();
  L1CachePlugin* helper = plugin.get();
  pb.register_plugin(std::move(plugin));
  pb.build();
  auto lookup = pb.node_of_logic_stage("lookup");

  // 预填 set 0x80, tag=0x12345
  helper->write_set(0x80, 0x12345, 0xCAFEBABEULL);

  // issue_request 路径 (无 MMU 输出)
  cf::bundles::CacheReq req{};
  req.address = 0x00012345800ULL;  // idx=0x80 (bits 11:4), tag=0x12345 (bits 31:12)
  req.id = 1;
  helper->issue_request(lookup, req);

  pb.run();

  auto resp = helper->read_response(lookup);
  CHECK(resp.hit);
  CHECK(resp.data == 0xCAFEBABEULL);
}

TEST_CASE("VIPTMissWhenVAddrMatchesButPAddrAbsent", "[cache][MMUCacheIntegration]") {
  // 仅写 MMU_VADDR, 不写 PADDR → tag_src 回退 g_addr (PIPT tag)
  // 与 baseline PIPT fallback 一致: 当 vaddr idx 匹配但 paddr tag 不匹配 → miss
  cf::plugin::PipeBuilder pb;
  auto plugin = std::make_unique<L1CachePlugin>();
  L1CachePlugin* helper = plugin.get();
  pb.register_plugin(std::move(plugin));
  pb.build();
  auto lookup = pb.node_of_logic_stage("lookup");

  using mmu_keys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;
  // Mock 只有 MMU_VADDR, 没 PADDR
  lookup->put(mmu_keys::MMU_VADDR, static_cast<std::uint64_t>(0x40000000ULL));
  // 不写 PADDR — tag_src = g_addr (default 0)

  pb.run();

  auto resp = helper->read_response(lookup);
  CHECK_FALSE(resp.hit);
}

// ptw-walk-bridge-fix commit B: MMUTLMBridge 端到端 issue_request → tick → read_response 回归网
// (修复 mmu-tlb-ptw-impl commit 9b stub 注释; 镜像 mmu_bridge_adapter.cpp:59-66 的 ch_stream→POD 转换)
TEST_CASE("EndToEndTranslationThroughBridge", "[cache][MMUCacheIntegration]") {
  // Setup: 构造 MMUTLMBridge + 预填 TLB (避免走 PTW walk 路径, 直接 hit)
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  auto plugin = std::make_unique<MMUPlugin>(cf::ip::mmu::SvMode::Sv39, levels, MMUPlugin::PTWConfig{2});
  MMUPlugin* plugin_raw = plugin.get();
  plugin_raw->multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);

  cf::plugin::bridge::MMUTLMBridge bridge(std::move(plugin));

  // Construct TlbReqBundle (ch_uint 字段) — mirror mmu_bridge_adapter.cpp:59-63
  ::bundles::TlbReqBundle ch_req{};
  ch_req.transaction_id.write(static_cast<uint64_t>(0x1));
  ch_req.vaddr.write(static_cast<uint64_t>(0x40000000ULL));
  ch_req.asid.write(static_cast<uint16_t>(0));
  ch_req.access_type.write(static_cast<uint8_t>(0));

  // issue_request → tick → read_response 真实调 MMUPlugin issue_request
  bridge.issue_request(ch_req);
  bridge.tick();
  ::bundles::TlbRespBundle ch_resp = bridge.read_response();

  CHECK(static_cast<uint64_t>(ch_resp.paddr.read()) == 0x80000000ULL);
  CHECK(static_cast<uint8_t>(ch_resp.hit.read()) == 1);
  CHECK(static_cast<uint8_t>(ch_resp.exception_code.read()) == 0);
}

}  // namespace tlm
}  // namespace cache
}  // namespace ip
}  // namespace cf
