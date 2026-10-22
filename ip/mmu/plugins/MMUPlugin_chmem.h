// ip/mmu/plugins/MMUPlugin_chmem.h
//
// 功能描述: MMUPluginChmem — MMU 的 CH_MEM (elaboration) 后端, **TLB 命中路径**
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-22
//
// ============================================================================
// 范围 (Phase B / B.3 的收窄增量, 2026-10-22 用户裁决路线 C)
// ============================================================================
// 本文件 **只** 交付 TLB 翻译路径:
//   - 8 项全相联 ch_reg 实现的 TLB (lookup → PADDR / PADDR_VALID / MMU_VADDR)
//   - satp 的 elaboration 期 ch_reg 初值 + Bare 隔离 (satp.MODE=0 ⇒ 永不翻译;
//     门控读的是 satp 寄存器, 不是编译期常量)
//   - 两个 lookup 端口 (ifetch / loadstore) 共享同一 TLB
//   - refill 端口 (ch_reg 句柄): Phase B 由测试直接驱动装载
//
// **不含** (显式 defer, 见 mmu-chmem-phase-b-c-launch/tasks.md §Phase B 侦察结论):
//   - PTW 页表 walk (TLB miss → 逐级读 PTE → refill)。原计划的「复用
//     PtWalkFsmPlugin」被推迟: 该插件输入在 create_fsm() 内才创建且立即拉边捕获
//     (mmu_ptw_chmem.h:226-243), 无法被表达式驱动, 复用前需先抽取自由函数
//     build_pt_walk_fsm()。该重构不得脱离其消费者单独落地 ⇒ 与 walk 一并
//     移入后续 change。
//   - TLB miss 的 fault 语义。miss ⇒ PADDR_VALID=0 (消费端回退 vaddr, 与 Bare
//     路径同构), **不**置 EXCEPTION_CODE ⇒ 不会误触发 trap。
//   - RiscvMMUPluginChmem 适配器 (csr_write_satp / sfence_vma / mmu_exit 闭包)
//     与 cpu_factory_chmem 注册 (注册归 C.4)。
//
// ============================================================================
// 为什么 TLB 用「全相联」而非计划写的「8 项直接映射」
// ============================================================================
// 1. 真实 TLB 就是全相联/set-associative (直接映射只出现在 L1 cache)。计划里的
//    「直接映射」是缩略写法, 不是时序需求。
// 2. 组合 DAG 里全相联只需 8 路 tag 比较 + OR 归约 + select 链; 直接映射需要
//    用 ch 信号做 index 的 8 选 1 mux 树 (逐项 == 比较再 select), 代码更绕而
//    能力更弱。
// 3. entry 数 8 不变 ⇒ Phase D 的 cycle/面积估算口径不变。
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0)
// ============================================================================
// - payload key 必须是 mode-aware (mmu_keys<uint64_t> 的 5 个输出 key 已于
//   chmem-mmu-keys-modality 模式化) ⇒ 这里写入的是 **ch 节点**, 不是常量。
// - 顺序契约 (§10.6): 生产者必须与消费者挂**同一 stage 名** + `Phase::EARLY`,
//   消费者 (ibus_chmem / dmem_chmem) 在 `Phase::NORMAL`。**禁**注册新 stage 名
//   (`canonical_stage_order()` 只看 stage 名首现序; factory 已在 build() 之前把
//   "fetch" 钉在最前 ⇒ 新 stage 名必排在消费者之后 ⇒ 永久静默 Bare)。需要
//   node 时用 `declare_substage` (只建 node, 不进 stages_)。
// - at_stage 回调内无早返, 无运行期 if(ch_bool) (guards 全部在成员函数内)。

#ifdef CF_PLUGIN_USE_CH_MEM

#ifndef CF_IP_MMU_PLUGINS_MMU_PLUGIN_CHMEM_H
#define CF_IP_MMU_PLUGINS_MMU_PLUGIN_CHMEM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <ch.hpp>
#include <core/bool.h>
#include <core/operators.h>
#include <core/reg.h>
#include <core/uint.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/mmu/lib/ptw.h"        // SvMode (纯 C++, 无 ch 依赖)
#include "ip/mmu/tlm/mmu_keys.h"   // 5 个已模式化的输出 key

namespace cf {
namespace ip {
namespace mmu {

namespace chc = ch::core;

class MMUPluginChmem : public cf::plugin::PluginBase {
 public:
  // sv32: 32-bit vaddr/paddr, 12-bit 页内偏移 ⇒ tag = vaddr[31:12]
  static constexpr unsigned kXlenBits = 32;
  static constexpr unsigned kPageOffBits = 12;
  static constexpr std::size_t kEntries = 8;

