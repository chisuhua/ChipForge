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
#include <fstream>
#include <memory>
#include <string>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/verify_mem_model_chmem.h"
#include "tests/cpu/open_core_verilog_helper.h"

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

  // ★★ **观测端口**（★ 契约名 `mem_sig` / `mem_sig_updates`）
  std::unique_ptr<ch::core::ch_out<U32>> mem_sig;
  std::unique_ptr<ch::core::ch_out<U32>> mem_sig_updates;

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

    mem_sig = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("mem_sig")));
    mem_sig_updates = std::make_unique<ch::core::ch_out<U32>>(
        must(pb.declare_out<U32>("mem_sig_updates")));

    mem = std::make_unique<cf::cpu::core::VerifyMemModelPlugin>();
    mem->bind_ports({imem_req_addr.get(), imem_req_valid.get(),
                     imem_req_ready.get(), imem_rsp_data.get(),
                     imem_rsp_valid.get(), dmem_req_addr.get(),
                     dmem_req_data.get(), dmem_req_size.get(),
                     dmem_req_we.get(), dmem_req_valid.get(),
                     dmem_req_ready.get(), dmem_rsp_data.get(),
                     dmem_rsp_valid.get(), mem_sig.get(),
                     mem_sig_updates.get()});
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

/// ★★ **C++ 侧 reference**：★ 复刻预载核 `dmem_chmem.h:244-270` 的折叠
///   ⇒ ★ 与从设备实现**逐拍对照** ⇒ ★ 「逐位同款」的**直接证据**
struct SigRef {
  std::uint32_t sig = 0;
  std::uint32_t upd = 0;

  /// ★ 一次写（★ `word_addr` = 字地址；★ `store_wdata` = **RMW 后的最终字**）
  void store(std::uint32_t word_addr, std::uint32_t store_wdata) {
    const std::uint32_t mix =
        word_addr * 31u + store_wdata + upd * 0x9E3779B1u;
    const std::uint32_t next = (sig * 16777619u) ^ mix;
    sig = next;
    upd += 1u;
  }
};

/// ★★ **影子内存**（★ C++ 侧）⇒ ★ 复刻从设备的 byte-strobe RMW
///   ⇒ ★ 提供 reference 需要的 `store_wdata`
struct ShadowMem {
  std::vector<std::uint32_t> words;

  explicit ShadowMem(std::size_t n_words, std::uint32_t init = 0u)
      : words(n_words, init) {}

  std::uint32_t read(std::uint32_t word_addr) const {
    return words[word_addr & 0x3FFFu];
  }

  /// ★ 返回**写入后的最终字**（★ 即从设备的 `merged`）
  std::uint32_t store(std::uint64_t byte_addr, std::uint32_t data,
                      std::uint32_t size) {
    const std::uint32_t word_addr =
        static_cast<std::uint32_t>((byte_addr >> 2) & 0x3FFFu);
    const std::uint32_t off = static_cast<std::uint32_t>(byte_addr & 3u);
    const std::uint32_t cur = words[word_addr];
    std::uint32_t merged = cur;
    if (size == 1u) {
      const std::uint32_t shift = off * 8u;
      merged = (cur & ~(0xFFu << shift)) | ((data & 0xFFu) << shift);
    } else if (size == 2u) {
      const std::uint32_t shift = (off & 2u) * 8u;
      merged = (cur & ~(0xFFFFu << shift)) | ((data & 0xFFFFu) << shift);
    } else {
      merged = data;
    }
    words[word_addr] = merged;
    return merged;
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

TEST_CASE("open_core: memsig folding is bit-identical to the preloaded core "
          "(ADR-091 v1.11 D3a.6)",
          "[open-core][chmem]") {
  // ★★★ **§D3a.6 的兑现**：★ `memsig` 折叠 MUST 与预载核
  //   `ip/cpu/plugins/dmem_chmem.h:244-270` **逐位同款**
  //   ⇒ ★ 本 case 用 **C++ 侧 reference**（★ 复刻预载核算法）**逐拍对照**
  //   ⇒ ★ 这是「逐位同款」的**直接证据**（★ 不是「两边都跑通」）
  MemRig rig;
  rig.build_and_run();

  SigRef ref;
  ShadowMem shadow(1u << 14, 0u);

  // ★ ★ **写序列**（★ 混合 1/2/4 字节 + 不同 byte_off ⇒ ★ 覆盖 RMW 分支）
  struct WriteOp {
    std::uint64_t addr;
    std::uint32_t data;
    std::uint32_t size;
  };
  const WriteOp ops[] = {
      {0x80000100u, 0xAABBCCDDu, 4u},
      {0x80000100u, 0x11u, 1u},
      {0x80000101u, 0x22u, 1u},
      {0x80000103u, 0x44u, 1u},
      {0x80000100u, 0xBEEFu, 2u},
      {0x80000102u, 0xCAFEu, 2u},
      {0x80000200u, 0xDEADBEEFu, 4u},
      {0x80000204u, 0x5Au, 1u},
  };

  for (const auto& op : ops) {
    // ★ C++ 侧：★ 影子内存先算 RMW 后的最终字（★ = 从设备的 `merged`）
    const std::uint32_t merged =
        shadow.store(op.addr, op.data, op.size);
    // ★ C++ 侧：★ reference 折叠
    ref.store(static_cast<std::uint32_t>((op.addr >> 2) & 0x3FFFu), merged);

    // ★ DUT 侧：★ 驱动一次写 + tick
    rig.write(op.addr, op.data, op.size);

    // ★★★ **逐拍对照**（★ 端口值 = tick 末的 sig ⇒ 与 reference 同步）
    REQUIRE(pv(*rig.sim, *rig.mem_sig) == ref.sig);
    REQUIRE(pv(*rig.sim, *rig.mem_sig_updates) == ref.upd);
  }

  // ★★ **自校验锚**（★ ADR-089 契约 S-6）：★ 写过 ⇒ 计数 MUST > 0
  REQUIRE(pv(*rig.sim, *rig.mem_sig_updates) == static_cast<std::uint64_t>(
      sizeof(ops) / sizeof(ops[0])));
}

TEST_CASE("open_core: memsig is exposed as top-level ports with contract names "
          "(ADR-091 v1.12 D3a.9)",
          "[open-core][chmem]") {
  // ★★ 载体 MUST 是**顶层 `ch_out`**（★ H-2）⇒ ★ 契约名
  //   `mem_sig` / `mem_sig_updates`（★ 与预载核同名 ⇒ runner 零改动）
  MemRig rig;
  rig.build_and_run();

  const std::string vpath = "/tmp/opencode/open_core_memsig_top.v";
  {
    ch::core::ctx_swap guard(&rig.ctx);
    ch::toVerilog(vpath, &rig.ctx);
  }
  using cf_test::open_core::top_port_declared;

  REQUIRE(top_port_declared(vpath, "mem_sig"));
  REQUIRE(top_port_declared(vpath, "mem_sig_updates"));
  // ★★ **反向断言（G-1o）**：★ 端口名 MUST **NOT** 被改名
  REQUIRE_FALSE(top_port_declared(vpath, "mem_sig_1"));
  REQUIRE_FALSE(top_port_declared(vpath, "mem_sig_updates_1"));
  // ★ 内部实现名 MUST 带 `open_` 前缀（★ 不与契约名撞名）
  std::ifstream f(vpath);
  REQUIRE(f.good());
  std::string all;
  std::string line;
  while (std::getline(f, line)) all += line + "\n";
  REQUIRE(all.find("open_mem_sig") != std::string::npos);
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
