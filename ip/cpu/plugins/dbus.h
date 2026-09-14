// ip/cpu/plugins/dbus.h
//
// 功能描述: DBusPlugin — 数据总线接口 (M2.5, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-14
//
// 设计:
//   - P0 (ISA-无关): 数据总线是通用 CPU 接口
//   - memory 阶段: 根据 MEM_ADDR/MEM_SIZE 发起读写请求
//   - 返回数据存入 MEM_DATA Payload
//   - M2 阶段: 存根 (LOAD 返 0, STORE no-op)
//   - cpu-pipeline-stubs-replace commit D: 注入 PicolibcHostMemory* 后真实 read/write_word
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
#include "ip/cpu/picolibc_host_memory.h"

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class DBusPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "DBusPlugin<T>: T must be unsigned");

 public:
  // cpu-pipeline-stubs-replace commit D: 注入 PicolibcHostMemory* 真实访存.
  // mem=nullptr 时维持旧 stub 行为 (LOAD 返 0, STORE no-op).
  DBusPlugin() = default;
  explicit DBusPlugin(PicolibcHostMemory* mem) : mem_(mem) {}

  ~DBusPlugin() override = default;

  DBusPlugin(const DBusPlugin&) = delete;
  DBusPlugin& operator=(const DBusPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

    pb.at_stage("memory", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (n) {
        const auto& dec = n->operator()(KeyType::DECODE);

        if (dec.op_class == cf::cpu::core::payload::DecodePayload::OpClass::LOAD) {
          T addr = n->operator()(KeyType::MEM_ADDR);
          // cpu-pipeline-stubs-replace commit D: 真实 read_word (mem 非空时)
          n->operator()(KeyType::MEM_DATA) =
              mem_ ? T(mem_->read_word(static_cast<std::uint64_t>(addr))) : T{0};
        } else if (dec.op_class == cf::cpu::core::payload::DecodePayload::OpClass::STORE) {
          T addr = n->operator()(KeyType::MEM_ADDR);
          T data = n->operator()(KeyType::MEM_DATA);
          // cpu-pipeline-stubs-replace commit D: 真实 write_word (mem 非空时)
          if (mem_) mem_->write_word(static_cast<std::uint64_t>(addr),
                                     static_cast<std::uint32_t>(data));
        }
      }
    });
  }

 private:
  PicolibcHostMemory* mem_ = nullptr;
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_DBUS_H
