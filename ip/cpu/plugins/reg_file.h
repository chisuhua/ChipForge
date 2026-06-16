// ip/cpu/plugins/reg_file.h
//
// 功能描述: RegFilePlugin — 通用寄存器堆 (M2.1, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 模板参数化 xlen: T = uint32_t (RV32) / uint64_t (RV64)
//   - 32 个寄存器 (x0-x31), 用 cf::plugin::storage::array_store<T, 32>
//   - x0 写屏蔽: 写 x0 时忽略 (硬件行为: x0 恒为 0)
//   - 读透明: 读 x0 返回 0
//   - 输入 Payload: RS1, RS2 (读索引), RD_IDX, RD_DATA (写回)
//   - 输出: 写回内部 storage, 不输出新 Payload
//
// 借鉴:
//   - L1CachePlugin::storage 模式 (array_store<T, N>)
//   - VexRiscv RegFilePlugin (Scala 版, 仅借鉴思路)
//
// 配套决策:
//   - 议题 3 选 B+C: array_store 抽象 (cf_plugin/storage.h)
//   - M1.7: payload_common.h (RS1/RS2/RD_IDX/RD_DATA Key)
//   - D4: 无业务 tick(), 阶段用 at_stage(), 存储用 array_store
//
// 约束:
//   - 头文件为主 (.cpp 仅含模板特例化)
//   - 编译期零开销 (array_store 在 TLM 模式等价 std::array)
//   - 不引入 ch::core::context (TLM 模式禁止)

#ifndef CF_IP_CPU_PLUGINS_REG_FILE_H
#define CF_IP_CPU_PLUGINS_REG_FILE_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/storage.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

// ----------------------------------------------------------------------------
// RegFilePlugin — 通用寄存器堆 (xlen 参数化)
//
// 模板参数 T:
//   T = uint32_t (RV32) 或 uint64_t (RV64)
//   编译期由 CpuFactory 根据 JSON config["xlen"] 实例化
//
// 寄存器数量: 32 (x0-x31), 固定
//   - x0 恒为 0 (写屏蔽 + 读透明)
//   - x1-x31 通用 (调用约定: ra/sp/gp/tp/t0-t6/s0-s11/a0-a7)
//
// 阶段绑定:
//   - 读: "decode" 或 "execute" 阶段 (由流水线配置决定)
//     读 RS1/RS2 索引, 从 storage 取出数据写入 RS1/RS2 Payload
//   - 写: "writeback" 阶段
//     读 RD_IDX + RD_DATA, 写入 storage (x0 屏蔽)
// ----------------------------------------------------------------------------
template <typename T>
class RegFilePlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RegFilePlugin<T>: T must be unsigned (uint32_t or uint64_t)");

 public:
  // 编译期常量
  static constexpr std::size_t kNumRegs = 32;

  explicit RegFilePlugin(T default_value = T{0}) : default_value_(default_value) {
    reset();
  }

  ~RegFilePlugin() override = default;

  RegFilePlugin(const RegFilePlugin&) = delete;
  RegFilePlugin& operator=(const RegFilePlugin&) = delete;

  // PluginBase 接口
  void setup(cf::plugin::PipeBuilder& /*pb*/) override {
    // 跨 Plugin 引用声明 (M2 阶段无依赖, 保留接口)
  }

  // ------------------------------------------------------------------------
  // 单元测试辅助 API (直接操作 storage, 不通过 Payload)
  // ------------------------------------------------------------------------

  // 读寄存器 (直接访问 storage, 测试用)
  T read_reg(std::size_t idx) const {
    T result = T{0};
    if (idx != 0 && idx < kNumRegs) {
      result = regs_[idx];
    }
    return result;
  }

  // 写寄存器 (直接访问 storage, 测试用; x0 屏蔽)
  void write_reg(std::size_t idx, T value) {
    if (idx != 0 && idx < kNumRegs) {
      regs_[idx] = value;
    }
  }

  // 全部置 0 (测试间隔离)
  void reset() {
    for (std::size_t i = 0; i < kNumRegs; ++i) {
      regs_[i] = (i == 0) ? T{0} : default_value_;
    }
  }

  // 检查 x0 是否始终为 0 (不变式验证)
  bool check_x0_zero() const { return regs_[0] == T{0}; }

  // ------------------------------------------------------------------------
  // build() — 注册 decode (读) 和 writeback (写) 阶段
  //   模板方法在头文件中实现, 编译单元内隐式实例化
  // ------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    // 直接使用模板化的 keys (T=uint32_t 时等价 keys_rv32, T=uint64_t 时等价 keys_rv64)
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

    // 读阶段: "decode" 阶段读取 RS1/RS2 数据
    pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("decode").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);

        if (dec.reads_rs1) {
          auto rs1_data = this->read_reg(dec.rs1_idx);
          n->put(KeyType::RS1, rs1_data);
        }

        if (dec.reads_rs2) {
          auto rs2_data = this->read_reg(dec.rs2_idx);
          n->put(KeyType::RS2, rs2_data);
        }
      }
    });

    // 写阶段: "writeback" 阶段写回 RD
    pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("writeback").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);

        if (dec.writes_rd) {
          auto rd_data = n->operator()(KeyType::RD_DATA);
          this->write_reg(dec.rd_idx, rd_data);
        }
      }
    });
  }

  // ------------------------------------------------------------------------
  // 存储类型 (供外部访问, 如测试验证 storage 布局)
  // ------------------------------------------------------------------------
  using RegStore = cf::plugin::storage::array_store<T, kNumRegs>;

 private:
  RegStore regs_{};
  T default_value_;
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_REG_FILE_H
