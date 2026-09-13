// tests/mmu/test_mmu_plugin.cpp (mmu-ip-skeleton + mmu-tlb-ptw-impl commit 11 + mmu-cache-integration commit 6)

#include "catch_amalgamated.hpp"

#include "ip/cpu/plugins/mmu.h"
#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace ip {
namespace mmu {

TEST_CASE("Construct", "[mmu][MMUPlugin]") {
  // VIPT-safe: 8/8 全关联 (idx=0 + offset=12=12 SAFE)
  std::vector<MMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "FIFO"},
    {"L1", 8, 8, 1, 2, "LRU"}
  };
  MMUPlugin::PTWConfig ptw_cfg{2};
  MMUPlugin mmu(SvMode::Sv39, levels, ptw_cfg);
  CHECK(mmu.num_levels() == 2u);
}

TEST_CASE("ModeSet", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv48, levels, {});
  CHECK(mmu.mode() == SvMode::Sv48);
}

TEST_CASE("SetupDeclaresSubstages", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  cf::plugin::PipeBuilder pb;
  // 直接用构造参数 make_unique, 避免 std::move (MMUPlugin 含 unique_ptr 不可拷贝)
  pb.register_plugin(std::make_unique<MMUPlugin>(SvMode::Sv39, levels, MMUPlugin::PTWConfig{2}));
  pb.build();
  // 验证 5 个 substage 都被注册
  CHECK(pb.has_stage("tlb_lookup_ifetch"));
  CHECK(pb.has_stage("tlb_lookup_loadstore"));
  CHECK(pb.has_stage("ptw_l0"));
  CHECK(pb.has_stage("ptw_l1"));
  CHECK(pb.has_stage("ptw_l2"));
}

TEST_CASE("BareMode", "[mmu][MMUPlugin]") {
  // VIPT-safe: 8/8 全关联
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "None"}};
  MMUPlugin mmu(SvMode::Bare, levels, {});
  CHECK(mmu.mode() == SvMode::Bare);
}

TEST_CASE("SingleLevelConfig", "[mmu][MMUPlugin]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  MMUPlugin mmu(SvMode::Sv39, levels, {});
  CHECK(mmu.num_levels() == 1u);
}

// mmu-cache-integration commit 6: RISC-V hook 实装回归网
// (sfence_vma + csr_write_satp: mmu-tlb-ptw-impl 遗留 defer)

TEST_CASE("SFENCEVMAInvalidatesTLBEntry", "[mmu][MMUPlugin][RiscV]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  cf::cpu::plugins::RiscvMMUPlugin mmu(SvMode::Sv39, levels, MMUPlugin::PTWConfig{2}, /*satp_value=*/0);
  // 预填 TLB entry (直接通过 multi_tlb())
  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);

  // SFENCE.VMA rs1=0x4000_0000, rs2=0 (single vaddr invalidate)
  mmu.sfence_vma(0x40000000LL, 0LL);

  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
}

TEST_CASE("SFENCEVMARs1ZeroTriggersAllInvalidate", "[mmu][MMUPlugin][RiscV]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  cf::cpu::plugins::RiscvMMUPlugin mmu(SvMode::Sv39, levels, MMUPlugin::PTWConfig{2}, 0);
  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);
  mmu.multi_tlb()->level(0)->insert(0x50000000ULL, 0x90000000ULL, 0, 0xFF);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x50000000ULL, 0).hit);

  // SFENCE.VMA rs1=x0, rs2=x0 → invalidate all
  mmu.sfence_vma(0LL, 0LL);

  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x50000000ULL, 0).hit);
}

TEST_CASE("CsrWriteSatpUpdatesModeAndInvalidatesTLB", "[mmu][MMUPlugin][RiscV]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  // satp_value 低 4 bits = 8 (Sv39), PPN = 0x1000
  constexpr std::uint64_t kSatpSv39 = 0x8000ULL;  // MODE=8, PPN=0
  cf::cpu::plugins::RiscvMMUPlugin mmu(SvMode::Bare, levels, MMUPlugin::PTWConfig{2}, kSatpSv39);
  CHECK(mmu.satp_value() == kSatpSv39);

  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);

  // 写入新 satp (切换地址空间) → invalidate all
  constexpr std::uint64_t kSatpSv39NewRoot = 0x8000'1000ULL;  // MODE=8, PPN=0x1000
  mmu.csr_write_satp(kSatpSv39NewRoot);
  CHECK(mmu.satp_value() == kSatpSv39NewRoot);
  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
}

TEST_CASE("MultiLevelTLBInvalidateASIDSwitch", "[mmu][MMUPlugin]") {
  // 通过 MultiLevelTLB 直接测 ASID invalidate (不依赖 RiscV hook)
  std::vector<MMUPlugin::TLBConfig> levels = {
    {"L0", 8, 8, 1, 1, "LRU"},
    {"L1", 8, 8, 1, 2, "LRU"}
  };
  MMUPlugin mmu(SvMode::Sv39, levels, {});

  // ASID 0 和 ASID 1 都有 entry
  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);
  mmu.multi_tlb()->level(0)->insert(0x50000000ULL, 0x90000000ULL, 1, 0xFF);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x50000000ULL, 1).hit);

  // ASID 0 invalidate
  mmu.multi_tlb()->invalidate_asid(0);
  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x50000000ULL, 1).hit);  // ASID 1 不受影响
}

// Oracle round-2 fix: SFENCE.VMA rs1!=x0, rs2=x0 必须 invalidate vaddr 在所有 ASID
// (之前实现只 invalidate asid == 0, 多 ASID 场景会留 stale entry)
TEST_CASE("SFENCEVMARs1NonZeroInvalidatesAllASIDs", "[mmu][MMUPlugin][RiscV]") {
  std::vector<MMUPlugin::TLBConfig> levels = {{"L0", 8, 8, 1, 1, "LRU"}};
  cf::cpu::plugins::RiscvMMUPlugin mmu(SvMode::Sv39, levels, MMUPlugin::PTWConfig{2}, 0);

  // 同一 vaddr 在 ASID 0、ASID 1 都有 entry (模拟多个 address space 映射同一虚拟地址)
  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x80000000ULL, 0, 0xFF);
  mmu.multi_tlb()->level(0)->insert(0x40000000ULL, 0x90000000ULL, 1, 0xFF);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 1).hit);

  // SFENCE.VMA rs1=0x4000_0000, rs2=x0 → invalidate vaddr 跨所有 ASID
  mmu.sfence_vma(0x40000000LL, 0LL);

  // 两个 ASID 的 entry 都应被失效 (RISC-V Spec §6.2)
  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 0).hit);
  CHECK_FALSE(mmu.multi_tlb()->level(0)->lookup(0x40000000ULL, 1).hit);
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