  // 输出 key 的实例必须与消费端 (ibus_chmem.h / dmem_chmem.h) **同一静态实例**
  // ⇒ 模板参数保持 raw std::uint64_t (PayloadStore 按 &key 指针身份索引)。
  using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

  explicit MMUPluginChmem(SvMode mode, std::uint64_t satp_value = 0)
      : satp_init_(encode_satp(mode, satp_value)) {}
  ~MMUPluginChmem() override = default;

  MMUPluginChmem(const MMUPluginChmem&) = delete;
  MMUPluginChmem& operator=(const MMUPluginChmem&) = delete;

  // --------------------------------------------------------------------------
  // setup — 只 declare_substage (建 node, 不进 stages_ ⇒ 不扰动 canonical 序)
  // --------------------------------------------------------------------------
  void setup(cf::plugin::PipeBuilder& pb) override {
    pb.declare_substage("fetch", "tlb_lookup_ifetch", 1);
    pb.declare_substage("memory", "tlb_lookup_loadstore", 1);
  }

  // --------------------------------------------------------------------------
  // build — 生产者挂与消费端同一 stage 名 + EARLY
  //   ibus_chmem.h 消费 ("fetch", NORMAL)   → 本插件 ("fetch", EARLY)
  //   dmem_chmem.h 消费 ("memory", NORMAL)  → 本插件 ("memory", EARLY)
  // 单行 lambda: at_stage 区域内无 return (Check 1); guards 在成员函数内。
  // --------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("fetch", cf::plugin::Phase::EARLY, [this, &pb]() { this->emit_ifetch(pb); });
    pb.at_stage("memory", cf::plugin::Phase::EARLY, [this, &pb]() { this->emit_loadstore(pb); });
  }

  // --------------------------------------------------------------------------
  // 输入端绑定 (ADR-040 v3.0 §10.6「输入侧走插件句柄」)
  //
  // ifetch  : 绑 IBusPluginChmem 的 pc_reg (ch_reg<ch_uint<32>>)。**不要**改用
  //           payload 的 keys<T>::PC —— factory 在 ("fetch", EARLY) 先把它写 0
  //           (cpu_factory_chmem.h:168), ibus 的 ("fetch", NORMAL) 才写真值
  //           ⇒ 本插件在 EARLY 读到的是 0。
  // loadstore: 同理, 由集成方 (C.4) 绑 DMEM 的字节地址信号。
  // 未绑定的端口保持惰性 (PADDR_VALID=0) ⇒ 消费端回退 vaddr, 与 Bare 同构。
  // --------------------------------------------------------------------------
  void bind_ifetch_vaddr(chc::ch_uint<kXlenBits>* vaddr) { ifetch_vaddr_ = vaddr; }
  void bind_loadstore_vaddr(chc::ch_uint<kXlenBits>* vaddr) { loadstore_vaddr_ = vaddr; }

  // --------------------------------------------------------------------------
  // satp 初值编码 (sv32: bit31 = MODE, [21:0] = PPN)
  //
  // 调用方显式给 satp_value 时以它为准; 给 0 时按 mode 编出 MODE 位
  // (与 TLM 侧 RiscvMMUPlugin ctor 的行为对齐 —— 否则 `mode=Sv32` 会被
  //  `satp=0` 抵消成 Bare)。
  // ⚠ 只支持 Sv32: RV32 的 satp 就是 32 位 (MODE=bit31, PPN=[21:0])。
  //    Sv39/Sv48 的 satp 是 64 位 (MODE=[63:60]), 在 RV32 上不存在 ⇒ 本函数
  //    对它们返回 0 (Bare), 即"不翻译"。这不是降级容错, 是 RV32 上 Sv39/48
  //    本就无定义。
  // --------------------------------------------------------------------------
  static std::uint32_t encode_satp(SvMode mode, std::uint64_t satp_value) {
    if (satp_value != 0) {
      return static_cast<std::uint32_t>(satp_value);
    }
    return (mode == SvMode::Sv32) ? 0x80000000u : 0u;
  }

  // --------------------------------------------------------------------------
  // TLB refill 端口 (elaborate 之后、创建 Simulator 之前驱动)
  //
  // Phase B: 测试直接装载 (TLB-only 增量, 无 PTW)。
  // 后续 change: PTW walk 的 DONE 结果写入同一组 ch_reg。
  // --------------------------------------------------------------------------
  chc::ch_reg<chc::ch_uint<1>>& entry_valid(std::size_t i) { return *entry_valid_[i]; }
  chc::ch_reg<chc::ch_uint<kXlenBits>>& entry_vaddr(std::size_t i) { return *entry_vaddr_[i]; }
  chc::ch_reg<chc::ch_uint<kXlenBits>>& entry_paddr(std::size_t i) { return *entry_paddr_[i]; }

