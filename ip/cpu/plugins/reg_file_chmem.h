// ip/cpu/plugins/reg_file_chmem.h
//
// 功能描述: RegFilePlugin (CH_MEM 模式) — 通用寄存器堆的 elaboration 版本
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17 (Phase 6c M3 Prereq-3 + M3/W8 #95)
//
// ── Oracle M3 Prereq-3: 修复 dual static thread_local 架构缺陷 ────────────
// 原问题: decode 和 writeback 闭包各自持有独立 static thread_local
//   std::array<ch_reg<ch_uint<XLEN>>, 32> 对象 → 两块分离存储,
//   写回操作对读操作不可见, 寄存器堆功能完全失效。
// 修复: 单一 static 单例 get_regs() 替代 dual static thread_local。
//
// ── M3/W8 #95: 去掉 static 单例, 改为 plugin 实例成员 ─────────────
// 原问题: static 单例 (函数-local static) 首次 elaboration 创建 32 个
//   ch_reg 绑定 context-A 的 lnodeimpl*。第二次 elaboration (不同
//   context) 复用同一单例, lnodeimpl* 指向已析构的 context → ASan
//   heap-use-after-free。
// 修复: 去掉 get_regs() static 属性, 改为 Plugin 实例成员 regs_。
//   每个 RegFilePlugin 实例在其自己的 context 中持有 32 个 ch_reg。
//   各实例独立 → 无跨 context 复用 → 无 use-after-free。
//   延迟初始化 (第一次 get_regs() 调用时) 确保 ch_reg 构建在正确的
//   context 中 (此时 ctx_swap 已生效, elaborate() 正在执行)。
//
// 为什么不用 unordered_map<void*, array> / thread_local + cached_ctx:
//   for 循环中栈分配的 context 地址可复用 (同一栈槽), map 返回旧
//    context 的已失效 ch_reg → 地址复用场景下 use-after-free。
//   实例成员从根本上规避了此问题: 每个 Plugin 拥有自己的 regs_,
//   其生命周期与 Plugin 实例绑定, 不依赖 context 指针作为 key。
// ────────────────────────────────────────────────────────────────────────────
//
// 设计:
//   - 仅在 CF_PLUGIN_USE_CH_MEM 下编译
//   - 32 个寄存器用 32 个 ch_reg<ch_uint<XLEN>> 实现
//   - x0 屏蔽: 读循环从 i=1 开始 (rs1_data 初始 0); 写循环从 i=1 开始
//   - setup() 注册 at_stage 回调; build() 空
//
// 约束:
//   - D4 合规: 无 tick(), 用 at_stage() 注册闭包
//   - D4 合规: 闭包内 select 替代 if/else 分支
//   - 与 TLM 版本 (reg_file.h) 互斥编译
// ────────────────────────────────────────────────────────────────────────────

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_CPU_PLUGINS_REG_FILE_CHMEM_H
#define CF_IP_CPU_PLUGINS_REG_FILE_CHMEM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <ch.hpp>
#include <component.h>
#include <core/context.h>
#include <core/reg.h>
#include <core/uint.h>
#include <core/bool.h>
#include <core/operators.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/plugins/decode_chmem.h"
#include "ip/cpu/plugins/ibus_chmem.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

 namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class IBusPlugin;

// ============================================================================
// RegFilePlugin (CH_MEM): 32 个 ch_reg<ch_uint<XLEN>> + x0 select 屏蔽
//
// M3 Prereq-3: 单一 static get_regs() 单例替代 dual static thread_local
// ============================================================================
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class RegFilePlugin : public PluginBase {
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_unsigned<T>::value,
                "RegFilePlugin<T>: T must be unsigned (TLM mode)");
#endif
  static_assert(N_REGS == 32, "CH_MEM PoC: 固定 32 寄存器 (RISC-V spec)");

 public:
  static constexpr std::size_t kNumRegs = N_REGS;
  static constexpr std::size_t kXlenBits =
#ifdef CF_PLUGIN_USE_CH_MEM
      32;  // CH_MEM PoC: 固定 RV32 (T=ch_uint<32> 时 sizeof(T)*8 ≠ 32)
#else
      sizeof(T) * 8;
#endif
  static constexpr std::size_t kAddrBits = 5;  // log2(32) = 5

  using addr_t = ch_uint<kAddrBits>;

  RegFilePlugin() = default;
  ~RegFilePlugin() override = default;

  RegFilePlugin(const RegFilePlugin&) = delete;
  RegFilePlugin& operator=(const RegFilePlugin&) = delete;

  // setup — 注册 writeback 回调 (M3 Prereq-3)
  void setup(PipeBuilder& pb) override {
    pb.at_stage("writeback", Phase::LATE,
                [this, &pb] { this->wb_writeback(pb); });
  }

  // build — 注册 id_decode (decode NORMAL)
  // 注意: id_decode 必须在 Decoder::build 注册的 decode_closure 之后,
  // 因此不能放 setup (setup 在所有 build 之前执行). 工厂侧需将 Decoder
  // 注册在 RegFile 之前 (见 cpu_factory_chmem.h).
  void build(PipeBuilder& pb) override {
    pb.at_stage("decode", Phase::NORMAL,
                [this, &pb] { this->id_decode(pb); });
  }

  // 单元测试辅助 API (PoC: 与 TLM 版本接口一致)
  T read_reg(std::size_t idx, std::uint8_t /*tid*/ = 0) const {
    // CH_MEM 模式下读寄存器需要 Simulator; PoC 简化: 返回 0
    (void)idx;
    return T{0};
  }

  void write_reg(std::size_t idx, T value, std::uint8_t /*tid*/ = 0) {
    // CH_MEM 模式下写寄存器需要 at_stage("writeback", ...) 触发
    (void)idx;
    (void)value;
  }

 private:
  // ── 实例成员 regs_: 替代 static 单例 ─────────────────────────────
  // 每个 RegFilePlugin 实例持有自己的 regs_ (std::unique_ptr<array>),
  // 在第一次 get_regs() 调用时延迟初始化 (lazy init)。
  //
  // 延迟初始化原因:
  //   ch_reg<T> 的构造需要活跃的 ch::core::context (以发射 regimpl 节点)。
  //   Plugin 对象构造时 (register_plugin 内) context 可能未激活;
  //   get_regs() 首次调用发生在 elaborate() 期间 (at_stage 回调),
  //   此时 ctx_swap 已生效, context 处于活跃状态。
  //
  // 注意:
  //   非 static → 每个实例独立的 regs_, 不会跨 context 复用。
  //   unique_ptr 确保 Plugin 析构时释放 ch_reg (不破坏 context 节点,
  //   lnodeimpl 由 context::node_storage_ 拥有)。
  using regs_array_t = std::array<ch_reg<ch_uint<kXlenBits>>, kNumRegs>;
  std::unique_ptr<regs_array_t> regs_;

  regs_array_t& get_regs() {
    if (!regs_) {
      regs_ = std::make_unique<regs_array_t>();
      for (std::size_t i = 0; i < kNumRegs; ++i) {
        (*regs_)[i] = ch_reg<ch_uint<kXlenBits>>(
            ch_uint<kXlenBits>(ch::core::ch_literal<0, 1>{}),
            ("reg_" + std::to_string(i)).c_str());
      }
    }
    return *regs_;
  }

  // ── ID stage: RS1/RS2 读取 ───────────────────────────────────────
  void id_decode(PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto& regs = get_regs();

    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      if (n->payloads().has(DecodePlugin::DECODED_INST)) {
        const auto& decoded = n->operator()(DecodePlugin::DECODED_INST);
        addr_t rs1_addr = decoded.rs1_idx;  // 已 ch_uint<5>
        addr_t rs2_addr = decoded.rs2_idx;

        // Select tree for RS1 (无运行期 if(ch_bool), D4 合规)
        ch_uint<kXlenBits> rs1_data(ch::core::ch_literal<0, 1>{}, "rs1_data");
        for (std::size_t i = 1; i < kNumRegs; ++i) {
          auto sel = (rs1_addr == addr_t(i)) && decoded.reads_rs1;
          rs1_data = select(sel, regs[i], rs1_data);
        }
        n->operator()(KeyType::RS1) = rs1_data;

        // Select tree for RS2
        ch_uint<kXlenBits> rs2_data(ch::core::ch_literal<0, 1>{}, "rs2_data");
        for (std::size_t i = 1; i < kNumRegs; ++i) {
          auto sel = (rs2_addr == addr_t(i)) && decoded.reads_rs2;
          rs2_data = select(sel, regs[i], rs2_data);
        }
        n->operator()(KeyType::RS2) = rs2_data;
      } else {
        // 无 DECODED_INST (单元级 PoC 测试): 输出零值, 防 null handle SEGV
        n->operator()(KeyType::RS1) = T(ch::core::ch_literal<0, 32>{});
        n->operator()(KeyType::RS2) = T(ch::core::ch_literal<0, 32>{});
      }
    }
  }

  // ── WB stage: RD 写回 (x0 屏蔽) ─────────────────────────────────
  void wb_writeback(PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto& regs = get_regs();

    auto* n = pb.node_of_logic_stage("writeback").get();
    if (n) {
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      // Phase 6d.4: branch flush — suppress register writes of the wrong-path
      // fall-through instruction fetched right after a taken branch.
      auto* fch = pb.node_of_logic_stage("fetch").get();
      auto flush = fch ? fch->operator()(cf::cpu::plugins::IBusPlugin<T>::FLUSH)
                       : ch_bool(false);
      // 缺省 we: DECODED_INST 缺失时全禁用写 (单元级 PoC 无译码信号)
      ch_bool we = ch_bool(false);
      addr_t rd_addr = addr_t(0);
      if (n->payloads().has(DecodePlugin::DECODED_INST) &&
          n->payloads().has(KeyType::RD_DATA)) {
        const auto& decoded = n->operator()(DecodePlugin::DECODED_INST);
        rd_addr = decoded.rd_idx;  // 已 ch_uint<5>
        // x0 屏蔽: writes_rd (ch_bool) && rd != 0; flush 抑制误取写回
        we = decoded.writes_rd && (rd_addr != addr_t(0)) && !flush;
      }

      // 32 路条件写: reg[i]->next = select(we && rd_addr == i, rd_data, reg[i])
      for (std::size_t i = 1; i < kNumRegs; ++i) {
        ch_bool sel = we && (rd_addr == addr_t(i));
        regs[i] <<= select(sel, n->operator()(KeyType::RD_DATA),
                          regs[i]);
      }
      // x0 (regs[0]) 永远不写, 始终保持 0
    }
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_REG_FILE_CHMEM_H

#endif  // CF_PLUGIN_USE_CH_MEM