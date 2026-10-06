// tests/cpu/test_mmu_exception_handler.cpp
//
// MmuExceptionHandlerPlugin TLM 单元测试 (cpu-pipeline-mmufault-handler v1 Phase 3)
//
// 4 TEST_CASE:
//   (a) EXCEPTION_CODE=12 page fault → mmu_exception_pending_ = true
//   (b) EXCEPTION_CODE=0 → mmu_exception_pending_ 保持 false (no flush)
//   (c) reset() 复位 pending (trap 入口完成后)
//   (d) ABI smoke (reset 后可重复使用)
//
// Family tag: [cpu][mmu-exception-handler]
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/cpu/plugins/mmu_exception_handler.h"
#include "ip/cpu/tlm/cpu_keys.h"

namespace {

constexpr std::uint8_t kExcPageFault = 12;

void write_exception_code(cf::plugin::PipeBuilder& pb, std::uint8_t code) {
  auto* mem_node = pb.node_of_logic_stage("memory").get();
  REQUIRE(mem_node != nullptr);
  mem_node->operator()(cf::cpu::tlm::payload::cpu_keys<std::uint32_t>::CPU_EXCEPTION_CODE) = code;
}

}  // namespace

TEST_CASE("mmu_exception_handler_page_fault_sets_pending",
          "[cpu][mmu-exception-handler]") {
  auto pb = std::make_unique<cf::plugin::PipeBuilder>();
  auto plugin = std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<std::uint32_t>>();
  auto* raw = plugin.get();
  plugin->setup(*pb);
  plugin->build(*pb);
  REQUIRE_NOTHROW(pb->register_plugin(std::move(plugin)));
  REQUIRE(pb->build());

  write_exception_code(*pb, 0);
  pb->run();
  REQUIRE_FALSE(raw->mmu_exception_pending());

  write_exception_code(*pb, kExcPageFault);
  pb->run();
  REQUIRE(raw->mmu_exception_pending());
}

TEST_CASE("mmu_exception_handler_no_fault_no_pending",
          "[cpu][mmu-exception-handler]") {
  auto pb = std::make_unique<cf::plugin::PipeBuilder>();
  auto plugin = std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<std::uint32_t>>();
  auto* raw = plugin.get();
  plugin->setup(*pb);
  plugin->build(*pb);
  REQUIRE_NOTHROW(pb->register_plugin(std::move(plugin)));
  REQUIRE(pb->build());

  for (int i = 0; i < 5; ++i) {
    write_exception_code(*pb, 0);
    pb->run();
    REQUIRE_FALSE(raw->mmu_exception_pending());
  }
}

TEST_CASE("mmu_exception_handler_reset_clears_pending",
          "[cpu][mmu-exception-handler]") {
  auto pb = std::make_unique<cf::plugin::PipeBuilder>();
  auto plugin = std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<std::uint32_t>>();
  auto* raw = plugin.get();
  plugin->setup(*pb);
  plugin->build(*pb);
  REQUIRE_NOTHROW(pb->register_plugin(std::move(plugin)));
  REQUIRE(pb->build());

  write_exception_code(*pb, kExcPageFault);
  pb->run();
  REQUIRE(raw->mmu_exception_pending());

  raw->reset();
  REQUIRE_FALSE(raw->mmu_exception_pending());
}

TEST_CASE("mmu_exception_handler_abi_smoke_reusable",
          "[cpu][mmu-exception-handler]") {
  auto pb = std::make_unique<cf::plugin::PipeBuilder>();
  auto plugin = std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<std::uint32_t>>();
  auto* raw = plugin.get();
  plugin->setup(*pb);
  plugin->build(*pb);
  REQUIRE_NOTHROW(pb->register_plugin(std::move(plugin)));
  REQUIRE(pb->build());

  for (int cycle = 0; cycle < 3; ++cycle) {
    write_exception_code(*pb, kExcPageFault);
    pb->run();
    REQUIRE(raw->mmu_exception_pending());
    raw->reset();
    REQUIRE_FALSE(raw->mmu_exception_pending());
    write_exception_code(*pb, 0);
    pb->run();
    REQUIRE_FALSE(raw->mmu_exception_pending());
  }
}
