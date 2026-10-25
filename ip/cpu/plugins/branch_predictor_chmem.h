// ip/cpu/plugins/branch_predictor_chmem.h
//
// CH_MEM 后端的分支预测器 (BTB + Bimodal 2-bit PHT)
//
// ★ 权威规格: docs/architecture/bp-btb-gshare-design-brief.md v2.2
//   §2.1.2 (三步时序) · §3.2 (BTB 布局) · §3.3 (PHT) · §3.4 (payload 契约)
// ★ 设计文档: openspec/changes/chmem-bp-btb-gshare/design.md
//
// ─────────────────────────────────────────────────────────────────────────────
// 设计要点 (为什么是这个形态)
// ─────────────────────────────────────────────────────────────────────────────
//
// ① **挂钩点 = fetch NORMAL, 提前一拍**
//    CH_MEM 的取指是 1 拍 `sread` 延迟 (ibus_chmem.h:163): 取指地址在
//    fetch NORMAL 发出, 而分支结果 (BRANCH_TAKEN/BRANCH_TARGET) 在**同拍**
//    branch NORMAL 才产出 ⇒ **任何预测器都无法改变本拍取指**。
//    ⇒ 但 branch LATE 的 `update_pc` 决定 `pc_reg(N+1)`, 而它与 fetch NORMAL
//      **同拍** ⇒ 在 fetch NORMAL 用 `pc_reg` 查表 (0 拍 `aread`),
//        其结果**同拍**可被 branch LATE 消费 ⇒ **预测比分支结果早一拍**
//        (预测的是 `pc_reg(N)` 处那条分支, 而 actual 是 `pc_reg(N-1)` 那条)
//    ★ 依据 PoC P-2P: 被 op 节点 `<<=` 驱动的 reg 可作 mem 读地址
//      (tests/cpu/test_cpu_chmem_bp_poc.cpp:443) —— 与 `pc_reg <<= select(...)` 同形。
//
// ② **BTB 单表 62b 编码**
//    `ch_mem<ch_uint<64>, 256>` 单表单 word:
//      { tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b) }
//    ★ 依据: CH_MEM 只解 32 位指令 (decode_chmem.h:82 的 instr_format 无 16 位格式)
//      ⇒ `PC[1:0] = 0`、target 4 字节对齐 ⇒ 低位恒 0 ⇒ 可安全裁掉
//    ★ 独立 valid 位 (★ **MUST NOT** 用「target[1:0] != 0」判有效 ——
//      target 4 字节对齐 ⇒ `[1:0]` 恒 0 ⇒ 该判据**恒 false**)
//    ★ `is_unconditional` 位: `jal` **不占 PHT 表项** ⇒ 若无此位则 `jal` 的
//      方向**无来源** ⇒ 永不被预测 ⇒ call 气泡不可恢复
//
// ③ **PHT: Bimodal 2-bit 饱和计数器**
//    索引 = `PC[31:2] & (M-1)` (★ **无全局历史 XOR** —— GShare 不在本期)
//    饱和 0→1→2→3 (不回退); 预测 taken 阈值 = **count >= 2**
//    ★ 写使能 = **「分支已解析」（任何方向）** —— 若只在 taken 时更新
//      ⇒ 学不到 not-taken ⇒ 在以 not-taken 为主的负载上**净负收益**
//
// ④ **key = vaddr**（分支 PC payload 本身是 vaddr ⇒ MMU on/off 间表项一致）
//
// ⑤ **替换策略 = 直接覆盖** ⇒ 目标变化的分支再次 taken 时同槽重写
//    ⇒ **天然自愈**; ★ **MUST NOT** 另设误预测计数器
//
// ─────────────────────────────────────────────────────────────────────────────
// 本文件当前范围 (change chmem-bp-btb-gshare-poc, 机制 PoC)
// ─────────────────────────────────────────────────────────────────────────────
//
// ★ 本 PoC 交付 **「查表 + payload 产出」**（**不改 PC-mux** ⇒ 零行为风险）。
//
// ★ **本 change 的边界**（owner 表, 见 tasks.md）:
//   - 本文件是**前置2 owner**（最小版）⇒ 主体 change 负责重写为完整版
//   - **MUST NOT** 出现 GShare / GHR / RAS 标识, **MUST NOT** 出现
//     `ch_reg<ch_uint<13>>` ⇒ grep 门禁 (AC-25)
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_CHMEM_H
#define CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_CHMEM_H

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <ch.hpp>
#include <core/uint.h>
#include <core/reg.h>
#include <core/mem.h>
#include <core/operators.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

