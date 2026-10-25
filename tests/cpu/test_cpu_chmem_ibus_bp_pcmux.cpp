// tests/cpu/test_cpu_chmem_ibus_bp_pcmux.cpp
//
// chmem-bp-btb-gshare-poc Task 2/3/4 —— IBus 侧 **PC-mux 五级链** +
// **门控式 flush** 的单元级锚。
//
// ★ 为什么必须有这个文件（branchbench 端到端判据不够）：
//   ① branchbench 每次 ~50s ⇒ 不能当快速回归；
//   ② 端到端校验和判据只能**整体**判红，无法隔离「哪条臂 / 哪个门控分量」出错
//      （实测：把 `spec_ok_buf` 误与 `pred_eff` 相与 ⇒ 校验和 93≠103，
//        但从数字上看不出是哪个分量）；
//   ③ **FLUSH 门控是 DUT 内部量**，端到端只能间接覆盖。
//   ⇒ 本文件用**真实的 IBusPlugin** + 一个 fake 生产者，把 5 条臂与门控的
//     **运行期值**逐条断出来（单点删除任一臂 / 任一门控分量 ⇒ 必 RED）。
//
// ★ 时序（本文件所有期望值的推导依据；已逐条实测核对）：
//   - `next_pc(t)` = branch LATE 第 t 拍算出的 pc_reg 下一拍值；
//   - fetch NORMAL 第 t 拍发布的 `BP_PC` = pc_val(t) = pc_reg(t) = next_pc(t-1)
//     ⇒ 主机读到的是**上一拍**的 mux 结果；
//   - 纠错记录 bufs 在 branch LATE 第 t 拍写入 ⇒ 第 t+1 拍的 fetch 读到
//     ⇒ `FLUSH`(t) = (br_taken(t-1) & !case_c(t-1)) | case_a(t-1)；
//   - 主机「tick k 次后读到的值」= 该 payload 在**第 k 拍**的信号值。
//
// ★ 情形定义（判定用**上一拍**的预测 vs **本拍**的实际结果，二者同指一条指令）：
//   A: 预测 taken / 实际 not-taken        ⇒ 纠回 fall-through (pc_lag+4) + flush
//   B: 预测 not-taken / 实际 taken        ⇒ 纠到实际目标 + flush
//   C: 预测 taken / 实际 taken / 目标一致   ⇒ 投机正确 ⇒ pc+4 + **不 flush**
//   E: 预测 not-taken / 实际 not-taken    ⇒ 无错路 ⇒ pc+4 + 不 flush
//   F: 预测 taken / 实际 taken / 目标不同   ⇒ 错路 ⇒ 纠到实际目标 + flush
//      （★ F 是设计简报**未列**的规格缺口，由本实现补）
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <core/reg.h>
#include <core/uint.h>
#include <simulator.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/branch_chmem.h"
#include "ip/cpu/plugins/branch_predictor_chmem.h"
#include "ip/cpu/plugins/ibus_chmem.h"

