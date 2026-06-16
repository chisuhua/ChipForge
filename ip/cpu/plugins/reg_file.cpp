// ip/cpu/plugins/reg_file.cpp
//
// 功能描述: RegFilePlugin 实现 (M2.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 实现要点:
//   - 模板参数 T 在编译期由外部实例化 (T = uint32_t / uint64_t)
//   - build() 内注册两个阶段: "decode" (读) 和 "writeback" (写)
//   - 读阶段: 从 DecodePayload.rs1_idx / rs2_idx 取索引, 从 storage 取数据写入 RS1/RS2 Payload
//   - 写阶段: 从 DecodePayload.rd_idx + RD_DATA Payload 读回数据, 写入 storage (x0 屏蔽)
//   - 使用 at_stage() 注册, D4 合规

#include "ip/cpu/plugins/reg_file.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

namespace pl = cf::cpu::core::payload;

// ----------------------------------------------------------------------------
// 显式模板实例化 (编译单元中实例化, 避免重复定义)
// ----------------------------------------------------------------------------
template class RegFilePlugin<std::uint32_t>;
template class RegFilePlugin<std::uint64_t>;

// ----------------------------------------------------------------------------
// build() — 注册 decode (读) 和 writeback (写) 阶段
// ----------------------------------------------------------------------------
template <typename T>
void RegFilePlugin<T>::build(cf::plugin::PipeBuilder& pb) {
  // 读阶段: "decode" 阶段读取 RS1/RS2 数据
  //   从 DecodePayload.rs1_idx / rs2_idx 取索引
  //   从 storage 取数据写入 RS1 / RS2 Payload
  pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      const auto& dec = n->operator()(pl::keys_rv32::DECODE);

      // 读 RS1 (如果指令需要)
      if (dec.reads_rs1) {
        auto rs1_data = this->read_reg(dec.rs1_idx);
        n->put(pl::keys_rv32::RS1, rs1_data);
      }

      // 读 RS2 (如果指令需要)
      if (dec.reads_rs2) {
        auto rs2_data = this->read_reg(dec.rs2_idx);
        n->put(pl::keys_rv32::RS2, rs2_data);
      }
    }
  });

  // 写阶段: "writeback" 阶段写回 RD
  //   从 DecodePayload.rd_idx 取索引, 从 RD_DATA Payload 取数据
  //   写入 storage (x0 屏蔽在 write_reg 内处理)
  pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("writeback").get();
    if (n) {
      const auto& dec = n->operator()(pl::keys_rv32::DECODE);

      if (dec.writes_rd) {
        auto rd_data = n->operator()(pl::keys_rv32::RD_DATA);
        this->write_reg(dec.rd_idx, rd_data);
      }
    }
  });
}

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
