// ip/cpu/plugins/mmu_ptw_chmem.h
//
// Phase 6d.6 (Oracle/Metis 修订): MMU PTW (Page Table Walker) FSM — sv32 5 状态
//
// 状态: IDLE → L0_WAIT → L1_WAIT → DONE / FAULT
//
// ============================================================================
// ADR-046 FSM 豁免 (多周期协议引擎)
// ============================================================================
//   本 Plugin 是固有的多周期有限状态机 (页表 walk 逐级等待内存响应), 豁免 D4
//   "无状态机" 禁令 (ADR-046)。必须且只能使用 chlib::ch_state_machine DSL:
//
//     #define CF_PLUGIN_USE_FSM_EXEMPT   // ADR-046: 多周期 FSM 豁免标记
//
//   Check 2 (verify_plugin_decision.sh) 检测到该标记后跳过本文件的
//   "enum class.*State" 状态机检查; check_plugin_portability.sh Check 5
//   at_stage 内禁 if(ch_bool) 同样据此豁免。
// ============================================================================
//
// sv32 页表 (2 级, RISC-V Privileged Spec v1.12 §5.3):
//   vaddr[31:22] = VPN[1]   (10 bit, level-1 索引)
//   vaddr[21:12] = VPN[0]   (10 bit, level-0 索引)
//   vaddr[11:0]  = offset
//
//   root   = satp.ppn << 12                (页表基址)
//   pte1   = root + VPN[1] * 4             (level-1 PTE 地址)
//   pte0   = pte1.ppn << 12 + VPN[0] * 4   (level-0 PTE 地址; 若 level-1 是叶则停)
//
//   PTE (32-bit sv32):
//     bit[0]  V   有效位
//     bit[1]  R   可读
//     bit[2]  W   可写
//     bit[3]  X   可执行
//     bit[4]  U
//     bit[5]  G
//     bit[6]  A
//     bit[7]  D
//     bit[9:8]   RSW
//     bit[31:10] PPN (22 bit)
//
//   PTE 类型判定 (R/W/X 组合, spec Table 5.1):
//     {0,0,0} = 非叶指针 (指向下一级页表)
//     {1,0,0} = RESERVED (保留编码 → 页错误)
//     其它 (有 R 或 X) = 叶 (翻译结果, ppn 拼接下级偏移)
//
// sv32 walk 故障优先级 (spec §5.4):
//   1. 任何一级 PTE.V == 0            → 页错误 (invalid)
//   2. 保留编码 {R,W,X} = {1,0,0}      → 页错误 (reserved)
//   3. 非叶 PTE 未对齐 (PoC 不检查)    → 页错误
//   4. 叶 PTE 权限不满足访问类型        → 页错误 (PoC 不检查, 只走 walk)
//
// FSM (5 状态, Oracle 修订: sv32 是 2 级 walk):
//
//   IDLE ──start──▶ L0_WAIT ──resp(非叶)──▶ L1_WAIT ──resp(叶)──▶ DONE ──auto──▶ IDLE
//    ▲                                      │                        ▲
//    └──────────────FAULT◀──────────────────┴──(invalid/reserved)────┘
//
//   IDLE    — 空闲, 等待 start (TLB miss → PTW 启动)
//   L0_WAIT — 等 level-1 PTE 内存响应
//   L1_WAIT — 等 level-0 PTE 内存响应 (非叶指针继续)
//   DONE    — walk 成功, 输出 ppn + perms, 下拍回 IDLE
//   FAULT   — walk 失败, 输出 fault_code, 下拍回 IDLE
//
// 输入信号 (测试经 ch_uint 句柄 set_input_value 驱动):
//   start      — walk 请求 (TLB miss)
//   pte_valid  — 内存响应有效 (当前级 PTE 已就绪)
//   pte_v      — 当前级 PTE.V
//   pte_r/w/x  — 当前级 PTE R/W/X
//   pte_ppn    — 当前级 PTE.PPN (22 bit, 直接透传到 DONE/FAULT 结果)
//
// 输出信号 (测试经 get_value 读取):
//   state      — 当前状态 (ch_uint<8>, 0..4)
//   done       — DONE 中为 1
//   fault      — FAULT 中为 1
//   fault_code — 故障码 (0=none, 1=invalid, 2=reserved)
//   result_ppn — walk 成功时的 ppn (leaf PTE.PPN)
//
// PoC 简化 (文档化, 见 ADR-046 §2.3 仿真限制):
//   - 内存模型由测试逐级重放 PTE: L0_WAIT 周期喂 level-1 PTE, L1_WAIT 周期
//     喂 level-0 PTE (与 6d.3 cpu_memory_model_chmem 的 byte-equal 参考模型
//     同思路: 组合/寄存器单周期仿真对多周期 FSM 用逐级驱动).
//   - megapage (level-1 叶, 4MB) 的 ppn 拼接简化: PoC 直接透传 pte_ppn,
//     不拼接 vaddr 剩余位 (Phase 6d.6 完整版补).
//   - 无访问类型检查 (R/W/X vs load/store/ifetch) — 只走 walk 成功/失败路径.
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-22 (Phase 6d.6 PoC)

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CPU_PLUGINS_MMU_PTW_CHMEM_H
#define CF_IP_CPU_PLUGINS_MMU_PTW_CHMEM_H

