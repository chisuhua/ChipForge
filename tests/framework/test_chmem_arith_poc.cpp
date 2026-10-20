// tests/framework/test_chmem_arith_poc.cpp
//
// 功能描述: 2026-10-20 PoC — 隔离验证 CppHDL ch_uint<32> 真算术语义
//   (mul/div/mod/mulhu) + MulDivFsmPlugin result_reg 锁存时序.
//
// 作者: build agent
// 日期: 2026-10-20 (rev 3)
//
// 上下文:
//   - CHANGELOG.md:443 v0.10.3 Known Limitation 记 "ch 算术语义异常":
//       ch_uint<32>(14) / ch_uint<32>(4) = 1   (期望 3)
//       ch_uint<32>(3)  * ch_uint<32>(4) = 16  (期望 12)
//   - ip/cpu/plugins/mul_div_fsm_chmem.h:149-171 result_reg_/busy_reg_
//     用 ch_literal 占位 (12/3/33), 不含真算术.
//   - CppHDL HEAD 7d69f56 (2026-10-20) = codegen(ch_reg 无 next hold-state),
//     属 Verilator 后端; 本 PoC 跑 interpreter, 与之无关.
//
// 四部分 (rev 3):
//   A. 纯组合算术 PoC (两 binary) — 直接 ch_uint 算术 + ch::Simulator,
//      完全绕开 ch_reg / plugin 框架. 直接回答 "truncate 是否已修".
//      样板: CppHDL/tests/chlib/test_arithmetic.cpp:13-39.
//   B. MulDivFsmPlugin 时序 PoC (CH_MEM binary) — 验证 result_reg select
//      树在正确 tick 序列下锁存值; 对照 test_chmem_multi_cycle_fsm.cpp.
//   C. ch_reg + ch_uint 算术组合 PoC — 寄存器迭代 + 真算术 + select.
//   D. 组合式 8 条 M-op 全实现 PoC — 证明方案 A (int_alu_chmem.h select
//      树用 ch_uint 组合算式实现 M 扩展) 语义可行. 无 ch_int → sign-magnitude.
//
// rev 2 修正 (rev 1 于 commit 1f14f33 声称 "ch_reg lock race" 被确证):
//   rev 1 的 result==0 是 PoC tick 序列 bug — 在 WRITE_BACK 拍把 opcode
//   归零, 导致 is_mul/is_div=false, result_reg next 恒为 0. fixture
//   (test_chmem_multi_cycle_fsm.cpp:188) 在 WB 拍保持 opcode 非零, 断言
//   result==3 PASS. 故 "lock race" 结论错误, 本 rev 更正.
//
// Part D (rev 3 追加): 组合式 8 条 M-op 全实现 PoC — 证明方案 A
//   (在 int_alu_chmem.h select 树里用 ch_uint 组合算式实现 M 扩展) 可行.

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

// 避免 <ch.hpp> (含 <module.h> 非 inline build_hierarchical_name, 多 TU
// 会 link 冲突). 仅用 core + simulator 最小头集.
#include <core/context.h>
#include <core/reg.h>
#include <core/uint.h>
#include <core/operators_width.h>  // bits<MSB, LSB>(operand)
#include <simulator.h>

namespace {

using ch::core::bits;
using ch::core::ch_literal;
using ch::core::ch_uint;

// 帮助函数: 构造一个 context, 对组合表达式求值一次, 返回 uint64.
template <typename ExprFn>
std::uint64_t eval_combinational(const char* ctx_name, ExprFn&& fn) {
  auto ctx = std::make_unique<ch::core::context>(ctx_name);
  ch::core::ctx_swap guard(ctx.get());
  auto result = fn();
  ch::Simulator sim(ctx.get());
  sim.tick();
  return static_cast<std::uint64_t>(sim.get_value(result));
}

}  // namespace

// =========================================================================
// Part A: 纯组合算术 — 直接回答 CHANGELOG.md:443 "ch 算术语义异常" 是否仍在
// =========================================================================

