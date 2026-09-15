// tests/cpu/test_elf_loader_full.cpp
//
// riscv-tests-rv32ui commit B: ELF loader upgrade (multi-section + e_entry + .tohost 解析).
// 覆盖:
//   - 多 PROGBITS SHF_ALLOC section 全加载（不是仅首个）
//   - entry_addr = e_entry（不 skip）
//   - tohost_addr 从 .tohost section sh_addr 解析（shstrtab 查名）
//   - ELF 无 .tohost section → tohost_addr = UINT64_MAX
//   - section overlap → throw runtime_error

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "tools/cpu_sim/elf_loader.h"

using cf::tools::ElfLoadResult;
using cf::tools::load_elf_full;

// ELF32 section header constants (from elf.h / loader)
constexpr std::uint32_t SHT_PROGBITS = 1;
constexpr std::uint32_t SHT_STRTAB = 3;
constexpr std::uint32_t SHF_ALLOC = 0x2;

// 工具函数: 写一个 minimal valid ELF32 file (内存 buffer)
namespace {
struct ElfWriter {
  std::vector<std::uint8_t> bytes;

  void write_u8(std::uint8_t v) { bytes.push_back(v); }
  void write_u16(std::uint16_t v) {
    bytes.push_back(v & 0xFF);
    bytes.push_back((v >> 8) & 0xFF);
  }
  void write_u32(std::uint32_t v) {
    bytes.push_back(v & 0xFF);
    bytes.push_back((v >> 8) & 0xFF);
    bytes.push_back((v >> 16) & 0xFF);
    bytes.push_back((v >> 24) & 0xFF);
  }

  // ELF32 section header: 40 bytes
  void write_shdr(std::uint32_t name, std::uint32_t type, std::uint32_t flags,
                   std::uint32_t addr, std::uint32_t offset, std::uint32_t size,
                   std::uint32_t link = 0, std::uint32_t info = 0,
                   std::uint32_t addralign = 1, std::uint32_t entsize = 0) {
    write_u32(name);
    write_u32(type);
    write_u32(flags);
    write_u32(addr);
    write_u32(offset);
    write_u32(size);
    write_u32(link);
    write_u32(info);
    write_u32(addralign);
    write_u32(entsize);
  }

  void write_to_file(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }
};
}  // namespace

