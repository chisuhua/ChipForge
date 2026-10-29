// tests/cpu/test_cpu_open_core_branch.cpp
//
// 功能描述: ADR-091 §D8 S3.2b —— CH_MEM 开放核的**分支重定向 + flush**
//           （`pc` 由 branch 驱动；错路 store 被抑制）
//
// ★ 本文件是 S3.2b 的契约测试（取指侧见 `test_cpu_open_core_ibus.cpp`，
//   访存侧见 `test_cpu_open_core_dmem.cpp`）。
//
// ---------------------------------------------------------------------------
// 验证 4 件事：
//
//   ① **顺序取指**：`BRANCH_TAKEN = 0` ⇒ `pc` 每拍 +4
//
//   ② **分支重定向**：`BRANCH_TAKEN = 1` + `BRANCH_TARGET = X`
//      ⇒ ★ 下一拍 `pc = X`（★ 与预载核 `branch LATE` 的 pc mux **同构**）
//
//   ③ ★ **`FLUSH` 的 1 拍对齐**（★ 最容易搞错的一条）：
//      ★ 拍 N `taken = 1` ⇒ ★ 拍 N 取到的是**顺序地址 A+4**（错路，已在飞）
//      ⇒ ★ `FLUSH` MUST 在 **拍 N+1** 有效（★ 生产者打 1 拍）
//
//   ④ ★ **flush 抑制 store**：`FLUSH = 1` + store 指令
//      ⇒ ★ `dmem_req_we` MUST 为 0（★ 错路 store **不得**写内存）
//
// ---------------------------------------------------------------------------
// ★ 逐拍驱动的范式（★ 外部从设备的义务）：
//
//     payload 是 **elaboration 期的组合节点** ⇒ ★ 无法逐拍变化
//     ⇒ ★ 要逐拍控制 `taken` / `target`，MUST 经**输入端口**注入
//     ⇒ ★ 本测试用 `branch_taken_in` / `branch_target_in` 两个 `ch_in`
//       （★ 由「branch 提供者」在 `branch` stage 的 `EARLY` 转写进 payload）
//
// ★ 归属: ADR-091（CH_MEM 开放核形态与端口架构）§D8 S3.2b / §D3a.5
// 关联: ADR-040 v3.0 §10.6（跨 stage 顺序契约）· ADR-089（parity 门禁）

#include "catch_amalgamated.hpp"

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— module.h 缺 inline ⇒ 多 TU 链接失败）
#include <codegen_verilog.h>
#include <core/bool.h>
#include <core/context.h>
#include <core/reg.h>
#include <core/traits.h>
#include <core/uint.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/branch_iface_chmem.h"
#include "tests/cpu/open_core_verilog_helper_chmem.h"
#include "ip/cpu/core/dmem_port_chmem.h"
#include "ip/cpu/core/ibus_port_chmem.h"

namespace {

using U1 = ch::core::ch_uint<1>;
using U3 = ch::core::ch_uint<3>;
using U32 = ch::core::ch_uint<32>;
using U64 = ch::core::ch_uint<64>;

/// ★ 解包 `Result<Port>`（★ ADR-047：静态配置头禁 throw ⇒ 端口声明走 `Result`）
/// ★★ **MUST 按值返回**（★ 不可返回引用 —— `Result<P>` 是临时对象 ⇒ 悬垂
///   ⇒ 端口 `impl_` 失效 ⇒ 端口「消失」⇒ 假绿）
template <typename P>
P must(cf::plugin::Result<P> r) {
  REQUIRE(r.has_value());
  return *r;
}

/// ★ 统一的端口取值（★ 避免为每种宽度/方向写重载）
template <typename T, typename Dir>
std::uint64_t pv(const ch::Simulator& sim, const ch::core::port<T, Dir>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}

/// ★ 单场景的逐拍观测轨迹
///
/// ★★★ **读取口径（★ 极易搞错 —— 本轮实测踩过）**：
///   ★ `sim.tick()` **之后**读端口 ⇒ ★ 读到的是**下一拍**的值
///   （★ `ch_reg` 的 next 在 tick 的 sequential 阶段已应用）
///   ⇒ ★ 即 `tr.pc[i]` = `pc_reg(i+1)` · `tr.flush[i]` = `FLUSH(i+1)`
///   ⇒ ★ 因此「`taken(N)=1` ⇒ 拍 N+1 的 pc = target」在轨迹上表现为
///     `tr.pc[N] == target`（★ 不是 `tr.pc[N+1]`）
struct BranchTrace {
  std::vector<std::uint64_t> pc;
  std::vector<std::uint64_t> flush;
  std::vector<std::uint64_t> req_we;
  std::vector<std::uint64_t> req_addr;
};

/// ★ 提供者 plugin（★ 必须注册在 ibus/dmem **之后** —— 见 `run_branch` 内注释）
class BranchMemProvider : public cf::plugin::PluginBase {
 public:
  using InU1 = ch::core::port<ch::core::ch_uint<1>, ch::core::input_direction>;
  using InU32 = ch::core::port<ch::core::ch_uint<32>, ch::core::input_direction>;

