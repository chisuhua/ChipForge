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
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/plugins/branch_chmem.h"

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

// ★★★ payload key 必须是**跨 TU 唯一**的单例 ⇒ 用 **`inline` 变量**
//   （C++17 起 inline 变量有唯一实例），**不可**用 `static inline`
//   （`static inline` 在头文件里 ⇒ **每个 TU 各有一份实例** ⇒
//     `PayloadStore` 按 `&key` 索引 ⇒ 不同 TU 的同名 key 是**不同 cell**）。
//   ★ 实测症状: BP 内部读回自己的 cell 非空，但**测试 TU** 读同一 key 得空
//     （测试 TU 包含本头文件 ⇒ 它持有**另一个** `BP_PRED_VALID` 实例）。
//   ⇒ 真实流水线里 IBus 与 BP 同属一个 TU（`cpu_factory_chmem.h` 包含两者）
//      ⇒ 侥幸能通；但任何**跨 TU** 的读写（测试、工具）都会断链。
//   ⇒ 修法: 全部 key 改 `inline`（C++17 inline 变量 ⇒ 跨 TU 唯一）。
//
// ★ 参照: `branch_chmem.h` 的 `BRANCH_TAKEN` 等也是 `static inline` ⇒ 同类隐患
//   （目前读写方恰好同 TU ⇒ 未暴露）。★ 本 change 至少对自己的 key 修对。

/// 预测目标地址 (32b) —— BTB 命中的分支目标
inline cf::plugin::Payload<cf::plugin::uint_t<32>> BP_PRED_TARGET{
    "cpu.bp_pred_target"};

/// 预测方向 (1b) —— 1 = 预测 taken
inline cf::plugin::Payload<cf::plugin::uint_t<1>> BP_PRED_TAKEN{
    "cpu.bp_pred_taken"};

/// 预测有效 (1b) —— 1 = BTB 命中且方向可判
inline cf::plugin::Payload<cf::plugin::uint_t<1>> BP_PRED_VALID{
    "cpu.bp_pred_valid"};

/// 本拍正在取的取指地址 (32b) —— **由 IBusPlugin 在 fetch NORMAL 写入**
/// (BP 需要 `pc_reg` 的**本拍值**; fetch 节点的 KT::PC 是 `pc_lag` = 上一拍地址)
inline cf::plugin::Payload<cf::plugin::uint_t<32>> BP_PC{"cpu.bp_pc"};

// ═════════════════════════════════════════════════════════════════════════════
// BranchPredictorPluginChmem<T>
// ═════════════════════════════════════════════════════════════════════════════

