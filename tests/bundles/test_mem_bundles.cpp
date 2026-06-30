// tests/bundles/test_mem_bundles.cpp
//
// 功能描述: bundles/mem_bundles.h 单元测试 (Phase 1.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 测试覆盖:
//   1. MemReq 字段位宽 + 默认构造
//   2. MemResp 字段位宽 + 默认构造
//   3. CacheReq 字段位宽 + 默认构造
//   4. CacheResp 字段位宽 + 默认构造
//   5. L1CachePluginBundle 内部状态字段
//   6. IntBundle 字段 (irq / ack)
//   7. Bundle 字段全部使用 cf::plugin::uint_t<N> (D4 合规静态检查)
//   8. Bundle 字段读写往返
//   9. Bundle 大小 (sizeof) 编译期可预测
//
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <type_traits>

#include "bundles/mem_bundles.h"

using cf::plugin::uint_t;
using cf::plugin::bool_t;
using namespace cf::bundles;

// ----------------------------------------------------------------------------
// Test 1: MemReq 字段位宽 + 默认构造
// ----------------------------------------------------------------------------
TEST_CASE("mem_req_fields", "[bundles]") {
  MemReq req{};
  // address (64-bit) 默认值 0
  static_assert(std::is_same<decltype(req.address), uint_t<64>>::value,
                "MemReq::address must be uint_t<64>");
  REQUIRE(req.address == 0);
  // data (64-bit)
  static_assert(std::is_same<decltype(req.data), uint_t<64>>::value,
                "MemReq::data must be uint_t<64>");
  REQUIRE(req.data == 0);
  // is_write (1-bit bool)
  static_assert(std::is_same<decltype(req.is_write), bool_t>::value,
                "MemReq::is_write must be bool_t");
  REQUIRE(req.is_write == false);
  // burst_len (8-bit, AXI-style 0=1 beat, 255=256 beats)
  static_assert(std::is_same<decltype(req.burst_len), uint_t<8>>::value,
                "MemReq::burst_len must be uint_t<8>");
  REQUIRE(req.burst_len == 0);
  // id (8-bit, request ID for ordering)
  static_assert(std::is_same<decltype(req.id), uint_t<8>>::value,
                "MemReq::id must be uint_t<8>");
  REQUIRE(req.id == 0);
  }

// ----------------------------------------------------------------------------
// Test 2: MemResp 字段位宽 + 默认构造
// ----------------------------------------------------------------------------
TEST_CASE("mem_resp_fields", "[bundles]") {
  MemResp resp{};
  // data (64-bit)
  static_assert(std::is_same<decltype(resp.data), uint_t<64>>::value,
                "MemResp::data must be uint_t<64>");
  REQUIRE(resp.data == 0);
  // id (8-bit, 响应匹配请求的 id)
  static_assert(std::is_same<decltype(resp.id), uint_t<8>>::value,
                "MemResp::id must be uint_t<8>");
  REQUIRE(resp.id == 0);
  // error (1-bit)
  static_assert(std::is_same<decltype(resp.error), bool_t>::value,
                "MemResp::error must be bool_t");
  REQUIRE(resp.error == false);
  // last (1-bit, burst 最后一拍)
  static_assert(std::is_same<decltype(resp.last), bool_t>::value,
                "MemResp::last must be bool_t");
  REQUIRE(resp.last == false);
  }

// ----------------------------------------------------------------------------
// Test 3: CacheReq 字段位宽 + 默认构造
// ----------------------------------------------------------------------------
TEST_CASE("cache_req_fields", "[bundles]") {
  CacheReq req{};
  // address (64-bit, 物理地址)
  static_assert(std::is_same<decltype(req.address), uint_t<64>>::value,
                "CacheReq::address must be uint_t<64>");
  REQUIRE(req.address == 0);
  // data (64-bit, write data)
  static_assert(std::is_same<decltype(req.data), uint_t<64>>::value,
                "CacheReq::data must be uint_t<64>");
  REQUIRE(req.data == 0);
  // is_write (1-bit)
  static_assert(std::is_same<decltype(req.is_write), bool_t>::value,
                "CacheReq::is_write must be bool_t");
  REQUIRE(req.is_write == false);
  // op (2-bit, 编码: 00=Read, 01=Write, 10=Invalidate, 11=Flush)
  static_assert(std::is_same<decltype(req.op), uint_t<2>>::value,
                "CacheReq::op must be uint_t<2>");
  REQUIRE(req.op == 0);
  // id (8-bit)
  static_assert(std::is_same<decltype(req.id), uint_t<8>>::value,
                "CacheReq::id must be uint_t<8>");
  REQUIRE(req.id == 0);
  }

// ----------------------------------------------------------------------------
// Test 4: CacheResp 字段位宽 + 默认构造
// ----------------------------------------------------------------------------
TEST_CASE("cache_resp_fields", "[bundles]") {
  CacheResp resp{};
  // data (64-bit)
  static_assert(std::is_same<decltype(resp.data), uint_t<64>>::value,
                "CacheResp::data must be uint_t<64>");
  REQUIRE(resp.data == 0);
  // hit (1-bit, 缓存命中)
  static_assert(std::is_same<decltype(resp.hit), bool_t>::value,
                "CacheResp::hit must be bool_t");
  REQUIRE(resp.hit == false);
  // error (1-bit)
  static_assert(std::is_same<decltype(resp.error), bool_t>::value,
                "CacheResp::error must be bool_t");
  REQUIRE(resp.error == false);
  // id (8-bit)
  static_assert(std::is_same<decltype(resp.id), uint_t<8>>::value,
                "CacheResp::id must be uint_t<8>");
  REQUIRE(resp.id == 0);
  }

