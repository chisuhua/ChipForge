// tests/cpu/test_cpu_open_core_regfile.cpp
//
// 功能描述: ADR-091 v1.12 §D3a.7 —— **两层范式**的 regfile 单元 + 桥接 plugin
//           （★ 层 2 = `ch::Component` · ★ 层 1 = `PluginBase` + `Payload`）
//
// ★ 本文件验证 4 件事：
//
//   ① ★ **G-1p（ctx 共用）**：★ Component MUST 与 ChipForge **共用同一 ctx**
//      ⇒ ★ 它的 output ports MUST 出现在网表 / simulator 的 `data_map` 里
//      ⇒ ★ （变异对照：★ `build(nullptr)` ⇒ 子 ctx ⇒ 端口消失 ⇒ **假绿**）
//
//   ② ★ **register re-naming（§D3a.8）**：★ reg 节点名 MUST 是 `reg_0..reg_31`
//      ⇒ ★ **MUST NOT** 因默认构造泄漏孤儿节点（★ 实证：★ 预载核
//      `reg_file_chmem.h:160-172` ⇒ Verilator `Duplicate declaration of
//      signal '_reg_N'`）
//
//   ③ ★ **桥接语义**：★ 层 2 的输出 MUST 被发布成 `Payload`（层 1）
//      ⇒ ★ `RS1` / `RS2` / `RD_DATA` / `RD_IDX` 可读
//
//   ④ ★ **功能正确性**：★ 写 x5 ⇒ 读 x5 得写入值；★ x0 恒 0
//
// ★ 归属: ADR-091 v1.12 §D3a.7 / §D3a.8 · §D6 G-1p
// 关联: ADR-040 v3.0（CH_MEM 移植性）· ADR-089（parity 门禁的 `reg_<i>` 契约）

#include "catch_amalgamated.hpp"

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <codegen_verilog.h>
#include <core/bool.h>
#include <core/context.h>
#include <core/uint.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/branch_iface_chmem.h"
#include "ip/cpu/core/ibus_port_chmem.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/core/regfile_bridge_chmem.h"
#include "ip/cpu/core/regfile_chmem.h"
#include "tests/cpu/open_core_verilog_helper_chmem.h"

namespace {

using U1 = ch::core::ch_uint<1>;
using U5 = ch::core::ch_uint<5>;
using U32 = ch::core::ch_uint<32>;

template <typename P>
P must(cf::plugin::Result<P> r) {
  REQUIRE(r.has_value());
  return *r;
}

std::uint64_t pv(const ch::Simulator& sim,
                 const ch::core::port<ch::core::ch_uint<32>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}

/// ★★ 一次「层 2 单元 + 层 1 桥接」的装配
struct Rig {
  ch::core::context ctx{"open_core_regfile"};
  cf::plugin::PipeBuilder pb{&ctx};
  std::unique_ptr<ch::Simulator> sim;

  // ★★★ **端口 MUST 用 `unique_ptr` 持有**（★ 两个陷阱叠加）：
  //   ① **禁** `&must(declare_*(...))` ⇒ ★ 那是对**临时对象**取地址
  //      ⇒ ★ 临时对象析构 ⇒ ★ 端口 `impl_` 失效 ⇒ ★ 端口「消失」⇒ **假绿**
  //      （★ ADR-091 v1.6 记录的同款坑）
  //   ② ★ `ch::core::port` 的 **`operator=` 被 `delete`**（★ 连拷贝赋值都不可）
  //      ⇒ ★ `std::optional<port>` **也不行**（★ optional 赋值要求 T 可赋值）
  //   ⇒ ★ ⇒ ★ 用 `unique_ptr`（★ `port` 的**拷贝构造**可用 ⇒ `make_unique` OK）
  std::unique_ptr<ch::core::ch_in<U5>> rs1_addr_v;
  std::unique_ptr<ch::core::ch_in<U5>> rs2_addr_v;
  std::unique_ptr<ch::core::ch_in<U5>> rd_addr_v;
  std::unique_ptr<ch::core::ch_in<U32>> rd_data_v;
  std::unique_ptr<ch::core::ch_in<U1>> rd_we_v;

  std::unique_ptr<ch::core::ch_out<U32>> open_rs1_v;
  std::unique_ptr<ch::core::ch_out<U32>> open_rs2_v;
  std::unique_ptr<ch::core::ch_out<U32>> open_rd_data_v;

  // ★★ **顶层观测端口**（★ 契约名 `reg_1..reg_31`）
  std::unique_ptr<ch::core::ch_out<U32>> open_reg_v[32];

  // ★ 视图指针（★ 指向堆上本体 ⇒ ★ 不悬垂）
  ch::core::ch_in<U5>* rs1_addr = nullptr;
  ch::core::ch_in<U5>* rs2_addr = nullptr;
  ch::core::ch_in<U5>* rd_addr = nullptr;
  ch::core::ch_in<U32>* rd_data = nullptr;
  ch::core::ch_in<U1>* rd_we = nullptr;
  ch::core::ch_out<U32>* open_rs1 = nullptr;
  ch::core::ch_out<U32>* open_rs2 = nullptr;
  ch::core::ch_out<U32>* open_rd_data = nullptr;

  std::unique_ptr<cf::cpu::core::RegFileBridgePlugin<U32>> bridge;

  Rig() {
    rs1_addr_v = std::make_unique<ch::core::ch_in<U5>>(
        must(pb.declare_in<U5>("rf_rs1_addr")));
    rs2_addr_v = std::make_unique<ch::core::ch_in<U5>>(
        must(pb.declare_in<U5>("rf_rs2_addr")));
    rd_addr_v = std::make_unique<ch::core::ch_in<U5>>(
        must(pb.declare_in<U5>("rf_rd_addr")));
    rd_data_v = std::make_unique<ch::core::ch_in<U32>>(
        must(pb.declare_in<U32>("rf_rd_data")));
    rd_we_v = std::make_unique<ch::core::ch_in<U1>>(
        must(pb.declare_in<U1>("rf_rd_we")));

    open_rs1_v = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("open_rs1")));
    open_rs2_v = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("open_rs2")));
    open_rd_data_v = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("open_rd_data")));

    rs1_addr = rs1_addr_v.get();
    rs2_addr = rs2_addr_v.get();
    rd_addr = rd_addr_v.get();
    rd_data = rd_data_v.get();
    rd_we = rd_we_v.get();
    open_rs1 = open_rs1_v.get();
    open_rs2 = open_rs2_v.get();
    open_rd_data = open_rd_data_v.get();

    // ★★ 31 个观测端口（★ **契约名** `reg_1..reg_31`；★ [0] 不用 ⇒ x0）
    cf::cpu::core::RegFileBridgePlugin<U32>::ObsPorts obs;
    for (std::size_t i = 1; i < 32; ++i) {
      open_reg_v[i] = std::make_unique<ch::core::ch_out<U32>>(
          must(pb.declare_out<U32>("reg_" + std::to_string(i))));
      obs.reg[i] = std::make_unique<ch::core::ch_out<U32>>(*open_reg_v[i]);
    }

    bridge = std::make_unique<cf::cpu::core::RegFileBridgePlugin<U32>>();
    bridge->bind_ports({rs1_addr, rs2_addr, rd_addr, rd_data, rd_we, nullptr});
    bridge->bind_obs_ports(std::move(obs));
    pb.register_plugin(std::move(bridge));
  }

  void build_and_run() {
    {
      ch::core::ctx_swap guard(&ctx);
      REQUIRE(pb.build().has_value());
      REQUIRE(pb.elaborate().has_value());
      auto sim_res = pb.create_simulator();
      REQUIRE(sim_res.has_value());
      sim = std::move(sim_res.value());
    }
  }

};

}  // namespace