TEST_CASE("chmem_arith_poc_div_comb",
          "[framework][chmem][arith-poc]") {
  const std::uint64_t v = eval_combinational("arith_div_comb", [] {
    ch_uint<32> a(ch_literal<14, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return a / b;
  });
  INFO("ch_uint<32>(14) / ch_uint<32>(4) = " << v
       << " (期望 3; CHANGELOG.md:443 曾记 =1)");
  REQUIRE(v == 3u);
}

TEST_CASE("chmem_arith_poc_mod_comb",
          "[framework][chmem][arith-poc]") {
  const std::uint64_t v = eval_combinational("arith_mod_comb", [] {
    ch_uint<32> a(ch_literal<14, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return a % b;
  });
  INFO("ch_uint<32>(14) % ch_uint<32>(4) = " << v << " (期望 2)");
  REQUIRE(v == 2u);
}

TEST_CASE("chmem_arith_poc_mul_comb",
          "[framework][chmem][arith-poc]") {
  // ch_uint<32> * ch_uint<32> = ch_uint<64> (mul_op::result_width = M+N)
  const std::uint64_t v = eval_combinational("arith_mul_comb", [] {
    ch_uint<32> a(ch_literal<3, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return a * b;
  });
  INFO("ch_uint<32>(3) * ch_uint<32>(4) = " << v
       << " (期望 12, 64-bit 中间; CHANGELOG.md:443 曾记 =16)");
  REQUIRE(v == 12u);
}

TEST_CASE("chmem_arith_poc_mulh_high_comb",
          "[framework][chmem][arith-poc]") {
  // MULH 语义: (a * b) >> 32 取高 32 位
  const std::uint64_t v = eval_combinational("arith_mulh_comb", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFFFu, 32>{});
    ch_uint<32> b(ch_literal<0xFFFFFFFFu, 32>{});
    auto prod = a * b;  // 64-bit
    return bits<63, 32>(prod);
  });
  // 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001; 高 32 = 0xFFFFFFFE
  INFO("MULHU(0xFFFFFFFF, 0xFFFFFFFF) 高 32 = " << v
       << " (期望 0xFFFFFFFE)");
  REQUIRE(v == 0xFFFFFFFEu);
}

// =========================================================================
// Part C: ch_reg + ch_uint 算术组合 — M-extension 真实现所需模式
//   (寄存器迭代 + 真算术 + select 树; 纯 CppHDL, 两 binary 均编译)
// =========================================================================

TEST_CASE("chmem_arith_poc_reg_accumulate",
          "[framework][chmem][arith-poc]") {
  // ch_reg 迭代累加: acc <<= acc + 1; tick N 次 → acc == N
  // (这是 M-extension radix-2 迭代里 "remainder/quotient 逐拍更新" 的最小原型)
  auto ctx = std::make_unique<ch::core::context>("arith_reg_acc");
  ch::core::ctx_swap guard(ctx.get());

  ch_uint<32> zero(ch_literal<0, 32>{});
  ch_uint<32> one(ch_literal<1, 32>{});
  ch::core::ch_reg<ch_uint<32>> acc(zero);
  acc <<= acc + one;

  ch::Simulator sim(ctx.get());
  for (std::uint64_t i = 1; i <= 10; ++i) {
    sim.tick();
    const std::uint64_t v =
        static_cast<std::uint64_t>(sim.get_value(acc));
    INFO("after tick " << i << ": acc = " << v);
    REQUIRE(v == i);
  }
}

TEST_CASE("chmem_arith_poc_reg_shift_add_mul",
          "[framework][chmem][arith-poc]") {
  // shift-add 乘法 (真算术 + ch_reg 迭代 + select):
  //   prod <<= prod + (a[0] ? b : 0); a >>= 1; b <<= 1;   (32 拍)
  // 3 * 4 = 12. 这正是 M-extension MUL 的 radix-2 部分积累积原型.
  auto ctx = std::make_unique<ch::core::context>("arith_reg_sa_mul");
  ch::core::ctx_swap guard(ctx.get());

  ch_uint<32> zero(ch_literal<0, 32>{});
  ch_uint<32> one(ch_literal<1, 32>{});
  ch_uint<1> one1(ch_literal<1, 1>{});
  ch::core::ch_reg<ch_uint<32>> prod(zero);
  ch::core::ch_reg<ch_uint<32>> a(ch_literal<3, 32>{});
  ch::core::ch_reg<ch_uint<32>> b(ch_literal<4, 32>{});

  auto a_bit0 = bits<0, 0>(a);
  prod <<= prod + ch::core::select(ch::core::ch_bool(a_bit0 == one1), b, zero);
  a <<= a >> one;
  b <<= b << one;

  ch::Simulator sim(ctx.get());
  sim.tick(32);
  const std::uint64_t v = static_cast<std::uint64_t>(sim.get_value(prod));
  INFO("shift-add 3*4 = " << v << " (期望 12)");
  REQUIRE(v == 12u);
}

// =========================================================================
// Part D: 组合式 8 条 M-op 全实现 — 证明方案 A (int_alu_chmem.h select 树
//   用 ch_uint 组合算式实现 M 扩展) 语义可行. CppHDL 无 ch_int, 符号运算
//   走 sign-magnitude (与 TLM mul_div_fsm.h 同法).
// =========================================================================

namespace {

using ch::core::ch_bool;
using ch::core::ch_uint;
using ch::core::select;
using ch::core::sext;
using ch::core::zext;

ch_uint<32> neg32(ch_uint<32> x) {
  return (~x) + ch_uint<32>(ch_literal<1, 32>{});
}
ch_bool neg_flag(ch_uint<32> x) {
  return ch_bool(bits<31, 31>(x) == ch_uint<1>(ch_literal<1, 1>{}));
}
ch_uint<32> abs32(ch_uint<32> x) {
  return select(neg_flag(x), neg32(x), x);
}

}  // namespace

TEST_CASE("chmem_arith_poc_mext_mul", "[framework][chmem][arith-poc]") {
  // MUL = (a*b) 低 32
  const std::uint64_t v = eval_combinational("mext_mul", [] {
    ch_uint<32> a(ch_literal<3, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return bits<31, 0>(a * b);
  });
  INFO("MUL(3,4) = " << v << " (期望 12)");
  REQUIRE(v == 12u);
}

TEST_CASE("chmem_arith_poc_mext_mulhu", "[framework][chmem][arith-poc]") {
  // MULHU = (a*b) 高 32 (无符号)
  const std::uint64_t v = eval_combinational("mext_mulhu", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFFFu, 32>{});
    ch_uint<32> b(ch_literal<0xFFFFFFFFu, 32>{});
    return bits<63, 32>(a * b);
  });
  INFO("MULHU(0xFFFFFFFF,0xFFFFFFFF) = " << v << " (期望 0xFFFFFFFE)");
  REQUIRE(v == 0xFFFFFFFEu);
}

TEST_CASE("chmem_arith_poc_mext_mulh", "[framework][chmem][arith-poc]") {
  // MULH = (signed a * signed b) 高 32: sext 到 64 相乘, 取 [63:32]
  const std::uint64_t v = eval_combinational("mext_mulh", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFFEu, 32>{});  // -2
    ch_uint<32> b(ch_literal<3, 32>{});
    auto p = sext<64>(a) * sext<64>(b);  // 128-bit; 低 64 = 有符号积
    return bits<63, 32>(p);
  });
  INFO("MULH(-2,3) = " << v << " (期望 0xFFFFFFFF)");
  REQUIRE(v == 0xFFFFFFFFu);
}

TEST_CASE("chmem_arith_poc_mext_mulhsu", "[framework][chmem][arith-poc]") {
  // MULHSU = (signed a * unsigned b) 高 32: sext(a) * zext(b)
  const std::uint64_t v = eval_combinational("mext_mulhsu", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFFFu, 32>{});  // -1 signed
    ch_uint<32> b(ch_literal<0x80000000u, 32>{});  // 2^31 unsigned
    auto p = sext<64>(a) * zext<64>(b);
    return bits<63, 32>(p);
  });
  // -1 * 2^31 = -2^31 = 0xFFFFFFFF80000000; 高 32 = 0xFFFFFFFF
  INFO("MULHSU(-1,0x80000000) = " << v << " (期望 0xFFFFFFFF)");
  REQUIRE(v == 0xFFFFFFFFu);
}

TEST_CASE("chmem_arith_poc_mext_divu", "[framework][chmem][arith-poc]") {
  const std::uint64_t v = eval_combinational("mext_divu", [] {
    ch_uint<32> a(ch_literal<14, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return a / b;
  });
  INFO("DIVU(14,4) = " << v << " (期望 3)");
  REQUIRE(v == 3u);
}

TEST_CASE("chmem_arith_poc_mext_remu", "[framework][chmem][arith-poc]") {
  const std::uint64_t v = eval_combinational("mext_remu", [] {
    ch_uint<32> a(ch_literal<14, 32>{});
    ch_uint<32> b(ch_literal<4, 32>{});
    return a % b;
  });
  INFO("REMU(14,4) = " << v << " (期望 2)");
  REQUIRE(v == 2u);
}

TEST_CASE("chmem_arith_poc_mext_div", "[framework][chmem][arith-poc]") {
  // DIV = sign-magnitude: sign = a^b, |a|/|b|, 再补符号; 除零 → 全 1
  const std::uint64_t v = eval_combinational("mext_div", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFF2u, 32>{});  // -14
    ch_uint<32> b(ch_literal<4, 32>{});
    ch_uint<32> zero(ch_literal<0, 32>{});
    ch_uint<32> allones(ch_literal<0xFFFFFFFFu, 32>{});
    auto sign = (neg_flag(a) != neg_flag(b));
    auto mag = abs32(a) / abs32(b);
    auto q = select(sign, neg32(mag), mag);
    return select(b == zero, allones, q);
  });
  INFO("DIV(-14,4) = " << v << " (期望 0xFFFFFFFD = -3)");
  REQUIRE(v == 0xFFFFFFFDu);
}

