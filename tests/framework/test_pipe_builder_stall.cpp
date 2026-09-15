// tests/framework/test_pipe_builder_stall.cpp
//
// plugin-framework-stall commit A: PipeBuilder::run() CtrlLink stall loop
// 覆盖: register_ctrl_link / should_stall_stage / ctrl_link_count /
//       get_ctrl_link / clear_ctrl_links / throw_when 全局异常 /
//       flush_when 不框架消费

#include "catch_amalgamated.hpp"

#include <atomic>
#include <memory>
#include <string>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/plugin_exception.h"

using cf::plugin::CtrlLink;
using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;
using cf::plugin::PluginException;

// minimal spy plugin: registers a callback in a named stage
class SpyPlugin : public PluginBase {
 public:
  explicit SpyPlugin(std::string stage_name) : stage_name_(std::move(stage_name)) {}
  void build(PipeBuilder& pb) override {
    pb.at_stage(stage_name_, Phase::NORMAL, [this]() { ++call_count_; });
  }
  int call_count() const { return call_count_; }
  void reset_count() { call_count_ = 0; }

 private:
  std::string stage_name_;
  int call_count_ = 0;
};

// ===========================================================================
// 1.1.1 empty_ctrl_no_effect: 无 CtrlLink 时 pb.run() 行为不变 (baseline 兼容)
// ===========================================================================
TEST_CASE("empty_ctrl_no_effect", "[framework][stall]") {
  PipeBuilder pb;
  auto spy_a = std::make_unique<SpyPlugin>("fetch");
  auto spy_b = std::make_unique<SpyPlugin>("execute");
  auto* ptr_a = spy_a.get();
  auto* ptr_b = spy_b.get();
  pb.register_plugin(std::move(spy_a));
  pb.register_plugin(std::move(spy_b));
  pb.build();
  pb.run();
  REQUIRE(ptr_a->call_count() == 1);
  REQUIRE(ptr_b->call_count() == 1);
}