// ADR-046: 多周期协议引擎豁免 D4 无状态机禁令 (verify_plugin_decision.sh
// Check 2 检测此标记跳过本文件的 enum class.*State 检查)
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
namespace cpu {
namespace plugins {

// ============================================================================
// PtWalkState — sv32 PTW 5 状态 (ADR-046 §2.2 规范命名, FAULT 复用第 4 编码)
//
// 映射 (Oracle 修正: sv32 不是 sv39 3-level):
//   0 = IDLE     空闲
//   1 = L0_WAIT  等 level-1 PTE (root)
//   2 = L1_WAIT  等 level-0 PTE (leaf)
//   3 = DONE     walk 成功
//   4 = FAULT    walk 失败
// ============================================================================
enum class PtWalkState : uint8_t {
  IDLE = 0,
  L0_WAIT = 1,
  L1_WAIT = 2,
  DONE = 3,
  FAULT = 4,
};

// ============================================================================
// PtWalkFsmPlugin — sv32 页表 walk FSM (ADR-046 豁免, ch_state_machine DSL)
//
// 生命周期 (CH_MEM):
//   1. setup(): 注册 at_stage("mmu_ptw", NORMAL) 闭包 (elaborate 期构建 FSM)
//   2. build(): 空 (所有工作延迟到 elaborate, ctx_swap 活跃时)
//   3. elaborate(): 闭包执行 → 创建信号/FSM → 发射 select-tree DAG
//   4. 测试: create_simulator → set_input_value(start/pte_*) → tick →
//      get_value(state/done/fault/result_ppn)
// ============================================================================
template <typename T = std::uint32_t>
class PtWalkFsmPlugin : public PluginBase {
  static constexpr unsigned kXlenBits = 32;   // sv32: 32-bit vaddr
  static constexpr unsigned kPtrBits  = 22;   // sv32 PTE.PPN / satp.ppn 位宽
  static constexpr unsigned kStateBits = 8;   // state 输出位宽 (方便读值与断言)

 public:
  PtWalkFsmPlugin() = default;
  ~PtWalkFsmPlugin() override = default;
  PtWalkFsmPlugin(const PtWalkFsmPlugin&) = delete;
  PtWalkFsmPlugin& operator=(const PtWalkFsmPlugin&) = delete;

  // --------------------------------------------------------------------------
  // setup — 注册 elaborate 闭包
  // --------------------------------------------------------------------------
  void setup(PipeBuilder& pb) override {
    pb.at_stage("mmu_ptw", Phase::NORMAL, [this] { this->create_fsm(); });
  }

  // --------------------------------------------------------------------------
  // build — 空 (节点创建延迟到 elaborate 期 ctx_swap 活跃时)
  // --------------------------------------------------------------------------
  void build(PipeBuilder&) override {}

