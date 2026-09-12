// src/cf_plugin/bridge/mmu_bridge.cpp
//
// 功能描述: MMUTLMBridge 实现 (mmu-tlb-ptw-impl commit 9b)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-07-02

#include "cf_plugin/bridge/mmu_bridge.h"

#include "core/stream_adapter_base.hh"
#include "ip/mmu/tlm/MMUPlugin.h"

namespace cf {
namespace plugin {
namespace bridge {

MMUTLMBridge::MMUTLMBridge(std::unique_ptr<cf::ip::mmu::MMUPlugin> plugin)
    : pb_(),
      plugin_(plugin.get()),
      payload_node_(nullptr),
      adapter_(nullptr) {
  pb_.register_plugin(std::move(plugin));
  pb_.build();
  payload_node_ = pb_.node_of_logic_stage("tlb_lookup_ifetch");
}

MMUTLMBridge::~MMUTLMBridge() = default;

void MMUTLMBridge::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
  adapter_ = adapter;
}

void MMUTLMBridge::tick() {
  if (adapter_) {
    adapter_->tick();
  }
  pb_.run();
  pb_run_count_++;
}

void MMUTLMBridge::issue_request(const ::bundles::TlbReqBundle& req) {
  if (!plugin_ || !payload_node_) return;
  // MMUPlugin 通过 at_stage() 闭包驱动, 这里只做 4 字段窄桥 stub.
  // 完整 issue_request 实装推迟到 mmu-cache-integration change.
  (void)req;
}

::bundles::TlbRespBundle MMUTLMBridge::read_response() const {
  if (!plugin_ || !payload_node_) return {};
  (void)plugin_;
  (void)payload_node_;
  return {};
}

}  // namespace bridge
}  // namespace plugin
}  // namespace cf
