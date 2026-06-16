// ip/cpu/arch/riscv/payload_riscv.h
//
// 功能描述: RISC-V ISA 特有 Payload Key + DecodeDetail 结构 (M3.2, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 补充 payload_common.h 的 ISA 特有字段 (RISC-V 32I/M/Zicsr/Zifencei)
//   - RiscvDecodeDetail 包含 funct3/funct7/imm 等 RISC-V 特有字段
//   - 通用字段 (rs1_idx/rs2_idx/rd_idx/op_class) 仍在 payload_common.h
//   - Payload Key: RISCV_DETAIL (RiscvDecodeDetail struct)
//
// 借鉴:
//   - VexRiscv Decode.scala (funct3/funct7 字段分离)
//   - Spike ISS (decode 阶段输出结构)
//
// 约束:
//   - 头文件为主 (无 .cpp, 跨翻译单元共享)
//   - 模板参数化 <typename T>: T = xlen 类型 (uint32_t/uint64_t)

#ifndef CF_IP_CPU_ARCH_RISCV_PAYLOAD_RISCV_H
#define CF_IP_CPU_ARCH_RISCV_PAYLOAD_RISCV_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// ----------------------------------------------------------------------------
// RiscvDecodeDetail —— RISC-V 译码结果细节 (M3.2 补充 payload_common.h)
//
// 字段语义:
//   - funct3       3-bit funct3 字段 (I/S/B/R-type 指令共用)
//   - funct7       7-bit funct7 字段 (R-type 指令专用, I-type 仅低 5 位)
//   - funct12      12-bit funct12 字段 (SYSTEM 指令: ECALL/EBREAK/MRET 等)
//   - imm          符号扩展后的立即数 (32-bit for RV32, 64-bit for RV64)
//   - csr_addr     CSR 寄存器地址 (12-bit, Zicsr 指令专用)
//
// 注: rs1_idx/rs2_idx/rd_idx/op_class 仍在通用 DecodePayload
//     (M1.7 定义, M2.1 补充), 本结构仅含 RISC-V 特有字段
// ----------------------------------------------------------------------------
struct RiscvDecodeDetail {
  std::uint8_t  funct3     = 0;   // 3-bit funct3
  std::uint8_t  funct7     = 0;   // 7-bit funct7
  std::uint16_t funct12    = 0;   // 12-bit funct12 (SYSTEM 指令)
  std::int32_t  imm        = 0;   // 符号扩展后的立即数 (32-bit, 兼容 RV32/RV64)
  std::uint16_t csr_addr   = 0;   // 12-bit CSR 地址
};

// ----------------------------------------------------------------------------
// payload_keys_riscv<T> —— RISC-V 特有 Payload Key 模板
//
// T = xlen 类型 (uint32_t / uint64_t)
//
// 配套通用 Key (payload_common.h) 使用, 例如:
//   using KeyType = cf::cpu::arch::riscv::payload_keys_riscv<std::uint32_t>;
//   (*node)(KeyType::RISCV_DETAIL) = ...;
// ----------------------------------------------------------------------------
template <typename T = std::uint32_t>
struct payload_keys_riscv {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  // RISCV_DETAIL —— RISC-V 译码结果细节 struct
  static inline cf::plugin::Payload<RiscvDecodeDetail> RISCV_DETAIL{"cpu.riscv_detail"};

  // BRANCH_TARGET —— 分支目标地址 (xlen 类型, 替代通用 DecodePayload.branch_target)
  static inline cf::plugin::Payload<T> BRANCH_TARGET{"cpu.branch_target"};
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_PAYLOAD_RISCV_H
