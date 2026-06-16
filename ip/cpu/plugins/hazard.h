// ip/cpu/plugins/hazard.h
//
// 功能描述: HazardPlugin — 数据冒险检测 (M2.2, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 模板参数化 xlen (与 RegFilePlugin 一致)
//   - 32 寄存器 scoreboard (bool 数组), 标记"正在飞行中"的写操作
//   - RAW (Read After Write): 读寄存器正在被前面的指令写入
//   - WAW (Write After Write): 写寄存器正在被前面的指令写入
//   - WAR (Write After Read): 不阻塞 (寄存器读发生在写之前, 通过转发解决)
//   - 当前 M2 阶段: 基础框架, 多指令飞行功能在 M4/M5 完整流水线后完善
//
// 借鉴:
//   - RegFilePlugin 模板模式
//   - L1CachePlugin 6 维度方法学 D2 (范式合规)
//
// 约束:
//   - 头文件为主 (.cpp 仅 stub)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_PLUGINS_HAZARD_H
#define CF_IP_CPU_PLUGINS_HAZARD_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

// ----------------------------------------------------------------------------
// HazardPlugin — 数据冒险检测
//
// 模板参数 T: uint32_t (RV32) / uint64_t (RV64)
// 寄存器数量: 32 (x0-x31)
//
// Scoreboard 模型:
//   - scoreboard_[i] = true: 寄存器 i 正在被某条指令写入 (飞行中)
//   - scoreboard_[i] = false: 寄存器 i 可用
//
// 检测逻辑:
//   RAW: reads_rs1 && scoreboard_[rs1_idx] → 冒险
//   RAW: reads_rs2 && scoreboard_[rs2_idx] → 冒险
//   WAW: writes_rd && scoreboard_[rd_idx] → 冒险
//   WAR: 不阻塞 (读在写之前, 不冲突)
// ----------------------------------------------------------------------------
template <typename T>
class HazardPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "HazardPlugin<T>: T must be unsigned");

 public:
  static constexpr std::size_t kNumRegs = 32;

  HazardPlugin() { reset(); }
  ~HazardPlugin() override = default;

  HazardPlugin(const HazardPlugin&) = delete;
  HazardPlugin& operator=(const HazardPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  // ------------------------------------------------------------------------
  // 单元测试辅助 API
  // ------------------------------------------------------------------------

  // 检测 RAW 冒险 (指定寄存器索引)
  bool has_raw(std::uint8_t rs_idx) const {
    return rs_idx < kNumRegs && scoreboard_[rs_idx];
  }

  // 检测 WAW 冒险
  bool has_waw(std::uint8_t rd_idx) const {
    return rd_idx < kNumRegs && scoreboard_[rd_idx];
  }

  // 检测完整指令冒险
  bool has_hazard(const cf::cpu::core::payload::DecodePayload& dec) const {
    if (dec.reads_rs1 && has_raw(dec.rs1_idx)) return true;
    if (dec.reads_rs2 && has_raw(dec.rs2_idx)) return true;
    if (dec.writes_rd && has_waw(dec.rd_idx)) return true;
    return false;
  }

  // 标记寄存器为飞行中
  void mark_in_flight(std::uint8_t rd_idx) {
    if (rd_idx < kNumRegs) scoreboard_[rd_idx] = true;
  }

  // 清除飞行标记
  void clear_in_flight(std::uint8_t rd_idx) {
    if (rd_idx < kNumRegs) scoreboard_[rd_idx] = false;
  }

  // 全部清除
  void reset() { scoreboard_.fill(false); }

  // 当前飞行中寄存器数量
  std::size_t in_flight_count() const {
    std::size_t count = 0;
    for (auto b : scoreboard_) if (b) ++count;
    return count;
  }

  // ------------------------------------------------------------------------
  // build() — 注册 decode (检测冒险) 和 writeback (清除飞行标记)
  // ------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

    // decode 阶段: 检测冒险并标记新的飞行中寄存器
    pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("decode").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);

        // 检测 RAW / WAW 冒险
        bool hazard = this->has_hazard(dec);
        if (hazard) {
          // M2 阶段: 仅记录冒险, 不实际阻塞 (M4 集成后通过 CtrlLink 阻塞)
          // 当前仅设置 Payload 标志供测试验证
          // 不 mark_in_flight (阻塞时该指令不进入执行)
        } else {
          // 无冒险: 标记目标寄存器为飞行中
          if (dec.writes_rd) {
            this->mark_in_flight(dec.rd_idx);
          }
        }
      }
    });

    // writeback 阶段: 清除飞行标记
    pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("writeback").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);
        if (dec.writes_rd) {
          this->clear_in_flight(dec.rd_idx);
        }
      }
    });
  }

 private:
  std::array<bool, kNumRegs> scoreboard_{};
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_HAZARD_H
