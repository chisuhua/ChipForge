// tests/cpu/test_cpu_chmem_alu_semantics.cpp
//
// 功能描述: CH_MEM RiscvIntAluPlugin 单元级语义锚 —— 真跑 int_alu_chmem.h 的
//           19 层 select 树 (11 base + 8 M-op), 断言 RD_DATA 数值.
//
// 作者: build agent
// 日期: 2026-10-20
//
// 背景 (chmem-mext-comb-datapath 验收发现的 known issue):
//   AGENTS.md "CH_MEM ALU 无单元级语义测试" —— 既有 ALU 测试的实况:
//     - m3_poc_alu_elaborate          : 只验 alu.v 非空, 无任何语义
//     - m3_poc_alu_byte_equal         : 比对两份**手写 C++ 镜像**
//                                        (tlm_alu_compute vs chmem_ref_alu),
//                                        **完全不跑 int_alu_chmem.h 的 select 树**
//     - m3_poc_5stage_byte_equal      : 同上 (tlm_alu_compute_op vs
//                                        chmem_alu_compute_op)
//   => 既有 "byte-equal" 是 C++ 镜像自比, 对真实 datapath 零覆盖.
//
//   tests/framework/test_chmem_arith_poc.cpp Part D 覆盖 8 条 M-op 表达式, 但那是
//   **裸 ch_uint 表达式**, 不经 RiscvIntAluPlugin => 不锚 gate / select 顺序 /
//   RD_DATA 写者链. 本文件补上这一层.
//
// 机制 (每 case 独立 context):
//   1. PipeBuilder.build() -> ALU 的 build() 调 at_stage("execute", NORMAL, closure)
//      -> at_stage 自动创建 "execute" 节点 (pipe_builder.h:116-118)
//   2. **elaborate() 之前**把 execute 节点的 payload 填成 literal 值
//      (RS1/RS2/PC + DecodedInst) -> closure 在 elaborate 期按该组值建 select 树 DAG
//   3. pb.create_simulator() + sim->tick() -> sim->get_value(RD_DATA) 取回数值
//
// 断言字段: **RD_DATA** (int_alu_chmem.h:250, = ALU 算术结果).
//   RESULT 是给 DMem 的字节地址 (select(is_store||is_load, rs1+imm, result)),
//   与 ALU 语义不等价, 本文件一律不断言 RESULT.
//
// 期望值来源: RISC-V ISA 规范独立手算 (每条注释给出推导). **禁止**调用
//   int_alu_chmem.h:384 的 compute_static —— 那是实现内的 C++ 镜像, 用了即自证.
//
// 用例分组:
//   G1 M-op 语义      : 8 条 M-op × 2 向量 = 16
//   G2 M-op 边界      : 除零 / INT_MIN÷-1 = 6
//   G3 判别互斥       : 每 funct3 验 funct7=0x01 走 M-op、0x00 走 base op = 8
//   G4 RV32I 回归     : 10 条 base R-type op
//   G5 I-type 不误 gate: 负立即数 I-type (bits[31:25] 是立即数, 不得被 f7_00 误伤) = 5
//   G6 顶层分支       : LUI / AUIPC = 2
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <core/context.h>
#include <core/uint.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/core/payload_common.h"
// 注意: decode_chmem.h **自身不 include** decoder_table.h —— 它依赖 includer
// 先提供 `cf::cpu::arch::riscv::opcode` 命名空间 (其 lambda 里有同名的局部
// 变量 `opcode`; 若命名空间未先声明, `opcode::OP_LUI` 会被解析成局部变量,
// 报 "wrong number of template arguments")。int_alu_chmem.h:74-75 即该顺序。
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/plugins/decode_chmem.h"
#include "ip/cpu/arch/riscv/int_alu_chmem.h"

