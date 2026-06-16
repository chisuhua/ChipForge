// ip/cpu/plugins/ibus.h
//
// 功能描述: IBusPlugin — 指令总线接口 (M2.4, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P0 (ISA-无关): 指令总线是通用 CPU 接口
//   - fetch 阶段: 根据 PC 发起指令读取请求
//   - 返回指令字存入 INSTRUCTION Payload
//   - M2 阶段: 存根实现, M4 集成 TLM 事务
//
// 约束:
//   - 头文件为主 (.cpp 仅 stub)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_PLUGINS_IBUS_H
#define CF_IP_CPU_PLUGINS_IBUS_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class IBusPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "IBusPlugin<T>: T must be unsigned");

 public:
  IBusPlugin() = default;
  ~IBusPlugin() override = default;

  IBusPlugin(const IBusPlugin&) = delete;
  IBusPlugin& operator=(const IBusPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (!n) return;
      T pc = n->operator()(KeyType::PC);

      // M2 阶段: 存根, 直接返回假指令
      // M4 集成 TLM 后: 发起总线事务, 等待响应
      n->operator()(KeyType::INSTRUCTION) = cf::plugin::uint_t<32>(0x00000013);  // NOP
    });
  }

  // 测试辅助: 手动设置指令
  void set_instruction(std::uint32_t inst) { next_instruction_ = inst; }

 private:
  std::uint32_t next_instruction_ = 0x00000013;  // NOP
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_IBUS_H
