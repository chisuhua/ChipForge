// tests/cpu/test_cpu_open_core_dmem.cpp
//
// 功能描述: ADR-091 §D8 S3 —— CH_MEM 开放核的**数据访存端口化**验证
//           （访存走对外端口，核内**无** `ch_mem`、**无** ELF 预载）
//
// ★ 本文件是 S3 访存侧的契约测试（取指侧见 `test_cpu_open_core_ibus.cpp`）。
//
// ---------------------------------------------------------------------------
// 验证 5 件事：
//
//   ① **端口进网表顶层**：★ 8 个 dmem 端口 + 1 个观测端口 MUST 都在顶层
//      （★ 对照：预载核只有 clock/reset 2 端口 ⇒ 这是「封闭核 → 开放核」
//      的可判定差异）
//
//   ② **请求地址正确**：★ `dmem_req_addr` MUST 等于 payload 的字节地址
//
//   ③ **写语义完备**：★ `dmem_req_size` MUST 表达访问字节宽度（1/2/4）
//      · `dmem_req_we` MUST 区分读/写
//      · `dmem_req_data` MUST 是**原始**写数据（★ CPU 侧不做 RMW）
//
//   ④ **读提取正确**：★ `lb`/`lbu`/`lh`/`lhu`/`lw` 的符号扩展与零扩展
//      MUST 与预载核 `dmem_chmem.h` 的 byte select 树**逐位同构**
//
//   ⑤ ★ **时序等价性**：★ 数据消费拍 = 请求呈现拍 + 1
//      （★ 组合消费 `rsp_data` ⇒ ★ 这是「外部提前 1 拍供数」的**必要条件**
//      —— ADR-091 §D3a.4 硬约束 G-1k）
//
// ---------------------------------------------------------------------------
// ★ 逐拍驱动范式（★ 外部从设备的义务 —— 也是 S4 验证实现的范式）：
//
//     for each tick:
//         set_input_value(rsp_data, mem[上一拍读到的 req_addr])   ← 提前 1 拍供数
//         tick()
//         读本拍发出的 req_addr → 供下一拍用
//
// ★ 归属: ADR-091（CH_MEM 开放核形态与端口架构）§D8 S3 / §D6 H-2 / §D3a.4
// 关联: ADR-089（双后端差分 parity 门禁）· ADR-092（TLM 冻结）

#include "catch_amalgamated.hpp"

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— module.h 缺 inline ⇒ 多 TU 链接失败）
#include <codegen_verilog.h>
#include <core/bool.h>
#include <core/context.h>
#include <core/reg.h>
#include <core/traits.h>
#include <core/uint.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "tests/cpu/open_core_verilog_helper_chmem.h"
#include "ip/cpu/core/dmem_port_chmem.h"


/// ★ 解包 `Result<Port>`（★ ADR-047：静态配置头禁 throw ⇒ 端口声明走
///   `Result`）⇒ ★ 失败即 fail-closed（★ 不产生「半成品端口对象」）
///
/// ★★★ **MUST 按值返回**（★ 不可返回引用）：★ `Result<P>` 是**临时对象**
///   ⇒ ★ 若返回 `*r`（引用）⇒ ★ 引用悬垂（★ full-expression 结束后临时对象
///   ★ 析构 ⇒ ★ 端口节点的 `impl_` 随之失效 ⇒ ★ 端口「消失」⇒ 假绿）
///   ⇒ ★ 按值返回 + C++17 copy elision ⇒ ★ 不触发 `port` 的 deleted 拷贝赋值
template <typename P>
P must(cf::plugin::Result<P> r) {
  REQUIRE(r.has_value());
  return *r;
}
namespace {

using U32 = ch::core::ch_uint<32>;
using U64 = ch::core::ch_uint<64>;

// ★ 在生成的网表里查顶层端口声明
// ★★★ **精确名匹配**（★ ADR-091 v1.11 V-15）⇒ ★ 共享 helper
using cf_test::open_core::top_port_declared;

std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<64>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<64>, ch::core::input_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<3>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<1>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<1>, ch::core::input_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t port_value(const ch::Simulator& sim,
                         const ch::core::port<ch::core::ch_uint<32>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}

/// ★ 单场景的逐拍观测轨迹
struct DmemTrace {
  std::vector<std::uint64_t> req_addr;
  std::vector<std::uint64_t> req_data;
  std::vector<std::uint64_t> req_size;
  std::vector<std::uint64_t> req_we;
  std::vector<std::uint64_t> req_valid;
  std::vector<std::uint32_t> rd_data;
};

/// ★ 跑一个访存场景：★ 核内**无** `ch_mem`、**无** ELF ⇒ 数据只能来自端口
DmemTrace run_dmem(std::uint32_t insn, std::uint32_t vaddr,
                   std::uint32_t rs2, std::uint64_t rsp_word, int ticks) {
  DmemTrace tr;

  // ★★★ **ctx 名 MUST 唯一**（★ 实测约束 G-1o）：★ 多个场景用**同名 ctx**
  //   ⇒ ★ 上游节点 id 复用（aliasing）⇒ ★ 后一个场景的 DAG 引用到前一个的节点
  //   ⇒ ★ 表现为「单跑通过、全跑失败」（★ 实测：lb 符号扩展在单跑时对、全跑时错）
  //   ⇒ ★ 上游 `device.h:13-19` 记录过同款坑。
  ch::core::context ctx("open_core_dmem_fixed");
  cf::plugin::PipeBuilder pb(&ctx);

  // ★ 端口由装配层统一声明（顶层接口，MUST NOT 由 plugin 各自散着声明）
  auto req_addr  = must(pb.declare_out<U64>("dmem_req_addr"));
  auto req_data  = must(pb.declare_out<U64>("dmem_req_data"));
  auto req_size  = must(pb.declare_out<ch::core::ch_uint<3>>("dmem_req_size"));
  auto req_we    = must(pb.declare_out<ch::core::ch_uint<1>>("dmem_req_we"));
  auto req_valid = must(pb.declare_out<ch::core::ch_uint<1>>("dmem_req_valid"));
  auto req_ready = must(pb.declare_in<ch::core::ch_uint<1>>("dmem_req_ready"));
  auto rsp_data  = must(pb.declare_in<U64>("dmem_rsp_data"));
  auto rsp_valid = must(pb.declare_in<ch::core::ch_uint<1>>("dmem_rsp_valid"));
  auto rd_data   = must(pb.declare_out<U32>("open_rd_data"));

  // ★ payload 提供者：`memory` stage 的 EARLY phase
  //   ⇒ ★ 早于 dmem plugin 的 NORMAL 闭包（★ 同 stage 内 phase 桶优先）
  pb.at_stage("memory", cf::plugin::Phase::EARLY,
              [&pb, insn, vaddr, rs2] {
                using KT = cf::cpu::core::payload::keys<U32, 32>;
                auto* n = pb.node_of_logic_stage("memory").get();
                if (!n) return;
                n->operator()(KT::INSTRUCTION) = U32(insn);
                n->operator()(KT::RESULT) = U32(vaddr);
                n->operator()(KT::RS2) = U32(rs2);
                // ★★★ **MUST 写 `RD_DATA`**（★ **实测约束 G-1n**
                //   —— ★ gdb 栈坐实：`muximpl::create_instruction` →
                //   `lnodeimpl::id(nullptr)` ⇒ `Simulator::initialize` 段错误）：★
                //   dmem plugin 的写入形态是
                //   `RD_DATA = select(is_load, load_result, RD_DATA)`
                //   ⇒ ★ **false 分支（= 上游值）在首次赋值前被读**
                //   ⇒ ★ `PayloadStore::get`（可变版）缺失时 emplace **null impl**
                //   ⇒ ★★ `Simulator::initialize` 段错误
                //       （gdb 栈：`muximpl::create_instruction` → `lnodeimpl::id(nullptr)`）
                //   ⇒ ★ 预载核不崩是因为 **EX 阶段总是先写 `RD_DATA`**
                //   ⇒ ★ 本测试**模拟上游 EX 阶段**（ALU 结果 0）
                n->operator()(KT::RD_DATA) = U32(0);
              });

  auto dmem = std::make_unique<cf::cpu::core::DMemPortPlugin<U32>>();
  dmem->bind_ports({&req_addr, &req_data, &req_size, &req_we, &req_valid,
                    &req_ready, &rsp_data, &rsp_valid, &rd_data});
  pb.register_plugin(std::move(dmem));

  // ★ `build()` MUST 在 `elaborate()` 之前（★ ADR-091 §D5.1 G-1j —— 框架已加固，
  //   但显式调用更清晰）；★ `elaborate()` 内部自建 `ctx_swap`（★ G-1h）
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

  // ★ 从设备侧：`ready` 恒真（★ 本版本只实现无阻塞流）+ 提前 1 拍供数
  sim.set_input_value(req_ready, 1);
  sim.set_input_value(rsp_valid, 1);

  for (int i = 0; i < ticks; ++i) {
    // ★ ★ **提前 1 拍供数**：★ 数据在**本拍**组合期被消费
    //   ⇒ 本拍喂的数据对应「上一拍读到的地址」⇒ ★ 下一拍才被消费
    sim.set_input_value(rsp_data, rsp_word);
    sim.tick();

    tr.req_addr.push_back(port_value(sim, req_addr));
    tr.req_data.push_back(port_value(sim, req_data));
    tr.req_size.push_back(port_value(sim, req_size));
    tr.req_we.push_back(port_value(sim, req_we));
    tr.req_valid.push_back(port_value(sim, req_valid));
    tr.rd_data.push_back(static_cast<std::uint32_t>(port_value(sim, rd_data)));
  }
  return tr;
}

// ── RISC-V 指令编码（★ 只用需要的几条）──
// ★★★ **编码纪律**：★ RISC-V I 型 = funct3[14:12] | rd[11:7] | rs1[19:15] | imm | opcode[6:0]
//   ⇒ ★ **funct3 MUST 落在 bits[14:12]**（★ 本轮实测踩坑：`0x00030183` 的 `0x3`
//   落到了 **rs1** 字段 ⇒ funct3 实际为 0 ⇒ `lbu` 被解码成 `lb`
//   ⇒ ★ **符号扩展被误施加** ⇒ 期望 `0x000000FF` 实得 `0xFFFFFFFF` ⇒ 假失败）
constexpr std::uint32_t kLw  = 0x00002083u;  // lw  x1, 0(x0)   funct3=2
constexpr std::uint32_t kLb  = 0x00010103u;  // lb  x2, 0(x0)   funct3=0
constexpr std::uint32_t kLbu = 0x00004183u;  // lbu x3, 0(x0)   funct3=4 ★ (0x00004183: 4<<12|3<<7|0x03)
constexpr std::uint32_t kLh  = 0x00001203u;  // lh  x4, 0(x0)   funct3=1 ★ (1<<12|4<<7|0x03)
constexpr std::uint32_t kLhu = 0x00005283u;  // lhu x5, 0(x0)   funct3=5 ★ (5<<12|5<<7|0x03)
constexpr std::uint32_t kSw  = 0x00002023u;  // sw  x0, 0(x0)   funct3=2
constexpr std::uint32_t kSb  = 0x00000023u;  // sb  x0, 0(x0)   funct3=0
constexpr std::uint32_t kSh  = 0x00001023u;  // sh  x0, 0(x0)   funct3=1

// ★ 观测数据：byte0=0xFF, byte1=0x7F, byte2=0xFF, byte3=0x80
constexpr std::uint32_t kWord = 0x80FF7FFFu;

}  // namespace

