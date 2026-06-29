// tests/mmu/test_multi_level_tlb.cpp (mmu-ip-skeleton, 13.2)
// lib/ 多级 TLB coherence 单元测试

#include <gtest/gtest.h>

#include "ip/mmu/lib/multi_level_tlb.h"
#include "ip/mmu/lib/tlb_factory.h"

namespace cf {
namespace ip {
namespace mmu {

namespace {
std::unique_ptr<MultiLevelTLB> make_2level(bool shadow = true) {
  std::vector<std::unique_ptr<TLBBase>> levels;
  levels.push_back(TLBFactory::create({"L0", 8, 8, 1, 1, "FIFO"}));
  levels.push_back(TLBFactory::create({"L1", 64, 4, 1, 2, "LRU"}));
  return std::make_unique<MultiLevelTLB>(std::move(levels), shadow);
}
}  // namespace

TEST(MultiLevelTLB, L0Hit) {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);  // fills deepest
  // shadow_fill true → L0 also gets it
  auto r = m->lookup(0x1000ULL, 0);
  EXPECT_TRUE(r.hit);
  EXPECT_EQ(m->level(0)->hit_count(), 1u);
}

TEST(MultiLevelTLB, L0MissL1HitShadowFill) {
  auto m = make_2level();
  // 直接插入 L1 (绕开 refill)
  m->level(1)->insert(0x3000ULL, 0x4000ULL, 0, 0xFF);
  auto r = m->lookup(0x3000ULL, 0);
  EXPECT_TRUE(r.hit);
  // shadow fill: L0 should now have it
  EXPECT_EQ(m->level(0)->hit_count(), 1u);  // looked up in L0 first, hit via shadow fill
}

TEST(MultiLevelTLB, BothMissTriggersRefill) {
  auto m = make_2level();
  auto r = m->lookup(0x9999ULL, 0);
  EXPECT_FALSE(r.hit);
  EXPECT_EQ(m->total_miss_count(), 2u);
}

TEST(MultiLevelTLB, RefillFromPtwFillAll) {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  // Both L0 (shadow fill) and L1 (deepest) should have it
  EXPECT_TRUE(m->level(0)->lookup(0x1000ULL, 0).hit);
  EXPECT_TRUE(m->level(1)->lookup(0x1000ULL, 0).hit);
}

TEST(MultiLevelTLB, NoShadowFillFlag) {
  auto m = make_2level(false);  // shadow_fill = false
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  // L0 should NOT have it (no shadow fill)
  EXPECT_FALSE(m->level(0)->lookup(0x1000ULL, 0).hit);
  // L1 should have it
  EXPECT_TRUE(m->level(1)->lookup(0x1000ULL, 0).hit);
}

TEST(MultiLevelTLB, InvalidateAsidAllLevels) {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 5, 0x2000ULL, 0xFF);
  m->invalidate_asid(5);
  EXPECT_FALSE(m->lookup(0x1000ULL, 5).hit);
}

TEST(MultiLevelTLB, InvalidateAllBothLevels) {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  m->invalidate_all();
  EXPECT_EQ(m->total_miss_count(), 0u);  // resets implicit
  EXPECT_FALSE(m->lookup(0x1000ULL, 0).hit);
}

TEST(MultiLevelTLB, NumLevels) {
  auto m = make_2level();
  EXPECT_EQ(m->num_levels(), 2u);
  EXPECT_STREQ(m->level(0)->name(), "L0");
  EXPECT_STREQ(m->level(1)->name(), "L1");
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
