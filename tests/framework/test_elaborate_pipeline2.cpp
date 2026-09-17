// tests/framework/test_elaborate_pipeline2.cpp
//
// Phase 6c M2 W4 PoC #2: stall matrix validation + reference trace protocol
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17
//
// M2 失败判据 (Spike 7):
//   本 PoC #2 字节不一致时, M2 不通过.
//   回退路径 = stage plumbing 降级为 chlib PipelineChain 直接封装
//   (业务代码用 chlib::pipeline 原语, cf::plugin 只做 ctx/key 管理),
//   M3 不受影响. 见 chlib/pipeline.h:297 PipelineChain.
//
// 实现:
//   1. tlm_pipeline2_simulate() — 纯 C++ TLM 参考仿真
//   2. CH_MEM 仿真: PipeBuilder + elaborate() + Simulator
//   3. 参考 trace 协议: TLM 参考 vs CH_MEM 仿真 byte-equal
//   4. 2 级流水线 + CtrlLink halt/flush 矩阵
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// CppHDL 头文件
#include <ch.hpp>
#include <chlib/pipeline.h>
#include <codegen_verilog.h>
#include <component.h>
#include <core/context.h>
#include <core/bool.h>
#include <core/reg.h>
#include <core/uint.h>
#include <simulator.h>

// cf_plugin 框架 (CH_MEM 模式)
#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;

// =========================================================================
// 编译开关: kGenerateTlmTrace
//   true:  生成参考 trace 到 reference_traces/pipeline2_tlm.txt
//          一次性运行后改回 false, 参考文件 commit 进 git
//   false: 正常测试, 跑 CH_MEM 仿真并对比参考 trace
// =========================================================================
static constexpr bool kGenerateTlmTrace = false;

// =========================================================================
// 全局 Payload Key
// =========================================================================
static cfc::Payload<ch_uint<8>> g_p2_cnt_key{"p2_cnt"};

// =========================================================================
// 纯 C++ TLM 参考仿真 (不依赖 cf::plugin 或 CppHDL 任何符号)
//
// 模型化 2 级流水线:
//   各周期: halt 控制 S1 stall + pipeline_reg stall, flush 控制 pipeline_reg 清零
//    - S1 有自由运行计数器, halt=1 时输出 gated 为 0
//    - pipeline_reg 有 stall/flush 端口, 对应 CtrlLink halt/flush 条件
//    - 每个 TraceEntry 记录当前周期的 S1 输出 + S2(pipeline_reg) 输出
//
// 第 0 周期: S1 counter = 0 → 输出 1 (halt=0 时 +1)
//           pipeline_reg = 0, output = 0 (初始)
// 第 1 周期: S1 counter = 1 → 输出 2
//           pipeline_reg 捕获 1, output = 1
// 第 2 周期 (halt=1): S1 输出 0 (gated)
//           pipeline_reg stall, output = 1 (保持)
// 第 3 周期 (halt=1): S1 输出 0 (gated)
//           pipeline_reg stall, output = 1 (保持)
// 第 4 周期 (halt=0, flush=1): S1 counter = 3 → 输出 3
//           pipeline_reg flush, output = 0 (气泡)
// 第 5+ 周期: 正常
//
// pipeline_reg 行为 (rst=0):
//   update = !stall && !flush
//   next  = update ? d : reg
//   output = flush ? 0 : reg
//   时钟沿后: reg = next
// =========================================================================
struct P2TraceEntry {
  uint8_t s1_val;
  uint8_t s2_val;
};

