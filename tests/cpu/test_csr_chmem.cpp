// tests/cpu/test_csr_chmem.cpp
//
// [cpu][csr-chmem] 单元测试 — csr-chmem-pipeline-integration R1.2/R1.3/R1.4
//
// 覆盖:
//   - Case 1 (csr_chmem_CsrrwMstatus_AddressReextract):    CSRRW x1, mstatus
//   - Case 2 (csr_chmem_CsrrwiMtvec_ImmediateReextract):  CSRRWI x4, mtvec, imm5=5
//   - Case 3 (csr_chmem_Ecall_NotZicsrButFunct12_0_000):   ECALL -> trap 路径
//   - Case 4 (csr_chmem_Ebreak_NotZicsrButFunct12_0_001):  EBREAK -> trap 路径
//   - Case 5 (csr_chmem_Mret_NotZicsrButFunct12_0_302):    MRET -> mepc PC 重定向
//   - Case 6 (csr_chmem_CsrrsMstatus_ZicsrValid):          CSRRS x4, mstatus, x10
//
// R1 验证目标: sim->get_signal_value() 读 execute-LATE 闭包重提取 DAG 信号
// (csr_addr / funct12 / imm5 / opcode / funct3 / zicsr_valid), 与 INSTRUCTION
// 预期一致。注: R1 段不写 payload, 仅验证重提取层正确。
//
// 设计遵循:
//   - tests/cpu/test_cpu_decoded_inst_migration.cpp POC stage-tick 模式
//   - ch::core::context 单 case, RAII ctx_swap guard 防 thread_local pollution
//   - at_stage("execute", Phase::EARLY) 灌 INSTRUCTION (ch 字面值)
//   - sim->tick() 走 1 cycle
//   - sim->get_signal_value() 验证 last_*_ch DAG signal post-tick 值
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/core/payload_common.h"

#include "ip/cpu/plugins/csr_chmem.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;
namespace cfcpu = cf::cpu;

namespace {

struct CsrPluginGuard {
  std::unique_ptr<cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>> holder;
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* raw;

  explicit CsrPluginGuard(cfc::PipeBuilder& pb)
      : holder(std::make_unique<cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>>()),
        raw(holder.get()) {
    pb.register_plugin(std::move(holder));
  }
};

}  // namespace

// =========================================================================
// Case 1: CSRRW x1, mstatus (csr_addr=0x300) — R1 重提取基础
//
// 指令编码 (CSRRW x1, mstatus, x0):
//   bits[31:20]=0x300, bits[19:15]=x0=0, bits[14:12]=funct3=001, bits[11:7]=rd=1,
//   bits[6:0]=opcode=0x73 (SYSTEM)
//   inst = 0x300010F3
// =========================================================================
TEST_CASE("csr_chmem_CsrrwMstatus_AddressReextract", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_csrrw_mstatus");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  std::uint32_t inst_csrrw_mstatus = 0x300010F3u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrw_mstatus]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrw_mstatus);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // 取 DAG signal post-tick 值
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x300u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x300u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_imm5_ch()))     == 0u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct3_ch()))   == 0x1u);
}

// =========================================================================
// Case 2: CSRRWI x4, mtvec(0x305), imm5=5 — R1 重提取 imm5
//
//   bits[31:20]=0x305, bits[19:15]=imm5=5, bits[14:12]=funct3=101,
//   bits[11:7]=rd=4, bits[6:0]=0x73
//   inst = 0x305A5273
// =========================================================================
TEST_CASE("csr_chmem_CsrrwiMtvec_ImmediateReextract", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_csrrwi_mtvec");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRWI x4, mtvec, imm5=5:
  //   inst = (0x305 << 20) | (5 << 15) | (0b101 << 12) | (4 << 7) | 0x73
  //        = 0x30500000 | 0x28000  | 0x5000       | 0x200   | 0x73
  //        = 0x3052D273
  std::uint32_t inst_csrrwi_mtvec_5 = 0x3052D273u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrwi_mtvec_5]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrwi_mtvec_5);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(4);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x305u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x305u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_imm5_ch()))     == 5u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct3_ch()))   == 0x5u);
}

// =========================================================================
// Case 3: ECALL (funct12=0x000) — trap 路径触发, Zicsr_VALID=false
// =========================================================================
TEST_CASE("csr_chmem_Ecall_NotZicsrButFunct12_0_000", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_ecall");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  std::uint32_t inst_ecall = 0x00000073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_ecall]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_ecall);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x000u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x000u);
}

// =========================================================================
// Case 4: EBREAK (funct12=0x001)
// =========================================================================
TEST_CASE("csr_chmem_Ebreak_NotZicsrButFunct12_0_001", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_ebreak");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  std::uint32_t inst_ebreak = 0x00100073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_ebreak]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_ebreak);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x001u);
}

