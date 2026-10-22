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
// RV32M (M 扩展, 2026-10-20 chmem-mext-comb-datapath):
//   在 11 层 base 树之后追加 8 层 M-op select (funct7 == 0x01):
//     MUL / MULH / MULHSU / MULHU / DIV / DIVU / REM / REMU
//   => 合计 19 层 select 树.
//   - CppHDL 无 ch_int → 有符号运算走 sign-magnitude (neg32/abs32/neg_flag)
//   - 除/模必须用 safe divisor 显式 guard 除零 (CppHDL 除零默认返回 0,
//     RISC-V 要求 DIV/DIVU -> 全 1, REM/REMU -> 被除数)
//   - base 侧 6 个 R-type op 补 f7_00 gate, 与 M-op 判别互斥 (双保险)
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
#include "ip/cpu/plugins/decode_chmem.h"

// csr-chmem-pipeline-integration R3.2: 消费 CSR_READ_VALUE 覆写 RD_DATA
#include "bundles/csr_bundles.h"

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

    pb.at_stage("execute", Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        // 读输入信号 (ch 类型, elaboration 期 DAG 节点)
      auto rs1_val = n->operator()(KeyType::RS1);
      auto rs2_val = n->operator()(KeyType::RS2);
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      if (n->payloads().has(DecodePlugin::DECODED_INST)) {
      const auto& decoded = n->operator()(DecodePlugin::DECODED_INST);

      // ===============================================================
      // CH_MEM 版本: select() 树替代 if/else
      // 顶层: AUIPC / LUI (U-type, 基于 opcode 分流)
      // 底层: 10 R-type ALU op + 1 implicit default = 11 层 select
      // ADDI 不占独立分支 — 与 ADD 共享 datapath, 经 op2 mux 免费获得
      // ===============================================================
      auto pc_val = n->operator()(KeyType::PC);
      auto imm = decoded.imm;
      auto is_auipc = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_AUIPC, 7>{}));
      auto is_lui   = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_LUI, 7>{}));

      // funct3/funct7 → ALU mux 选择 (ch 信号比较, DECODED_INST)
      // Opcode gate — 防止 SW (funct3=010) / BEQ (funct3=000) / JAL 等
      // 误匹配 ALU funct3 判别 (RESULT 恒 0, tohost=1 正确路径)
      auto is_op    = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_OP,    7>{}));
      auto is_opimm = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_OPIMM, 7>{}));
      auto is_op_or_opimm = is_op || is_opimm;

      auto f3_0 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto f3_1 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto f3_2 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<2, 3>{}));
      auto f3_3 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<3, 3>{}));
      auto f3_4 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto f3_5 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{}));
      auto f3_6 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<6, 3>{}));
      auto f3_7 = (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<7, 3>{}));

      auto f7_00 = (decoded.funct7 == ch_uint<7>(ch::core::ch_literal<0x00, 7>{}));
      auto f7_20 = (decoded.funct7 == ch_uint<7>(ch::core::ch_literal<0x20, 7>{}));
      auto f7_01 = (decoded.funct7 == ch_uint<7>(ch::core::ch_literal<0x01, 7>{}));

      // ADD/ADDI: funct3=000. OP (R-type) requires funct7=0x00 (SUB uses
      // 0x20); OPIMM (ADDI) must NOT check funct7 — bits[31:25] hold the
      // immediate's [11:5], which is non-zero for negative immediates.
      auto is_add  = (is_op && f3_0 && f7_00) || (is_opimm && f3_0);
      // SUB: OP only, funct3=000, funct7=0x20
      auto is_sub  = is_op && f3_0 && f7_20;
      // SLL/SLLI: funct3=001
      // 注: R-type 侧补 f7_00 —— MULH (funct7=0x01, funct3=001) 与 SLL 同
      // funct3, 无 gate 会误捕. I-type (SLLI) 不检查 funct7 (bits[31:25]
      // 是立即数 shamt[5]+funct7 字段, 负立即数时非 0).
      auto is_sll  = (is_op && f3_1 && f7_00) || (is_opimm && f3_1);
      // SLT/SLTI: funct3=010
      auto is_slt  = (is_op && f3_2 && f7_00) || (is_opimm && f3_2);
      // SLTU/SLTIU: funct3=011
      auto is_sltu = (is_op && f3_3 && f7_00) || (is_opimm && f3_3);
      // XOR/XORI: funct3=100
      auto is_xor  = (is_op && f3_4 && f7_00) || (is_opimm && f3_4);
      // SRL/SRLI: funct3=101, funct7=0x00
      auto is_srl  = is_op_or_opimm && f3_5 && f7_00;
      // SRA/SRAI: funct3=101, funct7=0x20
      auto is_sra  = is_op_or_opimm && f3_5 && f7_20;
      // OR/ORI: funct3=110
      auto is_or   = (is_op && f3_6 && f7_00) || (is_opimm && f3_6);
      // AND/ANDI: funct3=111
      auto is_and  = (is_op && f3_7 && f7_00) || (is_opimm && f3_7);

      // ── RV32M (M 扩展) 判别 ──────────────────────────────────────────
      // funct7 = 0x01 标识 M-op: MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU
      // (opcode 0x33 = OP, R-type; decode 侧 reads_rs2/writes_rd 已就绪)
      auto is_mext   = is_op && f7_01;
      auto is_mul    = is_mext && f3_0;
      auto is_mulh   = is_mext && f3_1;
      auto is_mulhsu = is_mext && f3_2;
      auto is_mulhu  = is_mext && f3_3;
      auto is_div    = is_mext && f3_4;
      auto is_divu   = is_mext && f3_5;
      auto is_rem    = is_mext && f3_6;
      auto is_remu   = is_mext && f3_7;

      // 10 R-type ALU ops + implicit default = 11 层 select 树
      // 注: 用 ch_literal<V, W> 显式宽度, 避开 _d 后缀字面值的字符解析限制
      ch_uint<kXlenBits> result(ch::core::ch_literal<0, 1>{}, "alu_result");

      // ── AUIPC / LUI ──┤ 顶层单独分支 (在 11 层外)
      result = select(is_auipc, pc_val + imm, result);
      result = select(is_lui, imm, result);

      // ── STORE / LOAD: 有效地址 = rs1 + imm (DMem 用 RESULT 作字节地址) ──
      // SW x4,0(x0) → addr = x0 + 0 = 0 (tohost 正确路径)
      auto is_store = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_STORE, 7>{}));
      auto is_load  = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_LOAD,  7>{}));
      result = select(is_store || is_load, rs1_val + imm, result);

      // I-type vs R-type: op2 = imm (I-type) or rs2_val (R-type)
      // ADDI 通过此 mux 与 ADD 共享 datapath
      auto op2 = select(decoded.reads_rs2, rs2_val, imm);

      // ── 计算各 ALU 结果 ──
      // 注: ch_literal<V, W> 显式宽度构造, 避 _d 字面值的字符解析限制
      // Phase 6d bugfix: AND with 0x1F widened shift_amount back to 32-bit
      // (and_op::result_width = max(lhs_w, rhs_w)), so shl_op's M+N=64-bit
      // got truncated to [30:0] 31-bit by codegen — `1 << 31` returned 0.
      // Use bits<4,0> so shl_op computes 32+5 = 37-bit and bit 31 survives.
      auto shift_amount = bits<4, 0>(op2);

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

      // SRA: 算术右移 (符号扩展填充高位). The previous `sign_rep >> shft << shft`
      // lost the high-shift bits when shr truncated them; the correct fill is
      // sign_rep & ~(all_ones >> shift), i.e. AND sign_rep with the high-shift mask.
      auto sra_sign = bits<kXlenBits - 1, kXlenBits - 1>(rs1_val);
      auto all_ones = ~ch_uint<kXlenBits>(ch::core::ch_literal<0, 1>{});
      auto sign_rep = select(ch_bool(sra_sign), all_ones,
                              ch_uint<kXlenBits>(ch::core::ch_literal<0, 1>{}));
      auto hi_mask = ~(all_ones >> shift_amount);
      auto sra_fill = sign_rep & hi_mask;
      auto r_sra = r_srl | sra_fill;

      // OR / AND
      auto r_or  = rs1_val | op2;
      auto r_and = rs1_val & op2;

      // ── RV32M (M 扩展) 结果计算 ──────────────────────────────────────
      // CppHDL 无 ch_int → 有符号运算走 sign-magnitude (neg32/abs32/neg_flag)
      //
      // 除/模必须用 safe divisor: CppHDL 的除零默认行为是打 stderr 并返回 0
      // (CppHDL/include/ast/instr_op_arith.h:51-76), 而 RISC-V 要求
      //   DIV / DIVU 除零 -> 全 1 (0xFFFFFFFF)
      //   REM / REMU 除零 -> 被除数 (rs1)
      // vendored ELF 的 divu/remu test_8/9/10 明确以 rs2 == 0 驱动
      // (反汇编核验见 openspec/changes/chmem-mext-comb-datapath/specs/
      //  chmem-mext-datapath/spec.md 的 "Special-case coverage mapping").
      //
      // 表达式来源: tests/framework/test_chmem_arith_poc.cpp Part D (rev 4,
      // CH_MEM 24/24 PASS) —— 移植时勿改语义.
      auto zero32 = T(ch::core::ch_literal<0, kXlenBits>{});

      // 无符号乘积: 32x32 -> 64 bit (mul_op::result_width = M+N)
      auto mul_prod = rs1_val * rs2_val;
      auto r_mul    = bits<31, 0>(mul_prod);
      auto r_mulhu  = bits<63, 32>(mul_prod);

      // 有符号乘积: sext 到 64 后相乘 (128 bit 中间宽度), 其低 64 bit 即
      // 有符号积的 2 补码 —— 取 bits<63,32> 得高 32 位.
      auto mulh_prod   = sext<64>(rs1_val) * sext<64>(rs2_val);
      auto r_mulh     = bits<63, 32>(mulh_prod);
      auto mulhsu_prod = sext<64>(rs1_val) * zext<64>(rs2_val);
      auto r_mulhsu   = bits<63, 32>(mulhsu_prod);

      // safe divisor: 必须是 32-bit 运行时节点 (select), 不能是字面量 ——
      //   mod_op 结果宽度 = 除数宽度 (CppHDL/include/lnode/operators_ext.h:
      //   204-209), 且对编译期字面量除数有收缩特化 compute_bit_width(value-1)
      //   (CppHDL/include/core/operators_width.h:34-53).
      auto b_safe_u   = safe_divisor_u(rs2_val);
      auto b_safe_mag = safe_divisor_mag(rs2_val);

      // DIV (signed): sign = neg(a) ^ neg(b); mag = |a| / |b|; 除零 -> 全 1
      auto div_sign = (neg_flag(rs1_val) != neg_flag(rs2_val));
      auto div_mag  = abs32(rs1_val) / b_safe_mag;
      auto div_q    = select(div_sign, neg32(div_mag), div_mag);
      auto r_div    = select(ch_bool(rs2_val == zero32), all_ones, div_q);

      // DIVU (unsigned): 除零 -> 全 1
      auto r_divu = select(ch_bool(rs2_val == zero32), all_ones,
                           rs1_val / b_safe_u);

      // REM (signed): 符号跟随被除数; 除零 -> 被除数
      auto rem_mag = abs32(rs1_val) % b_safe_mag;
      auto rem_r   = select(neg_flag(rs1_val), neg32(rem_mag), rem_mag);
      auto r_rem   = select(ch_bool(rs2_val == zero32), rs1_val, rem_r);

      // REMU (unsigned): 除零 -> 被除数
      auto r_remu = select(ch_bool(rs2_val == zero32), rs1_val,
                           rs1_val % b_safe_u);

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

      // ── RV32M: 8 层 M-op select (追加在 11 层 base 树之后) ──────────
      // select 后写覆盖前写 → M-op 无条件赢过 base 的同 funct3 误捕.
      // 与 D2 的 f7_00 加固构成双保险 (互不依赖).
      result = select(is_mul,    r_mul,    result);
      result = select(is_mulh,   r_mulh,   result);
      result = select(is_mulhsu, r_mulhsu, result);
      result = select(is_mulhu,  r_mulhu,  result);
      result = select(is_div,    r_div,    result);
      result = select(is_divu,   r_divu,   result);
      result = select(is_rem,    r_rem,    result);
      result = select(is_remu,   r_remu,   result);

      // RESULT: LD/ST = byte_addr (rs1+imm) for dmem; otherwise ALU result.
      // Phase 6d.7 bugfix: the old code set RESULT = result where result was
      // is_lw-overwritten to rdata, making RESULT a combinational self-loop
      // (rd_data → rdata → RESULT) that codegen resolved to 0.
      auto addr_result = select(is_store || is_load, rs1_val + imm, result);
      n->operator()(KeyType::RD_DATA) = result;
      n->operator()(KeyType::RESULT) = addr_result;
      } else {
        // 单元级 PoC (m3_poc_alu_elaborate): 无 DECODED_INST → 输出零值
        n->operator()(KeyType::RD_DATA) = T(ch::core::ch_literal<0, 32>{});
        n->operator()(KeyType::RESULT)  = T(ch::core::ch_literal<0, 32>{});
      }
      }  // end if (n)
    });

    // -------------------------------------------------------------------
    // csr-chmem-pipeline-integration R3.2 [GREEN]: execute-LATE 阶段消费
    //   CSR_READ_VALUE, 优先于 IntAlu default RD_DATA。
    //
    // Canonical ordering (ADR-048): IntAlu 位置 6 晚于 CSR 位置 5, 但
    //   同一 stage 内 NORMAL phase 早于 LATE phase。所以 LATE 阶段读
    //   NORMAL 已写的 RD_DATA + CSR_READ_VALUE/VALID, select 覆写。
    //
    // 无 CSR plugin 时: 必须 has() guard 跳过本闭包, 否则 DAG 引用未驱动的
    //   CSR_READ_VALUE/VALID → cpphdl 5.052 sim tick SEGV
    //   (per CpuFactoryChmem 7-plugin 流水线 regression 教训).
    // -------------------------------------------------------------------
    pb.at_stage("execute", Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
        // 1) has() guard: 仅当 CSR plugin 已写才覆写 (无 CSR plugin 时
        //   整闭包 no-op, 零影响 → R3.2 单独无害, 0 退化)
        if (n->payloads().has(CsrKT::CSR_READ_VALID)) {
          // 2) 读 NORMAL 阶段已写的 RD_DATA 作为 fallback
          auto default_rd_data = n->operator()(KeyType::RD_DATA);
          // 3) 读 csr plugin 写的 CSR_READ_VALUE / CSR_READ_VALID
          //    ⚠ `CSR_READ_VALID` 的 value type 是 `uint_t<1>` (CH_MEM = ch_uint<1>)
          //    ⇒ 这里读到的是**运行期 DAG 节点**而不是 POD 快照
          //    (chmem-cpu-keys-modality 修复前它是 `Payload<bool>` ⇒ 恒为折叠常量
          //     ⇒ 下面的 select 条件恒定 ⇒ CSR 读值覆写 RD_DATA 通道语义为零)。
          auto csr_read_value  = n->operator()(CsrKT::CSR_READ_VALUE);
          auto csr_read_valid  = n->operator()(CsrKT::CSR_READ_VALID);
          // 4) select 树: 1-bit valid 选 CSR 读值, 否则 default
          //    条件是 ch_uint<1> 节点 ⇒ select 走节点级 mux (非字面量 mux)
          n->operator()(KeyType::RD_DATA) =
              select(csr_read_valid, csr_read_value, default_rd_data);
        }
      }
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

 private:
  // ══════════════════════════════════════════════════════════════════════
  // RV32M 组合式算术 helper (sign-magnitude; CppHDL 无 ch_int)
  //
  // 声明为类模板的 static 成员函数: 隐式 inline (模板 vague linkage),
  // 无 ODR 风险, 且作用域收敛在本类内 —— 本头文件被多个 TU 包含.
  //
  // 表达式来源: tests/framework/test_chmem_arith_poc.cpp Part D (rev 4,
  // CH_MEM 24/24 PASS) —— 与该 PoC 保持逐字一致.
  // ══════════════════════════════════════════════════════════════════════

  // 二补数取负: (~x) + 1. 结果宽度 = max(lhs, rhs) = kXlenBits, 不增长,
  // 故 neg32(0x80000000) == 0x80000000 (INT_MIN 幅值可容纳于无符号 32 bit).
  static T neg32(T x) {
    return (~x) + T(ch::core::ch_literal<1, kXlenBits>{});
  }

  // 符号位: bit[kXlenBits-1]
  static ch_bool neg_flag(T x) {
    return ch_bool(bits<kXlenBits - 1, kXlenBits - 1>(x) ==
                   ch_uint<1>(ch::core::ch_literal<1, 1>{}));
  }

  // 幅值: |x| (用无符号 32 bit 表示, INT_MIN 装得下, 无需 33 bit)
  static T abs32(T x) {
    return select(neg_flag(x), neg32(x), x);
  }

  // 无符号除/模的 safe divisor: b == 0 时用 1 代替.
  // 必须是 select 构成的 32-bit 运行时节点 —— mod_op 结果宽度 = 除数宽度
  // (CppHDL/include/lnode/operators_ext.h:204-209), 且对编译期字面量除数
  // 有收缩特化 compute_bit_width(value-1)
  // (CppHDL/include/core/operators_width.h:34-53).
  static T safe_divisor_u(T b) {
    T zero(ch::core::ch_literal<0, kXlenBits>{});
    T one(ch::core::ch_literal<1, kXlenBits>{});
    return select(ch_bool(b == zero), one, b);
  }

  // 有符号除/模的 safe divisor: 作用于幅值 |b|, |b| == 0 时用 1 代替.
  // (abs32(0) == 0, 故 abs32(b) == 0 等价于 b == 0)
  static T safe_divisor_mag(T b) {
    T zero(ch::core::ch_literal<0, kXlenBits>{});
    T one(ch::core::ch_literal<1, kXlenBits>{});
    auto b_mag = abs32(b);
    return select(ch_bool(b_mag == zero), one, b_mag);
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM
#endif  // CF_IP_CPU_ARCH_RISCV_INT_ALU_CHMEM_H