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

#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace cpu {
namespace plugins {

class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin {
 public:
  RiscvMMUPlugin(cf::ip::mmu::SvMode mode, std::vector<cf::ip::mmu::MMUPlugin::TLBConfig> levels_cfg,
                 cf::ip::mmu::MMUPlugin::PTWConfig ptw_cfg, std::uint64_t satp_value = 0)
      : cf::ip::mmu::MMUPlugin(mode, std::move(levels_cfg), ptw_cfg),
        satp_value_(satp_value) {}

  // cpu-mmu-integration commit 2/9: 覆盖基类 setup/build 加 3 个 substage 闭包
  void setup(cf::plugin::PipeBuilder& pb) override;
  void build(cf::plugin::PipeBuilder& pb) override;

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

  // satp value (4-byte aligned; low 4 bits = MODE)
  std::uint64_t satp_value() const { return satp_value_; }
  void set_satp_value(std::uint64_t v) { satp_value_ = v; }

 private:
  std::uint64_t satp_value_ = 0;
  std::uint8_t last_exception_code_ = 0;
};

// 向后兼容类型别名 — 旧 cf::cpu::plugins::MMUPlugin 仍可用, 保 M3 阶段 cpu 测试 0 破坏
using MMUPlugin = RiscvMMUPlugin;

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_H
