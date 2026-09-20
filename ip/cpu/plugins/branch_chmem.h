// ip/cpu/plugins/branch_chmem.h
//
// M4/W7: BranchPlugin CH_MEM — select-tree branch decoder for RISC-V B-type
// instructions. Computes branch_taken (6-op select tree: BEQ/BNE/BLT/BGE/
// BLTU/BGEU) and branch_target (pc + imm). Integrates CtrlLink::flush_when
// for branch flush signaling (Spike 7 HLT detection pattern).
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17
//
// 设计:
//   - 仅在 CF_PLUGIN_USE_CH_MEM 下编译 (elaboration 路径)
//   - B-type funct3 → 6 层 select 树 (BEQ/BNE/BLT/BGE/BLTU/BGEU)
//   - 有符号比较 (BLT/BGE): sign-bit extraction pattern (同 int_alu_chmem.h SLT)
//   - ch_bool 表达式替代运行期 std::function<bool()>
//   - CtrlLink::flush_when(ch_bool) 注册分支 flush 信号 (Spike 7)
//
// Opcode 约束:
//   - B-type 仅 (opcode == 0x63)
//   - J-type (JAL/JALR) 推迟 (PoC 范围外)
//
// B-type funct3 映射 (RISC-V RV32I):
//   000 = BEQ  (branch if rs1 == rs2)
//   001 = BNE  (branch if rs1 != rs2)
//   100 = BLT  (branch if rs1 < rs2  signed)
//   101 = BGE  (branch if rs1 >= rs2 signed)
//   110 = BLTU (branch if rs1 < rs2  unsigned)
//   111 = BGEU (branch if rs1 >= rs2 unsigned)
//
// 借鉴:
//   - int_alu_chmem.h: select 树替代 if/else, ch_bool 包装模式
//   - hazard_chmem.h: CtrlLink integration (flush_when 注册)
//   - CppHDL/examples/riscv-mini/src/rv32i_alu.h (sign-bit signed compare)
//
// 约束:
//   - D4 合规: 无 tick(), 用 at_stage() 注册闭包
//   - D4 合规: 闭包内 select 树替代 if/else 分支
//   - 无 at_stage 闭包内 if-return 早返
//   - 无运行期 if(ch_bool) (ch_bool 的 explicit operator bool() 编译期不报错)
//   - 无 _d 后缀字面值 (CppHDL literal_ext.h 字符解析限制)

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CPU_PLUGINS_BRANCH_CHMEM_H
#define CF_IP_CPU_PLUGINS_BRANCH_CHMEM_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <ch.hpp>
#include <core/bool.h>
#include <core/operators.h>
#include <core/uint.h>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/decode_chmem.h"
using namespace cf::plugin;
using namespace ch;
using namespace ch::core;
using namespace cf::cpu::arch::riscv;

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class IBusPlugin;

// ============================================================================
// BranchPlugin (CH_MEM): 6-op B-type select 树 + CtrlLink flush
//
// 生命周期 (CH_MEM):
//   1. CpuFactory::build_cpu() 创建 plugin 实例并注册到 PipeBuilder
//   2. setup(): 创建 CtrlLink 并注册到 "branch" stage, 注册 at_stage 闭包
//   3. PipeBuilder::elaborate(): 运行 at_stage 闭包 → select 树计算
//      → PayloadStore 写信号 → CtrlLink::flush_when(ch_bool)
//
// 关键信号:
//   - Input:  RS1, RS2, PC, RISCV_DETAIL (opcode/funct3/imm)
//   - Output: BranchPlugin::BRANCH_TAKEN (ch_bool), BRANCH_TARGET (T)
//   - CtrlLink: flush_when(taken)  →  OR 接入流水线 flush 门控
// ============================================================================
template <typename T>
class BranchPlugin : public PluginBase {
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_unsigned<T>::value,
                "BranchPlugin<T>: T must be unsigned (TLM mode)");
#endif
  static constexpr std::size_t kXlenBits =
#ifdef CF_PLUGIN_USE_CH_MEM
      32;  // CH_MEM PoC: 固定 RV32 (T=ch_uint<32> 时 sizeof(T)*8 ≠ 32)
#else
      sizeof(T) * 8;
#endif

  // BRANCH_TAKEN payload key — ch_bool 信号 (独立于 DecodePayload.branch_taken
  // 的 bool 字段, 因为 CH_MEM 模式下需要 ch_bool 信号句柄)
  static inline Payload<ch::core::ch_bool> BRANCH_TAKEN{"cpu.branch_taken"};

 public:
  BranchPlugin() = default;
  ~BranchPlugin() override = default;

  BranchPlugin(const BranchPlugin&) = delete;
  BranchPlugin& operator=(const BranchPlugin&) = delete;

  // 公开 BRANCH_TAKEN key getter (Phase 6d 6d.3: 供 IBusPlugin/Factory 读取分支结果)
  static const Payload<ch::core::ch_bool>& get_branch_taken_key() { return BRANCH_TAKEN; }

  // --------------------------------------------------------------------------
  // setup — 创建 CtrlLink 并注册 at_stage("branch") 回调
  //
  // CH_MEM 模式:
  //   - CtrlLink 的 flush_when(ch_bool) 会在 elaboration 期
  //     被 PipeBuilder::elaborate() 读入 OR 合并, 连到 stage 的 flush 信号.
  //   - at_stage 闭包在 elaborate() 期间执行一次, 发射 select 树 DAG.
  // --------------------------------------------------------------------------
  void setup(PipeBuilder& pb) override {
    branch_ctrl_ = std::make_shared<CtrlLink>();
    pb.register_ctrl_link("branch", branch_ctrl_);

    pb.at_stage("branch", Phase::NORMAL, [this, &pb] {
      this->branch_compute(pb);
    });
  }

  // --------------------------------------------------------------------------
  // build — 空 (所有工作由 setup 中的 at_stage 闭包完成)
  // --------------------------------------------------------------------------
  void build(PipeBuilder&) override {}

 private:
  // CtrlLink shared_ptr: setup() 中创建并注册到 PipeBuilder
  // branch_compute() 中用此句柄调用 flush_when(ch_bool)
  std::shared_ptr<CtrlLink> branch_ctrl_;

  // --------------------------------------------------------------------------
  // branch_compute — B-type 分支决策组合逻辑
  //
  // CH_MEM 模式: at_stage 闭包在 PipeBuilder::elaborate() 期间执行一次,
  // 发射 select 树 DAG 到 ch_bool/ch_uint 信号.
  //
  // 计算步骤:
  //   1. 读 RS1, RS2, PC, RISCV_DETAIL (opcode/funct3/imm)
  //   2. branch_target = pc + imm (pc-relative)
  //   3. 6-op B-type select 树 → branch_taken
  //   4. 写 BRANCH_TAKEN / BRANCH_TARGET 到 PayloadStore
  //   5. CtrlLink::flush_when(taken) — 分支跳转时 flush 流水线
  // --------------------------------------------------------------------------
  void branch_compute(PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    using RvKey = payload_keys_riscv<T>;

    auto* n = pb.node_of_logic_stage("branch").get();
    if (n) {
      // ── 1. 读入信号 (ch 类型, elaboration 期 DAG 节点) ──
      auto rs1_val = n->operator()(KeyType::RS1);
      auto rs2_val = n->operator()(KeyType::RS2);
      auto pc_val  = n->operator()(KeyType::PC);
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      const auto& decoded = n->operator()(DecodePlugin::DECODED_INST);

      // ── 2. branch_target = pc + imm (pc-relative, always for B-type) ──
      auto branch_target = pc_val + decoded.imm;

      // ── 3. opcode 判别 (ch 信号, DECODED_INST) ──
      auto is_branch = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_BRANCH, 7>{}));
      auto is_jal    = (decoded.opcode == ch_uint<7>(ch::core::ch_literal<opcode::OP_JAL, 7>{}));
      // JALR 支持推迟 (Phase 6d follow-up): 需 rs1 + imm 目标计算 + rd 写回
      auto is_beq   = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<0, 3>{}));
      auto is_bne   = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<1, 3>{}));
      auto is_blt   = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<4, 3>{}));
      auto is_bge   = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<5, 3>{}));
      auto is_bltu  = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<6, 3>{}));
      auto is_bgeu  = is_branch && (decoded.funct3 == ch_uint<3>(ch::core::ch_literal<7, 3>{}));

      // BEQ: rs1 == rs2
      auto beq_taken = ch_bool(rs1_val == rs2_val);

      // BNE: rs1 != rs2
      auto bne_taken = ch_bool(rs1_val != rs2_val);

      // ── 有符号比较 (BLT / BGE) — sign-bit 模式 ──
      // 原理 (同 int_alu_chmem.h SLT):
      //   符号不同时为负 (a_sign), 符号相同时看减法借位
      auto a_sign      = bits<kXlenBits - 1, kXlenBits - 1>(rs1_val);
      auto b_sign      = bits<kXlenBits - 1, kXlenBits - 1>(rs2_val);
      auto lt_sd       = ch_bool(a_sign != b_sign);
      auto lt_aneg     = ch_bool(a_sign == ch_bool(true));
      auto r_sub       = rs1_val - rs2_val;
      auto lt_borrow   = ch_bool(bits<kXlenBits - 1, kXlenBits - 1>(r_sub) !=
                                 ch_uint<1>(ch::core::ch_literal<0, 1>{}));
      auto lt_signed   = select(lt_sd, lt_aneg, lt_borrow);
      auto ge_signed   = !lt_signed;

      // ── 无符号比较 (BLTU / BGEU) ──
      auto lt_unsigned = ch_bool(rs1_val < rs2_val);
      auto ge_unsigned = ch_bool(rs1_val >= rs2_val);

      // ── 6 层 select 树 (default: false) ──
      // 注: 用 ch_bool(false) 初始化, 经 select 逐层覆写
      ch::core::ch_bool taken(false);
      taken = select(is_beq,   beq_taken,   taken);
      taken = select(is_bne,   bne_taken,   taken);
      taken = select(is_blt,   lt_signed,   taken);
      taken = select(is_bge,   ge_signed,   taken);
      taken = select(is_bltu,  lt_unsigned, taken);
      taken = select(is_bgeu,  ge_unsigned, taken);

      // ── JAL: 无条件跳转 (pc + imm) ──
      taken = select(is_jal, ch::core::ch_bool(true), taken);
      // JALR: 推迟 (defer for now — 需 rs1 值计算 target = (rs1 + imm) & ~1)

      // ── 4. 写 PayloadStore ──
      // Phase 6d.4: 分支 flush — 上一周期 branch taken 时, 当前指令是
      // fall-through 误取, 其分支决策必须抑制 (否则误取指令再次跳转, 覆盖
      // 正确的 branch target). flush = fetch.FLUSH (IBusPlugin br_taken_buf).
      auto* fch = pb.node_of_logic_stage("fetch").get();
      auto flush = fch ? fch->operator()(IBusPlugin<T>::FLUSH)
                       : ch::core::ch_bool(false);
      taken = taken && !flush;
      n->operator()(BRANCH_TAKEN) = taken;
      n->operator()(RvKey::BRANCH_TARGET) = branch_target;

      // ── 5. CtrlLink flush 信号注册 ──
      // 当分支跳转时 flush 后续流水级指令 (Spike 7 HLT detection pattern)
      if (branch_ctrl_) {
        branch_ctrl_->flush_when(taken);
      }
    }
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_BRANCH_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM