// tests/framework/test_pipe_node.cpp
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08

#include "catch_amalgamated.hpp"
#include <memory>
#include <string>

#include "cf/plugin/pipe_node.h"

using cf::plugin::Payload;
using cf::plugin::PipeNode;

static Payload<uint64_t> g_addr_key{"addr"};
static Payload<uint32_t> g_idx_key{"idx"};
static Payload<bool> g_valid_key{"valid"};

TEST_CASE("initial_state_idle", "[framework]") {
  PipeNode n{"stage_a"};
  REQUIRE(n.is_idle());
  REQUIRE(!n.is_firing());
  REQUIRE(!n.is_moving());
  REQUIRE(!n.is_blocked());
  REQUIRE(!n.is_canceling());
  REQUIRE(n.state() == PipeNode::State::IDLE);
}

TEST_CASE("idle_to_firing", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  REQUIRE(n.is_firing());
  REQUIRE(!n.is_idle());
}

TEST_CASE("firing_to_moving", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  REQUIRE(n.is_moving());
}

TEST_CASE("moving_to_blocked", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.deassert_ready();
  REQUIRE(n.is_blocked());
}

TEST_CASE("blocked_to_firing", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.deassert_ready();
  REQUIRE(n.is_blocked());
  n.assert_ready();
  REQUIRE(n.is_firing());
}

TEST_CASE("cancel_from_firing", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.cancel();
  REQUIRE(n.is_canceling());
  n.complete_cancel();
  REQUIRE(n.is_idle());
}

TEST_CASE("cancel_from_moving", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  n.cancel();
  REQUIRE(n.is_canceling());
}

TEST_CASE("cancel_from_idle_no_op", "[framework]") {
  PipeNode n{"s"};
  n.cancel();
  REQUIRE(n.is_idle());
}

TEST_CASE("reset", "[framework]") {
  PipeNode n{"s"};
  n.assert_valid();
  n.assert_ready();
  REQUIRE(n.is_moving());
  n.reset();
  REQUIRE(n.is_idle());
}

TEST_CASE("payload_access_via_operator", "[framework]") {
  PipeNode n{"s"};
  n(g_addr_key) = 0xCAFE;
  n(g_idx_key) = 7;
  n(g_valid_key) = true;
  REQUIRE(n(g_addr_key) == 0xCAFE);
  REQUIRE(n(g_idx_key) == 7);
  REQUIRE(n(g_valid_key) == true);
  REQUIRE(n.has(g_addr_key));
}

TEST_CASE("payload_via_put", "[framework]") {
  PipeNode n{"s"};
  n.put(g_addr_key, uint64_t{0xBEEF});
  REQUIRE(n(g_addr_key) == 0xBEEF);
}

TEST_CASE("payload_isolation_between_nodes", "[framework]") {
  PipeNode a{"a"};
  PipeNode b{"b"};
  a(g_addr_key) = 100;
  b(g_addr_key) = 200;
  REQUIRE(a(g_addr_key) == 100);
  REQUIRE(b(g_addr_key) == 200);
}

TEST_CASE("factory_create_node", "[framework]") {
  auto n = PipeNode::create("factory_node");
  REQUIRE(n != nullptr);
  REQUIRE(n->name() == "factory_node");
  REQUIRE(n->is_idle());
}

TEST_CASE("state_name", "[framework]") {
  REQUIRE(std::string{PipeNode::state_name(PipeNode::State::IDLE)} == "IDLE");
  REQUIRE(std::string{PipeNode::state_name(PipeNode::State::FIRING)} == "FIRING");
  REQUIRE(std::string{PipeNode::state_name(PipeNode::State::MOVING)} == "MOVING");
  REQUIRE(std::string{PipeNode::state_name(PipeNode::State::BLOCKED)} == "BLOCKED");
  REQUIRE(std::string{PipeNode::state_name(PipeNode::State::CANCELING)} == "CANCELING");
}


