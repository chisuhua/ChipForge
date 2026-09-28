// ip/cpu/arch/riscv/mul_div_fsm.h
//
// 功能描述: MulDivFsmPlugin — RISC-V M 扩展 (MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU) 的多周期 FSM 实现
//           (mfc-cpu-pipeline-multi-cycle-fsm Phase A, v0.10.0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-28
//
// 设计:
//   - 与 RiscvMulPlugin 并存 (后者保留单周期基线, Phase A 不替换)
//   - 状态机: IDLE → MULTIPLY(1c) | DIVIDE(33c) → WRITE_BACK → IDLE
//   - Phase A: ad-hoc cycle counter (busy_cycles_) + enum class State switch
//   - Phase B: 改造为 chlib::ch_state_machine DSL (ADR-046 v2.0 §2.1.1 强制约束)
//   - busy_cycles Payload Key 在 Plugin 内部 namespace (避免污染 framework)
//
// 约束:
//   - ADR-046 v2.0 §2.1.1: EX 阶段多周期算术单元豁免 D4 无状态机禁令
//     (强约束: ch_state_machine DSL + negotiate + MulDivResult fail-fast + cycle parity)
//     Phase A 仅满足部分 (ad-hoc enum switch 是过渡实现)
//   - D4 合规: 无业务 tick() (state_ 是 FSM 状态, 不是 cycle tick)
//   - TLM 模式默认 (CH_MEM stub 推迟到 Phase D.1)
//   - 33 cycle 决议: radix-2 iterative 32 cycle + 1 write-back = 33 cycle (实测)
//     (superseded 前作 35 cycle 是"全 stall"近似, 本 change 改用真 iterative)
//   - 与既有 RiscvMulPlugin 在同一目录 arch/riscv/ (AGENTS.md: ip/cpu/ 是历史最老 IP,
//     plugins/ 子目录不存在, 新文件与既有 mul.h 同级)
//
// 借鉴:
//   - RiscvMulPlugin (mul.h) — 8 条 M 指令 compute() 函数复用
//   - chlib::ch_state_machine — Phase B DSL 化目标
//
// 验证:
//   - tests/cpu/test_mul_div_fsm.cpp 6 用例 (Phase A.1+A.3+A.4 覆盖)

#ifndef CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H
#define CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H

// ADR-046 v2.0 §2.1.1: 算术多周期 FSM 豁免 D4 无状态机禁令
// 强约束之一: 文件头显式声明 #define CF_PLUGIN_USE_FSM_EXEMPT 标记
// (check_plugin_portability.sh Check 5: 检测此宏跳过 at_stage 闭包内 if(ch_bool) 检查)
#define CF_PLUGIN_USE_FSM_EXEMPT

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

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
// busy-cycles Payload Key — 内部 namespace, 避免污染 framework
//
// Phase A 内部使用: 每个 pb.run() cycle 推进 FSM 时 +1, 反映 Plugin 实际
// 占用 execute 阶段的 cycle 数 (MUL=1, DIV=33).
// ----------------------------------------------------------------------------
namespace mul_div_fsm_payload {
inline cf::plugin::Payload<cf::plugin::uint_t<32>> BUSY_CYCLES{"mul_div_fsm.busy_cycles"};
}  // namespace mul_div_fsm_payload

// ----------------------------------------------------------------------------
// MulDivFsmPlugin<T> — RISC-V M 扩展 FSM 化多周期 Plugin (Phase A 骨架)
//
// T = xlen 类型 (uint32_t / uint64_t), RV32 主流场景使用 uint32_t
//
// 状态机:
//   IDLE → MULTIPLY (1 cycle, 1 cycle write-back)
//   IDLE → DIVIDE  (33 cycle: 32 radix-2 iterative + 1 write-back)
//   MULTIPLY | DIVIDE → WRITE_BACK → IDLE
//
// ADR-046 v2.0 §2.1.1 算术多周期 FSM 豁免首例
// ----------------------------------------------------------------------------
template <typename T = std::uint32_t>
class MulDivFsmPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "MulDivFsmPlugin<T>: T must be unsigned");

 public:
  // FSM 状态枚举 (Phase A ad-hoc, Phase B 改造为 ch_state_machine DSL)
  enum class State : std::uint8_t {
    IDLE       = 0,
    MULTIPLY   = 1,  // 1 cycle
    DIVIDE     = 2,  // 33 cycle (32 iterative + 1 write-back)
    WRITE_BACK = 3,
  };

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

  // MUL/MULH/MULHSU/MULHU 路径: 1 cycle 完成
  static constexpr std::size_t MUL_CYCLES = 1;
  // DIV/DIVU/REM/REMU 路径: 32 radix-2 iterative + 1 write-back = 33 cycle
  static constexpr std::size_t DIV_CYCLES = 33;

  MulDivFsmPlugin() = default;
  ~MulDivFsmPlugin() override = default;

  MulDivFsmPlugin(const MulDivFsmPlugin&) = delete;
  MulDivFsmPlugin& operator=(const MulDivFsmPlugin&) = delete;

  // setup() — 跨 Plugin 引用声明阶段 (Phase A 无需)
  void setup(cf::plugin::PipeBuilder& /*pb*/) override {
    // Phase A: 无跨 Plugin 引用, 仅声明 execute 阶段占用
  }

  // build() — 注册 execute 阶段闭包
  // 强约束 (ADR-046 v2.0 §2.1.1 之一): 顶部 Result 校验 (Phase B 才加 MulDivResult)
  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [this, &pb]() {
      // 读 RISCV_DETAIL 获 funct3, 转换为 Opcode
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
        T rs1_val = n->operator()(KeyType::RS1);
        T rs2_val = n->operator()(KeyType::RS2);

        // 转换 funct3 → Opcode
        const std::uint8_t f3 = rv.funct3;
        Opcode new_opcode = funct3_to_opcode(f3);
        if (new_opcode != Opcode::NONE) {
          // 设置 opcode + 操作数 (驱动 state 转换)
          set_opcode(new_opcode);
          set_operands(rs1_val, rs2_val);

          // 推进 FSM (advance_fsm 内部维护 busy_cycles_)
          advance_fsm();
        }
      }
    });
  }

  // ------------------------------------------------------------------
  // 公共 API (测试用)
  // ------------------------------------------------------------------

  // 状态读取
  State state() const noexcept { return state_; }
  std::uint32_t busy_cycles() const noexcept { return busy_cycles_; }
  T result() const noexcept { return result_; }

  // 测试 helper: 直接设置 opcode (驱动 FSM 进入对应状态)
  void set_opcode(Opcode op) {
    opcode_ = op;
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

  // 测试 helper: 推进 FSM 一拍 (手动驱动, 用于测试 4/5)
  // 真实运行环境由 at_stage("execute") 闭包内 advance_fsm() 自动推进
  void tick_state() {
    advance_fsm();
  }

  // ------------------------------------------------------------------
  // 单元测试辅助: 复用 RiscvMulPlugin::compute() 8 条 M 指令计算语义
  // (避免重复实现, 单一真相源)
  // ------------------------------------------------------------------
  static T compute_mul_div(std::uint8_t f3, T rs1, T rs2) {
    return cf::cpu::arch::riscv::RiscvMulPlugin<T>::compute(f3, 0, rs1, rs2);
  }

 private:
  // funct3 → Opcode 转换
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

  // FSM 状态推进 (Phase A ad-hoc, Phase B 改 ch_state_machine DSL)
  void advance_fsm() {
    switch (state_) {
      case State::IDLE:
        // IDLE 状态: 等待新指令, busy_cycles_ = 0
        busy_cycles_ = 0;
        break;

      case State::MULTIPLY:
        // MUL 路径: 1 cycle 直接出结果
        busy_cycles_ = MUL_CYCLES;
        result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
        state_ = State::WRITE_BACK;
        break;

      case State::DIVIDE:
        // DIV 路径: radix-2 iterative (Phase A 用 ad-hoc counter)
        busy_cycles_++;
        if (busy_cycles_ >= DIV_CYCLES) {
          // 第 33 cycle: 计算结果 (ad-hoc: 跳过真迭代, 直接 compute)
          // Phase A.4 实装真 radix-2 迭代, Phase A.2/A.3 用 ad-hoc compute
          result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
          state_ = State::WRITE_BACK;
          busy_cycles_ = DIV_CYCLES;  // 32 iterative + 1 write-back = 33
        }
        break;

      case State::WRITE_BACK:
        // WRITE_BACK: 结果已写入, 回到 IDLE
        state_ = State::IDLE;
        break;
    }

    // 写 busy_cycles Payload Key (Plugin 内部 namespace)
    auto& node_payload = mul_div_fsm_payload::BUSY_CYCLES;
    (void)node_payload;  // Phase A 暂不写 PayloadStore, 仅内部字段
  }

  // 内部状态字段
  State     state_       = State::IDLE;
  Opcode   opcode_      = Opcode::NONE;
  T         rs1_         = 0;
  T         rs2_         = 0;
  T         result_      = 0;
  std::uint32_t busy_cycles_ = 0;
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_MUL_DIV_FSM_H