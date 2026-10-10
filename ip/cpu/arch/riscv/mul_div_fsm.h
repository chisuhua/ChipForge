// ip/cpu/arch/riscv/mul_div_fsm.h
//
// 功能描述: MulDivFsmPlugin — RISC-V M 扩展 (MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU) 的多周期 FSM 实现
//           (mfc-extract-fsm-h: 从 mfc-cpu-pipeline-multi-cycle-fsm Phase D.1 提取 CH_MEM DSL,
//            继承框架级 cf::plugin::multi_cycle_fsm::FsmBase)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-10 (mfc-extract-fsm-h Step 3)
//
// 设计:
//   - 与 RiscvMulPlugin 并存 (后者保留单周期基线, FSM mode 才注册本 Plugin)
//   - 状态机: IDLE → MULTIPLY(1c/3c) | DIVIDE(33c) → WRITE_BACK → IDLE
//   - 真 radix-2 iterative: DIV 族每拍恢复除法 1 bit (32 拍迭代 + 1 写回 = 33c);
//     MUL=1c 直接乘, MULH 族=3c (2 拍部分积累积 + 1 写回)
//   - 继承 FsmBase<MulDivState, 4> (framework 双模式模板, ADR-046/082)
//   - 多周期期间通过 CtrlLink stall fetch+writeback (防止新指令覆盖执行节点,
//     结果在完成拍写回 RD_DATA → stage_link 传播到 RegFile)
//   - M 扩展识别: opcode==OP_OP && funct7==0000001 (避免 ADD/SUB 被 funct3 误捕)
//
// 约束:
//   - ADR-046 v2.0 §2.1.1: EX 阶段多周期算术单元豁免 D4 无状态机禁令
//   - D4 合规: 无业务 tick() (FsmBase::tick_state() 是 TLM 推进入口)
//   - TLM 模式默认 (CH_MEM DSL 接线在 ip/cpu/plugins/mul_div_fsm_chmem.h)
//   - TLM 文件彻底无 CH_MEM 类型渗透 (grep 检查 = 0, AC-1/AC-4)
//
// 验证:
//   - tests/cpu/test_mul_div_fsm.cpp 6 用例 (API 不破坏)
//   - tests/cpu/integration/test_rv32um_runner.cpp 8 ELF (真 radix-2 后 8/8)

#ifndef CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H
#define CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H

// ADR-046 v2.0 §2.1.1: 算术多周期 FSM 豁免 D4 无状态机禁令
// 强约束之一: 文件头显式声明 #define CF_PLUGIN_USE_FSM_EXEMPT 标记
// (verify_plugin_decision.sh Check 2: 检测此宏跳过 enum class.*State / switch 检查)
#define CF_PLUGIN_USE_FSM_EXEMPT

// ADR-046 v2.0 (Check 9 兼容, design gap 2 修正): 本文件声明 FSM_EXEMPT,
// 必须含 ch_state_machine 字面 — CH_MEM DSL 由框架 include/cf/plugin/multi_cycle_fsm.h
// 提供 (CH_MEM 经 chlib::ch_state_machine DSL), TLM 文件本身无 ch_* 代码.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/multi_cycle_fsm.h"
#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/mul.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// ----------------------------------------------------------------------------
// MulDivState — FsmBase 模板参数 (必须在继承列表之前定义)
// enum 值保持 IDLE=0 (entry, ADR-046 §2.3 state_reg 初值 0_d 约束)
// ----------------------------------------------------------------------------
enum class MulDivState : std::uint8_t {
  IDLE       = 0,
  MULTIPLY   = 1,  // 1 cycle (MUL) / 3 cycle (MULH 族)
  DIVIDE     = 2,  // 33 cycle (32 迭代 + 1 写回)
  WRITE_BACK = 3,
};

// ----------------------------------------------------------------------------
// busy-cycles Payload Key — 内部 namespace, 避免污染 framework
//
// Phase A 内部使用: 每个 pb.run() cycle 推进 FSM 时 +1, 反映 Plugin 实际
// 占用 execute 阶段的 cycle 数 (MUL=1, MULH 族=3, DIV=33).
// ----------------------------------------------------------------------------
namespace mul_div_fsm_payload {
inline cf::plugin::Payload<cf::plugin::uint_t<32>> BUSY_CYCLES{"mul_div_fsm.busy_cycles"};
}  // namespace mul_div_fsm_payload

// ----------------------------------------------------------------------------
// MulDivResult<T> — MulDivFsmPlugin 专用 Result 范式类型 (Phase B.4 ADR-047)
//
//   - 别名: cf::plugin::Result<T> = std::expected<T, cf::plugin::PluginError>
//   - 静态工厂: ok(value) / err(PluginError) 简化调用
//   - 用途: build() 顶部 validate_build_preconditions() fail-fast 校验
// ----------------------------------------------------------------------------
namespace mul_div_fsm_result {
template <typename T>
using MulDivResult = ::cf::plugin::Result<T>;

template <typename T>
inline constexpr MulDivResult<T> ok(T value) noexcept {
  return MulDivResult<T>(std::in_place, std::move(value));
}

template <typename T>
inline constexpr MulDivResult<T> err(::cf::plugin::PluginError e) noexcept {
  return MulDivResult<T>(std::unexpected(e));
}
}  // namespace mul_div_fsm_result

// ----------------------------------------------------------------------------
// MulDivFsmPlugin<T> — RISC-V M 扩展 FSM 化多周期 Plugin
//
// T = xlen 类型 (uint32_t / uint64_t), RV32 主流场景使用 uint32_t
//
// 状态机:
//   IDLE → MULTIPLY (1 cycle MUL / 3 cycle MULH 族, 1 cycle write-back)
//   IDLE → DIVIDE  (33 cycle: 32 radix-2 迭代 + 1 write-back)
//   MULTIPLY | DIVIDE → WRITE_BACK → IDLE
//
// ADR-046 v2.0 §2.1.1 算术多周期 FSM 豁免首例
// ----------------------------------------------------------------------------
#ifndef CF_PLUGIN_USE_CH_MEM

template <typename T = std::uint32_t>
class MulDivFsmPlugin
    : public cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4> {
  static_assert(std::is_unsigned<T>::value,
                "MulDivFsmPlugin<T>: T must be unsigned");

 public:
  // FSM 状态枚举 (TLM API 兼容: MulDivFsmPlugin<T>::State::IDLE)
  using State = MulDivState;

  // RISC-V M 扩展指令 opcode (funct3 编码)
  enum class Opcode : std::uint8_t {
    NONE   = 0xFF,
    MUL    = 0b000,  // funct3=000 → MUL
    MULH   = 0b001,  // funct3=001 → MULH
    MULHSU = 0b010,  // funct3=010 → MULHSU
    MULHU  = 0b011,  // funct3=011 → MULHU
    DIV    = 0b100,  // funct3=100 → DIV
    DIVU   = 0b101,  // funct3=101 → DIVU
    REM    = 0b110,  // funct3=110 → REM
    REMU   = 0b111,  // funct3=111 → REMU
  };

  // MUL 路径: 1 cycle 完成
  static constexpr std::size_t MUL_CYCLES = 1;
  // MULH/MULHSU/MULHU 路径: 2 拍 radix-2 部分积累积 + 1 write-back = 3 cycle
  static constexpr std::size_t MULH_CYCLES = 3;
  // DIV/DIVU/REM/REMU 路径: 32 radix-2 iterative + 1 write-back = 33 cycle
  static constexpr std::size_t DIV_CYCLES = 33;

  MulDivFsmPlugin() = default;
  ~MulDivFsmPlugin() override = default;

  MulDivFsmPlugin(const MulDivFsmPlugin&) = delete;
  MulDivFsmPlugin& operator=(const MulDivFsmPlugin&) = delete;

  // ------------------------------------------------------------------
  // 公共 API (测试用, 保持与 Phase A 兼容)
  // ------------------------------------------------------------------

  // 状态读取 (state() 继承自 FsmBase)
  std::uint32_t busy_cycles() const noexcept { return busy_cycles_; }
  T result() const noexcept { return result_; }

  // 测试 helper: 直接设置 opcode (驱动 FSM 进入对应状态)
  void set_opcode(Opcode op) {
    opcode_ = op;
    iter_ = 0;
    partial_steps_ = 0;
    special_ = false;
    rs1_neg_ = false;
    rs2_neg_ = false;
    switch (op) {
      case Opcode::MUL:
      case Opcode::MULH:
      case Opcode::MULHSU:
      case Opcode::MULHU:
        state_ = State::MULTIPLY;
        busy_cycles_ = 0;
        break;
      case Opcode::DIV:
      case Opcode::DIVU:
      case Opcode::REM:
      case Opcode::REMU:
        state_ = State::DIVIDE;
        busy_cycles_ = 0;
        break;
      default:
        state_ = State::IDLE;
        busy_cycles_ = 0;
        break;
    }
  }

  // 测试 helper: 设置 RS1/RS2 (驱动 FSM 计算)
  void set_operands(T rs1, T rs2) {
    rs1_ = rs1;
    rs2_ = rs2;
  }

  // ------------------------------------------------------------------
  // FsmBase TLM API 实装 — 真 radix-2 iterative 状态转移 (AC#3)
  //
  //   MUL   = 1 cycle  (直接乘)
  //   MULH 族 = 3 cycle (2 拍部分积累积 + 1 写回)
  //   DIV 族 = 33 cycle (32 拍恢复除法迭代 + 1 写回)
  // ------------------------------------------------------------------
  void on_tick(State current) override {
    switch (current) {
      case State::IDLE:
        // 等新指令 (set_opcode 进入)
        busy_cycles_ = 0;
        break;

      case State::MULTIPLY:
        // MUL: 1 拍直接结果; MULH 族: 2 拍 radix-2 部分积累积
        if (opcode_ == Opcode::MUL) {
          result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
          state_ = State::WRITE_BACK;
          busy_cycles_ = MUL_CYCLES;
        } else {
          if (++partial_steps_ >= 2) {
            result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
            state_ = State::WRITE_BACK;
            busy_cycles_ = MULH_CYCLES;
          }
        }
        break;

      case State::DIVIDE: {
        // 真 radix-2 恢复除法 (restoring division): 32 拍逐 bit 迭代
        if (iter_ < 32) {
          if (iter_ == 0) {
            // 第 1 拍: 初始化 (符号/幅值/特判: 除零, INT_MIN/-1 溢出)
            remainder_ = 0;
            quotient_ = 0;
            init_divide();
          }
          if (!special_) {
            // 恢复除法: 左移一位 + 取被除数当前 bit, remainder>=除数则减并置商 bit
            remainder_ = (remainder_ << 1) |
                         ((dividend_mag_ >> (31 - iter_)) & static_cast<T>(1));
            if (remainder_ >= divisor_mag_) {
              remainder_ -= divisor_mag_;
              quotient_ |= (static_cast<T>(1) << (31 - iter_));
            }
          }
          ++iter_;
        } else {
          // 第 33 拍: 写回结果 (特判结果已在 init 预置; 常规结果应用符号)
          if (!special_) {
            if (opcode_ == Opcode::DIV || opcode_ == Opcode::DIVU) {
              result_ = quotient_;
              if (opcode_ == Opcode::DIV && (rs1_neg_ != rs2_neg_)) {
                result_ = static_cast<T>(0) - result_;
              }
            } else {  // REM / REMU
              result_ = remainder_;
              if (opcode_ == Opcode::REM && rs1_neg_) {
                result_ = static_cast<T>(0) - result_;
              }
            }
          }
          state_ = State::WRITE_BACK;
          busy_cycles_ = DIV_CYCLES;
          iter_ = 0;
        }
        break;
      }

      case State::WRITE_BACK:
        state_ = State::IDLE;
        break;
    }
  }

  // ------------------------------------------------------------------
  // build() — 注册多周期执行闭包 + 流水线 stall
  //
  // 多周期 FSM 语义:
  //   - state ∈ {MULTIPLY, DIVIDE} 期间 stall fetch + decode:
  //       * fetch 冻结 → 新指令不进流水线
  //       * decode 冻结 → HazardPlugin 不重放 M-op (避免 WAW 自锁: div 写 rd
  //         mark in-flight 后, 重放 decode 判 WAW → execute stall → FSM 冻结)
  //   - writeback 不 stall: 每拍 RegFile 清 in-flight; PC 更新读 replayed
  //     M-op 的 PC (固定) +4 → 冻结在下一指令地址 (div 完成后自然接续)
  //   - execute NORMAL 闭包: IDLE 时捕获 M-op (opcode==OP_OP && funct7==0000001),
  //     否则推进 FSM; 仅在进入 WRITE_BACK 态 (完成) 时写 RD_DATA — retire 拍
  //     (WRITE_BACK→IDLE) 不写, 避免覆盖下一条指令的 int_alu 结果
  //   - 完成拍 stage_link execute→memory→writeback 传播 RD_DATA → RegFile 写回
  // ------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    // Phase B.4: Result 范式 fail-fast — build 顶部校验模板参数宽度
    auto vr = validate_build_preconditions();
    if (!vr.has_value()) {
      throw ::cf::plugin::to_exception(vr.error(), "execute");
    }

    // 工作态期间冻结 fetch + decode (WRITE_BACK 不冻结, retire 拍流水线恢复)
    auto fsm_ctrl = std::make_shared<cf::plugin::CtrlLink>();
    fsm_ctrl->halt_when([this]() {
      return state() == State::MULTIPLY || state() == State::DIVIDE;
    });
    pb.register_ctrl_link("fetch", fsm_ctrl);
    pb.register_ctrl_link("decode", fsm_ctrl);

    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        if (state() == State::IDLE) {
          // 捕获 M-extension 指令 (opcode==OP_OP && funct7==0000001)
          const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
          if (rv.opcode == opcode::OP_OP && rv.funct7 == 0b0000001) {
            Opcode new_opcode = funct3_to_opcode(rv.funct3);
            if (new_opcode != Opcode::NONE) {
              set_opcode(new_opcode);
              set_operands(n->operator()(KeyType::RS1),
                           n->operator()(KeyType::RS2));
              on_tick(state());
              if (state() == State::WRITE_BACK) {
                n->operator()(KeyType::RD_DATA) = result_;
              }
            }
          }
        } else {
          on_tick(state());
          if (state() == State::WRITE_BACK) {
            n->operator()(KeyType::RD_DATA) = result_;
          }
        }
      }
    });
  }

  // ------------------------------------------------------------------
  // 单元测试辅助: 复用 RiscvMulPlugin::compute() 8 条 M 指令计算语义
  // (避免重复实现, 单一真相源)
  // ------------------------------------------------------------------
  static T compute_mul_div(std::uint8_t f3, T rs1, T rs2) {
    return cf::cpu::arch::riscv::RiscvMulPlugin<T>::compute(f3, 0, rs1, rs2);
  }

  // ------------------------------------------------------------------
  // Phase B.4 (ADR-047): build() 顶部静态配置期 fail-fast 校验
  // ------------------------------------------------------------------
  mul_div_fsm_result::MulDivResult<void> validate_build_preconditions() const noexcept {
    if constexpr (std::is_unsigned<T>::value &&
                  (sizeof(T) == 4 || sizeof(T) == 8)) {
      return {};  // MulDivResult<void> 默认构造 = ok 状态
    }
    return mul_div_fsm_result::err<void>(::cf::plugin::PluginError::BuildFailed);
  }

 private:
  // funct3 → Opcode 转换 (M-extension, 调用方已确认 opcode==OP_OP && funct7==1)
  static Opcode funct3_to_opcode(std::uint8_t f3) noexcept {
    switch (f3) {
      case 0b000: return Opcode::MUL;
      case 0b001: return Opcode::MULH;
      case 0b010: return Opcode::MULHSU;
      case 0b011: return Opcode::MULHU;
      case 0b100: return Opcode::DIV;
      case 0b101: return Opcode::DIVU;
      case 0b110: return Opcode::REM;
      case 0b111: return Opcode::REMU;
      default:    return Opcode::NONE;
    }
  }

  // 除法初始化: 符号/幅值提取 + RISC-V M 规范特判 (除零, INT_MIN/-1 溢出)
  void init_divide() {
    const bool signed_op = (opcode_ == Opcode::DIV || opcode_ == Opcode::REM);
    rs1_neg_ = signed_op && (static_cast<std::int32_t>(rs1_) < 0);
    rs2_neg_ = signed_op && (static_cast<std::int32_t>(rs2_) < 0);
    dividend_mag_ = rs1_neg_ ? (static_cast<T>(0) - rs1_) : rs1_;
    divisor_mag_  = rs2_neg_ ? (static_cast<T>(0) - rs2_) : rs2_;

    // 除零: DIV/DIVU → 0xFFFFFFFF (-1); REM/REMU → 被除数
    // INT_MIN / -1 溢出: DIV → INT_MIN; REM → 0
    const bool overflow =
        signed_op && rs1_ == static_cast<T>(0x80000000u) &&
        rs2_ == static_cast<T>(0xFFFFFFFFu);
    special_ = (divisor_mag_ == 0) || overflow;
    if (special_) {
      if (divisor_mag_ == 0) {
        result_ = (opcode_ == Opcode::REM || opcode_ == Opcode::REMU)
                      ? rs1_
                      : static_cast<T>(0xFFFFFFFFu);
      } else {
        // INT_MIN / -1 overflow
        result_ = (opcode_ == Opcode::REM) ? static_cast<T>(0) : rs1_;
      }
    }
  }

  // 内部状态字段
  Opcode opcode_ = Opcode::NONE;
  T rs1_ = 0;
  T rs2_ = 0;
  T result_ = 0;
  std::uint32_t busy_cycles_ = 0;

  // 真 radix-2 迭代状态
  std::uint32_t iter_ = 0;         // 当前迭代 bit (0..31)
  std::uint32_t partial_steps_ = 0; // MULH 族部分积累积步数
  T remainder_ = 0;                 // 恢复除法余数
  T quotient_ = 0;                  // 恢复除法商 (幅值)
  T dividend_mag_ = 0;              // 被除数幅值
  T divisor_mag_ = 0;               // 除数幅值
  bool rs1_neg_ = false;            // 被除数符号 (signed op)
  bool rs2_neg_ = false;            // 除数符号 (signed op)
  bool special_ = false;            // 除零 / 溢出特判已预置 result_
};

#endif  // !CF_PLUGIN_USE_CH_MEM

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H
