// tests/framework/test_storage.cpp
//
// 功能描述: cf::plugin::storage::array_store<T, N> 单元测试
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 配套: ADR-040 (TLM→HDL 移植性约束)
//
// 测试目标:
//   1. 编译期约束: T 必须是 trivially copyable
//   2. 运行时行为: 与 std::array<T, N> 接口对齐
//   3. commit() 在 TLM 模式下是 no-op (即写即读)
//   4. 对接 PipeBuilder::register_commit_hook, 验证钩子在 pb.run() 末尾触发

#include "catch_amalgamated.hpp"
#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/storage.h"
#include "cf/plugin/uint_t.h"

using cf::plugin::uint_t;
using cf::plugin::bool_t;
using cf::plugin::storage::array_store;
using cf::plugin::PipeBuilder;

// ============================================================================
// 编译期约束测试
// ============================================================================
static_assert(std::is_trivially_copyable<uint_t<32>>::value,
              "uint_t<N> must be trivially copyable for array_store");
static_assert(std::is_trivially_copyable<bool_t>::value,
              "bool_t must be trivially copyable for array_store");

// ============================================================================
// 运行时行为测试
// ============================================================================
TEST_CASE("basic_array_interface", "[framework]") {
  array_store<uint_t<32>, 8> store{};

  for (std::size_t i = 0; i < store.size(); ++i) {
    REQUIRE(static_cast<uint32_t>(store[i]) == 0);
  }

  store[3] = uint_t<32>{0xDEADBEEF};
  REQUIRE(static_cast<uint32_t>(store[3]) == 0xDEADBEEF);

  REQUIRE(static_cast<uint32_t>(store.at(3)) == 0xDEADBEEF);

  int non_zero = 0;
  for (const auto& v : store) {
    if (static_cast<uint32_t>(v) != 0) ++non_zero;
  }
  REQUIRE(non_zero == 1);

  auto* raw = store.data();
  REQUIRE(static_cast<uint32_t>(raw[3]) == 0xDEADBEEF);

  REQUIRE(store.size() == 8);
  REQUIRE(!store.empty());
  REQUIRE(array_store<uint_t<8>, 0>::empty());
}

TEST_CASE("reset", "[framework]") {
  array_store<uint_t<8>, 4> store{};
  for (std::size_t i = 0; i < 4; ++i) store[i] = uint_t<8>{0xFF};
  store.reset();
  for (std::size_t i = 0; i < 4; ++i) {
    REQUIRE(static_cast<uint8_t>(store[i]) == 0);
  }
}

TEST_CASE("commit_noop_in_tlm", "[framework]") {
  array_store<uint_t<32>, 4> store{};
  store[0] = uint_t<32>{42};
  store.commit();
  REQUIRE(static_cast<uint32_t>(store[0]) == 42);

  store[0] = uint_t<32>{99};
  store.commit();
  store.commit();
  REQUIRE(static_cast<uint32_t>(store[0]) == 99);
}

// ============================================================================
// 与 PipeBuilder::register_commit_hook 集成测试
// ============================================================================
class CommitCounterPlugin : public cf::plugin::PluginBase {
 public:
  array_store<uint_t<8>, 4> storage{};
  int commit_count = 0;
  int build_called = 0;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    ++build_called;
    pb.register_commit_hook([this] {
      ++commit_count;
      storage.commit();
    });
  }
};

TEST_CASE("pipe_builder_commit_hook", "[framework]") {
  PipeBuilder pb;
  auto plugin = std::make_unique<CommitCounterPlugin>();
  auto* raw = plugin.get();
  pb.register_plugin(std::move(plugin));

  REQUIRE(pb.commit_hook_count() == 0);

  pb.build();
  REQUIRE(raw->build_called == 1);
  REQUIRE(pb.commit_hook_count() == 1);

  REQUIRE(raw->commit_count == 0);
  pb.run();
  REQUIRE(raw->commit_count == 1);

  pb.run();
  REQUIRE(raw->commit_count == 2);
}

TEST_CASE("multiple_hooks_order", "[framework]") {
  PipeBuilder pb;
  std::vector<int> order;

  class TestPlugin : public cf::plugin::PluginBase {
   public:
    std::vector<int>* order;
    explicit TestPlugin(std::vector<int>* o) : order(o) {}
    void build(cf::plugin::PipeBuilder& pb) override {
      pb.register_commit_hook([this] { order->push_back(1); });
      pb.register_commit_hook([this] { order->push_back(2); });
      pb.register_commit_hook([this] { order->push_back(3); });
    }
  };

  pb.register_plugin(std::make_unique<TestPlugin>(&order));
  pb.build();
  pb.run();
  REQUIRE(order.size() == 3);
  REQUIRE(order[0] == 1);
  REQUIRE(order[1] == 2);
  REQUIRE(order[2] == 3);
}