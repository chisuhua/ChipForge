// tests/cache/test_l1cache_refill_fsm_chmem.cpp
//
// Phase 6d.7: L1Cache refill FSM — 4 状态 PoC 测试
//
// 验证 (ADR-046 ch_state_machine DSL 实装):
//   1. l1cache_refill_fsm_hit: lookup hit (tag_match && line_valid) → IDLE
//   2. l1cache_refill_fsm_miss_refill: miss → refill_req → refill_done → IDLE
//   3. l1cache_refill_fsm_refill_race: REFILL_WAIT 期间新 lookup_request
//      到达 → 不中断, 完成 refill 后才回 IDLE
//
// 驱动模式 (同 test_mmu_ptw_fsm_chmem):
//   - PipeBuilder + L1CacheRefillFsmPlugin elaborate → create_simulator
//   - set_input_value(plugin.lookup_request()/tag_match()/...) 逐周期驱动
//   - tick() 推进 FSM; 断言当前拍状态/输出
//
// 状态编码: 0=IDLE, 1=LOOKUP, 2=MISS, 3=REFILL_WAIT
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cache/tlm/l1_cache_refill_fsm_chmem.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;
namespace cfcache = cf::ip::cache::tlm;

namespace {

// =========================================================================
// 通用: 构建 L1CacheRefillFsmPlugin + elaborate + simulator
// =========================================================================
struct RefillFixture {
  ch::core::context ctx;
  std::unique_ptr<cfc::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  cfcache::L1CacheRefillFsmPlugin<ch_uint<32>>* plugin = nullptr;

  RefillFixture()
      : ctx("l1c_refill_fsm_ctx"),
        pb(std::make_unique<cfc::PipeBuilder>(&ctx)) {
    ch::core::ctx_swap guard(&ctx);
    auto p =
        std::make_unique<cfcache::L1CacheRefillFsmPlugin<ch_uint<32>>>();
    plugin = p.get();
    pb->register_plugin(std::move(p));
    pb->build();
    pb->elaborate();
    sim = cf::plugin::auto_throw(pb->create_simulator());
    sim->reset();
  }

  // 单周期输入驱动 + tick
  void drive(uint64_t req, uint64_t match, uint64_t valid,
             uint64_t mem_valid) {
    sim->set_input_value(plugin->lookup_request(), req);
    sim->set_input_value(plugin->tag_match(), match);
    sim->set_input_value(plugin->line_valid(), valid);
    sim->set_input_value(plugin->mem_rdata_valid(), mem_valid);
    sim->tick();
  }

  uint64_t state() {
    return static_cast<uint64_t>(sim->get_value(plugin->state_out()));
  }
  uint64_t hit() {
    return static_cast<uint64_t>(sim->get_value(plugin->hit()));
  }
  uint64_t miss() {
    return static_cast<uint64_t>(sim->get_value(plugin->miss()));
  }
  uint64_t refill_req() {
    return static_cast<uint64_t>(sim->get_value(plugin->refill_req()));
  }
  uint64_t refill_done() {
    return static_cast<uint64_t>(sim->get_value(plugin->refill_done()));
  }
};

}  // anonymous namespace

// =========================================================================
// Test 1: lookup hit → 直接回 IDLE, hit=1, refill 未启动
// =========================================================================
TEST_CASE("l1cache_refill_fsm_hit", "[cache][chmem][6d7][refill][fsm]") {
  RefillFixture f;

  // 拍 0: IDLE + lookup_request=1 → LOOKUP
  f.drive(/*req=*/1, /*match=*/0, /*valid=*/0, /*mem_valid=*/0);
  REQUIRE(f.state() == 1);  // LOOKUP

  // 拍 1: LOOKUP + hit (tag_match=1 && line_valid=1) → 回 IDLE, hit=1
  f.drive(/*req=*/0, /*match=*/1, /*valid=*/1, /*mem_valid=*/0);
  REQUIRE(f.state() == 0);  // IDLE
  REQUIRE(f.hit() == 1);
  REQUIRE(f.miss() == 0);
  REQUIRE(f.refill_req() == 0);
  REQUIRE(f.refill_done() == 0);

  SUCCEED("L1C refill: LOOKUP hit → IDLE (hit=1, no refill)");
}

