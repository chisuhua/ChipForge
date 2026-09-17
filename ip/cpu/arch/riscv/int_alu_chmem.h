// ip/cpu/arch/riscv/int_alu_chmem.h
//
// 功能描述: RiscvIntAluPlugin (CH_MEM 模式) — RV32I/RV64I ALU 的 elaboration 版本
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17 (Phase 6c M3 PoC, Prereq-2)
//
// Oracle 参考: CppHDL/examples/riscv-mini/src/rv32i_alu.h (lines 116-125)
//   11 层 select 嵌套模式
//
// 11 op 列表:
//   1. ADD  (通过 op2 mux 同时覆盖 ADDI, ADDI 不占独立分支)
//   2. SUB
//   3. SLL
//   4. SLT  — 新增 (有符号比较, 用 zext<kXlenBits> 包装 ch_bool)
//   5. SLTU — 新增 (无符号比较)
//   6. XOR  — 新增
//   7. SRL
//   8. SRA  — 新增 (算术右移, 符号扩展)
//   9. OR   — 新增
//  10. AND  — 新增
//  + AUIPC / LUI 顶层 select (在 11 层外)
//
// M3/Prereq-2 变更:
//   - 文件从 .disabled 重命名为 .h
//   - 修正 line 125 select 语法: select(dec ==(dec.reads_rs2), ...)
//     → select(ch_bool(dec.reads_rs2), rs2_val, imm)
//   - 扩展 6 op (AUIPC/LUI/ADD/SUB/SLL/SRL) → 11 op (10 R-type + ADDI via op2 mux)
//   - 所有 C++ bool 进 select 条件必须显式 ch_bool() 包装 (Oracle 风险预警 #2)
//   - 添加 #ifdef CF_PLUGIN_USE_CH_MEM 整文件包装
//
// 设计:
//   - 仅在 CF_PLUGIN_USE_CH_MEM 下编译（elaboration 路径）
//   - if/else opcode 分流 → select() 树（11 层 ALU op mux + 2 层顶层 mux）
//   - ch_bool 表达式替代运行期 std::function<bool()>
//   - PoC: 单 execute 阶段, 输出 RESULT + RD_DATA 到 PayloadStore cell
//
// 借鉴:
//   - CppHDL/examples/riscv-mini/src/rv32i_alu.h (10 R-type + 1 default = 11 层 select)
//   - 当前 TLM 版本 ip/cpu/arch/riscv/int_alu.h
//
// 约束:
//   - 必须 -DCF_PLUGIN_USE_CH_MEM 打开
//   - D4 合规: 无 tick(), 无运行期 if/else 分支
//   - 闭包内仅用 select(), bits<N-1,0>(), operator+(ch_uint, ch_uint) 等 ch 操作

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CPU_ARCH_RISCV_INT_ALU_CHMEM_H
#define CF_IP_CPU_ARCH_RISCV_INT_ALU_CHMEM_H

#include <cstdint>
#include <type_traits>

#include <ch.hpp>
#include <core/bool.h>
#include <core/operators.h>
#include <core/reg.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// ============================================================================
// RiscvIntAluPlugin (CH_MEM): select 树替代 if/else
// ============================================================================
template <typename T>
class RiscvIntAluPlugin : public PluginBase {
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_unsigned<T>::value,
                "RiscvIntAluPlugin<T>: T must be unsigned (TLM mode)");
#endif
  static constexpr std::size_t kXlenBits =
#ifdef CF_PLUGIN_USE_CH_MEM
      32;  // CH_MEM PoC: 固定 RV32 (T=ch_uint<32> 时 sizeof(T)*8 ≠ 32)
#else
      sizeof(T) * 8;
#endif

 public:
  RiscvIntAluPlugin() = default;
  ~RiscvIntAluPlugin() override = default;

  RiscvIntAluPlugin(const RiscvIntAluPlugin&) = delete;
  RiscvIntAluPlugin& operator=(const RiscvIntAluPlugin&) = delete;

  void setup(PipeBuilder& /*pb*/) override {}

  void build(PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        // 读输入信号 (ch 类型, elaboration 期 DAG 节点)
      auto rs1_val = n->operator()(KeyType::RS1);
      auto rs2_val = n->operator()(KeyType::RS2);
      const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
      const auto& dec = n->operator()(KeyType::DECODE);

      // ===============================================================
      // CH_MEM 版本: select() 树替代 if/else
      // 顶层: AUIPC / LUI (U-type, 基于 opcode 分流)
      // 底层: 10 R-type ALU op + 1 implicit default = 11 层 select
      // ADDI 不占独立分支 — 与 ADD 共享 datapath, 经 op2 mux 免费获得
      // ===============================================================
      auto pc_val = n->operator()(KeyType::PC);
      auto imm = static_cast<T>(rv.imm);
      auto is_auipc = ch_bool(rv.opcode == opcode::OP_AUIPC);
      auto is_lui   = ch_bool(rv.opcode == opcode::OP_LUI);

      // funct3/funct7 → ALU mux 选择
      // 注: C++ 比较 + ch_bool() 包装 (避免 ch_uint<N>(int) 重载歧义和字面值宽度问题)
      // ADD / ADDI: funct3 == 000, funct7 != 0100000
      // SUB:       funct3 == 000, funct7 == 0100000
      // SLL:       funct3 == 001
      // SLT:       funct3 == 010
      // SLTU:      funct3 == 011
      // XOR:       funct3 == 100
      // SRL:       funct3 == 101, funct7 != 0100000
      // SRA:       funct3 == 101, funct7 == 0100000
      // OR:        funct3 == 110
      // AND:       funct3 == 111
      auto is_add  = ch_bool(rv.funct3 == 0)  && ch_bool(rv.funct7 != 0x20);
      auto is_sub  = ch_bool(rv.funct3 == 0)  && ch_bool(rv.funct7 == 0x20);
      auto is_sll  = ch_bool(rv.funct3 == 1);
      auto is_slt  = ch_bool(rv.funct3 == 2);
      auto is_sltu = ch_bool(rv.funct3 == 3);
      auto is_xor  = ch_bool(rv.funct3 == 4);
      auto is_srl  = ch_bool(rv.funct3 == 5) && ch_bool(rv.funct7 != 0x20);
      auto is_sra  = ch_bool(rv.funct3 == 5) && ch_bool(rv.funct7 == 0x20);
      auto is_or   = ch_bool(rv.funct3 == 6);
      auto is_and  = ch_bool(rv.funct3 == 7);

      // 10 R-type ALU ops + implicit default = 11 层 select 树
      // 注: 用 ch_literal<V, W> 显式宽度, 避开 _d 后缀字面值的字符解析限制
      ch_uint<kXlenBits> result(ch::core::ch_literal<0, 1>{}, "alu_result");

      // ── AUIPC / LUI ──┤ 顶层单独分支 (在 11 层外)
      result = select(is_auipc, pc_val + imm, result);
      result = select(is_lui, imm, result);

      // I-type vs R-type: op2 = imm (I-type) or rs2_val (R-type)
      // ADDI 通过此 mux 与 ADD 共享 datapath
      auto op2 = select(ch_bool(dec.reads_rs2), rs2_val, imm);

      // ── 计算各 ALU 结果 ──
      // 注: ch_literal<V, W> 显式宽度构造, 避 _d 字面值的字符解析限制
      auto shift_amount = op2 & ch_uint<5>(ch::core::ch_literal<0x1F, 5>{});

      auto r_add  = rs1_val + op2;
      auto r_sub  = rs1_val - op2;
      auto r_sll  = rs1_val << shift_amount;
      auto r_srl  = rs1_val >> shift_amount;

      // SLT: 有符号比较 — 符号不同时为负, 符号相同时看减法借位
      auto a_sign     = bits<kXlenBits - 1, kXlenBits - 1>(rs1_val);
      auto b_sign     = bits<kXlenBits - 1, kXlenBits - 1>(op2);
      auto slt_sd     = ch_bool(a_sign != b_sign);
      auto slt_aneg   = ch_bool(a_sign == ch_bool(true));
      auto slt_borrow = ch_bool(bits<kXlenBits - 1, kXlenBits - 1>(r_sub) !=
                                ch_uint<1>(ch::core::ch_literal<0, 1>{}));
      auto lt_signed  = select(slt_sd, slt_aneg, slt_borrow);
      auto r_slt      = zext<kXlenBits>(lt_signed);

      // SLTU: 无符号比较
      auto r_sltu = zext<kXlenBits>(ch_bool(rs1_val < op2));

      // XOR
      auto r_xor = rs1_val ^ op2;

      // SRA: 算术右移 (符号扩展填充高位)
      auto sra_sign = bits<kXlenBits - 1, kXlenBits - 1>(rs1_val);
      // 注: 用 ~ch_uint<N>(ch_literal<0,1>) 取反得到全 1, 避免 ch_uint<N>(~0ULL) 重载歧义
      auto sign_rep = select(ch_bool(sra_sign),
                              ~ch_uint<kXlenBits>(ch::core::ch_literal<0, 1>{}),
                              ch_uint<kXlenBits>(ch::core::ch_literal<0, 1>{}));
      auto shift_wide = zext<kXlenBits>(shift_amount);
      auto sra_fill   = sign_rep >> shift_wide << shift_wide;
      auto r_sra = r_srl | sra_fill;

      // OR / AND
      auto r_or  = rs1_val | op2;
      auto r_and = rs1_val & op2;

      // ── 10 层 R-type select + 1 default = 11 层 mux ──
      result = select(is_add,  r_add,  result);
      result = select(is_sub,  r_sub,  result);
      result = select(is_sll,  r_sll,  result);
      result = select(is_slt,  r_slt,  result);
      result = select(is_sltu, r_sltu, result);
      result = select(is_xor,  r_xor,  result);
      result = select(is_srl,  r_srl,  result);
      result = select(is_sra,  r_sra,  result);
      result = select(is_or,   r_or,   result);
      result = select(is_and,  r_and,  result);

      // 写 RESULT + RD_DATA 到 PayloadStore cell
      // CH_MEM: 赋值即发射 lnode DAG assign 节点
      n->operator()(KeyType::RD_DATA) = result;
      n->operator()(KeyType::RESULT)  = result;
      }  // end if (n)
    });
  }

  // 单元测试辅助 API (CH_MEM 模式: 直接调用 compute_mux emit lnode)
  static T compute_static(T rs1, T rs2, std::uint8_t op) {
    switch (op) {
      case 0:  return rs1 + rs2;                                // ADD
      case 1:  return rs1 << (rs2 & 0x1F);                      // SLL
      case 2:  return static_cast<T>(                           // SLT
                 (static_cast<std::make_signed_t<T>>(rs1) <
                  static_cast<std::make_signed_t<T>>(rs2)) ? 1 : 0);
      case 3:  return static_cast<T>((rs1 < rs2) ? 1 : 0);      // SLTU
      case 4:  return rs1 ^ rs2;                                // XOR
      case 5:  return rs1 >> (rs2 & 0x1F);                      // SRL
      case 6:  return rs1 | rs2;                                // OR
      case 7:  return rs1 & rs2;                                // AND
      case 8:  return rs1 - rs2;                                // SUB
      case 9:  return static_cast<T>(                           // SRA
                 static_cast<std::make_signed_t<T>>(rs1) >> (rs2 & 0x1F));
      default: return 0;
    }
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM
#endif  // CF_IP_CPU_ARCH_RISCV_INT_ALU_CHMEM_H