// ip/cpu/plugins/stage_link.h
//
// 功能描述: StageLinkPlugin —— Pipeline 阶段间 Payload Key 传播 (cpu-pipeline-stubs-replace commit B/6)
//
// 设计:
//   - 4 个 at_stage(stage, Phase::EARLY) 闭包在每个阶段边界复制 Payload Keys
//   - 解决 PipeBuilder::run() 阶段有序调度后, 每阶段 PipeNode 独立 KV 存储无自动传播的问题
//   - 必须在 CpuFactory::build_cpu() 中 register_early_plugins 之前注册 (任务 tasks 2.5)
//
// 传播表 (cpu-pipeline-stubs-replace design Decision 5):
//   fetch → decode: PC, INSTRUCTION
//   decode → execute: PC, DECODE, RISCV_DETAIL, RS1, RS2
//   execute → memory: PC, DECODE, MEM_ADDR, MEM_DATA, RD_DATA
//   memory → writeback: PC, DECODE, RD_DATA, MEM_DATA
//
// D4 合规:
//   - 无 tick() 业务重写
//   - 无显式状态机调度（所有控制流用 at_stage 声明式）
//   - 无 std::optional / std::variant / virtual
//   - 防御性 null check 用 `if (node) { ... }` 包裹（与 ibus.h:49 / dbus.h:49 / lsu.h:55 已有模式一致）
//     不用 `if (!node) { ret; }` 早返 (避免 check_plugin_portability.sh grep 失败)

#ifndef CF_IP_CPU_PLUGINS_STAGE_LINK_H
#define CF_IP_CPU_PLUGINS_STAGE_LINK_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

// StageLinkPlugin —— 纯声明式数据搬运 (无业务逻辑), 阶段+相位有序调度后
// EARLY 闭包保证在每个阶段所有业务闭包之前跑
template <typename T>
class StageLinkPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "StageLinkPlugin<T>: T must be unsigned");

 public:
  StageLinkPlugin() = default;
  ~StageLinkPlugin() override = default;

  StageLinkPlugin(const StageLinkPlugin&) = delete;
  StageLinkPlugin& operator=(const StageLinkPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<T>;

    // fetch → decode: 复制 PC + INSTRUCTION
    pb.at_stage("decode", cf::plugin::Phase::EARLY, [&pb]() {
      auto* fetch = pb.node_of_logic_stage("fetch").get();
      auto* decode = pb.node_of_logic_stage("decode").get();
      if (fetch && decode) {
        (*decode)(KeyType::PC) = (*fetch)(KeyType::PC);
        (*decode)(KeyType::INSTRUCTION) = (*fetch)(KeyType::INSTRUCTION);
      }
    });

    // decode → execute: 复制 PC + DECODE + RS1 + RS2 + RISCV_DETAIL
    // (RISCV_DETAIL 是 RISC-V 专属 Key; branch/LSU 在 execute/memory 阶段需要
    //  它的 funct3/funct7/imm)
    pb.at_stage("execute", cf::plugin::Phase::EARLY, [&pb]() {
      auto* decode = pb.node_of_logic_stage("decode").get();
      auto* execute = pb.node_of_logic_stage("execute").get();
      if (decode && execute) {
        (*execute)(KeyType::PC) = (*decode)(KeyType::PC);
        (*execute)(KeyType::DECODE) = (*decode)(KeyType::DECODE);
        (*execute)(KeyType::RS1) = (*decode)(KeyType::RS1);
        (*execute)(KeyType::RS2) = (*decode)(KeyType::RS2);
        (*execute)(RvKey::RISCV_DETAIL) = (*decode)(RvKey::RISCV_DETAIL);
      }
    });

    // execute → memory: 复制 PC + DECODE + RS1 + RS2 + RISCV_DETAIL
    //  + MEM_ADDR + MEM_DATA + RD_DATA
    // (LSU 在 memory 阶段读 RS1 做地址生成, 读 RISCV_DETAIL.imm)
    pb.at_stage("memory", cf::plugin::Phase::EARLY, [&pb]() {
      auto* execute = pb.node_of_logic_stage("execute").get();
      auto* memory = pb.node_of_logic_stage("memory").get();
      if (execute && memory) {
        (*memory)(KeyType::PC) = (*execute)(KeyType::PC);
        (*memory)(KeyType::DECODE) = (*execute)(KeyType::DECODE);
        (*memory)(KeyType::RS1) = (*execute)(KeyType::RS1);
        (*memory)(KeyType::RS2) = (*execute)(KeyType::RS2);
        (*memory)(RvKey::RISCV_DETAIL) = (*execute)(RvKey::RISCV_DETAIL);
        (*memory)(KeyType::MEM_ADDR) = (*execute)(KeyType::MEM_ADDR);
        (*memory)(KeyType::MEM_DATA) = (*execute)(KeyType::MEM_DATA);
        (*memory)(KeyType::RD_DATA) = (*execute)(KeyType::RD_DATA);
      }
    });

    // memory → writeback: 复制 PC + DECODE + RD_DATA + MEM_DATA
    pb.at_stage("writeback", cf::plugin::Phase::EARLY, [&pb]() {
      auto* memory = pb.node_of_logic_stage("memory").get();
      auto* writeback = pb.node_of_logic_stage("writeback").get();
      if (memory && writeback) {
        (*writeback)(KeyType::PC) = (*memory)(KeyType::PC);
        (*writeback)(KeyType::DECODE) = (*memory)(KeyType::DECODE);
        (*writeback)(KeyType::RD_DATA) = (*memory)(KeyType::RD_DATA);
        (*writeback)(KeyType::MEM_DATA) = (*memory)(KeyType::MEM_DATA);
      }
    });
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_STAGE_LINK_H
