// ip/mmu/lib/tlb.h
//
// 功能描述: TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS> 模板化单级 TLB
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - 模板化单级 TLB, 1:1 映射 CppHDL ch::Component
//   - ENTRIES: entry 总数, WAYS: 关联度, TAG_BITS: VPN tag 位宽, ASID_BITS: ASID 位宽, PORTS: 并行查找端口数
//   - 仅 include cf/plugin/uint_t.h (位宽 typedef), 不依赖 Plugin 框架
//   - static_assert 锁死 HDL 友好性
//
// 约束:
//   - lib/ 纯 C++ (D4 范式强制)
//   - 头文件, 无 .cpp (Template 隐式实例化)
//   - 全 std::array, 无 std::vector (在 TLB<> 模板内)
//   - 无 std::optional / std::variant / virtual (基类 TLBBase 是唯一 virtual 入口)

#ifndef CF_IP_MMU_LIB_TLB_H
#define CF_IP_MMU_LIB_TLB_H

#include <array>
#include <cstdint>
#include <memory>

#include "cf/plugin/uint_t.h"
#include "ip/mmu/lib/tlb_base.h"
#include "ip/mmu/lib/tlb_entry.h"
#include "ip/mmu/lib/tlb_lookup.h"
#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES,
          std::size_t WAYS,
          std::size_t TAG_BITS,
          std::size_t ASID_BITS,
          std::size_t PORTS = 1>
class TLB : public TLBBase {
  static_assert(ENTRIES > 0, "ENTRIES must be > 0");
  static_assert(WAYS > 0, "WAYS must be > 0");
  static_assert((ENTRIES % WAYS) == 0, "ENTRIES must be multiple of WAYS");
  static_assert(TAG_BITS > 0 && TAG_BITS <= 52, "TAG_BITS in (0, 52]");
  static_assert(ASID_BITS >= 0 && ASID_BITS <= 16, "ASID_BITS in [0, 16]");
  static_assert(PORTS >= 1 && PORTS <= 8, "PORTS in [1, 8]");

 public:
  using Entry = TLBEntry<TAG_BITS, ASID_BITS>;
  using LookupResult = TLBLookup;

  static constexpr std::size_t kEntries = ENTRIES;
  static constexpr std::size_t kWays = WAYS;
  static constexpr std::size_t kSets = ENTRIES / WAYS;
  static constexpr std::size_t kTagBits = TAG_BITS;
  static constexpr std::size_t kAsidBits = ASID_BITS;
  static constexpr std::size_t kPorts = PORTS;

  TLB() : policy_(nullptr) {}
  virtual ~TLB() = default;

  // 设置替换策略 (骨架阶段: 接受 nullptr, 默认无策略 = 1-way / 全关联)
  void set_policy(std::unique_ptr<class TLBReplacementPolicy<ENTRIES, WAYS>> policy) {
    policy_ = std::move(policy);
  }

  // 设置层级名 (如 "L0" / "L1")
  void set_name(const char* n) { name_ = n; }

