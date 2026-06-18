// tests/cache/test_replacement_policy.cpp
//
// 功能描述: ReplacementPolicy 抽象接口 + 工厂方法 + 2 个具体策略 (Phase 1.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-18
//
// 测试覆盖 (5 个, 与 openspec/changes/cache-policy-foundation/tasks.md §4.2 一致):
//   1. factory-create-LRU           ReplacementPolicy::create("LRU") 返回非空, name()=="LRU"
//   2. factory-create-None          ReplacementPolicy::create("None") 返回非空, name()=="None"
//   3. factory-unknown-throws       ReplacementPolicy::create("Unknown") 抛 std::runtime_error
//   4. LRU-on-access-increments     LRUPolicy 5 次 on_access(0,0) 后 access_counter > 0
//   5. NoReplacement-victim-zero    NoReplacementPolicy select_victim(any) 返回 0
//
// 设计说明:
//   - 纯 main() + assert (与项目其他 cache/framework 测试一致, 不引入 Catch2)
//   - tests/cache/ 而非 ip/cache/test/ (v0.0.5 empty-directory-cleanup 约定)
//   - LRU 1-way reference impl 的 on_insert 不验证 (spec 1.4 范围外; Phase 1.5 扩展)
//   - 不验证 L1CachePlugin 集成 (由 test_l1_cache_plugin_unit 的 4 个测试保证字节级兼容)
//
// 详见:
//   - openspec/changes/cache-policy-foundation/specs/cache-replacement-policy-abstraction/spec.md
//   - ip/cache/policies/replacement_policy.h (工厂方法定义)
//   - ip/cache/policies/no_replacement_policy.h
//   - ip/cache/policies/lru_policy.h

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include "ip/cache/policies/lru_policy.h"
#include "ip/cache/policies/no_replacement_policy.h"
#include "ip/cache/policies/replacement_policy.h"

using cf::ip::cache::policies::LRUPolicy;
using cf::ip::cache::policies::NoReplacementPolicy;
using cf::ip::cache::policies::ReplacementPolicy;

// ----------------------------------------------------------------------------
// 1. factory-create-LRU
//    ReplacementPolicy::create("LRU") 返回非空指针, name() == "LRU"
// ----------------------------------------------------------------------------
static void test_factory_create_lru() {
  std::unique_ptr<ReplacementPolicy> policy = ReplacementPolicy::create("LRU");
  assert(policy != nullptr && "create('LRU') must return non-null");
  assert(policy->name() == "LRU" && "LRU name() must be 'LRU'");

  // 多态性验证: 实际类型是 LRUPolicy (有 access_counter() 辅助 API)
  LRUPolicy* lru = dynamic_cast<LRUPolicy*>(policy.get());
  assert(lru != nullptr && "create('LRU') must return LRUPolicy instance");

  printf("  [PASS] factory-create-LRU\n");
}

// ----------------------------------------------------------------------------
// 2. factory-create-None
//    ReplacementPolicy::create("None") 返回非空指针, name() == "None"
// ----------------------------------------------------------------------------
static void test_factory_create_none() {
  std::unique_ptr<ReplacementPolicy> policy = ReplacementPolicy::create("None");
  assert(policy != nullptr && "create('None') must return non-null");
  assert(policy->name() == "None" && "None name() must be 'None'");

  // 多态性验证
  NoReplacementPolicy* none = dynamic_cast<NoReplacementPolicy*>(policy.get());
  assert(none != nullptr && "create('None') must return NoReplacementPolicy instance");

  printf("  [PASS] factory-create-None\n");
}

// ----------------------------------------------------------------------------
// 3. factory-unknown-throws
//    ReplacementPolicy::create("Unknown") 抛 std::runtime_error
// ----------------------------------------------------------------------------
static void test_factory_unknown_throws() {
  bool caught = false;
  try {
    std::unique_ptr<ReplacementPolicy> policy =
        ReplacementPolicy::create("UnknownBogusPolicyName");
    (void)policy;  // 抑制 unused warning
  } catch (const std::runtime_error& e) {
    caught = true;
    // 验证异常消息包含原始输入, 便于调试
    std::string msg = e.what();
    assert(msg.find("UnknownBogusPolicyName") != std::string::npos &&
           "exception message should contain original input name");
  }
  assert(caught && "create('Unknown...') must throw std::runtime_error");

  printf("  [PASS] factory-unknown-throws\n");
}

// ----------------------------------------------------------------------------
// 4. LRU-on-access-increments
//    5 次 on_access(0, 0) 后 access_counter > 0 (单调递增验证)
// ----------------------------------------------------------------------------
static void test_lru_on_access_increments() {
  LRUPolicy lru;
  const uint64_t initial = lru.access_counter();
  assert(initial == 0 && "fresh LRUPolicy must start with counter == 0");

  // 5 次 on_access (参数 set/way 对 1-way LRU 无影响, 但 API 必须支持任意输入)
  for (int i = 0; i < 5; ++i) {
    lru.on_access(0u, 0u);
  }

  const uint64_t after = lru.access_counter();
  assert(after == 5 && "access_counter must be 5 after 5 on_access calls");

  // 单调递增验证
  assert(after > initial && "access_counter must monotonically increase");

  printf("  [PASS] LRU-on-access-increments\n");
}

// ----------------------------------------------------------------------------
// 5. NoReplacement-victim-returns-zero
//    NoReplacementPolicy::select_victim(any) 始终返回 0
// ----------------------------------------------------------------------------
static void test_no_replacement_victim_returns_zero() {
  NoReplacementPolicy none;

  // 测试多个不同的 set 输入, 都应返回 0
  assert(none.select_victim(0u) == 0u);
  assert(none.select_victim(42u) == 0u);
  assert(none.select_victim(255u) == 0u);  // 1-way 场景下 256 sets 边界

  // name() 验证 (附带的额外检查)
  assert(none.name() == "None");

  printf("  [PASS] NoReplacement-victim-returns-zero\n");
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
  printf("=== ReplacementPolicy Unit Tests (Phase 1.4) ===\n");

  test_factory_create_lru();
  test_factory_create_none();
  test_factory_unknown_throws();
  test_lru_on_access_increments();
  test_no_replacement_victim_returns_zero();

  printf("=== All 5 ReplacementPolicy unit tests passed ===\n");
  return 0;
}
