// tests/cpu/test_cpu_open_core_mem_model.cpp
//
// 功能描述: ADR-091 §D8 S4 —— CH_MEM 开放核的**验证实现（从设备 / 内存模型）**
//           （★ A5-1：★ 内存 + 组合 imem/dmem 响应 + 按 `size` 的 byte-strobe RMW）
//
// ★ 本文件验证 4 件事：
//
//   ① ★ **组合 slave**：★ 请求有效 ⇒ 同拍应答（★ 0 拍读延迟）
//      ⇒ ★ `ready` 恒真（★ ADR-091 §D3a.2 第一档）
//
//   ② ★ **取指读**：★ `imem_req_addr` ⇒ ★ `imem_rsp_data` = 该**字**的内容
//
//   ③ ★★★ **写语义（★ 本文件的核心）**：★ `size` = 字节数 + `address` 低 2 位
//      ⇒ ★ **从设备做 byte-strobe RMW**（★ ADR-091 v1.7：★ CPU 侧不做 RMW）
//      ⇒ ★ 覆盖 1/2/4 字节 × 不同 byte_off
//
//   ④ ★ **未写过的位置保持 0**
//
// ★ ★ **本文件不依赖核**（★ 从设备**独立可测** —— ADR-091 v1.12 §D3a.7 的
//   「层 2 = 可复用 IP 单元」精神）★ ⇒ ★ 直接驱动从设备的端口。
//
// ★ 归属: ADR-091 §D8 S4（tasks 4.1/4.2）· §D3a.4（从设备 = 组合 slave）
// 关联: ADR-040 v3.0（CH_MEM 移植性）· ADR-091 v1.7（`size` 字段）

#include "catch_amalgamated.hpp"

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <core/bool.h>
#include <core/context.h>
#include <core/uint.h>

#include <cstdint>
#include <memory>
#include <string>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/verify_mem_model_chmem.h"

