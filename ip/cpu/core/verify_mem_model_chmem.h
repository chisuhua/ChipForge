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
    // ── ★★★ **观测端口**（★ ADR-091 v1.11 §D3a.6 + v1.12 §D3a.9）──
    // ★ `mem_sig` / `mem_sig_updates` = ★ **DUT 侧自算**的内存签名
    //   （★ 折叠算法 MUST 与预载核 `dmem_chmem.h:244-270` **逐位同款**）
    // ★ ★ 载体 MUST 是**顶层 `ch_out`**（★ H-2：★ Verilator 后端
    //   `port_access_` 只认顶层端口 ⇒ ★ MUST NOT 依赖 `--public-flat-rw` 穿透）
    // ★ ★ 名字 MUST 与预载核**同名**（★ `mem_sig` / `mem_sig_updates`）
    //   ⇒ ★ parity runner 的 `resolve_node_id` 两侧都能命中（★ 零改动）
    ch::core::ch_out<U32>* mem_sig = nullptr;
    ch::core::ch_out<U32>* mem_sig_updates = nullptr;
    // ── ★ **`tohost` 探针**（★ ADR-091 v1.12 §D3a.9）──
    // ★ 归**从设备**（★ 开放核**不持有内存** ⇒ 核侧无法 `aread`）
    // ★ ★ **契约名 = `tohost_probe_data_proxy`**（★ 与预载核
    //   `dmem_chmem.h:278` 的 `mem.aread(probe_addr, "tohost_probe")`
    //   生成的**内部** proxy 节点名**完全一致**）⇒ ★
    //   ⇒ ★ **parity runner 的 `resolve_node_id` 两侧都命中** ⇒ ★ **零改动** ✔
    // ★ ★ 载体 MUST 是**顶层 `ch_out`**（★ H-2）⇒ ★ 而非内部 proxy
    ch::core::ch_out<U32>* tohost_probe = nullptr;
  };

  void bind_ports(const Ports& p) { ports_ = p; }

  /// ★★ **ELF 镜像钩子**（★ A5-2 的入口）—— ★ 本版本**只存储，不解析**
  ///   ⇒ ★ 诚实标注：★ **ELF 加载尚未实现**（★ tasks 4.2 的另一半）
  void set_elf_image(const std::vector<std::uint8_t>& bytes) {
    elf_image_ = bytes;
  }

  /// ★★ **`tohost` 探针的字索引**（★ 与预载核 `dmem_chmem.h:57` **同款 API**）
  ///   ⇒ ★ 预载核由 `cpu_factory_chmem.h:333` 设 `0x400`（★ 字节 0x1000）
  void set_tohost_probe_word(std::size_t word) {
    tohost_probe_word_ = word;
    tohost_probe_enabled_ = true;
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

      // ★★★ **`memsig` 折叠**（★ ADR-091 v1.11 §D3a.6）
      //   ★ MUST 与预载核 `ip/cpu/plugins/dmem_chmem.h:244-270` **逐位同款**：
      //     mix      = word_addr * 31 + store_wdata + sig_upd * 0x9E3779B1
      //     sig_next = is_store_active ? ((sig * 16777619) ^ mix) : sig
      //     sig_upd += is_store_active
      //   ★ ★ `store_wdata` = **RMW 后的最终字**（★ 上面的 `merged`）
      //   ★ ★ `is_store_active` = 写使能（★ 开放核的 flush 抑制**在核侧**
      //     ⇒ ★ `dmem_req_we` 已含抑制 ⇒ ★ 从设备无需再判 flush）
      //   ★ ★ **只观测、不参与 datapath** ⇒ ★ 零 cycle 影响
      {
        auto& sig = get_mem_sig();
        auto& sig_upd = get_mem_sig_updates();
        // ★★★ **必须显式取值**（★ 预载核注释点名的坑）：
        //   ★ 直接写 `sig * X` ⇒ ★ 走 ch_uint 的**编译期值**路径（恒 0）
        //   ⇒ ★ 整条表达式退化成常量 ⇒ ★ **reg 无驱动逻辑**
        //   ⇒ ★ **被 codegen 裁掉**（★ Verilog 里根本没有 `mem_sig`）
        ch::core::ch_uint<32> sig_val(sig);
        ch::core::ch_uint<32> sig_upd_val(sig_upd);

        // ★ `dmem_word` = `bits<15,2>(byte_addr)`（★ 14 bit）⇒ ★ 与预载核的
        //   `word_addr` **同宽同义** ⇒ ★ 折叠逐位同款 ✔
        const auto mix =
            (dmem_word * ch::core::ch_uint<32>(ch::core::ch_literal<31, 32>{})) +
            ch::core::ch_uint<32>(merged) +
            (sig_upd_val * ch::core::ch_uint<32>(
                 ch::core::ch_literal<0x9E3779B1, 32>{}));
        const auto sig_next = ch::core::ch_uint<32>(ch::core::select(
            we, (sig_val * ch::core::ch_uint<32>(
                     ch::core::ch_literal<16777619, 32>{})) ^ mix,
            sig_val));
        sig <<= sig_next;
        sig_upd <<= ch::core::ch_uint<32>(ch::core::select(
            we, sig_upd_val + ch::core::ch_uint<32>(ch::core::ch_literal<1, 32>{}),
            sig_upd_val));

        // ★ 顶层观测端口（★ 名 `mem_sig` / `mem_sig_updates` —— 契约名）
        if (ports_.mem_sig) *ports_.mem_sig = ch::core::ch_uint<32>(sig);
        if (ports_.mem_sig_updates) {
          *ports_.mem_sig_updates = ch::core::ch_uint<32>(sig_upd);
        }
      }

      // ★★★ **`tohost` 探针**（★ ADR-091 v1.12 §D3a.9）
      //   ★ 预载核同款：`mem.aread(probe_addr, "tohost_probe")`
      //   ⇒ ★ 生成的**内部** proxy 节点名 = `"tohost_probe" + "_data_proxy"`
      //   ★ ★★ 本实现的**内部** proxy 用 `tohost_probe_internal`（★ 避开同名）
      //     ⇒ ★ **顶层端口**才能用契约名 `tohost_probe_data_proxy`
      //     ⇒ ★ 否则同名 ⇒ ★ CppHDL 唯一化把端口改名 ⇒ ★ **破契约**（★ G-1o）
      if (tohost_probe_enabled_ && ports_.tohost_probe && !tohost_probe_port_) {
        const auto probe_addr = ch::core::ch_uint<14>(
            static_cast<std::uint32_t>(tohost_probe_word_));
        tohost_probe_port_ = std::make_unique<
            ch::core::ch_mem<U32, kMemWords>::read_port>(
            mem.aread(probe_addr, "tohost_probe_internal"));
      }
      if (ports_.tohost_probe && tohost_probe_port_) {
        *ports_.tohost_probe = ch::core::ch_uint<32>(tohost_probe_port_->impl());
      }
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

  /// ★★ **内部签名寄存器**（★ 名带 `open_` 前缀 —— G-1o：★ 不与顶层
  ///   观测端口名 `mem_sig` / `mem_sig_updates` **撞名**）
  ///   ⇒ ★ 契约名由**顶层端口**承担 ⇒ ★ `resolve_node_id` 仍能命中 ✔
  ch::core::ch_reg<U32>& get_mem_sig() {
    if (!mem_sig_) {
      mem_sig_ = std::make_unique<ch::core::ch_reg<U32>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "open_mem_sig");
    }
    return *mem_sig_;
  }
  ch::core::ch_reg<U32>& get_mem_sig_updates() {
    if (!mem_sig_updates_) {
      mem_sig_updates_ = std::make_unique<ch::core::ch_reg<U32>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}),
          "open_mem_sig_updates");
    }
    return *mem_sig_updates_;
  }

  std::unique_ptr<ch::core::ch_mem<U32, kMemWords>> mem_;
  std::unique_ptr<ch::core::ch_reg<U32>> mem_sig_;
  std::unique_ptr<ch::core::ch_reg<U32>> mem_sig_updates_;
  /// ★ `tohost` 探针的读端口（★ 组合 `aread` ⇒ 0 拍）
  std::unique_ptr<ch::core::ch_mem<U32, kMemWords>::read_port> tohost_probe_port_;
  std::size_t tohost_probe_word_ = 0;
  bool tohost_probe_enabled_ = false;
  Ports ports_{};
  std::vector<std::uint8_t> elf_image_;
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_VERIFY_MEM_MODEL_CHMEM_H
