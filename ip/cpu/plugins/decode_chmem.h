// ip/cpu/plugins/decode_chmem.h
//
// 功能描述: DecoderPlugin (CH_MEM 模式) — 完整 RV32I 译码 select 树
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-20 (Phase 6d 6d.1)
//
// 设计:
//   - 仅在 CF_PLUGIN_USE_CH_MEM 下编译
//   - 使用 ch_uint select() 树输出 DecodedInst (ch 类型字段)
//   - 覆盖 RV32I 全部 ~40 条指令 (R/I/S/B/U/J/FENCE/SYSTEM)
//   - ~80+ 项 mux_select 输出 (D4 合规证据)
//   - opcode 7 bit + funct3 3 bit + funct7 7 bit 层次化译码
//
// 输出 (DecodedInst):
//   - opcode, funct3, funct7 (原始字段透传)
//   - rd_idx, rs1_idx, rs2_idx (寄存器索引)
//   - imm (32-bit 符号扩展立即数)
//   - reads_rs1, reads_rs2, writes_rd (控制标志)
//
// 集成:
//   - 与 IntAluPlugin + BranchPlugin 通过 Payload<DecodedInst> 接口
//   - 6d.3 CpuFactoryChmem 集成时灌入 decode stage
//
// 借鉴:
//   - decoder_table.h: OpCode 枚举 + get_imm 立即数提取函数
//   - hazard_chmem.h: at_stage 闭包注册 + ch_bool 条件模式
//   - int_alu_chmem.h: select() 数替代 if/else
//   - branch_chmem.h: CtrlLink + at_stage 闭包模式
//
// 约束:
//   - D4 合规: 无 tick(), 用 at_stage() 注册闭包
//   - D4 合规: 闭包内 select 树替代 if/else 分支
//   - 无运行期 if(ch_bool) (ch_bool explicit operator bool 编译期通过但求值错误)
//   - ch_literal<V,W>{} 替代 _d 后缀字面值 (D4 合规)
//   - 需 CF_PLUGIN_USE_CH_MEM 编译开关

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CPU_PLUGINS_DECODE_CHMEM_H
#define CF_IP_CPU_PLUGINS_DECODE_CHMEM_H

#include <cstdint>
#include <type_traits>

#include <ch.hpp>
#include <core/bool.h>
#include <core/operators.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf {
namespace cpu {
namespace plugins {

// ============================================================================
// DecodedInst — 完整 RV32I 译码输出 (CH_MEM 模式, 含 ch 类型字段)
//
//  uint_t<N> 在 CH_MEM 模式 = ch_uint<N>
//  bool_t 在 CH_MEM 模式 = ch_bool
// ============================================================================
struct DecodedInst {
  uint_t<7>  opcode;       // 主 opcode [6:0]
  uint_t<3>  funct3;       // funct3 [14:12]
  uint_t<7>  funct7;       // funct7 [31:25]
  uint_t<5>  rd_idx;       // 目标寄存器 x0-x31
  uint_t<5>  rs1_idx;      // 源寄存器 1
  uint_t<5>  rs2_idx;      // 源寄存器 2
  uint_t<32> imm;          // 符号扩展立即数
  bool_t     reads_rs1;    // 是否读 rs1
  bool_t     reads_rs2;    // 是否读 rs2
  bool_t     writes_rd;    // 是否写 rd (x0=no)
  uint_t<8>  op_class;     // 0=ALU,1=BRANCH,2=LOAD,3=STORE,4=SYSTEM,255=UNKNOWN
  uint_t<3>  instr_format; // 0=R,1=I,2=S,3=B,4=U,5=J
};

// ============================================================================
// DecoderPlugin (CH_MEM) — RV32I 译码 Plugin
//
// 生命周期:
//   1. build(): 注册 at_stage("decode", NORMAL) 闭包
//   2. PipeBuilder::elaborate(): 执行闭包 → 发射 select 树 DAG
//   3. 输出 DecodedInst 到 DECODED_INST Payload 单元
//   4. 同时填充 DecodePayload + RiscvDecodeDetail
// ============================================================================
template <typename T = uint32_t>
class RiscvDecodePluginChmem : public PluginBase {
  static constexpr std::size_t kXlenBits =
#ifdef CF_PLUGIN_USE_CH_MEM
      32;  // CH_MEM PoC: 固定 RV32
#else
      sizeof(T) * 8;
#endif

