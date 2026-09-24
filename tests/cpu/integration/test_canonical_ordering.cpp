// tests/cpu/integration/test_canonical_ordering.cpp
//
// ADR-048: Plugin canonical ordering assertions
//   - RED: wrong registration order (IBus before MMU) → std::logic_error
//   - GREEN: correct order (MMU before IBus via CpuFactory) → passes

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <memory>

#include "ip/cpu/cpu_factory.h"

using namespace cf::cpu;
using T = std::uint32_t;

// =========================================================================
// RED test: wrong registration order must be rejected
// =========================================================================
TEST_CASE("canonical_ordering_violation_detected", "[cpu-integration]") {
  detail::reset_canonical_counters();
  detail::IBUS_REG_ORDER = 1;    // IBusPlugin registered FIRST
  detail::MMU_REG_ORDER = 2;     // MMUPlugin registered LATER (wrong!)
  detail::DBUS_REG_ORDER = 0;
  detail::PLUGIN_SEQ = 2;
  REQUIRE_THROWS_AS(detail::check_canonical_ordering(), std::logic_error);
}

// =========================================================================
// GREEN test: MMU disabled → no constraint → no throw
// =========================================================================
TEST_CASE("canonical_ordering_mmu_disabled_skips", "[cpu-integration]") {
  detail::reset_canonical_counters();
  detail::IBUS_REG_ORDER = 1;
  detail::MMU_REG_ORDER = 0;     // 0 = not registered → early return
  REQUIRE_NOTHROW(detail::check_canonical_ordering());
}

// =========================================================================
// GREEN test: CpuFactory registers correct order (MMU before IBus)
//   After P0#1, register_early_plugins places MMU before IBus
// =========================================================================
TEST_CASE("canonical_ordering_cpu_factory_correct", "[cpu-integration]") {
  CPUConfig cfg;
  cfg.pipeline_stages = 3;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  cfg.mul_latency = 1;
  REQUIRE_NOTHROW(CpuFactory<T>::build_cpu(cfg));
}

TEST_CASE("canonical_ordering_counters_record_order", "[cpu-integration]") {
  detail::reset_canonical_counters();
  CPUConfig cfg;
  cfg.pipeline_stages = 3;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  cfg.mul_latency = 1;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  // Post-build counters should reflect MMU before IBus
  CHECK(detail::MMU_REG_ORDER > 0);
  CHECK(detail::IBUS_REG_ORDER > 0);
  CHECK(detail::MMU_REG_ORDER < detail::IBUS_REG_ORDER);
  CHECK(detail::DBUS_REG_ORDER > detail::IBUS_REG_ORDER);  // DBus after IBus
}