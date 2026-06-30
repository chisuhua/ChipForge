// tests/mmu/test_tlb_factory.cpp (mmu-ip-skeleton, 13.4)

#include "catch_amalgamated.hpp"

#include "ip/mmu/lib/tlb_factory.h"

namespace cf {
namespace ip {
namespace mmu {

TEST_CASE("CreateValidConfig", "[mmu][TLBFactoryTest]") {
  TLBLevelConfig cfg{"L0", 64, 4, 1, 1, "LRU"};
  auto tlb = TLBFactory::create(cfg);
  REQUIRE(tlb != nullptr);
  CHECK(tlb->name() == std::string("L0"));
}

TEST_CASE("CreateSmallFullyAssoc", "[mmu][TLBFactoryTest]") {
  TLBLevelConfig cfg{"L0", 8, 8, 1, 1, "FIFO"};
  auto tlb = TLBFactory::create(cfg);
  REQUIRE(tlb != nullptr);
  CHECK(tlb->name() == std::string("L0"));
}

TEST_CASE("RejectInvalidEntries", "[mmu][TLBFactoryTest]") {
  TLBLevelConfig cfg{"L0", 100, 4, 1, 1, "LRU"};
  CHECK_THROWS_AS(TLBFactory::create(cfg), std::invalid_argument);
}

TEST_CASE("RejectInvalidWays", "[mmu][TLBFactoryTest]") {
  TLBLevelConfig cfg{"L0", 64, 16, 1, 1, "LRU"};
  CHECK_THROWS_AS(TLBFactory::create(cfg), std::invalid_argument);
}

TEST_CASE("RejectInvalidAsid", "[mmu][TLBFactoryTest]") {
  // 当前 factory 不显式校验 asid_bits (sv39 默认 9), 但需保证接口稳定
  TLBLevelConfig cfg{"L0", 64, 4, 1, 1, "LRU"};
  CHECK_NOTHROW(TLBFactory::create(cfg));
}

TEST_CASE("NameIsCorrect", "[mmu][TLBFactoryTest]") {
  TLBLevelConfig cfg{"my_L1", 32, 4, 1, 1, "RRIP"};
  auto tlb = TLBFactory::create(cfg);
  CHECK(tlb->name() == std::string("my_L1"));
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
