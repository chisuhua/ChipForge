// ip/cache/tlm/l1_cache_refill_fsm_chmem.h
//
// Phase 6d.7: L1Cache refill FSM — 4 状态 (IDLE/LOOKUP/MISS/REFILL_WAIT)
//
// ============================================================================
// ADR-046 FSM 豁免 (多周期协议引擎)
// ============================================================================
//   本 Plugin 是固有的多周期有限状态机 (cache miss → refill 等待内存返回),
//   豁免 D4 "无状态机" 禁令 (ADR-046)。必须且只能使用 chlib::ch_state_machine
//   DSL:
//
//     #define CF_PLUGIN_USE_FSM_EXEMPT   // ADR-046: 多周期 FSM 豁免标记
//
//   verify_plugin_decision.sh Check 2 检测到该标记后跳过本文件的
//   "enum class.*State" 检查; check_plugin_portability.sh Check 9 检测到该
//   标记后验证本文件确实使用了 ch_state_machine DSL。
// ============================================================================
//
// L1Cache lookup + refill 两阶段 (Phase 1.2 L1CachePlugin TLM 参考):
//   lookup:  tag compare + valid bit → hit / miss
//   refill:  miss 时向内存请求整行, 等 mem_rdata_valid 写回 cache
//
// FSM (4 状态):
//
//   IDLE ──lookup_request──▶ LOOKUP ──hit──▶ IDLE
//    ▲                       │
//    │                       └──miss──▶ MISS ──issue──▶ REFILL_WAIT
//    └────────────mem_rdata_valid◀─────────────────────┘
//
//   IDLE        — 等待 lookup 请求
//   LOOKUP      — tag 比较 + valid 位; hit → 输出数据回 IDLE; miss → MISS
//   MISS        — 向内存发 refill 请求 (mem_addr + we=false)
//   REFILL_WAIT — 等 mem_rdata_valid; 数据写回 cache 后回 IDLE
//
// 输入信号 (测试经 ch_uint 句柄 set_input_value 驱动):
//   lookup_request  — lookup 请求有效
//   tag_match       — tag 比较结果 (外部 tag array 提供)
//   line_valid      — cache line valid 位
//   mem_rdata_valid — refill 数据有效 (内存响应)
//
// 输出信号 (测试经 get_value 读取):
//   state      — 当前状态 (ch_uint<8>, 0..3)
//   hit        — LOOKUP 中 tag_match && line_valid (命中, 数据可用)
//   miss       — LOOKUP 中 tag 未命中或 line 无效
//   refill_req — MISS 中 (正在向内存发 refill 请求)
//   refill_done— REFILL_WAIT 中 mem_rdata_valid (refill 完成)
//
// 并发 race 语义 (PoC 第 3 个 test):
//   - REFILL_WAIT 期间新 lookup_request 到达: FSM 停留在 REFILL_WAIT,
//     不中断正在进行的 refill; refill 完成后回 IDLE 才接受新请求.
//   - 与 TLM 版 L1CachePlugin 的 refill 行为对齐 (Phase 1.2 语义).
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-22 (Phase 6d.7 PoC)

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CACHE_TLM_L1_CACHE_REFILL_FSM_CHMEM_H
#define CF_IP_CACHE_TLM_L1_CACHE_REFILL_FSM_CHMEM_H

// ADR-046: 多周期协议引擎豁免 D4 无状态机禁令
#define CF_PLUGIN_USE_FSM_EXEMPT

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <chlib/state_machine.h>
#include <core/bool.h>
#include <core/context.h>
#include <core/operators.h>
#include <core/reg.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf {
namespace ip {
namespace cache {
namespace tlm {

// ============================================================================
// RefillState — L1Cache refill 4 状态
// ============================================================================
enum class RefillState : uint8_t {
  IDLE = 0,
  LOOKUP = 1,
  MISS = 2,
  REFILL_WAIT = 3,
};

// ============================================================================
// L1CacheRefillFsmPlugin — refill FSM (ADR-046 豁免, ch_state_machine DSL)
// ============================================================================
template <typename T = std::uint32_t>
class L1CacheRefillFsmPlugin : public PluginBase {
  static constexpr unsigned kStateBits = 8;  // state 输出位宽

 public:
  L1CacheRefillFsmPlugin() = default;
  ~L1CacheRefillFsmPlugin() override = default;
  L1CacheRefillFsmPlugin(const L1CacheRefillFsmPlugin&) = delete;
  L1CacheRefillFsmPlugin& operator=(const L1CacheRefillFsmPlugin&) = delete;

  void setup(PipeBuilder& pb) override {
    pb.at_stage("cache_refill", Phase::NORMAL, [this] { this->create_fsm(); });
  }

  void build(PipeBuilder&) override {}

  // 输入信号访问器
  ch_uint<1>& lookup_request() { return *lookup_request_; }
  ch_uint<1>& tag_match() { return *tag_match_; }
  ch_uint<1>& line_valid() { return *line_valid_; }
  ch_uint<1>& mem_rdata_valid() { return *mem_rdata_valid_; }

  // 输出信号访问器 (经 get_value 读取)
  ch_uint<kStateBits>& state_out() { return *state_out_; }
  ch_uint<1>& hit() { return *hit_cap_; }
  ch_uint<1>& miss() { return *miss_cap_; }
  ch_uint<1>& refill_req() { return *refill_req_cap_; }
  ch_uint<1>& refill_done() { return *refill_done_cap_; }

 private:
  std::unique_ptr<ch_uint<1>> lookup_request_;
  std::unique_ptr<ch_uint<1>> tag_match_;
  std::unique_ptr<ch_uint<1>> line_valid_;
  std::unique_ptr<ch_uint<1>> mem_rdata_valid_;

  std::unique_ptr<ch_uint<kStateBits>> state_out_;
  // 状态标志捕获寄存器: hit/miss/refill_req/refill_done 是单拍脉冲信号,
  // 用 ch_reg 在对应状态活跃拍锁存, 否则 tick 后状态已转移, 组合 is_in()
  // 读不到旧状态 (信号丢失).
  std::unique_ptr<ch_reg<ch_uint<1>>> hit_cap_;
  std::unique_ptr<ch_reg<ch_uint<1>>> miss_cap_;
  std::unique_ptr<ch_reg<ch_uint<1>>> refill_req_cap_;
  std::unique_ptr<ch_reg<ch_uint<1>>> refill_done_cap_;

  std::unique_ptr<chlib::ch_state_machine<RefillState, 4>> fsm_;

  // --------------------------------------------------------------------------
  // create_fsm — elaborate 期构建 refill FSM (一次, DAG 发射)
  //
  // 组合逻辑:
  //   IDLE:        lookup_request → LOOKUP
  //   LOOKUP:      tag_match && line_valid → IDLE (hit, 输出数据)
  //                !(tag_match && line_valid) → MISS
  //   MISS:        无条件 → REFILL_WAIT (refill_req=1 在 MISS 拍)
  //   REFILL_WAIT: mem_rdata_valid → IDLE (refill_done=1 在 REFILL_WAIT 拍)
  // --------------------------------------------------------------------------
  void create_fsm() {
    lookup_request_ = std::make_unique<ch_uint<1>>(
        ch_literal<0, 1>{}, "l1c_lookup_request");
    tag_match_ = std::make_unique<ch_uint<1>>(
        ch_literal<0, 1>{}, "l1c_tag_match");
    line_valid_ = std::make_unique<ch_uint<1>>(
        ch_literal<0, 1>{}, "l1c_line_valid");
    mem_rdata_valid_ = std::make_unique<ch_uint<1>>(
        ch_literal<0, 1>{}, "l1c_mem_rdata_valid");

    auto req_sig  = ch_bool(*lookup_request_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto match_sig = ch_bool(*tag_match_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto valid_sig = ch_bool(*line_valid_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto rv_sig   = ch_bool(*mem_rdata_valid_ == ch_uint<1>(ch_literal<1, 1>{}));

    ch_bool hit_cond = match_sig && valid_sig;  // LOOKUP 命中

    fsm_ = std::make_unique<chlib::ch_state_machine<RefillState, 4>>();
    auto& sm = *fsm_;

    sm.state(RefillState::IDLE).on_active([&] {
      sm.transition_when(req_sig, RefillState::LOOKUP);
    });
    sm.state(RefillState::LOOKUP).on_active([&] {
      sm.transition_when(hit_cond, RefillState::IDLE);       // hit → 回 IDLE
      sm.transition_when(!hit_cond, RefillState::MISS);      // miss → refill
    });
    sm.state(RefillState::MISS).on_active([&] {
      sm.transition_to(RefillState::REFILL_WAIT);            // 无条件发请求
    });
    sm.state(RefillState::REFILL_WAIT).on_active([&] {
      sm.transition_when(rv_sig, RefillState::IDLE);         // 数据到 → 写回
    });

    sm.set_entry(RefillState::IDLE);
    sm.build();

    state_out_ = std::make_unique<ch_uint<kStateBits>>(
        ch_uint<kStateBits>(sm.current_state_uint()), "l1c_state_out");

    // 状态标志捕获: 锁存脉冲, 测试在对应拍读取
    hit_cap_ = std::make_unique<ch_reg<ch_uint<1>>>(
        ch_uint<1>(ch_literal<0, 1>{}), "l1c_hit_cap");
    miss_cap_ = std::make_unique<ch_reg<ch_uint<1>>>(
        ch_uint<1>(ch_literal<0, 1>{}), "l1c_miss_cap");
    refill_req_cap_ = std::make_unique<ch_reg<ch_uint<1>>>(
        ch_uint<1>(ch_literal<0, 1>{}), "l1c_refill_req_cap");
    refill_done_cap_ = std::make_unique<ch_reg<ch_uint<1>>>(
        ch_uint<1>(ch_literal<0, 1>{}), "l1c_refill_done_cap");

    auto in_lookup = sm.is_in(RefillState::LOOKUP);
    auto in_miss = sm.is_in(RefillState::MISS);
    auto in_rwait = sm.is_in(RefillState::REFILL_WAIT);
    (*hit_cap_) <<= select(in_lookup && hit_cond, ch_uint<1>(ch_literal<1, 1>{}),
                           ch_uint<1>(ch_literal<0, 1>{}));
    (*miss_cap_) <<= select(in_lookup && !hit_cond,
                            ch_uint<1>(ch_literal<1, 1>{}),
                            ch_uint<1>(ch_literal<0, 1>{}));
    (*refill_req_cap_) <<= select(in_miss, ch_uint<1>(ch_literal<1, 1>{}),
                                  ch_uint<1>(ch_literal<0, 1>{}));
    (*refill_done_cap_) <<= select(in_rwait && rv_sig,
                                   ch_uint<1>(ch_literal<1, 1>{}),
                                   ch_uint<1>(ch_literal<0, 1>{}));
  }
};

}  // namespace tlm
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_TLM_L1_CACHE_REFILL_FSM_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM