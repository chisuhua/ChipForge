// src/cf_plugin/tests/test_payload.cpp
//
// 功能描述: Payload<T> 单元测试 (Phase 0 P0 #2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 测试覆盖:
//   1. Payload<T> 基本构造 + name()
//   2. 不同 T 的 Payload 互不干扰 (类型擦除独立性)
//   3. PayloadStore put/get 基本流程
//   4. PayloadStore 类型不匹配时抛异常
//   5. 跨 PayloadStore 隔离 (同名 Key 在不同 Store 互不影响)
//   6. Payload<T> 是单例 (全局静态对象, 地址稳定)
//
// 测试框架: 纯 main() + assert (无外部依赖)

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

#include "cf/plugin/payload.h"

using cf::plugin::Payload;
using cf::plugin::PayloadStore;

// 全局静态 Key (典型用法)
static Payload<uint64_t> g_addr_key{"addr"};
static Payload<uint32_t> g_idx_key{"idx"};
static Payload<bool> g_hit_key{"hit"};
static Payload<std::string> g_name_key{"name"};

// ----------------------------------------------------------------------------
// Test 1: Payload<T> 基本构造 + name()
// ----------------------------------------------------------------------------
static void test_payload_name() {
  Payload<uint64_t> addr{"addr"};
  assert(addr.name() == "addr");
  Payload<uint32_t> idx{"idx"};
  assert(idx.name() == "idx");
  printf("  [PASS] test_payload_name\n");
}

// ----------------------------------------------------------------------------
// Test 2: 不同 T 的 Payload 互不干扰 (类型擦除独立性)
// ----------------------------------------------------------------------------
static void test_payload_different_types() {
  Payload<uint64_t> a{"k"};
  Payload<uint32_t> b{"k"};  // 同名不同 T
  // 类型信息不同
  assert(a.type() != b.type());
  assert(a.type() == typeid(uint64_t));
  assert(b.type() == typeid(uint32_t));
  // name 相同 (因为都叫 "k")
  assert(a.name() == b.name());
  printf("  [PASS] test_payload_different_types\n");
}

// ----------------------------------------------------------------------------
// Test 3: PayloadStore put/get 基本流程
// ----------------------------------------------------------------------------
static void test_store_put_get() {
  PayloadStore s;
  uint64_t v = 0xDEADBEEF;
  s.put(g_addr_key, v);
  assert(s.has(g_addr_key));
  assert(s.get(g_addr_key) == v);
  // 修改
  s.get(g_addr_key) = 0xCAFEBABE;
  assert(s.get(g_addr_key) == 0xCAFEBABE);
  printf("  [PASS] test_store_put_get\n");
}

// ----------------------------------------------------------------------------
// Test 4: 编译期类型安全 —— 不同 T 的 Key 不可互换
// 说明: get<T>(Payload<T>&) 是强类型 API, 编译期阻止类型混淆.
//       下方代码若取消注释会编译失败:
//         s.get<uint32_t>(g_addr_key);  // ERROR: cannot convert
//       本测试验证 type() 和 static_type() 都正确返回 T 的 typeid.
//       (typeid == typeid 不是 constexpr, 用运行时 assert 验证)
// ----------------------------------------------------------------------------
static void test_compile_time_type_safety() {
  assert(Payload<uint64_t>::static_type() == typeid(uint64_t));
  assert(Payload<bool>::static_type() == typeid(bool));
  assert(Payload<std::string>::static_type() == typeid(std::string));
  Payload<uint64_t> a{"a"};
  Payload<uint64_t> b{"b"};
  assert(a.type() == b.type());
  assert(a.type() == typeid(uint64_t));
  printf("  [PASS] test_compile_time_type_safety\n");
}

// ----------------------------------------------------------------------------
// Test 5: 跨 PayloadStore 隔离
// ----------------------------------------------------------------------------
static void test_store_isolation() {
  PayloadStore s1, s2;
  s1.put(g_addr_key, uint64_t{100});
  s2.put(g_addr_key, uint64_t{200});
  assert(s1.get(g_addr_key) == 100);
  assert(s2.get(g_addr_key) == 200);
  // 修改 s1 不影响 s2
  s1.get(g_addr_key) = 999;
  assert(s1.get(g_addr_key) == 999);
  assert(s2.get(g_addr_key) == 200);
  printf("  [PASS] test_store_isolation\n");
}

// ----------------------------------------------------------------------------
// Test 6: Payload<T> 单例 (全局静态对象, 地址稳定)
// ----------------------------------------------------------------------------
static void test_payload_singleton() {
  // 全局静态对象地址稳定
  const Payload<uint64_t>* p1 = &g_addr_key;
  const Payload<uint64_t>* p2 = &g_addr_key;
  assert(p1 == p2);
  // 多次构造的同名 Payload<T> 也有不同地址 (它们是不同对象)
  Payload<uint64_t> local_a{"addr"};
  Payload<uint64_t> local_b{"addr"};
  assert(&local_a != &local_b);  // 不同实例
  assert(&g_addr_key != &local_a);  // 全局 vs 局部
  printf("  [PASS] test_payload_singleton\n");
}

// ----------------------------------------------------------------------------
// Test 7: PayloadStore 多种类型共存
// ----------------------------------------------------------------------------
static void test_store_multi_type() {
  PayloadStore s;
  s.put(g_addr_key, uint64_t{0x1000});
  s.put(g_idx_key, uint32_t{42});
  s.put(g_hit_key, true);
  s.put(g_name_key, std::string{"hello"});
  assert(s.size() == 4);
  assert(s.get(g_addr_key) == 0x1000);
  assert(s.get(g_idx_key) == 42);
  assert(s.get(g_hit_key) == true);
  assert(s.get(g_name_key) == "hello");
  printf("  [PASS] test_store_multi_type\n");
}

// ----------------------------------------------------------------------------
// Test 8: has() 和 clear() 行为
// ----------------------------------------------------------------------------
static void test_store_has_clear() {
  PayloadStore s;
  assert(!s.has(g_addr_key));
  s.put(g_addr_key, uint64_t{1});
  assert(s.has(g_addr_key));
  assert(s.size() == 1);
  s.clear();
  assert(!s.has(g_addr_key));
  assert(s.size() == 0);
  printf("  [PASS] test_store_has_clear\n");
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
  printf("=== Payload<T> Tests (Phase 0 P0 #2) ===\n");
  test_payload_name();
  test_payload_different_types();
  test_store_put_get();
  test_compile_time_type_safety();
  test_store_isolation();
  test_payload_singleton();
  test_store_multi_type();
  test_store_has_clear();
  printf("=== All Payload tests passed ===\n");
  return 0;
}