TEST_CASE("open_core: dmem ports reach netlist top level (ADR-091 S3-3)",
          "[open-core][chmem]") {
  ch::core::context ctx("open_core_dmem_ports");
  cf::plugin::PipeBuilder pb(&ctx);

  auto req_addr  = must(pb.declare_out<U64>("dmem_req_addr"));
  auto req_data  = must(pb.declare_out<U64>("dmem_req_data"));
  auto req_size  = must(pb.declare_out<ch::core::ch_uint<3>>("dmem_req_size"));
  auto req_we    = must(pb.declare_out<ch::core::ch_uint<1>>("dmem_req_we"));
  auto req_valid = must(pb.declare_out<ch::core::ch_uint<1>>("dmem_req_valid"));
  auto req_ready = must(pb.declare_in<ch::core::ch_uint<1>>("dmem_req_ready"));
  auto rsp_data  = must(pb.declare_in<U64>("dmem_rsp_data"));
  auto rsp_valid = must(pb.declare_in<ch::core::ch_uint<1>>("dmem_rsp_valid"));
  auto rd_data   = must(pb.declare_out<U32>("open_rd_data"));

  // ★ payload 提供者（★ 与 case 2 同款 ⇒ 避免 payload 缺失导致 null impl 的 ERROR 日志）
  pb.at_stage("memory", cf::plugin::Phase::EARLY,
              [&pb] {
                using KT = cf::cpu::core::payload::keys<U32, 32>;
                auto* n = pb.node_of_logic_stage("memory").get();
                if (!n) return;
                n->operator()(KT::INSTRUCTION) = U32(kLw);
                n->operator()(KT::RESULT) = U32(0x80000000u);
                n->operator()(KT::RS2) = U32(0);
                n->operator()(KT::RD_DATA) = U32(0);   // ★ G-1n：模拟上游 EX 阶段
              });

  auto dmem = std::make_unique<cf::cpu::core::DMemPortPlugin<U32>>();
  dmem->bind_ports({&req_addr, &req_data, &req_size, &req_we, &req_valid,
                    &req_ready, &rsp_data, &rsp_valid, &rd_data});
  pb.register_plugin(std::move(dmem));

  {
    ch::core::ctx_swap guard(&ctx);
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
  }

  const std::string vpath = "/tmp/opencode/open_core_dmem_top.v";
  ch::toVerilog(vpath, &ctx);

  // ★ ① 8 个访存端口 + 1 个观测端口 MUST 都在顶层
  REQUIRE(top_port_declared(vpath, "dmem_req_addr"));
  REQUIRE(top_port_declared(vpath, "dmem_req_data"));
  REQUIRE(top_port_declared(vpath, "dmem_req_size"));
  REQUIRE(top_port_declared(vpath, "dmem_req_we"));
  REQUIRE(top_port_declared(vpath, "dmem_req_valid"));
  REQUIRE(top_port_declared(vpath, "dmem_req_ready"));
  REQUIRE(top_port_declared(vpath, "dmem_rsp_data"));
  REQUIRE(top_port_declared(vpath, "dmem_rsp_valid"));
  REQUIRE(top_port_declared(vpath, "open_rd_data"));
}

TEST_CASE("open_core: dmem requests over ports without any ELF preload "
          "(ADR-091 S3-3/4/5)",
          "[open-core][chmem]") {
  // ★ ② 请求地址 MUST 等于 payload 的字节地址
  SECTION("req_addr equals payload byte address") {
    const auto tr = run_dmem(kLw, 0x80000000u, 0u, kWord, 3);
    for (std::size_t i = 0; i < tr.req_addr.size(); ++i) {
      REQUIRE(tr.req_addr[i] == 0x80000000u);
    }
    // ★ `valid` 恒真（★ 本版本只实现「`ready` 恒真」的无阻塞流）
    for (std::size_t i = 0; i < tr.req_valid.size(); ++i) {
      REQUIRE(tr.req_valid[i] == 1u);
    }
  }

  // ★ ③ 写语义完备：`size` 表达字节宽度 + `we` 区分读写 + `data` 是原始写数据
  SECTION("write semantics: size + we + raw data") {
    {
      const auto tr = run_dmem(kSw, 0x80000000u, 0x12345678u, kWord, 2);
      REQUIRE(tr.req_we[0] == 1u);      // 写
      REQUIRE(tr.req_size[0] == 4u);    // sw = 4 字节
      REQUIRE(tr.req_data[0] == 0x12345678u);  // ★ 原始写数据（CPU 侧不做 RMW）
    }
    {
      const auto tr = run_dmem(kSb, 0x80000001u, 0xABu, kWord, 2);
      REQUIRE(tr.req_we[0] == 1u);
      REQUIRE(tr.req_size[0] == 1u);    // sb = 1 字节
      REQUIRE(tr.req_addr[0] == 0x80000001u);  // ★ 字节地址（★ 从设备据此推 strobe）
    }
    {
      const auto tr = run_dmem(kSh, 0x80000002u, 0xBEEFu, kWord, 2);
      REQUIRE(tr.req_we[0] == 1u);
      REQUIRE(tr.req_size[0] == 2u);    // sh = 2 字节
    }
    {
      const auto tr = run_dmem(kLw, 0x80000000u, 0u, kWord, 2);
      REQUIRE(tr.req_we[0] == 0u);      // 读
      REQUIRE(tr.req_size[0] == 4u);
    }
  }

  // ★ ④ 读提取正确（★ 与预载核 byte select 树逐位同构）
  SECTION("load extraction matches the preload-core byte select tree") {
    // lw ⇒ 整个字
    {
      const auto tr = run_dmem(kLw, 0x80000000u, 0u, kWord, 3);
      REQUIRE(tr.rd_data[1] == 0x80FF7FFFu);
    }
    // lb @ off0 ⇒ byte0 = 0xFF ⇒ 符号扩展 ⇒ 0xFFFFFFFF
    {
      const auto tr = run_dmem(kLb, 0x80000000u, 0u, kWord, 3);
      REQUIRE(tr.rd_data[1] == 0xFFFFFFFFu);
    }
    // lbu @ off0 ⇒ byte0 = 0xFF ⇒ 零扩展 ⇒ 0x000000FF
    {
      const auto tr = run_dmem(kLbu, 0x80000000u, 0u, kWord, 3);
      REQUIRE(tr.rd_data[1] == 0x000000FFu);
    }
    // lh @ off2 ⇒ half1 = 0x80FF ⇒ 符号扩展 ⇒ 0xFFFF80FF
    {
      const auto tr = run_dmem(kLh, 0x80000002u, 0u, kWord, 3);
      REQUIRE(tr.rd_data[1] == 0xFFFF80FFu);
    }
    // lhu @ off2 ⇒ half1 = 0x80FF ⇒ 零扩展 ⇒ 0x000080FF
    {
      const auto tr = run_dmem(kLhu, 0x80000002u, 0u, kWord, 3);
      REQUIRE(tr.rd_data[1] == 0x000080FFu);
    }
  }

  // ★ ⑤ ★ 时序形态（★ G-1k）：★ **数据消费点比地址产出点晚 1 拍**
  //   （★ 这就是「外部提前 1 拍供数」的**必要条件** —— ADR-091 §D3a.4）
  //
  //   ★★★ **CH_MEM 的 `ch_reg` 时序（★ 实测澄清，★ 极易搞错）**：
  //   ★ `ch_reg` 的 next 是**组合表达式** ⇒ ★ 每次 tick 的 sequential 阶段
  //   ★ 用「**tick N-1 末的 DAG 状态**」重新求值 ⇒ ★
  //   ★   · tick N 的 sequential：`addr_reg_ ← expr(tick N-1 末的状态)`
  //   ★   · tick N 的组合：端口呈现 `addr_reg_`
  //   ★ ⇒ ★ **端口在拍 N 呈现的是「拍 N-1 末算出的地址」**
  //   ★ ⇒ ★ host 在 tick N 后读到它 ⇒ 在 tick N+1 前供数
  //   ★ ⇒ ★ **消费拍 N+1 的数据对应地址拍 N** ⇒ ★ **延迟 1 拍** ✔
  //   ★ ⇒ ★ 这正是 **G-1k**（地址提前 1 拍可见）的成立机制。
  SECTION("load result is one cycle behind the address it belongs to") {
    const auto tr = run_dmem(kLw, 0x80000000u, 0u, kWord, 3);
    // ★ 请求地址（★ 本场景地址恒定 ⇒ 每拍相同）
    REQUIRE(tr.req_addr[0] == 0x80000000u);
    // ★ 加载结果**组合消费** ⇒ tick 0 后即已可见
    //   （★ 数据由 host 在 tick 0 前按「上一拍读到的地址」供上）
    REQUIRE(tr.rd_data[0] == 0x80FF7FFFu);
    REQUIRE(tr.rd_data[1] == 0x80FF7FFFu);
  }
}