  // satp (sv32 编码: bit31 = MODE, [21:0] = PPN)。
  // **Bare 隔离读的是本寄存器的 MODE 位** (运行期信号, 见 emit_lookup) ⇒ 后续
  // `csr_write_satp` 接线时只要驱动本寄存器即可改模式。初始值由 ctor 的
  // encode_satp(mode, satp_value) 给出。
  chc::ch_reg<chc::ch_uint<kXlenBits>>& satp_reg() { return *satp_; }

 private:
  // --------------------------------------------------------------------------
  // create_tlb — elaborate 期一次性创建 8 项 ch_reg (ctx 活跃时)
  // --------------------------------------------------------------------------
  void create_tlb() {
    if (tlb_created_) {
      return;
    }
    for (std::size_t i = 0; i < kEntries; ++i) {
      const std::string idx = std::to_string(i);
      entry_valid_[i] = std::make_unique<chc::ch_reg<chc::ch_uint<1>>>(
          chc::ch_uint<1>(chc::ch_literal<0, 1>{}), "mmu_tlb" + idx + "_valid");
      entry_vaddr_[i] = std::make_unique<chc::ch_reg<chc::ch_uint<kXlenBits>>>(
          chc::ch_uint<kXlenBits>(chc::ch_literal<0, kXlenBits>{}), "mmu_tlb" + idx + "_vaddr");
      entry_paddr_[i] = std::make_unique<chc::ch_reg<chc::ch_uint<kXlenBits>>>(
          chc::ch_uint<kXlenBits>(chc::ch_literal<0, kXlenBits>{}), "mmu_tlb" + idx + "_paddr");
    }
    // satp 初值走 **两条** 通道, 值相同, 覆盖两种宿主用法:
    //   ① ctor init —— 宿主不调 `Simulator::reset()` 时生效
    //      (先例: tests/cpu/test_cpu_chmem_vendored_elf.cpp:156)
    //   ② next 值 (`<<=`) —— 宿主调 `reset()` 时生效。CppHDL 的 `reset()` 把
    //      每个 ch_reg 打成 rst_val (constructor 未给 ⇒ nullptr ⇒ 0), 会**清掉**
    //      ctor init ⇒ 忘了 ② 就是「satp 恒 0 ⇒ 恒 Bare」的静默失效。
    satp_ = std::make_unique<chc::ch_reg<chc::ch_uint<kXlenBits>>>(
        chc::ch_uint<kXlenBits>(chc::ch_literal<0, kXlenBits>{}), "mmu_satp");
    *satp_ <<= chc::ch_uint<kXlenBits>(static_cast<std::uint32_t>(satp_init_));
    tlb_created_ = true;
  }

  void emit_ifetch(cf::plugin::PipeBuilder& pb) {
    create_tlb();
    if (ifetch_vaddr_ != nullptr) {
      emit_lookup(pb, "tlb_lookup_ifetch", chc::ch_uint<kXlenBits>(*ifetch_vaddr_));
    } else {
      emit_lookup(pb, "tlb_lookup_ifetch", null_vaddr());
    }
  }

  void emit_loadstore(cf::plugin::PipeBuilder& pb) {
    create_tlb();
    if (loadstore_vaddr_ != nullptr) {
      emit_lookup(pb, "tlb_lookup_loadstore", chc::ch_uint<kXlenBits>(*loadstore_vaddr_));
    } else {
      emit_lookup(pb, "tlb_lookup_loadstore", null_vaddr());
    }
  }

  // 未绑定输入时的惰性 0: 保证 cell 里仍是 **节点** (而非未写的空句柄),
  // 消费端 has() 恒 true 且读到 0 ⇒ PADDR_VALID=0 ⇒ 回退 vaddr。
  static chc::ch_uint<kXlenBits> null_vaddr() {
    return chc::ch_uint<kXlenBits>(chc::ch_literal<0, kXlenBits>{});
  }

