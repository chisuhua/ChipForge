// ip/mmu/tlm/MMUPlugin.cpp
//
// 功能描述: MMUPlugin 实装 — D4 at_stage 闭包 (mmu-ip-skeleton, 8.3-8.9)

#include <functional>
#include <memory>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/mmu/lib/ptw.h"
#include "ip/mmu/tlm/MMUPlugin.h"
#include "ip/mmu/tlm/mmu_keys.h"

namespace cf {
namespace ip {
namespace mmu {

MMUPlugin::MMUPlugin(SvMode mode, std::vector<TLBConfig> levels_cfg, PTWConfig ptw_cfg)
    : sv_mode_(mode), ptw_config_(ptw_cfg) {
  std::vector<std::unique_ptr<TLBBase>> levels;
  for (const auto& cfg : levels_cfg) {
    levels.push_back(TLBFactory::create(cfg));
  }
  multi_tlb_ = std::make_unique<MultiLevelTLB>(std::move(levels), true);
  ptw_ = std::make_unique<PTW>(mode, ptw_cfg.max_inflight);
}

void MMUPlugin::setup(cf::plugin::PipeBuilder& pb) {
  // 5 个 logical stage 声明 (parent 必须为已存在 logic_stage 或上一 substage)
  pb.declare_substage("fetch", "tlb_lookup_ifetch", 1);
  pb.declare_substage("memory", "tlb_lookup_loadstore", 1);
  pb.declare_substage("tlb_lookup_ifetch", "ptw_l0", 1);
  pb.declare_substage("ptw_l0", "ptw_l1", 1);
  pb.declare_substage("ptw_l1", "ptw_l2", 1);
}

void MMUPlugin::build(cf::plugin::PipeBuilder& pb) {
  using Phase = cf::plugin::Phase;
  using K = payload::mmu_keys<std::uint64_t>;
  using K16 = payload::mmu_keys<std::uint16_t>;
  using K8 = payload::mmu_keys<std::uint8_t>;
  using K64 = payload::mmu_keys<std::uint64_t>;

  // Logical stage 0: tlb_lookup_ifetch — TLB hit 1-cycle, miss 触发 PTW
  pb.at_stage("tlb_lookup_ifetch", Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch");
    if (n) {
      std::uint64_t vaddr = (*n)(K::VADDR);
      auto r = multi_tlb_->lookup(vaddr, current_asid_);
      if (r.hit) {
        (*n)(K::PADDR) = r.paddr;
        (*n)(K8::PTW_ACTIVE) = false;
        (*n)(K8::PTW_FAULT) = 0;
      } else {
        (*n)(K8::PTW_ACTIVE) = true;
        (*n)(K::PTW_VADDR) = vaddr;
        (*n)(K16::PTW_ASID) = current_asid_;
        last_vaddr_ = vaddr;
        ptw_->start_walk(vaddr, current_asid_,
                         [this](std::uint64_t paddr, std::uint8_t perms) {
                           last_paddr_ = paddr;
                           last_perms_ = perms;
                         },
                         [this](std::uint8_t fault) {
                           last_fault_ = fault;
                         });
      }
    }
  });

  // Logical stage 0 (parallel): tlb_lookup_loadstore
  pb.at_stage("tlb_lookup_loadstore", Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("tlb_lookup_loadstore");
    if (n) {
      // 同 lookup_ifetch 逻辑, 使用 MEM_ADDR 作为 vaddr 源
      // 简化: 共用 lookup 逻辑
      std::uint64_t vaddr = (*n)(K::VADDR);
      auto r = multi_tlb_->lookup(vaddr, current_asid_);
      if (r.hit) {
        (*n)(K::PADDR) = r.paddr;
        (*n)(K8::PTW_ACTIVE) = false;
      } else {
        (*n)(K8::PTW_ACTIVE) = true;
        (*n)(K::PTW_VADDR) = vaddr;
        (*n)(K16::PTW_ASID) = current_asid_;
        last_vaddr_ = vaddr;
        ptw_->start_walk(vaddr, current_asid_, nullptr, nullptr);
      }
    }
  });

  // Logical stage 1-3: PTW 3 级 walk (mmu-tlb-ptw-impl 实施具体 PTE 读取)
  pb.at_stage("ptw_l0", Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("ptw_l0");
    if (n && (*n)(K8::PTW_ACTIVE)) {
      std::uint64_t pte = 0;  // stub: 实际从 memory 读
      ptw_->advance(pte, 0);
      (*n)(K64::PTW_L0_RAW) = pte;
    }
  });

  pb.at_stage("ptw_l1", Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("ptw_l1");
    if (n && (*n)(K8::PTW_ACTIVE)) {
      std::uint64_t pte = 0;  // stub
      ptw_->advance(pte, 1);
      (*n)(K64::PTW_L1_RAW) = pte;
    }
  });

  pb.at_stage("ptw_l2", Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("ptw_l2");
    if (n && (*n)(K8::PTW_ACTIVE)) {
      std::uint64_t pte = 0;  // stub: 末级 PTE, 触发 leaf
      ptw_->advance(pte, 2);
      (*n)(K64::PTW_L2_RAW) = pte;

      if (ptw_->is_done() && ptw_->result_fault() == 0) {
        (*n)(K::PADDR) = ptw_->result_paddr();
        (*n)(K8::PERMS) = ptw_->result_perms();
        (*n)(K8::PTW_ACTIVE) = false;
        multi_tlb_->refill_from_ptw(last_vaddr_, current_asid_,
                                   ptw_->result_paddr(), ptw_->result_perms());
      } else if (ptw_->is_done()) {
        (*n)(K8::PTW_FAULT) = ptw_->result_fault();
      }
    }
  });

  // CtrlLink halt_when PTW 期间 stall 下游 (推迟到 mmu-tlb-ptw-impl)
  // 骨架阶段仅声明接口位置
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