template <typename T>
class BranchPredictorPluginChmem : public cf::plugin::PluginBase {
 public:
  // ★ CH_MEM 下 T 是 ch_uint<32>（不是 uint32_t）⇒ `sizeof(T)*8` **不是** 32
  //   ⇒ 显式取 32（payload_common.h 的 keys<T,32> 要求 XLEN==32）
  static constexpr unsigned kXlenBits = 32;

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
    // ★ SF1 修正: 合法域是**精确集合** {16,32,64,128,256} —— 不是区间。
    //   原检查只挡 `<16` / `>256` ⇒ `logical_entries=17` 静默通过 ⇒
    //   索引 mask = `&17` ⇒ index ∈ {0,16} ⇒ **静默半坏**。
    //   ★ MUST NOT 静默 clamp（AC-24）⇒ 越界 MUST **快速失败**。
    if (logical_entries_ != 16 && logical_entries_ != 32 &&
        logical_entries_ != 64 && logical_entries_ != 128 &&
        logical_entries_ != 256) {
      throw std::runtime_error(
          "BranchPredictorPluginChmem: logical_entries must be one of "
          "{16,32,64,128,256} (got " + std::to_string(logical_entries_) + ")");
    }
  }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ fetch NORMAL: 以 `pc_reg` 本拍值为地址做 **0 拍** 查表, 产出 3 路 payload。
    //   与 branch LATE (ibus_chmem.h 的 update_pc) 同拍 ⇒ 提前一拍预测。
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() { this->predict(pb); });
    // ★ branch NORMAL: 分支已解析 ⇒ 更新 BTB / PHT (B1 写路径)
    //   ★ 顺序: branch NORMAL **晚于** fetch NORMAL (canonical_stage_order:
    //     stage 首现序 fetch ≺ branch) ⇒ 本拍的预测已写入 payload, 可被比较。
    pb.at_stage("branch", cf::plugin::Phase::NORMAL, [this, &pb]() { this->update(pb); });
  }

  void build(cf::plugin::PipeBuilder&) override {}

  /// 当前逻辑条目数 (M)
  std::uint32_t logical_entries() const { return logical_entries_; }

  /// 诊断辅助: 由**标量** PC 算索引 (非 DAG 路径; 供单元测试与调试用)
  /// ★ 索引 = **PC[9:2]**（8 bit，对齐 ch_mem<_,256> 的 addr_width=8）
  static std::uint32_t index_of_scalar(std::uint32_t pc) {
    return (pc >> 2) & (kBtbEntries - 1u);
  }

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

  // ── 预测记录 (ch_reg 组, 跨拍保持) ──────────────────────────────────
  // ★ Task 1 交付物「ch_reg 存预测记录」: 记录**本拍**的预测, 供**下一拍**
  //   与实际分支结果比对 (判「预测是否正确」⇒ 决定是否 flush / 是否纠错)。
  // ★ 跨拍保持已由 PoC P-1 验证 (ch_reg 1→3→7)。
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<1>>>  pred_taken_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pred_target_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<1>>>  pred_valid_reg_;

  // ── host 可见的事件计数 (AC-F2-13/14 需要) ──────────────────────────
  // ★ BP 的输出是 **payload**（不出现在 Verilog 顶层）⇒ 计数必须用 ch_reg
  //   （CppHDL codegen 出顶层信号 ⇒ host 经 --public-flat-rw 读）
  // ★ 与 FLUSH 计数（ibus_chmem.h 内）同一机制。
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> hit_count_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> miss_count_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> taken_count_;

  // 读端口 (每拍重绑到当前地址 —— PoC P-2P 证明: 地址来自**已被驱动**的信号)
  std::shared_ptr<std::optional<typename BtbMem::read_port>> btb_rp_;
  std::shared_ptr<std::optional<typename PhtMem::read_port>> pht_rp_;
  // ★ 写端口 (B1: 无写路径 ⇒ valid≡0 ⇒ hit≡false ⇒ BP 永远惰性)
  std::shared_ptr<std::optional<typename BtbMem::write_port>> btb_wp_;
  std::shared_ptr<std::optional<typename PhtMem::write_port>> pht_wp_;

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_hit_count() {
    if (!hit_count_)
      hit_count_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "bp_hit_count");
    return *hit_count_;
  }
  ch::core::ch_reg<ch::core::ch_uint<32>>& get_miss_count() {
    if (!miss_count_)
      miss_count_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "bp_miss_count");
    return *miss_count_;
  }
  ch::core::ch_reg<ch::core::ch_uint<32>>& get_taken_count() {
    if (!taken_count_)
      taken_count_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "bp_taken_count");
    return *taken_count_;
  }

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
  // ── 更新 (branch NORMAL) ──────────────────────────────────────────────
  // ★ B1 写路径:
  //   BTB: **taken 时**写 (PC → target) —— 目标此时才已知
  //   PHT: **「分支已解析」（任何方向）**时更新 2-bit 饱和计数
  //        ★ MUST NOT 仅在 taken 时更新 —— 否则学不到 not-taken
  //          ⇒ 在以 not-taken 为主的负载上恒预测 taken ⇒ **净负收益**
  //   ★ 更新地址 = **分支解析处的 PC**（= 预测时用的 PC）⇒ 二者必须**同指一条分支**
  //     （off-by-one ⇒ 学到错误表项）
  void update(cf::plugin::PipeBuilder& pb) {
    auto* brn = pb.node_of_logic_stage("branch").get();
    if (!brn) return;

    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<T>;
    using BrPlugin = cf::cpu::plugins::BranchPlugin<T>;

    // 分支所在指令的 PC（branch 节点的 KT::PC = decode 的 PC = pc_lag）
    ch::core::ch_uint<32> br_pc(ch::core::ch_literal<0, 32>{});
    if (brn->payloads().has(KT::PC)) {
      br_pc = ch::core::ch_uint<32>(brn->operator()(KT::PC));
    }
    // ★ 全部用 `if/else` 而非三目 —— 三目的条件位对 ch_bool 做 contextual
    //   conversion ⇒ 运行期语义丢失（ADR-040 §10.5 那一族）。★ 这里条件是
    //   `has()`（elaboration 期的 map 查询）⇒ 三目本可行，但为让
    //   `check_plugin_portability.sh` Check 5b 能**零误报**地检查全仓 ⇒ 统一用 if。
    ch::core::ch_bool taken(ch::core::ch_literal<0, 1>{});
    ch::core::ch_bool taken_raw(ch::core::ch_literal<0, 1>{});
    ch::core::ch_bool flushed(ch::core::ch_literal<0, 1>{});
    if (brn->payloads().has(BrPlugin::get_branch_taken_key())) {
      taken = ch::core::ch_bool(brn->operator()(BrPlugin::get_branch_taken_key()));
    }
    // ★ **未抑制**的方向（供 PHT 方向学习）+ 错路标记（供表更新门控）
    if (brn->payloads().has(BrPlugin::BRANCH_TAKEN_RAW)) {
      taken_raw = ch::core::ch_bool(brn->operator()(BrPlugin::BRANCH_TAKEN_RAW));
    }
    if (brn->payloads().has(BrPlugin::BRANCH_FLUSHED)) {
      flushed = ch::core::ch_bool(brn->operator()(BrPlugin::BRANCH_FLUSHED));
    }
    ch::core::ch_bool is_branch(ch::core::ch_literal<0, 1>{});
    if (brn->payloads().has(BrPlugin::IS_BRANCH)) {
      is_branch = ch::core::ch_bool(brn->operator()(BrPlugin::IS_BRANCH));
    }
    ch::core::ch_uint<32> br_target(ch::core::ch_literal<0, 32>{});
    if (brn->payloads().has(RvKey::BRANCH_TARGET)) {
      br_target = ch::core::ch_uint<32>(brn->operator()(RvKey::BRANCH_TARGET));
    }

    // ── 索引（与读路径**同一表达式** —— PoC 结论 3）──
    auto idx8 = ch::core::bits<kBtbAddrWidth + 1, 2>(br_pc);
    auto mask = ch::core::ch_uint<kBtbAddrWidth>(
        ch::core::ch_literal<0, kBtbAddrWidth>{});
    mask = ch::core::ch_uint<kBtbAddrWidth>(ch::core::ch_uint<32>(
        static_cast<std::uint32_t>(logical_entries_ - 1u)));
    auto idx = ch::core::ch_uint<kBtbAddrWidth>(idx8 & mask);

    // ── BTB 写（taken 时）──
    // ★ is_unconditional: 无条件跳转 (jal) 恒 taken ⇒ 不占 PHT 表项
    //   ⇒ 方向来源为「无条件」⇒ 需在 BTB 表项里标记 (否则 jal 永不被预测)
    // ★ 间接跳转 (jalr) 目标随运行时变化 ⇒ **不建 BTB 项** (静态缓存必错)
    {
      auto& btb = get_btb();
      if (!btb_wp_) {
        btb_wp_ = std::make_shared<std::optional<typename BtbMem::write_port>>();
      }
      // ★ 「无条件跳转」= taken 但**不是**条件分支 (is_branch=false) ⇒ jal/jalr
      //   ★ 但 jalr 不建项 ⇒ 需区分 jal 与 jalr —— 用 opcode 不可得（branch 节点
      //   未导出）⇒ 保守：**只有 is_branch（条件分支）建项**；jal 的方向问题
      //   留给主体 change（它需要 jal/jalr 的 opcode 区分）
      // ★ B7: **错路指令 MUST NOT 写 BTB**（否则把错路 target 写进表项）
      const ch::core::ch_bool write_en = taken & is_branch & (!flushed);
      ch::core::ch_uint<64> wdata(ch::core::ch_literal<0, 64>{});
      // ★ 打包必须是**活**表达式（tag 来自 br_pc，target 来自 br_target）
      // ★ 打包必须是**活**表达式 (tag 来自 br_pc, target 来自 br_target)
      //   ⇒ 全部用 ch 表达式拼装, **MUST NOT** 转 C++ 标量 (会折成常量)
      //
      // ★ 移位量的写法: `operator<<` 的 RHS 传 **ch_uint**（非 ch_literal）时
      //   ResultWidth = LHS 宽 + RHS 额外宽 ⇒ 结果宽度会变 ⇒ 需显式收窄。
      //   ⇒ 统一在 **64 bit** 域内运算: 先把 30 bit 字段零扩展到 64 bit,
      //     再用 ch_uint<64> 左移（结果仍是 64 bit 域）。
      const ch::core::ch_uint<64> pc_hi =
          ch::core::ch_uint<64>(ch::core::bits<31, 2>(br_pc));
      const ch::core::ch_uint<64> tgt_hi =
          ch::core::ch_uint<64>(ch::core::bits<31, 2>(br_target));
      // ★ 用 `shl<64>(lhs, rhs)`（**显式结果宽度**）而不是 `operator<<`:
      //   后者的 ResultWidth = LHS 宽 + RHS 额外宽 ⇒ 64 bit 左移 64 bit 会溢出
      //   ⇒ 编译失败（上游 operators_shift.h:58）。
      const ch::core::ch_uint<64> pc_shifted = ch::core::shl<64>(
          pc_hi, ch::core::ch_uint<64>(ch::core::ch_literal<kTagShift, 64>{}));
      const ch::core::ch_uint<64> tgt_shifted = ch::core::shl<64>(
          tgt_hi, ch::core::ch_uint<64>(ch::core::ch_literal<kTargetShift, 64>{}));
      const ch::core::ch_uint<64> valid_at = ch::core::shl<64>(
          ch::core::ch_uint<64>(ch::core::ch_literal<1, 64>{}),
          ch::core::ch_uint<64>(ch::core::ch_literal<kValidBit, 64>{}));
      ch::core::ch_uint<64> packed_w(tgt_shifted | pc_shifted | valid_at);
      wdata = packed_w;
      *btb_wp_ = btb.write(idx, wdata, write_en, "btb_write");
    }

    // ── PHT 更新（分支已解析，任何方向）──
    {
      auto& pht = get_pht();
      if (!pht_wp_) {
        pht_wp_ = std::make_shared<std::optional<typename PhtMem::write_port>>();
      }
      // ★ 读当前计数（读端口与写端口**同一地址**）⇒ 饱和 +1
      if (!pht_rp_) {
        pht_rp_ = std::make_shared<std::optional<typename PhtMem::read_port>>();
      }
      ch::core::ch_uint<kPhtAddrWidth> pidx(
          ch::core::ch_literal<0, kPhtAddrWidth>{});
      pidx = ch::core::ch_uint<kPhtAddrWidth>(idx);
      *pht_rp_ = pht.aread(pidx, "pht_read");

      const ch::core::ch_uint<2> cur((*pht_rp_)->impl());
      const auto cur2 = ch::core::bits<1, 0>(cur);
      const auto one = ch::core::ch_uint<2>(ch::core::ch_literal<1, 2>{});
      const auto sat3 = ch::core::ch_uint<2>(ch::core::ch_literal<3, 2>{});
      const auto zero2 = ch::core::ch_uint<2>(ch::core::ch_literal<0, 2>{});

      // ★★ B1 修正: 饱和判定**MUST 用 `select()`** —— 不可用 C++ 三目 `?:`。
      //   `ch_bool` 只有 `explicit operator bool()` ⇒ 三目条件位在 **elaboration 期**
      //   被求值成 C++ false ⇒ **饱和分支成死代码** ⇒ 2-bit 计数器 3→0 **回绕**
      //   （实测: Verilog 里 `ge_ge_2` dangling，写入值 = `(cur+1)&3`；
      //     行为指纹 = `pred_taken ≈ hit/2` —— 正是 0→1→2→3→0 循环）。
      //   ⇒ 这是 ADR-040 §10.5 那一族（运行期 ch_bool 被静默折成常量），只是藏在 `?:`。
      const auto inc_val = select(cur2 >= sat3, sat3,
                                  ch::core::ch_uint<2>(cur2 + one));
      // ★ 饱和 -1: cur>0 ? cur-1 : 0
      const auto dec_val = select(cur2 == zero2, zero2,
                                  ch::core::ch_uint<2>(cur2 - one));
      // ★★ B2 修正: **方向学习** —— taken ⇒ 饱和 +1；not-taken ⇒ 饱和 -1。
      //   原实现「任何方向都 +1」⇒ 计数器单调上升 ⇒ 恒 3 ⇒ **PHT 零方向信息**
      //   ⇒ 退化成 brief 明文判定为「净负收益」的 BTB-only。
      //   ★ 与 TLM 侧语义一致（branch_predictor.h:274-286: taken⇒饱和+1 / not⇒饱和-1）。
      const ch::core::ch_uint<2> newval = select(taken_raw, inc_val, dec_val);

      // ★ B7: **错路指令 MUST NOT 污染 PHT**（否则学到错路方向 ⇒ 净负收益）
      //   ★ 写使能 = 「是条件分支」且「不是错路」⇒ 真实分支的**任何方向**都学
      const ch::core::ch_bool write_en = is_branch & (!flushed);
      *pht_wp_ = pht.write(pidx, newval, write_en, "pht_write");
    }
  }

  // ── 预测 (fetch NORMAL) ───────────────────────────────────────────────
  void predict(cf::plugin::PipeBuilder& pb) {
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;

    // ★ PC 来源: fetch 节点的 KT::PC 是 **pc_lag** (ibus_chmem.h:166) —— 上一拍地址。
    //   预测必须用 **pc_reg 本拍值** ⇒ 由 IBusPlugin 写 BP_PC payload 供本插件读。
    // ★ SF5: `BP_PC` 缺失 ⇒ **fail-fast**（不可静默降级为 PC=0 ——
    //   那会让查表地址恒 0 ⇒ 命中/未命中全错 ⇒ 又一个「全绿但语义为零」的通道）。
    if (!n->payloads().has(BP_PC)) {
      throw std::runtime_error(
          "BranchPredictorPluginChmem: fetch node lacks BP_PC payload "
          "(the IBus plugin must write it in fetch NORMAL; the BP plugin MUST be "
          "registered AFTER the IBus plugin)");
    }
    ch::core::ch_uint<32> pc(ch::core::ch_uint<32>(n->operator()(BP_PC)));
    // ★ 查表地址 = **pc_reg 本拍值**（BP_PC payload，由 IBus 在 fetch NORMAL 写）
    //   ⇒ 预测的是「本拍正在取的那条指令」⇒ 若该指令是分支且表项存在（上次
    //     执行时写的）⇒ 命中 ⇒ 预测**领先** actual 一拍。

    // ── 索引 (运行期 mask) ──────────────────────────────────────────────
    // ★ 索引 = `PC[9:2] & (M-1)` (物理深度 256 ⇒ addr_width 8)
    //   —— 全部是**活**表达式 (pc 是活节点) ⇒ 索引随 PC 变化
    // ★ B2 修正: 索引取 **PC[9:2]**（8 bit，对齐 addr_width=8）
    //   ★ MUST NOT 用 `bits<7,0>(pc)` —— 那是 PC[7:0]，而 PC[1:0] 恒 0
    //     ⇒ 只有 64 个 index 可达（4× 容量损失；M=16 时仅 4 槽可达）
    auto idx8 = ch::core::bits<kBtbAddrWidth + 1, 2>(pc);
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
    const ch::core::ch_uint<32> target_abs = ch::core::ch_uint<32>(
        ch::core::shl<64>(ch::core::ch_uint<64>(target30),
                          ch::core::ch_uint<64>(ch::core::ch_literal<2, 64>{})));
    n->operator()(BP_PRED_TARGET) = cf::plugin::uint_t<32>(target_abs);
    n->operator()(BP_PRED_TAKEN) = cf::plugin::uint_t<1>(ch::core::ch_uint<1>(
        ch::core::ch_uint<1>(pred_taken)));
    n->operator()(BP_PRED_VALID) = cf::plugin::uint_t<1>(ch::core::ch_uint<1>(
        ch::core::ch_uint<1>(hit)));

    // ── 事件计数 (host 可见) ──────────────────────────────────────────
    // ★ 命中 / 未命中 / 预测为 taken 的次数 ⇒ 供 AC-F2-13（计数器自证）与
    //   AC-F2-14（BTB 命中率自证: 命中次数 MUST > 0）使用。
    {
      const ch::core::ch_uint<32> one(ch::core::ch_literal<1, 32>{});
      const ch::core::ch_uint<32> zero(ch::core::ch_literal<0, 32>{});
      const ch::core::ch_uint<32> hit_inc =
          select(hit, one, zero);
      const ch::core::ch_uint<32> miss_inc =
          select(hit, zero, one);
      const ch::core::ch_uint<32> taken_inc =
          select(pred_taken, one, zero);
      auto& hc = get_hit_count();
      auto& mc = get_miss_count();
      auto& tc = get_taken_count();
      ch::core::ch_uint<32> hv(hc);
      ch::core::ch_uint<32> mv(mc);
      ch::core::ch_uint<32> tv(tc);
      hc <<= hv + hit_inc;
      mc <<= mv + miss_inc;
      tc <<= tv + taken_inc;
    }
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM
#endif  // CF_IP_CPU_PLUGINS_BRANCH_PREDICTOR_CHMEM_H