namespace {

using ch::core::ch_bool;
using ch::core::ch_uint;

using IBus = cf::cpu::plugins::IBusPlugin<ch::core::ch_uint<32>>;
using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<ch::core::ch_uint<32>>;
using BrPlugin = cf::cpu::plugins::BranchPlugin<ch::core::ch_uint<32>>;

/// 测试常量
constexpr std::uint32_t kInitPc = 0x1000;  ///< 初始 PC
constexpr std::uint32_t kPredTgt = 0x2000;  ///< BP 预测目标 P
constexpr std::uint32_t kActTgt = 0x3000;   ///< 实际分支目标 T（P≠T ⇒ 触发情形 F）

/// 一段脚本：tick < end_tick 时用本段的取值（末段无上界）
struct Seg {
  std::uint32_t end_tick = 1000;
  bool br_taken = false;         ///< 本拍「实际」分支方向（★ **已被 flush 掩码后**的值，
                                  ///<   与真实 branch plugin 的 `BRANCH_TAKEN` 同口径）
  std::uint32_t br_target = 0;   ///< 本拍「实际」分支目标
  bool flushed = false;          ///< ★ 本拍指令**是否被 flush 抑制**（= 错路指令）
                                  ///<   （真实 branch plugin 写 `BRANCH_FLUSHED`）
  bool pred_valid = false;       ///< 本拍 BP 预测「有效」
  bool pred_taken = false;       ///< 本拍 BP 预测「方向 = taken」
  std::uint32_t pred_target = 0; ///< 本拍 BP 预测「目标」
};

/// ★ fake 生产者：一个插件同时扮演「BranchPlugin」与「BP 预测器」两个角色。
///   - fetch  NORMAL: 写 `BP_PRED_VALID` / `BP_PRED_TAKEN` / `BP_PRED_TARGET`
///   - branch NORMAL: 写 `BRANCH_TAKEN` / `BRANCH_TARGET`
///   ★ 取值都是**随 tick 变化的活表达式**（tick 计数寄存器驱动）—— CH_MEM 里
///     at_stage 闭包只在 elaboration 跑一次 ⇒ 想在运行期改值 MUST 让 payload
///     装的是**活节点**（写成常量则只在 elaboration 定值 ⇒ 变不了）。
///   ★ branch payload 的生产者**必需**：否则 branch node 的 cell 为 null ⇒
///     IBus 的 mux 操作数为 null ⇒ Simulator 构造期 SIGSEGV。
class ScriptDriver : public cf::plugin::PluginBase {
 public:
  /// ★ `with_bp=false` ⇒ **不写** `BP_PRED_*` ⇒ 等价于「BP 插件未注册」
  ///   （IBus 侧 `payloads().has(BP_PRED_VALID)` 为 false ⇒ 预测臂恒不生效）
  ScriptDriver(std::vector<Seg> segs, bool with_bp)
      : segs_(std::move(segs)), with_bp_(with_bp) {
    if (segs_.empty()) throw std::runtime_error("ScriptDriver: empty segs");
  }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ 注册在 **fetch NORMAL** —— 写 BP 预测 payload
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (!n) return;
      if (!with_bp_) return;  // ★ 「BP 未注册」⇒ payload 缺失 ⇒ 预测臂恒不生效
      n->operator()(cf::cpu::plugins::BP_PRED_VALID) = cf::plugin::uint_t<1>(
          ch_uint<1>(pick_bool([](const Seg& s) { return s.pred_valid; })));
      n->operator()(cf::cpu::plugins::BP_PRED_TAKEN) = cf::plugin::uint_t<1>(
          ch_uint<1>(pick_bool([](const Seg& s) { return s.pred_taken; })));
      n->operator()(cf::cpu::plugins::BP_PRED_TARGET) = cf::plugin::uint_t<32>(
          pick_u32([](const Seg& s) { return s.pred_target; }));
    });
    // ★ 注册在 **branch NORMAL** —— 写「实际」分支结果
    //   （IBus 的 branch LATE 读它们 ⇒ LATE > NORMAL 已保证顺序）
    pb.at_stage("branch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("branch").get();
      if (!n) return;
      n->operator()(BrPlugin::get_branch_taken_key()) =
          pick_bool([](const Seg& s) { return s.br_taken; });
      n->operator()(RvKey::BRANCH_TARGET) = cf::plugin::uint_t<32>(
          pick_u32([](const Seg& s) { return s.br_target; }));
      // ★★ `BRANCH_FLUSHED` —— 「本拍指令是错路指令（副作用已被抑制）」
      //   ★ 真实 branch plugin 会写它；IBus 的情形判定**依赖**它排除错路指令
      //     （critic 十一轮 B-2）⇒ 夹具 MUST 提供，否则测不到那条路径。
      n->operator()(BrPlugin::BRANCH_FLUSHED) =
          pick_bool([](const Seg& s) { return s.flushed; });
      // ── tick 计数自增 ──
      const ch_uint<32> tv(tick_);
      tick_ <<= tv + ch_uint<32>(static_cast<std::uint32_t>(1));
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  /// 取「当前 tick 落在哪一段」的**活**布尔表达式
  template <typename Pick>
  ch_bool pick_bool(Pick pick) const {
    ch_bool acc(pick(segs_.back()));
    for (std::size_t i = segs_.size() - 1; i-- > 0;) {
      const ch_uint<32> c(tick_);
      const ch_uint<32> thresh(static_cast<std::uint32_t>(segs_[i].end_tick));
      acc = select(c < thresh, ch_bool(pick(segs_[i])), acc);
    }
    return acc;
  }

  /// 取「当前 tick 落在哪一段」的**活**32 位表达式
  template <typename Pick>
  ch_uint<32> pick_u32(Pick pick) const {
    ch_uint<32> acc(static_cast<std::uint32_t>(pick(segs_.back())));
    for (std::size_t i = segs_.size() - 1; i-- > 0;) {
      const ch_uint<32> c(tick_);
      const ch_uint<32> thresh(static_cast<std::uint32_t>(segs_[i].end_tick));
      acc = select(c < thresh,
                   ch_uint<32>(static_cast<std::uint32_t>(pick(segs_[i]))), acc);
    }
    return acc;
  }

  std::vector<Seg> segs_;
  bool with_bp_ = true;
  ch::core::ch_reg<ch_uint<32>> tick_{
      ch_uint<32>(ch::core::ch_literal<0, 32>{}), "pcmux_test_tick"};
};