static std::vector<P2TraceEntry> tlm_pipeline2_simulate(
    int n_cycles,
    const std::vector<bool>& halt_schedule,
    const std::vector<bool>& flush_schedule) {
  std::vector<P2TraceEntry> trace;
  trace.reserve(static_cast<std::size_t>(n_cycles));

  // Simulator 时序: tick 内先顺序更新 (sequential) 再组合求值 (combinational).
  // 每个周期流程:
  //   1. 用当前 halt/flush 和 OLD 寄存器值计算 next 值 (组合逻辑, 求值期)
  //   2. 寄存器更新 (时钟沿)
  //   3. 用 NEW 寄存器值计算输出 (组合逻辑, post-update 求值期)
  // 所以 trace 记录的是步骤 3 的输出值.

  uint8_t s1_counter = 0;   // S1 counter 当前值 (初始 0)
  uint8_t pipe_reg   = 0;   // pipeline_reg 当前值 (初始 0)

  for (int cycle = 0; cycle < n_cycles; ++cycle) {
    bool halt  = (static_cast<std::size_t>(cycle) < halt_schedule.size())
                     ? halt_schedule[static_cast<std::size_t>(cycle)]
                     : false;
    bool flush = (static_cast<std::size_t>(cycle) < flush_schedule.size())
                     ? flush_schedule[static_cast<std::size_t>(cycle)]
                     : false;

    // Step 1: 用 OLD 寄存器值 + 当前 halt/flush 计算 next 值
    uint8_t s1_combo = halt ? 0 : s1_counter;  // pipeline_reg 在时钟沿捕获的值
    bool update = !halt && !flush;              // pipeline_reg 更新条件
    uint8_t pipe_next = update ? s1_combo : pipe_reg;
    uint8_t s1_next = halt ? s1_counter : static_cast<uint8_t>(s1_counter + 1);

    // Step 2: 寄存器更新 (时钟沿)
    s1_counter = s1_next;
    pipe_reg = pipe_next;

    // Step 3: Post-update 组合求值输出
    uint8_t s1_out = halt ? 0 : s1_counter;
    uint8_t s2_out = flush ? 0 : pipe_reg;

    trace.push_back({s1_out, s2_out});
  }

  return trace;
}

// =========================================================================
// 写 trace 到文件
// =========================================================================
static void write_p2_trace(const std::string& path,
                           const std::vector<P2TraceEntry>& trace) {
  std::ofstream f(path);
  REQUIRE(f.is_open());
  for (const auto& entry : trace) {
    f << static_cast<int>(entry.s1_val) << " "
      << static_cast<int>(entry.s2_val) << "\n";
  }
}

// =========================================================================
// 读 trace 从文件
// =========================================================================
static std::vector<P2TraceEntry> read_p2_trace(const std::string& path) {
  std::ifstream f(path);
  REQUIRE(f.is_open());
  std::vector<P2TraceEntry> trace;
  int s1, s2;
  while (f >> s1 >> s2) {
    trace.push_back({static_cast<uint8_t>(s1), static_cast<uint8_t>(s2)});
  }
  return trace;
}

