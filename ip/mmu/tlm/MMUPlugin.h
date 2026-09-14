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
#include "ip/mmu/tlm/mmu_keys.h"
#include "bundles/tlb_bundles_extension.h"  // cf::bundles::TlbResp POD (commit B read_response)

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

  // ptw-walk-bridge-fix commit B: 公开 issue_request API 喂 last_vaddr_/current_asid_ 给 at_stage 闭包
  // (mirror L1CachePlugin::issue_request L1CachePlugin.h:123-124)
  void issue_request(uint64_t vaddr, uint16_t asid = 0) {
    last_vaddr_ = vaddr;
    current_asid_ = asid;
  }

  // ptw-walk-bridge-fix commit B: 公开 read_response API 从 tlb_lookup_ifetch 节点读结果
  // (mirror L1CachePlugin::read_response L1CachePlugin.h:133-134)
  // 从节点读 mmu_keys<T>::PADDR / EXCEPTION_CODE (do_lookup 闭包写入)
  // hit 推导规则: hit = (exception_code == 0 && paddr != 0)
  // (mmu-cache-integration commit 8 没 缺 PERMS payload key 写入, 暂以 paddr!=0 间接证明 hit)
  cf::bundles::TlbResp read_response(
      const std::shared_ptr<cf::plugin::PipeNode>& n) const {
    using Keys = payload::mmu_keys<std::uint64_t>;
    cf::bundles::TlbResp resp{};
    if (!n) return resp;
    std::uint64_t paddr = static_cast<std::uint64_t>(n->operator()(Keys::PADDR));
    std::uint8_t exception = static_cast<std::uint8_t>(n->operator()(Keys::EXCEPTION_CODE));
    resp.paddr = static_cast<cf::plugin::uint_t<64>>(paddr);
    resp.fault_code = static_cast<cf::plugin::uint_t<4>>(exception);
    resp.hit = (exception == 0 && paddr != 0);
    resp.fault = (exception != 0);
    return resp;
  }

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
