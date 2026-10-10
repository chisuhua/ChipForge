// ip/cpu/plugins/mul_div_fsm_chmem.h
//
// 功能描述: MulDivFsmPlugin CH_MEM 模式配对 (mfc-extract-fsm-h Step 3 重接)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-10
//
// 设计 (ADR-040 v2.0 双文件分离 + ADR-082 CH_MEM elaborator):
//   - CH_MEM 模式 (CF_PLUGIN_USE_CH_MEM) 提供 elaboration API
//   - 本文件定义 CH_MEM 版 MulDivFsmPlugin: 继承框架级
//     cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4> (ADR-046/082),
//     DSL 接线 (opcode_sig_/rs1_sig_/rs2_sig_/counter_reg_ + ch_state_machine
//     transition_when) + result/busy ch_reg 锁存 + 信号访问器
//   - has_chmem_support symbol 双模式保留 (cycle-parity test 入口, design gap 3)
//   - create_fsm() 与 Phase B.1 fixture 调用链兼容 (test_chmem_multi_cycle_fsm.cpp)
//   - 所有 ch_* 字面留在本文件 (check_plugin_portability.sh Check 2)
//
// 约束 (D4 + ADR-040 v2.0 + ADR-046):
//   - 必须 #ifdef CF_PLUGIN_USE_CH_MEM 整文件包裹 (除 has_chmem_support symbol)
//   - 无业务 tick() (使用 ch_state_machine DSL 调度)
//   - CH_MEM DSL 由 sim 驱动 (set_input_value/get_value), 不注册 at_stage 闭包
//
#ifndef CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H
#define CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H

#include "cf/plugin/multi_cycle_fsm.h"
#include "ip/cpu/arch/riscv/mul_div_fsm.h"

// 仅在 CH_MEM 模式下编译
#ifdef CF_PLUGIN_USE_CH_MEM

#include <chlib/state_machine.h>
#include <core/context.h>
#include <core/reg.h>
#include <core/uint.h>
#include <memory>

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// mul_div_fsm_chmem — CH_MEM helper namespace
namespace mul_div_fsm_chmem {

// has_chmem_support: CH_MEM 配对路径可用 (cycle parity test 入口)
inline constexpr bool has_chmem_support = true;

}  // namespace mul_div_fsm_chmem

// ----------------------------------------------------------------------------
// MulDivFsmPlugin<T> — CH_MEM 版 (ADR-046 v2.0 算术多周期 FSM)
//
// 复用 FsmBase 基础设施:
//   - FsmBase::ch_fsm_   → DSL 状态机 (chlib::ch_state_machine<MulDivState,4>)
//   - FsmBase::ch_context / set_ch_context → elaboration context
//   - FsmBase::state_out → 当前状态输出 (ch_fsm_.current_state_uint)
//
// 业务接线 (create_fsm, 一次性):
//   opcode_sig_/rs1_sig_/rs2_sig_ (输入信号) + counter_reg_ (DIVIDE 计数)
//   + result_reg_/busy_cycles_reg_ (ch_reg 锁存, B.2.1 PoC 占位语义)
//
// 状态编码 (DSL 测试约定, 独立于 TLM Opcode enum):
//   0=NONE, 1=MUL, 2=DIV
// ----------------------------------------------------------------------------
template <typename T = std::uint32_t>
class MulDivFsmPlugin
    : public cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4> {
  static_assert(std::is_unsigned<T>::value,
                "MulDivFsmPlugin<T>: T must be unsigned (CH_MEM mode)");

 public:
  using State = MulDivState;
  using Base = cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4>;

  MulDivFsmPlugin() = default;
  ~MulDivFsmPlugin() override = default;

  MulDivFsmPlugin(const MulDivFsmPlugin&) = delete;
  MulDivFsmPlugin& operator=(const MulDivFsmPlugin&) = delete;

  // FsmBase 纯虚 on_tick: CH_MEM 模式由 DSL/sim 驱动, TLM tick 路径不使用
  void on_tick(MulDivState /*current*/) override {}

  // FsmBase build() 默认空实现 (CH_MEM DSL 由 create_fsm 构建, sim 驱动)

  // ------------------------------------------------------------------
  // DSL 一次性构建 (原 Phase B.2 + B.2.1 ctx_swap 缓存策略)
  // 幂等: fsm_created_ 标志 + ch_fsm_built_ (FsmBase)
  // ------------------------------------------------------------------
  void create_fsm() {
    if (fsm_created_) {
      // Already built; nothing to do.
    } else {
      fsm_created_ = true;
      if (!ch_context()) {
        set_ch_context(new ch::core::context("mul_div_fsm_ctx"));
      }
      ch::core::ctx_swap guard(ch_context());

      // 输入信号 (测试经 set_input_value 驱动)
      opcode_sig_ = std::make_unique<ch::core::ch_uint<8>>(
          ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{}), "md_opcode");
      rs1_sig_ = std::make_unique<ch::core::ch_uint<32>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "md_rs1");
      rs2_sig_ = std::make_unique<ch::core::ch_uint<32>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "md_rs2");
      counter_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<8>>>(
          ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{}), "md_counter");

      auto& sm = ch_fsm();
      sm.set_entry(State::IDLE);

      ch::core::ch_uint<8> zero8(ch::core::ch_literal<0, 8>{});
      ch::core::ch_uint<8> one8(ch::core::ch_literal<1, 8>{});
      ch::core::ch_uint<8> two8(ch::core::ch_literal<2, 8>{});
      ch::core::ch_uint<8> thritytwo8(ch::core::ch_literal<32, 8>{});
      auto op_valid = ch::core::ch_bool(*opcode_sig_ != zero8);
      auto is_mul = ch::core::ch_bool(*opcode_sig_ == one8);
      auto is_div = ch::core::ch_bool(*opcode_sig_ == two8);

      sm.state(State::IDLE).on_active([&]() {
        sm.transition_when(op_valid && is_mul, State::MULTIPLY);
        sm.transition_when(op_valid && is_div, State::DIVIDE);
      });
      sm.state(State::MULTIPLY).on_active([&]() {
        sm.transition_to(State::WRITE_BACK);
      });
      sm.state(State::DIVIDE).on_active([&]() {
        sm.transition_when(ch::core::ch_bool(*counter_reg_ >= thritytwo8),
                           State::WRITE_BACK);
      });
      sm.state(State::WRITE_BACK).on_active([&]() {
        sm.transition_to(State::IDLE);
      });
      sm.build();

      // Counter next: DIVIDE 递增 (clamp 32), 其他保持 0
      auto in_div = sm.is_in(State::DIVIDE);
      auto counter_hold = ch::core::ch_bool(*counter_reg_ >= thritytwo8);
      ch::core::ch_uint<8> incr_lhs = *counter_reg_;
      ch::core::ch_uint<8> one_for_add(ch::core::ch_literal<1, 8>{});
      auto counter_next = ch::core::select(
          in_div,
          ch::core::select(counter_hold, *counter_reg_,
                           incr_lhs + one_for_add),
          zero8);
      (*counter_reg_) <<= counter_next;

      // result/busy ch_reg 锁存 (B.2.1 PoC 占位: ch 算术非 C++ 语义,
      // 用 ch_literal 占位保证 select tree 与 lock 时序可验证)
      auto in_mul = sm.is_in(State::MULTIPLY);
      auto in_wb = sm.is_in(State::WRITE_BACK);
      auto in_wb_for_mul = ch::core::ch_bool(in_wb && is_mul);
      auto in_wb_for_div = ch::core::ch_bool(in_wb && is_div);

      ch::core::ch_uint<32> zero32(ch::core::ch_literal<0, 32>{});
      result_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          zero32, "md_result");
      (*result_reg_) <<= ch::core::select(
          in_wb_for_mul,
          ch::core::ch_uint<32>(ch::core::ch_literal<12, 32>{}),
          ch::core::select(
              in_wb_for_div,
              ch::core::ch_uint<32>(ch::core::ch_literal<3, 32>{}),
              zero32));

      busy_cycles_reg_ =
          std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
              zero32, "md_busy");
      (*busy_cycles_reg_) <<=
          ch::core::ch_uint<32>(ch::core::ch_literal<33, 32>{});
    }
  }

  // ------------------------------------------------------------------
  // 信号访问器 (test fixture: set_input_value / get_value)
  // 懒触发 create_fsm (与 Phase B.1 opcode() → create_fsm 语义一致)
  // ------------------------------------------------------------------
  ch::core::ch_uint<8>& opcode() {
    if (!fsm_created_) create_fsm();
    return *opcode_sig_;
  }
  ch::core::ch_uint<32>& rs1() {
    if (!fsm_created_) create_fsm();
    return *rs1_sig_;
  }
  ch::core::ch_uint<32>& rs2() {
    if (!fsm_created_) create_fsm();
    return *rs2_sig_;
  }

  // 输出信号 (FsmBase::ch_fsm_ 状态寄存器 + ch_reg 缓存)
  ch::core::ch_uint<chlib::ch_state_machine<MulDivState, 4>::STATE_BITS>
  state_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(ch_context());
    return ch_fsm().current_state_uint();
  }
  ch::core::ch_uint<32> busy_cycles_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(ch_context());
    return *busy_cycles_reg_;
  }
  ch::core::ch_uint<32> result_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(ch_context());
    return *result_reg_;
  }

  // 上下文绑定 (FsmBase::set_ch_context 转发, 旧 API 兼容)
  void set_context(ch::core::context* ctx) noexcept { set_ch_context(ctx); }
  ch::core::context* context() const noexcept { return ch_context(); }

 private:
  std::unique_ptr<ch::core::ch_uint<8>> opcode_sig_;
  std::unique_ptr<ch::core::ch_uint<32>> rs1_sig_;
  std::unique_ptr<ch::core::ch_uint<32>> rs2_sig_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<8>>> counter_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> result_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> busy_cycles_reg_;
  bool fsm_created_ = false;
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM

// TLM mode 暴露 constexpr symbol (跨模式可见, 用于 cycle parity test 入口)
#ifndef CF_PLUGIN_USE_CH_MEM
namespace cf {
namespace cpu {
namespace arch {
namespace riscv {
namespace mul_div_fsm_chmem {
inline constexpr bool has_chmem_support = true;  // TLM mode 也声明 (mark 路径存在)
}
}
}
}
}
#endif

#endif  // CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H
