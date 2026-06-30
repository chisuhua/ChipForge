// tests/framework/test_ctrl_link.cpp
//

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "cf/plugin/ctrl_link.h"

using cf::plugin::CtrlLink;
using cf::plugin::Payload;

static Payload<uint64_t> g_data_key{"data"};

TEST_CASE("empty_no_halt", "[framework]") {
  CtrlLink c;
  REQUIRE(!c.should_halt());
  REQUIRE(!c.should_throw());
  REQUIRE(!c.should_flush());
  REQUIRE(c.halt_count() == 0);
}

TEST_CASE("single_halt", "[framework]") {
  CtrlLink c;
  bool flag = false;
  c.halt_when([&flag] { return flag; });
  REQUIRE(!c.should_halt());
  flag = true;
  REQUIRE(c.should_halt());
  flag = false;
  REQUIRE(!c.should_halt());
}

TEST_CASE("halt_or_merge", "[framework]") {
  CtrlLink c;
  bool a = false, b = false, c_flag = false;
  c.halt_when([&a] { return a; });
  c.halt_when([&b] { return b; });
  c.halt_when([&c_flag] { return c_flag; });
  REQUIRE(!c.should_halt());
  a = true;
  REQUIRE(c.should_halt());
  a = false;
  b = true;
  REQUIRE(c.should_halt());
  b = false;
  c_flag = true;
  REQUIRE(c.should_halt());
  c_flag = false;
  REQUIRE(!c.should_halt());
  REQUIRE(c.halt_count() == 3);
}

TEST_CASE("throw_independent", "[framework]") {
  CtrlLink c;
  bool halt_flag = false, throw_flag = false;
  c.halt_when([&halt_flag] { return halt_flag; });
  c.throw_when([&throw_flag] { return throw_flag; });
  halt_flag = true;
  REQUIRE(c.should_halt());
  REQUIRE(!c.should_throw());
  halt_flag = false;
  throw_flag = true;
  REQUIRE(!c.should_halt());
  REQUIRE(c.should_throw());
}

TEST_CASE("flush_independent", "[framework]") {
  CtrlLink c;
  bool flush_flag = false;
  c.flush_when([&flush_flag] { return flush_flag; });
  REQUIRE(!c.should_flush());
  flush_flag = true;
  REQUIRE(c.should_flush());
  REQUIRE(!c.should_halt());
  REQUIRE(!c.should_throw());
}

TEST_CASE("priority_halt_beats_throw", "[framework]") {
  CtrlLink c;
  bool f = true;
  c.halt_when([&f] { return f; });
  c.throw_when([&f] { return f; });
  c.flush_when([&f] { return f; });
  REQUIRE(c.should_halt());
  REQUIRE(c.should_throw());
  REQUIRE(c.should_flush());
  f = false;
  REQUIRE(!c.should_halt());
  REQUIRE(!c.should_throw());
  REQUIRE(!c.should_flush());
}

TEST_CASE("bypass", "[framework]") {
  CtrlLink c;
  bool bypass_active = false;
  c.bypass(g_data_key, [&bypass_active] { return bypass_active; });
  REQUIRE(!c.bypass_active(g_data_key));
  bypass_active = true;
  REQUIRE(c.bypass_active(g_data_key));
  bypass_active = false;
  REQUIRE(!c.bypass_active(g_data_key));
  REQUIRE(c.bypass_count() == 1);
}

TEST_CASE("bypass_unregistered", "[framework]") {
  CtrlLink c;
  Payload<uint32_t> other_key{"other"};
  REQUIRE(!c.bypass_active(other_key));
  REQUIRE(c.bypass_count() == 0);
}

TEST_CASE("clear", "[framework]") {
  CtrlLink c;
  bool f = true;
  c.halt_when([&f] { return f; });
  c.throw_when([&f] { return f; });
  c.flush_when([&f] { return f; });
  c.bypass(g_data_key, [&f] { return f; });
  REQUIRE(c.halt_count() == 1);
  REQUIRE(c.throw_count() == 1);
  REQUIRE(c.flush_count() == 1);
  REQUIRE(c.bypass_count() == 1);
  c.clear();
  REQUIRE(c.halt_count() == 0);
  REQUIRE(c.throw_count() == 0);
  REQUIRE(c.flush_count() == 0);
  REQUIRE(c.bypass_count() == 0);
}

TEST_CASE("null_condition_ignored", "[framework]") {
  CtrlLink c;
  c.halt_when(nullptr);
  c.throw_when(nullptr);
  c.flush_when(nullptr);
  REQUIRE(c.halt_count() == 0);
  REQUIRE(c.throw_count() == 0);
  REQUIRE(c.flush_count() == 0);
}

TEST_CASE("fluent_api_returns_self", "[framework]") {
  CtrlLink c;
  bool f = false;
  CtrlLink& r1 = c.halt_when([&f] { return f; });
  CtrlLink& r2 = c.throw_when([&f] { return f; });
  CtrlLink& r3 = c.flush_when([&f] { return f; });
  REQUIRE(&r1 == &c);
  REQUIRE(&r2 == &c);
  REQUIRE(&r3 == &c);
}