/// 读 payload cell 的**运行期值**（32b）
std::uint64_t read_u32(ch::Simulator& sim, const ch::core::ch_uint<32>& cell) {
  return static_cast<std::uint64_t>(
      sim.get_signal_value(ch::core::ch_uint<32>(cell.impl())));
}

/// 读 payload cell 的**运行期值**（ch_bool ⇒ 用 make_bool 复用同一 impl 指针，
///   不新建 op 节点）
std::uint64_t read_bool(ch::Simulator& sim, const ch::core::ch_bool& cell) {
  ch::core::ch_uint<1> u = ch::core::ch_uint<1>::make_bool(cell);
  return static_cast<std::uint64_t>(sim.get_signal_value(u));
}

/// 一个场景的观测结果
struct Trace {
  std::vector<std::uint64_t> pc;     ///< 每拍的 `BP_PC`（= 上一拍的 mux 结果）
  std::vector<std::uint64_t> flush;  ///< 每拍的 `FLUSH`（门控式）
};

/// 起一套「真实 IBusPlugin + fake 生产者」的环境并跑 `n` 拍
Trace run(ch::core::context& ctx, std::vector<Seg> segs, bool with_bp, int n) {
  auto pb = std::make_unique<cf::plugin::PipeBuilder>(&ctx);
  auto ibus = std::make_unique<IBus>();
  ibus->set_initial_pc(ch_uint<32>(static_cast<std::uint32_t>(kInitPc)));
  pb->register_plugin(std::move(ibus));
  pb->register_plugin(std::make_unique<ScriptDriver>(std::move(segs), with_bp));
  pb->build();
  if (!pb->elaborate().has_value()) {
    throw std::runtime_error("elaborate failed");
  }
  auto sim = cf::plugin::auto_throw(pb->create_simulator());
  // ★ **不调用 sim->reset()** —— CppHDL 的 reset() 会把每个 ch_reg 复位成
  //   `rst_val`（本项目用 ch_reg(value, name) 构造 ⇒ rst_val = nullptr ⇒ 0），
  //   于是 `pc` 会被清 0，`set_initial_pc` 形同虚设。
  //   （同一事实见 tests/cpu/test_cpu_chmem_vendored_elf.cpp:156 的注释。）

  auto* fnode = pb->node_of_logic_stage("fetch").get();
  if (!fnode) throw std::runtime_error("no fetch node");

  Trace tr;
  for (int i = 0; i < n; ++i) {
    sim->tick();
    tr.pc.push_back(read_u32(*sim, ch::core::ch_uint<32>(
                                  fnode->operator()(cf::cpu::plugins::BP_PC).impl())));
    tr.flush.push_back(read_bool(*sim, fnode->operator()(IBus::FLUSH)));
  }
  return tr;
}

/// 断言序列相等（失败时打印实际序列）
void expect_seq(const std::vector<std::uint64_t>& got,
                const std::vector<std::uint64_t>& want, const char* what) {
  INFO(what << " got=");
  for (auto v : got) INFO("  " << v);
  REQUIRE(got.size() == want.size());
  for (std::size_t i = 0; i < want.size(); ++i) {
    INFO(what << " index " << i);
    REQUIRE(got[i] == want[i]);
  }
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// R1 | BP **关闭** + 分支 not-taken ⇒ 逐拍顺序取指 + 恒不 flush
//     ★ 这是「BP 关闭 ⇒ **零退化**」的锚：PC 只受 seq 臂驱动，FLUSH 恒 0。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R1_bp_off_not_taken_is_zero_regression",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r1");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = false, .br_target = kActTgt,
                  .flushed = false, .pred_valid = false, .pred_taken = false,
                  .pred_target = kPredTgt}},
                /*with_bp=*/false, 6);
  // R_k = next_pc(k-1) = pc_reg(k) = init + 4k
  expect_seq(tr.pc,
             {kInitPc + 4, kInitPc + 8, kInitPc + 12, kInitPc + 16, kInitPc + 20,
              kInitPc + 24},
             "R1 pc");
  expect_seq(tr.flush, {0, 0, 0, 0, 0, 0}, "R1 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R2 | BP **关闭** + 分支 taken ⇒ PC 恒 = 实际目标 + FLUSH 恒 = taken_buf
//     ★ 零退化的另一半：**没有** BP 时 flush 语义与改动前**逐位相同**。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R2_bp_off_taken_is_zero_regression",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r2");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = false, .pred_taken = false,
                  .pred_target = kPredTgt}},
                /*with_bp=*/false, 6);
  expect_seq(tr.pc, {kActTgt, kActTgt, kActTgt, kActTgt, kActTgt, kActTgt},
             "R2 pc");
  expect_seq(tr.flush, {1, 1, 1, 1, 1, 1}, "R2 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R3 | **情形 A**（预测 taken / 实际 not-taken）
//     ⇒ PC 纠回 **fall-through**（pc_lag+4 = init+4），**不是**投机目标 P
//     ⇒ FLUSH = 1（错路指令必须被抑制 ⇒ `mispredict_a_buf` 分量）
//     ★ 变异：删掉 case_a 臂 ⇒ pc 变 P+4 ⇒ RED；
//              删掉 `| mispredict_a_buf` ⇒ flush 变 0 ⇒ RED。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R3_case_A_falls_back_and_flushes",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r3");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = false, .br_target = kActTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 6);
  // next_pc(0)=P（投机）; next_pc(1)=init+4（情形 A 纠回）; next_pc(2)=P+4 …
  expect_seq(tr.pc,
             {kPredTgt, kInitPc + 4, kPredTgt + 4, kInitPc + 8, kPredTgt + 8,
              kInitPc + 12},
             "R3 pc");
  // FLUSH(t) = (taken(t-1) & !case_c(t-1)) | case_a(t-1)
  //   taken 恒 0 ⇒ 只由 case_a 驱动 ⇒ 第 2 拍起恒 1
  expect_seq(tr.flush, {0, 1, 1, 1, 1, 1}, "R3 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R4 | **情形 B**（预测 not-taken / 实际 taken ⇒ 暖机主路径）
//     ⇒ PC 纠到**实际目标** T（不是 fall-through init+4）
//     ⇒ FLUSH = 1（fall-through 是错路 ⇒ 必须抑制）
//     ★ 变异：删掉 br_taken 臂 ⇒ pc 变 init+4 ⇒ RED。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R4_case_B_redirects_to_actual_target",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r4");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = false,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 6);
  expect_seq(tr.pc, {kActTgt, kActTgt, kActTgt, kActTgt, kActTgt, kActTgt},
             "R4 pc");
  expect_seq(tr.flush, {1, 1, 1, 1, 1, 1}, "R4 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R5 | **情形 C**（预测 taken / 实际 taken / 目标一致 ⇒ 投机被证明正确）
//     ⇒ PC = **bp_next**（= 当前取指指令的投机；本夹具下 = 预测目标 P）
//     ⇒ FLUSH = **0**（投机路径是对的 ⇒ 无需抑制错路 ⇒ 门控生效）
//     ★★ 门控式 flush 的核心锚：
//       变异「去掉 `!spec_ok`」或「退回 FLUSH = taken_buf」⇒ flush 变 1 ⇒ RED；
//       变异「删掉 case_c 臂」⇒ pc 变 br_target ⇒ RED；
//       变异「case_c 臂退回 `pc_plus_4`」（= 简报 §2.1.2 的字面写法）
//         ⇒ pc 变 P+4 ⇒ RED ★★（这是 critic 第十轮 B-1 的锚）
//     ★ 本夹具的预测是**常量**（「任何指令都被预测 taken→P」）⇒ 情形 C 下
//       bp_next = P ⇒ PC 自环于 P（不是顺序推进）—— ★ 这正是
//       「case_c MUST 保留 bp_next」的语义后果。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R5_case_C_speculation_correct_no_flush",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r5");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  // 预测目标 == 实际目标 ⇒ 情形 C
  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = true, .br_target = kPredTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 6);
  expect_seq(tr.pc,
             {kPredTgt, kPredTgt, kPredTgt, kPredTgt, kPredTgt, kPredTgt},
             "R5 pc");
  // 第 1 拍 case_c 尚未成立（还没有上一拍记录）⇒ flush=1；
  // 第 2 拍起 spec_ok=1 ⇒ 门控生效 ⇒ flush=0
  expect_seq(tr.flush, {1, 0, 0, 0, 0, 0}, "R5 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R6 | **情形 F**（预测 taken 到 P / 实际 taken 到 T≠P ⇒ ★ 简报未列的规格缺口）
//     ⇒ PC 纠到**实际目标** T（**不是** P+4 —— 目标不一致 ⇒ 投机是错路）
//     ⇒ FLUSH = **1**（错路必须被抑制；**不可**被 case_c 门控掉）
//     ★★ 与 R5 对照即证明「case_c 必须比较目标」：
//       变异「case_c 忽略目标一致性」⇒ pc 变 P+4 且 flush 变 0 ⇒ RED。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R6_case_F_target_mismatch_flushes",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r6");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  // 预测目标 P ≠ 实际目标 T ⇒ 情形 F
  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 6);
  expect_seq(tr.pc, {kActTgt, kActTgt, kActTgt, kActTgt, kActTgt, kActTgt},
             "R6 pc");
  expect_seq(tr.flush, {1, 1, 1, 1, 1, 1}, "R6 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R7 | `spec_ok` MUST **只**等于情形 C，**不可**再与 `pred_eff` 相与
//     ★ `pred_eff` 是「**本拍**对 pc_reg(N) 的预测」，而 `case_c` 讲的是
//       「**上一拍**的预测是否被证明正确」—— 两者是**不同指令**的条件。
//     ★ 误与 ⇒ 误清 spec_ok ⇒ 过度 flush ⇒ 抑制合法指令 ⇒ 校验和错
//       （端到端实测 m0 93≠103）。本用例用**时变**脚本制造
//       「case_c=1 但 pred_eff=0」的时刻 ⇒ 两种实现的 flush 序列**不同**。
//     ★ 脚本: 第 0~1 拍预测 taken（⇒ case_c 成立），第 2 拍起预测 not-taken
//       （⇒ pred_eff=0 但 case_c 仍成立）⇒ 第 3 拍的 flush 暴露差异。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R7_spec_ok_is_case_c_only",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r7");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx,
                {{.end_tick = 2u, .br_taken = true, .br_target = kPredTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt},
                 {.end_tick = 1000u, .br_taken = true, .br_target = kPredTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = false,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 6);
  // next_pc(0)=P; next_pc(1)=P（case_c ⇒ 保留 bp_next；本夹具的预测是常量
  //   ⇒ bp_next=P）；next_pc(2)=P+4（case_c，但本拍 pred_eff=0 ⇒ bp_next=pc+4）；
  // next_pc(3)=P（case_b：上一拍预测变 not-taken ⇒ 纠到实际目标 P）
  expect_seq(tr.pc,
             {kPredTgt, kPredTgt, kPredTgt + 4, kPredTgt, kPredTgt,
              kPredTgt},
             "R7 pc");
  // FLUSH(1)=1（case_c(0)=0）; FLUSH(2)=0（case_c(1)=1 ⇒ spec_ok=1）
  // FLUSH(3)=0 ⇒ ★★ 关键：case_c(2)=1 ⇒ spec_ok=1 ⇒ 门控生效
  //   （若 spec_ok 被误与 pred_eff(=0) 相与 ⇒ 此拍 flush=1 ⇒ RED）
  expect_seq(tr.flush, {1, 0, 0, 1, 1, 1}, "R7 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R8 | ★★ **链式投机**（critic 第十轮 B-1 的锚）
//     场景：B1 在地址 X（预测 taken→P、实际 taken→P ⇒ 情形 C），
//           而 **P 处本身又是一条被正确预测 taken 的分支** B2（预测→Q）。
//     ⇒ 正确行为：B1 的 redirect 已被抑制，但 **B2 的投机 MUST 保留**
//        ⇒ pc_reg(N+1) = Q（不是 P+4）。
//     ⇒ ★ 若 case_c 臂写成 `pc_plus_4`（简报 §2.1.2 的字面写法）⇒ B2 的投机
//        被丢掉 ⇒ PC 顺序走到 P+4/P+8… ⇒ **Q 永不被取**（走错路）且
//        `case_c` 使 `spec_ok=1` ⇒ 错路指令**不被 flush** ⇒ 副作用泄漏。
//     ★ 该场景在 branchbench 4 档与 vendored ELF 上**结构性不可达**
//       （每 iter 只 1 条条件分支、其 target 非分支）⇒ MUST 有单元锚。
//     ★★ **保真度边界（critic 十一轮 NIT）**：本夹具的预测是**按 tick 分段的常量
//       脚本**、**不按 PC 查表** ⇒ R8 复现的是「链式投机的**时序形状** + 恰好对齐
//       的常量预测」⇒ ★ 它**鉴别不了**「BP 按 PC 查错表」类变异（那是 `[bp][plugin]`
//       侧的职责）⇒ 但它对目标变异 M12（`case_c` 退回 `pc_plus_4`）的鉴别力
//       **实测成立**（RED）⇒ 作为 B-1 的锚**合格**。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R8_chained_speculation_keeps_bp_next",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r8");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx,
                {{.end_tick = 1u, .br_taken = false, .br_target = 0,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt},   // tick0: 预测 init 处 taken→P
                 {.end_tick = 2u, .br_taken = true, .br_target = kPredTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kActTgt},   // tick1: B1 实际 taken→P（情形 C）
                                               //        + 预测 P 处 taken→Q
                 {.end_tick = 3u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = false, .pred_taken = false,
                  .pred_target = 0},         // tick2: B2 实际 taken→Q
                 {.end_tick = 1000u, .br_taken = false, .br_target = 0,
                  .flushed = false, .pred_valid = false, .pred_taken = false,
                  .pred_target = 0}},
                /*with_bp=*/true, 4);
  // next_pc(0)=P（投机）；next_pc(1)=**Q**（case_c ⇒ 保留 bp_next）；
  // next_pc(2)=Q+4（B2 已解析 ⇒ 顺序推进）
  expect_seq(tr.pc, {kPredTgt, kActTgt, kActTgt + 4, kActTgt + 8}, "R8 pc");
  // 全程无错路指令 ⇒ flush 恒 0
  expect_seq(tr.flush, {0, 0, 0, 0}, "R8 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R9 | **情形 E**（BP **已注册**但 `pred_valid=0` ⇒ BTB miss ⇒ 未投机；
//                 而实际 taken）⇒ fall-through 是错路 ⇒ **MUST flush**
//     ★ 与 R2（BP **未注册**）是**不同的代码路径**：
//       R2 走 `payloads().has(...) == false`；R9 走「payload 在、但值为 0」。
//     ★ 这是 AC-F2-17 ③ 的真实锚（R1/R2 只是「BP 未注册」路径）。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R9_case_E_btb_miss_still_flushes",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r9");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  // ★ BP 已注册（with_bp=true ⇒ payload 存在）但 `pred_valid=0` ⇒ 未投机
  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = false, .pred_taken = false,
                  .pred_target = 0}},
                /*with_bp=*/true, 6);
  expect_seq(tr.pc, {kActTgt, kActTgt, kActTgt, kActTgt, kActTgt, kActTgt},
             "R9 pc");
  expect_seq(tr.flush, {1, 1, 1, 1, 1, 1}, "R9 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R10 | `pred_valid=0` ⇒ **预测臂 MUST NOT 生效**（哪怕 `pred_taken=1`）
//     ★ BTB miss 时 PHT 读数是垃圾 ⇒ `pred_taken` 可能为 1 ⇒ MUST 被
//       `pred_valid` 屏蔽 ⇒ 否则会跳到一个**从未被预测过**的目标。
//     ★ 这是「`pred_eff` 丢掉 `pred_valid_now`」这个变异的唯一锚
//       （此前 7 个 case 全部 `valid ⇒ taken` 同真假 ⇒ 该变异抓不住）。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R10_pred_valid_zero_blocks_speculation",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r10");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx, {{.end_tick = 1000u, .br_taken = false, .br_target = kActTgt,
                  .flushed = false, .pred_valid = false, .pred_taken = true,
                  .pred_target = kPredTgt}},
                /*with_bp=*/true, 4);
  // 预测臂 MUST 不生效 ⇒ 逐拍顺序取指
  expect_seq(tr.pc, {kInitPc + 4, kInitPc + 8, kInitPc + 12, kInitPc + 16},
             "R10 pc");
  expect_seq(tr.flush, {0, 0, 0, 0}, "R10 flush");
}

