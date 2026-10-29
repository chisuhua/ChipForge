// ip/cpu/core/verify_mem_model_chmem.h
//
// 功能描述: CH_MEM 开放核的**验证实现（从设备 / 内存模型）**
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-29
//
// ============================================================================
// 归属 (ADR-091 §D8 S4 —— tasks 4.2)
// ============================================================================
//
// ★ 本文件是 ADR-091 §D8 **S4** 的落点：★ 「`verify_mem_model_chmem.h`
//   （验证实现：ELF 加载 + 内存模型）」。
//
// ★★★ **架构定位（★ 核外，不是层 1/层 2 的划分对象）**：★
//   ★ **层 1** = 核内 stage 间（`PluginBase` + `at_stage` + `Payload`）
//   ★ **层 2** = 可复用 IP 单元（`ch::Component` + `io()` struct）
//     —— ★ ADR-091 v1.12 §D3a.7
//   ★ ★ **从设备既不是层 1 也不是层 2** ⇒ ★ 理由：
//     · ★ 它的接口**就是网表顶层端口**（★ 它是 DUT 的**外部边界**）
//       ⇒ ★ 若做成 `Component` ⇒ ★ 它的 `io()` 不是顶层端口
//         ⇒ ★ 仍需「桥接」把 io 接到顶层 ⇒ ★ **同义反复**
//     · ★ 它**不属于核**⇒ ★ 不参与「核内 stage 间」的 payload 机制
//   ★ ⇒ ★ **从设备 = 普通 plugin**（★ 用 `pb.declare_in/out` 声明自己的顶层端口）。
//
// ============================================================================
// ★★★ 时序契约（★ ADR-091 §D3a.4 —— 「从设备 MUST 是组合 slave」）
// ============================================================================
//
// ★ **组合 slave** ⇒ ★ 收到地址的**当拍**返回数据（★ 0 拍读延迟）
//   ⇒ ★ 写**无延迟**（★ 写不依赖读响应 ⇒ ★ 无 load-after-store 问题
//     ⇒ ★ 无需 stall / forwarding —— ADR-091 §D3a.4）
// ★ ★ 上游已把 `ch_mem::aread` 对齐成 0 拍组合 assign
//   （★ CppHDL `c27543d`）⇒ ★ 可行。
//
// ★★ **与预载核的差异（★ 关键）**：★
//   ★ 预载核：★ 指令译码出 `is_sb`/`is_sh`/`is_sw` ⇒ ★ **CPU 侧**做 RMW
//   ★ 本文件：★ 端口带 `size`（★ ADR-091 v1.7）⇒ ★ **从设备**按
//     `size` + `byte_off` 做 byte-strobe RMW ⇒ ★ **CPU 侧不做 RMW**
//     ⇒ ★ 写**无延迟**（★ 与预载核的「写生效绝对拍」一致）
//
// ============================================================================
// ★★★ 待实现（★ **诚实标注** —— 本版本**不含**这些）
// ============================================================================
//
//   ✗ **ELF 加载**（★ tasks 4.2 的另一半）⇒ ★ 归 **A5-2**
//     ⇒ ★ 本版本只提供 `set_elf_image()` 钩子（★ 存储字节，★ 不解析）
//   ✗ **`mem_sig` / `mem_sig_updates`**（★ ADR-091 v1.11 §D3a.6 的观测来源）
//     ⇒ ★ 归 **A5-3**
//   ✗ **`tohost` 探针**（★ ADR-091 v1.12 §D3a.9）⇒ ★ 归 **A5-4**
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0 / ADR-091 §D5.1)
// ============================================================================
//
//   · CH_MEM-only ⇒ 不引 `cf::plugin::uint_t` / `cpptlm`
//   · ★ **MUST NOT** include `<ch.hpp>`（★ G-1b：★ `module.h:13` 缺 `inline`）
//     ⇒ ★ 只 include 具体头
//   · ★ **MUST NOT** include `ip/cpu/picolibc_host_memory.h`（★ ADR-092 冻结集
//     ⇒ ★ 只能**参考语义**，★ 不得 include）
//   · ★ 顶层端口名 MUST NOT 与内部 `ch_reg` / `ch_mem` 名同名（★ G-1o）
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_VERIFY_MEM_MODEL_CHMEM_H
#define CF_IP_CPU_CORE_VERIFY_MEM_MODEL_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <core/bool.h>
#include <core/io.h>
#include <core/mem.h>
#include <core/operators.h>
#include <core/operators_width.h>
#include <core/reg.h>
#include <core/uint.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace core {

/// ★★ **验证实现（从设备 / 内存模型）**
///
/// ★ 端口（★ 与 `ibus_port_chmem.h` / `dmem_port_chmem.h` 的核侧**逐字段对应**）：
///   · 取指：★ 核 = master（`imem_req_*` 出 / `imem_rsp_*` 入）
///   · 访存：★ 核 = master（`dmem_req_*` 出 / `dmem_rsp_*` 入）
///
/// ★★ **写语义**（★ ADR-091 v1.7）：★ `size` = **字节数**（1/2/4）
///   ★ 数据 = **原始写数据**（★ 低 32 bit 有效，★ **未移位**）
///   ⇒ ★ 本从设备按 `size` + `address` 低 2 位做 byte-strobe RMW
class VerifyMemModelPlugin : public cf::plugin::PluginBase {
 public:
  using U1 = ch::core::ch_uint<1>;
  using U3 = ch::core::ch_uint<3>;
  using U32 = ch::core::ch_uint<32>;
  using U64 = ch::core::ch_uint<64>;

