// ip/cpu/plugins/branch_predictor.h
//
// 功能描述: BranchPredictorPlugin — 分支预测器 (M2.3, P1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P1 (非 ISA-无关但基础): 分支预测是 CPU 核心逻辑
//   - 3 种预测模式: BTB (分支目标缓冲) + Bimodal (2-bit 饱和计数器) + GShare (全局历史)
//   - 模板参数化 xlen (与 RegFilePlugin/HazardPlugin 一致)
//   - fetch 阶段: 预测下一条 PC (跳转目标或顺序+4)
//   - execute 阶段: 验证预测, 更新 BTB/计数器
//
// 借鉴:
//   - L1CachePlugin 6 维度方法学 D2 (范式合规)
//   - 经典 5 级 RISC-V 流水线分支预测器
//
// 约束:
//   - 头文件为主 (.cpp 仅 stub)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_H
#define CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_H

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
// BranchPredictorPlugin — 分支预测器
//
// 模板参数 T: uint32_t (RV32) / uint64_t (RV64)
//
// 组件:
//   1. BTB (Branch Target Buffer): 直接映射, 16 条目
//      - tag: PC 高位, target: 跳转目标地址
//   2. Bimodal: 2-bit 饱和计数器, 16 条目
//      - 00=强不跳转, 01=弱不跳转, 10=弱跳转, 11=强跳转
//   3. GShare: 全局历史寄存器 (8-bit) XOR PC 低位索引
//      - 2-bit 计数器, 16 条目
//
// 使用方式:
//   - fetch 阶段: predict(pc) → 预测目标地址或 0 (不跳转)
//   - execute 阶段: update(pc, actual_taken, actual_target) → 更新 BTB/计数器
// ----------------------------------------------------------------------------
template <typename T>
class BranchPredictorPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "BranchPredictorPlugin<T>: T must be unsigned");

 public:
  static constexpr std::size_t kBtbSize = 16;       // BTB 条目数
  static constexpr std::size_t kBimodalSize = 16;   // Bimodal 计数器数量
  static constexpr std::size_t kGshareSize = 16;    // GShare 计数器数量
  static constexpr std::size_t kHistoryBits = 8;    // 全局历史位数

  // 2-bit 饱和计数器状态
  enum class Counter : std::uint8_t {
    STRONG_NOT_TAKEN = 0,  // 00
    WEAK_NOT_TAKEN = 1,    // 01
    WEAK_TAKEN = 2,        // 10
    STRONG_TAKEN = 3,      // 11
  };

  struct BtbEntry {
    T tag;       // PC 标签 (用于匹配)
    T target;    // 分支目标地址
    bool valid;  // 有效位
  };

  BranchPredictorPlugin() { reset(); }
  ~BranchPredictorPlugin() override = default;

  BranchPredictorPlugin(const BranchPredictorPlugin&) = delete;
  BranchPredictorPlugin& operator=(const BranchPredictorPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  // ------------------------------------------------------------------------
  // build() — 注册 fetch (预测) 和 execute (更新) 阶段
  // ------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using DecodePayload = cf::cpu::core::payload::DecodePayload;

    // fetch 阶段: 预测分支 (基于当前 PC)
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (n) {
        T pc = n->operator()(KeyType::PC);

        T predicted = this->predict(pc);
        if (predicted != 0) {
          // 预测跳转: 设置 next_pc 为预测目标
          // M2 阶段: 仅存储预测结果, M4 集成后通过 CtrlLink 修改 PC
        }
      }
    });

    // execute 阶段: 验证预测并更新
    pb.at_stage("execute", cf::plugin::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);
        if (dec.op_class == DecodePayload::OpClass::BRANCH) {
          T pc = n->operator()(KeyType::PC);
          this->update(pc, dec.branch_taken, static_cast<T>(dec.branch_target));
        }
      }
    });
  }

  // ------------------------------------------------------------------------
  // 预测 API
  // ------------------------------------------------------------------------

  // 预测分支目标地址
  // 返回 0: 不跳转 (顺序执行)
  // 返回非 0: 预测跳转目标
  T predict(T pc) const {
    std::size_t idx = pc & (kBtbSize - 1);
    const auto& entry = btb_[idx];
    if (!entry.valid || entry.tag != pc) return T{0};

    // 使用 GShare > Bimodal > 默认不跳转 的优先级
    bool gshare_taken = gshare_predict(pc);
    bool bimodal_taken = bimodal_predict(pc);

    // 简单策略: GShare 优先
    if (gshare_taken || bimodal_taken) {
      return entry.target;
    }
    return T{0};
  }

  // 单独预测是否跳转 (用于测试)
  bool predict_taken(T pc) const {
    return gshare_predict(pc) || bimodal_predict(pc);
  }

  // ------------------------------------------------------------------------
  // 更新 API
  // ------------------------------------------------------------------------

  // 更新预测器 (execute 阶段调用)
  void update(T pc, bool taken, T target) {
    std::size_t idx = pc & (kBtbSize - 1);

    // 更新 BTB
    btb_[idx].tag = pc;
    btb_[idx].target = target;
    btb_[idx].valid = true;

    // 更新 Bimodal
    bimodal_update(pc, taken);

    // 更新 GShare
    gshare_update(pc, taken);
  }

  // ------------------------------------------------------------------------
  // 单元测试辅助 API
  // ------------------------------------------------------------------------

  // 查询 BTB 条目
  const BtbEntry& btb_entry(std::size_t idx) const {
    return btb_[idx % kBtbSize];
  }

  // 查询 Bimodal 计数器
  Counter bimodal_counter(std::size_t idx) const {
    return bimodal_[idx % kBimodalSize];
  }

  // 查询 GShare 计数器
  Counter gshare_counter(std::size_t idx) const {
    return gshare_[idx % kGshareSize];
  }

  // 当前全局历史
  std::uint8_t global_history() const { return global_history_; }

  // 重置所有状态
  void reset() {
    for (auto& e : btb_) { e = BtbEntry{T{0}, T{0}, false}; }
    bimodal_.fill(Counter::WEAK_NOT_TAKEN);
    gshare_.fill(Counter::WEAK_NOT_TAKEN);
    global_history_ = 0;
  }

  // 预测准确率统计 (M4 后完善)
  std::size_t total_branches() const { return total_branches_; }
  std::size_t correct_predictions() const { return correct_predictions_; }

 private:
  // BTB
  std::array<BtbEntry, kBtbSize> btb_{};

  // Bimodal 计数器
  std::array<Counter, kBimodalSize> bimodal_{};

  // GShare 计数器
  std::array<Counter, kGshareSize> gshare_{};

  // 全局历史寄存器 (8-bit)
  std::uint8_t global_history_ = 0;

  // 统计
  std::size_t total_branches_ = 0;
  std::size_t correct_predictions_ = 0;

  // Bimodal 预测
  bool bimodal_predict(T pc) const {
    std::size_t idx = pc & (kBimodalSize - 1);
    auto c = bimodal_[idx];
    return c == Counter::WEAK_TAKEN || c == Counter::STRONG_TAKEN;
  }

  // Bimodal 更新
  void bimodal_update(T pc, bool taken) {
    std::size_t idx = pc & (kBimodalSize - 1);
    auto& c = bimodal_[idx];
    auto val = static_cast<std::uint8_t>(c);
    if (taken) {
      if (val < 3) ++val;
    } else {
      if (val > 0) --val;
    }
    c = static_cast<Counter>(val);
  }

  // GShare 预测
  bool gshare_predict(T pc) const {
    std::size_t idx = (pc ^ global_history_) & (kGshareSize - 1);
    auto c = gshare_[idx];
    return c == Counter::WEAK_TAKEN || c == Counter::STRONG_TAKEN;
  }

  // GShare 更新
  void gshare_update(T pc, bool taken) {
    std::size_t idx = (pc ^ global_history_) & (kGshareSize - 1);
    auto& c = gshare_[idx];
    auto val = static_cast<std::uint8_t>(c);
    if (taken) {
      if (val < 3) ++val;
    } else {
      if (val > 0) --val;
    }
    c = static_cast<Counter>(val);

    // 更新全局历史 (左移, 最低位为 taken)
    global_history_ = ((global_history_ << 1) | (taken ? 1 : 0)) & ((1 << kHistoryBits) - 1);
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_H