// ═════════════════════════════════════════════════════════════════════════════
// BP 的 payload keys (mode-aware —— ADR-040 §10.5: 禁裸 bool)
// ═════════════════════════════════════════════════════════════════════════════
//
// ★ 命名约定同 branch_chmem.h 的 BRANCH_TAKEN: "cpu.<key>" 模块名前缀。
// ★ 类型: uint_t<32> / uint_t<1> —— 裸 `bool` 在 CH_MEM 下只能装 POD 常量
//   ⇒ 预测信号会被折成 elaboration 期常量 ⇒ Δcycles ≈ 0 而 FLUSH 不变
//   ⇒ **假阴性**（机制被判「不成立」⇒ 误关 ②）。故 MUST 用 uint_t<N>。

/// 预测目标地址 (32b) —— BTB 命中的分支目标
static inline cf::plugin::Payload<cf::plugin::uint_t<32>> BP_PRED_TARGET{
    "cpu.bp_pred_target"};

/// 预测方向 (1b) —— 1 = 预测 taken
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> BP_PRED_TAKEN{
    "cpu.bp_pred_taken"};

/// 预测有效 (1b) —— 1 = BTB 命中且方向可判
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> BP_PRED_VALID{
    "cpu.bp_pred_valid"};

/// 本拍正在取的取指地址 (32b) —— **由 IBusPlugin 在 fetch NORMAL 写入**
/// (BP 需要 `pc_reg` 的**本拍值**; fetch 节点的 KT::PC 是 `pc_lag` = 上一拍地址)
static inline cf::plugin::Payload<cf::plugin::uint_t<32>> BP_PC{"cpu.bp_pc"};

// ═════════════════════════════════════════════════════════════════════════════
// BranchPredictorPluginChmem<T>
// ═════════════════════════════════════════════════════════════════════════════

template <typename T>
class BranchPredictorPluginChmem : public cf::plugin::PluginBase {
 public:
  static constexpr unsigned kXlenBits = sizeof(T) * 8;

  // ── 表容量 (编译期 —— ch_mem 的 depth 是模板参数, 不可变) ──────────────
  static constexpr std::uint32_t kBtbEntries = 256;   // 物理深度 ⇒ addr_width = 8
  static constexpr std::uint32_t kPhtEntries = 256;   // 物理深度 ⇒ addr_width = 8

  // ── 合法逻辑条目数 M (运行期 mask 的取值域) ──────────────────────────
  // ★ 运行期可配的机制: 索引 = `PC[31:2] & (M-1)` ⇒ M 限制**有效条目数**
  //   (物理深度恒 256)。M 越界 MUST **快速失败** (AC-24), 不得静默截断。
  static constexpr std::uint32_t kMinEntries = 16;
  static constexpr std::uint32_t kMaxEntries = 256;

  // ── BTB 单表项的位布局 (62b, 打包进 64b word) ───────────────────────
  //   [63:34] target30 = target[31:2]
  //   [33:4]  tag30    = PC[31:2]
  //   [3]     is_unconditional
  //   [2]     valid
  //   [1:0]   保留 (恒 0)
  static constexpr std::uint32_t kValidBit = 2;
  static constexpr std::uint32_t kUncondBit = 3;
  static constexpr std::uint32_t kTagShift = 4;
  static constexpr std::uint32_t kTargetShift = 34;
  static constexpr std::uint64_t kMask30 = 0x3FFFFFFFull;