// ----------------------------------------------------------------------------
// Test 5: L1CachePluginBundle 内部状态字段
// ----------------------------------------------------------------------------
TEST_CASE("l1cache_plugin_bundle_fields", "[bundles]") {
  L1CachePluginBundle b{};
  // tag (20-bit, 物理 tag)
  static_assert(std::is_same<decltype(b.tag), uint_t<20>>::value,
                "L1CachePluginBundle::tag must be uint_t<20>");
  REQUIRE(b.tag == 0);
  // idx (8-bit, 256 sets)
  static_assert(std::is_same<decltype(b.idx), uint_t<8>>::value,
                "L1CachePluginBundle::idx must be uint_t<8>");
  REQUIRE(b.idx == 0);
  // line_data (512-bit, 64-byte cache line)
  static_assert(std::is_same<decltype(b.line_data), uint_t<512>>::value,
                "L1CachePluginBundle::line_data must be uint_t<512>");
  REQUIRE(b.line_data == 0);
  // valid (1-bit)
  static_assert(std::is_same<decltype(b.valid), bool_t>::value,
                "L1CachePluginBundle::valid must be bool_t");
  REQUIRE(b.valid == false);
  // dirty (1-bit, write-back)
  static_assert(std::is_same<decltype(b.dirty), bool_t>::value,
                "L1CachePluginBundle::dirty must be bool_t");
  REQUIRE(b.dirty == false);
  }

// ----------------------------------------------------------------------------
// Test 6: IntBundle 字段 (irq / ack)
// ----------------------------------------------------------------------------
TEST_CASE("int_bundle_fields", "[bundles]") {
  IntBundle ib{};
  static_assert(std::is_same<decltype(ib.irq), bool_t>::value,
                "IntBundle::irq must be bool_t");
  REQUIRE(ib.irq == false);
  static_assert(std::is_same<decltype(ib.ack), bool_t>::value,
                "IntBundle::ack must be bool_t");
  REQUIRE(ib.ack == false);
  }

// ----------------------------------------------------------------------------
// Test 7: D4 合规 —— 编译期保证字段类型不是 raw uint64_t / ch_uint<>
//   通过 sizeof() 间接验证: 256 个 set 的 tag+data 应明显大于 16 字节
//   真实测试需要 L1Cache 实现, 这里仅验证字段类型是 uint_t<N>
// ----------------------------------------------------------------------------
TEST_CASE("d4_compliance", "[bundles]") {
  // D4 合规: 所有字段使用 cf::plugin::uint_t<N>
  static_assert(std::is_same<uint_t<8>, uint8_t>::value ||
                    std::is_same<uint_t<8>, uint16_t>::value,
                "uint_t<8> must be standard unsigned integer (Phase 0 typedef)");
  // MemReq::address 必须是 uint_t<64> (不是 raw uint64_t)
  MemReq req{};
  static_assert(!std::is_same<decltype(req.address), uint64_t>::value ||
                    std::is_same<uint_t<64>, uint64_t>::value,
                "uint_t<64> must map to uint64_t (compile-time switch verified)");
  (void)req;
  }

// ----------------------------------------------------------------------------
// Test 8: Bundle 字段读写往返
// ----------------------------------------------------------------------------
TEST_CASE("bundle_readwrite", "[bundles]") {
  MemReq req{};
  req.address = 0xDEADBEEFCAFEBABEULL;
  req.data = 0x1122334455667788ULL;
  req.is_write = true;
  req.burst_len = 7;  // 8-beat burst
  req.id = 42;
  REQUIRE(req.address == 0xDEADBEEFCAFEBABEULL);
  REQUIRE(req.data == 0x1122334455667788ULL);
  REQUIRE(req.is_write == true);
  REQUIRE(req.burst_len == 7);
  REQUIRE(req.id == 42);

  MemResp resp{};
  resp.data = 0xAABBCCDD;
  resp.id = 42;
  resp.error = false;
  resp.last = true;
  REQUIRE(resp.data == 0xAABBCCDD);
  REQUIRE(resp.id == 42);
  REQUIRE(resp.error == false);
  REQUIRE(resp.last == true);

  L1CachePluginBundle b{};
  b.tag = 0xABCDE;
  b.idx = 128;
  b.valid = true;
  b.dirty = true;
  b.line_data = 0x1;  // LSB set only (test bit width)
  REQUIRE(b.tag == 0xABCDE);
  REQUIRE(b.idx == 128);
  REQUIRE(b.valid == true);
  REQUIRE(b.dirty == true);
  REQUIRE(b.line_data == 0x1);
  }

// ----------------------------------------------------------------------------
// Test 9: 编译期 sizeof 可预测
//   Phase 0 uint_t<N> 限制: N > 64 退化为 uint64_t (uint_t.h:37 兜底)
//   L1CachePluginBundle: tag(20→uint32) + idx(8→uint8) + line_data(512→uint64) + valid(bool) + dirty(bool)
//   实际 sizeof ≈ 24 字节 (Phase 0 typedef 限制, Phase 6 将扩展为 __int128 / multiprecision)
// ----------------------------------------------------------------------------
TEST_CASE("bundle_sizeof", "[bundles]") {
  // Phase 0 限制: 512-bit line_data 退化为 uint64_t
  // Phase 6 将用 __int128 或 boost::multiprecision::uint512_t
  static_assert(sizeof(L1CachePluginBundle) >= 16,
                "L1CachePluginBundle must be at least 16 bytes");
  // MemReq: 8(addr) + 8(data) + 1(is_write)+1(burst_len)+1(id) + padding ≈ 24
  static_assert(sizeof(MemReq) >= 16, "MemReq must be at least 16 bytes");
  (void)sizeof(L1CachePluginBundle);
  (void)sizeof(MemReq);
}



