// ip/cpu/plugins/dbus.h
//
// 功能描述: DBusPlugin — 数据总线接口 (M2.5, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P0 (ISA-无关): 数据总线是通用 CPU 接口
//   - memory 阶段: 根据 MEM_ADDR/MEM_SIZE 发起读写请求
//   - 返回数据存入 MEM_DATA Payload
//   - M2 阶段: 存根实现, M4 集成 TLM 事务
//
// 约束:
//   - 头文件为主 (.cpp 仅 stub)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_PLUGINS_DBUS_H
#define CF_IP_CPU_PLUGINS_DBUS_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class DBusPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "DBusPlugin<T>: T must be unsigned");

 public:
  DBusPlugin() = default;
  ~DBusPlugin() override = default;

  DBusPlugin(const DBusPlugin&) = delete;
  DBusPlugin& operator=(const DBusPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

    pb.at_stage("memory", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (!n) return;
      const auto& dec = n->operator()(KeyType::DECODE);

      if (dec.op_class == cf::cpu::core::payload::DecodePayload::OpClass::LOAD) {
        T addr = n->operator()(KeyType::MEM_ADDR);
        // M2 阶段: 存根, 直接返回 0
        // M4 集成 TLM 后: 发起总线读事务
        n->operator()(KeyType::MEM_DATA) = T{0};
      } else if (dec.op_class == cf::cpu::core::payload::DecodePayload::OpClass::STORE) {
        T addr = n->operator()(KeyType::MEM_ADDR);
        T data = n->operator()(KeyType::MEM_DATA);
        // M2 阶段: 存根
        // M4 集成 TLM 后: 发起总线写事务
      }
    });
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_DBUS_H
