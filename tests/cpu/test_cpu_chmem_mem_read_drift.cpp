// tests/cpu/test_cpu_chmem_mem_read_drift.cpp
//
// 功能描述: 钉死 CppHDL ch_mem::aread (async_read) 在 **Simulator** 中相对
//           生成 Verilog 的 `assign` 的 **1-cycle 滞后** —— 这是 CH_MEM 路径
//           10 个 rv32ui load/store ELF 失败的根因.
//
// 作者: build agent
// 日期: 2026-10-20
//
// 背景 (chmem-mext-comb-datapath 验收 P0 归位):
//   上游 CppHDL 契约 `openspec/specs/mem-read-timing/spec.md`:
//     R3 "Simulator classifies mem read ports as sequential"
//         —— sync 与 async 两类读端口都归入 sequential_instr_list_
//     R4 "known simulator-vs-Verilog drift"
//         —— "Simulator 中代理在下一个 posedge 才更新（有效 1-cycle 滞后）"
//         —— 设计意图: "由下游 register 提供 1-cycle 依赖，如 ChipForge CPU
//            reg_file <<="，且 "SHALL NOT 触发任何回归 fail"
//   ⇒ 上游行为是 **by-contract**，不是 bug。**违约方是 ChipForge 的 CH_MEM dmem**:
//     CH_MEM 数据通路全组合 (cpu_factory_chmem.h:431-435 无 stage 寄存器),
//     regfile 在同一次 eval_sequential() 里锁存, 且按 node id 顺序 **先于**
//     读端口更新 ⇒ regfile 捕获到的是 **上一拍** 的读值 ⇒
//     "load 的结果 = 上一个访存地址的内容" ⇒ 所有含 load 的测试全错.
//
// 本文件把该漂移 **钉死为可执行断言**:
//   - 上游若把 async 读端口改回 combinational ⇒ 本测试会 FAIL ⇒ 提醒
//     ChipForge 侧同步更新 dmem 适配 (并重估 feature 归位)
//   - 上游维持现状 ⇒ 本测试 PASS, 作为该 known issue 的回归锚
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <core/reg.h>
#include <core/uint.h>
#include <simulator.h>

namespace {

using ch::core::ch_literal;
using ch::core::ch_uint;

TEST_CASE("chmem_mem_read_drift_aread_lags_one_cycle",
          "[cpu][chmem][mem-drift]") {
  ch::core::context ctx("mem_drift_ctx");
  ch::core::ctx_swap guard(&ctx);

  // memory[i] = i  (ch_mem 的 init 容器元素必须是整型)
  std::vector<std::uint32_t> init(16);
  for (std::size_t i = 0; i < init.size(); ++i) {
    init[i] = static_cast<std::uint32_t>(i);
  }
  ch::core::ch_mem<ch_uint<32>, 16> mem(init, "m");

  // 地址寄存器: 每拍 +1 (模拟 register-driven read address)
  ch::core::ch_reg<ch_uint<4>> areg(ch_literal<0, 4>{}, "areg");
  areg <<= areg + ch_uint<4>(ch_literal<1, 4>{});
  ch_uint<4> addr = areg;

  auto rp = mem.aread(addr, "rd");
  ch_uint<32> rdata(rp.impl());

  ch::Simulator sim(&ctx);
  constexpr int kTicks = 6;
  int mismatches = 0;
  for (int t = 0; t < kTicks; ++t) {
    sim.tick();
    const std::uint64_t obs = static_cast<std::uint64_t>(sim.get_value(rdata));
    const std::uint64_t aregv =
        static_cast<std::uint64_t>(sim.get_value(ch_uint<4>(areg.impl())));
    INFO("tick " << t << ": 当前地址=" << aregv
                 << "  观测读值=" << obs
                 << "  Verilog 语义期望=" << aregv);
    if (obs != aregv) ++mismatches;
  }

  // 已知漂移: **每一拍** 的观测读值都等于 **上一拍地址** 的内容 (含第 0 拍:
  // 读端口在 tick 0 的 posedge 读到的是地址 0, 而此时 areg 已被递增到 1)
  // ⇒ 6/6 全部与 Verilog 语义不符.
  REQUIRE(mismatches == kTicks);

  SUCCEED("aread 1-cycle 滞后已钉死 (upstream mem-read-timing R3/R4 by-contract); "
          "CH_MEM dmem 需据此适配");
}

}  // namespace

#endif  // CF_PLUGIN_USE_CH_MEM