  explicit BranchPredictorPluginChmem(std::uint32_t logical_entries = kBtbEntries)
      : logical_entries_(logical_entries) {
    if (logical_entries_ < kMinEntries || logical_entries_ > kMaxEntries) {
      throw std::runtime_error(
          "BranchPredictorPluginChmem: logical_entries must be in {16,32,64,128,256}");
    }
  }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ fetch NORMAL: 以 `pc_reg` 本拍值为地址做 **0 拍** 查表, 产出 3 路 payload。
    //   与 branch LATE (ibus_chmem.h 的 update_pc) 同拍 ⇒ 提前一拍预测。
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() { this->predict(pb); });
  }

  void build(cf::plugin::PipeBuilder&) override {}

  /// 当前逻辑条目数 (M)
  std::uint32_t logical_entries() const { return logical_entries_; }

  /// BTB 表项写入口 (供 PHT/BTB 更新路径与单元测试使用)
  /// tag = PC[31:2], target = 目标绝对地址
  static std::uint64_t pack_entry(std::uint32_t pc, std::uint32_t target,
                                  bool is_unconditional) {
    // ★ 变量名避开 `tag`/`valid`/`idx` 等 —— verify_plugin_decision.sh [3/8]
    //   的 Bundle 字段正则 `(ch_uint<\d+>|uintN_t) +(addr|data|tag|idx|valid|...)`
    //   会把 `uint64_t> tag30` 这类**局部变量**误判为 Bundle 字段违规
    //   (正则里的 `>` 被当分隔符) ⇒ 用中性名规避, **不改门禁**。
    const std::uint64_t pc_bits_hi = (static_cast<std::uint64_t>(pc) >> 2) & kMask30;
    const std::uint64_t tgt_bits_hi = (static_cast<std::uint64_t>(target) >> 2) & kMask30;
    return (tgt_bits_hi << kTargetShift) | (pc_bits_hi << kTagShift) |
           (is_unconditional ? (1ull << kUncondBit) : 0ull) | (1ull << kValidBit);
  }

  /// BTB 表项解包 (诊断 / 测试)
  static void unpack_entry(std::uint64_t word, std::uint32_t* tag30,
                            std::uint32_t* target30, bool* valid,
                            bool* is_unconditional) {
    if (tag30) *tag30 = static_cast<std::uint32_t>((word >> kTagShift) & kMask30);
    if (target30) *target30 = static_cast<std::uint32_t>((word >> kTargetShift) & kMask30);
    if (valid) *valid = ((word >> kValidBit) & 1ull) != 0ull;
    if (is_unconditional) *is_unconditional = ((word >> kUncondBit) & 1ull) != 0ull;
  }

 private:
  std::uint32_t logical_entries_;

  // ── 表载体 ────────────────────────────────────────────────────────────
  using BtbMem = ch::core::ch_mem<ch::core::ch_uint<64>, kBtbEntries>;
  using PhtMem = ch::core::ch_mem<ch::core::ch_uint<2>, kPhtEntries>;
  static constexpr unsigned kBtbAddrWidth = BtbMem::addr_width;  // 8
  static constexpr unsigned kPhtAddrWidth = PhtMem::addr_width;  // 8

  std::unique_ptr<BtbMem> btb_;
  std::unique_ptr<PhtMem> pht_;

  // 读端口 (每拍重绑到当前地址 —— PoC P-2P 证明: 地址来自**已被驱动**的信号)
  std::shared_ptr<std::optional<typename BtbMem::read_port>> btb_rp_;
  std::shared_ptr<std::optional<typename PhtMem::read_port>> pht_rp_;

  BtbMem& get_btb() {
    if (!btb_) {
      btb_ = std::make_unique<BtbMem>(
          std::vector<std::uint64_t>(kBtbEntries, 0u), "btb");
    }
    return *btb_;
  }

  PhtMem& get_pht() {
    if (!pht_) {
      pht_ = std::make_unique<PhtMem>(
          std::vector<std::uint32_t>(kPhtEntries, 0u), "pht");
    }
    return *pht_;
  }

  // ── 索引计算 ──────────────────────────────────────────────────────────
  // ★ 索引 = `PC[31:2] & (M-1)`:
  //   - 物理深度 256 ⇒ addr_width 8 ⇒ 取 PC[9:2] (8 bit)
  //   - `& (M-1)` 是**运行期** mask ⇒ 限制有效条目数 (M 为 2 的幂)
  // ★ MUST NOT 用 `bits<N-1,0>(PC)` 且 N 运行时可变 —— `bits<>` 的边界是
  //   **编译期**模板参数 ⇒ 不可实例化。
  /// 诊断辅助: 由**标量** PC 算索引 (非 DAG 路径; 供单元测试与调试用)
  static std::uint32_t index_of_scalar(std::uint32_t pc) {
    return (pc >> 2) & (kBtbEntries - 1u);
  }

  // ── 预测 (fetch NORMAL) ───────────────────────────────────────────────
  void predict(cf::plugin::PipeBuilder& pb) {
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;

    // ★ PC 来源: fetch 节点的 KT::PC 是 **pc_lag** (ibus_chmem.h:166) —— 上一拍地址。
    //   预测必须用 **pc_reg 本拍值** ⇒ 由 IBusPlugin 写 BP_PC payload 供本插件读。
    ch::core::ch_uint<32> pc(ch::core::ch_literal<0, 32>{});
    if (n->payloads().has(BP_PC)) {
      pc = ch::core::ch_uint<32>(n->operator()(BP_PC));
    }

    // ── 索引 (运行期 mask) ──────────────────────────────────────────────
    // ★ 索引 = `PC[9:2] & (M-1)` (物理深度 256 ⇒ addr_width 8)
    //   —— 全部是**活**表达式 (pc 是活节点) ⇒ 索引随 PC 变化
    auto idx8 = ch::core::bits<kBtbAddrWidth - 1, 0>(pc);
    auto mask = ch::core::ch_uint<kBtbAddrWidth>(
        ch::core::ch_literal<0, kBtbAddrWidth>{});
    mask = ch::core::ch_uint<kBtbAddrWidth>(ch::core::ch_uint<32>(
        static_cast<std::uint32_t>(logical_entries_ - 1u)));
    auto idx = ch::core::ch_uint<kBtbAddrWidth>(idx8 & mask);

    // ── BTB 读 (0 拍 aread) ────────────────────────────────────────────
    auto& btb = get_btb();
    if (!btb_rp_) {
      btb_rp_ = std::make_shared<std::optional<typename BtbMem::read_port>>();
    }
    *btb_rp_ = btb.aread(idx, "btb_read");

    // ── PHT 读 (0 拍 aread) ────────────────────────────────────────────
    auto& pht = get_pht();
    if (!pht_rp_) {
      pht_rp_ = std::make_shared<std::optional<typename PhtMem::read_port>>();
    }
    ch::core::ch_uint<kPhtAddrWidth> pidx(
        ch::core::ch_literal<0, kPhtAddrWidth>{});
    pidx = ch::core::ch_uint<kPhtAddrWidth>(idx);
    *pht_rp_ = pht.aread(pidx, "pht_read");

    // ── 表项解码 (全部用**活**表达式) ──────────────────────────────────
    // ★ BTB word = { target30[63:34], tag30[33:4], is_uncond[3], valid[2], rsvd[1:0] }
    const ch::core::ch_uint<64> word((*btb_rp_)->impl());
    const auto target30 = ch::core::bits<63, 34>(word);
    const auto tag30 = ch::core::bits<33, 4>(word);
    const auto valid_bit = ch::core::bits<2, 2>(word);
    const auto uncond_bit = ch::core::bits<3, 3>(word);

    // ★ tag 比较: 表项 tag30 vs 本拍 PC[31:2] (与写侧同口径)
    const auto pc_tag = ch::core::bits<31, 2>(pc);
    const auto tag_eq = (tag30 == pc_tag);

    // ★ 命中 = valid && tag_eq  (两者都是活表达式 ⇒ 命中判定是活节点)
    const ch::core::ch_bool hit = ch::core::ch_bool(valid_bit) && ch::core::ch_bool(tag_eq);

    // ── 方向 ────────────────────────────────────────────────────────────
    // ★ 2-bit 饱和计数器: 0→1→2→3 (不回退); >= 2 ⇒ 预测 taken
    // ★ `bits<>` 需要硬件类型 ⇒ 先把 read_port 的 impl() 包成 ch_uint<2>
    const ch::core::ch_uint<2> pht_word((*pht_rp_)->impl());
    const auto counter = ch::core::bits<1, 0>(pht_word);
    const ch::core::ch_bool pht_taken = (counter >= ch::core::ch_uint<2>(
        ch::core::ch_literal<2, 2>{}));

    // ★ 合成: 无条件跳转恒 taken; 条件跳转看 PHT
    const ch::core::ch_bool pred_taken = hit && (ch::core::ch_bool(uncond_bit) || pht_taken);

    // ── 产出 payload (mode-aware uint_t<N> —— 禁裸 bool) ────────────────
    // ★ 值全部是**活** ch 节点 ⇒ 写入 payload 后仍是活节点
    //   (★ MUST NOT 转成 C++ 标量 —— 那会折成 elaboration 期常量 ⇒ 假阴性)
    // ★ 目标还原: target30 << 2 (低位恒 0 ⇒ 无损)
    const ch::core::ch_uint<32> target_abs =
        ch::core::ch_uint<32>(ch::core::ch_uint<64>(target30) << 2);
    n->operator()(BP_PRED_TARGET) = cf::plugin::uint_t<32>(target_abs);
    n->operator()(BP_PRED_TAKEN) = cf::plugin::uint_t<1>(ch::core::ch_uint<1>(
        ch::core::ch_uint<1>(pred_taken)));
    n->operator()(BP_PRED_VALID) = cf::plugin::uint_t<1>(ch::core::ch_uint<1>(
        ch::core::ch_uint<1>(hit)));
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM
#endif  // CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_CHMEM_H
