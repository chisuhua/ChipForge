// tests/cpu/test_hazard.cpp
//
// 功能描述: HazardPlugin 单元测试 (M2.9 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. 初始无冒险
//   2. RAW 冒险检测 (reads_rs1)
//   3. RAW 冒险检测 (reads_rs2)
//   4. WAW 冒险检测
//   5. 重置清除 scoreboard
//
// 约束:
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/plugins/hazard.h"
#include "ip/cpu/core/payload_common.h"

using cf::cpu::plugins::HazardPlugin;
using cf::cpu::core::payload::DecodePayload;
using T = std::uint32_t;

static void test_no_hazard_initially() {
  HazardPlugin<T> hz;
  assert(hz.in_flight_count() == 0);
  DecodePayload dec{};
  dec.reads_rs1 = true;
  dec.rs1_idx = 1;
  assert(!hz.has_hazard(dec));
  printf("  [PASS] test_no_hazard_initially\n");
}

static void test_raw_hazard_rs1() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(5);
  DecodePayload dec{};
  dec.reads_rs1 = true;
  dec.rs1_idx = 5;
  assert(hz.has_hazard(dec));
  printf("  [PASS] test_raw_hazard_rs1\n");
}

static void test_raw_hazard_rs2() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(7);
  DecodePayload dec{};
  dec.reads_rs1 = false;
  dec.reads_rs2 = true;
  dec.rs2_idx = 7;
  assert(hz.has_hazard(dec));
  printf("  [PASS] test_raw_hazard_rs2\n");
}

static void test_waw_hazard() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(10);
  DecodePayload dec{};
  dec.writes_rd = true;
  dec.rd_idx = 10;
  assert(hz.has_hazard(dec));
  printf("  [PASS] test_waw_hazard\n");
}

static void test_reset_clears_scoreboard() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(1);
  hz.mark_in_flight(15);
  assert(hz.in_flight_count() == 2);
  hz.reset();
  assert(hz.in_flight_count() == 0);
  printf("  [PASS] test_reset_clears_scoreboard\n");
}

int main() {
  printf("test_hazard:\n");
  test_no_hazard_initially();
  test_raw_hazard_rs1();
  test_raw_hazard_rs2();
  test_waw_hazard();
  test_reset_clears_scoreboard();
  printf("[PASS] all HazardPlugin tests\n");
  return 0;
}
