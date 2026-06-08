// src/cf_plugin/tests/test_pipe_node.cpp
//
// 功能描述: PipeNode 单元测试 (Phase 0 P0 #3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

#include "cf/plugin/pipe_node.h"

using cf::plugin::Payload;
using cf::plugin::PipeNode;

static Payload<uint64_t> g_addr_key{"addr"};
static Payload<uint32_t> g_idx_key{"idx"};
static Payload<bool> g_valid_key{"valid"};

static void test_initial_state_idle() {
  PipeNode n{"stage_a"};
  assert(n.is_idle());
  assert(!n.is_firing());
  assert(!n.is_moving());
  assert(!n.is_blocked());
  assert(!n.is_canceling());
  assert(n.state() == PipeNode::State::IDLE);
  printf("  [PASS] test_initial_state_idle\n");
}

static void test_idle_to_firing() {
  PipeNode n{"s"};
  n.assert_valid();
  assert(n.is_firing());
  assert(!n.is_idle());
  printf("  [PASS] test_idle_to_firing\n");
}

static void test_firing_to_moving() {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  assert(n.is_moving());
  printf("  [PASS] test_firing_to_moving\n");
}

static void test_moving_to_blocked() {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.deassert_ready();
  assert(n.is_blocked());
  printf("  [PASS] test_moving_to_blocked\n");
}

static void test_blocked_to_firing() {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.deassert_ready();
  assert(n.is_blocked());
  n.assert_ready();
  assert(n.is_firing());
  printf("  [PASS] test_blocked_to_firing\n");
}

static void test_cancel_from_firing() {
  PipeNode n{"s"};
  n.assert_valid();
  n.cancel();
  assert(n.is_canceling());
  n.complete_cancel();
  assert(n.is_idle());
  printf("  [PASS] test_cancel_from_firing\n");
}

static void test_cancel_from_moving() {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.cancel();
  assert(n.is_canceling());
  printf("  [PASS] test_cancel_from_moving\n");
}

static void test_cancel_from_idle_no_op() {
  PipeNode n{"s"};
  n.cancel();
  assert(n.is_idle());
  printf("  [PASS] test_cancel_from_idle_no_op\n");
}

static void test_reset() {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  assert(n.is_moving());
  n.reset();
  assert(n.is_idle());
  printf("  [PASS] test_reset\n");
}

static void test_payload_access_via_operator() {
  PipeNode n{"s"};
  n(g_addr_key) = 0xCAFE;
  n(g_idx_key) = 7;
  n(g_valid_key) = true;
  assert(n(g_addr_key) == 0xCAFE);
  assert(n(g_idx_key) == 7);
  assert(n(g_valid_key) == true);
  assert(n.has(g_addr_key));
  printf("  [PASS] test_payload_access_via_operator\n");
}

static void test_payload_via_put() {
  PipeNode n{"s"};
  n.put(g_addr_key, uint64_t{0xBEEF});
  assert(n(g_addr_key) == 0xBEEF);
  printf("  [PASS] test_payload_via_put\n");
}

static void test_payload_isolation_between_nodes() {
  PipeNode a{"a"};
  PipeNode b{"b"};
  a(g_addr_key) = 100;
  b(g_addr_key) = 200;
  assert(a(g_addr_key) == 100);
  assert(b(g_addr_key) == 200);
  printf("  [PASS] test_payload_isolation_between_nodes\n");
}

static void test_factory_create_node() {
  auto n = PipeNode::create("factory_node");
  assert(n != nullptr);
  assert(n->name() == "factory_node");
  assert(n->is_idle());
  printf("  [PASS] test_factory_create_node\n");
}

static void test_state_name() {
  assert(std::string{PipeNode::state_name(PipeNode::State::IDLE)} == "IDLE");
  assert(std::string{PipeNode::state_name(PipeNode::State::FIRING)} == "FIRING");
  assert(std::string{PipeNode::state_name(PipeNode::State::MOVING)} == "MOVING");
  assert(std::string{PipeNode::state_name(PipeNode::State::BLOCKED)} == "BLOCKED");
  assert(std::string{PipeNode::state_name(PipeNode::State::CANCELING)} == "CANCELING");
  printf("  [PASS] test_state_name\n");
}

int main() {
  printf("=== PipeNode Tests (Phase 0 P0 #3) ===\n");
  test_initial_state_idle();
  test_idle_to_firing();
  test_firing_to_moving();
  test_moving_to_blocked();
  test_blocked_to_firing();
  test_cancel_from_firing();
  test_cancel_from_moving();
  test_cancel_from_idle_no_op();
  test_reset();
  test_payload_access_via_operator();
  test_payload_via_put();
  test_payload_isolation_between_nodes();
  test_factory_create_node();
  test_state_name();
  printf("=== All PipeNode tests passed ===\n");
  return 0;
}
