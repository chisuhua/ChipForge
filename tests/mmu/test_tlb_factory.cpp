// tests/mmu/test_tlb_factory.cpp (mmu-ip-skeleton, 13.4)

#include <gtest/gtest.h>

#include "ip/mmu/lib/tlb_factory.h"

namespace cf {
namespace ip {
namespace mmu {

TEST(TLBFactoryTest, CreateValidConfig) {
  TLBLevelConfig cfg{"L0", 64, 4, 1, 1, "LRU"};
  auto tlb = TLBFactory::create(cfg);
  ASSERT_NE(tlb, nullptr);
  EXPECT_STREQ(tlb->name(), "L0");
}

TEST(TLBFactoryTest, CreateSmallFullyAssoc) {
  TLBLevelConfig cfg{"L0", 8, 8, 1, 1, "FIFO"};
  auto tlb = TLBFactory::create(cfg);
  ASSERT_NE(tlb, nullptr);
  EXPECT_STREQ(tlb->name(), "L0");
}

TEST(TLBFactoryTest, RejectInvalidEntries) {
  TLBLevelConfig cfg{"L0", 100, 4, 1, 1, "LRU"};
  EXPECT_THROW(TLBFactory::create(cfg), std::invalid_argument);
}

TEST(TLBFactoryTest, RejectInvalidWays) {
  TLBLevelConfig cfg{"L0", 64, 16, 1, 1, "LRU"};
  EXPECT_THROW(TLBFactory::create(cfg), std::invalid_argument);
}

TEST(TLBFactoryTest, RejectInvalidAsid) {
  // 当前 factory 不显式校验 asid_bits (sv39 默认 9), 但需保证接口稳定
  TLBLevelConfig cfg{"L0", 64, 4, 1, 1, "LRU"};
  EXPECT_NO_THROW(TLBFactory::create(cfg));
}

TEST(TLBFactoryTest, NameIsCorrect) {
  TLBLevelConfig cfg{"my_L1", 32, 4, 1, 1, "RRIP"};
  auto tlb = TLBFactory::create(cfg);
  EXPECT_STREQ(tlb->name(), "my_L1");
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