  /// ★ 内存规模（★ 字数）⇒ ★ 字节地址低 16 bit（★ 字地址 14 bit）
  /// ★★ 与预载核同款（★ `cpu_factory_chmem.h` 的 `kMemWords`）
  static constexpr std::size_t kMemWords = 16384;

  struct Ports {
    // ── 取指（★ 核 = master）──
    ch::core::ch_in<U32>* imem_req_addr = nullptr;
    ch::core::ch_in<U1>* imem_req_valid = nullptr;
    ch::core::ch_out<U1>* imem_req_ready = nullptr;
    ch::core::ch_out<U32>* imem_rsp_data = nullptr;
    ch::core::ch_out<U1>* imem_rsp_valid = nullptr;
    // ── 访存（★ 核 = master）──
    ch::core::ch_in<U64>* dmem_req_addr = nullptr;
    ch::core::ch_in<U64>* dmem_req_data = nullptr;
    ch::core::ch_in<U3>* dmem_req_size = nullptr;
    ch::core::ch_in<U1>* dmem_req_we = nullptr;
    ch::core::ch_in<U1>* dmem_req_valid = nullptr;
    ch::core::ch_out<U1>* dmem_req_ready = nullptr;
    ch::core::ch_out<U64>* dmem_rsp_data = nullptr;
    ch::core::ch_out<U1>* dmem_rsp_valid = nullptr;
  };

  void bind_ports(const Ports& p) { ports_ = p; }

  /// ★★ **ELF 镜像钩子**（★ A5-2 的入口）—— ★ 本版本**只存储，不解析**
  ///   ⇒ ★ 诚实标注：★ **ELF 加载尚未实现**（★ tasks 4.2 的另一半）
  void set_elf_image(const std::vector<std::uint8_t>& bytes) {
    elf_image_ = bytes;
  }
  bool has_elf_image() const { return !elf_image_.empty(); }

