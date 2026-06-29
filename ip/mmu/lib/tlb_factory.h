// ip/mmu/lib/tlb_factory.h
//
// 功能描述: TLBFactory —— 按 JSON config 选模板特化 (mmu-ip-skeleton, 5.1-5.5)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29

#ifndef CF_IP_MMU_LIB_TLB_FACTORY_H
#define CF_IP_MMU_LIB_TLB_FACTORY_H

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#include "ip/mmu/lib/tlb_base.h"

namespace cf {
namespace ip {
namespace mmu {

struct TLBLevelConfig {
  std::string name = "L0";
  std::size_t entries = 64;
  std::size_t associativity = 4;
  std::size_t num_lookup_ports = 1;
  std::size_t lookup_latency_cycles = 1;
  std::string replacement_policy = "LRU";
};

class TLBFactory {
 public:
  static std::unique_ptr<TLBBase> create(const TLBLevelConfig& cfg);
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_FACTORY_H
