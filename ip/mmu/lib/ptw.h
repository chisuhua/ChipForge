// ip/mmu/lib/ptw.h
//
// 功能描述: PageTableWalker 接口 (mmu-ip-skeleton, 7.1-7.7)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 关键设计 (D4 强制):
//   - PTW 不派生 PluginBase, 不调 at_stage, 不持 PipeNode
//   - MMUPlugin 在 at_stage 闭包内调 PTW::advance() 推进 (1 步 / stage)
//   - PTW 自身无 tick(), 状态用成员变量, 推进由外部驱动
//   - lib/ 纯 C++

#ifndef CF_IP_MMU_LIB_PTW_H
#define CF_IP_MMU_LIB_PTW_H

#include <cstddef>
#include <cstdint>
#include <functional>

namespace cf {
namespace ip {
namespace mmu {

enum class SvMode : std::uint8_t {
  Bare = 0,
  Sv32 = 1,
  Sv39 = 2,
  Sv48 = 3,
};

// PTE 基础结构 (RISC-V 通用; Sv32/Sv39/Sv48 PPN 长度不同, 统一用 uint64)
struct PTE {
  std::uint64_t raw = 0;  // 原始 64-bit PTE
  bool v = false;         // Valid
  bool r = false;         // Readable
  bool w = false;         // Writable
  bool x = false;         // Executable
  bool u = false;         // User
  bool g = false;         // Global
  bool a = false;         // Accessed
  bool d = false;         // Dirty
  std::uint8_t rsw = 0;   // Reserved for SW
  std::uint64_t ppn = 0; // Physical Page Number
  std::size_t ppn_bits = 0;  // PPN 长度 (Sv32=10, Sv39=44, Sv48=52)
};

// PageTableWalker 接口 (骨架阶段: 状态机 + 回调驱动)
class PTW {
 public:
  using WalkCallback = std::function<void(uint64_t paddr, uint8_t perms)>;
  using FaultCallback = std::function<void(uint8_t fault_code)>;

  PTW();
  explicit PTW(SvMode mode, std::size_t max_inflight = 2);

  void start_walk(uint64_t vaddr, uint16_t asid,
                  WalkCallback on_success, FaultCallback on_fault);

  // 由 MMUPlugin 在 at_stage 闭包内调用, 推进 1 步 walk
  // (读 1 个 PTE 后调用; PTW 决定是否需要继续)
  void advance(std::uint64_t pte_raw, std::size_t level);

  bool is_busy() const { return busy_; }
  bool is_done() const { return done_; }
  std::uint64_t result_paddr() const { return result_paddr_; }
  std::uint8_t result_perms() const { return result_perms_; }
  std::uint8_t result_fault() const { return result_fault_; }
  std::size_t current_level() const { return current_level_; }

  SvMode mode() const { return mode_; }
  std::size_t max_levels() const;

 private:
  SvMode mode_;
  std::size_t max_inflight_;
  std::size_t max_levels_;

  // walk 状态
  bool busy_ = false;
  bool done_ = false;
  std::size_t current_level_ = 0;
  std::uint64_t vaddr_ = 0;
  std::uint16_t asid_ = 0;
  std::uint64_t current_pte_paddr_ = 0;

  // 结果
  std::uint64_t result_paddr_ = 0;
  std::uint8_t result_perms_ = 0;
  std::uint8_t result_fault_ = 0;

  // 回调
  WalkCallback on_success_;
  FaultCallback on_fault_;

  // PTE 解码 stub (mmu-tlb-ptw-impl 实施)
  static PTE decode_pte(std::uint64_t raw, SvMode mode);

  // 计算下一级 PTE 地址 (stub)
  std::uint64_t next_pte_paddr(std::uint64_t pte_ppn, std::size_t level) const;
};

// SvMode 字符串化 (for logging)
inline const char* sv_mode_name(SvMode m) {
  switch (m) {
    case SvMode::Bare: return "Bare";
    case SvMode::Sv32: return "Sv32";
    case SvMode::Sv39: return "Sv39";
    case SvMode::Sv48: return "Sv48";
  }
  return "?";
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_PTW_H