TEST_CASE("chmem_arith_poc_mext_rem", "[framework][chmem][arith-poc]") {
  // REM = sign 跟随被除数; 除零 → 被除数
  const std::uint64_t v = eval_combinational("mext_rem", [] {
    ch_uint<32> a(ch_literal<0xFFFFFFF2u, 32>{});  // -14
    ch_uint<32> b(ch_literal<4, 32>{});
    ch_uint<32> zero(ch_literal<0, 32>{});
    auto mag = abs32(a) % abs32(b);
    auto r = select(neg_flag(a), neg32(mag), mag);
    return select(b == zero, a, r);
  });
  INFO("REM(-14,4) = " << v << " (期望 0xFFFFFFFE = -2)");
  REQUIRE(v == 0xFFFFFFFEu);
}

// =========================================================================
// Part B: MulDivFsmPlugin result_reg 锁存时序 (CH_MEM binary only)
// =========================================================================
#ifdef CF_PLUGIN_USE_CH_MEM

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/plugin_exception.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/multi_cycle_fsm.h"

#include "ip/cpu/arch/riscv/mul_div_fsm.h"
#include "ip/cpu/plugins/mul_div_fsm_chmem.h"

namespace cfcpu = cf::cpu::arch::riscv;

namespace {

// ArithPoCFix: 复用 test_chmem_multi_cycle_fsm.cpp::MulDivFsmFixture 模式
struct ArithPoCFix {
  ch::core::context ctx;
  std::unique_ptr<cf::plugin::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  cfcpu::MulDivFsmPlugin<std::uint32_t>* plugin = nullptr;

  ArithPoCFix()
      : ctx("arith_poc_ctx"),
        pb(std::make_unique<cf::plugin::PipeBuilder>(&ctx)) {
    ch::core::ctx_swap guard(&ctx);
    auto p = std::make_unique<cfcpu::MulDivFsmPlugin<std::uint32_t>>();
    plugin = p.get();
    plugin->set_context(&ctx);
    pb->register_plugin(std::move(p));
    pb->build();
    pb->elaborate();
    (void)plugin->opcode();
    sim = cf::plugin::auto_throw(pb->create_simulator());
    sim->reset();
  }

  void set_operands(std::uint64_t a, std::uint64_t b) {
    sim->set_input_value(plugin->rs1(), a);
    sim->set_input_value(plugin->rs2(), b);
  }
  void drive(std::uint64_t op) {
    sim->set_input_value(plugin->opcode(), op);
    sim->tick();
  }
  std::uint64_t result() {
    return static_cast<std::uint64_t>(sim->get_value(plugin->result_out()));
  }
};

}  // namespace

TEST_CASE("chmem_arith_poc_fsm_mul_result",
          "[framework][chmem][arith-poc]") {
  ArithPoCFix f;
  f.set_operands(3, 4);
  // IDLE+MUL → MULTIPLY; MULTIPLY → WB (keep opcode=1); WB → IDLE latch result
  f.drive(1);
  f.drive(1);
  f.drive(1);
  INFO("MulDivFsmPlugin result_reg (MUL path) = " << f.result()
       << " (期望 12 — ch_literal 占位, 验证 select+lock 时序)");
  REQUIRE(f.result() == 12u);
}

TEST_CASE("chmem_arith_poc_fsm_div_result",
          "[framework][chmem][arith-poc]") {
  ArithPoCFix f;
  f.set_operands(14, 4);
  // mirror test_chmem_multi_cycle_fsm.cpp:188 (35 drives, keep opcode at WB)
  f.drive(2);                                  // tick1  → DIVIDE
  for (std::size_t i = 0; i < 32; ++i) {       // tick2..33
    f.drive(0);
  }
  f.drive(0);                                  // tick34 → WRITE_BACK
  f.drive(2);                                  // tick35 → IDLE; latch result
  INFO("MulDivFsmPlugin result_reg (DIV path) = " << f.result()
       << " (期望 3 — ch_literal 占位)");
  REQUIRE(f.result() == 3u);
}

#endif  // CF_PLUGIN_USE_CH_MEM
