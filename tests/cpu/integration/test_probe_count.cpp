#include "catch_amalgamated.hpp"
#include "ip/cpu/cpu_factory.h"
using namespace cf::cpu;
TEST_CASE("probe_7stage", "[probe]") {
  CPUConfig cfg;
  cfg.pipeline_stages = 7;
  cfg.dispatch_width = 2;
  cfg.n_lanes = 2;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 128;
  cfg.mul_latency = 3;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  auto pb = CpuFactory<std::uint32_t>::build_cpu(cfg);
  INFO("nodes=" << pb->node_count() << " stages=" << pb->stage_count());
  INFO("csr_write_satp=" << pb->has_stage("csr_write_satp"));
  INFO("sfence_vma=" << pb->has_stage("sfence_vma"));
  INFO("mmu_exit=" << pb->has_stage("mmu_exit"));
  INFO("tlb_lookup_ifetch=" << pb->has_stage("tlb_lookup_ifetch"));
  CHECK(pb->node_count() > 0);
}