// ===========================================================================
// 1.1.2 single_halt_skip_callback: halt=true 时该 stage 闭包不被调用
// ===========================================================================
TEST_CASE("single_halt_skip_callback", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> flag{true};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->halt_when([&flag]() { return flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  pb.run();
  REQUIRE(ptr->call_count() == 0);  // skipped due to halt
}

// ===========================================================================
// 1.1.3 single_halt_false_normal: halt=false 时该 stage 闭包正常执行
// ===========================================================================
TEST_CASE("single_halt_false_normal", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> flag{false};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->halt_when([&flag]() { return flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  pb.run();
  REQUIRE(ptr->call_count() == 1);  // normal execution
}

// ===========================================================================
// 1.1.4 or_merge_two_ctrls: 2 CtrlLink OR 合并
// ===========================================================================
TEST_CASE("or_merge_two_ctrls", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> a{false}, b{false};
  auto c1 = std::make_shared<CtrlLink>();
  c1->halt_when([&a]() { return a.load(); });
  auto c2 = std::make_shared<CtrlLink>();
  c2->halt_when([&b]() { return b.load(); });
  pb.register_ctrl_link("fetch", c1);
  pb.register_ctrl_link("fetch", c2);

  SECTION("both false → not stalled") {
    pb.run();
    REQUIRE(ptr->call_count() == 1);
  }
  SECTION("a true → stalled") {
    a = true;
    pb.run();
    REQUIRE(ptr->call_count() == 0);
  }
  SECTION("b true → stalled") {
    b = true;
    pb.run();
    REQUIRE(ptr->call_count() == 0);
  }
}

// ===========================================================================
// 1.1.5 or_merge_three_ctrls: 3 CtrlLink OR 合并（含 ctrl_link_count 断言）
// ===========================================================================
TEST_CASE("or_merge_three_ctrls", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  auto c1 = std::make_shared<CtrlLink>();
  auto c2 = std::make_shared<CtrlLink>();
  auto c3 = std::make_shared<CtrlLink>();
  pb.register_ctrl_link("fetch", c1);
  pb.register_ctrl_link("fetch", c2);
  pb.register_ctrl_link("fetch", c3);

  REQUIRE(pb.ctrl_link_count("fetch") == 3);
  REQUIRE(pb.should_stall_stage("fetch") == false);

  pb.run();
  REQUIRE(ptr->call_count() == 1);
}

// ===========================================================================
// 1.1.6 other_stage_not_affected: stage A stall 不影响 stage B 推进
// ===========================================================================
TEST_CASE("other_stage_not_affected", "[framework][stall]") {
  PipeBuilder pb;
  auto spy_a = std::make_unique<SpyPlugin>("fetch");
  auto spy_b = std::make_unique<SpyPlugin>("execute");
  auto* ptr_a = spy_a.get();
  auto* ptr_b = spy_b.get();
  pb.register_plugin(std::move(spy_a));
  pb.register_plugin(std::move(spy_b));
  pb.build();

  std::atomic<bool> flag{true};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->halt_when([&flag]() { return flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  pb.run();
  REQUIRE(ptr_a->call_count() == 0);  // fetch stalled
  REQUIRE(ptr_b->call_count() == 1);  // execute not affected
}

// ===========================================================================
// 1.1.7 throw_when_throws: should_throw=true 抛 PluginException
// ===========================================================================
TEST_CASE("throw_when_throws", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> throw_flag{true};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->throw_when([&throw_flag]() { return throw_flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  REQUIRE_THROWS_AS(pb.run(), PluginException);
}

// ===========================================================================
// 1.1.8 throw_then_no_commit: throw 后 commit_storages 不执行（异常路径）
// ===========================================================================
TEST_CASE("throw_then_no_commit", "[framework][stall]") {
  PipeBuilder pb;
  std::atomic<int> commit_count{0};
  pb.register_commit_hook([&commit_count]() { ++commit_count; });

  auto spy = std::make_unique<SpyPlugin>("fetch");
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> throw_flag{true};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->throw_when([&throw_flag]() { return throw_flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  REQUIRE_THROWS_AS(pb.run(), PluginException);
  REQUIRE(commit_count.load() == 0);  // commit_storages skipped on throw
}

// ===========================================================================
// 1.1.9 flush_not_framework_consumed: should_flush=true 时闭包仍执行
// ===========================================================================
TEST_CASE("flush_not_framework_consumed", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> flush_flag{true};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->flush_when([&flush_flag]() { return flush_flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  pb.run();
  // flush is plugin-consumed, not framework-consumed
  REQUIRE(ptr->call_count() == 1);
}

// ===========================================================================
// 1.1.10 register_ctrl_link_count: ctrl_link_count(stage) 返回注册数
// ===========================================================================
TEST_CASE("register_ctrl_link_count", "[framework][stall]") {
  PipeBuilder pb;
  REQUIRE(pb.ctrl_link_count("fetch") == 0);
  REQUIRE(pb.ctrl_link_count("execute") == 0);

  pb.register_ctrl_link("fetch", std::make_shared<CtrlLink>());
  REQUIRE(pb.ctrl_link_count("fetch") == 1);

  pb.register_ctrl_link("fetch", std::make_shared<CtrlLink>());
  pb.register_ctrl_link("fetch", std::make_shared<CtrlLink>());
  REQUIRE(pb.ctrl_link_count("fetch") == 3);

  // get_ctrl_link roundtrip
  auto retrieved = pb.get_ctrl_link("fetch", 1);
  REQUIRE(retrieved != nullptr);
}

// ===========================================================================
// 1.1.11 halt_stateful: 多次 run() 时 lambda 状态保留（flag 改变）
// ===========================================================================
TEST_CASE("halt_stateful", "[framework][stall]") {
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));
  pb.build();

  std::atomic<bool> flag{false};
  auto ctrl = std::make_shared<CtrlLink>();
  ctrl->halt_when([&flag]() { return flag.load(); });
  pb.register_ctrl_link("fetch", ctrl);

  pb.run();
  REQUIRE(ptr->call_count() == 1);  // flag false → executes

  flag = true;
  pb.run();
  REQUIRE(ptr->call_count() == 1);  // flag true → skipped (still 1)

  flag = false;
  pb.run();
  REQUIRE(ptr->call_count() == 2);  // flag false again → executes (now 2)
}

// ===========================================================================
// 1.1.12 shared_ptr_ctrl_keeps_alive: shared_ptr 持有 lambda 捕获对象安全
// ===========================================================================
TEST_CASE("shared_ptr_ctrl_keeps_alive", "[framework][stall]") {
  // lambda captures by reference to local flag; if CtrlLink dies before run,
  // captured reference dangles. shared_ptr must keep lambda+flag alive.
  PipeBuilder pb;
  auto spy = std::make_unique<SpyPlugin>("fetch");
  auto* ptr = spy.get();
  pb.register_plugin(std::move(spy));

  std::atomic<bool> flag{false};
  {
    auto ctrl = std::make_shared<CtrlLink>();
    ctrl->halt_when([&flag]() { return flag.load(); });
    pb.register_ctrl_link("fetch", ctrl);
  }
  // ctrl out of scope, but shared_ptr in PipeBuilder should keep it alive

  pb.build();
  flag = true;  // change flag after ctrl would have died
  pb.run();
  REQUIRE(ptr->call_count() == 0);  // halt still observable
}
