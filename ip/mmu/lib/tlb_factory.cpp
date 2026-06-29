// ip/mmu/lib/tlb_factory.cpp
//
// 功能描述: TLBFactory::create 工厂实装 (mmu-ip-skeleton, 5.2-5.5)
// 白名单: entries ∈ {8,16,32,64,128,256}, ways ∈ {1,2,4,8}, asid_bits ∈ {0,9,12,16}

#include "ip/mmu/lib/tlb.h"
#include "ip/mmu/lib/tlb_factory.h"
#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

namespace {

// 预定义 ASID_BITS 集合 (编译期枚举)
template <std::size_t ENTRIES, std::size_t WAYS>
std::unique_ptr<TLBBase> create_with_asid(const TLBLevelConfig& cfg) {
  // 当前 skeleton 简化: 所有 entry 用 ASID_BITS=9 (sv39 默认)
  // 后续在 mmu-tlb-ptw-impl 扩展 asid_bits 0/12/16
  constexpr std::size_t TAG_BITS = 27;  // sv39 VPN = 64-12 = 52; high 27 bits as tag
  constexpr std::size_t ASID_BITS = 9;
  constexpr std::size_t PORTS = 1;

  using TLBType = TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>;
  auto tlb = std::make_unique<TLBType>();
  tlb->set_name(cfg.name.c_str());
  tlb->set_policy(TLBReplacementPolicy<ENTRIES, WAYS>::create(cfg.replacement_policy));
  return tlb;
}

}  // namespace

std::unique_ptr<TLBBase> TLBFactory::create(const TLBLevelConfig& cfg) {
  // 白名单: entries × ways
  if (cfg.entries == 8 && cfg.associativity == 1)
    return create_with_asid<8, 1>(cfg);
  if (cfg.entries == 8 && cfg.associativity == 8)
    return create_with_asid<8, 8>(cfg);
  if (cfg.entries == 16 && cfg.associativity == 2)
    return create_with_asid<16, 2>(cfg);
  if (cfg.entries == 16 && cfg.associativity == 16)
    return create_with_asid<16, 16>(cfg);
  if (cfg.entries == 32 && cfg.associativity == 4)
    return create_with_asid<32, 4>(cfg);
  if (cfg.entries == 64 && cfg.associativity == 4)
    return create_with_asid<64, 4>(cfg);
  if (cfg.entries == 128 && cfg.associativity == 4)
    return create_with_asid<128, 4>(cfg);
  if (cfg.entries == 256 && cfg.associativity == 8)
    return create_with_asid<256, 8>(cfg);

  throw std::invalid_argument(
      "TLBFactory::create: entries=" + std::to_string(cfg.entries) +
      " ways=" + std::to_string(cfg.associativity) + " not supported");
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
