// ip/cache/tlm/cache_keys.h
//
// 功能描述: L1Cache Plugin Payload<T> Key 集合 (mmu-cache-integration commit 1/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 设计:
//   - 集中声明 L1CachePlugin 用 Payload Key (避免分散在 .cpp namespace { })
//   - g_vaddr: VIPT 索引来源 (mirror mmu_keys::MMU_VADDR, 但作为独立 Key 对象)
//     L1Cache 读 mmu_keys::MMU_VADDR 是 ADR-044 §3.2 数据流方案 A 的核心契约
//     g_vaddr 是预留的 L1Cache 内部 vaddr (若未来 L1Cache 需要独立 vaddr 跟踪, 不与 MMU 共享)
//   - vipt_fallback: 配置 Knob (enum), 默认 "auto" 静默回退 PIPT
//   - 向后兼容: 不动现有 g_addr/g_idx/g_tag (留在 L1CachePlugin.cpp file-scope)

#ifndef CF_IP_CACHE_TLM_CACHE_KEYS_H
#define CF_IP_CACHE_TLM_CACHE_KEYS_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace ip {
namespace cache {
namespace tlm {
namespace payload {

// VIPT fallback 行为配置 Knob (mmu-cache-integration commit 1)
// 默认 "auto": 无 MMU_VADDR 时静默回退 PIPT (与 mmu-tlb-ptw-impl 前 baseline 行为一致)
// "panic": 期望 MMU_VADDR 存在, 缺失时报错 (Phase 1.5 升 4-way 强制启用)
// "paddr_only": 永远只用 g_addr 做 idx (强制 PIPT, 调试用)
enum class ViptFallback : std::uint8_t {
  Auto = 0,         // 静默回退 (默认; 21 baseline [cache] 测试兼容)
  PaddrOnly = 1,    // 强制 PIPT (调试, 跳过 VIPT 路径)
  Panic = 2,        // 缺失 MMU_VADDR 时抛异常 (Phase 1.5 强制)
};

template <typename T = std::uint64_t>
struct cache_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  // VIPT 索引来源 (mmu-cache-integration commit 1 新增)
  // L1Cache 在 lookup 闭包读 mmu_keys::MMU_VADDR (ADR-044 §3.2 数据流方案 A),
  // g_vaddr 留作 L1Cache 内部独立 vaddr 跟踪的扩展点.
  static inline cf::plugin::Payload<T> g_vaddr{"l1cache.vaddr"};

  // VIPT fallback 配置 Knob (enum Value stored as uint8_t)
  static inline cf::plugin::Payload<std::uint8_t> vipt_fallback{
      "l1cache.vipt_fallback"};
};

}  // namespace payload
}  // namespace tlm
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_TLM_CACHE_KEYS_H
