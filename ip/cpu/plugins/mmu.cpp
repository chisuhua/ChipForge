// ip/cpu/plugins/mmu.cpp
//
// 功能描述: RISC-V MMU 适配器实装 (mmu-cache-integration commit 6/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 设计:
//   - RiscvMMUPlugin 继承 cf::ip::mmu::MMUPlugin
//   - sfence_vma + csr_write_satp hook 路由到 lib/MultiLevelTLB 的 invalidate_* API
//   - 通过 mmu-cache-integration commit 6 新增的 MMUPlugin::multi_tlb() 公开 getter 访问
//   - D4 强制: 0 业务 tick(), 0 状态机, 用 lib/ 纯 C++ 算法
//   - 实装 mmu-tlb-ptw-impl 遗留的 sfence_vma + csr_write_satp defer
//
// RISC-V Spec §6.2 SFENCE.VMA 语义 (接口约定: 0 表示 x0 寄存器):
//   - rs1=0, rs2=0: invalidate ALL entries (regardless of asid)
//   - rs1!=0, rs2=0: invalidate all entries with matching vaddr, all asid
//   - rs1=0, rs2!=0: invalidate all entries with matching asid, all vaddr
//   - rs1!=0, rs2!=0: invalidate single entry (vaddr, asid)

#include "ip/cpu/plugins/mmu.h"

#include "ip/mmu/lib/multi_level_tlb.h"

namespace cf {
namespace cpu {
namespace plugins {

void RiscvMMUPlugin::csr_write_satp(std::uint64_t satp_value) {
  satp_value_ = satp_value;
  // satp CSR 切换地址空间 → invalidate ALL TLB entries (新 root page table)
  if (auto* tlb = multi_tlb()) {
    tlb->invalidate_all();
  }
}

void RiscvMMUPlugin::sfence_vma(std::int64_t rs1_vaddr, std::int64_t rs2_asid) {
  auto* tlb = multi_tlb();
  if (!tlb) return;

  if (rs1_vaddr == 0 && rs2_asid == 0) {
    // rs1=x0, rs2=x0: invalidate all
    tlb->invalidate_all();
  } else if (rs1_vaddr != 0 && rs2_asid == 0) {
    // rs1!=x0, rs2=x0: invalidate vaddr across ALL ASIDs (RISC-V Spec §6.2)
    tlb->invalidate_vaddr_any_asid(static_cast<std::uint64_t>(rs1_vaddr));
  } else if (rs1_vaddr == 0 && rs2_asid != 0) {
    // rs1=x0, rs2!=x0: invalidate asid (all vaddr)
    tlb->invalidate_asid(static_cast<std::uint16_t>(rs2_asid));
  } else {
    // rs1!=x0, rs2!=x0: invalidate single entry (vaddr, asid)
    tlb->invalidate_vaddr(static_cast<std::uint64_t>(rs1_vaddr),
                          static_cast<std::uint16_t>(rs2_asid));
  }
}

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
