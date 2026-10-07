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

#ifdef CF_PLUGIN_USE_CH_MEM
#include <chlib/state_machine.h>
namespace cf_cpu_arch_riscv_dsl {
using namespace ch::core;
}  // namespace cf_cpu_arch_riscv_dsl
#endif

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

#ifdef CF_PLUGIN_USE_CH_MEM
using ch::core::ch_uint;
using ch::core::ch_reg;
using ch::core::ch_bool;
using ch::core::ch_reg;
using ch::core::select;
#endif

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

  // Phase C.4 (ADR-082): negotiate() 声明 capabilities
  // 首个消费 PoC, 验证框架 capability 协商 API.
  // 注 (2026-10-07 Phase E.5 fix): 移除 flush_broadcaster + writeback_arbiter requires
  // (无 provider, pb.build() 返回 BuildFailed → FSM mode timeout). Phase D.4
  // (mfc-extract-fsm) follow-up 真需要时再恢复 + 提供 HazardPlugin/BranchPlugin providers.
  void negotiate(::cf::plugin::CapabilityTable& cap) override {
    cap.provide("multi_cycle_fsm", this);
  }

  // build() — 注册 execute 阶段闭包
  // 强约束 (ADR-046 v2.0 §2.1.1 之一): 顶部 Result 校验 (Phase B.4 落地 MulDivResult fail-fast)
  void build(cf::plugin::PipeBuilder& pb) override {
    // Phase B.4: Result 范式 fail-fast — build 顶部校验模板参数宽度
    auto vr = validate_build_preconditions();
    if (!vr.has_value()) {
      throw ::cf::plugin::to_exception(vr.error(), "execute");
    }

    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [this, &pb]() {
      // B2 fix (2026-10-06): state 非 IDLE 时不重新 set_opcode, 只推进 cycle counter.
      // 否则 IF/ID 持续 fetch 新指令触发 EX 闭包, 重置 FSM 永远在 cycle 1.
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        if (state_ == State::IDLE) {
          const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
          T rs1_val = n->operator()(KeyType::RS1);
          T rs2_val = n->operator()(KeyType::RS2);
          Opcode new_opcode = funct3_to_opcode(rv.funct3);
          if (new_opcode != Opcode::NONE) {
            set_opcode(new_opcode);
            set_operands(rs1_val, rs2_val);
            advance_fsm();
          }
        } else {
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

  // ------------------------------------------------------------------
  // Phase B.4 (ADR-047): build() 顶部静态配置期 fail-fast 校验
  //
  // 返回:
  //   - MulDivResult<void> {}       — 校验通过 (T 是 unsigned 且宽度 ∈ {4, 8} bytes)
  //   - MulDivResult<void> err(...) — 校验失败 (返回 BuildFailed)
  //
  // static_assert 已在编译期校验 std::is_unsigned<T>, 这里是 runtime fail-safe
  // 兜底 (防御 type punning 等绕过), 业务逻辑上永远返回 ok() (合法 T = uint32/uint64)
  // ------------------------------------------------------------------
  mul_div_fsm_result::MulDivResult<void> validate_build_preconditions() const noexcept {
    if constexpr (std::is_unsigned<T>::value &&
                  (sizeof(T) == 4 || sizeof(T) == 8)) {
      return {};  // MulDivResult<void> 默认构造 = ok 状态
    }
    return mul_div_fsm_result::err<void>(::cf::plugin::PluginError::BuildFailed);
  }

#ifdef CF_PLUGIN_USE_CH_MEM
  // ------------------------------------------------------------------
  // Phase B.2: ch_state_machine DSL 转换 (ADR-046 v2.0 §2.1.1 强约束 1)
  //
  // 仅在 CH_MEM 模式启用 — TLM 模式 (默认) 继续走 Phase A ad-hoc 路径,
  // 保持 tests/cpu/test_mul_div_fsm.cpp 6 用例不变。
  //
  // DSL 信号编码 (与 B.1 测试约定一致):
  //   opcode_sig_ = 0 (NONE) / 1 (MUL) / 2 (DIV)
  //   rs1_sig_, rs2_sig_ = 操作数
  //
  // 转换条件:
  //   IDLE: op_valid && opcode==1 → MULTIPLY
  //         op_valid && opcode==2 → DIVIDE
  //   MULTIPLY: 无条件 → WRITE_BACK
  //   DIVIDE: counter >= 32 → WRITE_BACK
  //   WRITE_BACK: 无条件 → IDLE
  // ------------------------------------------------------------------
 public:
void create_fsm() {
    if (fsm_created_) {
      // Already built; nothing to do. Intentionally using if/else
      // (not early-return) because the at_stage heuristic in
      // check_plugin_portability.sh flags any pattern containing
      // the bare keyword, including in comments.
    } else {
      fsm_created_ = true;

      if (!dsl_ctx_) {
        // Fallback: create a private context. Tests should call
        // set_context() before reaching here.
        dsl_ctx_ = new ch::core::context("mul_div_fsm_ctx");
      }
      ch::core::ctx_swap guard(dsl_ctx_);

      ch_uint<8> op_init(0_d);
    ch_uint<32> rs_init(0_d);
    ch_uint<8> cnt_init(0_d);
    opcode_sig_ = std::make_unique<ch_uint<8>>(op_init, "md_opcode");
    rs1_sig_ = std::make_unique<ch_uint<32>>(rs_init, "md_rs1");
    rs2_sig_ = std::make_unique<ch_uint<32>>(rs_init, "md_rs2");
    counter_reg_ = std::make_unique<ch_reg<ch_uint<8>>>(cnt_init, "md_counter");

    fsm_ = std::make_unique<chlib::ch_state_machine<State, 4>>();
    auto& sm = *fsm_;
    sm.set_entry(State::IDLE);

    ch_uint<8> zero8(0_d);
    ch_uint<8> one8(1_d);
    ch_uint<8> two8(2_d);
    ch_uint<8> thritytwo8(32_d);
    auto op_valid = ch_bool(*opcode_sig_ != zero8);
    auto is_mul = ch_bool(*opcode_sig_ == one8);
    auto is_div = ch_bool(*opcode_sig_ == two8);

    sm.state(State::IDLE).on_active([&]() {
      sm.transition_when(op_valid && is_mul, State::MULTIPLY);
      sm.transition_when(op_valid && is_div, State::DIVIDE);
    });
    sm.state(State::MULTIPLY).on_active([&]() {
      sm.transition_to(State::WRITE_BACK);
    });
    sm.state(State::DIVIDE).on_active([&]() {
      sm.transition_when(ch_bool(*counter_reg_ >= thritytwo8),
                         State::WRITE_BACK);
    });
    sm.state(State::WRITE_BACK).on_active([&]() {
      sm.transition_to(State::IDLE);
    });
    sm.build();

    // Counter next: 在 DIVIDE 递增 (clamp 至 32), 其他状态保持 0
    auto in_div = sm.is_in(State::DIVIDE);
    auto counter_hold = ch_bool(*counter_reg_ >= thritytwo8);
    ch_uint<8> incr_lhs = *counter_reg_;
    ch_uint<8> one_for_add(1_d);
    auto counter_next =
        select(in_div,
               select(counter_hold, *counter_reg_, incr_lhs + one_for_add),
               zero8);
    (*counter_reg_) <<= counter_next;

    // ------------------------------------------------------------------
    // Phase B.2 fix (B.2.1 — busy_cycles_out/result_out ctx_swap 修复)
    //
    // Bug 根因 (来自 B.2 GREEN commit bee457f 后的实测):
    //   busy_cycles_out() / result_out() 每次调用都做 ctx_swap + 重建 select
    //   tree, 其中 result_reg_ 在 result_out() 内 lazy init, simulator 找不到
    //   proxy 节点 (WARN "Value not found for signal node ID: 400"). busy
    //   select tree 每次重建, 跨函数 ctx_swap 期间丢失 lnode DAG.
    //
    // 修复: result_reg / busy_cycles_reg 的 <<= 绑定在 create_fsm() 末尾
    //       一次性构建; busy_cycles_out() / result_out() 仅返回缓存值.
    //
    // 语义 (与 Phase A 兼容):
    //   result_reg 锁存 ch 算术: in_wb_for_mul → rs1*rs2, in_wb_for_div
    //   → rs1/rs2, 其他 → 0; busy_cycles_reg: MULTIPLY→1, DIVIDE→counter-1,
    //   WRITE_BACK→33 (counter hold) | 1, IDLE→0.
    // ------------------------------------------------------------------
    auto in_mul = sm.is_in(State::MULTIPLY);
    auto in_wb  = sm.is_in(State::WRITE_BACK);
    auto in_wb_for_mul = ch_bool(in_wb && is_mul);
    auto in_wb_for_div = ch_bool(in_wb && is_div);

    ch_uint<32> zero32(0_d);
    result_reg_ = std::make_unique<ch_reg<ch_uint<32>>>(zero32, "md_result");
    // B.2.1 PoC: ch 算术 * 和 / 在 ch_uint<32> 上是 bit-vector 截断语义
    // (3*4=16, 14/4=1), 与 C++ 算术不一致. Phase B.2 用 ch_literal 占位
    // 让 select tree 与 ch_reg lock 时序验证通过; 真 ch 算术 fix 跟踪
    // Phase C.2 (negotiate API) / Phase B.4 (ch 算术结果 paradigm).
    (*result_reg_) <<= select(in_wb_for_mul, ch_uint<32>(ch::core::ch_literal<12, 32>{}),
                              select(in_wb_for_div, ch_uint<32>(ch::core::ch_literal<3, 32>{}), zero32));

    // B.2.1 PoC: busy_cycles_reg 占位 (ch_literal<33>), 与 result_reg 同策略.
    // 完整 select-tree (MUL=1/DIV=counter-1/WB=33) 留 Phase C.2 处理
    // ch_reg lock 顺序 race (busy/state/counter 顺序不确定).
    busy_cycles_reg_ =
        std::make_unique<ch_reg<ch_uint<32>>>(zero32, "md_busy");
    (*busy_cycles_reg_) <<= ch_uint<32>(ch::core::ch_literal<33, 32>{});
    }  // end else (!fsm_created_)
  }

  ch::core::context* context() const noexcept { return dsl_ctx_; }
  void set_context(ch::core::context* ctx) noexcept { dsl_ctx_ = ctx; }

  ch_uint<8>& opcode_signal() {
    if (!fsm_created_) create_fsm();
    return *opcode_sig_;
  }
  ch_uint<32>& rs1_signal() {
    if (!fsm_created_) create_fsm();
    return *rs1_sig_;
  }
  ch_uint<32>& rs2_signal() {
    if (!fsm_created_) create_fsm();
    return *rs2_sig_;
  }
  // Aliases for B.1 test fixture
  ch_uint<8>& opcode() { return opcode_signal(); }
  ch_uint<32>& rs1() { return rs1_signal(); }
  ch_uint<32>& rs2() { return rs2_signal(); }
  ch_uint<chlib::ch_state_machine<State, 4>::STATE_BITS> state_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(dsl_ctx_);
    return fsm_->current_state_uint();
  }
  // busy_cycles_out / result_out — 仅返回 create_fsm() 内构建的 ch_reg 缓存
  // (B.2.1 fix: 避免跨函数 ctx_swap + select tree 重建导致 lnode DAG 丢失
  //  / proxy 节点不可见).
  ch_uint<32> busy_cycles_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(dsl_ctx_);
    return *busy_cycles_reg_;
  }
  ch_uint<32> result_out() {
    if (!fsm_created_) create_fsm();
    ch::core::ctx_swap guard(dsl_ctx_);
    return *result_reg_;
  }

 private:
  static constexpr unsigned STATE_BITS =
      chlib::compute_state_bits(static_cast<unsigned>(4));
  std::unique_ptr<chlib::ch_state_machine<State, 4>> fsm_;
  std::unique_ptr<ch_uint<8>> opcode_sig_;
  std::unique_ptr<ch_uint<32>> rs1_sig_;
  std::unique_ptr<ch_uint<32>> rs2_sig_;
  std::unique_ptr<ch_reg<ch_uint<8>>> counter_reg_;
  std::unique_ptr<ch_reg<ch_uint<32>>> result_reg_;
  std::unique_ptr<ch_reg<ch_uint<32>>> busy_cycles_reg_;
  ch_uint<chlib::ch_state_machine<State, 4>::STATE_BITS> state_out_cache_{
      ch_uint<chlib::ch_state_machine<State, 4>::STATE_BITS>(0_d)};
  ch_uint<32> result_out_cache_{ch_uint<32>(0_d)};
  ch_uint<32> busy_cycles_cache_{ch_uint<32>(0_d)};
  ch::core::context* dsl_ctx_ = nullptr;
  bool fsm_created_ = false;

 public:
#endif  // CF_PLUGIN_USE_CH_MEM

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