namespace {

using ch::core::ch_bool;
using ch::core::ch_uint;

using AluT = ch_uint<32>;
using KT = cf::cpu::core::payload::keys<AluT, 32>;
using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<AluT>;

// ---------------------------------------------------------------------------
// RISC-V opcode (仅本测试用到的)
// ---------------------------------------------------------------------------
constexpr std::uint8_t kOpImm = 0x13;  // ADDI / SLTI / XORI / ...
constexpr std::uint8_t kOp    = 0x33;  // ADD / MUL / DIV / ...
constexpr std::uint8_t kLui   = 0x37;
constexpr std::uint8_t kAuipc = 0x17;

// ---------------------------------------------------------------------------
// eval_alu —— 在独立 context 里驱动一次真实 ALU select 树, 返回 RD_DATA
// ---------------------------------------------------------------------------
// 说明:
//   - 所有 ch_uint 成员用 ch_literal 真实初始化 (null impl -> 下游比较 SEGV,
//     对齐 cpu_factory_chmem.h:187-199 的既有做法)
//   - reads_rs2 决定 op2 = rs2_val 还是 imm (int_alu_chmem.h:187)
//   - funct7 是**译码后的 funct7 字段**: R-type 是 bits[31:25]; I-type 也是
//     bits[31:25] (即立即数的高 7 位) —— G5 组据此构造负立即数用例
// ---------------------------------------------------------------------------
std::uint32_t eval_alu(std::uint8_t opcode, std::uint8_t funct3,
                        std::uint8_t funct7, std::uint32_t rs1_val,
                        std::uint32_t rs2_val, std::uint32_t imm_val,
                        bool reads_rs2, std::uint32_t pc_val = 0x80000000u) {
  ch::core::context ctx("alu_semantics_ctx");
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  pb.register_plugin(
      std::make_unique<cf::cpu::arch::riscv::RiscvIntAluPlugin<AluT>>());
  REQUIRE(pb.build().has_value());

  // "execute" 节点由 at_stage() 自动创建 (PipeBuilder::at_stage)
  auto* n = pb.node_of_logic_stage("execute").get();
  REQUIRE(n != nullptr);

  // ── 操作数 (literal; CH_MEM 整数构造器建 literal 节点, 上游 47af57f 修复) ──
  n->operator()(KT::RS1) = AluT(static_cast<std::uint64_t>(rs1_val));
  n->operator()(KT::RS2) = AluT(static_cast<std::uint64_t>(rs2_val));
  n->operator()(KT::PC)  = AluT(static_cast<std::uint64_t>(pc_val));

  // ── DECODED_INST ──
  cf::cpu::plugins::DecodedInst d;
  d.opcode       = ch_uint<7>(static_cast<std::uint64_t>(opcode));
  d.funct3       = ch_uint<3>(static_cast<std::uint64_t>(funct3));
  d.funct7       = ch_uint<7>(static_cast<std::uint64_t>(funct7));
  d.rd_idx       = ch_uint<5>(ch_literal<1, 5>{});
  d.rs1_idx      = ch_uint<5>(ch_literal<2, 5>{});
  d.rs2_idx      = ch_uint<5>(ch_literal<3, 5>{});
  d.imm          = ch_uint<32>(static_cast<std::uint64_t>(imm_val));
  d.reads_rs1    = ch_bool(true);
  d.reads_rs2    = ch_bool(reads_rs2);
  d.writes_rd    = ch_bool(true);
  d.op_class     = ch_uint<8>(ch_literal<0, 8>{});
  d.instr_format = ch_uint<3>(ch_literal<0, 3>{});
  n->payloads().put(DecodePlugin::DECODED_INST, d);

  REQUIRE(pb.elaborate().has_value());

  auto sim = cf::plugin::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  sim->tick();

  const auto& rd = n->operator()(KT::RD_DATA);
  // sim->get_value 返回 sdata_type (explicit operator uint64_t)
  return static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(sim->get_value(rd)));
}

// ---------------------------------------------------------------------------
// 用例表
// ---------------------------------------------------------------------------
struct AluCase {
  std::uint8_t  opcode;
  std::uint8_t  funct3;
  std::uint8_t  funct7;
  std::uint32_t rs1;
  std::uint32_t rs2;
  std::uint32_t imm;
  bool          reads_rs2;
  std::uint32_t expected;
  const char*   label;
};

// 逐条跑一组用例 (每条独立 context)
void run_cases(const std::vector<AluCase>& cases) {
  for (const auto& tc : cases) {
    const std::uint32_t got =
        eval_alu(tc.opcode, tc.funct3, tc.funct7, tc.rs1, tc.rs2, tc.imm,
                 tc.reads_rs2);
    INFO(tc.label << " | opcode=0x" << std::hex << static_cast<int>(tc.opcode)
                  << " funct3=" << static_cast<int>(tc.funct3)
                  << " funct7=0x" << static_cast<int>(tc.funct7) << std::dec
                  << " rs1=" << tc.rs1 << " rs2=" << tc.rs2 << " imm=0x"
                  << std::hex << tc.imm << std::dec
                  << " reads_rs2=" << tc.reads_rs2);
    INFO("  期望 = " << tc.expected << " (0x" << std::hex << tc.expected
                     << std::dec << "), 实得 = " << got << " (0x" << std::hex
                  << got << std::dec << ")");
    REQUIRE(got == tc.expected);
  }
}

}  // namespace

// ===========================================================================
// G1 — 8 条 M-op 语义 (期望值取自 RISC-V ISA 规范, 独立手算)
// ===========================================================================
TEST_CASE("chmem_alu_semantics_mext_basic",
          "[cpu][chmem][alu][semantics][mext]") {
  run_cases({
      // MUL = (rs1*rs2)[31:0]
      {kOp, 0, 0x01, 3, 4, 0, true, 12u, "MUL(3,4)=12"},
      {kOp, 0, 0x01, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, true, 0x00000001u,
       "MUL(0xFFFFFFFF,0xFFFFFFFF): 0xFFFFFFFE00000001 低 32 位 = 1"},
      // MULH = (signed rs1 * signed rs2)[63:32]
      {kOp, 1, 0x01, 0xFFFFFFFEu, 3, 0, true, 0xFFFFFFFFu,
       "MULH(-2,3): 有符号积 -6 = 0xFFFFFFFFFFFFFFFA, 高 32 位 = 0xFFFFFFFF"},
      {kOp, 1, 0x01, 0x7FFFFFFFu, 0x7FFFFFFFu, 0, true, 0x3FFFFFFFu,
       "MULH(0x7FFFFFFF,0x7FFFFFFF): 积 0x3FFFFFFF00000001, 高 32 位"},
      // MULHSU = (signed rs1 * unsigned rs2)[63:32]
      {kOp, 2, 0x01, 0xFFFFFFFFu, 0x80000000u, 0, true, 0xFFFFFFFFu,
       "MULHSU(-1,0x80000000): -2^31 = 0xFFFFFFFF80000000, 高 32 位 = 0xFFFFFFFF"},
      {kOp, 2, 0x01, 2, 0x80000000u, 0, true, 0x1u,
       "MULHSU(2,0x80000000): 2^32 = 0x100000000, 高 32 位 = 1"},
      // MULHU = (unsigned rs1 * unsigned rs2)[63:32]
      {kOp, 3, 0x01, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, true, 0xFFFFFFFEu,
       "MULHU(0xFFFFFFFF,0xFFFFFFFF): 高 32 位 = 0xFFFFFFFE"},
      {kOp, 3, 0x01, 0x10000u, 0x10000u, 0, true, 0x1u,
       "MULHU(0x10000,0x10000): 积 0x100000000, 高 32 位 = 1"},
      // DIV = signed, 向零取整
      {kOp, 4, 0x01, 0xFFFFFFF2u, 4, 0, true, 0xFFFFFFFDu,
       "DIV(-14,4) = -3 (向零取整) = 0xFFFFFFFD"},
      {kOp, 4, 0x01, 7, 2, 0, true, 3u, "DIV(7,2) = 3"},
      // DIVU = unsigned
      {kOp, 5, 0x01, 14, 4, 0, true, 3u, "DIVU(14,4) = 3"},
      {kOp, 5, 0x01, 0xFFFFFFFFu, 2, 0, true, 0x7FFFFFFFu,
       "DIVU(0xFFFFFFFF,2) = 0x7FFFFFFF"},
      // REM = signed, 符号跟随被除数
      {kOp, 6, 0x01, 0xFFFFFFF2u, 4, 0, true, 0xFFFFFFFEu,
       "REM(-14,4) = -2 (符号跟随被除数) = 0xFFFFFFFE"},
      {kOp, 6, 0x01, 7, 2, 0, true, 1u, "REM(7,2) = 1"},
      // REMU = unsigned
      {kOp, 7, 0x01, 14, 4, 0, true, 2u, "REMU(14,4) = 2"},
      {kOp, 7, 0x01, 0xFFFFFFFFu, 2, 0, true, 1u, "REMU(0xFFFFFFFF,2) = 1"},
  });
}

// ===========================================================================
// G2 — M-op 边界 (RISC-V 规范特例)
// ===========================================================================
TEST_CASE("chmem_alu_semantics_mext_edge",
          "[cpu][chmem][alu][semantics][mext]") {
  run_cases({
      // 除零: DIV/DIVU -> 全 1; REM/REMU -> 被除数
      {kOp, 4, 0x01, 5, 0, 0, true, 0xFFFFFFFFu,
       "DIV(5,0) = -1 = 0xFFFFFFFF (RISC-V 规定)"},
      {kOp, 5, 0x01, 5, 0, 0, true, 0xFFFFFFFFu,
       "DIVU(5,0) = 0xFFFFFFFF (RISC-V 规定)"},
      {kOp, 6, 0x01, 5, 0, 0, true, 5u,
       "REM(5,0) = 5 (被除数, RISC-V 规定)"},
      {kOp, 7, 0x01, 5, 0, 0, true, 5u,
       "REMU(5,0) = 5 (被除数, RISC-V 规定)"},
      // 溢出: INT_MIN / -1 -> INT_MIN; INT_MIN % -1 -> 0
      {kOp, 4, 0x01, 0x80000000u, 0xFFFFFFFFu, 0, true, 0x80000000u,
       "DIV(INT_MIN,-1) = INT_MIN = 0x80000000 (溢出, RISC-V 规定)"},
      {kOp, 6, 0x01, 0x80000000u, 0xFFFFFFFFu, 0, true, 0u,
       "REM(INT_MIN,-1) = 0 (溢出, RISC-V 规定)"},
  });
}

// ===========================================================================
// G3 — 判别互斥 (核心价值)
//   同一 funct3 下, funct7=0x01 必须走 M-op, funct7=0x00 必须走 base op.
//   两者结果刻意选成**不同** => 任何误捕/漏捕都会被抓到.
//   (f=2/f=3 用 (4,12) 而非 (12,4): (12,4) 时 MULHSU 与 SLT 同为 0, 无法区分)
// ===========================================================================
TEST_CASE("chmem_alu_semantics_mext_vs_base",
          "[cpu][chmem][alu][semantics][mext]") {
  // (funct3, M-op 期望, base op 期望, 操作数, 说明)
  struct Pair {
    std::uint8_t  funct3;
    std::uint32_t mext_expected;
    std::uint32_t base_expected;
    std::uint32_t rs1;
    std::uint32_t rs2;
    const char*   label;
  };
  const std::vector<Pair> pairs = {
      {0, 48u, 16u, 12, 4, "f3=0: MUL(12,4)=48 vs ADD(12,4)=16"},
      {1, 0u, 192u, 12, 4, "f3=1: MULH(12,4)高32=0 vs SLL(12,4)=12<<4=192"},
      {2, 0u, 1u, 4, 12, "f3=2: MULHSU(4,12)高32=0 vs SLT(4,12)=1"},
      {3, 0u, 1u, 4, 12, "f3=3: MULHU(4,12)高32=0 vs SLTU(4,12)=1"},
      {4, 3u, 8u, 12, 4, "f3=4: DIV(12,4)=3 vs XOR(12,4)=8"},
      {5, 3u, 0u, 12, 4, "f3=5: DIVU(12,4)=3 vs SRL(12,4)=12>>4=0"},
      {6, 0u, 12u, 12, 4, "f3=6: REM(12,4)=0 vs OR(12,4)=12"},
      {7, 0u, 4u, 12, 4, "f3=7: REMU(12,4)=0 vs AND(12,4)=4"},
  };

  for (const auto& p : pairs) {
    const std::uint32_t mext = eval_alu(kOp, p.funct3, 0x01, p.rs1, p.rs2, 0,
                                        true);
    INFO(p.label << " [funct7=0x01 应走 M-op]");
    INFO("  M-op 期望 = " << p.mext_expected << ", 实得 = " << mext);
    REQUIRE(mext == p.mext_expected);

    const std::uint32_t base = eval_alu(kOp, p.funct3, 0x00, p.rs1, p.rs2, 0,
                                        true);
    INFO(p.label << " [funct7=0x00 应走 base op]");
    INFO("  base 期望 = " << p.base_expected << ", 实得 = " << base);
    REQUIRE(base == p.base_expected);

    // 两者必须不同 (否则本组用例无区分力)
    REQUIRE(mext != base);
  }
}

