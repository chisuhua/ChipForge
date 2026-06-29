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
#include <vector>

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
//   1. BTB (Branch Target Buffer): 直接映射, 大小由 ctor 参数 btb_entries 决定
//      (M4.14: BTB_SIZE 从模板参数移至运行时构造参数, 由 CPUConfig::btb_entries 注入)
//      - tag: PC 高位, target: 跳转目标地址
//   2. Bimodal: 2-bit 饱和计数器, BIMODAL_SZ 条目
//      - 00=强不跳转, 01=弱不跳转, 10=弱跳转, 11=强跳转
//   3. GShare: 全局历史寄存器 (GHR_BITS-bit) XOR PC 低位索引
//      - 2-bit 计数器, GSHARE_SZ 条目
//
// 使用方式:
//   - fetch 阶段: predict(pc) → 预测目标地址或 0 (不跳转)
//   - execute 阶段: update(pc, actual_taken, actual_target) → 更新 BTB/计数器
// ----------------------------------------------------------------------------
template <typename T,
          std::size_t BIMODAL_SZ = 16,
          std::size_t GSHARE_SZ = 16,
          std::uint8_t GHR_BITS = 8,
          std::size_t N_THREADS = 1>
class BranchPredictorPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "BranchPredictorPlugin<T>: T must be unsigned");
  static_assert(N_THREADS >= 1 && N_THREADS <= 4,
                "BranchPredictorPlugin: N_THREADS must be in [1, 4]");
  // M4.14: BTB_SIZE 从模板参数移至运行时构造参数 btb_entries
  // (CPUConfig::btb_entries enum 保证为 16/32/64/128/256 之一的 2 的幂)
  static_assert(BIMODAL_SZ >= 1 && (BIMODAL_SZ & (BIMODAL_SZ - 1)) == 0,
                "BranchPredictorPlugin: BIMODAL_SZ must be power of 2 (>= 1)");
  static_assert(GSHARE_SZ >= 1 && (GSHARE_SZ & (GSHARE_SZ - 1)) == 0,
                "BranchPredictorPlugin: GSHARE_SZ must be power of 2 (>= 1)");
  static_assert(GHR_BITS >= 1 && GHR_BITS <= 16,
                "BranchPredictorPlugin: GHR_BITS must be in [1, 16]");

 public:
  // M4.14: BTB 大小从 constexpr 改为运行时访问器 (ctor 注入)
  static constexpr std::size_t kBimodalSize = BIMODAL_SZ;
  static constexpr std::size_t kGshareSize = GSHARE_SZ;
  static constexpr std::uint8_t kHistoryBits = GHR_BITS;
  static constexpr std::size_t kNumThreads = N_THREADS;    // 全局历史位数

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

  // M4.14: ctor 接受 btb_entries 运行时参数 (从 CPUConfig::btb_entries 注入)
  // btb_entries 必须为 2 的幂 (CPUConfig::btb_entries enum 限定 16/32/64/128/256)
  explicit BranchPredictorPlugin(std::size_t btb_entries)
      : btb_(btb_entries), btb_entries_(btb_entries) {
    reset();
  }
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
    // M4G-extend: tid 从 this->tid_ 读取, factory 端 dispatch 注入

    // fetch 阶段: 预测分支 (基于当前 PC)
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (n) {
        T pc = n->operator()(KeyType::PC);
        T predicted = this->predict(pc, this->tid_);
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
          this->update(pc, dec.branch_taken, static_cast<T>(dec.branch_target),
                       this->tid_);
        }
      }
    });
  }

  // ------------------------------------------------------------------------
  // 预测 API
  // ------------------------------------------------------------------------

  // 预测分支目标地址 (默认 tid=0, 行为不变)
  // 返回 0: 不跳转 (顺序执行)
  // 返回非 0: 预测跳转目标
  T predict(T pc, std::uint8_t tid = 0) const {
    std::size_t idx = pc & (btb_size() - 1);
    const auto& entry = btb_[idx];
    if (!entry.valid || entry.tag != pc) return T{0};

    // 使用 GShare > Bimodal > 默认不跳转 的优先级
    bool gshare_taken = gshare_predict(pc, tid);
    bool bimodal_taken = bimodal_predict(pc, tid);

    // 简单策略: GShare 优先
    if (gshare_taken || bimodal_taken) {
      return entry.target;
    }
    return T{0};
  }

  // 单独预测是否跳转 (用于测试, 默认 tid=0)
  bool predict_taken(T pc, std::uint8_t tid = 0) const {
    return gshare_predict(pc, tid) || bimodal_predict(pc, tid);
  }

  // ------------------------------------------------------------------------
  // 更新 API
  // ------------------------------------------------------------------------

  // 更新预测器 (execute 阶段调用, 默认 tid=0)
  void update(T pc, bool taken, T target, std::uint8_t tid = 0) {
    std::size_t idx = pc & (btb_size() - 1);

    // 更新 BTB
    btb_[idx].tag = pc;
    btb_[idx].target = target;
    btb_[idx].valid = true;

    // 更新 Bimodal
    bimodal_update(pc, taken, tid);

    // 更新 GShare (内部更新 global_history_[tid])
    gshare_update(pc, taken, tid);
  }

  // ------------------------------------------------------------------------
  // 单元测试辅助 API
  // ------------------------------------------------------------------------

  // 查询 BTB 条目
  const BtbEntry& btb_entry(std::size_t idx) const {
    return btb_[idx % btb_size()];
  }

  std::size_t btb_size() const { return btb_entries_; }

  // 查询 Bimodal 计数器
  Counter bimodal_counter(std::size_t idx) const {
    return bimodal_[idx % kBimodalSize];
  }

  // 查询 GShare 计数器
  Counter gshare_counter(std::size_t idx) const {
    return gshare_[idx % kGshareSize];
  }

  // 当前全局历史 (默认 tid=0, 行为不变)
  std::uint8_t global_history(std::uint8_t tid = 0) const {
    return tid < N_THREADS ? global_history_[tid] : 0;
  }

  // 重置所有状态 (清空所有线程)
  void reset() {
    for (auto& e : btb_) { e = BtbEntry{T{0}, T{0}, false}; }
    bimodal_.fill(Counter::WEAK_NOT_TAKEN);
    gshare_.fill(Counter::WEAK_NOT_TAKEN);
    global_history_.fill(0);
  }

  // set_tid —— per-thread dispatch (M4G-extend G.X)
  void set_tid(std::uint8_t tid) override { tid_ = tid; }

  // current_tid —— 测试/调试访问器
  std::uint8_t current_tid() const { return tid_; }

  // 预测准确率统计 (M4 后完善)
  std::size_t total_branches() const { return total_branches_; }
  std::size_t correct_predictions() const { return correct_predictions_; }

 private:
  // BTB (M4.14: std::vector, 大小由 ctor 运行时参数 btb_entries 决定)
  std::vector<BtbEntry> btb_;
  std::size_t btb_entries_;

  // Bimodal 计数器
  std::array<Counter, kBimodalSize> bimodal_{};

  // GShare 计数器
  std::array<Counter, kGshareSize> gshare_{};

  // 全局历史寄存器: per-thread (M4G D.4)
  std::array<std::uint8_t, N_THREADS> global_history_{};

  // 统计
  std::size_t total_branches_ = 0;
  std::size_t correct_predictions_ = 0;
  std::uint8_t tid_ = 0;

  // Bimodal 预测 (per-tid, tid 验证范围内)
  bool bimodal_predict(T pc, std::uint8_t tid) const {
    if (tid >= N_THREADS) return false;
    std::size_t idx = pc & (kBimodalSize - 1);
    auto c = bimodal_[idx];
    return c == Counter::WEAK_TAKEN || c == Counter::STRONG_TAKEN;
  }

  // Bimodal 更新 (per-tid, BTB/Bimodal 共享)
  void bimodal_update(T pc, bool taken, std::uint8_t tid) {
    if (tid >= N_THREADS) return;
    (void)tid;  // Bimodal 表本身是共享的
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

  // GShare 预测 (per-tid: 使用 global_history_[tid])
  bool gshare_predict(T pc, std::uint8_t tid) const {
    if (tid >= N_THREADS) return false;
    std::size_t idx = (pc ^ global_history_[tid]) & (kGshareSize - 1);
    auto c = gshare_[idx];
    return c == Counter::WEAK_TAKEN || c == Counter::STRONG_TAKEN;
  }

  // GShare 更新 (per-tid: 更新 global_history_[tid])
  void gshare_update(T pc, bool taken, std::uint8_t tid) {
    if (tid >= N_THREADS) return;
    std::size_t idx = (pc ^ global_history_[tid]) & (kGshareSize - 1);
    auto& c = gshare_[idx];
    auto val = static_cast<std::uint8_t>(c);
    if (taken) {
      if (val < 3) ++val;
    } else {
      if (val > 0) --val;
    }
    c = static_cast<Counter>(val);

    // 更新 per-thread 全局历史 (左移, 最低位为 taken)
    global_history_[tid] = ((global_history_[tid] << 1) | (taken ? 1 : 0))
                           & ((1 << kHistoryBits) - 1);
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_H
