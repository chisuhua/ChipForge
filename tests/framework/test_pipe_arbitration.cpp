// tests/framework/test_pipe_arbitration.cpp
//
// 功能描述: PipeArbitration 单元测试 (M1.5 + M1.6 验证, Phase 1.5+)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (6 用例):
//   1. 默认构造 = idle (全 false)
//   2. assert_valid + assert_ready = fired
//   3. 仅 assert_valid = blocked (valid && !ready)
//   4. cancel_op 隐含放弃 valid (cancel && !valid)
//   5. complete_cancel 回 idle
//   6. 嵌入 PipeNode 后的 arb() / arb_mut() / arbitration() 访问
//
// 约束:
//   - 编译期零开销 (POD 结构)

#include "catch_amalgamated.hpp"
#include <memory>

#include "cf/plugin/pipe_arbitration.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"

using cf::plugin::PipeArbitration;
using cf::plugin::PipeBuilder;
using cf::plugin::PipeNode;
using cf::plugin::Phase;

// ----------------------------------------------------------------------------
// Test 1: 默认构造 = idle
// ----------------------------------------------------------------------------
TEST_CASE("default_idle", "[framework]") {
  PipeArbitration arb;
  REQUIRE(arb.valid == false);
  REQUIRE(arb.ready == false);
  REQUIRE(arb.cancel == false);
  REQUIRE(arb.idle());
  REQUIRE(!arb.fired());
  REQUIRE(!arb.blocked());
  REQUIRE(!arb.canceling());
}

// ----------------------------------------------------------------------------
// Test 2: assert_valid + assert_ready = fired
// ----------------------------------------------------------------------------
TEST_CASE("fired_path", "[framework]") {
  PipeArbitration arb;
  arb.assert_valid();
  REQUIRE(arb.valid);
  REQUIRE(!arb.fired());  // ready 仍 false, 未握手
  arb.assert_ready();
  REQUIRE(arb.valid);
  REQUIRE(arb.ready);
  REQUIRE(arb.fired());   // 握手成功
  REQUIRE(!arb.canceling());
}

// ----------------------------------------------------------------------------
// Test 3: 仅 assert_valid = blocked (valid && !ready)
// ----------------------------------------------------------------------------
TEST_CASE("blocked_path", "[framework]") {
  PipeArbitration arb;
  arb.assert_valid();
  REQUIRE(arb.blocked());
  REQUIRE(!arb.fired());
  REQUIRE(!arb.canceling());
  // 下游反压
  arb.deassert_ready();
  REQUIRE(!arb.ready);
  REQUIRE(arb.blocked());
}

// ----------------------------------------------------------------------------
// Test 4: cancel_op 隐含放弃 valid
// ----------------------------------------------------------------------------
TEST_CASE("cancel_path", "[framework]") {
  PipeArbitration arb;
  arb.set_fired();  // valid=true, ready=true
  REQUIRE(arb.fired());
  arb.cancel_op();  // cancel=true, valid=false (隐含)
  REQUIRE(arb.cancel);
  REQUIRE(!arb.valid);
  REQUIRE(arb.canceling());
  REQUIRE(!arb.fired());
  REQUIRE(!arb.blocked());
}

// ----------------------------------------------------------------------------
// Test 5: complete_cancel 回 idle
// ----------------------------------------------------------------------------
TEST_CASE("complete_cancel", "[framework]") {
  PipeArbitration arb;
  arb.set_fired();
  arb.cancel_op();
  REQUIRE(arb.canceling());
  arb.complete_cancel();  // cancel=false, ready=false
  REQUIRE(!arb.cancel);
  REQUIRE(!arb.ready);
  REQUIRE(arb.idle());  // 全部 false
}

// ----------------------------------------------------------------------------
// Test 6: 嵌入 PipeNode 后的 arb() / arb_mut() / arbitration() 访问
// ----------------------------------------------------------------------------
TEST_CASE("arb_in_pipe_node", "[framework]") {
  PipeNode n{"test_node"};
  // const 访问: 默认 idle
  const PipeArbitration& arb_const = n.arb();
  REQUIRE(arb_const.idle());
  // 可写访问: arb_mut() / arbitration() 等价
  n.arb_mut().set_fired();
  REQUIRE(n.arb().fired());
  n.arbitration().cancel_op();
  REQUIRE(n.arb().canceling());
  // 验证: arb_ 与 5 态字段独立 (不委托)
  REQUIRE(n.state() == PipeNode::State::IDLE);  // 5 态未受影响
}

// ----------------------------------------------------------------------------
// Bonus Test: 集成到 PipeBuilder, 通过 at_stage 注册后 arb_ 仍可访问
// ----------------------------------------------------------------------------
TEST_CASE("arb_in_pipe_builder", "[framework]") {
  PipeBuilder pb;
  pb.at_stage("s1", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s1");
  REQUIRE(n);
  REQUIRE(n->arb().idle());
  n->arb_mut().assert_valid();
  pb.run();  // run() 末尾 commit_storages(), 不影响 arb_ 字段
  REQUIRE(n->arb().valid);
  REQUIRE(!n->arb().ready);
  REQUIRE(n->arb().blocked());
}

// ----------------------------------------------------------------------------
// 编译期检查 (sizeof 必须 3 字节, POD)
// ----------------------------------------------------------------------------
static_assert(sizeof(PipeArbitration) == sizeof(bool) * 3,
              "PipeArbitration must be 3 bytes (no padding)");

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

