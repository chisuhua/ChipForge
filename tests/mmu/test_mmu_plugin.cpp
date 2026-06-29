// tests/mmu/test_mmu_plugin.cpp (mmu-ip-skeleton, 13.5)

#include <gtest/gtest.h>

#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace ip {
namespace mmu {

TEST(MMUPlugin, Construct) {
  std::vector<MMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "FIFO"},
    {"L1", 64, 4, 1, 2, "LRU"}
  };
  MMUPlugin::PTWConfig ptw_cfg{2};
  MMUPlugin mmu(SvMode::Sv39, levels, ptw_cfg);
  EXPECT_EQ(mmu.num_levels(), 2u);
}

TEST(MMUPlugin, ModeSet) {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv48, levels, {});
  EXPECT_EQ(mmu.mode(), SvMode::Sv48);
}

TEST(MMUPlugin, SetupDeclaresSubstages) {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv39, levels, {});
  cf::plugin::PipeBuilder pb;
  pb.register_plugin(std::make_unique<MMUPlugin>(std::move(mmu)));
  pb.build();
  // 验证 5 个 substage 都被注册
  EXPECT_TRUE(pb.has_stage("tlb_lookup_ifetch"));
  EXPECT_TRUE(pb.has_stage("tlb_lookup_loadstore"));
  EXPECT_TRUE(pb.has_stage("ptw_l0"));
  EXPECT_TRUE(pb.has_stage("ptw_l1"));
  EXPECT_TRUE(pb.has_stage("ptw_l2"));
}

TEST(MMUPlugin, BareMode) {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 4, 1, 1, 1, "None"}};
  MMUPlugin mmu(SvMode::Bare, levels, {});
  EXPECT_EQ(mmu.mode(), SvMode::Bare);
}

TEST(MMUPlugin, SingleLevelConfig) {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 64, 4, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv39, levels, {});
  EXPECT_EQ(mmu.num_levels(), 1u);
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
