// ip/cpu/arch/riscv/mul.h
//
// 功能描述: RiscvMulPlugin — M 扩展乘除法 (M3.6, P1) + 多周期 LATENCY (M5.14, P2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 设计:
//   - M 扩展: MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU (8 条)
//   - execute 阶段: 读取 RS1/RS2, 写 RESULT
//   - 当前实现: 单周期组合逻辑 (M2 框架简化), M4 集成 3 级子流水
//   - 模板参数化 <typename T, std::size_t LATENCY = 1>: T = xlen 类型,
//     LATENCY ∈ {1, 3, 5} (默认 1 = 单周期, 与 baseline byte-identical)
//   - 多周期 LATENCY: setup() 中通过 declare_substage("execute", "mul_sN")
//     声明 mul_s1..mul_s(LATENCY-1) 子阶段 (M5-DSE M5.14 §3.2)
//   - 5-stage baseline byte-identical: LATENCY=1 不声明任何 substage
//
// 约束:
//   - 头文件为主 (无 .cpp)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_ARCH_RISCV_MUL_H
#define CF_IP_CPU_ARCH_RISCV_MUL_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

template <typename T, std::size_t LATENCY = 1>
class RiscvMulPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RiscvMulPlugin<T>: T must be unsigned");
  static_assert(LATENCY >= 1 && LATENCY <= 5,
                "RiscvMulPlugin: LATENCY must be in {1, 3, 5}");

 public:
  static constexpr std::size_t LAT = LATENCY;

  RiscvMulPlugin() = default;
  ~RiscvMulPlugin() override = default;

  RiscvMulPlugin(const RiscvMulPlugin&) = delete;
  RiscvMulPlugin& operator=(const RiscvMulPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& pb) override {
    if constexpr (LATENCY == 1) {
      // 单周期: 无 substage, byte-identical to baseline
    } else {
      // 多周期: 声明 mul_s1..mul_s(LATENCY-1) 作为 execute 的子阶段
      for (std::size_t i = 1; i < LATENCY; ++i) {
        pb.declare_substage(
            "execute",
            std::string("mul_s") + std::to_string(i),
            1);
      }
    }
  }

  std::size_t expected_latency() const noexcept { return LATENCY; }
  std::size_t substage_count() const noexcept {
    return LATENCY > 0 ? LATENCY - 1 : 0;
  }

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        T rs1_val = n->operator()(KeyType::RS1);
        T rs2_val = n->operator()(KeyType::RS2);
        const auto& rv = n->operator()(RvKey::RISCV_DETAIL);

        T result = compute(rv.funct3, rv.funct7, rs1_val, rs2_val);
        n->operator()(KeyType::RESULT) = result;
      }
    });
  }

  // 单元测试辅助: 计算乘除法
  static T compute(std::uint8_t f3, std::uint8_t f7, T rs1, T rs2) {
    (void)f7;
    switch (f3) {
      case 0b000: {  // MUL
        std::int64_t r = static_cast<std::int64_t>(rs1) *
                         static_cast<std::int64_t>(rs2);
        return static_cast<T>(r & static_cast<std::int64_t>(~T{0}));
      }
      case 0b001: {  // MULH (有符号 × 有符号, 取高 32 位结果的高 32 位, RV32)
        // RV32: 返回 (rs1 * rs2) 的高 32 位 (有符号)
        std::int64_t a = static_cast<std::int64_t>(static_cast<std::int32_t>(rs1));
        std::int64_t b = static_cast<std::int64_t>(static_cast<std::int32_t>(rs2));
        std::int64_t r = a * b;
        return static_cast<T>(static_cast<std::uint32_t>(r >> 32));
      }
      case 0b010: {  // MULHSU
        std::int64_t a = static_cast<std::int64_t>(static_cast<std::int32_t>(rs1));
        std::uint64_t b = rs2;
        std::int64_t r = static_cast<std::int64_t>(a * static_cast<std::int64_t>(b));
        return static_cast<T>(static_cast<std::uint32_t>(r >> 32));
      }
      case 0b011: {  // MULHU
        std::uint64_t a = rs1;
        std::uint64_t b = rs2;
        std::uint64_t r = (a * b);
        return static_cast<T>(static_cast<std::uint32_t>(r >> 32));
      }
      case 0b100: {  // DIV
        if (rs2 == 0) return static_cast<T>(-1);
        std::int64_t a = static_cast<std::int64_t>(static_cast<std::int32_t>(rs1));
        std::int64_t b = static_cast<std::int64_t>(static_cast<std::int32_t>(rs2));
        if (a == (std::int64_t)std::numeric_limits<std::int32_t>::min() && b == -1)
          return rs1;  // 除法溢出: INT32_MIN / -1 = INT32_MIN
        return static_cast<T>(static_cast<std::int32_t>(a / b));
      }
      case 0b101: {  // DIVU
        if (rs2 == 0) return static_cast<T>(-1);
        return rs1 / rs2;
      }
      case 0b110: {  // REM
        if (rs2 == 0) return rs1;
        std::int64_t a = static_cast<std::int64_t>(static_cast<std::int32_t>(rs1));
        std::int64_t b = static_cast<std::int64_t>(static_cast<std::int32_t>(rs2));
        if (a == (std::int64_t)std::numeric_limits<std::int32_t>::min() && b == -1)
          return 0;  // 除法溢出: 余数 = 0
        return static_cast<T>(static_cast<std::int32_t>(a % b));
      }
      case 0b111: {  // REMU
        if (rs2 == 0) return rs1;
        return rs1 % rs2;
      }
      default: return 0;
    }
  }

 private:
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_MUL_H