  BranchMemProvider(InU1* taken, InU32* target, std::uint32_t insn)
      : taken_(taken), target_(target), insn_(insn) {}

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ branch EARLY：把输入端口转写进 branch 契约 payload
    //   （★ EARLY 桶早于 ibus 的 `branch LATE` ⇒ 同拍可见）
    pb.at_stage("branch", cf::plugin::Phase::EARLY, [this, &pb] {
      auto* n = pb.node_of_logic_stage("branch").get();
      if (!n) return;
      namespace BK = cf::cpu::core::branch_keys;
      n->operator()(BK::BRANCH_TAKEN) = ch::core::ch_bool(
          ch::core::operator==(U1(*taken_),
                               U1(ch::core::ch_literal<1, 1>{})));
      n->operator()(BK::BRANCH_TARGET) = U32(*target_);
    });
    // ★ memory EARLY：模拟上游 EX 阶段（★ G-1l：`RD_DATA` MUST 先写）
    pb.at_stage("memory", cf::plugin::Phase::EARLY, [this, &pb] {
      using KT = cf::cpu::core::payload::keys<U32, 32>;
      auto* n = pb.node_of_logic_stage("memory").get();
      if (!n) return;
      n->operator()(KT::INSTRUCTION) = U32(insn_);
      n->operator()(KT::RESULT) = U32(0x80000000u);
      n->operator()(KT::RS2) = U32(0x12345678u);
      n->operator()(KT::RD_DATA) = U32(0);   // ★ G-1l
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  InU1* taken_;
  InU32* target_;
  std::uint32_t insn_;
};

/// ★ 跑一个分支场景
///
/// `taken_seq[i]` = 拍 i 注入的 `BRANCH_TAKEN`（★ 0/1）
/// ★ 同一 store 指令恒定注入 imem ⇒ ★ 便于验证 flush 抑制写
BranchTrace run_branch(std::uint32_t insn, const std::vector<std::uint32_t>& taken_seq,
                       std::uint32_t target, std::uint32_t start_pc) {
  BranchTrace tr;

  // ★ ctx 名 MUST 唯一（★ 同名 ⇒ 节点 id aliasing ⇒ 单跑通过 / 全跑失败）
  static int ctx_seq = 0;
  ch::core::context ctx("open_core_branch_" + std::to_string(++ctx_seq));
  cf::plugin::PipeBuilder pb(&ctx);

  // ── 取指端口（★ 与 ibus 单测同名同义）──
  auto imem_req_addr  = must(pb.declare_out<U32>("imem_req_addr"));
  auto imem_req_valid = must(pb.declare_out<U1>("imem_req_valid"));
  auto imem_req_ready = must(pb.declare_in<U1>("imem_req_ready"));
  auto imem_rsp_data  = must(pb.declare_in<U32>("imem_rsp_data"));
  auto imem_rsp_valid = must(pb.declare_in<U1>("imem_rsp_valid"));
  auto open_pc        = must(pb.declare_out<U32>("open_pc"));
  auto open_flush     = must(pb.declare_out<U1>("open_flush"));

  // ── 访存端口（★ 与 dmem 单测同名同义）──
  auto dmem_req_addr  = must(pb.declare_out<U64>("dmem_req_addr"));
  auto dmem_req_data  = must(pb.declare_out<U64>("dmem_req_data"));
  auto dmem_req_size  = must(pb.declare_out<U3>("dmem_req_size"));
  auto dmem_req_we    = must(pb.declare_out<U1>("dmem_req_we"));
  auto dmem_req_valid = must(pb.declare_out<U1>("dmem_req_valid"));
  auto dmem_req_ready = must(pb.declare_in<U1>("dmem_req_ready"));
  auto dmem_rsp_data  = must(pb.declare_in<U64>("dmem_rsp_data"));
  auto dmem_rsp_valid = must(pb.declare_in<U1>("dmem_rsp_valid"));
  auto open_rd_data   = must(pb.declare_out<U32>("open_rd_data"));

  // ── 分支输入端口（★ 逐拍注入的通道 —— payload 是组合节点，无法逐拍变化）──
  auto branch_taken_in  = must(pb.declare_in<U1>("branch_taken_in"));
  auto branch_target_in = must(pb.declare_in<U32>("branch_target_in"));

  auto ibus = std::make_unique<cf::cpu::core::IBusPortPlugin<U32>>();
  ibus->set_initial_pc(start_pc);
  ibus->bind_ports({&imem_req_addr, &imem_req_valid, &imem_req_ready,
                    &imem_rsp_data, &imem_rsp_valid, &open_pc, &open_flush});
  pb.register_plugin(std::move(ibus));

  auto dmem = std::make_unique<cf::cpu::core::DMemPortPlugin<U32>>();
  dmem->bind_ports({&dmem_req_addr, &dmem_req_data, &dmem_req_size, &dmem_req_we,
                    &dmem_req_valid, &dmem_req_ready, &dmem_rsp_data,
                    &dmem_rsp_valid, &open_rd_data});
  pb.register_plugin(std::move(dmem));

  // ★★★ **stage 提供者 MUST 是「注册在 ibus/dmem 之后」的 plugin**
  //   （★ **实测约束 G-1n** —— 本轮踩坑，★ 见 plan §K20）：
  //   ★ `PipeBuilder::build()` 才按 `plugins_` 顺序调用 `setup()`
  //     ⇒ ★ 测试里**直接**调 `pb.at_stage(...)` 的闭包会**先于所有 plugin**
  //       注册 ⇒ ★ `canonical_stage_order()`（按 stage 名**首现序**）
  //       会把 `fetch` 排到 `branch` / `memory` **之后**
  //     ⇒ ★ `memory NORMAL`(dmem) **先于** `fetch NORMAL`(ibus) 执行
  //     ⇒ ★ dmem 读 `FLUSH` 时 `has()` = false ⇒ ★ **flush 抑制静默失效**
  //   ★ ⇒ ★ 提供者 MUST 做成 plugin 且**注册在 ibus/dmem 之后**
  //     ⇒ ★ 首现序 = `fetch, branch, memory` = **流水顺序** ✔
  auto provider = std::make_unique<BranchMemProvider>(
      &branch_taken_in, &branch_target_in, insn);
  pb.register_plugin(std::move(provider));

  std::unique_ptr<ch::Simulator> sim_holder;
  {
    ch::core::ctx_swap guard(&ctx);
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
    auto sim_res = pb.create_simulator();
    REQUIRE(sim_res.has_value());
    sim_holder = std::move(sim_res.value());
  }
  auto& sim = *sim_holder;

  // ★ 从设备侧：`ready` 恒真 + 恒定指令
  sim.set_input_value(imem_req_ready, 1);
  sim.set_input_value(imem_rsp_valid, 1);
  sim.set_input_value(imem_rsp_data, insn);
  sim.set_input_value(dmem_req_ready, 1);
  sim.set_input_value(dmem_rsp_valid, 1);
  sim.set_input_value(dmem_rsp_data, 0);
  sim.set_input_value(branch_target_in, target);

  for (std::size_t i = 0; i < taken_seq.size(); ++i) {
    sim.set_input_value(branch_taken_in, taken_seq[i]);
    sim.tick();

    tr.pc.push_back(pv(sim, open_pc));
    tr.flush.push_back(pv(sim, open_flush));
    tr.req_we.push_back(pv(sim, dmem_req_we));
    tr.req_addr.push_back(pv(sim, dmem_req_addr));
  }
  return tr;
}

// ── RISC-V 指令编码（★ funct3 MUST 落在 bits[14:12]）──
constexpr std::uint32_t kSw = 0x00002023u;  // sw x0, 0(x0)   funct3=2
constexpr std::uint32_t kNop = 0x00000013u; // addi x0, x0, 0 funct3=0

constexpr std::uint32_t kStart = 0x80000000u;
constexpr std::uint32_t kTarget = 0x80000010u;

}  // namespace

TEST_CASE("open_core: branch redirect — pc follows branch target (ADR-091 S3.2b)",
          "[open-core][chmem]") {
  // ★ ① 顺序取指：taken 恒 0 ⇒ pc 每拍 +4
  //   ★ `tr.pc[i]` = `pc_reg(i+1)`（tick 后读 ⇒ 下一拍）
  SECTION("sequential fetch when branch is not taken") {
    const auto tr = run_branch(kNop, {0, 0, 0, 0}, kTarget, kStart);
    REQUIRE(tr.pc[0] == kStart + 4);
    REQUIRE(tr.pc[1] == kStart + 8);
    REQUIRE(tr.pc[2] == kStart + 12);
    REQUIRE(tr.pc[3] == kStart + 16);
    // ★ 无重定向 ⇒ 无 flush
    for (std::size_t i = 0; i < tr.flush.size(); ++i) REQUIRE(tr.flush[i] == 0u);
  }

  // ★ ② 分支重定向：`taken(N)=1` ⇒ ★ `pc_reg(N+1) = target`
  //   ⇒ ★ 轨迹上表现为 `tr.pc[N] == target`
  SECTION("pc redirects to branch target one cycle after taken") {
    const auto tr = run_branch(kNop, {0, 1, 1, 1}, kTarget, kStart);
    REQUIRE(tr.pc[0] == kStart + 4);  // taken(0)=0 ⇒ 拍 1 顺序
    REQUIRE(tr.pc[1] == kTarget);     // ★ taken(1)=1 ⇒ 拍 2 = target
    REQUIRE(tr.pc[2] == kTarget);     // taken(2)=1 ⇒ 拍 3 = target（自跳）
    REQUIRE(tr.pc[3] == kTarget);
  }

  // ★ ③ ★ `FLUSH` 的 1 拍对齐：`FLUSH(N+1) = BRANCH_TAKEN(N)`
  //   ⇒ ★ 轨迹上表现为 `tr.flush[N] == taken_seq[N]`
  SECTION("FLUSH is one cycle behind BRANCH_TAKEN") {
    const auto tr = run_branch(kNop, {0, 1, 0, 0}, kTarget, kStart);
    // ★ 拍 1 的 taken=1 ⇒ ★ FLUSH 在**拍 2** 有效
    //   ⇒ ★ 轨迹：flush[1] 对应拍 2
    REQUIRE(tr.flush[0] == 0u);  // 拍 1（taken(0)=0）
    REQUIRE(tr.flush[1] == 1u);  // ★ 拍 2（taken(1)=1）
    REQUIRE(tr.flush[2] == 0u);  // 拍 3（taken(2)=0）
    REQUIRE(tr.flush[3] == 0u);
  }
}

TEST_CASE("open_core: top-level port names are stable (ADR-091 v1.11 G-1o)",
          "[open-core][chmem]") {
  // ★★★ **V-14**：★ 生成网表的顶层端口名 MUST **等于**契约名
  //   ⇒ ★ **MUST NOT** 出现 `_N` 后缀改名
  // ★ 机制（CppHDL codegen `codegen_verilog.cpp:64-76`）：★ 按 `eval_list`
  //   顺序建 `name_counts` ⇒ ★ **内部 node 先占名** ⇒ 重名者加 `_N` 后缀
  //   ⇒ ★ 若内部 `ch_reg` 与顶层端口同名 ⇒ ★ 端口被静默改名
  //   ⇒ ★ ⇒ 内部 reg 一律用 `_reg` 后缀（`open_pc_reg` / `open_flush_reg`）
  // ★★ **本 case MUST 走 `toVerilog`**（★ A2 之前 `branch` 测试**未生成**
  //   Verilog ⇒ ★ 新增的 `flush_reg` + `open_flush` 端口从未过 codegen）

  ch::core::context ctx("open_core_branch_verilog");
  cf::plugin::PipeBuilder pb(&ctx);

  auto imem_req_addr  = must(pb.declare_out<U32>("imem_req_addr"));
  auto imem_req_valid = must(pb.declare_out<U1>("imem_req_valid"));
  auto imem_req_ready = must(pb.declare_in<U1>("imem_req_ready"));
  auto imem_rsp_data  = must(pb.declare_in<U32>("imem_rsp_data"));
  auto imem_rsp_valid = must(pb.declare_in<U1>("imem_rsp_valid"));
  auto open_pc        = must(pb.declare_out<U32>("open_pc"));
  auto open_flush     = must(pb.declare_out<U1>("open_flush"));

  auto ibus = std::make_unique<cf::cpu::core::IBusPortPlugin<U32>>();
  ibus->bind_ports({&imem_req_addr, &imem_req_valid, &imem_req_ready,
                    &imem_rsp_data, &imem_rsp_valid, &open_pc, &open_flush});
  pb.register_plugin(std::move(ibus));

  {
    ch::core::ctx_swap guard(&ctx);
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
  }

  const std::string vpath = "/tmp/opencode/open_core_branch_top.v";
  ch::toVerilog(vpath, &ctx);

  // ★★★ **精确名匹配**（★ ADR-091 v1.11 V-15 ⇒ 共享 helper）
  //   ★ 子串匹配会让改名后的 `open_pc_1`「通过」⇒ ★ 假绿
  using cf_test::open_core::top_port_declared;

  REQUIRE(top_port_declared(vpath, "imem_req_addr"));
  REQUIRE(top_port_declared(vpath, "imem_req_valid"));
  REQUIRE(top_port_declared(vpath, "imem_req_ready"));
  REQUIRE(top_port_declared(vpath, "imem_rsp_data"));
  REQUIRE(top_port_declared(vpath, "imem_rsp_valid"));
  // ★★ 观测端口：**MUST NOT** 被改名 ⇒ ★ 名字精确等于契约名
  REQUIRE(top_port_declared(vpath, "open_pc"));
  REQUIRE(top_port_declared(vpath, "open_flush"));

  // ★★ **反向断言（★ 辅助护栏）**：★ 若端口被改名成 `open_pc_1`
  //   ⇒ ★ 契约名 `open_pc` MUST **不在**顶层
  // ★ 作用：★ **即使**正向断言被降级为子串匹配 ⇒ ★ 反向断言仍能捕获改名
  //   ⇒ ★ 实测（plan §K22）：★ 子串匹配 + 无反向断言 ⇒ **假绿复现**
  //     ⇒ ★ 反向断言是**独立**的第二道护栏
  REQUIRE_FALSE(top_port_declared(vpath, "open_pc_1"));
  REQUIRE_FALSE(top_port_declared(vpath, "open_flush_1"));
}

TEST_CASE("open_core: FLUSH suppresses wrong-path stores (ADR-091 S3.2b)",
          "[open-core][chmem]") {
  // ★ ④ flush 抑制 store：store 指令恒定 ⇒ 无 flush 时 req_we=1，有 flush 时 =0
  SECTION("store is emitted when not flushed") {
    const auto tr = run_branch(kSw, {0, 0, 0}, kTarget, kStart);
    // ★ `tr.req_we[i]` = 拍 i+1 的写使能；★ 全程无 flush ⇒ 恒 1
    for (std::size_t i = 0; i < tr.req_we.size(); ++i) {
      REQUIRE(tr.req_we[i] == 1u);
    }
  }

  SECTION("wrong-path store is suppressed by FLUSH") {
    // ★★ **抑制链（★ 逐拍推导 —— 3 个 1 拍延迟叠加）**：
    //   · `taken(k) = 1` ⇔ ★ `pc_reg(k)` 是**错路**（★ 该拍的取指本应跳 target）
    //   · `FLUSH(N) = taken(N-1)`（★ ibus 的 `flush_reg` 打 1 拍）
    //     ⇒ ★ 与 `INSTRUCTION(N)`（= 地址 `pc_reg(N-1)` 的指令）**对齐**
    //   · `we_reg(N+1) = store_ok(N) = is_store & !FLUSH(N)`（★ dmem 的 `we_reg` 打 1 拍）
    //   ⇒ ⇒ ★ **`FLUSH(N)=1` ⇒ 该 store 的写请求在拍 N+1 发出且 `we=0`**
    //   ⇒ ★ 轨迹上（tick 后读 = 下一拍）：★ `tr.req_we[i] == !taken_seq[i-1]`
    const auto tr = run_branch(kSw, {0, 1, 0, 0}, kTarget, kStart);
    REQUIRE(tr.flush[1] == 1u);     // 拍 2 的 FLUSH = taken(1)
    // ★ 拍 3 的 we = !FLUSH(2) = !taken(1) = 0 ⇒ ★ 错路 store 不写内存
    REQUIRE(tr.req_we[2] == 0u);
    // ★ 拍 4 的 we = !FLUSH(3) = !taken(2) = 1 ⇒ ★ store 恢复
    REQUIRE(tr.req_we[3] == 1u);
  }
}
