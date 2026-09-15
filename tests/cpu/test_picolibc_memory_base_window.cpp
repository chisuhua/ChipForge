// tests/framework/test_picolibc_memory_base_window.cpp
//
// riscv-tests-rv32ui commit A: PicolibcHostMemory base address window + write_half +
//                              write_word per-byte tohost check + load_section.
// 覆盖:
//   - default base=0 保持 add.elf 兼容（tohost=0, exit_code=0）
//   - base=0x80000000 实例接受读写（tohost=0x80001000 触发出退出）
//   - write_half 写 2 字节 little-endian
//   - write_word 每个字节独立触发 check_tohost（修复 partial-byte bug）
//   - base=0x80000000 时写 0x0 静默 drop（underflow guard）
//   - load_section 多段加载（无重叠，size 正确）

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <vector>

#include "ip/cpu/picolibc_host_memory.h"

using cf::cpu::PicolibcHostMemory;

TEST_CASE("picolibc base=0 default preserves add.elf behavior", "[picolibc][base-window]") {
  PicolibcHostMemory mem;
  // write_word(0, 1) is the add.elf exit signal — must fire
  mem.write_word(0, 1);
  REQUIRE(mem.exited());
  REQUIRE(mem.exit_code() == 0);
  REQUIRE(mem.tohost() == 1);
}

TEST_CASE("picolibc base=0x80000000 accepts riscv-tests memory layout", "[picolibc][base-window]") {
  PicolibcHostMemory mem({.base_addr = 0x80000000, .size = 64 * 1024, .tohost_addr = 0x80001000});
  // Write first instruction (NOP = 0x00000013) to ELF base
  mem.write_word(0x80000000, 0x00000013);
  REQUIRE(mem.read_word(0x80000000) == 0x00000013);
  // write_word to tohost_addr triggers exit
  mem.write_word(0x80001000, 1);
  REQUIRE(mem.exited());
  REQUIRE(mem.exit_code() == 0);
}

TEST_CASE("picolibc write_half writes 2 bytes little-endian", "[picolibc][write_half]") {
  PicolibcHostMemory mem;
  mem.write_half(0x100, 0xCAFE);
  REQUIRE(mem.read_byte(0x100) == 0xFE);
  REQUIRE(mem.read_byte(0x101) == 0xCA);
  // Adjacent bytes unchanged
  REQUIRE(mem.read_byte(0x102) == 0x00);
  REQUIRE(mem.read_byte(0x0FF) == 0x00);
}

TEST_CASE("picolibc write_word per-byte tohost check (non-zero low byte)", "[picolibc][write_word]") {
  PicolibcHostMemory mem({.base_addr = 0x80000000, .size = 64 * 1024, .tohost_addr = 0x80001000});
  // write_word 0xCAFEBABE at tohost addr — low byte = 0xBE != 1 → FAIL (exit_code 1)
  mem.write_word(0x80001000, 0xCAFEBABE);
  REQUIRE(mem.exited());
  REQUIRE(mem.exit_code() == 1);
}

TEST_CASE("picolibc write_half at tohost triggers exit (PASS)", "[picolibc][write_half][tohost]") {
  PicolibcHostMemory mem({.base_addr = 0x80000000, .size = 64 * 1024, .tohost_addr = 0x80001000});
  mem.write_half(0x80001000, 1);
  REQUIRE(mem.exited());
  REQUIRE(mem.exit_code() == 0);
}

TEST_CASE("picolibc out-of-window write silently dropped", "[picolibc][base-window]") {
  PicolibcHostMemory mem({.base_addr = 0x80000000, .size = 64 * 1024, .tohost_addr = 0x80001000});
  // write to 0x0 is out-of-window for base=0x80000000 → dropped silently
  mem.write_word(0x0, 1);
  REQUIRE_FALSE(mem.exited());
  // base+size boundary also dropped
  mem.write_word(0x80010000, 1);
  REQUIRE_FALSE(mem.exited());
  // in-window write at tohost still works
  mem.write_word(0x80001000, 1);
  REQUIRE(mem.exited());
}

TEST_CASE("picolibc load_section loads at sh_addr", "[picolibc][load_section]") {
  PicolibcHostMemory mem({.base_addr = 0x80000000, .size = 64 * 1024, .tohost_addr = 0x80001000});
  std::vector<std::uint8_t> bytes = {0x13, 0x00, 0x00, 0x00};  // NOP
  mem.load_section(0x80000000, bytes);
  REQUIRE(mem.read_word(0x80000000) == 0x00000013);
  // Section at different offset
  std::vector<std::uint8_t> bytes2 = {0xAA, 0xBB};
  mem.load_section(0x80001200, bytes2);
  REQUIRE(mem.read_byte(0x80001200) == 0xAA);
  REQUIRE(mem.read_byte(0x80001201) == 0xBB);
}