  // 查询
  LookupResult lookup(uint64_t vaddr, uint16_t asid) const override {
    const uint64_t vpn = vaddr >> 12;
    const typename Entry::tag_type tag = vpn >> ASID_BITS;  // 简化提取 (实际按 sv39 格式)
    const std::size_t set_idx = vpn % kSets;
    LookupResult result;

    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (!valid_[idx]) continue;
      const Entry& e = entries_[idx];
      if (e.global || e.asid == asid) {
        if (e.tag == tag) {
          result.hit = true;
          result.paddr = (e.pfn << 12) | (vaddr & 0xFFF);
          result.perms = e.perms;
          ++hits_;
          return result;
        }
      }
    }
    ++misses_;
    return result;
  }

  // 写入 (refill / 覆盖)
  void insert(uint64_t vaddr, uint64_t paddr, uint16_t asid, uint8_t perms) override {
    const uint64_t vpn = vaddr >> 12;
    const typename Entry::tag_type tag = vpn >> ASID_BITS;
    const std::size_t set_idx = vpn % kSets;

    // 先检查同 vaddr 是否已存在 (覆盖)
    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (valid_[idx] && entries_[idx].tag == tag && entries_[idx].asid == asid) {
        entries_[idx].pfn = paddr >> 12;
        entries_[idx].perms = perms;
        ++evicts_;  // 覆盖算一次 evict
        return;
      }
    }

    // 找空槽
    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (!valid_[idx]) {
        valid_[idx] = true;
        entries_[idx] = Entry{};
        entries_[idx].valid = true;
        entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);
        entries_[idx].pfn = paddr >> 12;
        entries_[idx].asid = asid;
        entries_[idx].perms = perms;
        return;
      }
    }

    // 选 victim (1-way: way 0; N-way: 调 policy)
    std::size_t victim_way = 0;
    if (policy_) {
      victim_way = policy_->select_victim(static_cast<uint32_t>(set_idx));
    }
    const std::size_t idx = set_idx * WAYS + victim_way;
    valid_[idx] = true;
    entries_[idx] = Entry{};
    entries_[idx].valid = true;
    entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);
    entries_[idx].pfn = paddr >> 12;
    entries_[idx].asid = asid;
    entries_[idx].perms = perms;
    ++evicts_;
  }

  // 写入 (shadow fill, 不污染统计)
  void insert_from(uint64_t vaddr, uint64_t paddr, uint16_t asid, uint8_t perms) override {
    const uint64_t vpn = vaddr >> 12;
    const typename Entry::tag_type tag = vpn >> ASID_BITS;
    const std::size_t set_idx = vpn % kSets;

    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (valid_[idx] && entries_[idx].tag == tag && entries_[idx].asid == asid) {
        entries_[idx].pfn = paddr >> 12;
        entries_[idx].perms = perms;
        return;  // 不计 evict
      }
    }

    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (!valid_[idx]) {
        valid_[idx] = true;
        entries_[idx] = Entry{};
        entries_[idx].valid = true;
        entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);
        entries_[idx].pfn = paddr >> 12;
        entries_[idx].asid = asid;
        entries_[idx].perms = perms;
        return;
      }
    }

    std::size_t victim_way = 0;
    if (policy_) {
      victim_way = policy_->select_victim(static_cast<uint32_t>(set_idx));
    }
    const std::size_t idx = set_idx * WAYS + victim_way;
    valid_[idx] = true;
    entries_[idx] = Entry{};
    entries_[idx].valid = true;
    entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);
    entries_[idx].pfn = paddr >> 12;
    entries_[idx].asid = asid;
    entries_[idx].perms = perms;
  }

  // 失效 (单条)
  void invalidate_vaddr(uint64_t vaddr, uint16_t asid) override {
    const uint64_t vpn = vaddr >> 12;
    const typename Entry::tag_type tag = vpn >> ASID_BITS;
    const std::size_t set_idx = vpn % kSets;
    for (std::size_t way = 0; way < WAYS; ++way) {
      const std::size_t idx = set_idx * WAYS + way;
      if (valid_[idx] && entries_[idx].tag == tag && (entries_[idx].global || entries_[idx].asid == asid)) {
        valid_[idx] = false;
      }
    }
  }

  // 失效 (整 ASID)
  void invalidate_asid(uint16_t asid) override {
    for (std::size_t idx = 0; idx < ENTRIES; ++idx) {
      if (valid_[idx] && !entries_[idx].global && entries_[idx].asid == asid) {
        valid_[idx] = false;
      }
    }
  }

  // 失效 (全清)
  void invalidate_all() override {
    for (std::size_t idx = 0; idx < ENTRIES; ++idx) {
      valid_[idx] = false;
    }
  }

  uint64_t hit_count() const override { return hits_; }
  uint64_t miss_count() const override { return misses_; }
  uint64_t evict_count() const override { return evicts_; }
  const char* name() const override { return name_; }

 private:
  std::array<Entry, ENTRIES> entries_{};
  std::array<cf::plugin::bool_t, ENTRIES> valid_{};
  std::unique_ptr<class TLBReplacementPolicy<ENTRIES, WAYS>> policy_;
  const char* name_ = "L?";

  uint64_t hits_ = 0;
  uint64_t misses_ = 0;
  uint64_t evicts_ = 0;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_H
