// src/cf_plugin/tests/test_ctrl_link.cpp
//
// 功能描述: CtrlLink 单元测试 (Phase 0 P0 #5)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "cf/plugin/ctrl_link.h"

using cf::plugin::CtrlLink;
using cf::plugin::Payload;

static Payload<uint64_t> g_data_key{"data"};

static void test_empty_no_halt() {
  CtrlLink c;
  assert(!c.should_halt());
  assert(!c.should_throw());
  assert(!c.should_flush());
  assert(c.halt_count() == 0);
  printf("  [PASS] test_empty_no_halt\n");
}

static void test_single_halt() {
  CtrlLink c;
  bool flag = false;
  c.halt_when([&flag] { return flag; });
  assert(!c.should_halt());
  flag = true;
  assert(c.should_halt());
  flag = false;
  assert(!c.should_halt());
  printf("  [PASS] test_single_halt\n");
}

static void test_halt_or_merge() {
  CtrlLink c;
  bool a = false, b = false, c_flag = false;
  c.halt_when([&a] { return a; });
  c.halt_when([&b] { return b; });
  c.halt_when([&c_flag] { return c_flag; });
  assert(!c.should_halt());
  a = true;
  assert(c.should_halt());
  a = false;
  b = true;
  assert(c.should_halt());
  b = false;
  c_flag = true;
  assert(c.should_halt());
  c_flag = false;
  assert(!c.should_halt());
  assert(c.halt_count() == 3);
  printf("  [PASS] test_halt_or_merge\n");
}

static void test_throw_independent() {
  CtrlLink c;
  bool halt_flag = false, throw_flag = false;
  c.halt_when([&halt_flag] { return halt_flag; });
  c.throw_when([&throw_flag] { return throw_flag; });
  halt_flag = true;
  assert(c.should_halt());
  assert(!c.should_throw());
  halt_flag = false;
  throw_flag = true;
  assert(!c.should_halt());
  assert(c.should_throw());
  printf("  [PASS] test_throw_independent\n");
}

static void test_flush_independent() {
  CtrlLink c;
  bool flush_flag = false;
  c.flush_when([&flush_flag] { return flush_flag; });
  assert(!c.should_flush());
  flush_flag = true;
  assert(c.should_flush());
  assert(!c.should_halt());
  assert(!c.should_throw());
  printf("  [PASS] test_flush_independent\n");
}

static void test_priority_halt_beats_throw() {
  CtrlLink c;
  bool f = true;
  c.halt_when([&f] { return f; });
  c.throw_when([&f] { return f; });
  c.flush_when([&f] { return f; });
  assert(c.should_halt());
  assert(c.should_throw());
  assert(c.should_flush());
  f = false;
  assert(!c.should_halt());
  assert(!c.should_throw());
  assert(!c.should_flush());
  printf("  [PASS] test_priority_halt_beats_throw\n");
}

static void test_bypass() {
  CtrlLink c;
  bool bypass_active = false;
  c.bypass(g_data_key, [&bypass_active] { return bypass_active; });
  assert(!c.bypass_active(g_data_key));
  bypass_active = true;
  assert(c.bypass_active(g_data_key));
  bypass_active = false;
  assert(!c.bypass_active(g_data_key));
  assert(c.bypass_count() == 1);
  printf("  [PASS] test_bypass\n");
}

static void test_bypass_unregistered() {
  CtrlLink c;
  Payload<uint32_t> other_key{"other"};
  assert(!c.bypass_active(other_key));
  assert(c.bypass_count() == 0);
  printf("  [PASS] test_bypass_unregistered\n");
}

static void test_clear() {
  CtrlLink c;
  bool f = true;
  c.halt_when([&f] { return f; });
  c.throw_when([&f] { return f; });
  c.flush_when([&f] { return f; });
  c.bypass(g_data_key, [&f] { return f; });
  assert(c.halt_count() == 1);
  assert(c.throw_count() == 1);
  assert(c.flush_count() == 1);
  assert(c.bypass_count() == 1);
  c.clear();
  assert(c.halt_count() == 0);
  assert(c.throw_count() == 0);
  assert(c.flush_count() == 0);
  assert(c.bypass_count() == 0);
  printf("  [PASS] test_clear\n");
}

static void test_null_condition_ignored() {
  CtrlLink c;
  c.halt_when(nullptr);
  c.throw_when(nullptr);
  c.flush_when(nullptr);
  assert(c.halt_count() == 0);
  assert(c.throw_count() == 0);
  assert(c.flush_count() == 0);
  printf("  [PASS] test_null_condition_ignored\n");
}

static void test_fluent_api_returns_self() {
  CtrlLink c;
  bool f = false;
  CtrlLink& r1 = c.halt_when([&f] { return f; });
  CtrlLink& r2 = c.throw_when([&f] { return f; });
  CtrlLink& r3 = c.flush_when([&f] { return f; });
  assert(&r1 == &c);
  assert(&r2 == &c);
  assert(&r3 == &c);
  printf("  [PASS] test_fluent_api_returns_self\n");
}

int main() {
  printf("=== CtrlLink Tests (Phase 0 P0 #5) ===\n");
  test_empty_no_halt();
  test_single_halt();
  test_halt_or_merge();
  test_throw_independent();
  test_flush_independent();
  test_priority_halt_beats_throw();
  test_bypass();
  test_bypass_unregistered();
  test_clear();
  test_null_condition_ignored();
  test_fluent_api_returns_self();
  printf("=== All CtrlLink tests passed ===\n");
  return 0;
}
