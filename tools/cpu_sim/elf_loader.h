// tools/cpu_sim/elf_loader.h
//
// 功能描述: Minimal RV32 ELF32 parser for cpu_sim (M4.15)
//   - Parses ELF32 header + section headers
//   - Returns .text section bytes + load base address
//   - Used by cpu_sim to populate PicolibcHostMemory
//
// 约束 (M4.15 修订范围):
//   - 仅支持 ELF32 little-endian (RV32 only)
//   - 取第一个 PROGBITS section 作为代码段
//   - 不解析 program headers / relocations (picolibc static link 已处理)
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-23

#ifndef CF_TOOLS_CPU_SIM_ELF_LOADER_H
#define CF_TOOLS_CPU_SIM_ELF_LOADER_H

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace cf {
namespace tools {

/// riscv-tests-rv32ui v0.2.0: ELF loader upgrade result
struct ElfLoadResult {
  std::vector<std::pair<std::uint64_t, std::vector<std::uint8_t>>>
      sections;                        // (sh_addr, bytes) — 多 SHF_ALLOC PROGBITS 段
  std::uint64_t entry_addr = 0;        // e_entry (不再 skip)
  std::uint64_t tohost_addr = std::numeric_limits<std::uint64_t>::max();  // UINT64_MAX if absent
};

// 解析 ELF32 文件, 提取第一个 PROGBITS section 的字节和加载地址
// - path:     ELF 文件路径
// - base_addr: 输出参数, section 的 sh_addr (PicolibcHostMemory 加载基址)
// 返回: section 的字节内容
inline std::vector<std::uint8_t> load_elf_text(const std::string& path,
                                                std::uint64_t& base_addr) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("Cannot open ELF: " + path);
  }

  // ELF32 header: e_ident[16] = 16 bytes
  char e_ident[16];
  f.read(e_ident, 16);
  if (f.gcount() != 16 ||
      e_ident[0] != 0x7F || e_ident[1] != 'E' ||
      e_ident[2] != 'L'  || e_ident[3] != 'F') {
    throw std::runtime_error("Not an ELF file: " + path);
  }
  // 仅支持 ELF32 (RV32)
  if (static_cast<unsigned char>(e_ident[4]) != 1) {
    throw std::runtime_error("Not ELF32 (only RV32 supported): " + path);
  }
  // 仅支持 little-endian (RISC-V 规范)
  if (static_cast<unsigned char>(e_ident[5]) != 1) {
    throw std::runtime_error("Not little-endian ELF: " + path);
  }

  // ELF32 header 字段 (跳过 e_ident 后)
  // e_type (2), e_machine (2), e_version (4), e_entry (4), e_phoff (4),
  // e_shoff (4), e_flags (4), e_ehsize (2), e_phentsize (2), e_phnum (2),
  // e_shentsize (2), e_shnum (2), e_shstrndx (2)
  std::uint32_t e_shoff = 0;
  std::uint16_t e_shentsize = 0;
  std::uint16_t e_shnum = 0;
  // skip e_type, e_machine, e_version, e_entry, e_phoff (2+2+4+4+4 = 16 bytes)
  f.seekg(16, std::ios::cur);
  // e_shoff (4)
  f.read(reinterpret_cast<char*>(&e_shoff), 4);
  // skip e_flags, e_ehsize, e_phentsize, e_phnum (4+2+2+2 = 10 bytes)
  f.seekg(10, std::ios::cur);
  // e_shentsize (2)
  f.read(reinterpret_cast<char*>(&e_shentsize), 2);
  // e_shnum (2)
  f.read(reinterpret_cast<char*>(&e_shnum), 2);

  if (!f) {
    throw std::runtime_error("Truncated ELF header: " + path);
  }

  // 遍历 section headers, 找第一个 PROGBITS
  std::vector<std::uint8_t> text_bytes;
  base_addr = 0;
  for (std::uint16_t i = 0; i < e_shnum; ++i) {
    f.seekg(static_cast<std::streamoff>(e_shoff) +
            static_cast<std::streamoff>(i) * e_shentsize);
    std::uint32_t sh_type = 0, sh_addr = 0;
    std::uint32_t sh_offset = 0, sh_size = 0;
    // skip sh_name (4), read sh_type (4)
    f.seekg(4, std::ios::cur);
    f.read(reinterpret_cast<char*>(&sh_type), 4);
    // skip sh_flags (4)
    f.seekg(4, std::ios::cur);
    // sh_addr (4)
    f.read(reinterpret_cast<char*>(&sh_addr), 4);
    // sh_offset (4)
    f.read(reinterpret_cast<char*>(&sh_offset), 4);
    // sh_size (4)
    f.read(reinterpret_cast<char*>(&sh_size), 4);

    // SHT_PROGBITS = 1
    if (sh_type == 1 && sh_size > 0) {
      base_addr = sh_addr;
      text_bytes.resize(sh_size);
      f.seekg(sh_offset);
      f.read(reinterpret_cast<char*>(text_bytes.data()), sh_size);
      break;
    }
  }

  if (text_bytes.empty()) {
    throw std::runtime_error("No PROGBITS section found in ELF: " + path);
  }

  return text_bytes;
}

/// riscv-tests-rv32ui v0.2.0: 多 SHF_ALLOC PROGBITS 段加载 + e_entry + .tohost 解析.
/// 返回 ElfLoadResult; 调用方需处理 section overlap (本函数内部检测 + throw).
inline ElfLoadResult load_elf_full(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("Cannot open ELF: " + path);
  }

  char e_ident[16];
  f.read(e_ident, 16);
  if (f.gcount() != 16 ||
      e_ident[0] != 0x7F || e_ident[1] != 'E' ||
      e_ident[2] != 'L'  || e_ident[3] != 'F') {
    throw std::runtime_error("Not an ELF file: " + path);
  }
  if (static_cast<unsigned char>(e_ident[4]) != 1) {
    throw std::runtime_error("Not ELF32 (only RV32 supported): " + path);
  }
  if (static_cast<unsigned char>(e_ident[5]) != 1) {
    throw std::runtime_error("Not little-endian ELF: " + path);
  }

  std::uint32_t e_entry = 0;
  std::uint32_t e_shoff = 0;
  std::uint16_t e_shentsize = 0;
  std::uint16_t e_shnum = 0;
  std::uint16_t e_shstrndx = 0;
  // skip e_type, e_machine, e_version (2+2+4 = 8 bytes)
  f.seekg(8, std::ios::cur);
  // e_entry (4) — 不再 skip
  f.read(reinterpret_cast<char*>(&e_entry), 4);
  // skip e_phoff (4)
  f.seekg(4, std::ios::cur);
  // e_shoff (4)
  f.read(reinterpret_cast<char*>(&e_shoff), 4);
  // skip e_flags, e_ehsize, e_phentsize, e_phnum (4+2+2+2 = 10 bytes)
  f.seekg(10, std::ios::cur);
  // e_shentsize (2)
  f.read(reinterpret_cast<char*>(&e_shentsize), 2);
  // e_shnum (2)
  f.read(reinterpret_cast<char*>(&e_shnum), 2);
  // e_shstrndx (2)
  f.read(reinterpret_cast<char*>(&e_shstrndx), 2);

  if (!f) {
    throw std::runtime_error("Truncated ELF header: " + path);
  }

  constexpr std::uint32_t SHT_PROGBITS = 1;
  constexpr std::uint32_t SHT_STRTAB = 3;
  constexpr std::uint32_t SHF_ALLOC = 0x2;

  // 读取所有 section headers
  struct SectionInfo {
    std::uint32_t sh_name = 0;
    std::uint32_t sh_type = 0;
    std::uint32_t sh_flags = 0;
    std::uint32_t sh_addr = 0;
    std::uint32_t sh_offset = 0;
    std::uint32_t sh_size = 0;
  };
  std::vector<SectionInfo> sections;
  sections.reserve(e_shnum);
  for (std::uint16_t i = 0; i < e_shnum; ++i) {
    f.seekg(static_cast<std::streamoff>(e_shoff) +
            static_cast<std::streamoff>(i) * e_shentsize);
    SectionInfo s;
    f.read(reinterpret_cast<char*>(&s.sh_name), 4);
    f.read(reinterpret_cast<char*>(&s.sh_type), 4);
    f.read(reinterpret_cast<char*>(&s.sh_flags), 4);
    f.read(reinterpret_cast<char*>(&s.sh_addr), 4);
    f.read(reinterpret_cast<char*>(&s.sh_offset), 4);
    f.read(reinterpret_cast<char*>(&s.sh_size), 4);
    sections.push_back(s);
  }

  // 读 shstrtab (解析 section name)
  std::vector<char> shstrtab;
  if (e_shstrndx < sections.size()) {
    const auto& strtab = sections[e_shstrndx];
    if (strtab.sh_type == SHT_STRTAB && strtab.sh_size > 0) {
      shstrtab.resize(strtab.sh_size);
      f.seekg(strtab.sh_offset);
      f.read(shstrtab.data(), strtab.sh_size);
    }
  }
  auto section_name = [&](std::uint32_t name_offset) -> std::string {
    if (name_offset >= shstrtab.size()) return {};
    const char* p = shstrtab.data() + name_offset;
    std::size_t max_len = shstrtab.size() - name_offset;
    std::size_t len = 0;
    while (len < max_len && p[len] != '\0') ++len;
    return std::string(p, len);
  };

  // 收集所有 SHF_ALLOC PROGBITS 段 + 找 .tohost
  ElfLoadResult result;
  result.entry_addr = e_entry;

  std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;  // [addr, end) for overlap check

  for (const auto& s : sections) {
    if (s.sh_type == SHT_PROGBITS && (s.sh_flags & SHF_ALLOC)) {
      std::vector<std::uint8_t> bytes(s.sh_size);
      if (s.sh_size > 0) {
        f.seekg(s.sh_offset);
        f.read(reinterpret_cast<char*>(bytes.data()), s.sh_size);
      }
      std::uint64_t start = s.sh_addr;
      std::uint64_t end = s.sh_addr + s.sh_size;
      // overlap detection
      for (const auto& r : ranges) {
        if (!(end <= r.first || start >= r.second)) {
          throw std::runtime_error("ELF section overlap: [" +
                                   std::to_string(start) + "," +
                                   std::to_string(end) + ") vs [" +
                                   std::to_string(r.first) + "," +
                                   std::to_string(r.second) + ")");
        }
      }
      ranges.emplace_back(start, end);
      result.sections.emplace_back(start, std::move(bytes));
    }
    // 找名为 ".tohost" 的 section
    if (s.sh_type == SHT_PROGBITS && section_name(s.sh_name) == ".tohost") {
      result.tohost_addr = s.sh_addr;
    }
  }

  if (result.sections.empty()) {
    throw std::runtime_error("No PROGBITS SHF_ALLOC section found in ELF: " + path);
  }

  return result;
}

}  // namespace tools
}  // namespace cf

#endif  // CF_TOOLS_CPU_SIM_ELF_LOADER_H