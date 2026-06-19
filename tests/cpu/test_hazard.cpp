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
using cf::cpu::plugins::HazardKind;  // M4G D.3 (G.5)
using cf::cpu::core::payload::DecodePayload;
using T = std::uint32_t;

static void test_no_hazard_initially() {
  HazardPlugin<T> hz;
  assert(hz.in_flight_count() == 0);
  DecodePayload dec{};
  dec.reads_rs1 = true;
  dec.rs1_idx = 1;
  // M4G D.3 (G.5.6): has_hazard 返回 HazardKind, 不再用 bool 隐式转换
  assert(hz.has_hazard(dec) == HazardKind::NONE);
  printf("  [PASS] test_no_hazard_initially\n");
}

static void test_raw_hazard_rs1() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(5);
  DecodePayload dec{};
  dec.reads_rs1 = true;
  dec.rs1_idx = 5;
  assert(hz.has_hazard(dec) == HazardKind::RAW_RS1);
  printf("  [PASS] test_raw_hazard_rs1\n");
}

static void test_raw_hazard_rs2() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(7);
  DecodePayload dec{};
  dec.reads_rs1 = false;
  dec.reads_rs2 = true;
  dec.rs2_idx = 7;
  assert(hz.has_hazard(dec) == HazardKind::RAW_RS2);
  printf("  [PASS] test_raw_hazard_rs2\n");
}

static void test_waw_hazard() {
  HazardPlugin<T> hz;
  hz.mark_in_flight(10);
  DecodePayload dec{};
  dec.writes_rd = true;
  dec.rd_idx = 10;
  assert(hz.has_hazard(dec) == HazardKind::WAW);
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

// M4G D.2 (G.3): per-thread scoreboard 隔离
static void test_multi_thread_scoreboard() {
  HazardPlugin<T, 32, 2> hz_mt;  // 2 线程
  // tid=0 mark_in_flight 不影响 tid=1
  hz_mt.mark_in_flight(3, /*tid*/ 0);
  assert(hz_mt.has_raw(3, /*tid*/ 0));
  assert(!hz_mt.has_raw(3, /*tid*/ 1));
  // tid=1 独立 mark
  hz_mt.mark_in_flight(3, /*tid*/ 1);
  assert(hz_mt.has_raw(3, /*tid*/ 1));
  // in_flight_count 默认 tid=0
  assert(hz_mt.in_flight_count(/*tid*/ 0) == 1);
  assert(hz_mt.in_flight_count(/*tid*/ 1) == 1);
  // 总和: 2
  assert(hz_mt.in_flight_count(/*tid*/ 0) + hz_mt.in_flight_count(/*tid*/ 1) == 2);
  // reset 默认清空所有线程
  hz_mt.reset();
  assert(hz_mt.in_flight_count(/*tid*/ 0) == 0);
  assert(hz_mt.in_flight_count(/*tid*/ 1) == 0);
  printf("  [PASS] test_multi_thread_scoreboard\n");
}

// M4G D.2 (G.3): 自定义 N_REGS 编译 + 行为正确
static void test_custom_n_regs() {
  HazardPlugin<T, 8> hz8;  // 8 寄存器 (非标准 RISC-V)
  hz8.mark_in_flight(5);
  assert(hz8.has_raw(5));
  assert(!hz8.has_raw(7));  // 边界外不飞
  printf("  [PASS] test_custom_n_regs\n");
}

int main() {
  printf("test_hazard:\n");
  test_no_hazard_initially();
  test_raw_hazard_rs1();
  test_raw_hazard_rs2();
  test_waw_hazard();
  test_reset_clears_scoreboard();
  test_multi_thread_scoreboard();
  test_custom_n_regs();
  printf("[PASS] all HazardPlugin tests\n");
  return 0;
}
