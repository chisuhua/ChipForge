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
#include <stdexcept>
#include <string>
#include <vector>

namespace cf {
namespace tools {

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

}  // namespace tools
}  // namespace cf

#endif  // CF_TOOLS_CPU_SIM_ELF_LOADER_H