// tests/cache/test_l1_cache_plugin_unit.cpp
//
// 功能描述: L1CachePlugin 单元测试 (Phase 1.2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 测试覆盖:
//   1. Miss 路径: 首次访问空 set, hit=false, data=0
//   2. Refill 路径: MemResp 到达后, set 被填充, valid=true, tag 匹配
//   3. Hit 路径: refill 后再次访问同一地址, hit=true, data 正确返回
//   4. D4 合规: L1CachePlugin 派生自 PluginBase, PipeBuilder 编译/运行不崩溃
//
// 设计说明:
//   - issue_request / refill_from_memory / read_response / is_set_valid /
//     read_tag 都是非 static 成员, 但写入的是全局 Payload Key (匿名 namespace 静态)
//     所以任意 L1CachePlugin 实例都能驱动同一个 node 的 Payload.
//   - 存储数组 (tags_/data_/valid_) 是成员变量, 因此需要 helper 实例的 storage
//     与注册的 Plugin 实例的 storage 同步. 测试通过让 helper 与注册的 Plugin
//     都是 "空 + 单独操作" 的等价实例, 简化驱动路径.
//
// 详见:
//   - docs/roadmap/phases/phase-1-tlm-foundation.md §1.2
//   - bundles/mem_bundles.h (CacheReq / CacheResp / MemResp)
//   - ip/cache/tlm/L1CachePlugin.h (Phase 1.2 主实现)

#include "catch_amalgamated.hpp"
#include <cstddef>
#include <memory>
#include <type_traits>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "bundles/mem_bundles.h"
#include "ip/cache/tlm/L1CachePlugin.h"

using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;
using cf::bundles::CacheReq;
using cf::bundles::CacheResp;
using cf::bundles::MemResp;

// L1CachePlugin 必须派生自 PluginBase (D4 强制)
static_assert(std::is_base_of<PluginBase, cf::ip::cache::tlm::L1CachePlugin>::value,
              "L1CachePlugin must derive from cf::plugin::PluginBase");

namespace {

// helper (持有 storage + 驱动 Payload) 同时作为 pb 注册的 Plugin:
// lookup 读源与 refill 写目标必须命中同一 storage, 故 helper 直接 move 进 pb
// 这里保留原始指针供测试访问 issue_request / refill_from_memory / read_response
struct TestCtx {
  cf::ip::cache::tlm::L1CachePlugin* helper;
  PipeBuilder pb;
  std::shared_ptr<cf::plugin::PipeNode> lookup_node;
  std::shared_ptr<cf::plugin::PipeNode> refill_node;

  TestCtx() {
    auto plugin = std::make_unique<cf::ip::cache::tlm::L1CachePlugin>();
    helper = plugin.get();
    pb.register_plugin(std::move(plugin));
    pb.build();
    lookup_node = pb.node_of_logic_stage("lookup");
    refill_node = pb.node_of_logic_stage("refill");
    REQUIRE(lookup_node != nullptr);
    REQUIRE(refill_node != nullptr);
  }
};

}  // namespace

// ----------------------------------------------------------------------------
// Test 1: Miss 路径 —— 首次访问空 set
// ----------------------------------------------------------------------------
TEST_CASE("lookup_miss_path", "[cache]") {
  TestCtx ctx;
  // address = 0x00001234_0ABCDE00
  //   idx (addr[11:4])  = 0xE0
  //   tag (addr[31:12]) = 0x00001234_0A (20-bit mask)
  CacheReq req{};
  req.address = 0x000012340ABCDE00ULL;
  req.op = 0;
  req.is_write = false;
  req.id = 1;

  ctx.helper->issue_request(ctx.lookup_node, req);
  ctx.pb.run();

  CacheResp resp = ctx.helper->read_response(ctx.lookup_node);
  REQUIRE(resp.hit == false);
  REQUIRE(resp.data == 0);
  REQUIRE(resp.error == false);
  REQUIRE(resp.id == 1);
}

// ----------------------------------------------------------------------------
// Test 2: Refill 路径 —— MemResp 到达后 set 被填充
// ----------------------------------------------------------------------------
TEST_CASE("refill_path", "[cache]") {
  TestCtx ctx;

  CacheReq req{};
  req.address = 0x000012340ABCDE00ULL;
  req.op = 0;
  req.is_write = false;
  req.id = 2;
  ctx.helper->issue_request(ctx.lookup_node, req);
  MemResp mem{};
  mem.data = 0xCAFEBABEDEADBEEFULL;
  mem.id = 2;
  mem.error = false;
  mem.last = true;
  ctx.helper->refill_from_memory(ctx.refill_node, mem);
  ctx.pb.run();  // lookup + refill 同 cycle

  constexpr std::size_t kSet = 0xE0;
  REQUIRE(ctx.helper->is_set_valid(kSet) == true);
  REQUIRE(ctx.helper->read_tag(kSet) == 0x0ABCDULL);
}

// ----------------------------------------------------------------------------
// Test 3: Hit 路径 —— refill 后再次访问命中
// ----------------------------------------------------------------------------
TEST_CASE("hit_after_refill", "[cache]") {
  TestCtx ctx;

  CacheReq req{};
  req.address = 0x000012340ABCDE00ULL;
  req.op = 0;
  req.is_write = false;
  req.id = 3;

  // 第一次: miss + refill (lookup 与 refill 在同一次 pb.run() 内执行)
  // 步骤:
  //   a. issue_request 写 g_addr (lookup input)
  //   b. refill_from_memory 写 g_mem_data (refill input)
  //   c. pb.run() -> lookup (miss, 写 g_idx/g_tag/g_hit=false/g_data=0)
  //                -> refill (读 g_hit=false, 写 storage data = g_mem_data)
  ctx.helper->issue_request(ctx.lookup_node, req);
  MemResp mem{};
  mem.data = 0xCAFEBABEDEADBEEFULL;
  mem.id = 3;
  mem.error = false;
  mem.last = true;
  ctx.helper->refill_from_memory(ctx.refill_node, mem);
  ctx.pb.run();  // lookup + refill 同 cycle

  // 第二次: hit
  ctx.helper->issue_request(ctx.lookup_node, req);
  ctx.pb.run();

  CacheResp resp = ctx.helper->read_response(ctx.lookup_node);
  REQUIRE(resp.hit == true);
  REQUIRE(resp.data == 0xCAFEBABEDEADBEEFULL);
  REQUIRE(resp.error == false);
  REQUIRE(resp.id == 3);
}

// ----------------------------------------------------------------------------
// Test 4: D4 合规 —— Plugin 在最小 PipeBuilder 下运行不崩溃
// ----------------------------------------------------------------------------
TEST_CASE("d4_compliance_runtime", "[cache]") {
  TestCtx ctx;
  ctx.pb.run();
  ctx.pb.reset_all();
}

