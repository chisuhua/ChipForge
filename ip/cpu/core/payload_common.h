// ip/cpu/core/payload_common.h
//
// 功能描述: CPU 通用 Payload Key 集合 (M1.7) —— 跨 ISA 通用, RISC-V 特有
//           Key 放 ip/cpu/arch/riscv/payload_riscv.h (M3 实施)。
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计目标:
//   提供 8 个跨阶段 IPC 的通用 Payload Key, 覆盖 CPU 5 级流水线的
//   核心数据流 (PC/INSTRUCTION → DECODE → EXECUTE → WRITEBACK):
//     1. PC          程序计数器 (取指阶段输出, 全程透传)
//     2. INSTRUCTION 原始指令字 (取指阶段输出, 译码阶段输入)
//     3. RS1         源寄存器 1 数据 (译码/执行阶段输入)
//     4. RS2         源寄存器 2 数据 (译码/执行阶段输入)
//     5. RD_DATA     写回数据 (执行/写回阶段输出)
//     6. RD_IDX      写回目标寄存器索引 (译码/写回阶段输入)
//     7. DECODE      译码结果 struct (译码阶段输出, 执行阶段输入)
//     8. RESULT      通用执行结果 (执行阶段输出, 写回阶段输入)
//
// 借鉴:
//   - L1CachePlugin.cpp 47-77 (匿名 namespace 静态全局 Payload Key 模式)
//   - VexRiscv Stageable[T] 静态对象模式 (via cf::plugin::Payload<T>)
//   - RISC-V Spike ISS (Decode/Execute/Writeback 数据流)
//
// 配套决策:
//   - 蓝图 §1.1.4 (5 级流水 Plugin 横向切片)
//   - 蓝图 §3.2 (用 logical_stage 名跨 3/5/7 级流水复用)
//   - 议题 2 选 B (Plugin 拆分粒度: 5 个核心 P0 + 6 P1+ 推迟)
//   - 议题 3 选 B+C (RegFilePlugin array_store 抽象)
//
// 约束:
//   - 头文件为主 (无 .cpp, 跨翻译单元共享 Payload Key 静态对象)
//   - 模板参数化 <typename T, unsigned XLEN>: T=xlen 类型 (uint32_t/uint64_t),
//     XLEN=32 或 64 (用于编译期 sanity check)
//   - 编译期零开销 (Payload Key 静态对象即全局单例)
//   - 8 Key 必须全局唯一 (不与 cf_plugin::Payload / bundles 冲突)
//   - 不引入 ch::core::context (TLM 模式禁止)
//
// 与蓝图的微小调整:
//   蓝图说"DecodePayload struct 包含 13 个 Key"—— 实际只 8 Key。
//   理由: 8 Key 覆盖核心数据流, 剩余 5 Key 推迟到 M3 (RISC-V 特有)
//   或 Phase 5+ (FPU/MMU/Exception Plugin 引入时再补)。 避免 M1 范围蔓延。

#ifndef CF_IP_CPU_CORE_PAYLOAD_COMMON_H
#define CF_IP_CPU_CORE_PAYLOAD_COMMON_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace cpu {
namespace core {
namespace payload {

// ----------------------------------------------------------------------------
// DecodePayload —— 译码结果结构 (M1.7 暂用最小字段集)
//
// 字段语义:
//   - op_class      操作类别 (ALU/BRANCH/LOAD/STORE/SYSTEM 等)
//   - writes_rd     是否写回通用寄存器 (影响写回阶段 enable)
//   - reads_rs1     是否读 RS1 (影响 RegFile 读端口 enable)
//   - reads_rs2     是否读 RS2 (影响 RegFile 读端口 enable)
//   - rd_class      写回目标的"类别" (普通/CSR/PC 等, RISC-V 细化推迟 M3)
//
// 注: RISC-V 特有字段 (funct3/funct7/imm 等) 推迟到
//     ip/cpu/arch/riscv/payload_riscv.h (M3.2 实施)。
// ----------------------------------------------------------------------------
struct DecodePayload {
  enum class OpClass : std::uint8_t {
    ALU     = 0,
    BRANCH  = 1,
    LOAD    = 2,
    STORE   = 3,
    SYSTEM  = 4,   // CSR / ECALL / EBREAK / MRET 等
    FPU     = 5,   // P3+ 推迟
    VECTOR  = 6,   // Phase 6+ 推迟
    UNKNOWN = 255,
  };

  OpClass  op_class   = OpClass::UNKNOWN;
  bool     writes_rd  = false;
  bool     reads_rs1  = false;
  bool     reads_rs2  = false;
  std::uint8_t rd_class = 0;  // 0=普通 GPR, 1=CSR (M3 细化)
};

// ----------------------------------------------------------------------------
// payload_keys<T, XLEN> —— 8 通用 Payload Key 模板
//
// T = xlen 类型 (uint32_t / uint64_t), 编译期由 RegFilePlugin 实例化
// XLEN = 32 或 64 (编译期 sanity check, 防 T 与 XLEN 不一致)
//
// 用法 (典型 5 级流水, RV32):
//   using namespace cf::cpu::core::payload;
//   cf::plugin::PipeBuilder pb(cf::plugin::ImplMode::TLM);
//   auto* if_id = pb.create_node("if_id");
//   (*if_id)(keys<>::PC) = 0x80000000;          // 取指阶段写入
//   auto* id_ex = pb.node_of_logic_stage("decode");
//   auto pc = (*id_ex)(keys<>::PC);             // 译码阶段读取
// ----------------------------------------------------------------------------
template <typename T = std::uint32_t, unsigned XLEN = sizeof(T) * 8>
struct keys {
  static_assert(XLEN == 32 || XLEN == 64,
                "XLEN must be 32 or 64 (RISC-V only supports these)");
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  // 跨阶段 IPC Key (匿名 namespace 等价: 文件作用域静态全局对象, 跨 TU 共享)
  // 命名约定: "cpu.<key>" (模块名前缀, 避免与 cache.bundles 冲突)

  // 1. PC —— 程序计数器 (xlen 类型, RV32=32bit, RV64=64bit)
  static inline cf::plugin::Payload<T> PC{"cpu.pc"};

  // 2. INSTRUCTION —— 原始指令字 (32-bit, RV32/RV64 都用 32-bit 指令)
  static inline cf::plugin::Payload<cf::plugin::uint_t<32>> INSTRUCTION{"cpu.instruction"};

  // 3. RS1 —— 源寄存器 1 数据 (xlen 类型)
  static inline cf::plugin::Payload<T> RS1{"cpu.rs1"};

  // 4. RS2 —— 源寄存器 2 数据 (xlen 类型)
  static inline cf::plugin::Payload<T> RS2{"cpu.rs2"};

  // 5. RD_DATA —— 写回数据 (xlen 类型, ALU 输出 / Load 数据 / CSR 读出)
  static inline cf::plugin::Payload<T> RD_DATA{"cpu.rd_data"};

  // 6. RD_IDX —— 写回目标寄存器索引 (5-bit, x0-x31 索引)
  static inline cf::plugin::Payload<cf::plugin::uint_t<5>> RD_IDX{"cpu.rd_idx"};

  // 7. DECODE —— 译码结果 struct (cf::cpu::core::payload::DecodePayload)
  static inline cf::plugin::Payload<DecodePayload> DECODE{"cpu.decode"};

  // 8. RESULT —— 通用执行结果 (xlen 类型, 多数指令类型, FPU 推迟 P3+)
  static inline cf::plugin::Payload<T> RESULT{"cpu.result"};
};

// ----------------------------------------------------------------------------
// 常用实例化 (RV32 默认, RV64 可在 .cpp 中显式特化)
// ----------------------------------------------------------------------------
using keys_rv32 = keys<std::uint32_t, 32>;
using keys_rv64 = keys<std::uint64_t, 64>;

}  // namespace payload
}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_PAYLOAD_COMMON_H