TEST_CASE("open_core: regfile unit shares ctx with ChipForge (ADR-091 G-1p)",
          "[open-core][chmem]") {
  // ★★ **G-1p**：★ Component MUST 与 ChipForge **共用同一 ctx**
  //   ⇒ ★ 否则子 ctx ⇒ node-id aliasing ⇒ ★ 组件的 output ports
  //     **从 simulator 的 `data_map` 消失** ⇒ ★ 观测**静默读不到** ⇒ 假绿。
  Rig rig;
  rig.build_and_run();

  // ★ ① 仿真器建立成功（★ Component 的 build 未炸 ⇒ ctx 共用路径通）
  REQUIRE(rig.sim != nullptr);

  // ★ ② ★ **网表里 MUST 出现 `reg_0..reg_31`**（★ 契约名 —— §D3a.8）
  const std::string vpath = "/tmp/opencode/open_core_regfile_top.v";
  {
    ch::core::ctx_swap guard(&rig.ctx);
    ch::toVerilog(vpath, &rig.ctx);
  }
  std::ifstream f(vpath);
  REQUIRE(f.good());
  std::string all;
  std::string line;
  while (std::getline(f, line)) all += line + "\n";

  // ★ `reg_N` 是**内部节点**（★ 非顶层端口）⇒ ★ 在 Verilog 里以
  //   `logic [31:0] reg_N;` 形式出现 ⇒ ★ 断言其存在
  for (int i = 0; i < 32; ++i) {
    const std::string needle = "reg_" + std::to_string(i);
    REQUIRE(all.find(needle) != std::string::npos);
  }
  // ★★ **反向断言（★ §D3a.8 的核心）**：★ MUST **NOT** 出现默认名 `"reg"`
  //   ⇒ ★ 若用 `std::array` 默认构造 ⇒ ★ 泄漏 32 个名为 `reg` 的孤儿节点
  //     ⇒ ★ 与显式命名的 `reg_N` 撞名 ⇒ ★ Verilator Duplicate declaration
  REQUIRE(all.find("logic [31:0] reg;") == std::string::npos);
}

TEST_CASE("open_core: regfile bridge publishes layer-2 outputs as payload",
          "[open-core][chmem]") {
  // ★ 桥接语义：★ 层 2 的 `io()` 输出 MUST 被发布成 `Payload`（层 1）
  //   ⇒ ★ `RS1` / `RS2` / `RD_DATA` / `RD_IDX` 在 payload store 里可读
  Rig rig;

  // ★ 追加一个「payload 消费者」闭包（★ 把 payload 接到顶层端口 ⇒ 可观测）
  rig.pb.at_stage("decode", cf::plugin::Phase::NORMAL, [&rig] {
    auto* n = rig.pb.node_of_logic_stage("decode").get();
    if (!n) return;
    using KT = cf::cpu::core::payload::keys<U32, 32>;
    *rig.open_rs1 = n->operator()(KT::RS1);
    *rig.open_rs2 = n->operator()(KT::RS2);
    *rig.open_rd_data = n->operator()(KT::RD_DATA);
  });

  rig.build_and_run();

  // ★ 驱动：★ 读 x5 / 写 x5 = 0xDEAD
  rig.sim->set_input_value(*rig.rs1_addr, 5);
  rig.sim->set_input_value(*rig.rs2_addr, 5);
  rig.sim->set_input_value(*rig.rd_addr, 5);
  rig.sim->set_input_value(*rig.rd_data, 0xDEADu);

  // ★★★ **读取口径**（★ ADR-091 已记录）：★ `sim.tick()` **之后**读端口
  //   ⇒ ★ 读到的是**下一拍**的值 ⇒ ★ `tick` 次数 = 拍号 + 1

  // ★ 拍 1：**不写**（`rd_we = 0`）⇒ ★ 拍 2 读 x5 = 初值 0
  rig.sim->set_input_value(*rig.rd_we, 0);
  rig.sim->tick();
  REQUIRE(pv(*rig.sim, *rig.open_rs1) == 0u);
  REQUIRE(pv(*rig.sim, *rig.open_rs2) == 0u);
  // ★ 写数据通路（★ 组合）：★ `RD_DATA` = 0xDEAD（★ 与 `rd_we` 无关）
  REQUIRE(pv(*rig.sim, *rig.open_rd_data) == 0xDEADu);

  // ★ 拍 2：**写**（`rd_we = 1`）⇒ ★ 写入在拍 2 末生效 ⇒ ★ 拍 3 读到 0xDEAD
  rig.sim->set_input_value(*rig.rd_we, 1);
  rig.sim->tick();
  REQUIRE(pv(*rig.sim, *rig.open_rs1) == 0xDEADu);
  REQUIRE(pv(*rig.sim, *rig.open_rs2) == 0xDEADu);

  // ★ 拍 3：★ 值保持
  rig.sim->tick();
  REQUIRE(pv(*rig.sim, *rig.open_rs1) == 0xDEADu);

  // ★★★ **顶层观测端口 `reg_5` MUST 反映寄存器值**（★ 不只是「端口存在」）
  //   ⇒ ★ 端口存在但**恒 0** 也是**假绿** ⇒ ★ MUST 验**值**
  //   ⇒ ★ `reg_5` 在 `decode NORMAL` 闭包里被驱动（★ 读 `ch_reg` 当前值）
  REQUIRE(pv(*rig.sim, *rig.open_reg_v[5]) == 0xDEADu);
  // ★ 未写过的 x7 ⇒ ★ 仍为初值 0
  REQUIRE(pv(*rig.sim, *rig.open_reg_v[7]) == 0u);
}

