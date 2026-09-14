// ip/mmu/tlm/MMUPlugin.h
//
// 功能描述: MMUPlugin — D4 Plugin 框架入口 (mmu-ip-skeleton, 8.2-8.9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - 派生 cf::plugin::PluginBase
//   - 持 lib/ 算法 (MultiLevelTLB + PTW) 为成员
//   - setup() 用 declare_substage() 声明 5 个 logical stage
//   - build() 用 at_stage() 注册 5 个闭包 (D4 强制)
//   - 0 业务 tick() (D4 强制)

#ifndef CF_IP_MMU_TLM_MMU_PLUGIN_H
#define CF_IP_MMU_TLM_MMU_PLUGIN_H

#include <cstdint>
#include <memory>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/mmu/lib/multi_level_tlb.h"
#include "ip/mmu/lib/ptw.h"
#include "ip/mmu/lib/tlb_factory.h"

namespace cf {
namespace ip {
namespace mmu {

class MMUPlugin : public cf::plugin::PluginBase {
 public:
  struct TLBConfig {
    std::string name;
    std::size_t entries;
    std::size_t associativity;
    std::size_t num_lookup_ports = 1;
    std::size_t lookup_latency_cycles = 1;
    std::string replacement_policy = "LRU";
  };
  struct PTWConfig {
    std::size_t max_inflight = 2;
  };

  MMUPlugin(SvMode mode, std::vector<TLBConfig> levels_cfg, PTWConfig ptw_cfg);
  ~MMUPlugin() override = default;

  void setup(cf::plugin::PipeBuilder& pb) override;
  void build(cf::plugin::PipeBuilder& pb) override;

  // 测试访问
  SvMode mode() const { return sv_mode_; }
  std::size_t num_levels() const { return multi_tlb_ ? multi_tlb_->num_levels() : 0; }

  // mmu-cache-integration commit 6: 公开 multi_tlb_ 访问器供 RiscvMMUPlugin
  // sfence_vma + csr_write_satp hook 调用 invalidate_* API (mmu-tlb-ptw-impl 遗留 defer)
  cf::ip::mmu::MultiLevelTLB* multi_tlb() const { return multi_tlb_.get(); }

  // 多 ASID 失效 (RISC-V SFENCE.VMA rs1!=x0, rs2=x0 语义)
  void invalidate_vaddr_any_asid(std::uint64_t vaddr) {
    if (multi_tlb_) multi_tlb_->invalidate_vaddr_any_asid(vaddr);
  }

  // ptw-walk-bridge-fix commit A: 公开 ptw_ 访问器供测试种 PTE chain
  // (mirror multi_tlb 模式; lib/ 类型出现在 tlm/ public API 是 stub 测试 API 的
  // 妥协, 与 mmutlb-ptw-impl commit 4 stub_write_pte public accessor 一致)
  cf::ip::mmu::PTW* ptw() const { return ptw_.get(); }

 private:
  SvMode sv_mode_;
  PTWConfig ptw_config_;
  std::unique_ptr<MultiLevelTLB> multi_tlb_;
  std::unique_ptr<PTW> ptw_;
  std::uint16_t current_asid_ = 0;

  // 闭包状态 (跨 at_stage 调用, 同一 logical stage 复用)
  std::uint64_t last_vaddr_ = 0;
  std::uint64_t last_paddr_ = 0;
  std::uint8_t  last_perms_ = 0;
  std::uint8_t  last_fault_ = 0;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_TLM_MMU_PLUGIN_H