// ===========================================================================
// G4 — RV32I base R-type 回归 (funct7=0x00 / 0x20)
// ===========================================================================
TEST_CASE("chmem_alu_semantics_rv32i_base",
          "[cpu][chmem][alu][semantics][rv32i]") {
  run_cases({
      {kOp, 0, 0x00, 5, 3, 0, true, 8u, "ADD(5,3)=8"},
      {kOp, 0, 0x20, 10, 3, 0, true, 7u, "SUB(10,3)=7"},
      {kOp, 1, 0x00, 1, 4, 0, true, 16u, "SLL(1,4)=1<<4=16"},
      {kOp, 2, 0x00, 5, 10, 0, true, 1u, "SLT(5,10)=1 (有符号)"},
      {kOp, 3, 0x00, 5, 10, 0, true, 1u, "SLTU(5,10)=1 (无符号)"},
      {kOp, 4, 0x00, 0xFFu, 0x0Fu, 0, true, 0xF0u, "XOR(0xFF,0x0F)=0xF0"},
      {kOp, 5, 0x00, 0x100u, 4, 0, true, 0x10u, "SRL(0x100,4)=0x10"},
      {kOp, 5, 0x20, 0xFFFFFFF0u, 2, 0, true, 0xFFFFFFFCu,
       "SRA(-16,2) = -4 = 0xFFFFFFFC (算术右移)"},
      {kOp, 6, 0x00, 0xF0u, 0x0Fu, 0, true, 0xFFu, "OR(0xF0,0x0F)=0xFF"},
      {kOp, 7, 0x00, 0xF0u, 0x0Fu, 0, true, 0u, "AND(0xF0,0x0F)=0"},
  });
}

// ===========================================================================
// G5 — I-type 不得被 f7_00 gate 误伤
//   I-type 的 bits[31:25] 是**立即数**而非 funct7 (int_alu_chmem.h:148-150 注释)。
//   负立即数会让 bits[31:25] = 0x20 —— 若错误地把 f7_00 gate 加到 is_opimm 侧,
//   这些用例会失败. (op2 = imm, 因为 reads_rs2=false)
// ===========================================================================
TEST_CASE("chmem_alu_semantics_itype_not_gated",
          "[cpu][chmem][alu][semantics][rv32i]") {
  run_cases({
      // XORI(rs1=0xF0, imm=-4096): 指令 bits[31:25] = 0x20
      {kOpImm, 4, 0x20, 0xF0u, 0, 0xFFFFF000u, false, 0xFFFFF0F0u,
       "XORI(0xF0,-4096) = 0xF0 ^ 0xFFFFF000 = 0xFFFFF0F0 (bits[31:25]=0x20)"},
      // ANDI(rs1=0xF0, imm=-4096): bits[31:25] = 0x20
      {kOpImm, 7, 0x20, 0xF0u, 0, 0xFFFFF000u, false, 0x00u,
       "ANDI(0xF0,-4096) = 0xF0 & 0xFFFFF000 = 0x00 (0xFFFFF000 低 8 位为 0; bits[31:25]=0x20)"},
      // ORI(rs1=0xF0, imm=-4096): bits[31:25] = 0x20
      {kOpImm, 6, 0x20, 0xF0u, 0, 0xFFFFF000u, false, 0xFFFFF0F0u,
       "ORI(0xF0,-4096) = 0xF0 | 0xFFFFF000 = 0xFFFFF0F0 (bits[31:25]=0x20)"},
      // SLTI(rs1=5, imm=-1): 5 < -1 ? -> 0
      {kOpImm, 2, 0x20, 5, 0, 0xFFFFFFFFu, false, 0u,
       "SLTI(5,-1) = 0 (有符号 5 < -1 为假; bits[31:25]=0x20)"},
      // SLTIU(rs1=5, imm=-1): 5 <u 0xFFFFFFFF ? -> 1
      {kOpImm, 3, 0x20, 5, 0, 0xFFFFFFFFu, false, 1u,
       "SLTIU(5,-1) = 1 (无符号 5 < 0xFFFFFFFF; bits[31:25]=0x20)"},
      // SLLI(rs1=1, shamt=4): bits[31:25] = 0x00
      {kOpImm, 1, 0x00, 1, 0, 4, false, 16u, "SLLI(1,4) = 1<<4 = 16"},
  });
}

// ===========================================================================
// G6 — 顶层分支 (LUI / AUIPC) 未被 M-op 层破坏
// ===========================================================================
TEST_CASE("chmem_alu_semantics_top_level",
          "[cpu][chmem][alu][semantics][rv32i]") {
  run_cases({
      {kLui, 0, 0x00, 0, 0, 0xABCDE000u, false, 0xABCDE000u,
       "LUI(imm=0xABCDE000) = imm"},
      {kAuipc, 0, 0x00, 0, 0, 0x00001000u, false, 0x80001000u,
       "AUIPC(pc=0x80000000, imm=0x1000) = 0x80001000"},
  });
}

#endif  // CF_PLUGIN_USE_CH_MEM