// =========================================================================
// Test 2: miss → refill_req → refill_done → IDLE
// =========================================================================
TEST_CASE("l1cache_refill_fsm_miss_refill", "[cache][chmem][6d7][refill][fsm]") {
  RefillFixture f;

  // 拍 0: IDLE + lookup_request=1 → LOOKUP
  f.drive(1, 0, 0, 0);
  REQUIRE(f.state() == 1);

  // 拍 1: LOOKUP + miss (tag_match=0) → MISS
  f.drive(0, 0, 1, 0);  // line_valid=1 但 tag 不匹配 → miss
  REQUIRE(f.state() == 2);  // MISS
  REQUIRE(f.miss() == 1);
  REQUIRE(f.hit() == 0);

  // 拍 2: MISS → REFILL_WAIT, refill_req=1
  f.drive(0, 0, 0, 0);
  REQUIRE(f.state() == 3);  // REFILL_WAIT
  REQUIRE(f.refill_req() == 1);

  // 拍 3: REFILL_WAIT + mem_rdata_valid=0 → 停留
  f.drive(0, 0, 0, 0);
  REQUIRE(f.state() == 3);
  REQUIRE(f.refill_done() == 0);

  // 拍 4: REFILL_WAIT + mem_rdata_valid=1 → 回 IDLE, refill_done=1
  f.drive(0, 0, 0, 1);
  REQUIRE(f.state() == 0);  // IDLE
  REQUIRE(f.refill_done() == 1);

  SUCCEED("L1C refill: LOOKUP miss → MISS → REFILL_WAIT → IDLE (refill_done=1)");
}

// =========================================================================
// Test 3: refill race — REFILL_WAIT 期间新 lookup_request=1
//         不中断 refill; 完成 (mem_rdata_valid=1) 后才回 IDLE
// =========================================================================
TEST_CASE("l1cache_refill_fsm_refill_race", "[cache][chmem][6d7][refill][fsm]") {
  RefillFixture f;

  // 拍 0: IDLE + lookup_request=1 → LOOKUP
  f.drive(1, 0, 0, 0);
  REQUIRE(f.state() == 1);

  // 拍 1: LOOKUP miss → MISS
  f.drive(0, 0, 0, 0);
  REQUIRE(f.state() == 2);

  // 拍 2: MISS → REFILL_WAIT
  f.drive(0, 0, 0, 0);
  REQUIRE(f.state() == 3);

  // 拍 3-4: REFILL_WAIT 期间新 lookup_request=1 (并发请求)
  //         FSM 停留 REFILL_WAIT (不中断 refill), 等 mem_rdata_valid
  f.drive(1, 0, 0, 0);
  REQUIRE(f.state() == 3);
  f.drive(1, 1, 1, 0);  // 并发请求 + tag 命中, 但不理 (还在 refill)
  REQUIRE(f.state() == 3);
  REQUIRE(f.refill_done() == 0);

  // 拍 5: refill 数据到 → 回 IDLE (此刻才接受新请求)
  f.drive(1, 1, 1, 1);
  REQUIRE(f.state() == 0);  // IDLE (refill 完成, 新请求下拍处理)
  REQUIRE(f.refill_done() == 1);

  // 拍 6: IDLE + 悬置的 lookup_request → 新 lookup 启动
  f.drive(1, 0, 0, 0);
  REQUIRE(f.state() == 1);  // LOOKUP (新请求被接受)

  SUCCEED("L1C refill race: REFILL_WAIT 期间新请求不中断, 完成后回 IDLE");
}

#endif  // CF_PLUGIN_USE_CH_MEM