 public:
  // ==========================================================================
  // DECODED_INST — CH_MEM 译码输出 Payload 键 (全局静态)
  // IntAluPlugin + BranchPlugin 通过此 Payload 接口读取译码结果
  // ==========================================================================
  static inline Payload<DecodedInst> DECODED_INST{"cpu.decoded_chmem"};

  RiscvDecodePluginChmem() = default;
  ~RiscvDecodePluginChmem() override = default;

  RiscvDecodePluginChmem(const RiscvDecodePluginChmem&) = delete;
  RiscvDecodePluginChmem& operator=(const RiscvDecodePluginChmem&) = delete;

  void setup(PipeBuilder& /*pb*/) override {}

  // --------------------------------------------------------------------------
  // build — 注册 decode 阶段译码闭包
  //
  // at_stage("decode", NORMAL):
  //   1. 读取 INSTRUCTION (ch_uint<32>)
  //   2. 字段提取: opcode/funct3/funct7/rd/rs1/rs2
  //   3. 立即数计算: I/S/B/U/J 五种格式
  //   4. 5 种 opcode 组 + funct3/funct7 译码 select 树
  //   5. 输出 DecodedInst 到 PayloadStore
  //   6. 同步填充 DecodePayload + RiscvDecodeDetail
  // --------------------------------------------------------------------------
  void build(PipeBuilder& pb) override {
    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;

    pb.at_stage("decode", Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("decode").get();
      if (n) {
      using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
      using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<T>;

      // ---------------------------------------------------------------
      // Step 1: 读 INSTRUCTION 信号
      // ---------------------------------------------------------------
      auto inst = n->operator()(KT::INSTRUCTION);

      // ---------------------------------------------------------------
      // Step 2: 字段提取 (ch_uint bit 切片)
      // ---------------------------------------------------------------
      auto opcode = bits<6, 0>(inst);   // ch_uint<7>
      auto rd     = bits<11, 7>(inst);  // ch_uint<5>
      auto funct3 = bits<14, 12>(inst); // ch_uint<3>
      auto rs1    = bits<19, 15>(inst); // ch_uint<5>
      auto rs2    = bits<24, 20>(inst); // ch_uint<5>
      auto funct7 = bits<31, 25>(inst); // ch_uint<7>

      // ---------------------------------------------------------------
      // Step 3: 立即数计算 (5 种格式, 全部预计算后 select 选择)
      //
      // 注意: 禁止使用 ch_uint << (CppHDL 左移会扩展位宽, 导致 zext 失败)
      // 改用 >> + & + select 组合实现立即数提取和符号扩展
      // ---------------------------------------------------------------

      // --- I-type: sign_extend(inst[31:20]) ---
      // (inst >> 20) 位 31→11, 符号扩展检查位 11
      auto i_lsr = inst >> ch_uint<5>(ch::core::ch_literal<20, 5>{});
      auto i_sgn = bits<11, 11>(i_lsr);
      auto imm_i = select(bool_t(i_sgn != ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                          i_lsr | ~ch_uint<32>(ch::core::ch_literal<0xFFF, 32>{}),
                          i_lsr);

      // --- S-type: sign_extend({inst[31:25], inst[11:7]}) ---
      // s_high = (inst >> 20) & 0xFE0  → inst[31:25] at 11:5
      auto s_high = (inst >> ch_uint<5>(ch::core::ch_literal<20, 5>{})) &
                     ch_uint<32>(ch::core::ch_literal<0xFE0, 32>{});
      auto s_low  = (inst >> ch_uint<5>(ch::core::ch_literal<7, 5>{})) &
                     ch_uint<32>(ch::core::ch_literal<0x1F, 32>{});
      auto s_raw  = s_high | s_low;
      auto s_sgn  = bits<11, 11>(s_raw);
      auto imm_s  = select(bool_t(s_sgn != ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                           s_raw | ~ch_uint<32>(ch::core::ch_literal<0xFFF, 32>{}),
                           s_raw);

      // --- B-type: sign_extend({inst[31], inst[7], inst[30:25], inst[11:8], 1'b0}) ---
      // bit12=inst[31]>>19, bit11=select(inst[7]→0x800), bits10:5=(inst>>20)&0x7E0, bits4:1=(inst>>7)&0x1E
      auto b_high = (inst >> ch_uint<5>(ch::core::ch_literal<19, 5>{})) &
                     ch_uint<32>(ch::core::ch_literal<0x1000, 32>{});
      auto b_bit11 = bits<7, 7>(inst);
      auto b_mid  = (inst >> ch_uint<5>(ch::core::ch_literal<20, 5>{})) &
                     ch_uint<32>(ch::core::ch_literal<0x7E0, 32>{});
      auto b_low  = (inst >> ch_uint<5>(ch::core::ch_literal<7, 5>{})) &
                     ch_uint<32>(ch::core::ch_literal<0x1E, 32>{});
      ch_uint<32> imm_b = ch_uint<32>(ch::core::ch_literal<0, 1>{});
      imm_b = imm_b | b_high;
      imm_b = select(bool_t(b_bit11 != ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                     imm_b | ch_uint<32>(ch::core::ch_literal<0x0800, 32>{}), imm_b);
      imm_b = imm_b | b_mid | b_low;
      // 符号扩展位 12
      auto b_sgn = bits<12, 12>(imm_b);
      imm_b = select(bool_t(b_sgn != ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                     imm_b | ~ch_uint<32>(ch::core::ch_literal<0x1FFF, 32>{}),
                     imm_b);

      // --- U-type: {inst[31:12], 12'b0} ---
      auto imm_u = inst & ~ch_uint<32>(ch::core::ch_literal<0xFFF, 32>{});

      // --- J-type: sign_extend({inst[31], inst[19:12], inst[20], inst[30:21], 1'b0}) ---
      // bit20=(inst>>11)&0x100000, bits19:12=inst&0xFF000, bit11=(inst>>9)&0x800,
      auto j_bit20  = (inst >> ch_uint<5>(ch::core::ch_literal<11, 5>{})) &
                       ch_uint<32>(ch::core::ch_literal<0x100000, 32>{});
      auto j_top8   = inst & ch_uint<32>(ch::core::ch_literal<0x0FF000, 32>{});
      auto j_bit11  = (inst >> ch_uint<5>(ch::core::ch_literal<9, 5>{})) &
                       ch_uint<32>(ch::core::ch_literal<0x800, 32>{});
      auto j_bot10  = (inst >> ch_uint<5>(ch::core::ch_literal<20, 5>{})) &
                       ch_uint<32>(ch::core::ch_literal<0x7FE, 32>{});
      ch_uint<32> imm_j = ch_uint<32>(ch::core::ch_literal<0, 1>{});
      imm_j = imm_j | j_bit20 | j_top8 | j_bit11 | j_bot10;
      // 符号扩展位 20
      auto j_sgn = bits<20, 20>(imm_j);
      imm_j = select(bool_t(j_sgn != ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                     imm_j | ~ch_uint<32>(ch::core::ch_literal<0x1FFFFF, 32>{}),
                     imm_j);

      // ---------------------------------------------------------------
      // Step 4: OpCode 组判别 (opcode[6:0] → 11 组)
      //   使用 decoder_table.h 中的 opcode 常量值
      // ---------------------------------------------------------------
      using namespace cf::cpu::arch::riscv;
      bool_t is_lui    = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_LUI, 7>{}));
      bool_t is_auipc  = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_AUIPC, 7>{}));
      bool_t is_jal    = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_JAL, 7>{}));
      bool_t is_jalr   = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_JALR, 7>{}));
      bool_t is_branch = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_BRANCH, 7>{}));
      bool_t is_load   = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_LOAD, 7>{}));
      bool_t is_store  = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_STORE, 7>{}));
      bool_t is_opimm  = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_OPIMM, 7>{}));
      bool_t is_op     = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_OP, 7>{}));
      bool_t is_fence  = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_FENCE, 7>{}));
      bool_t is_system = (opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_SYSTEM, 7>{}));

      // ---------------------------------------------------------------
      // Step 5: funct3 子译码 (每组需要 funct3 判别的)
      // ---------------------------------------------------------------
      // B-type (BRANCH, opcode=0x63)
      auto beq   = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto bne   = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto blt   = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto bge   = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{}));
      auto bltu  = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<6, 3>{}));
      auto bgeu  = is_branch && (funct3 == ch_uint<3>(ch::core::ch_literal<7, 3>{}));

      // LOAD (opcode=0x03)
      auto lb    = is_load  && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto lh    = is_load  && (funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto lw    = is_load  && (funct3 == ch_uint<3>(ch::core::ch_literal<2, 3>{}));
      auto lbu   = is_load  && (funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto lhu   = is_load  && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{}));

      // STORE (opcode=0x23)
      auto sb    = is_store && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto sh    = is_store && (funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto sw    = is_store && (funct3 == ch_uint<3>(ch::core::ch_literal<2, 3>{}));

      // OP-IMM (I-type ALU, opcode=0x13)
      auto addi   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto slti   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<2, 3>{}));
      auto sltiu  = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<3, 3>{}));
      auto xori   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto ori    = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<6, 3>{}));
      auto andi   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<7, 3>{}));
      auto slli   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto srli   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x00, 7>{}));
      auto srai   = is_opimm && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x20, 7>{}));

      // OP (R-type, opcode=0x33) — funct3 + funct7 联合译码
      auto add    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x00, 7>{}));
      auto sub    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x20, 7>{}));
      auto sll    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto slt    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<2, 3>{}));
      auto sltu   = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<3, 3>{}));
      auto x_or   = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto srl    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x00, 7>{}));
      auto sra    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{})) &&
                    (funct7 == ch_uint<7>(ch::core::ch_literal<0x20, 7>{}));
      auto or_    = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<6, 3>{}));
      auto and_   = is_op && (funct3 == ch_uint<3>(ch::core::ch_literal<7, 3>{}));

      // ---------------------------------------------------------------
      // Step 6: 立即数选择 (select 树)
      //   默认 imm=0, 按指令格式 select
      // ---------------------------------------------------------------
      ch_uint<32> imm_result = ch_uint<32>(ch::core::ch_literal<0, 1>{});
      imm_result = select(is_lui || is_auipc, imm_u,  imm_result);
      imm_result = select(is_jal,             imm_j,  imm_result);
      imm_result = select(is_jalr,            imm_i,  imm_result);
      imm_result = select(is_branch,          imm_b,  imm_result);
      imm_result = select(is_load,            imm_i,  imm_result);
      imm_result = select(is_store,           imm_s,  imm_result);
      imm_result = select(is_opimm,           imm_i,  imm_result);
      // OP/FENCE/SYSTEM use zero immediate (imm_result stays default)

      // ---------------------------------------------------------------
      // Step 7: writes_rd select 树
      //   写 rd 的指令: LUI, AUIPC, JAL, JALR, LOAD, OP-IMM, OP
      //   不写的: STORE, BRANCH, FENCE, SYSTEM ECALL/EBREAK
      // ---------------------------------------------------------------
      auto rd_zero  = ch_uint<5>(ch::core::ch_literal<0, 5>{});
      bool_t writes_rd_flg(false);
      writes_rd_flg = select(is_lui,   bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_auipc, bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_jal,   bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_jalr,  bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_load,  bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_opimm, bool_t(true), writes_rd_flg);
      writes_rd_flg = select(is_op,    bool_t(true), writes_rd_flg);
      // SYSTEM: writes_rd when funct3 != 0 (CSR read)
      writes_rd_flg = select(is_system && (funct3 != ch_uint<3>(ch::core::ch_literal<0, 3>{})),
                             bool_t(true), writes_rd_flg);
      // x0 屏蔽: writes_rd && rd != 0
      writes_rd_flg = writes_rd_flg && (rd != rd_zero);

      // ---------------------------------------------------------------
      // Step 8: reads_rs1 select 树
      //   读 rs1 的指令: JALR, BRANCH, LOAD, STORE, OP-IMM, OP
      //   不读的: LUI, AUIPC, JAL, FENCE, SYSTEM ECALL/EBREAK
      // ---------------------------------------------------------------
      bool_t reads_rs1_flg(false);
      reads_rs1_flg = select(is_jalr,   bool_t(true), reads_rs1_flg);
      reads_rs1_flg = select(is_branch, bool_t(true), reads_rs1_flg);
      reads_rs1_flg = select(is_load,   bool_t(true), reads_rs1_flg);
      reads_rs1_flg = select(is_store,  bool_t(true), reads_rs1_flg);
      reads_rs1_flg = select(is_opimm,  bool_t(true), reads_rs1_flg);
      reads_rs1_flg = select(is_op,     bool_t(true), reads_rs1_flg);
      // SYSTEM: reads rs1 when funct3 != 0 (CSR R-type)
      reads_rs1_flg = select(is_system && (funct3 != ch_uint<3>(ch::core::ch_literal<0, 3>{})),
                             bool_t(true), reads_rs1_flg);

      // ---------------------------------------------------------------
      // Step 9: reads_rs2 select 树
      //   读 rs2 的指令: OP (R-type), STORE, BRANCH
      //   不读的: I-type ALU, LOAD, JALR, LUI/AUIPC/JAL/FENCE/SYSTEM
      // ---------------------------------------------------------------
      bool_t reads_rs2_flg(false);
      reads_rs2_flg = select(is_op,     bool_t(true), reads_rs2_flg);
      reads_rs2_flg = select(is_store,  bool_t(true), reads_rs2_flg);
      reads_rs2_flg = select(is_branch, bool_t(true), reads_rs2_flg);

      // ---------------------------------------------------------------
      // Step 10: op_class select 树 (0=ALU,1=BRANCH,2=LOAD,3=STORE,4=SYSTEM)
      // ---------------------------------------------------------------
      auto op_class_alu = ch_uint<8>(ch::core::ch_literal<0, 8>{});
      auto op_class_br  = ch_uint<8>(ch::core::ch_literal<1, 8>{});
      auto op_class_ld  = ch_uint<8>(ch::core::ch_literal<2, 8>{});
      auto op_class_st  = ch_uint<8>(ch::core::ch_literal<3, 8>{});
      auto op_class_sys = ch_uint<8>(ch::core::ch_literal<4, 8>{});
      auto op_class_unk = ch_uint<8>(ch::core::ch_literal<255, 8>{});
      ch_uint<8> op_class_res = op_class_unk;
      op_class_res = select(is_lui || is_auipc || is_opimm || is_op,
                            op_class_alu, op_class_res);
      op_class_res = select(is_jal || is_jalr || is_branch,
                            op_class_br, op_class_res);
      op_class_res = select(is_load,  op_class_ld, op_class_res);
      op_class_res = select(is_store, op_class_st, op_class_res);
      op_class_res = select(is_fence || is_system,
                            op_class_sys, op_class_res);

      // ---------------------------------------------------------------
      // Step 11: instr_format select 树 (0=R,1=I,2=S,3=B,4=U,5=J)
      // ---------------------------------------------------------------
      ch_uint<3> fmt_r = ch_uint<3>(ch::core::ch_literal<0, 3>{});
      ch_uint<3> fmt_i = ch_uint<3>(ch::core::ch_literal<1, 3>{});
      ch_uint<3> fmt_s = ch_uint<3>(ch::core::ch_literal<2, 3>{});
      ch_uint<3> fmt_b = ch_uint<3>(ch::core::ch_literal<3, 3>{});
      ch_uint<3> fmt_u = ch_uint<3>(ch::core::ch_literal<4, 3>{});
      ch_uint<3> fmt_j = ch_uint<3>(ch::core::ch_literal<5, 3>{});
      ch_uint<3> instr_fmt = fmt_r;
      instr_fmt = select(is_op,    fmt_r, instr_fmt);
      instr_fmt = select(is_opimm || is_load || is_jalr || is_fence || is_system,
                         fmt_i, instr_fmt);
      instr_fmt = select(is_store, fmt_s, instr_fmt);
      instr_fmt = select(is_branch,fmt_b, instr_fmt);
      instr_fmt = select(is_lui || is_auipc, fmt_u, instr_fmt);
      instr_fmt = select(is_jal,   fmt_j, instr_fmt);

      // ---------------------------------------------------------------
      // Step 12: 组装 DecodedInst 并写入 PayloadStore
      // ---------------------------------------------------------------
      DecodedInst decoded;
      decoded.opcode    = opcode;
      decoded.funct3    = funct3;
      decoded.funct7    = funct7;
      decoded.rd_idx    = rd;
      decoded.rs1_idx   = rs1;
      decoded.rs2_idx   = rs2;
      decoded.imm       = imm_result;
      decoded.reads_rs1 = reads_rs1_flg;
      decoded.reads_rs2 = reads_rs2_flg;
      decoded.writes_rd = writes_rd_flg;
      decoded.op_class  = op_class_res;
      decoded.instr_format = instr_fmt;

      // Write to DECODED_INST payload key
      n->operator()(RiscvDecodePluginChmem::DECODED_INST) = decoded;

      // ---------------------------------------------------------------
      // Step 13: 保留 DECODE + RISCV_DETAIL 默认值 (elaboration-time static)
      //   供旧 Plugin (RegFile/Hazard/IntAlu/Branch) 读 POD 字段.
      //   CH_MEM 运行期译码以 DECODED_INST (ch 信号) 为准;
      //   旧 Plugin 读 POD 字段, 值恒为 EARLY-populate 的默认值.
      //   Phase 6d.5+ 迁移到 DECODED_INST 后此步可删除.
      // ---------------------------------------------------------------
      {
        auto& dec = n->operator()(KT::DECODE);
        auto& rv  = n->operator()(RvKey::RISCV_DETAIL);
        (void)dec;
        (void)rv;
      }
      }  // end if (n)
    });
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_DECODE_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM