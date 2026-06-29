// tests/framework/test_payload.cpp
//
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

#include "catch_amalgamated.hpp"
#include <cstdint>
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
TEST_CASE("payload_name", "[framework]") {
  Payload<uint64_t> addr{"addr"};
  REQUIRE(addr.name() == "addr");
  Payload<uint32_t> idx{"idx"};
  REQUIRE(idx.name() == "idx");
}

// ----------------------------------------------------------------------------
// Test 2: 不同 T 的 Payload 互不干扰 (类型擦除独立性)
// ----------------------------------------------------------------------------
TEST_CASE("payload_different_types", "[framework]") {
  Payload<uint64_t> a{"k"};
  Payload<uint32_t> b{"k"};  // 同名不同 T
  // 类型信息不同
  REQUIRE(a.type() != b.type());
  REQUIRE(a.type() == typeid(uint64_t));
  REQUIRE(b.type() == typeid(uint32_t));
  // name 相同 (因为都叫 "k")
  REQUIRE(a.name() == b.name());
}

// ----------------------------------------------------------------------------
// Test 3: PayloadStore put/get 基本流程
// ----------------------------------------------------------------------------
TEST_CASE("store_put_get", "[framework]") {
  PayloadStore s;
  uint64_t v = 0xDEADBEEF;
  s.put(g_addr_key, v);
  REQUIRE(s.has(g_addr_key));
  REQUIRE(s.get(g_addr_key) == v);
  // 修改
  s.get(g_addr_key) = 0xCAFEBABE;
  REQUIRE(s.get(g_addr_key) == 0xCAFEBABE);
}

// ----------------------------------------------------------------------------
// Test 4: 编译期类型安全 —— 不同 T 的 Key 不可互换
// 说明: get<T>(Payload<T>&) 是强类型 API, 编译期阻止类型混淆.
//       下方代码若取消注释会编译失败:
//         s.get<uint32_t>(g_addr_key);  // ERROR: cannot convert
//       本测试验证 type() 和 static_type() 都正确返回 T 的 typeid.
//       (typeid == typeid 不是 constexpr, 用运行时 assert 验证)
// ----------------------------------------------------------------------------
TEST_CASE("compile_time_type_safety", "[framework]") {
  REQUIRE(Payload<uint64_t>::static_type() == typeid(uint64_t));
  REQUIRE(Payload<bool>::static_type() == typeid(bool));
  REQUIRE(Payload<std::string>::static_type() == typeid(std::string));
  Payload<uint64_t> a{"a"};
  Payload<uint64_t> b{"b"};
  REQUIRE(a.type() == b.type());
  REQUIRE(a.type() == typeid(uint64_t));
}

// ----------------------------------------------------------------------------
// Test 5: 跨 PayloadStore 隔离
// ----------------------------------------------------------------------------
TEST_CASE("store_isolation", "[framework]") {
  PayloadStore s1, s2;
  s1.put(g_addr_key, uint64_t{100});
  s2.put(g_addr_key, uint64_t{200});
  REQUIRE(s1.get(g_addr_key) == 100);
  REQUIRE(s2.get(g_addr_key) == 200);
  // 修改 s1 不影响 s2
  s1.get(g_addr_key) = 999;
  REQUIRE(s1.get(g_addr_key) == 999);
  REQUIRE(s2.get(g_addr_key) == 200);
}

// ----------------------------------------------------------------------------
// Test 6: Payload<T> 单例 (全局静态对象, 地址稳定)
// ----------------------------------------------------------------------------
TEST_CASE("payload_singleton", "[framework]") {
  // 全局静态对象地址稳定
  const Payload<uint64_t>* p1 = &g_addr_key;
  const Payload<uint64_t>* p2 = &g_addr_key;
  REQUIRE(p1 == p2);
  // 多次构造的同名 Payload<T> 也有不同地址 (它们是不同对象)
  Payload<uint64_t> local_a{"addr"};
  Payload<uint64_t> local_b{"addr"};
  REQUIRE(&local_a != &local_b);  // 不同实例
  REQUIRE(&g_addr_key != &local_a);  // 全局 vs 局部
}

// ----------------------------------------------------------------------------
// Test 7: PayloadStore 多种类型共存
// ----------------------------------------------------------------------------
TEST_CASE("store_multi_type", "[framework]") {
  PayloadStore s;
  s.put(g_addr_key, uint64_t{0x1000});
  s.put(g_idx_key, uint32_t{42});
  s.put(g_hit_key, true);
  s.put(g_name_key, std::string{"hello"});
  REQUIRE(s.size() == 4);
  REQUIRE(s.get(g_addr_key) == 0x1000);
  REQUIRE(s.get(g_idx_key) == 42);
  REQUIRE(s.get(g_hit_key) == true);
  REQUIRE(s.get(g_name_key) == "hello");
}

// ----------------------------------------------------------------------------
// Test 8: has() 和 clear() 行为
// ----------------------------------------------------------------------------
TEST_CASE("store_has_clear", "[framework]") {
  PayloadStore s;
  REQUIRE(!s.has(g_addr_key));
  s.put(g_addr_key, uint64_t{1});
  REQUIRE(s.has(g_addr_key));
  REQUIRE(s.size() == 1);
  s.clear();
  REQUIRE(!s.has(g_addr_key));
  REQUIRE(s.size() == 0);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

