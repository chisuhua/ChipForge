// ip/mmu/policies/tlb_replacement_policy.cpp
//
// 功能描述: TLBReplacementPolicy::create 工厂实装 (mmu-ip-skeleton, 4.6)

#include <memory>
#include <stdexcept>
#include <string>

#include "ip/mmu/policies/fifo_policy.h"
#include "ip/mmu/policies/lru_policy.h"
#include "ip/mmu/policies/no_replacement_policy.h"
#include "ip/mmu/policies/rrip_policy.h"
#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
std::unique_ptr<TLBReplacementPolicy<ENTRIES, WAYS>>
TLBReplacementPolicy<ENTRIES, WAYS>::create(const std::string& name) {
  if (name == "None") {
    return std::make_unique<NoReplacementPolicy<ENTRIES, WAYS>>();
  } else if (name == "FIFO") {
    return std::make_unique<FIFOPolicy<ENTRIES, WAYS>>();
  } else if (name == "LRU") {
    return std::make_unique<LRUPolicy<ENTRIES, WAYS>>();
  } else if (name == "RRIP") {
    return std::make_unique<RRIPPolicy<ENTRIES, WAYS>>();
  }
  throw std::runtime_error("TLBReplacementPolicy::create: unknown name '" + name + "'");
}

// 显式实例化 (避免链接器丢符号)
template class TLBReplacementPolicy<8, 1>;
template class TLBReplacementPolicy<16, 2>;
template class TLBReplacementPolicy<32, 4>;
template class TLBReplacementPolicy<64, 4>;
template class TLBReplacementPolicy<128, 4>;
template class TLBReplacementPolicy<256, 8>;
template class TLBReplacementPolicy<8, 8>;
template class TLBReplacementPolicy<16, 16>;

}  // namespace mmu
}  // namespace ip
}  // namespace cf
