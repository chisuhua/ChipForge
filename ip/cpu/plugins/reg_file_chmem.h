// ip/cpu/plugins/reg_file_chmem.h
//
// 功能描述: RegFilePlugin (CH_MEM 模式) — 通用寄存器堆的 elaboration 版本
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17 (Phase 6c M3 Prereq-3)
//
// ── Oracle M3 Prereq-3: 修复 dual static thread_local 架构缺陷 ────────────
// 原问题:
//   decode 闭包和 writeback 闭包各自持有独立 static thread_local
//   std::array<ch_reg<ch_uint<XLEN>>, 32> 对象 → 两块分离存储,
//   写回操作对读操作不可见, 寄存器堆功能完全失效。
//
// 修复:
//   单一 static 单例 get_regs() 替代 dual static thread_local。
//   单个 std::array 在首次调用时延迟初始化, decode 和 writeback 闭包
//   共享同一份寄存器数组, 写操作即时反映到随后的读操作。
//
// 选型理由 (见 M3 Prereq-3 交付物 §5):
//   选用 static 单例 (function-local static) 而非 plugin 实例成员,
//   因为 ch_reg<T> 构造需要活跃的 ch::core::context, 其创建时机
//   晚于 plugin 对象构造 (在 PipeBuilder::elaborate() 内)。
//   单进程仿真场景下 static 单例语义正确; 多核场景 (M4/W7) 再升格
//   为 plugin 实例成员 (std::optional lazy init)。
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

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf {
namespace cpu {
namespace plugins {

// ============================================================================
// RegFilePlugin (CH_MEM): 32 个 ch_reg<ch_uint<XLEN>> + x0 select 屏蔽
//
// M3 Prereq-3: 单一 static get_regs() 单例替代 dual static thread_local
// ============================================================================
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class RegFilePlugin : public PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RegFilePlugin<T>: T must be unsigned");
  static_assert(N_REGS == 32, "CH_MEM PoC: 固定 32 寄存器 (RISC-V spec)");

 public:
  static constexpr std::size_t kNumRegs = N_REGS;
  static constexpr std::size_t kXlenBits = sizeof(T) * 8;
  static constexpr std::size_t kAddrBits = 5;  // log2(32) = 5

  using addr_t = ch_uint<kAddrBits>;

  RegFilePlugin() = default;
  ~RegFilePlugin() override = default;

  RegFilePlugin(const RegFilePlugin&) = delete;
  RegFilePlugin& operator=(const RegFilePlugin&) = delete;

  // setup — 注册 at_stage 回调 (M3 Prereq-3: 从 build 移到 setup)
  void setup(PipeBuilder& pb) override {
    pb.at_stage("decode", Phase::NORMAL,
                [this, &pb] { this->id_decode(pb); });
    pb.at_stage("writeback", Phase::LATE,
                [this, &pb] { this->wb_writeback(pb); });
  }

  // build — 空 (所有闭包在 setup 注册)
  void build(PipeBuilder&) override {}

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
  // ── 单一 regs 单例 ──────────────────────────────────────────────
  // M3 Prereq-3: 替代原 dual static thread_local 数组。
  //   static (非 thread_local) 保证 decode 和 writeback 闭包访问同一数组。
  //   延迟初始化: ch_reg 构造需要活跃的 ch::core::context
  //   (= elaborate() 期间 ctx_curr_ 已设置)。
  static std::array<ch_reg<ch_uint<kXlenBits>>, kNumRegs>& get_regs() {
    static std::array<ch_reg<ch_uint<kXlenBits>>, kNumRegs> regs{};
    return regs;
  }

  // ── ID stage: RS1/RS2 读取 ───────────────────────────────────────
  void id_decode(PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto& regs = get_regs();

    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      const auto& dec = n->operator()(KeyType::DECODE);
      addr_t rs1_addr = static_cast<addr_t>(dec.rs1_idx);
      addr_t rs2_addr = static_cast<addr_t>(dec.rs2_idx);

      if (dec.reads_rs1) {
        ch_uint<kXlenBits> rs1_data(0_d, "rs1_data");
        for (std::size_t i = 1; i < kNumRegs; ++i) {
          rs1_data = select(rs1_addr == addr_t(i), regs[i], rs1_data);
        }
        n->operator()(KeyType::RS1) = rs1_data;
      }

      if (dec.reads_rs2) {
        ch_uint<kXlenBits> rs2_data(0_d, "rs2_data");
        for (std::size_t i = 1; i < kNumRegs; ++i) {
          rs2_data = select(rs2_addr == addr_t(i), regs[i], rs2_data);
        }
        n->operator()(KeyType::RS2) = rs2_data;
      }
    }
  }

  // ── WB stage: RD 写回 (x0 屏蔽) ─────────────────────────────────
  void wb_writeback(PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto& regs = get_regs();

    auto* n = pb.node_of_logic_stage("writeback").get();
    if (n) {
      const auto& dec = n->operator()(KeyType::DECODE);
      addr_t rd_addr = static_cast<addr_t>(dec.rd_idx);
      ch_bool we = dec.writes_rd && (rd_addr != addr_t(0));

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