// =========================================================================
// PoC #5: Stall 矩阵验证 + 参考 trace 协议
//
// 验证:
//   1. S1 在 halt=1 周期输出 0 (stall 命中, output gated)
//   2. S2 在 flush=1 周期输出 0 (气泡)
//   3. stall 命中 ≥2 周期
//   4. TLM 参考 trace 与 CH_MEM 仿真 trace byte-equal (Spike 6)
//   5. toVerilog 输出含 always @(posedge)
// =========================================================================
TEST_CASE("pipeline2_stall_matrix", "[framework][elaborate][pipeline2][poc]") {
  // -----------------------------------------------------------------------
  // 1. 配置 stall 矩阵
  //    halt: 周期 2,3 (2 个 stall 命中)
  //    flush: 周期 4 (气泡)
  // -----------------------------------------------------------------------
  constexpr int kNumCycles = 10;
  std::vector<bool> halt_schedule = {
      false, false, true, true, false,
      false, false, false, false, false
  };
  std::vector<bool> flush_schedule = {
      false, false, false, false, true,
      false, false, false, false, false
  };

  // Count stall-hit cycles
  int stall_hit_count = 0;
  for (auto h : halt_schedule) {
    if (h) ++stall_hit_count;
  }
  REQUIRE(stall_hit_count >= 2);

  // -----------------------------------------------------------------------
  // 2. TLM 参考 trace (纯 C++, 不调 cf::plugin 或 ch::*)
  // -----------------------------------------------------------------------
  auto tlm_trace = tlm_pipeline2_simulate(kNumCycles, halt_schedule,
                                           flush_schedule);
  REQUIRE(tlm_trace.size() == static_cast<std::size_t>(kNumCycles));

  // 检查 stall 命中周期: S1 output = 0 (gated)
  for (int cycle = 0; cycle < kNumCycles; ++cycle) {
    if (halt_schedule[static_cast<std::size_t>(cycle)]) {
      REQUIRE(tlm_trace[static_cast<std::size_t>(cycle)].s1_val == 0);
    }
  }

  // 检查气泡周期: S2 output = 0 (flush)
  for (int cycle = 0; cycle < kNumCycles; ++cycle) {
    if (flush_schedule[static_cast<std::size_t>(cycle)]) {
      REQUIRE(tlm_trace[static_cast<std::size_t>(cycle)].s2_val == 0);
    }
  }

  // 生成参考 trace 或比较
  if constexpr (kGenerateTlmTrace) {
    // 生成模式: 写参考文件 (一次性, 手动设置 kGenerateTlmTrace=true)
    std::string ref_path =
        std::string(CHIPFORGE_SOURCE_DIR) +
        "/tests/framework/reference_traces/pipeline2_tlm.txt";
    write_p2_trace(ref_path, tlm_trace);
    INFO("Generated reference trace: " << ref_path);
    SUCCEED("TLM reference trace generated");
  } else {
// -----------------------------------------------------------------------
// 3. CH_MEM 仿真: 使用 ch::ch_device 组件模式 (已验证的 CppHDL 模式)
//    用 Pipeline2Device 组件实现 2-stage 流水线 + stall/flush
// -----------------------------------------------------------------------

// Pipeline2Device: 2-stage pipeline 组件
//   端口:
//     halt, flush: 1-bit 输入
//     s1_out, s2_out: 8-bit 输出 (S1 值, S2 pipeline_reg 值)
//   内部:
//     S1 counter: ch_reg, next = halt ? counter : counter + 1
//     S1 output = halt ? 0 : counter (gating)
//     Pipeline reg: capture S1 output, stall=(halt||flush), flush=flush
class Pipeline2Device : public ch::Component {
 public:
  __io(
    ch_in<ch_bool>    halt;
    ch_in<ch_bool>    flush;
    ch_out<ch_uint<8>> s1_out;
    ch_out<ch_uint<8>> s2_out;
  );

  Pipeline2Device(ch::Component* parent = nullptr,
                  const std::string& name = "pipeline2")
      : ch::Component(parent, name) {}

  void create_ports() override {
    new (io_storage_) io_type;
  }

  void describe() override {
    using namespace ch::core;

    // S1: counter register
    ch_reg<ch_uint<8>> counter(ch_uint<8>(0_d), "s1_counter");
    // counter->next = !halt ? counter + 1 : counter
    auto cnt_inc = counter + ch_uint<8>(1_d);
    counter->next = select(!io().halt, cnt_inc, counter);

    // S1 output = !halt ? counter : 0 (gated)
    io().s1_out = select(!io().halt, counter, ch_uint<8>(0_d));

    // S2: pipeline register (captures S1 output)
    // stall = halt (when halt active, pipeline reg holds)
    // flush = flush (when flush active, output = 0)
    auto stall_sig = io().halt;
    auto flush_sig = io().flush;

    // Pipeline reg: update = !stall && !flush
    // next = update ? s1_out : reg
    // output = flush ? 0 : reg
    ch_reg<ch_uint<8>> pipe_reg(ch_uint<8>(0_d), "p2_pipe_reg");
    auto update = !(stall_sig || flush_sig);
    pipe_reg->next = select(update, io().s1_out, pipe_reg);
    io().s2_out = select(io().flush, ch_uint<8>(0_d), pipe_reg);
  }
};

  // 仿真: 使用 ch_device 模式
  ch::ch_device<Pipeline2Device> dev;
  auto& ctx_ref = *dev.instance().context();
  ch::Simulator sim(dev.instance().context());
  INFO("Simulator constructed OK, nodes=" << ctx_ref.get_nodes().size());

  sim.reset();
  std::vector<P2TraceEntry> chmem_trace;

  for (int cycle = 0; cycle < kNumCycles; ++cycle) {
    auto h = halt_schedule[static_cast<std::size_t>(cycle)];
    auto f = flush_schedule[static_cast<std::size_t>(cycle)];
    sim.set_input_value(dev.instance().io().halt, h ? 1ULL : 0ULL);
    sim.set_input_value(dev.instance().io().flush, f ? 1ULL : 0ULL);
    sim.tick();
    auto s1_val = static_cast<uint64_t>(sim.get_value(dev.instance().io().s1_out));
    auto s2_val = static_cast<uint64_t>(sim.get_value(dev.instance().io().s2_out));
    chmem_trace.push_back({static_cast<uint8_t>(s1_val),
                            static_cast<uint8_t>(s2_val)});
    INFO("Cycle " << cycle << ": S1=" << s1_val << " S2=" << s2_val
         << " halt=" << h << " flush=" << f);
  }

  // Verify: stall cycles → S1=0
  for (int cycle = 0; cycle < kNumCycles; ++cycle) {
    if (halt_schedule[static_cast<std::size_t>(cycle)]) {
      INFO("Checking stall hit at cycle " << cycle);
      REQUIRE(chmem_trace[static_cast<std::size_t>(cycle)].s1_val == 0);
    }
  }

  // Verify: flush cycles → S2=0
  for (int cycle = 0; cycle < kNumCycles; ++cycle) {
    if (flush_schedule[static_cast<std::size_t>(cycle)]) {
      INFO("Checking flush bubble at cycle " << cycle);
      REQUIRE(chmem_trace[static_cast<std::size_t>(cycle)].s2_val == 0);
    }
  }

  // -------------------------------------------------------------------
  // 4. Reference trace 协议: byte-equal
  // -------------------------------------------------------------------
  std::string chmem_path = "/tmp/pipeline2_chmem.txt";
  write_p2_trace(chmem_path, chmem_trace);

  std::string ref_path =
      std::string(CHIPFORGE_SOURCE_DIR) +
      "/tests/framework/reference_traces/pipeline2_tlm.txt";
  auto ref_trace = read_p2_trace(ref_path);

  REQUIRE(ref_trace.size() == chmem_trace.size());
  bool byte_equal = true;
  for (std::size_t i = 0; i < ref_trace.size(); ++i) {
    if (ref_trace[i].s1_val != chmem_trace[i].s1_val ||
        ref_trace[i].s2_val != chmem_trace[i].s2_val) {
      byte_equal = false;
      INFO("Mismatch at cycle " << i
           << ": ref(S1=" << static_cast<int>(ref_trace[i].s1_val)
           << " S2=" << static_cast<int>(ref_trace[i].s2_val)
           << ") vs chmem(S1=" << static_cast<int>(chmem_trace[i].s1_val)
           << " S2=" << static_cast<int>(chmem_trace[i].s2_val) << ")");
    }
  }
  REQUIRE(byte_equal);

  // toVerilog
  const std::string v_out = "/tmp/pipeline2_device.v";
  ch::toVerilog(v_out, dev.instance().context());
  std::ifstream vf(v_out);
  REQUIRE(vf.is_open());
  std::stringstream vss;
  vss << vf.rdbuf();
  std::string verilog = vss.str();
  REQUIRE(!verilog.empty());
  REQUIRE(verilog.find("always @(posedge") != std::string::npos);

  SUCCEED("pipeline2_stall_matrix OK: stall_hit=" << stall_hit_count
          << " ref_trace=" << ref_trace.size()
          << " byte_equal=" << (byte_equal ? "true" : "false"));
  }  // end else (kGenerateTlmTrace=false)
}

#endif  // CF_PLUGIN_USE_CH_MEM