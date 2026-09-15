// ip/cpu/plugins/ibus.h
//
// 功能描述: IBusPlugin — 指令总线接口 (M2.4, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-14
//
// 设计:
//   - P0 (ISA-无关): 指令总线是通用 CPU 接口
//   - fetch 阶段: 根据 PC 发起指令读取请求
//   - 返回指令字存入 INSTRUCTION Payload
//   - M2 阶段: 存根实现, 默认 NOP (cpu-pipeline-stubs-replace commit C 保留向后兼容)
//   - cpu-pipeline-stubs-replace commit C: 注入 PicolibcHostMemory* 后真实 read_word 取指
//
// 约束:
//   - 头文件为主 (.cpp 仅 stub)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_PLUGINS_IBUS_H
#define CF_IP_CPU_PLUGINS_IBUS_H

#include <cstdint>
#include <memory>
#include <type_traits>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "ip/mmu/tlm/mmu_keys.h"

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class IBusPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "IBusPlugin<T>: T must be unsigned");

 public:
  // cpu-pipeline-stubs-replace commit C: 注入 PicolibcHostMemory* 真实取指.
  // mem=nullptr 时维持旧 NOP stub 行为, 保持 tests/cpu/test_ibus.cpp 兼容.
  IBusPlugin() = default;
  explicit IBusPlugin(PicolibcHostMemory* mem) : mem_(mem) {}

  ~IBusPlugin() override = default;

  IBusPlugin(const IBusPlugin&) = delete;
  IBusPlugin& operator=(const IBusPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

    // plugin-framework-stall commit B: register fetch CtrlLink (PTW-busy halt)
    // halt when tlb_lookup_ifetch node has PTW_ACTIVE=1 (PTW walk in progress).
    // shared_ptr holds the lambda (no need for a member ctrl_link).
    auto fetch_ctrl = std::make_shared<cf::plugin::CtrlLink>();
    fetch_ctrl->halt_when([&pb]() {
      auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
      if (!n) return false;
      return static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE));
    });
    pb.register_ctrl_link("fetch", fetch_ctrl);

    // fetch 阶段 NORMAL: 取指 (mem 非空时真读; 否则 NOP stub)
    // 当 PTW busy 时, 框架 stall loop 会 skip 本闭包 (不调), 保持 PC 不推进.
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (n) {
        T pc = n->operator()(KeyType::PC);
        const cf::plugin::uint_t<32> inst =
            mem_ ? cf::plugin::uint_t<32>(mem_->read_word(static_cast<std::uint64_t>(pc)))
                 : cf::plugin::uint_t<32>(0x00000013u);
        n->operator()(KeyType::INSTRUCTION) = inst;
      }
    });

    // writeback 阶段 LATE: PC 更新 (cpu-pipeline-stubs-replace commit C)
    // 单 pass 语义下, 分支决策在 execute 已完成 (DECODE.branch_taken/target),
    // 写回阶段计算新 PC 供下一轮取指. **PC 是循环携带值** — 必须写回 fetch 节点
    // (fetch 每次从 fetch 节点读 PC); writeback 节点只是当轮快照.
    pb.at_stage("writeback", cf::plugin::Phase::LATE, [&pb]() {
      auto* wb = pb.node_of_logic_stage("writeback").get();
      auto* fetch = pb.node_of_logic_stage("fetch").get();
      if (wb && fetch) {
        const auto& dec = wb->operator()(KeyType::DECODE);
        T pc = wb->operator()(KeyType::PC);
        const bool taken = dec.branch_taken;
        const T target = static_cast<T>(dec.branch_target);
        fetch->operator()(KeyType::PC) = taken ? target : static_cast<T>(pc + 4);
      }
    });
  }

  // 测试辅助: 手动设置指令 (cpu-pipeline-stubs-replace commit C 保留向后兼容)
  void set_instruction(std::uint32_t inst) { next_instruction_ = inst; }

 private:
  PicolibcHostMemory* mem_ = nullptr;
  std::uint32_t next_instruction_ = 0x00000013;  // NOP (default for legacy test)
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_IBUS_H