namespace {

using U1 = ch::core::ch_uint<1>;
using U3 = ch::core::ch_uint<3>;
using U32 = ch::core::ch_uint<32>;
using U64 = ch::core::ch_uint<64>;

template <typename P>
P must(cf::plugin::Result<P> r) {
  REQUIRE(r.has_value());
  return *r;
}

std::uint64_t pv(const ch::Simulator& sim,
                 const ch::core::port<ch::core::ch_uint<32>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t pv64(const ch::Simulator& sim,
                   const ch::core::port<ch::core::ch_uint<64>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}
std::uint64_t pv1(const ch::Simulator& sim,
                  const ch::core::port<ch::core::ch_uint<1>, ch::core::output_direction>& p) {
  return static_cast<std::uint64_t>(sim.get_port_value(p));
}

/// ★★ 从设备的独立装配（★ 不依赖核）
struct MemRig {
  ch::core::context ctx{"open_core_mem_model"};
  cf::plugin::PipeBuilder pb{&ctx};
  std::unique_ptr<ch::Simulator> sim;

  // ★ 输入端口（★ 核侧）
  std::unique_ptr<ch::core::ch_in<U32>> imem_req_addr;
  std::unique_ptr<ch::core::ch_in<U1>> imem_req_valid;
  std::unique_ptr<ch::core::ch_in<U64>> dmem_req_addr;
  std::unique_ptr<ch::core::ch_in<U64>> dmem_req_data;
  std::unique_ptr<ch::core::ch_in<U3>> dmem_req_size;
  std::unique_ptr<ch::core::ch_in<U1>> dmem_req_we;
  std::unique_ptr<ch::core::ch_in<U1>> dmem_req_valid;

  // ★ 输出端口（★ 从设备侧）
  std::unique_ptr<ch::core::ch_out<U1>> imem_req_ready;
  std::unique_ptr<ch::core::ch_out<U32>> imem_rsp_data;
  std::unique_ptr<ch::core::ch_out<U1>> imem_rsp_valid;
  std::unique_ptr<ch::core::ch_out<U1>> dmem_req_ready;
  std::unique_ptr<ch::core::ch_out<U64>> dmem_rsp_data;
  std::unique_ptr<ch::core::ch_out<U1>> dmem_rsp_valid;

  std::unique_ptr<cf::cpu::core::VerifyMemModelPlugin> mem;

  MemRig() {
    imem_req_addr = std::make_unique<ch::core::ch_in<U32>>(
        must(pb.declare_in<U32>("imem_req_addr")));
    imem_req_valid = std::make_unique<ch::core::ch_in<U1>>(
        must(pb.declare_in<U1>("imem_req_valid")));
    imem_req_ready = std::make_unique<ch::core::ch_out<U1>>(
        must(pb.declare_out<U1>("imem_req_ready")));
    imem_rsp_data = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("imem_rsp_data")));
    imem_rsp_valid = std::make_unique<ch::core::ch_out<U1>>(
        must(pb.declare_out<U1>("imem_rsp_valid")));

    dmem_req_addr = std::make_unique<ch::core::ch_in<U64>>(
        must(pb.declare_in<U64>("dmem_req_addr")));
    dmem_req_data = std::make_unique<ch::core::ch_in<U64>>(
        must(pb.declare_in<U64>("dmem_req_data")));
    dmem_req_size = std::make_unique<ch::core::ch_in<U3>>(
        must(pb.declare_in<U3>("dmem_req_size")));
    dmem_req_we = std::make_unique<ch::core::ch_in<U1>>(
        must(pb.declare_in<U1>("dmem_req_we")));
    dmem_req_valid = std::make_unique<ch::core::ch_in<U1>>(
        must(pb.declare_in<U1>("dmem_req_valid")));
    dmem_req_ready = std::make_unique<ch::core::ch_out<U1>>(
        must(pb.declare_out<U1>("dmem_req_ready")));
    dmem_rsp_data = std::make_unique<ch::core::ch_out<U64>>(
        must(pb.declare_out<U64>("dmem_rsp_data")));
    dmem_rsp_valid = std::make_unique<ch::core::ch_out<U1>>(
        must(pb.declare_out<U1>("dmem_rsp_valid")));

    mem = std::make_unique<cf::cpu::core::VerifyMemModelPlugin>();
    mem->bind_ports({imem_req_addr.get(), imem_req_valid.get(),
                     imem_req_ready.get(), imem_rsp_data.get(),
                     imem_rsp_valid.get(), dmem_req_addr.get(),
                     dmem_req_data.get(), dmem_req_size.get(),
                     dmem_req_we.get(), dmem_req_valid.get(),
                     dmem_req_ready.get(), dmem_rsp_data.get(),
                     dmem_rsp_valid.get()});
    pb.register_plugin(std::move(mem));
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
    sim->set_input_value(*imem_req_valid, 1);
    sim->set_input_value(*dmem_req_valid, 1);
    // ★★★ **必须先 tick 一次**（★ `ch_out` 端口的值在 simulator 里**更新于
    //   tick** ⇒ ★ 未 tick 就读 ⇒ ★ 读到**初值 0** ⇒ ★ 假失败）
    //   ⇒ ★ 本轮实测踩过（★ A2 的 branch 测试因先 tick 而没踩到）
    sim->tick();
  }

  /// ★ 读一个字（★ 组合 ⇒ 同一拍就有值）
  std::uint32_t read_word(std::uint32_t byte_addr) {
    sim->set_input_value(*dmem_req_addr, static_cast<std::uint64_t>(byte_addr));
    sim->set_input_value(*dmem_req_size, 4);
    sim->set_input_value(*dmem_req_we, 0);
    sim->tick();
    return static_cast<std::uint32_t>(pv64(*sim, *dmem_rsp_data) & 0xFFFFFFFFull);
  }

  /// ★ 写（★ `size` = 字节数；★ `data` 低 32 bit 有效，★ 未移位）
  void write(std::uint64_t byte_addr, std::uint32_t data, std::uint32_t size) {
    sim->set_input_value(*dmem_req_addr, byte_addr);
    sim->set_input_value(*dmem_req_data, static_cast<std::uint64_t>(data));
    sim->set_input_value(*dmem_req_size, size);
    sim->set_input_value(*dmem_req_we, 1);
    sim->tick();
    // ★ 撤销写使能（★ 后续读不被写影响）
    sim->set_input_value(*dmem_req_we, 0);
  }
};

}  // namespace

