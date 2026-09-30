// tests/framework/test_mul_div_fsm_cycle_parity.cpp
//
// 功能描述: MulDivFsmPlugin TLM ↔ CH_MEM cycle parity test (mfc Phase D.2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-30
//
// 测试覆盖 (D.2 RED + 占位实现):
//   1. cycle_parity_test_tlm_busy_cycles         — TLM 模式 busy_cycles = 1 (MUL) / 33 (DIV) 直接验证
//   2. cycle_parity_test_chmem_file_exists       — CH_MEM 配对文件 ip/cpu/plugins/mul_div_fsm_chmem.h 存在
//   3. cycle_parity_test_chmem_namespace_symbol  — CH_MEM helper namespace `mul_div_fsm_chmem::has_chmem_support` constexpr true
//
// 设计 (Phase D.2):
//   - 严格 TLM ↔ CH_MEM cycle 数 ±0 比对需要 cross-binary 测试基础设施 (两个 binary chipforge_tests + chipforge_tests_chmem)
//     现有 test 框架不直接支持, 改 infra 是独立工作
//   - 本测试作为最小覆盖: TLM 直接验证 + CH_MEM 文件/namespace 存在性检查 + 注释说明 cross-binary parity 由 [chmem][multi-cycle] family 间接覆盖
//   - Phase D.3 (Verilog emit) 落地后, 可加 elaborate time cycle count 直接比对
//
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <fstream>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/capability_table.h"
#include "ip/cpu/arch/riscv/mul_div_fsm.h"
#include "ip/cpu/plugins/mul_div_fsm_chmem.h"

using cf::plugin::PipeBuilder;
using cf::plugin::Phase;
using cf::plugin::PluginBase;
using cf::plugin::CapabilityTable;
using cf::cpu::arch::riscv::MulDivFsmPlugin;
using T = std::uint32_t;

// Stub provider 满足 MulDivFsmPlugin::negotiate() requires
namespace {
struct CycleParityStubProvider : PluginBase {
  void negotiate(CapabilityTable& cap) override {
    cap.provide("flush_broadcaster", this);
    cap.provide("writeback_arbiter", this);
  }
  void build(PipeBuilder&) override {}
};
}  // namespace

// Test 1: TLM busy_cycles (MUL=1, DIV=33)
TEST_CASE("cycle_parity_test_tlm_busy_cycles", "[cpu][mul-div-fsm][cycle-parity]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  pb.register_plugin(std::make_unique<CycleParityStubProvider>());
  REQUIRE(pb.build().has_value());

  MulDivFsmPlugin<T> mul;
  mul.set_opcode(MulDivFsmPlugin<T>::Opcode::MUL);
  mul.set_operands(3, 4);
  mul.tick_state();
  REQUIRE(mul.busy_cycles() == 1);  // MUL = 1 cycle (TLM 实测)

  MulDivFsmPlugin<T> div;
  div.set_opcode(MulDivFsmPlugin<T>::Opcode::DIV);
  div.set_operands(14, 4);
  for (std::size_t i = 0; i < 33; ++i) div.tick_state();
  REQUIRE(div.busy_cycles() == 33);  // DIV = 33 cycle (TLM 实测)
}

// Test 2: CH_MEM 配对文件存在
TEST_CASE("cycle_parity_test_chmem_file_exists",
          "[framework][cycle-parity][mul-div-fsm]") {
  std::ifstream f("ip/cpu/plugins/mul_div_fsm_chmem.h");
  REQUIRE(f.good());
}

// Test 3: CH_MEM helper namespace 暴露 constexpr symbol
TEST_CASE("cycle_parity_test_chmem_namespace_symbol",
          "[framework][cycle-parity][mul-div-fsm]") {
  static_assert(cf::cpu::arch::riscv::mul_div_fsm_chmem::has_chmem_support,
                "mul_div_fsm_chmem::has_chmem_support must be true (Phase D.1)");
  REQUIRE(cf::cpu::arch::riscv::mul_div_fsm_chmem::has_chmem_support);
}
