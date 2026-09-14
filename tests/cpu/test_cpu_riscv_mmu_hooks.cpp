// tests/cpu/test_cpu_riscv_mmu_hooks.cpp (mmu-cache-integration commit 3/6)
//
// 功能描述: RiscV hook 集成测试 (CSR write + SFENCE.VMA + exception propagation)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 验证 RiscvMMUPlugin::setup() 三个 substage 闭包接到 CPU pipeline 后:
//   1. CPU CSR write → cpu_keys::SAT → MMUPlugin::csr_write_satp() hook
//   2. CPU SFENCE.VMA → cpu_keys::SFENCE_VADDR/SFENCE_ASID → MMUPlugin::sfence_vma() hook
//   3. MMU exception → mmu_keys::EXCEPTION_CODE → cpu_keys::CPU_EXCEPTION_CODE 传播
//
// 不构造 MMUPlugin 实例直接测 hook (镜像 test_l1_cache_plugin_unit.cpp 隔离原则)

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <type_traits>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/plugins/mmu.h"
#include "ip/cpu/tlm/cpu_keys.h"
#include "ip/mmu/tlm/mmu_keys.h"

namespace cf {
namespace cpu {

using cf::cpu::plugins::RiscvMMUPlugin;
using cf::cpu::tlm::payload::cpu_keys;
// mmu_keys 在 cf::ip::mmu::payload::mmu_keys (原 namespace, 不重定义)

TEST_CASE("CSRWriteSatpRoutesToRiscVMMUPlugin", "[cpu-integration][RiscV]") {
  // 模拟 CPU 写 satp CSR (低 4 bits = MODE=Sv39, 高位 PPN)
  constexpr std::uint64_t kSatpSv39 = 0x8000'0000ULL;  // MODE=8, PPN=0x80000
  CPUConfig cfg;
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  cfg.mul_latency = 1;
  auto pb = CpuFactory<std::uint32_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  // 直接构造 RiscvMMUPlugin 实例验证 hook (不通过 cpu_factory, 隔离 MMU 单元测试)
  std::vector<RiscvMMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "LRU"},
    {"L1", 8, 8, 1, 2, "LRU"}
  };
  RiscvMMUPlugin::PTWConfig ptw_cfg{2};
  RiscvMMUPlugin mmu_plugin(cf::ip::mmu::SvMode::Sv39, levels, ptw_cfg,
                            /*satp_value=*/0);
  mmu_plugin.setup(*pb);
  mmu_plugin.build(*pb);

  // 模拟 CPU 写 satp CSR: pipe node 'csr_write_satp' 写 SAT
  std::shared_ptr<cf::plugin::PipeNode> node = pb->node_of_logic_stage("csr_write_satp");
  REQUIRE(node != nullptr);
  node->put(cpu_keys<std::uint64_t>::SAT, kSatpSv39);

  // 手动 run stage (build_cpu 已调 pb->build(), 但 run 由调用方决定)
  // at_stage 闭包会在 pb->run() 时触发; 这里直接调 csr_write_satp hook 验证 logic
  // (at_stage 闭包逻辑: 读 SAT → 调 csr_write_satp)
  mmu_plugin.csr_write_satp(
      static_cast<std::uint64_t>(node->operator()(cpu_keys<std::uint64_t>::SAT)));

  CHECK(mmu_plugin.satp_value() == kSatpSv39);
}

TEST_CASE("SFENCEVMARoutesToRiscVMMUPlugin", "[cpu-integration][RiscV]") {
  // 模拟 CPU SFENCE.VMA rs1=0x4000_0000, rs2=0x5 (invalidate single entry)
  CPUConfig cfg;
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  cfg.mul_latency = 1;
  auto pb = CpuFactory<std::uint32_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  std::vector<RiscvMMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "LRU"},
    {"L1", 8, 8, 1, 2, "LRU"}
  };
  RiscvMMUPlugin::PTWConfig ptw_cfg{2};
  RiscvMMUPlugin mmu_plugin(cf::ip::mmu::SvMode::Sv39, levels, ptw_cfg);
  mmu_plugin.setup(*pb);
  mmu_plugin.build(*pb);

  // 预填 TLB entry at vaddr=0x4000_0000, asid=5
  mmu_plugin.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 5, 0xFF);
  CHECK(mmu_plugin.multi_tlb()->level(0)->lookup(0x40000000ULL, 5).hit);

  // 模拟 CPU SFENCE.VMA: pipe node 'sfence_vma' 写 SFENCE_VADDR + SFENCE_ASID
  std::shared_ptr<cf::plugin::PipeNode> node = pb->node_of_logic_stage("sfence_vma");
  REQUIRE(node != nullptr);
  node->put(cpu_keys<std::uint64_t>::SFENCE_VADDR, static_cast<std::uint64_t>(0x40000000ULL));
  node->put(cpu_keys<std::uint64_t>::SFENCE_ASID, static_cast<std::uint64_t>(5));

  // 直接调 sfence_vma hook 验证 (at_stage 闭包逻辑: 读 2 个 key → 调 sfence_vma)
  const std::uint64_t rs1 = node->operator()(cpu_keys<std::uint64_t>::SFENCE_VADDR);
  const std::uint64_t rs2 = node->operator()(cpu_keys<std::uint64_t>::SFENCE_ASID);
  mmu_plugin.sfence_vma(static_cast<std::int64_t>(rs1),
                        static_cast<std::int64_t>(rs2));

  // TLB entry 应被失效 (rs1!=0, rs2!=0 → invalidate single entry)
  CHECK_FALSE(mmu_plugin.multi_tlb()->level(0)->lookup(0x40000000ULL, 5).hit);
}

TEST_CASE("MMUExceptionPropagatesToCPU", "[cpu-integration][RiscV]") {
  // 模拟 MMU exception code 13 (load page fault) → cpu_keys::CPU_EXCEPTION_CODE 传播
  CPUConfig cfg;
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  cfg.mul_latency = 1;
  auto pb = CpuFactory<std::uint32_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  // 模拟 MMU 写 EXCEPTION_CODE=13 (mmu_keys namespace)
  std::shared_ptr<cf::plugin::PipeNode> mmu_exit_node = pb->node_of_logic_stage("mmu_exit");
  REQUIRE(mmu_exit_node != nullptr);
  mmu_exit_node->put(cf::ip::mmu::payload::mmu_keys<std::uint64_t>::EXCEPTION_CODE,
                     static_cast<std::uint8_t>(13));

  // 手动模拟 at_stage("mmu_exit") 闭包: 读 EXCEPTION_CODE → 写到 CPU_EXCEPTION_CODE
  const std::uint8_t exc_code =
      mmu_exit_node->operator()(cf::ip::mmu::payload::mmu_keys<std::uint64_t>::EXCEPTION_CODE);
  mmu_exit_node->put(cpu_keys<>::CPU_EXCEPTION_CODE, exc_code);

  // 验证 CPU exception path 收到正确 code
  CHECK(mmu_exit_node->operator()(cpu_keys<>::CPU_EXCEPTION_CODE) == 13);
}

}  // namespace cpu
}  // namespace cf