  // --------------------------------------------------------------------------
  // 输入信号访问器 (测试驱动)
  // --------------------------------------------------------------------------
  ch_uint<1>& start() { return *start_; }
  ch_uint<1>& pte_valid() { return *pte_valid_; }
  ch_uint<1>& pte_v() { return *pte_v_; }
  ch_uint<1>& pte_r() { return *pte_r_; }
  ch_uint<1>& pte_w() { return *pte_w_; }
  ch_uint<1>& pte_x() { return *pte_x_; }
  ch_uint<kPtrBits>& pte_ppn() { return *pte_ppn_; }

  // --------------------------------------------------------------------------
  // 输出信号访问器 (测试读取)
  // --------------------------------------------------------------------------
  ch_uint<kStateBits>& state_out() { return *state_out_; }
  ch_uint<1>& done() { return *done_; }
  ch_uint<1>& fault() { return *fault_; }
  ch_uint<8>& fault_code() { return *fault_code_; }
  ch_uint<kPtrBits>& result_ppn() { return *result_ppn_; }

 private:
  // 输入信号 (lazy: elaborate 期创建, ctx_swap 活跃)
  std::unique_ptr<ch_uint<1>>     start_;
  std::unique_ptr<ch_uint<1>>     pte_valid_;
  std::unique_ptr<ch_uint<1>>     pte_v_;
  std::unique_ptr<ch_uint<1>>     pte_r_;
  std::unique_ptr<ch_uint<1>>     pte_w_;
  std::unique_ptr<ch_uint<1>>     pte_x_;
  std::unique_ptr<ch_uint<kPtrBits>> pte_ppn_;

  // 输出信号
  std::unique_ptr<ch_uint<kStateBits>> state_out_;
  std::unique_ptr<ch_uint<1>>     done_;
  std::unique_ptr<ch_uint<1>>     fault_;
  std::unique_ptr<ch_uint<8>>     fault_code_;
  std::unique_ptr<ch_uint<kPtrBits>> result_ppn_;

  // ch_state_machine DSL (ADR-046 强约束)
  std::unique_ptr<chlib::ch_state_machine<PtWalkState, 5>> fsm_;