TEST_CASE("open_core: mem model is a combinational slave (ADR-091 S4)",
          "[open-core][chmem]") {
  // ★ ① 组合 slave：★ `ready` 恒真；★ 请求有效 ⇒ 同拍应答
  MemRig rig;
  rig.build_and_run();

  REQUIRE(pv1(*rig.sim, *rig.imem_req_ready) == 1u);
  REQUIRE(pv1(*rig.sim, *rig.dmem_req_ready) == 1u);
  REQUIRE(pv1(*rig.sim, *rig.imem_rsp_valid) == 1u);
  REQUIRE(pv1(*rig.sim, *rig.dmem_rsp_valid) == 1u);

  // ★ ② 取指读：★ 未写过的地址 ⇒ ★ 0（★ 内存初值全 0）
  rig.sim->set_input_value(*rig.imem_req_addr, 0x80000000u);
  rig.sim->tick();
  REQUIRE(pv(*rig.sim, *rig.imem_rsp_data) == 0u);
}

TEST_CASE("open_core: mem model does byte-strobe RMW by size (ADR-091 v1.7)",
          "[open-core][chmem]") {
  // ★★★ **写语义的核心**：★ `size` = 字节数 + `address` 低 2 位
  //   ⇒ ★ **从设备**做 byte-strobe RMW（★ CPU 侧不做 RMW ⇒ 写无延迟）
  MemRig rig;
  rig.build_and_run();

  constexpr std::uint64_t kWordAddr = 0x80000100u;   // ★ 4 字节对齐

  // ★ ② **整字写**（size=4）⇒ ★ 整个字被替换
  rig.write(kWordAddr, 0xAABBCCDDu, 4);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0xAABBCCDDu);

  // ★ ③ **1 字节写**（size=1，byte_off=0）⇒ ★ 只改最低字节
  rig.write(kWordAddr + 0, 0x11u, 1);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0xAABBCC11u);

  // ★ ④ **1 字节写**（size=1，byte_off=1）⇒ ★ 只改第 2 字节
  rig.write(kWordAddr + 1, 0x22u, 1);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0xAABB2211u);

  // ★ ⑤ **1 字节写**（size=1，byte_off=3）⇒ ★ 只改最高字节
  rig.write(kWordAddr + 3, 0x44u, 1);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0x44BB2211u);

  // ★ ⑥ **2 字节写**（size=2，byte_off=0 ⇒ 低半字）
  rig.write(kWordAddr + 0, 0xBEEFu, 2);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0x44BBBEEFu);

  // ★ ⑦ **2 字节写**（size=2，byte_off=2 ⇒ 高半字）
  rig.write(kWordAddr + 2, 0xCAFEu, 2);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr)) == 0xCAFEBEEFu);

  // ★ ⑧ **未写过的位置保持 0**（★ 另一个字）
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kWordAddr + 4)) == 0u);
}

TEST_CASE("open_core: mem model write enable gates the write (ADR-091 S4)",
          "[open-core][chmem]") {
  // ★ 写使能为 0 ⇒ ★ **整个字不被写**（★ RMW 的结果也 MUST NOT 落盘）
  MemRig rig;
  rig.build_and_run();

  constexpr std::uint64_t kAddr = 0x80000200u;

  // ★ 先写一个基准值
  rig.write(kAddr, 0x12345678u, 4);
  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kAddr)) == 0x12345678u);

  // ★ ★ **写使能 = 0 + 带数据** ⇒ ★ MUST NOT 改动内存
  rig.sim->set_input_value(*rig.dmem_req_addr, kAddr);
  rig.sim->set_input_value(*rig.dmem_req_data, 0xFFFFFFFFull);
  rig.sim->set_input_value(*rig.dmem_req_size, 4);
  rig.sim->set_input_value(*rig.dmem_req_we, 0);
  rig.sim->tick();

  REQUIRE(rig.read_word(static_cast<std::uint32_t>(kAddr)) == 0x12345678u);
}