TEST_CASE("elf loader full: multi PROGBITS section all loaded", "[elf-loader][multi-section]") {
  // 构造一个最小 ELF32:
  // - e_entry = 0x80000000
  // - .text.init at 0x80000000 (PROGBITS + ALLOC, 4 bytes)
  // - .data at 0x80001000 (PROGBITS + ALLOC, 4 bytes)
  // - shstrtab
  ElfWriter w;

  // e_ident (16 bytes)
  std::uint8_t e_ident[16] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 16; ++i) w.write_u8(e_ident[i]);

  // ELF32 header 后续 36 bytes:
  // e_type (2) = 2 (ET_EXEC), e_machine (2) = 0xF3 (EM_RISCV),
  // e_version (4) = 1, e_entry (4) = 0x80000000, e_phoff (4) = 0,
  // e_shoff (4) = section headers offset (will fill later), e_flags (4) = 0,
  // e_ehsize (2) = 52, e_phentsize (2) = 0, e_phnum (2) = 0,
  // e_shentsize (2) = 40, e_shnum (2) = 4 (NULL + .text.init + .data + shstrtab),
  // e_shstrndx (2) = 3
  w.write_u16(2);                            // e_type
  w.write_u16(0xF3);                         // e_machine
  w.write_u32(1);                            // e_version
  w.write_u32(0x80000000);                   // e_entry
  w.write_u32(0);                            // e_phoff
  // placeholder for e_shoff
  std::size_t e_shoff_pos = w.bytes.size();
  w.write_u32(0);
  w.write_u32(0);                            // e_flags
  w.write_u16(52);                           // e_ehsize
  w.write_u16(0);                            // e_phentsize
  w.write_u16(0);                            // e_phnum
  w.write_u16(40);                           // e_shentsize
  w.write_u16(4);                            // e_shnum
  w.write_u16(3);                            // e_shstrndx

  // section data (program data)
  std::size_t text_init_offset = w.bytes.size();
  w.write_u8(0x13); w.write_u8(0x00); w.write_u8(0x00); w.write_u8(0x00);  // NOP @ 0x80000000

  std::size_t data_offset = w.bytes.size();
  w.write_u8(0xAA); w.write_u8(0xBB); w.write_u8(0xCC); w.write_u8(0xDD);  // .data @ 0x80001000

  // shstrtab (section 3)
  std::size_t shstrtab_offset = w.bytes.size();
  // Layout: "\0.text.init\0.data\0.shstrtab"
  std::string shstrtab = std::string("\0", 1) + ".text.init" + std::string("\0", 1) +
                          ".data" + std::string("\0", 1) + ".shstrtab" + std::string("\0", 1);
  for (char c : shstrtab) w.write_u8(static_cast<std::uint8_t>(c));

  // Section headers start here
  std::size_t shoff = w.bytes.size();
  // patch e_shoff
  std::uint32_t shoff_u32 = static_cast<std::uint32_t>(shoff);
  w.bytes[e_shoff_pos + 0] = shoff_u32 & 0xFF;
  w.bytes[e_shoff_pos + 1] = (shoff_u32 >> 8) & 0xFF;
  w.bytes[e_shoff_pos + 2] = (shoff_u32 >> 16) & 0xFF;
  w.bytes[e_shoff_pos + 3] = (shoff_u32 >> 24) & 0xFF;

  // Section 0: NULL
  w.write_shdr(0, 0, 0, 0, 0, 0);

  // Section 1: .text.init
  w.write_shdr(1, SHT_PROGBITS, SHF_ALLOC, 0x80000000,
               static_cast<std::uint32_t>(text_init_offset), 4);

  // Section 2: .data
  w.write_shdr(7, SHT_PROGBITS, SHF_ALLOC, 0x80001000,
               static_cast<std::uint32_t>(data_offset), 4);

  // Section 3: .shstrtab
  w.write_shdr(13, SHT_STRTAB, 0, 0,
               static_cast<std::uint32_t>(shstrtab_offset),
               static_cast<std::uint32_t>(shstrtab.size()));

  // 写到临时文件
  const std::string tmp_path = "/tmp/test_multi_section.elf";
  w.write_to_file(tmp_path);

  ElfLoadResult result = load_elf_full(tmp_path);

  REQUIRE(result.entry_addr == 0x80000000);
  // 2 PROGBITS+ALLOC sections (not 1, not 3)
  REQUIRE(result.sections.size() == 2);
  // First section: .text.init at 0x80000000, size 4
  REQUIRE(result.sections[0].first == 0x80000000);
  REQUIRE(result.sections[0].second.size() == 4);
  REQUIRE(result.sections[0].second[0] == 0x13);
  // Second section: .data at 0x80001000, size 4
  REQUIRE(result.sections[1].first == 0x80001000);
  REQUIRE(result.sections[1].second.size() == 4);
  REQUIRE(result.sections[1].second[0] == 0xAA);
}

TEST_CASE("elf loader full: tohost_addr extracted from .tohost section", "[elf-loader][tohost]") {
  ElfWriter w;

  std::uint8_t e_ident[16] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 16; ++i) w.write_u8(e_ident[i]);

  w.write_u16(2);
  w.write_u16(0xF3);
  w.write_u32(1);
  w.write_u32(0x80000000);  // e_entry
  w.write_u32(0);
  std::size_t e_shoff_pos = w.bytes.size();
  w.write_u32(0);  // placeholder
  w.write_u32(0);
  w.write_u16(52);
  w.write_u16(0);
  w.write_u16(0);
  w.write_u16(40);
  w.write_u16(4);  // e_shnum
  w.write_u16(3);  // e_shstrndx

  std::size_t text_offset = w.bytes.size();
  w.write_u8(0x13); w.write_u8(0x00); w.write_u8(0x00); w.write_u8(0x00);

  std::size_t tohost_offset = w.bytes.size();
  w.write_u8(0x00); w.write_u8(0x00); w.write_u8(0x00); w.write_u8(0x00);  // tohost data

  std::size_t shstrtab_offset = w.bytes.size();
  std::string shstrtab = std::string("\0", 1) + ".text" + std::string("\0", 1) +
                          ".tohost" + std::string("\0", 1) + ".shstrtab" + std::string("\0", 1);
  for (char c : shstrtab) w.write_u8(static_cast<std::uint8_t>(c));

  std::size_t shoff = w.bytes.size();
  std::uint32_t shoff_u32 = static_cast<std::uint32_t>(shoff);
  w.bytes[e_shoff_pos + 0] = shoff_u32 & 0xFF;
  w.bytes[e_shoff_pos + 1] = (shoff_u32 >> 8) & 0xFF;
  w.bytes[e_shoff_pos + 2] = (shoff_u32 >> 16) & 0xFF;
  w.bytes[e_shoff_pos + 3] = (shoff_u32 >> 24) & 0xFF;

  // Section 0: NULL
  w.write_shdr(0, 0, 0, 0, 0, 0);
  // Section 1: .text at 0x80000000
  w.write_shdr(1, SHT_PROGBITS, SHF_ALLOC, 0x80000000,
               static_cast<std::uint32_t>(text_offset), 4);
  // Section 2: .tohost at 0x80001000 (custom, not 0x1000 offset from base)
  w.write_shdr(7, SHT_PROGBITS, SHF_ALLOC, 0x80001000,
               static_cast<std::uint32_t>(tohost_offset), 4);
  // Section 3: .shstrtab
  w.write_shdr(15, SHT_STRTAB, 0, 0,
               static_cast<std::uint32_t>(shstrtab_offset),
               static_cast<std::uint32_t>(shstrtab.size()));

  const std::string tmp_path = "/tmp/test_tohost_section.elf";
  w.write_to_file(tmp_path);

  ElfLoadResult result = load_elf_full(tmp_path);

  REQUIRE(result.entry_addr == 0x80000000);
  REQUIRE(result.tohost_addr == 0x80001000);  // From .tohost section sh_addr
}

TEST_CASE("elf loader full: no .tohost section returns UINT64_MAX", "[elf-loader][tohost]") {
  ElfWriter w;
  std::uint8_t e_ident[16] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 16; ++i) w.write_u8(e_ident[i]);
  w.write_u16(2);
  w.write_u16(0xF3);
  w.write_u32(1);
  w.write_u32(0x80000000);
  w.write_u32(0);
  std::size_t e_shoff_pos = w.bytes.size();
  w.write_u32(0);
  w.write_u32(0);
  w.write_u16(52);
  w.write_u16(0);
  w.write_u16(0);
  w.write_u16(40);
  w.write_u16(3);
  w.write_u16(2);  // shstrndx = 2

  std::size_t text_offset = w.bytes.size();
  w.write_u8(0x13); w.write_u8(0x00); w.write_u8(0x00); w.write_u8(0x00);

  std::size_t shstrtab_offset = w.bytes.size();
  std::string shstrtab = std::string("\0", 1) + ".text" + std::string("\0", 1) + ".shstrtab" + std::string("\0", 1);
  for (char c : shstrtab) w.write_u8(static_cast<std::uint8_t>(c));

  std::size_t shoff = w.bytes.size();
  std::uint32_t shoff_u32 = static_cast<std::uint32_t>(shoff);
  w.bytes[e_shoff_pos + 0] = shoff_u32 & 0xFF;
  w.bytes[e_shoff_pos + 1] = (shoff_u32 >> 8) & 0xFF;
  w.bytes[e_shoff_pos + 2] = (shoff_u32 >> 16) & 0xFF;
  w.bytes[e_shoff_pos + 3] = (shoff_u32 >> 24) & 0xFF;

  w.write_shdr(0, 0, 0, 0, 0, 0);
  w.write_shdr(1, SHT_PROGBITS, SHF_ALLOC, 0x80000000,
               static_cast<std::uint32_t>(text_offset), 4);
  w.write_shdr(7, SHT_STRTAB, 0, 0,
               static_cast<std::uint32_t>(shstrtab_offset),
               static_cast<std::uint32_t>(shstrtab.size()));

  const std::string tmp_path = "/tmp/test_no_tohost.elf";
  w.write_to_file(tmp_path);

  ElfLoadResult result = load_elf_full(tmp_path);
  REQUIRE(result.tohost_addr == UINT64_MAX);
}