  // --------------------------------------------------------------------------
  // emit_lookup — 全相联 8 项 TLB 的组合查找 + 写 5 个输出 key
  //
  //   tag_hit_i = valid_i && (entry_vaddr_i[31:12] == vaddr[31:12])
  //   hit       = OR(tag_hit_i)
  //   page      = select 链 (命中项的 paddr[31:12])
  //   paddr     = page | vaddr[11:0]
  //   PADDR_VALID = sv_active && hit
  //
  // miss ⇒ PADDR_VALID=0 且 EXCEPTION_CODE=0 (**不是** fault: TLB miss 由 PTW
  // 处理, PTW 未落地前表现为回退 vaddr, 与 Bare 路径同构)。
  // --------------------------------------------------------------------------
  void emit_lookup(cf::plugin::PipeBuilder& pb, const char* node_name,
                   chc::ch_uint<kXlenBits> vaddr) {
    auto* n = pb.node_of_logic_stage(node_name).get();
    if (n == nullptr) {
      return;
    }

    const auto vpn = chc::bits<kXlenBits - 1, kPageOffBits>(vaddr);
    const auto page_off = chc::bits<kPageOffBits - 1, 0>(vaddr);

    // 8 路 tag 比较 (逐项 ch_bool)
    std::array<chc::ch_bool, kEntries> hit_lane{};
    for (std::size_t i = 0; i < kEntries; ++i) {
      const chc::ch_uint<kXlenBits> entry_va(*entry_vaddr_[i]);
      const chc::ch_uint<1> entry_v(*entry_valid_[i]);
      const auto tag = chc::bits<kXlenBits - 1, kPageOffBits>(entry_va);
      hit_lane[i] = (entry_v == chc::ch_uint<1>(chc::ch_literal<1, 1>{})) && (tag == vpn);
    }

    // OR 归约 → hit
    chc::ch_bool hit = hit_lane[0];
    for (std::size_t i = 1; i < kEntries; ++i) {
      hit = hit || hit_lane[i];
    }

    // 命中页基址: select 链 (各 lane 互斥 ⇒ 无歧义)
    const chc::ch_uint<kXlenBits> page_mask =
        chc::ch_uint<kXlenBits>(chc::ch_literal<0xFFFFF000U, kXlenBits>{});
    chc::ch_uint<kXlenBits> page = chc::ch_uint<kXlenBits>(chc::ch_literal<0, kXlenBits>{});
    for (std::size_t i = 0; i < kEntries; ++i) {
      const chc::ch_uint<kXlenBits> entry_pa(*entry_paddr_[i]);
      page = chc::select(hit_lane[i], entry_pa & page_mask, page);
    }

    // paddr = 页基址 | 页内偏移
    const chc::ch_uint<kXlenBits> paddr32 = page | chc::ch_uint<kXlenBits>(page_off);

    // sv 模式门控: 读 satp 的 MODE 位 (sv32: bit31) —— **运行期信号**, 不是
    // 编译期常量 ⇒ `csr_write_satp` 后续接线时模式可改。MODE=0 (Bare) ⇒ 恒不翻译。
    const chc::ch_bool sv_active =
        chc::bits<kXlenBits - 1, kXlenBits - 1>(chc::ch_uint<kXlenBits>(*satp_)) ==
        chc::ch_uint<1>(chc::ch_literal<1, 1>{});
    const chc::ch_bool paddr_valid = sv_active && hit;

    // 5 个输出 key —— 写入的是 **节点** (key 已 mode-aware):
    //   MMU_VADDR / PADDR : uint_t<64> = ch_uint<64>  ⇒ 需从 32 位 zext
    //   PADDR_VALID / PTW_ACTIVE : uint_t<1> = ch_uint<1>
    //   EXCEPTION_CODE : uint_t<8> = ch_uint<8>
    // PTW_ACTIVE / EXCEPTION_CODE 本增量恒 0 —— 这不是"折常量凑绿", 而是语义
    // 事实: 无 PTW 时恒不 active; TLB miss 不是 fault (回退 vaddr, 与 Bare 同构)。
    // 仍显式写入而非留空: 留空会让消费端 has()=false 并拿到空句柄。
    n->operator()(MmuKeys::MMU_VADDR) = chc::ch_uint<64>(vaddr);
    n->operator()(MmuKeys::PADDR) = chc::ch_uint<64>(paddr32);
    n->operator()(MmuKeys::PADDR_VALID) = chc::ch_uint<1>(paddr_valid);
    n->operator()(MmuKeys::PTW_ACTIVE) = chc::ch_uint<1>(chc::ch_literal<0, 1>{});
    n->operator()(MmuKeys::EXCEPTION_CODE) = chc::ch_uint<8>(chc::ch_literal<0, 8>{});
  }

  std::uint32_t satp_init_;
  bool tlb_created_ = false;

  chc::ch_uint<kXlenBits>* ifetch_vaddr_ = nullptr;
  chc::ch_uint<kXlenBits>* loadstore_vaddr_ = nullptr;

  std::array<std::unique_ptr<chc::ch_reg<chc::ch_uint<1>>>, kEntries> entry_valid_{};
  std::array<std::unique_ptr<chc::ch_reg<chc::ch_uint<kXlenBits>>>, kEntries> entry_vaddr_{};
  std::array<std::unique_ptr<chc::ch_reg<chc::ch_uint<kXlenBits>>>, kEntries> entry_paddr_{};
  std::unique_ptr<chc::ch_reg<chc::ch_uint<kXlenBits>>> satp_;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_PLUGINS_MMU_PLUGIN_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM
