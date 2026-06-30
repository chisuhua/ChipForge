// tests/mmu/test_multi_level_tlb.cpp (mmu-ip-skeleton, 13.2)
// lib/ 多级 TLB coherence 单元测试

#include "catch_amalgamated.hpp"

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

TEST_CASE("L0Hit", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);  // fills deepest
  // shadow_fill true → L0 also gets it
  auto r = m->lookup(0x1000ULL, 0);
  CHECK(r.hit);
  CHECK(m->level(0)->hit_count() == 1u);
}

TEST_CASE("L0MissL1HitShadowFill", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  // 直接插入 L1 (绕开 refill)
  m->level(1)->insert(0x3000ULL, 0x4000ULL, 0, 0xFF);
  auto r = m->lookup(0x3000ULL, 0);
  CHECK(r.hit);
  // shadow fill: L0 should now have it
  CHECK(m->level(0)->hit_count() == 1u);  // looked up in L0 first, hit via shadow fill
}

TEST_CASE("BothMissTriggersRefill", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  auto r = m->lookup(0x9999ULL, 0);
  CHECK_FALSE(r.hit);
  CHECK(m->total_miss_count() == 2u);
}

TEST_CASE("RefillFromPtwFillAll", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  // Both L0 (shadow fill) and L1 (deepest) should have it
  CHECK(m->level(0)->lookup(0x1000ULL, 0).hit);
  CHECK(m->level(1)->lookup(0x1000ULL, 0).hit);
}

TEST_CASE("NoShadowFillFlag", "[mmu][MultiLevelTLB]") {
  auto m = make_2level(false);  // shadow_fill = false
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  // L0 should NOT have it (no shadow fill)
  CHECK_FALSE(m->level(0)->lookup(0x1000ULL, 0).hit);
  // L1 should have it
  CHECK(m->level(1)->lookup(0x1000ULL, 0).hit);
}

TEST_CASE("InvalidateAsidAllLevels", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 5, 0x2000ULL, 0xFF);
  m->invalidate_asid(5);
  CHECK_FALSE(m->lookup(0x1000ULL, 5).hit);
}

TEST_CASE("InvalidateAllBothLevels", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  m->refill_from_ptw(0x1000ULL, 0, 0x2000ULL, 0xFF);
  m->invalidate_all();
  CHECK(m->total_miss_count() == 0u);  // resets implicit
  CHECK_FALSE(m->lookup(0x1000ULL, 0).hit);
}

TEST_CASE("NumLevels", "[mmu][MultiLevelTLB]") {
  auto m = make_2level();
  CHECK(m->num_levels() == 2u);
  CHECK(m->level(0)->name() == std::string("L0"));
  CHECK(m->level(1)->name() == std::string("L1"));
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