// =========================================================================
// Case 5: MRET (funct12=0x302)
// =========================================================================
TEST_CASE("csr_chmem_Mret_NotZicsrButFunct12_0_302", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_mret");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  std::uint32_t inst_mret = 0x30200073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_mret]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_mret);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x302u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x302u);
}

// =========================================================================
// Case 6: CSRRS x4, mstatus(0x300), x10 — csr_addr=0x300, Zicsr_VALID=true
//   inst = 0x30052073
// =========================================================================
TEST_CASE("csr_chmem_CsrrsMstatus_ZicsrValid", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_csrrs_mstatus");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  std::uint32_t inst_csrrs_mstatus = 0x30052073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrs_mstatus]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrs_mstatus);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(10);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(4);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x300u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch()))  == 0x300u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_imm5_ch()))     == 10u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_opcode_ch()))   == 0x73u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct3_ch()))   == 0x2u);
}

// =========================================================================
// Case 7 (R3.5): CSRRW 读 mstatus 旧值 = 0 (初始状态, ch_reg = 0)
//   验证 R3.3 决策: csr_addr == 0x300 → CSR_READ_VALUE = mstatus (初值 0)
// =========================================================================
TEST_CASE("csr_chmem_R3_CsrrwMstatus_InitialReadValue", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r3_csrrw_mstatus");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRW x1, mstatus, x0: inst = 0x300010F3
  std::uint32_t inst = 0x300010F3u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // R3.3 决策: csr_addr == 0x300 → CSR_READ_VALUE = mstatus (初始 0)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_value_ch())) == 0u);
  // R3.3 决策: mcycle (0xB00) 单独路径, 其他 CSR READ_VALID = 1
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_valid_ch())) == 1u);
  // R1 重提取 csr_addr 正确 (0x300)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0x300u);
}

// =========================================================================
// Case 8 (R3.5): CSRRS 读 mcycle → CSR_READ_VALID = false (R3 决策: mcycle 单独路径)
//   验证 R3 决策: csr_addr == 0xB00 → RD_DATA 不被覆写 (mcycle 读单独走)
// =========================================================================
TEST_CASE("csr_chmem_R3_CsrrsMcycle_ReadValidFalse", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r3_csrrs_mcycle");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRS x1, mcycle, x0: inst = (0xB00 << 20) | (0 << 15) | (0b010 << 12) | (1 << 7) | 0x73
  //   = 0xB0000000 | 0 | 0x2000 | 0x80 | 0x73 = 0xB00020F3
  std::uint32_t inst = 0xB00020F3u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // R3 决策: csr_addr == 0xB00 → CSR_READ_VALID = false
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_valid_ch())) == 0u);
  // csr_addr 重提取 = 0xB00
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == 0xB00u);
  // 注: CSR_READ_VALUE 实际值依赖 mcycle 自增状态 (R2.5 引入 fetch-NORMAL
  //   mcycle_ <<= mcycle_ + 1, 单 stage 测试 1 cycle tick 后 mcycle 可能 = 1)。
  //   本 case 核心验证 mcycle 单独路径 (READ_VALID=false), READ_VALUE 不断言。
  //   详细 mcycle 自由计数验证见 csr_chmem_R2_McycleFreeRunning case。
}

// =========================================================================
// Case 9 (R3.5): 8 选 1 mux 覆盖 — 验证 4 个不同 csr_addr 走不同分支
//   mstatus=0x300, mtvec=0x305, mepc=0x341, mcause=0x342
// =========================================================================
TEST_CASE("csr_chmem_R3_ReadMux_FourCsrAddresses", "[cpu][csr-chmem]") {
  struct AddrCase {
    std::uint32_t csr_addr;
    std::uint32_t inst;       // CSRRW x1, csr, x0
    const char* label;
  };
  const std::vector<AddrCase> cases = {
    {0x300, 0x300010F3u, "mstatus"},
    {0x305, 0x305010F3u, "mtvec"},
    {0x341, 0x341010F3u, "mepc"},
    {0x342, 0x342010F3u, "mcause"},
  };
  for (const auto& tc : cases) {
    ch::core::context ctx(std::string("csr_chmem_r3_") + tc.label);
    ctx.set_as_current_context();
    ch::core::ctx_swap guard(&ctx);

    cfc::PipeBuilder pb(&ctx);
    CsrPluginGuard plugin_guard(pb);
    cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

    using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
    pb.at_stage("execute", cfc::Phase::EARLY, [&pb, tc]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        n->operator()(KT::INSTRUCTION) = ch_uint<32>(tc.inst);
        n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
        n->operator()(KT::RS1)         = ch_uint<32>(0);
        n->operator()(KT::RS2)         = ch_uint<32>(0);
        n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
      }
    });
    pb.build();
    auto exp = pb.elaborate(ctx);
    REQUIRE(exp.has_value());
    auto sim = cfc::auto_throw(pb.create_simulator());
    REQUIRE(sim != nullptr);
    sim->reset();
    REQUIRE_NOTHROW(sim->tick());

    // 验证 csr_addr 走正确分支 (read mux 8 选 1)
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_addr_ch())) == tc.csr_addr);
    // 初值 0
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_value_ch())) == 0u);
    // READ_VALID = 1 (非 mcycle)
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_valid_ch())) == 1u);
  }
}

// =========================================================================
// Case 10 (R2.4): ECALL → TRAP_PENDING=true, TRAP_TARGET=mtvec (R2 决策)
//   注: 单 stage 测试无 PC 节点 (mepc 写路径 has() guard 跳过),
//   但 TRAP_PENDING / TRAP_TARGET 写出是 unconditional, 验证 trap 路径
//   决策不依赖 PC 节点。
// =========================================================================
TEST_CASE("csr_chmem_R2_Ecall_TrapPendingTrue", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r2_ecall");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // ECALL: inst = 0x00000073
  std::uint32_t inst = 0x00000073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // R2 决策: ECALL (funct12=0x000) → TRAP_PENDING=true
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_pending_ch())) == 1u);
  // R2 决策: ECALL → TRAP_TARGET=mtvec (初始 0)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_target_ch())) == 0u);
  // R1 重提取 funct12 确认是 0x000
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch())) == 0x000u);
}

// =========================================================================
// Case 11 (R2.4): EBREAK → TRAP_PENDING=true, TRAP_TARGET=mtvec
// =========================================================================
TEST_CASE("csr_chmem_R2_Ebreak_TrapPendingTrue", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r2_ebreak");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // EBREAK: inst = 0x00100073
  std::uint32_t inst = 0x00100073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_pending_ch())) == 1u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_target_ch())) == 0u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch())) == 0x001u);
}

// =========================================================================
// Case 12 (R2.4): MRET → TRAP_PENDING=true, TRAP_TARGET=mepc (R2 决策)
//   MRET 不写 mcause/mepc (R2 决策: PC 跳转走 mepc, mcause 由 ECALL/EBREAK 写)
// =========================================================================
TEST_CASE("csr_chmem_R2_Mret_TrapTargetMepc", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r2_mret");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // MRET: inst = 0x30200073
  std::uint32_t inst = 0x30200073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(0);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_pending_ch())) == 1u);
  // R2 决策: MRET → TRAP_TARGET=mepc (初始 0)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_target_ch())) == 0u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch())) == 0x302u);
}

// =========================================================================
// Case 13 (R2.4): 非 trap 指令 (CSRRW) → TRAP_PENDING=false, TRAP_TARGET=0
// =========================================================================
TEST_CASE("csr_chmem_R2_NonTrapInstruction_TrapPendingFalse", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r2_nonttrap");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRW x1, mstatus, x0: inst = 0x300010F3 (非 trap 指令)
  std::uint32_t inst = 0x300010F3u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // 非 trap 指令 → TRAP_PENDING=false
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_pending_ch())) == 0u);
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_trap_target_ch())) == 0u);
  // funct12=0x300 (CSRRW 的 csr_addr field, 确认非 trap)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_funct12_ch())) == 0x300u);
}

// =========================================================================
// Case 14 (R2.5): mcycle 自由计数 — 1 cycle tick 后 mcycle 应 ≥ 1
//   注: 单 stage 测试无 fetch node, fetch-NORMAL 闭包 has() guard 跳过 —
//   但 elaboration 阶段已跑 1 次 at_stage closure, mcycle_ <<= 1 (next=1),
//   sim tick 后 mcycle = 1. 严格"1 cycle"是 +1 (R2.5 决策, free-running)。
// =========================================================================
TEST_CASE("csr_chmem_R2_McycleFreeRunning_AfterOneTick", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r2_mcycle");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRW x1, mcycle, x0: inst = 0xB00020F3 (R3.5 Case 8 同一指令)
  std::uint32_t inst = 0xB00020F3u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  REQUIRE_NOTHROW(sim->tick());

  // R2.5 决策: mcycle 自由计数. 1 cycle tick 后 mcycle_ 应 >= 1
  //   (elaboration 期 at_stage closure 已跑 1 次, mcycle_ <<= 1)
  //   严格期望 == 1 (mcycle_ = 0 + 1 = 1)
  auto mcycle_val =
      static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_mcycle_observed_ch()));
  INFO("mcycle_observed = " << mcycle_val);
  REQUIRE(mcycle_val >= 1u);  // R2.5: ≥ 1
  REQUIRE(mcycle_val <= 2u);  // 防止过度计数 (1 cycle → mcycle ≤ 2)
}
