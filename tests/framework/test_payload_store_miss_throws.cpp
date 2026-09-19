// tests/framework/test_payload_store_miss_throws.cpp
//
// Phase 6c M6: PayloadStore CH_MEM fail-fast 单测
// 验证:
//   1. CH_MEM 模式下 const get(key) 读未填充 cell 抛 std::runtime_error
//      异常消息含 "PayloadStore cell missing" 和 key.name()
//   2. CH_MEM 模式下写后读正常 (走非 const get 路径, emplace-on-miss)
//   3. TLM 模式 (不在 CH_MEM 编译) 行为不变 — 通过 #ifdef 拆分

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <memory>
#include <string>

#include <ch.hpp>
#include <core/context.h>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"

namespace cfp = cf::plugin;

namespace {

struct TestKeyTag {
  cfp::Payload<ch::core::ch_uint<32>> key{"test.payload_store.rs1"};
};

}  // namespace

TEST_CASE("payload_store_chmem_const_get_miss_throws",
          "[framework][payload][chmem][fail_fast]") {
  ch::core::context ctx("test_ctx");
  ch::core::ctx_swap guard(&ctx);

  cfp::PayloadStore store;
  const cfp::PayloadStore& cstore = store;

  TestKeyTag tag;
  const auto& key = tag.key;

  REQUIRE_THROWS_AS(cstore.get(key), std::runtime_error);

  try {
    (void)cstore.get(key);
    FAIL("expected throw");
  } catch (const std::runtime_error& e) {
    const std::string msg = e.what();
    REQUIRE(msg.find("PayloadStore cell missing") != std::string::npos);
    REQUIRE(msg.find("test.payload_store.rs1") != std::string::npos);
  }
}

TEST_CASE("payload_store_chmem_write_then_read_works",
          "[framework][payload][chmem][write]") {
  ch::core::context ctx("test_ctx_write");
  ch::core::ctx_swap guard(&ctx);

  cfp::PayloadStore store;

  TestKeyTag tag;
  auto& key = tag.key;

  ch::core::ch_uint<32> val(ch::core::ch_literal<7, 32>{});
  REQUIRE(val.impl() != nullptr);
  store.put(key, val);

  const auto& read = store.get(key);
  REQUIRE(read.impl() != nullptr);
}

#endif  // CF_PLUGIN_USE_CH_MEM