  // --------------------------------------------------------------------------
  // create_fsm — elaborate 期构建 sv32 walk FSM (一次, DAG 发射)
  //
  // 组合逻辑 (每周期由 Simulator 求值):
  //   L0_WAIT:  pte_valid=1 且 PTE 非叶 (R|W|X == 0) 且 V=1  → L1_WAIT
  //             pte_valid=1 且 PTE 叶 (R|W|X != 0) 且 V=1    → DONE (superpage)
  //             pte_valid=1 且 V=0                           → FAULT (invalid)
  //             pte_valid=1 且 V=1 且 R=1 W=0 X=0            → FAULT (reserved)
  //   L1_WAIT:  同 L0_WAIT (响应为 level-0 PTE, 非叶 → FAULT: 页表层级不符)
  //   DONE/FAULT: 无条件下拍回 IDLE
  //
  // 优先级: last-select-wins (build() 内按注册序累加; 各条件互斥, 无歧义)
  // --------------------------------------------------------------------------
  void create_fsm() {
    // ── 1. 输入信号 (具名节点, 经 set_input_value 驱动) ──────────────
    start_     = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_start");
    pte_valid_ = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_pte_valid");
    pte_v_     = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_pte_v");
    pte_r_     = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_pte_r");
    pte_w_     = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_pte_w");
    pte_x_     = std::make_unique<ch_uint<1>>(ch_literal<0, 1>{}, "ptw_pte_x");
    pte_ppn_   = std::make_unique<ch_uint<kPtrBits>>(
        ch_literal<0, kPtrBits>{}, "ptw_pte_ppn");

    // ── 2. 硬件条件信号 (ch_bool, 每周期求值) ─────────────────────────
    auto start_sig  = ch_bool(*start_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto pte_ok_sig = ch_bool(*pte_valid_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto v_sig      = ch_bool(*pte_v_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto r_sig      = ch_bool(*pte_r_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto w_sig      = ch_bool(*pte_w_ == ch_uint<1>(ch_literal<1, 1>{}));
    auto x_sig      = ch_bool(*pte_x_ == ch_uint<1>(ch_literal<1, 1>{}));

    // PTE 类型判定 (任务规范: reserved = R=1 && W=1):
    //   is_invalid = V=0
    //   is_reserved = V=1 && R=1 && W=1  (保留编码 → 页错误)
    //   is_leaf     = V=1 && (R=1 或 X=1) 且非保留  (叶 PTE → DONE)
    //   is_ptr      = V=1 && R=W=X=0             (非叶指针 → 下一级)
    // 四者互斥 → select-tree 优先级无歧义.
    ch_bool is_reserved = r_sig && w_sig;
    ch_bool is_leaf     = (r_sig || x_sig) && !is_reserved;
    ch_bool is_ptr      = !r_sig && !w_sig && !x_sig;

    // ── 3. ch_state_machine DSL (ADR-046 强制) ───────────────────────
    fsm_ = std::make_unique<chlib::ch_state_machine<PtWalkState, 5>>();
    auto& sm = *fsm_;

    sm.state(PtWalkState::IDLE).on_active([&] {
      sm.transition_when(start_sig, PtWalkState::L0_WAIT);
    });
    sm.state(PtWalkState::L0_WAIT).on_active([&] {
      // 互斥条件 (任务规范优先级):
      //   V=0 → invalid fault; V=1 && R&&W → reserved fault
      //   V=1 && 叶 → DONE (level-1 superpage, PoC 直接透传 ppn)
      //   V=1 && 非叶指针 → L1_WAIT
      sm.transition_when(pte_ok_sig && !v_sig, PtWalkState::FAULT);
      sm.transition_when(pte_ok_sig && v_sig && is_reserved, PtWalkState::FAULT);
      sm.transition_when(pte_ok_sig && v_sig && is_leaf, PtWalkState::DONE);
      sm.transition_when(pte_ok_sig && v_sig && is_ptr, PtWalkState::L1_WAIT);
    });
    sm.state(PtWalkState::L1_WAIT).on_active([&] {
      // level-0 PTE: 必须为叶; V=0 / 保留 / 非叶指针 → FAULT
      sm.transition_when(pte_ok_sig && !v_sig, PtWalkState::FAULT);
      sm.transition_when(pte_ok_sig && v_sig && is_reserved, PtWalkState::FAULT);
      sm.transition_when(pte_ok_sig && v_sig && is_ptr, PtWalkState::FAULT);
      sm.transition_when(pte_ok_sig && v_sig && is_leaf, PtWalkState::DONE);
    });
    sm.state(PtWalkState::DONE).on_active([&] {
      sm.transition_to(PtWalkState::IDLE);  // 无条件下拍回 IDLE
    });
    sm.state(PtWalkState::FAULT).on_active([&] {
      sm.transition_to(PtWalkState::IDLE);  // 无条件下拍回 IDLE
    });

    sm.set_entry(PtWalkState::IDLE);
    sm.build();

    // ── 4. 输出信号 (具名代理, 经 get_value 读取) ────────────────────
    state_out_ = std::make_unique<ch_uint<kStateBits>>(
        ch_uint<kStateBits>(sm.current_state_uint()), "ptw_state_out");
    done_ = std::make_unique<ch_uint<1>>(
        ch_uint<1>(sm.is_in(PtWalkState::DONE)), "ptw_done");
    fault_ = std::make_unique<ch_uint<1>>(
        ch_uint<1>(sm.is_in(PtWalkState::FAULT)), "ptw_fault");
    fault_code_ = std::make_unique<ch_uint<8>>(
        select(sm.is_in(PtWalkState::FAULT),
               select(ch_bool(!v_sig),
                      ch_uint<8>(1),  // V=0 → invalid
                      ch_uint<8>(2)), // V=1 且保留编码 → reserved
               ch_uint<8>(0)),
        "ptw_fault_code");
    result_ppn_ = std::make_unique<ch_uint<kPtrBits>>(
        select(sm.is_in(PtWalkState::DONE), *pte_ppn_,
               ch_uint<kPtrBits>(ch_literal<0, kPtrBits>{})),
        "ptw_result_ppn");
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_PTW_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM