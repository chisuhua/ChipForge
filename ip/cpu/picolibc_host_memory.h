// ip/cpu/picolibc_host_memory.h
//
// 功能描述: PicolibcHostMemory — base address window + tohost 退出机制 (M4.10 + riscv-tests-rv32ui)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-15 (riscv-tests-rv32ui v0.2.0)
//
// 设计:
//   - 议题 6 选 C: 绕过 MemoryTLM, 用 64KB 静态 RAM + tohost 机制
//   - riscv-tests-rv32ui v0.2.0: 参数化 base address window 支持 riscv-tests (base=0x80000000)
//                                  + write_half (sh 指令) + write_word per-byte tohost check
//                                  + load_section (多 SHF_ALLOC PROGBITS 段)
//   - tohost 机制: 程序写 1 到 mem[tohost_addr] 表示 PASS, 写其他值 = FAIL
//   - 简单字节可寻址 (byte-addressable)
//
// 借鉴:
//   - picolibc tohost 机制 (https://github.com/picolibc/picolibc)
//   - Spike ISS tohost convention
//   - HTIF protocol (UC Berkeley)
//
// 约束:
//   - 64KB 限制: 手工编译小 ELF (add.S < 1KB) / riscv-tests 单测试 < 8KB
//   - 默认 base=0 兼容 add.elf; base=0x80000000 兼容 riscv-tests env/p
//   - 仅字节读写接口, M5 集成 TLM 事务

#ifndef CF_IP_CPU_PICOLIBC_HOST_MEMORY_H
#define CF_IP_CPU_PICOLIBC_HOST_MEMORY_H

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cf {
namespace cpu {

class PicolibcHostMemory {
 public:
  static constexpr std::size_t kMemorySize = 64 * 1024;  // 64KB (legacy public, kept for back-compat)

  /// 配置参数 (riscv-tests-rv32ui v0.2.0): 参数化 base address window
  /// 默认值 base=0, size=64KB, tohost=0 保持 v0.1.3 add.elf 兼容
  struct Config {
    std::uint64_t base_addr = 0;          // RAM window 起点
    std::uint64_t size = 64 * 1024;       // RAM window 大小 (default 64KB)
    std::uint64_t tohost_addr = 0;        // tohost 绝对地址 (set by ELF loader)
  };

  // 兼容旧代码 default 构造 (base=0, tohost=0)
  PicolibcHostMemory() : PicolibcHostMemory(Config{}) {}

  // 新构造函数 (riscv-tests-rv32ui v0.2.0)
  explicit PicolibcHostMemory(Config cfg) : cfg_(cfg) { reset(); }

  void reset() {
    mem_.fill(0);
    tohost_ = 0;
    exit_code_ = 0;
  }

  /// ELF loader 在解析 .tohost section 后调用, 设置运行时 tohost 绝对地址
  void set_tohost_addr(std::uint64_t abs_addr) { cfg_.tohost_addr = abs_addr; }
  std::uint64_t get_base_addr() const { return cfg_.base_addr; }
  std::uint64_t get_size() const { return cfg_.size; }

  // 字节写 (per-byte tohost check)
  void write_byte(std::uint64_t mem_address, std::uint8_t val) {
    if (!in_window(mem_address)) return;
    std::uint64_t off = mem_address - cfg_.base_addr;
    mem_[off] = val;
    check_tohost(mem_address, val);
  }

  // 字节读
  std::uint8_t read_byte(std::uint64_t mem_address) const {
    if (!in_window(mem_address)) return 0;
    std::uint64_t off = mem_address - cfg_.base_addr;
    return mem_[off];
  }

  // 半字写 (16-bit, little-endian) — riscv-tests-rv32ui v0.2.0 新增
  void write_half(std::uint64_t mem_address, std::uint16_t val) {
    if (!in_window(mem_address)) return;
    if (!in_window(mem_address + 1)) return;
    std::uint64_t off = mem_address - cfg_.base_addr;
    mem_[off + 0] = (val >> 0) & 0xFF;
    mem_[off + 1] = (val >> 8) & 0xFF;
    check_tohost(mem_address, static_cast<std::uint8_t>(val & 0xFF));
    check_tohost(mem_address + 1, static_cast<std::uint8_t>((val >> 8) & 0xFF));
  }

  // 半字读 (16-bit, little-endian)
  std::uint16_t read_half(std::uint64_t mem_address) const {
    if (!in_window(mem_address) || !in_window(mem_address + 1)) return 0;
    std::uint64_t off = mem_address - cfg_.base_addr;
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(mem_[off + 0]) << 0) |
        (static_cast<std::uint16_t>(mem_[off + 1]) << 8));
  }

  // 字写 (32-bit, little-endian, per-byte tohost check — fix partial-byte bug)
  void write_word(std::uint64_t mem_address, std::uint32_t val) {
    if (!in_window(mem_address) || !in_window(mem_address + 3)) return;
    std::uint64_t off = mem_address - cfg_.base_addr;
    mem_[off + 0] = (val >> 0) & 0xFF;
    mem_[off + 1] = (val >> 8) & 0xFF;
    mem_[off + 2] = (val >> 16) & 0xFF;
    mem_[off + 3] = (val >> 24) & 0xFF;
    // riscv-tests-rv32ui v0.2.0: 修复 partial-byte bug, 每个字节独立触发 check_tohost
    check_tohost(mem_address + 0, static_cast<std::uint8_t>((val >> 0) & 0xFF));
    check_tohost(mem_address + 1, static_cast<std::uint8_t>((val >> 8) & 0xFF));
    check_tohost(mem_address + 2, static_cast<std::uint8_t>((val >> 16) & 0xFF));
    check_tohost(mem_address + 3, static_cast<std::uint8_t>((val >> 24) & 0xFF));
  }

  // 字读
  std::uint32_t read_word(std::uint64_t mem_address) const {
    if (!in_window(mem_address) || !in_window(mem_address + 3)) return 0;
    std::uint64_t off = mem_address - cfg_.base_addr;
    return (static_cast<std::uint32_t>(mem_[off + 0]) << 0)
         | (static_cast<std::uint32_t>(mem_[off + 1]) << 8)
         | (static_cast<std::uint32_t>(mem_[off + 2]) << 16)
         | (static_cast<std::uint32_t>(mem_[off + 3]) << 24);
  }

  /// 加载 ELF 单一 PROGBITS 段 (legacy API, kept for back-compat)
  void load_binary(const std::uint8_t* data, std::size_t size, std::uint64_t base = 0) {
    if (!data) return;
    std::vector<std::uint8_t> bytes(data, data + size);
    load_section(base, bytes);
  }

  /// riscv-tests-rv32ui v0.2.0: 多 SHF_ALLOC PROGBITS 段加载 (loader 升级)
  /// sh_addr 是 ELF section 的虚拟地址 (绝对地址, 非 window offset)
  void load_section(std::uint64_t sh_addr, const std::vector<std::uint8_t>& bytes) {
    if (!in_window(sh_addr)) return;
    if (!in_window(sh_addr + bytes.size() - 1)) return;
    std::uint64_t off = sh_addr - cfg_.base_addr;
    std::memcpy(&mem_[off], bytes.data(), bytes.size());
  }

  // 单元测试辅助
  std::uint8_t tohost() const { return tohost_; }
  bool exited() const { return tohost_ != 0; }
  int exit_code() const { return exit_code_; }

  // 直接访问 (测试用)
  const std::array<std::uint8_t, 64 * 1024>& mem() const { return mem_; }

 private:
  Config cfg_;
  std::array<std::uint8_t, kMemorySize> mem_{};
  std::uint8_t tohost_ = 0;
  int exit_code_ = 0;

  /// 窗口检查: underflow 早判 + 双界检查 (cfg_.size 和 mem_ 物理大小)
  /// 注: cfg_.size 是逻辑窗口大小, mem_ 固定 64KB; 取 min 防 OOB
  bool in_window(std::uint64_t abs_addr) const {
    if (abs_addr < cfg_.base_addr) return false;
    std::uint64_t off = abs_addr - cfg_.base_addr;
    std::uint64_t eff_size = (cfg_.size < kMemorySize) ? cfg_.size : kMemorySize;
    if (off >= eff_size) return false;
    return true;
  }

  /// 检测 tohost 写入 (基于运行时 cfg_.tohost_addr 而非 constexpr kTohostAddr)
  void check_tohost(std::uint64_t mem_address, std::uint8_t val) {
    if (mem_address == cfg_.tohost_addr && val != 0) {
      tohost_ = val;
      exit_code_ = (val == 1) ? 0 : 1;  // 1 = PASS, 其他 = FAIL
    }
  }
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PICOLIBC_HOST_MEMORY_H
