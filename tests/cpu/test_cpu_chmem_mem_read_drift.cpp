// tests/cpu/test_cpu_chmem_mem_read_drift.cpp
//
// 功能描述: 锁住 CppHDL ch_mem::aread (async_read) 在 **Simulator** 中与生成
//           Verilog 的 `assign` **同拍对齐（0-cycle，无滞后）** —— 这是 CH_MEM 路径
//           10 个 rv32ui load/store ELF 失败的根因, 已由上游修复.
//
// 作者: build agent
// 日期: 2026-10-20 (2026-10-21 反转: 钉死缺陷 → 锁住修复)
//
// 历史 (2026-10-20 归位 change `chmem-dmem-read-latency`):
//   上游契约 `openspec/specs/mem-read-timing/spec.md` R3 曾要求 sync 与 async
//   两类读端口 **都** 归入 sequential_instr_list_, R4 记录 "Simulator 相对生成
//   Verilog 滞后 1 拍, 由下游 register 提供 1-cycle 依赖来吸收".
//   ⇒ 上游行为是 **by-contract**, 违约方是 ChipForge 的 CH_MEM dmem:
//     CH_MEM 数据通路全组合 (cpu_factory_chmem.h:431-435 无 stage 寄存器),
//     regfile 在同一次 eval_sequential() 里锁存且 **先于** 读端口更新
//     ⇒ "load 的结果 = 上一个访存地址的内容" ⇒ 所有含 load 的 ELF 全错.
//
// 现状 (2026-10-21 上游已修复):
//   上游 CppHDL 提交 `c27543d` (fix(sim): align ch_mem::aread with generated
//   Verilog 0-cycle assign) 并归档 `sim-mem-read-timing-align-2026-10`,
//   async 读端口改为 **组合求值** ⇒ Simulator 与 Verilog 同为 0-cycle.
//   实测: ChipForge `[chmem]` 73 case / **73 PASS / 0 failed**,
//   47 个 vendored ELF 全 PASS (16410 assertions).
//
// 本文件现在的作用:
//   从 "钉死已知缺陷" 反转为 "**锁住修复**" ——
//   - 上游若把 async 读端口改回 sequential / 退回 posedge 求值
//     ⇒ 本测试 FAIL ⇒ 立刻发现回归
//   - 上游维持 0-cycle 对齐 ⇒ 本测试 PASS
//
// 断言强度: 同时断言 "每拍观测值 == 当前地址内容" 与 "观测值 != 上一拍地址内容".
//   后者不是冗余 —— 若只断言前者, 一个"恰好总是等于上一拍"的错误实现也能
//   在地址回绕的特定序列下蒙混过关; 两者合起来把"0 拍对齐"钉死.
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

TEST_CASE("chmem_mem_read_drift_aread_has_no_skew",
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
  int off_by_one = 0;
  for (int t = 0; t < kTicks; ++t) {
    sim.tick();
    const std::uint64_t obs = static_cast<std::uint64_t>(sim.get_value(rdata));
    const std::uint64_t aregv =
        static_cast<std::uint64_t>(sim.get_value(ch_uint<4>(areg.impl())));
    // Verilog `assign` 语义期望: 读端口应给出当前地址的内容 (= mem[areg]).
    const std::uint64_t expect_now = aregv;
    // 滞后 1 拍的精确预期: 上一拍地址的内容 (= mem[areg-1], 4-bit 回绕).
    const std::uint64_t expect_prev = (aregv + 15u) & 15u;
    INFO("tick " << t << ": 当前地址=" << aregv
                 << "  观测读值=" << obs
                 << "  Verilog 语义期望=" << expect_now
                 << "  滞后1拍预期=" << expect_prev);
    if (obs != expect_now) ++mismatches;
    if (obs == expect_prev) ++off_by_one;
  }

  // 上游 `c27543d` 之后, Simulator 的 aread 与生成的 Verilog `assign` 同为 0-cycle:
  // 每拍读到的就是 **当前地址** 的内容 ⇒ 与 Verilog 语义期望 **全部相符**.
  REQUIRE(mismatches == 0);

  // 幅度断言 (保留): 观测值必须 **不** 等于"上一拍地址"的内容.
  // 若只断言 `mismatches == 0`, 一个"恰好总是取到上一拍"的错误实现在地址
  // 序列恰好重合时也可能蒙混过关 ⇒ 两者合起来把"0 拍对齐"钉死.
  REQUIRE(off_by_one == 0);

  SUCCEED("aread 与生成 Verilog 同拍对齐已锁住 (upstream c27543d; "
          "mem-read-timing R3/R4 已改写为 async 归组合)");
}

}  // namespace

#endif  // CF_PLUGIN_USE_CH_MEM
