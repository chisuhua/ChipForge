// tests/cpu/test_branch_predictor.cpp
//
// 功能描述: BranchPredictorPlugin 单元测试 (M2.9 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (6 用例):
//   1. 初始无预测
//   2. update 后可预测 taken
//   3. update 后 BTB 命中
//   4. 不跳转时 predict 返回 0
//   5. 多个地址独立 BTB
//   6. reset 清除所有状态
//
// 约束:
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/plugins/branch_predictor.h"

using cf::cpu::plugins::BranchPredictorPlugin;
using T = std::uint32_t;

static void test_no_predict_initially() {
  BranchPredictorPlugin<T> bp;
  assert(bp.predict(0x1000) == 0);
  assert(!bp.predict_taken(0x1000));
  printf("  [PASS] test_no_predict_initially\n");
}

static void test_predict_after_update() {
  BranchPredictorPlugin<T> bp;
  bp.update(0x1000, true, 0x2000);
  assert(bp.predict(0x1000) == 0x2000);
  assert(bp.predict_taken(0x1000));
  printf("  [PASS] test_predict_after_update\n");
}

static void test_btb_entry() {
  BranchPredictorPlugin<T> bp;
  bp.update(0x1000, true, 0x2000);
  auto entry = bp.btb_entry(0);
  assert(entry.valid);
  assert(entry.tag == 0x1000);
  assert(entry.target == 0x2000);
  printf("  [PASS] test_btb_entry\n");
}

static void test_not_taken_returns_zero() {
  BranchPredictorPlugin<T> bp;
  bp.update(0x1000, false, 0);
  assert(bp.predict(0x1000) == 0);
  assert(!bp.predict_taken(0x1000));
  printf("  [PASS] test_not_taken_returns_zero\n");
}

static void test_multiple_addresses() {
  BranchPredictorPlugin<T> bp;
  bp.update(0x1004, true, 0x3000);
  bp.update(0x1008, true, 0x4000);
  assert(bp.predict(0x1004) == 0x3000);
  assert(bp.predict(0x1008) == 0x4000);
  printf("  [PASS] test_multiple_addresses\n");
}

static void test_reset() {
  BranchPredictorPlugin<T> bp;
  bp.update(0x1000, true, 0x2000);
  bp.reset();
  assert(bp.predict(0x1000) == 0);
  assert(bp.global_history() == 0);
  printf("  [PASS] test_reset\n");
}

int main() {
  printf("test_branch_predictor:\n");
  test_no_predict_initially();
  test_predict_after_update();
  test_btb_entry();
  test_not_taken_returns_zero();
  test_multiple_addresses();
  test_reset();
  printf("[PASS] all BranchPredictorPlugin tests\n");
  return 0;
}
