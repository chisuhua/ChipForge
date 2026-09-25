// ip/mmu/lib/memory_interface.h
//
// 功能描述: PTW 真内存读抽象 (ADR-049 D2/D3, P1#3 mmu-paddr-consume-and-real-memory task 3.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-25
//
// 设计:
//   - 抽象类: read_word / write_word 最小接口 (PTW 32-bit PTE 粒度)
//   - lib/ 层 purity: 纯 C++, 零 cf::plugin::* 依赖 (与 ptw.h 同层)
//   - 解耦 PTW 与具体内存实现: PicolibcHostMemory 继承, 未来可注入 L2Cache / DRAM / mmap
//   - 默认 PicolibcHostMemory 64KB RAM 窗口, base=0x80000000 (riscv-tests env/p)
//
// 关联:
//   - ADR-049 D2: MemoryInterface 抽象类 (本文件)
//   - ADR-049 D3: PicolibcHostMemory 继承实现
//   - ip/mmu/lib/ptw.h::advance_from_real_memory(MemoryInterface*) 消费者
//   - P1#3 tasks.md §3.1 (MemoryInterface 抽象)
//   - P1#3 tasks.md §3.2 (PicolibcHostMemory 继承)
//   - P1#6 mmu-config-json-driven ADR-049 §后续项 (配置驱动层)

#ifndef CF_IP_MMU_LIB_MEMORY_INTERFACE_H
#define CF_IP_MMU_LIB_MEMORY_INTERFACE_H

#include <cstdint>

namespace cf {
namespace ip {
namespace mmu {

/// 内存访问抽象 (PTW 真内存读消费者端)
///
/// 解耦 PTW 与具体内存实现。生产环境由 `PicolibcHostMemory` 继承实现
/// (`ip/cpu/picolibc_host_memory.h`)。测试可注入 mock 实现。
///
/// 约束:
///   - lib/ 层 purity: 不依赖 `cf::plugin::*`, 保证 Phase 5 CppHDL 转换零阻力
///   - 接口最小化: 仅 32-bit 粒度读写 (PTW PTE 粒度足够)
///   - read_word 非 const: 允许未来加读统计 (例如 MMIO 副作用检测)
///   - write_word 非 const: 写入可能触发副作用 (tohost 检测等)
///
/// ABI 稳定性: 这是新接口, 不破坏现有代码。`PicolibcHostMemory::read_word` 移除
/// `const` 限定后, 既有 4 callers (dbus.h:84, ibus.h:76, test_picolibc_memory_base_window.cpp:35/84)
/// 通过非 const 指针调用, 兼容性已验证 (commit `5763011` design.md §Open Questions Q3 验证)。
class MemoryInterface {
 public:
  virtual ~MemoryInterface() = default;

  /// 读 32-bit 字 (little-endian)
  /// @param phys_addr 绝对物理地址 (非相对窗口偏移)
  /// @return 读取的字; 地址越界时返回 0 (与 PicolibcHostMemory::read_word 行为一致)
  virtual std::uint32_t read_word(std::uint64_t phys_addr) = 0;

  /// 写 32-bit 字 (little-endian)
  /// @param phys_addr 绝对物理地址
  /// @param val 要写入的字
  /// @return 无返回值; 地址越界时 no-op (与 PicolibcHostMemory::write_word 行为一致)
  virtual void write_word(std::uint64_t phys_addr, std::uint32_t val) = 0;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_MEMORY_INTERFACE_H