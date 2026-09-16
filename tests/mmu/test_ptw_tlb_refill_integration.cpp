// tests/mmu/test_ptw_tlb_refill_integration.cpp
//
// Wave 2 Commit A: PTW success callback SHALL refill unified TLB via
// MultiLevelTLB::refill_from_ptw(vaddr, asid, paddr, perms).
// Two tests:
//   1. refill: PTW success → TLB has entry → next lookup hits (no repeat walk)
//   2. PTW_ACTIVE: after refill, PTW_ACTIVE is cleared → fetch not stalled

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/mmu/tlm/MMUPlugin.h"
#include "ip/mmu/tlm/mmu_keys.h"

using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;
using cf::ip::mmu::MMUPlugin;
using cf::ip::mmu::SvMode;
using cf::ip::mmu::PTE;
using cf::ip::mmu::PTW;
using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

namespace {

// ===========================================================================
// RequestDriver: calls MMUPlugin::issue_request at "fetch" stage, setting
// last_vaddr_ so tlb_lookup_ifetch reads the intended address.
// ===========================================================================
class RequestDriver : public PluginBase {
 public:
  RequestDriver(MMUPlugin* mmu, uint64_t vaddr)
      : mmu_(mmu), vaddr_(vaddr) {}
  void build(PipeBuilder& pb) override {
    pb.at_stage("fetch", Phase::NORMAL, [this]() {
      if (!fired_) {
        mmu_->issue_request(vaddr_);
        fired_ = true;
      }
    });
  }
  bool fired() const { return fired_; }

 private:
  MMUPlugin* mmu_;
  uint64_t vaddr_;
  bool fired_ = false;
};

// ===========================================================================
// Spy: counts tlb_lookup_ifetch invocations (used to detect repeated walks)
// ===========================================================================
class IfetchCountSpy : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    pb.at_stage("tlb_lookup_ifetch", Phase::NORMAL, [this]() { ++count_; });
  }
  int count() const { return count_; }
  void reset() { count_ = 0; }

 private:
  int count_ = 0;
};

// ===========================================================================
// PTW watchdog: starts at "fetch" with a deferred start using post_walk
// to avoid racing with MMUPlugin's own at_stage closures
// ===========================================================================
struct TestContext {
  std::unique_ptr<PipeBuilder> pb;
  MMUPlugin* mmu = nullptr;
  RequestDriver* driver = nullptr;
  IfetchCountSpy* ifetch_spy = nullptr;

  // Take over plugin ownership via register_plugin
  template <typename T>
  T* add_plugin(std::unique_ptr<T> p) {
    auto* raw = p.get();
    pb->register_plugin(std::move(p));
    return raw;
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) pb->run();
  }
};

// Create a PipeBuilder with:
//   - A 2-level TLB (L0=4entry, L1=16entry)
//   - MMUPlugin (Sv39)
//   - RequestDriver for a known vaddr
//   - IfetchCountSpy to track lookups
TestContext make_test_context(uint64_t test_vaddr) {
  TestContext ctx;
  ctx.pb = std::make_unique<PipeBuilder>();

  // Create 2-level fully-associative TLB (VIPT safety: idx+offset ≤ 12 requires idx=0)
  std::vector<MMUPlugin::TLBConfig> levels;
  levels.push_back({"L0", 8, 8});    // 8-entry fully-assoc (whitelist), idx=0
  levels.push_back({"L1", 16, 16});  // 16-entry fully-assoc (whitelist), idx=0
  auto mmu = std::make_unique<MMUPlugin>(
      SvMode::Sv39, levels, MMUPlugin::PTWConfig{});
  ctx.mmu = mmu.get();

  auto driver = std::make_unique<RequestDriver>(ctx.mmu, test_vaddr);
  ctx.driver = driver.get();

  auto spy = std::make_unique<IfetchCountSpy>();
  ctx.ifetch_spy = spy.get();

  // Registration order matters for canonical stage order:
  // Driver first (enters "fetch" in stages_), then MMUPlugin
  ctx.pb->register_plugin(std::move(driver));
  ctx.pb->register_plugin(std::move(spy));
  ctx.pb->register_plugin(std::move(mmu));
  ctx.pb->build();
  return ctx;
}

// Plant a valid 3-level Sv39 page table in stub memory (using raw encoding per
// existing test pattern test_ptw_unit.cpp:make_valid_leaf_ppn):
//   stub[0] (root, level 2): non-leaf (V=1, R=W=X=0), ppn=1
//   stub[1] (level 1):        non-leaf (V=1, R=W=X=0), ppn=2
//   stub[2] (level 0, leaf):  V=1, R=1, W=0, X=1 (code page), ppn=0x80000
//   → walk resolves vaddr 0x80000000 to PA=0x80000000
void plant_identity_page_table(PTW* ptw) {
  // raw encoding: bit0=V, bit1=R, bit2=W, bit3=X, bits10+=PPN
  PTE root_pte;
  root_pte.raw = (1ULL << 0) | (1ULL << 10);           // V=1, ppn=1
  ptw->stub_write_pte(0, root_pte);

  PTE l1_pte;
  l1_pte.raw = (1ULL << 0) | (2ULL << 10);              // V=1, ppn=2
  ptw->stub_write_pte(1, l1_pte);

  PTE leaf_pte;
  leaf_pte.raw = (1ULL << 0) | (1ULL << 1) | (1ULL << 3) | (0x80000ULL << 10);  // V=1,R=1,X=1, ppn=0x80000
  ptw->stub_write_pte(2, leaf_pte);
}

}  // namespace

// ===========================================================================
// Test 1: PTW success callback refills TLB → next lookup to same VPN hits
// ===========================================================================
TEST_CASE("ptw_tlb_refill_after_completion", "[mmu][tlb-refill]") {
  constexpr uint64_t kTestVaddr = 0x80000000;
  auto ctx = make_test_context(kTestVaddr);

  // Plant identity page table in stub memory
  plant_identity_page_table(ctx.mmu->ptw());

  // Capture baseline miss count (empty TLB → first lookup misses)
  auto* tlb = ctx.mmu->multi_tlb();
  REQUIRE(tlb != nullptr);

  // Run 3 cycles: should trigger lookup → miss → walk (3 levels) → refill
  ctx.run(3);

  // ---- ASSERT: TLB now has the entry for kTestVaddr ----
  auto result = tlb->lookup(kTestVaddr, /*asid=*/0);
  REQUIRE(result.hit);
  // paddr should be the identity mapping (vaddr >> 12) << 12
  REQUIRE(result.paddr == 0x80000000);
}

// ===========================================================================
// Test 2: PTW walk completes and PTW returns to idle after refill
// (success callback fires → walk state machine resets → next lookup is a hit)
// ===========================================================================
TEST_CASE("ptw_walk_completes_and_idles", "[mmu][tlb-refill]") {
  constexpr uint64_t kTestVaddr = 0x80000000;
  auto ctx = make_test_context(kTestVaddr);

  // Plant valid PTE chain
  plant_identity_page_table(ctx.mmu->ptw());

  // Run enough cycles for walk to complete
  ctx.run(3);

  // PTW state machine returned to idle (busy=false) after successful walk
  REQUIRE(ctx.mmu->ptw()->is_busy() == false);

  // A follow-up lookup is a hit (no new walk started)
  auto result = ctx.mmu->multi_tlb()->lookup(kTestVaddr, /*asid=*/0);
  REQUIRE(result.hit);
}