  void setup(cf::plugin::PipeBuilder& pb) override {
    auto& mem = get_mem();

    // ★★ **组合 slave**：★ `ready` 恒真（★ ADR-091 §D3a.2 第一档
    //   「`ready` 恒真 ⇒ 无阻塞流」★ 本从设备**恒**支持）
    if (ports_.imem_req_ready) *ports_.imem_req_ready = U1(ch::core::ch_literal<1, 1>{});
    if (ports_.dmem_req_ready) *ports_.dmem_req_ready = U1(ch::core::ch_literal<1, 1>{});

    // ── 取指响应（★ 组合读）──
    if (ports_.imem_rsp_data && ports_.imem_rsp_valid) {
      const auto imem_addr = ch::core::ch_uint<32>(*ports_.imem_req_addr);
      // ★ `bits<15,2>` 的宽度 = 15-2+1 = **14 bit** ⇒ ★ 与 `ch_mem` 的
      //   addr 宽度一致（★ N = 2^14）⇒ ★ **MUST NOT** 再包跨宽构造
      //   （★ G-1r：★ 跨宽构造须 `zext`，★ 且语义未文档化）
      const auto imem_word = ch::core::bits<15, 2>(imem_addr);
      const auto rp = mem.aread(imem_word, "imem_aread");
      *ports_.imem_rsp_data = U32(rp.impl());
      // ★ `valid` = 请求有效（★ 组合 slave ⇒ 同拍应答）
      *ports_.imem_rsp_valid = U1(*ports_.imem_req_valid);
    }

    // ── 访存响应 + 写（★ 组合读 + byte-strobe RMW）──
    if (ports_.dmem_rsp_data && ports_.dmem_rsp_valid) {
      const auto dmem_addr = ch::core::ch_uint<64>(*ports_.dmem_req_addr);
      const auto dmem_word = ch::core::bits<15, 2>(dmem_addr);
      const auto byte_off = ch::core::ch_uint<2>(ch::core::bits<1, 0>(dmem_addr));

      // ★★ **RMW 的「读当前字」**（★ sub-word store 必须先读）
      const auto rp = mem.aread(dmem_word, "dmem_aread");
      const auto cur_word = ch::core::ch_uint<32>(rp.impl());
      *ports_.dmem_rsp_data = ch::core::zext<64>(cur_word);
      *ports_.dmem_rsp_valid = U1(*ports_.dmem_req_valid);

      // ★★ **写数据**（★ 原始写数据，★ 低 32 bit 有效，★ **未移位**）
      const auto wdata = ch::core::ch_uint<32>(
          ch::core::bits<31, 0>(ch::core::ch_uint<64>(*ports_.dmem_req_data)));
      const auto size = ch::core::ch_uint<3>(*ports_.dmem_req_size);
      // ★ `we` 保持 `ch_uint<1>`（★ `select` 的 cond 走 `to_operand` ⇒
      //   **不限** `ch_bool`）⇒ ★ **MUST NOT** 强行转 `ch_bool`
      //   （★ `ch_bool` 无从 `ch_in<U1>` 的构造路径 ⇒ 编译错）
      const auto we = ch::core::ch_uint<1>(*ports_.dmem_req_we);

      // ★★ **byte-strobe RMW**（★ 按 `size` = 字节数 1/2/4）
      //   ★ 语义与预载核 `dmem_chmem.h:238` 的 `store_wdata` **同款**：
      //   ★ 目标字节清零 ⇒ ★ 写入数据**左移**到目标位置 ⇒ ★ OR 回去
      const auto is_size1 = ch::core::ch_bool(ch::core::operator==(
          size, ch::core::ch_uint<3>(ch::core::ch_literal<1, 3>{})));
      const auto is_size2 = ch::core::ch_bool(ch::core::operator==(
          size, ch::core::ch_uint<3>(ch::core::ch_literal<2, 3>{})));

      // ★ byte_off 决定字节位置（★ 0..3）
      const auto off0 = ch::core::ch_bool(ch::core::operator==(
          byte_off, ch::core::ch_uint<2>(ch::core::ch_literal<0, 2>{})));
      const auto off1 = ch::core::ch_bool(ch::core::operator==(
          byte_off, ch::core::ch_uint<2>(ch::core::ch_literal<1, 2>{})));
      const auto off2 = ch::core::ch_bool(ch::core::operator==(
          byte_off, ch::core::ch_uint<2>(ch::core::ch_literal<2, 2>{})));

      // ★ 字节移位量（★ 0/8/16/24）
      const auto byte_shift = ch::core::ch_uint<5>(ch::core::select(
          off0, ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}),
          ch::core::select(off1, ch::core::ch_uint<5>(ch::core::ch_literal<8, 5>{}),
          ch::core::select(off2, ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}),
                           ch::core::ch_uint<5>(ch::core::ch_literal<24, 5>{})))));
      // ★ 字节掩码（★ 0x000000FF / 0x0000FF00 / 0x00FF0000 / 0xFF000000）
      const auto byte_mask = ch::core::ch_uint<32>(ch::core::select(
          off0, ch::core::ch_uint<32>(ch::core::ch_literal<0x000000FF, 32>{}),
          ch::core::select(off1, ch::core::ch_uint<32>(ch::core::ch_literal<0x0000FF00, 32>{}),
          ch::core::select(off2, ch::core::ch_uint<32>(ch::core::ch_literal<0x00FF0000, 32>{}),
                           ch::core::ch_uint<32>(ch::core::ch_literal<0xFF000000, 32>{})))));
      // ★ 半字掩码（★ byte_off[1] 决定低/高半字）
      const auto half_off = ch::core::ch_bool(off2);
      const auto half_mask = ch::core::ch_uint<32>(ch::core::select(
          half_off, ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF0000, 32>{}),
                     ch::core::ch_uint<32>(ch::core::ch_literal<0x0000FFFF, 32>{})));
      const auto half_shift = ch::core::ch_uint<5>(ch::core::select(
          half_off, ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}),
                     ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{})));

      // ★ 合并：★ 目标字节清零 + 移位后的数据 OR 回去
      const auto byte_new = (cur_word & ~byte_mask) |
                            ((wdata << byte_shift) & byte_mask);
      const auto half_new = (cur_word & ~half_mask) |
                            ((wdata << half_shift) & half_mask);
      // ★ size=1 ⇒ 字节；★ size=2 ⇒ 半字；★ 其余（4/8）⇒ 整字
      const auto merged = ch::core::ch_uint<32>(ch::core::select(
          is_size1, byte_new, ch::core::select(is_size2, half_new, wdata)));
      // ★★ **写使能为 0 时整个字不写**（★ `mem.write` 的 enable 语义）
      //   ⇒ ★ ⇒ ★ `final_data` **MUST NOT** 用 `select(we, ...)` 兜底
      //     （★ 多余的一次 mux ⇒ ★ DAG 变大）
      mem.write(dmem_word, merged, we, "dmem_write");
    }
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  /// ★★ **懒创建内存**（★ 创建点 MUST 在 `setup()` 内 ⇒ ★ 此时有 current ctx）
  ch::core::ch_mem<U32, kMemWords>& get_mem() {
    if (!mem_) {
      // ★ `ch_mem` 的初值 MUST 是**整型**（★ `std::vector<uint32_t>`）
      std::vector<std::uint32_t> init(kMemWords, 0);
      mem_ = std::make_unique<ch::core::ch_mem<U32, kMemWords>>(init, "verify_mem");
    }
    return *mem_;
  }

  std::unique_ptr<ch::core::ch_mem<U32, kMemWords>> mem_;
  Ports ports_{};
  std::vector<std::uint8_t> elf_image_;
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_VERIFY_MEM_MODEL_CHMEM_H
