// tests/mmu/test_ptw_stall_integration.cpp
//
// plugin-framework-stall commit B: MMU PTW-busy → IBus fetch stalled
// 验证: PipeBuilder stall loop 让 fetch stage 在 PTW_ACTIVE=1 时 skip 闭包

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/mmu/tlm/mmu_keys.h"

using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;
using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

namespace {

// Spy: counts fetch callback invocations
class FetchSpyPlugin : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    pb.at_stage("fetch", Phase::NORMAL, [this]() { ++fetch_count_; });
  }
  int fetch_count() const { return fetch_count_; }

 private:
  int fetch_count_ = 0;
};

// TLB-write plugin: injects PTW_ACTIVE=1 to tlb_lookup_ifetch node
class PTWActiveInjector : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    target_ = pb.node_of_logic_stage("tlb_lookup_ifetch");
  }
  void arm() {
    REQUIRE(target_ != nullptr);
    (*target_)(MmuKeys::PTW_ACTIVE) = true;
  }
  void disarm() {
    REQUIRE(target_ != nullptr);
    (*target_)(MmuKeys::PTW_ACTIVE) = false;
  }

 private:
  std::shared_ptr<cf::plugin::PipeNode> target_;
};

// Consumer: IBusPlugin-like fetch CtrlLink (mirrors design §3.2)
class FetchCtrlPlugin : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    auto ctrl = std::make_shared<cf::plugin::CtrlLink>();
    ctrl->halt_when([&pb]() {
      auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
      if (!n) return false;
      return static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE));
    });
    pb.register_ctrl_link("fetch", ctrl);
  }
};

// Spy: counts decode callback (must NOT be stalled by fetch stall)
class DecodeSpyPlugin : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    pb.at_stage("decode", Phase::NORMAL, [this]() { ++decode_count_; });
  }
  int decode_count() const { return decode_count_; }

 private:
  int decode_count_ = 0;
};

struct TestSetup {
  std::unique_ptr<PipeBuilder> pb;
  FetchSpyPlugin* fetch = nullptr;
  PTWActiveInjector* injector = nullptr;
  DecodeSpyPlugin* decode = nullptr;
};

TestSetup make_setup() {
  TestSetup s;
  s.pb = std::make_unique<PipeBuilder>();
  // canonical order: tlb_lookup_ifetch (substage of fetch) before fetch
  s.pb->declare_substage("fetch", "tlb_lookup_ifetch", 1);

  auto fetch_spy = std::make_unique<FetchSpyPlugin>();
  auto decoder = std::make_unique<DecodeSpyPlugin>();
  auto injector = std::make_unique<PTWActiveInjector>();
  auto fetch_ctrl = std::make_unique<FetchCtrlPlugin>();
  s.fetch = fetch_spy.get();
  s.decode = decoder.get();
  s.injector = injector.get();

  s.pb->register_plugin(std::move(fetch_spy));
  s.pb->register_plugin(std::move(decoder));
  s.pb->register_plugin(std::move(injector));
  s.pb->register_plugin(std::move(fetch_ctrl));
  s.pb->build();
  return s;
}

}  // namespace

// ===========================================================================
// 2.1.1 tlb_hit_no_stall: PTW_ACTIVE=0, fetch 正常执行 (5 cycle)
// ===========================================================================
TEST_CASE("ptw_tlb_hit_fetch_not_stalled", "[mmu][stall]") {
  auto s = make_setup();
  for (int i = 0; i < 5; ++i) s.pb->run();
  REQUIRE(s.fetch->fetch_count() == 5);
  REQUIRE(s.decode->decode_count() == 5);
}

// ===========================================================================
// 2.1.2 tlb_miss_stall_fetch: PTW_ACTIVE=1, fetch 闭包被 skip
// ===========================================================================
TEST_CASE("ptw_tlb_miss_fetch_stalled", "[mmu][stall]") {
  auto s = make_setup();
  s.injector->arm();
  s.pb->run();
  REQUIRE(s.fetch->fetch_count() == 0);  // skipped
  REQUIRE(s.decode->decode_count() == 1);  // not stalled
}

// ===========================================================================
// 2.1.3 ptw_complete_unstall: disarm 后下一 cycle fetch 重新执行
// ===========================================================================
TEST_CASE("ptw_complete_unstall_fetch_resumes", "[mmu][stall]") {
  auto s = make_setup();
  // cycle 0: PTW busy
  s.injector->arm();
  s.pb->run();
  REQUIRE(s.fetch->fetch_count() == 0);
  // cycle 1: PTW completes (real MMU completion callback clears PTW_ACTIVE)
  s.injector->disarm();
  s.pb->run();
  REQUIRE(s.fetch->fetch_count() == 1);
}
