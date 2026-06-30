// tests/mmu/test_mmu_plugin.cpp (mmu-ip-skeleton, 13.5)

#include "catch_amalgamated.hpp"

#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace ip {
namespace mmu {

TEST_CASE("Construct", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "FIFO"},
    {"L1", 64, 4, 1, 2, "LRU"}
  };
  MMUPlugin::PTWConfig ptw_cfg{2};
  MMUPlugin mmu(SvMode::Sv39, levels, ptw_cfg);
  CHECK(mmu.num_levels() == 2u);
}

TEST_CASE("ModeSet", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv48, levels, {});
  CHECK(mmu.mode() == SvMode::Sv48);
}

TEST_CASE("SetupDeclaresSubstages", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  cf::plugin::PipeBuilder pb;
  // 直接用构造参数 make_unique, 避免 std::move (MMUPlugin 含 unique_ptr 不可拷贝)
  pb.register_plugin(std::make_unique<MMUPlugin>(SvMode::Sv39, levels, MMUPlugin::PTWConfig{2}));
  pb.build();
  // 验证 5 个 substage 都被注册
  CHECK(pb.has_stage("tlb_lookup_ifetch"));
  CHECK(pb.has_stage("tlb_lookup_loadstore"));
  CHECK(pb.has_stage("ptw_l0"));
  CHECK(pb.has_stage("ptw_l1"));
  CHECK(pb.has_stage("ptw_l2"));
}

TEST_CASE("BareMode", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 4, 1, 1, 1, "None"}};
  MMUPlugin mmu(SvMode::Bare, levels, {});
  CHECK(mmu.mode() == SvMode::Bare);
}

TEST_CASE("SingleLevelConfig", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 64, 4, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv39, levels, {});
  CHECK(mmu.num_levels() == 1u);
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