// ★★★ **branch 提供者**（★ MUST 做成 plugin 且注册在 ibus **之后** —— ★ G-1n）
class BranchTakenProvider : public cf::plugin::PluginBase {
 public:
  explicit BranchTakenProvider(ch::core::ch_in<U1>* taken)
      : taken_(taken) {}

  void setup(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("branch", cf::plugin::Phase::EARLY, [this, &pb] {
      auto* n = pb.node_of_logic_stage("branch").get();
      if (n) {
        namespace BK = cf::cpu::core::branch_keys;
        n->operator()(BK::BRANCH_TAKEN) = ch::core::ch_bool(
            ch::core::operator==(U1(*taken_), U1(ch::core::ch_literal<1, 1>{})));
      }
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  ch::core::ch_in<U1>* taken_;
};

TEST_CASE("open_core: FLUSH suppresses wrong-path register writes (ADR-091 A4-3)",
          "[open-core][chmem]") {
  // ★★★ **A4-3**（★ ADR-091 v1.13 · tasks A4-3）：★ **错路指令 MUST NOT 写寄存器**
  //   ★ 预载核 `reg_file_chmem.h:222-232` 做 `we = writes_rd && (rd != 0) && !flush`
  //   ★ ⇒ ★ 开放核 MUST 同款（★ 掩码在**桥接层** —— ★ Component 是层 2，读不到 payload）
  //   ★ ★★ **不掩码的后果**：★ H-3 的 `regs` 终值必挂（★ 错路写会留痕）
  //
  // ★ **集成级**验证（★ 不只是单元）：★ ibus（产生 FLUSH）+ branch 提供者
  //   （产生 `BRANCH_TAKEN`）+ regfile bridge（做掩码）★ 三者对接。

  ch::core::context ctx{"open_core_regfile_flush"};
  cf::plugin::PipeBuilder pb(&ctx);

  // ★★ **端口 MUST 用 `unique_ptr` 持有**（★ 禁 `&must(...)` —— ★ 那是对
  //   **临时对象**取地址 ⇒ ★ 端口 `impl_` 失效 ⇒ ★ 端口「消失」⇒ ★ **假绿**）
  // ── 取指侧（★ 提供 `FLUSH` 的生产者）──
  auto imem_req_addr  = std::make_unique<ch::core::ch_out<U32>>(
      must(pb.declare_out<U32>("imem_req_addr")));
  auto imem_req_valid = std::make_unique<ch::core::ch_out<U1>>(
      must(pb.declare_out<U1>("imem_req_valid")));
  auto imem_req_ready = std::make_unique<ch::core::ch_in<U1>>(
      must(pb.declare_in<U1>("imem_req_ready")));
  auto imem_rsp_data  = std::make_unique<ch::core::ch_in<U32>>(
      must(pb.declare_in<U32>("imem_rsp_data")));
  auto imem_rsp_valid = std::make_unique<ch::core::ch_in<U1>>(
      must(pb.declare_in<U1>("imem_rsp_valid")));
  auto open_pc        = std::make_unique<ch::core::ch_out<U32>>(
      must(pb.declare_out<U32>("open_pc")));
  auto open_flush     = std::make_unique<ch::core::ch_out<U1>>(
      must(pb.declare_out<U1>("open_flush")));

  // ── regfile 侧（★ 桥接的输入）──
  auto rf_rd_addr = std::make_unique<ch::core::ch_in<U5>>(
      must(pb.declare_in<U5>("rf_rd_addr")));
  auto rf_rd_data = std::make_unique<ch::core::ch_in<U32>>(
      must(pb.declare_in<U32>("rf_rd_data")));
  auto rf_rd_we   = std::make_unique<ch::core::ch_in<U1>>(
      must(pb.declare_in<U1>("rf_rd_we")));
  // ★ 观测端口（★ 契约名 `reg_1..reg_31`）
  std::vector<std::unique_ptr<ch::core::ch_out<U32>>> reg_ports(32);
  cf::cpu::core::RegFileBridgePlugin<U32>::ObsPorts obs;
  for (std::size_t i = 1; i < 32; ++i) {
    reg_ports[i] = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("reg_" + std::to_string(i))));
    obs.reg[i] = std::make_unique<ch::core::ch_out<U32>>(*reg_ports[i]);
  }
  // ★ branch 提供者的输入
  auto branch_taken_in = std::make_unique<ch::core::ch_in<U1>>(
      must(pb.declare_in<U1>("branch_taken_in")));

  // ★ ★★ **注册顺序 = 流水顺序**（★ G-1n：★ 首现序 MUST 等于流水顺序）
  //   ★ ibus（建 `fetch` + `branch`）→ ★ bridge（建 `decode`）→ ★ provider（`branch` 已存在）
  auto ibus = std::make_unique<cf::cpu::core::IBusPortPlugin<U32>>();
  ibus->bind_ports({imem_req_addr.get(), imem_req_valid.get(),
                    imem_req_ready.get(), imem_rsp_data.get(),
                    imem_rsp_valid.get(), open_pc.get(), open_flush.get()});
  pb.register_plugin(std::move(ibus));

  auto bridge = std::make_unique<cf::cpu::core::RegFileBridgePlugin<U32>>();
  // ★ ★★ `flush_src` = ibus 的 `open_flush`（★ **顶层 `ch_out`** ⇒ ★ 组合节点
  //   ⇒ ★ 在 `build()` 时已有效 ⇒ ★ 掩码逐拍正确）
  bridge->bind_ports({nullptr, nullptr, rf_rd_addr.get(), rf_rd_data.get(),
                      rf_rd_we.get(), open_flush.get()});
  bridge->bind_obs_ports(std::move(obs));
  pb.register_plugin(std::move(bridge));

  pb.register_plugin(
      std::make_unique<BranchTakenProvider>(branch_taken_in.get()));

  std::unique_ptr<ch::Simulator> sim;
  {
    ch::core::ctx_swap guard(&ctx);
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
    auto sim_res = pb.create_simulator();
    REQUIRE(sim_res.has_value());
    sim = std::move(sim_res.value());
  }

  sim->set_input_value(*imem_req_ready, 1);
  sim->set_input_value(*imem_rsp_valid, 1);
  sim->set_input_value(*imem_rsp_data, 0x00000013u);
  sim->set_input_value(*rf_rd_addr, 5);
  sim->set_input_value(*rf_rd_we, 1);
  sim->set_input_value(*branch_taken_in, 0);

  auto read_reg5 = [&sim, &reg_ports]() -> std::uint64_t {
    return static_cast<std::uint64_t>(sim->get_port_value(*reg_ports[5]));
  };

  // ★★★ **拍 0**：★ 无 flush ⇒ ★ 写入生效（★ 写 x5 = 0xDEAD）
  sim->set_input_value(*rf_rd_data, 0xDEADu);
  sim->tick();
  // ★ tick 后读 = 下一拍的值 ⇒ ★ 拍 1 读到 0xDEAD
  REQUIRE(read_reg5() == 0xDEADu);

  // ★★★ **拍 1**：★ `BRANCH_TAKEN = 1` ⇒ ★ 拍 1 末 `flush_reg` 生效
  //   ⇒ ★ ⇒ ★ **拍 2 的 `FLUSH` = 1** ⇒ ★ **拍 2 的写入 MUST 被抑制**
  sim->set_input_value(*branch_taken_in, 1);
  sim->tick();
  REQUIRE(static_cast<std::uint64_t>(sim->get_port_value(*open_flush)) == 1u);

  // ★★★ **拍 2**：★ 写 x5 = 0xBEEF ⇒ ★ **被 flush 抑制** ⇒ ★ 值不变
  sim->set_input_value(*rf_rd_data, 0xBEEFu);
  sim->tick();
  REQUIRE(read_reg5() == 0xDEADu);   // ★ ★ 未被 0xBEEF 覆盖

  // ★ **拍 3**：★ `FLUSH` 撤销 ⇒ ★ 写入恢复
  sim->set_input_value(*branch_taken_in, 0);
  sim->tick();
  sim->set_input_value(*rf_rd_data, 0xCAFEu);
  sim->tick();
  REQUIRE(read_reg5() == 0xCAFEu);   // ★ ★ 恢复后写入生效
}

TEST_CASE("open_core: regs observation is exposed as top-level ports "
          "(ADR-091 v1.12 D3a.9)",
          "[open-core][chmem]") {
  // ★★ **§D3a.9**：`regs[32]` 归**核** ⇒ ★ 载体 MUST 是**顶层 `ch_out`**
  //   ⇒ ★ 契约名 = `reg_1..reg_31`（★ parity runner 按 `reg_<i>` 采集）
  //   ⇒ ★ **MUST NOT** 与内部 `ch_reg` 名同名（★ G-1o）
  Rig rig;
  rig.build_and_run();

  const std::string vpath = "/tmp/opencode/open_core_regs_top.v";
  {
    ch::core::ctx_swap guard(&rig.ctx);
    ch::toVerilog(vpath, &rig.ctx);
  }
  using cf_test::open_core::top_port_declared;

  // ★ ① **契约名 MUST 精确存在**（★ 顶层端口）
  for (int i = 1; i < 32; ++i) {
    REQUIRE(top_port_declared(vpath, "reg_" + std::to_string(i)));
  }
  // ★ ② ★★ **反向断言（G-1o）**：★ 端口名 MUST **NOT** 被改名
  //   ⇒ ★ 若内部 `ch_reg` 名与端口同名 ⇒ ★ 端口变 `reg_1_1`
  //   ⇒ ★ ⇒ ★ 断言 `_1` 后缀形式**不在**顶层
  for (int i = 1; i < 32; ++i) {
    REQUIRE_FALSE(top_port_declared(vpath, "reg_" + std::to_string(i) + "_1"));
  }
  // ★ ③ `reg_0`（x0）MUST **NOT** 出现在顶层（★ 恒 0 ⇒ 无需观测）
  REQUIRE_FALSE(top_port_declared(vpath, "reg_0"));
  // ★ ④ 内部实现名 MUST 带 `open_` 前缀（★ 不与契约名撞名）
  std::ifstream f(vpath);
  REQUIRE(f.good());
  std::string all;
  std::string line;
  while (std::getline(f, line)) all += line + "\n";
  REQUIRE(all.find("open_reg_1") != std::string::npos);
}

TEST_CASE("open_core: regfile x0 is hardwired zero (RISC-V semantics)",
          "[open-core][chmem]") {
  Rig rig;
  rig.pb.at_stage("decode", cf::plugin::Phase::NORMAL, [&rig] {
    auto* n = rig.pb.node_of_logic_stage("decode").get();
    if (!n) return;
    using KT = cf::cpu::core::payload::keys<U32, 32>;
    *rig.open_rs1 = n->operator()(KT::RS1);
  });
  rig.build_and_run();

  // ★ 尝试写 x0 ⇒ ★ MUST 仍读出 0
  rig.sim->set_input_value(*rig.rs1_addr, 0);
  rig.sim->set_input_value(*rig.rd_addr, 0);
  rig.sim->set_input_value(*rig.rd_data, 0x1234u);
  rig.sim->set_input_value(*rig.rd_we, 1);

  rig.sim->tick();
  rig.sim->tick();
  REQUIRE(pv(*rig.sim, *rig.open_rs1) == 0u);
}
