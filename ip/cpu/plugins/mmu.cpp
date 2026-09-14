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

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/tlm/cpu_keys.h"
#include "ip/mmu/lib/multi_level_tlb.h"
#include "ip/mmu/tlm/mmu_keys.h"

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

void RiscvMMUPlugin::setup(cf::plugin::PipeBuilder& pb) {
  // mmu-cache-integration commit 2/9: 声明 3 个 substage 给 CPU pipeline hook
  // csr_write_satp / sfence_vma 挂 execute 阶段 (CSR 写 / SFENCE.VMA 在 execute 拦截)
  // mmu_exit 挂 memory 阶段 (MMU access 后路由 exception)
  pb.declare_substage("execute", "csr_write_satp", 1);
  pb.declare_substage("execute", "sfence_vma", 1);
  pb.declare_substage("memory", "mmu_exit", 1);
}

void RiscvMMUPlugin::build(cf::plugin::PipeBuilder& pb) {
  using cpu_keys_t = cf::cpu::tlm::payload::cpu_keys<std::uint64_t>;
  using mmu_keys_t = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

  // csr_write_satp 闭包: CPU execute 阶段写 cpu_keys::SAT 后, 此闭包读取并路由到 csr_write_satp hook
  // D4 合规: 用 if/else 全分支, 无早返
  pb.at_stage("csr_write_satp", cf::plugin::Phase::NORMAL, [this, &pb]() {
    auto node = pb.node_of_logic_stage("csr_write_satp");
    if (node != nullptr && node->has(cpu_keys_t::SAT)) {
      const std::uint64_t satp_value = node->operator()(cpu_keys_t::SAT);
      csr_write_satp(satp_value);
    } else {
      // node 不存在或 SAT 未写入, no-op (D4 全分支)
      (void)0;
    }
  });

  // sfence_vma 闭包: CPU execute 阶段写 cpu_keys::SFENCE_VADDR + SFENCE_ASID 后, 此闭包读取
  pb.at_stage("sfence_vma", cf::plugin::Phase::NORMAL, [this, &pb]() {
    auto node = pb.node_of_logic_stage("sfence_vma");
    if (node != nullptr && node->has(cpu_keys_t::SFENCE_VADDR)
        && node->has(cpu_keys_t::SFENCE_ASID)) {
      const std::int64_t rs1_vaddr = static_cast<std::int64_t>(
          node->operator()(cpu_keys_t::SFENCE_VADDR));
      const std::int64_t rs2_asid = static_cast<std::int64_t>(
          node->operator()(cpu_keys_t::SFENCE_ASID));
      sfence_vma(rs1_vaddr, rs2_asid);
    } else {
      (void)0;
    }
  });

  // mmu_exit 闭包: MMU access 完后, 读 mmu_keys::EXCEPTION_CODE 并写到 cpu_keys::CPU_EXCEPTION_CODE
  // 让 CPU pipeline 走 trap 路径 (M5 集成时)
  pb.at_stage("mmu_exit", cf::plugin::Phase::NORMAL, [&pb]() {
    auto node = pb.node_of_logic_stage("mmu_exit");
    if (node != nullptr && node->has(mmu_keys_t::EXCEPTION_CODE)) {
      const std::uint8_t exc_code = node->operator()(mmu_keys_t::EXCEPTION_CODE);
      node->put(cpu_keys_t::CPU_EXCEPTION_CODE, exc_code);
    } else {
      (void)0;
    }
  });
}

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
