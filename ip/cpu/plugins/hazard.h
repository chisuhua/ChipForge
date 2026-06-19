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

// M4G D.3 (G.5): HazardKind enum — 区分 4 种冒险状态
//   - NONE: 无冒险
//   - RAW_RS1: 读 RS1 时遇到 RAW 冒险 (Phase 5+ 发射端口 1)
//   - RAW_RS2: 读 RS2 时遇到 RAW 冒险 (Phase 5+ 发射端口 2)
//   - WAW: 写 RD 时遇到 WAW 冒险 (Phase 5+ 提交端口)
// 检测顺序 RAW_RS1 > RAW_RS2 > WAW (第一个匹配即返回)
enum class HazardKind : std::uint8_t {
  NONE = 0,
  RAW_RS1,
  RAW_RS2,
  WAW,
};

// ----------------------------------------------------------------------------
// HazardPlugin — 数据冒险检测
//
// 模板参数 T: uint32_t (RV32) / uint64_t (RV64)
// 寄存器数量: 32 (x0-x31)
//
// Scoreboard 模型:
//   - scoreboard_[tid][i] = true: 寄存器 i 正在被某条指令写入 (飞行中, 线程 tid)
//   - scoreboard_[tid][i] = false: 寄存器 i 可用
//
// 检测逻辑:
//   RAW: reads_rs1 && scoreboard_[tid][rs1_idx] → 冒险
//   RAW: reads_rs2 && scoreboard_[tid][rs2_idx] → 冒险
//   WAW: writes_rd && scoreboard_[tid][rd_idx] → 冒险
//   WAR: 不阻塞 (读在写之前, 不冲突)
//
// M4G D.2 (G.3) 模板参数化扩展:
//   N_REGS    寄存器数量 (默认 32, 范围 [1, 128], 推荐 2 的幂)
//   N_THREADS 线程数量 (默认 1, 范围 [1, 4], SMT 前瞻锁定)
//   所有 scoreboard API 接受 tid 默认参数 (默认 0, 保持 ABI 兼容)
//   has_hazard 仍返回 bool (D.3 改 HazardKind, 推迟到 G.5)
// ----------------------------------------------------------------------------
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class HazardPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "HazardPlugin<T>: T must be unsigned");
  static_assert(N_REGS >= 1 && N_REGS <= 128,
                "HazardPlugin: N_REGS must be in [1, 128]");
  static_assert(N_THREADS >= 1 && N_THREADS <= 4,
                "HazardPlugin: N_THREADS must be in [1, 4]");
  static_assert((N_REGS & (N_REGS - 1)) == 0 || N_REGS == 1,
                "HazardPlugin: N_REGS should be power of 2 (or 1)");

 public:
  static constexpr std::size_t kNumRegs = N_REGS;
  static constexpr std::size_t kNumThreads = N_THREADS;

  HazardPlugin() { reset(); }
  ~HazardPlugin() override = default;

  HazardPlugin(const HazardPlugin&) = delete;
  HazardPlugin& operator=(const HazardPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  // ------------------------------------------------------------------------
  // 单元测试辅助 API
  // ------------------------------------------------------------------------

  // 检测 RAW 冒险 (指定寄存器索引, 默认 tid=0)
  bool has_raw(std::uint8_t rs_idx, std::uint8_t tid = 0) const {
    return tid < N_THREADS && rs_idx < kNumRegs && scoreboard_[tid][rs_idx];
  }

  // 检测 WAW 冒险 (默认 tid=0)
  bool has_waw(std::uint8_t rd_idx, std::uint8_t tid = 0) const {
    return tid < N_THREADS && rd_idx < kNumRegs && scoreboard_[tid][rd_idx];
  }

  // 检测完整指令冒险 (默认 tid=0, M4G D.3 返回 HazardKind 区分冒险源)
  HazardKind has_hazard(const cf::cpu::core::payload::DecodePayload& dec,
                        std::uint8_t tid = 0) const {
    if (dec.reads_rs1 && has_raw(dec.rs1_idx, tid)) return HazardKind::RAW_RS1;
    if (dec.reads_rs2 && has_raw(dec.rs2_idx, tid)) return HazardKind::RAW_RS2;
    if (dec.writes_rd && has_waw(dec.rd_idx, tid)) return HazardKind::WAW;
    return HazardKind::NONE;
  }

  // 标记寄存器为飞行中 (默认 tid=0)
  void mark_in_flight(std::uint8_t rd_idx, std::uint8_t tid = 0) {
    if (tid < N_THREADS && rd_idx < kNumRegs) scoreboard_[tid][rd_idx] = true;
  }

  // 清除飞行标记 (默认 tid=0)
  void clear_in_flight(std::uint8_t rd_idx, std::uint8_t tid = 0) {
    if (tid < N_THREADS && rd_idx < kNumRegs) scoreboard_[tid][rd_idx] = false;
  }

  // 全部清除 (默认清空所有线程, 保持向后兼容)
  void reset() {
    for (std::size_t t = 0; t < N_THREADS; ++t) {
      scoreboard_[t].fill(false);
    }
  }

  // 当前飞行中寄存器数量 (默认 tid=0, 行为不变)
  std::size_t in_flight_count(std::uint8_t tid = 0) const {
    if (tid >= N_THREADS) return 0;
    std::size_t count = 0;
    for (auto b : scoreboard_[tid]) if (b) ++count;
    return count;
  }

  // ------------------------------------------------------------------------
  // build() — 注册 decode (检测冒险) 和 writeback (清除飞行标记)
  // ------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    const std::uint8_t tid = 0;  // Phase 1: 单线程硬编码

    // decode 阶段: 检测冒险并标记新的飞行中寄存器
    pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb, tid]() {
      auto* n = pb.node_of_logic_stage("decode").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);

        // 检测 RAW / WAW 冒险 (M4G D.3: has_hazard 返回 HazardKind)
        HazardKind hazard = this->has_hazard(dec, tid);
        if (hazard != HazardKind::NONE) {
          // M2 阶段: 仅记录冒险, 不实际阻塞 (M4 集成后通过 CtrlLink 阻塞)
          // 当前仅设置 Payload 标志供测试验证
          // 不 mark_in_flight (阻塞时该指令不进入执行)
        } else {
          // 无冒险: 标记目标寄存器为飞行中
          if (dec.writes_rd) {
            this->mark_in_flight(dec.rd_idx, tid);
          }
        }
      }
    });

    // writeback 阶段: 清除飞行标记
    pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb, tid]() {
      auto* n = pb.node_of_logic_stage("writeback").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);
        if (dec.writes_rd) {
          this->clear_in_flight(dec.rd_idx, tid);
        }
      }
    });
  }

 private:
  std::array<std::array<bool, kNumRegs>, N_THREADS> scoreboard_{};
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_HAZARD_H
