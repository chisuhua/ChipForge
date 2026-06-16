// ip/cpu/picolibc_host_memory.h
//
// 功能描述: PicolibcHostMemory — 64KB 静态 RAM (M4.10)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 议题 6 选 C: 绕过 MemoryTLM, 用 64KB 静态 RAM + picolibc tohost 机制
//   - 64KB 内存: 0x0000 - 0xFFFF
//   - tohost 机制: 程序写 1 到 mem[0] 表示 PASS, 仿真器检测后退出
//   - 简单字节可寻址 (byte-mem_addressessable)
//
// 借鉴:
//   - picolibc tohost 机制 (https://github.com/picolibc/picolibc)
//   - Spike ISS tohost convention
//
// 约束:
//   - 64KB 限制: 手工编译小 ELF (add.S < 1KB)
//   - 仅字节读写接口, M5 集成 TLM 事务

#ifndef CF_IP_CPU_PICOLIBC_HOST_MEMORY_H
#define CF_IP_CPU_PICOLIBC_HOST_MEMORY_H

#include <array>
#include <cstdint>
#include <cstring>

namespace cf {
namespace cpu {

class PicolibcHostMemory {
 public:
  static constexpr std::size_t kMemorySize = 64 * 1024;  // 64KB
  static constexpr std::uint64_t kTohostAddr = 0x0;       // tohost 在地址 0

  PicolibcHostMemory() { reset(); }

  void reset() {
    mem_.fill(0);
    tohost_ = 0;
    exit_code_ = 0;
  }

  // 字节写
  void write_byte(std::uint64_t mem_address, std::uint8_t val) {
    if (mem_address < kMemorySize) {
      mem_[mem_address] = val;
      check_tohost(mem_address, val);
    }
  }

  // 字节读
  std::uint8_t read_byte(std::uint64_t mem_address) const {
    if (mem_address < kMemorySize) return mem_[mem_address];
    return 0;
  }

  // 字写 (32-bit, little-endian)
  void write_word(std::uint64_t mem_address, std::uint32_t val) {
    if (mem_address + 3 < kMemorySize) {
      mem_[mem_address + 0] = (val >> 0) & 0xFF;
      mem_[mem_address + 1] = (val >> 8) & 0xFF;
      mem_[mem_address + 2] = (val >> 16) & 0xFF;
      mem_[mem_address + 3] = (val >> 24) & 0xFF;
      check_tohost(mem_address, val & 0xFF);
    }
  }

  // 字读
  std::uint32_t read_word(std::uint64_t mem_address) const {
    if (mem_address + 3 < kMemorySize) {
      return (std::uint32_t)mem_[mem_address + 0]
           | ((std::uint32_t)mem_[mem_address + 1] << 8)
           | ((std::uint32_t)mem_[mem_address + 2] << 16)
           | ((std::uint32_t)mem_[mem_address + 3] << 24);
    }
    return 0;
  }

  // 加载 ELF (简化: 假设 ELF 格式已知, 直接复制 .text 段)
  // M4 stub: 当前仅支持 raw binary 加载到 base
  void load_binary(const std::uint8_t* data, std::size_t size, std::uint64_t base = 0) {
    if (base + size <= kMemorySize) {
      std::memcpy(&mem_[base], data, size);
    }
  }

  // 单元测试辅助
  std::uint8_t tohost() const { return tohost_; }
  bool exited() const { return tohost_ != 0; }
  int exit_code() const { return exit_code_; }

  // 直接访问 (测试用)
  const std::array<std::uint8_t, kMemorySize>& mem() const { return mem_; }

 private:
  std::array<std::uint8_t, kMemorySize> mem_{};
  std::uint8_t tohost_ = 0;
  int exit_code_ = 0;

  // 检测 tohost 写入
  void check_tohost(std::uint64_t mem_address, std::uint8_t val) {
    if (mem_address == kTohostAddr && val != 0) {
      tohost_ = val;
      exit_code_ = (val == 1) ? 0 : 1;  // 1 = PASS, 其他 = FAIL
    }
  }
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PICOLIBC_HOST_MEMORY_H
