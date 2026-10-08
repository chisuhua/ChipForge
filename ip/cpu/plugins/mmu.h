// ip/cpu/plugins/mmu.h
//
// 功能描述: RISC-V MMU 适配器 (mmu-ip-skeleton, 11.1-11.3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - RiscvMMUPlugin 继承 cf::ip::mmu::MMUPlugin
//   - 骨架阶段占位: RISC-V 特定 hook 推迟到 mmu-tlb-ptw-impl
//   - 向后兼容: using MMUPlugin = RiscvMMUPlugin (旧名仍可用, 保 M3 阶段 cpu 测试 0 破坏)
//
// 未来 RISC-V 特定 hook (推迟到 mmu-tlb-ptw-impl):
//   - satp CSR 写入拦截 → 更新 PTW 配置
//   - SFENCE.VMA 指令拦截 → 触发 multi_tlb_->invalidate_vaddr/asid/all
//   - exception code 12/13/15 (page fault / access fault) 映射
//   - mstatus.MXR/SUM 位行为 (影响 permission check)
//
// 约束:
//   - 骨架阶段: 仅依赖基类接口, RISC-V 特定 hook 推迟

#ifndef CF_IP_CPU_PLUGINS_MMU_H
#define CF_IP_CPU_PLUGINS_MMU_H

#include <cstdint>
#include <utility>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/mmu/lib/memory_interface.h"
#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace cpu {
namespace plugins {

class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin {
 public:
  // mmu-paddr-consume-and-real-memory (P1#3 task §4.2, Oracle C7):
  // mem 透传到基类 MMUPlugin, nullptr → stub 路径 (向后兼容, 现有 6 cpu-l1-mmu-demo 测试 0 回归)
  //
  // cpu-factory-satp-mapping (v0.10.2): 原 ctor 仅存 satp_value_ 到派生成员, 基类 satp_ppn_ 仍 0
  // → ADR-049 Bare shortcut (sv_mode_==Bare || satp_ppn_==0) 永真 → 翻译永远 Bare → 修复无效.
  // 必须把 satp 传到基类让 PTW 知道根页表位置. 派生 set_satp_value shadow 基类同名, 用 qualified.
  // PPN 提取 mode-aware (避免 mmu.cpp:37 错 48-bit mask 对 Sv32 含 MODE bit31 → root 天文数字).
  // 此处 inline 而非 #include "ip/cpu/cpu_factory.h" 是因 cpu_factory.h:46 已 include 本头
  // (会循环依赖); cf::cpu::detail::extract_satp_ppn 是单元测试入口, 此处重复 6 行可接受.
  RiscvMMUPlugin(cf::ip::mmu::SvMode mode, std::vector<cf::ip::mmu::MMUPlugin::TLBConfig> levels_cfg,
                 cf::ip::mmu::MMUPlugin::PTWConfig ptw_cfg, std::uint64_t satp_value = 0,
                 cf::ip::mmu::MemoryInterface* mem = nullptr)
      : cf::ip::mmu::MMUPlugin(mode, std::move(levels_cfg), ptw_cfg, mem) {
    cf::ip::mmu::MMUPlugin::set_satp_value(satp_value);  // qualified: 调用基类 (派生 shadow)
    std::uint64_t ppn = 0;
    switch (mode) {
      case cf::ip::mmu::SvMode::Sv32:
        ppn = satp_value & 0x3FFFFFULL;       // Sv32 PPN 22 bits [21:0] (RISC-V Spec §4.3.1)
        break;
      case cf::ip::mmu::SvMode::Sv39:
      case cf::ip::mmu::SvMode::Sv48:
        ppn = satp_value & 0xFFFFFFFFFULL;    // Sv39/48 PPN 44 bits [43:0]
        break;
      case cf::ip::mmu::SvMode::Bare:
      default:
        ppn = 0;                              // Bare mode: 无 PTE chain
        break;
    }
    set_satp_ppn(ppn);                         // unqualified: 派生无 shadow, 解析到基类
  }

  // cpu-mmu-integration commit 2/9: 覆盖基类 setup/build 加 3 个 substage 闭包
  void setup(cf::plugin::PipeBuilder& pb) override;
  void build(cf::plugin::PipeBuilder& pb) override;

  // mmu-paddr-consume-and-real-memory (P1#3 task §7 C9-b, Oracle 2026-09-25):
  // vaddr_for_stage override — production 路径从父 stage 节点读真实 vaddr
  //   tlb_lookup_ifetch → 父 "fetch" 节点读 PC
  //   tlb_lookup_loadstore → 父 "memory" 节点读 MEM_ADDR
  //   fallback: 基类返回 last_vaddr_ (test/bridge 路径)
 protected:
  std::uint64_t vaddr_for_stage(const char* stage_name) const override;

 public:
  ~RiscvMMUPlugin() override = default;

  RiscvMMUPlugin(const RiscvMMUPlugin&) = delete;
  RiscvMMUPlugin& operator=(const RiscvMMUPlugin&) = delete;

  // RISC-V satp CSR 拦截：解析 MODE (4 bits) + PPN
  void csr_write_satp(std::uint64_t satp_value);

  // RISC-V SFENCE.VMA 拦截: 接口约定 0 表示 x0 寄存器 (all-invalidate 维度)
  void sfence_vma(std::int64_t rs1_vaddr, std::int64_t rs2_asid);

  // 异常码 12/13/15 写入 (RISC-V page fault mapping)
  void set_exception_code(std::uint8_t code) { last_exception_code_ = code; }
  std::uint8_t exception_code() const { return last_exception_code_; }

 private:
  std::uint8_t last_exception_code_ = 0;
  cf::plugin::PipeBuilder* pb_for_vaddr_ = nullptr;  // debug-cpu-l1-mmu-demo-paddr-regression Phase C
};

using MMUPlugin = RiscvMMUPlugin;

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_H
