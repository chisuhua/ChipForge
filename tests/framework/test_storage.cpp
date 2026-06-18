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

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/storage.h"
#include "cf/plugin/uint_t.h"

using cf::plugin::uint_t;
using cf::plugin::bool_t;
using cf::plugin::storage::array_store;
using cf::plugin::PipeBuilder;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const char* msg) {
  if (cond) {
    std::fprintf(stderr, "  [PASS] %s\n", msg);
    ++g_pass;
  } else {
    std::fprintf(stderr, "  [FAIL] %s\n", msg);
    ++g_fail;
  }
}

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
void test_basic_array_interface() {
  std::fprintf(stderr, "[1/5] array_store 与 std::array 接口对齐 ...\n");
  array_store<uint_t<32>, 8> store{};

  // 默认零初始化
  for (std::size_t i = 0; i < store.size(); ++i) {
    check(static_cast<uint32_t>(store[i]) == 0, "default-zero init");
  }

  // operator[] 写入并读回
  store[3] = uint_t<32>{0xDEADBEEF};
  check(static_cast<uint32_t>(store[3]) == 0xDEADBEEF, "operator[] write/read");

  // at() 边界检查 + 访问
  check(static_cast<uint32_t>(store.at(3)) == 0xDEADBEEF, "at() access");

  // 迭代器遍历
  int non_zero = 0;
  for (const auto& v : store) {
    if (static_cast<uint32_t>(v) != 0) ++non_zero;
  }
  check(non_zero == 1, "iterator finds 1 non-zero");

  // data() 指针访问
  auto* raw = store.data();
  check(static_cast<uint32_t>(raw[3]) == 0xDEADBEEF, "data() raw access");

  // size() / empty()
  check(store.size() == 8, "size() returns N");
  check(!store.empty(), "empty() returns false for N>0");
  check(array_store<uint_t<8>, 0>::empty(), "empty() returns true for N=0");
}

void test_reset() {
  std::fprintf(stderr, "[2/5] reset() 行为 ...\n");
  array_store<uint_t<8>, 4> store{};
  for (std::size_t i = 0; i < 4; ++i) store[i] = uint_t<8>{0xFF};
  store.reset();
  for (std::size_t i = 0; i < 4; ++i) {
    check(static_cast<uint8_t>(store[i]) == 0, "reset zeros element");
  }
}

void test_commit_noop_in_tlm() {
  std::fprintf(stderr, "[3/5] TLM 模式 commit() 是 no-op ...\n");
  // TLM 模式下, 即写即读; commit() 不改变可见状态
  array_store<uint_t<32>, 4> store{};
  store[0] = uint_t<32>{42};
  store.commit();
  check(static_cast<uint32_t>(store[0]) == 42,
        "commit() preserves written value (TLM no-op)");
  store[0] = uint_t<32>{99};
  // 多次 commit 无副作用
  store.commit();
  store.commit();
  check(static_cast<uint32_t>(store[0]) == 99,
        "multiple commit() calls are idempotent (TLM)");
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

void test_pipe_builder_commit_hook() {
  std::fprintf(stderr, "[4/5] PipeBuilder::register_commit_hook 集成 ...\n");
  PipeBuilder pb;
  auto plugin = std::make_unique<CommitCounterPlugin>();
  auto* raw = plugin.get();
  pb.register_plugin(std::move(plugin));

  // 注册前钩子数 = 0
  check(pb.commit_hook_count() == 0, "no hooks before build()");

  pb.build();
  check(raw->build_called == 1, "plugin build() called once");
  check(pb.commit_hook_count() == 1, "one hook registered after build()");

  // run() 末尾应自动 commit
  check(raw->commit_count == 0, "no commit before run()");
  pb.run();
  check(raw->commit_count == 1, "commit() called after run()");

  // 第二次 run() 再次触发
  pb.run();
  check(raw->commit_count == 2, "commit() called on every run()");
}

void test_multiple_hooks_order() {
  std::fprintf(stderr, "[5/5] 多钩子按注册顺序执行 ...\n");
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
  check(order.size() == 3, "all 3 hooks executed");
  if (order.size() == 3) {
    check(order[0] == 1, "hook 1 executed first");
    check(order[1] == 2, "hook 2 executed second");
    check(order[2] == 3, "hook 3 executed third");
  }
}

}  // namespace

int main() {
  std::fprintf(stderr, "=== cf::plugin::storage::array_store + commit hook 测试 ===\n\n");
  test_basic_array_interface();
  test_reset();
  test_commit_noop_in_tlm();
  test_pipe_builder_commit_hook();
  test_multiple_hooks_order();
  std::fprintf(stderr, "\n=== 结果: %d PASS, %d FAIL ===\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