// ═══════════════════════════════════════════════════════════════════════════
// R11 | ★★ **错路指令上的 `case_a` 误触发**（critic 十一轮 B-2）
//     ★ `BRANCH_TAKEN` 是**被 flush 掩码后**的值（branch plugin: `taken = taken
//       & !flush`）⇒ 错路指令的 taken 恒读作 **0**。
//     ⇒ 若情形判定不排除错路指令，`case_a = pv & pt & !br_taken` 会在**错路
//       指令**上误触发（`!br_taken` 恒真）⇒
//         ① PC 跳到**错路的 fall-through**（pc_lag+4）⇒ PC 出轨；
//         ② `mispredict_a_buf` 置位 ⇒ **过度 flush** ⇒ 抑制**正确路径**的指令
//            ⇒ 双向副作用泄漏。
//     ★ 场景：拍 0 的分支实际 taken→T（正确路径），拍 1 解析的是**被 flush
//       抑制的错路指令**，而上一拍给它记了「预测 valid+taken」⇒ 误触发。
//     ★ 该场景在 branchbench 4 档 / vendored ELF 47 个上**结构性不可达**
//       ⇒ MUST 有单元锚。
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("ibus_bp_pcmux_R11_flushed_instruction_excluded_from_case_a",
          "[cpu][chmem][bp][pcmux]") {
  ch::core::context ctx("ibus_bp_pcmux_r11");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  auto tr = run(ctx,
                {{.end_tick = 1u, .br_taken = true, .br_target = kActTgt,
                  .flushed = false, .pred_valid = true, .pred_taken = true,
                  .pred_target = kPredTgt},
                 {.end_tick = 1000u, .br_taken = false, .br_target = 0,
                  .flushed = true, .pred_valid = false, .pred_taken = false,
                  .pred_target = 0}},
                /*with_bp=*/true, 4);
  // next_pc(0)=T（实际 taken）；next_pc(1)=T+4（★ 错路指令 MUST NOT 触发 case_a
  //   ⇒ 顺序推进）；next_pc(2)=T+8
  expect_seq(tr.pc, {kActTgt, kActTgt + 4, kActTgt + 8, kActTgt + 12},
             "R11 pc");
  // 第 1 拍 flush=1（实际 taken 而未被投机）；第 2 拍起 MUST NOT 过度 flush
  expect_seq(tr.flush, {1, 0, 0, 0}, "R11 flush");
}

#endif  // CF_PLUGIN_USE_CH_MEM
