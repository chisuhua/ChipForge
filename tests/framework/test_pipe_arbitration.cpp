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
//   - 纯 main() + assert (与 Phase 0 cf_plugin + Phase 1.1 mem_bundles 一致)
//   - 编译期零开销 (POD 结构)

#include <cassert>
#include <cstdio>
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
static void test_default_idle() {
  PipeArbitration arb;
  assert(arb.valid == false);
  assert(arb.ready == false);
  assert(arb.cancel == false);
  assert(arb.idle());
  assert(!arb.fired());
  assert(!arb.blocked());
  assert(!arb.canceling());
  printf("  [PASS] test_default_idle\n");
}

// ----------------------------------------------------------------------------
// Test 2: assert_valid + assert_ready = fired
// ----------------------------------------------------------------------------
static void test_fired_path() {
  PipeArbitration arb;
  arb.assert_valid();
  assert(arb.valid);
  assert(!arb.fired());  // ready 仍 false, 未握手
  arb.assert_ready();
  assert(arb.valid);
  assert(arb.ready);
  assert(arb.fired());   // 握手成功
  assert(!arb.canceling());
  printf("  [PASS] test_fired_path\n");
}

// ----------------------------------------------------------------------------
// Test 3: 仅 assert_valid = blocked (valid && !ready)
// ----------------------------------------------------------------------------
static void test_blocked_path() {
  PipeArbitration arb;
  arb.assert_valid();
  assert(arb.blocked());
  assert(!arb.fired());
  assert(!arb.canceling());
  // 下游反压
  arb.deassert_ready();
  assert(!arb.ready);
  assert(arb.blocked());
  printf("  [PASS] test_blocked_path\n");
}

// ----------------------------------------------------------------------------
// Test 4: cancel_op 隐含放弃 valid
// ----------------------------------------------------------------------------
static void test_cancel_path() {
  PipeArbitration arb;
  arb.set_fired();  // valid=true, ready=true
  assert(arb.fired());
  arb.cancel_op();  // cancel=true, valid=false (隐含)
  assert(arb.cancel);
  assert(!arb.valid);
  assert(arb.canceling());
  assert(!arb.fired());
  assert(!arb.blocked());
  printf("  [PASS] test_cancel_path\n");
}

// ----------------------------------------------------------------------------
// Test 5: complete_cancel 回 idle
// ----------------------------------------------------------------------------
static void test_complete_cancel() {
  PipeArbitration arb;
  arb.set_fired();
  arb.cancel_op();
  assert(arb.canceling());
  arb.complete_cancel();  // cancel=false, ready=false
  assert(!arb.cancel);
  assert(!arb.ready);
  assert(arb.idle());  // 全部 false
  printf("  [PASS] test_complete_cancel\n");
}

// ----------------------------------------------------------------------------
// Test 6: 嵌入 PipeNode 后的 arb() / arb_mut() / arbitration() 访问
// ----------------------------------------------------------------------------
static void test_arb_in_pipe_node() {
  PipeNode n{"test_node"};
  // const 访问: 默认 idle
  const PipeArbitration& arb_const = n.arb();
  assert(arb_const.idle());
  // 可写访问: arb_mut() / arbitration() 等价
  n.arb_mut().set_fired();
  assert(n.arb().fired());
  n.arbitration().cancel_op();
  assert(n.arb().canceling());
  // 验证: arb_ 与 5 态字段独立 (不委托)
  assert(n.state() == PipeNode::State::IDLE);  // 5 态未受影响
  printf("  [PASS] test_arb_in_pipe_node\n");
}

// ----------------------------------------------------------------------------
// Bonus Test: 集成到 PipeBuilder, 通过 at_stage 注册后 arb_ 仍可访问
// ----------------------------------------------------------------------------
static void test_arb_in_pipe_builder() {
  PipeBuilder pb;
  pb.at_stage("s1", Phase::NORMAL, [](){});
  pb.build();
  auto n = pb.node_of_logic_stage("s1");
  assert(n);
  assert(n->arb().idle());
  n->arb_mut().assert_valid();
  pb.run();  // run() 末尾 commit_storages(), 不影响 arb_ 字段
  assert(n->arb().valid);
  assert(!n->arb().ready);
  assert(n->arb().blocked());
  printf("  [PASS] test_arb_in_pipe_builder\n");
}

// ----------------------------------------------------------------------------
// 编译期检查 (sizeof 必须 3 字节, POD)
// ----------------------------------------------------------------------------
static_assert(sizeof(PipeArbitration) == sizeof(bool) * 3,
              "PipeArbitration must be 3 bytes (no padding)");

// ----------------------------------------------------------------------------
// 主入口
// ----------------------------------------------------------------------------
int main() {
  printf("=== test_pipe_arbitration (M1.5+M1.6) ===\n");
  test_default_idle();
  test_fired_path();
  test_blocked_path();
  test_cancel_path();
  test_complete_cancel();
  test_arb_in_pipe_node();
  test_arb_in_pipe_builder();
  printf("=== ALL 7 TESTS PASSED ===\n");
  return 0;
}
