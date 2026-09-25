// ip/mmu/tlm/MMUPlugin.cpp
//
// 功能描述: MMUPlugin at_stage closures 实装 (mmu-tlb-ptw-impl, commit 7/11)
//
// 关键设计:
//   - 5 个 logical stage 声明: tlb_lookup_ifetch / tlb_lookup_loadstore / ptw_l2 / ptw_l1 / ptw_l0
//   - 命中时同时写 pl::PADDR + pl::MMU_VADDR (ADR-044 §3.2)
//   - PTW miss 期间写 pl::PTW_ACTIVE=1, walk 完成写 pl::PADDR + pl::MMU_VADDR
//   - D4: 0 业务 tick(), 0 早返, 0 ch_mem/ch_reg/ch_uint 渗透

#include "ip/mmu/tlm/MMUPlugin.h"

#include "cf/plugin/pipe_builder.h"  // for Phase enum

#include "ip/mmu/tlm/mmu_keys.h"

namespace cf {
namespace ip {
namespace mmu {

MMUPlugin::MMUPlugin(SvMode mode, std::vector<TLBConfig> levels_cfg, PTWConfig ptw_cfg,
                       MemoryInterface* mem)
    : sv_mode_(mode), ptw_config_(ptw_cfg), mem_(mem) {
  std::vector<std::unique_ptr<TLBBase>> levels;
  for (auto& cfg : levels_cfg) {
    TLBLevelConfig level_cfg;
    level_cfg.name = cfg.name;
    level_cfg.entries = cfg.entries;
    level_cfg.associativity = cfg.associativity;
    level_cfg.replacement_policy = cfg.replacement_policy;
    levels.push_back(TLBFactory::create(level_cfg));
  }
  multi_tlb_ = std::make_unique<MultiLevelTLB>(std::move(levels));
  ptw_ = std::make_unique<PTW>(mode, ptw_cfg.max_inflight);
}

void MMUPlugin::setup(cf::plugin::PipeBuilder& pb) {
  // 5 个 logical stage (mmu-tlb-ptw-impl commit 7: 沿用骨架的 5 substage 命名)
  pb.declare_substage("fetch", "tlb_lookup_ifetch", 1);
  pb.declare_substage("memory", "tlb_lookup_loadstore", 1);
  pb.declare_substage("tlb_lookup_ifetch", "ptw_l0", 1);
  pb.declare_substage("ptw_l0", "ptw_l1", 1);
  pb.declare_substage("ptw_l1", "ptw_l2", 1);
}

void MMUPlugin::build(cf::plugin::PipeBuilder& pb) {
  using namespace cf::plugin;
  using Key = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

  auto do_lookup = [this, &pb](const char* stage_name, uint64_t vaddr) {
    auto node = pb.node_of_logic_stage(stage_name);
    if (!node) return;
    auto result = multi_tlb_->lookup(vaddr, current_asid_);
    if (result.hit) {
      // mmu-paddr-consume-and-real-memory (P1#3 task §4.3 + §5.3, Oracle C6):
      // PADDR + PADDR_VALID 同点同 phase (NORMAL) 原子写, 镜像 PTW_ACTIVE 先例
      (*node)(Key::PADDR) = result.paddr;
      (*node)(Key::PADDR_VALID) = true;
      (*node)(Key::MMU_VADDR) = vaddr;  // ADR-044 §3.2 VIPT dual-write
    } else {
      (*node)(Key::PTW_ACTIVE) = 1;
      (*node)(Key::PTW_VADDR) = vaddr;
    ptw_->start_walk(vaddr, current_asid_, /*satp_ppn=*/0,
      [this, node, vaddr](uint64_t paddr, uint8_t perms) {
        // PTW 成功完成: 同点同 phase 原子写 PADDR + PADDR_VALID
        (*node)(Key::PADDR) = paddr;
        (*node)(Key::PADDR_VALID) = true;
        (*node)(Key::MMU_VADDR) = vaddr;
        // Wave 2 Commit A: refill 统一 TLB，关闭 tlb_lookup_ifetch stall 链空转
        multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms);
        (*node)(Key::PTW_ACTIVE) = 0;
      },
      [node, vaddr](uint8_t fault_code) {
        // PTW fault: 写 EXCEPTION_CODE, PADDR_VALID=false (Oracle C6)
        (*node)(Key::PADDR_VALID) = false;
        (*node)(Key::EXCEPTION_CODE) = fault_code;
        (*node)(Key::MMU_VADDR) = vaddr;
        (*node)(Key::PTW_ACTIVE) = 0;
      });
    }
  };

  pb.at_stage("tlb_lookup_ifetch", Phase::NORMAL, [this, &pb, do_lookup]() {
    do_lookup("tlb_lookup_ifetch", last_vaddr_);
  });

  pb.at_stage("tlb_lookup_loadstore", Phase::NORMAL, [this, &pb, do_lookup]() {
    do_lookup("tlb_lookup_loadstore", last_vaddr_);
  });

  // mmu-paddr-consume-and-real-memory (P1#3 task §4.3):
  // mem_ == nullptr → stub 路径 (现有 47 mmu 测试 0 回归)
  // mem_ != nullptr → 真内存读路径 (MemoryInterface 抽象, design.md D2 Sv32-only scope)
  // 由 advance_from_real_memory 内部 null 检查统一处理 (DRY)
  pb.at_stage("ptw_l0", Phase::NORMAL, [this]() { ptw_->advance_from_real_memory(mem_); });
  pb.at_stage("ptw_l1", Phase::NORMAL, [this]() { ptw_->advance_from_real_memory(mem_); });
  pb.at_stage("ptw_l2", Phase::NORMAL, [this]() { ptw_->advance_from_real_memory(mem_); });
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
