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
  // Bundle→POD 转换: TlbReqBundle ch_uint 字段 → cf::bundles::TlbReq POD 字段
  // (trans_id 和 access_type 在 POD 中无对应字段, 丢弃; mirror l1_cache_bridge.cpp:62-65)
  const uint64_t vaddr = req.vaddr.read();
  const uint16_t asid  = static_cast<uint16_t>(req.asid.read());
  plugin_->issue_request(vaddr, asid);
}

::bundles::TlbRespBundle MMUTLMBridge::read_response() const {
  ::bundles::TlbRespBundle ch_resp{};
  if (!plugin_ || !payload_node_) return ch_resp;
  // POD→Bundle 转换: cf::bundles::TlbResp POD 字段 → TlbRespBundle ch_uint 字段
  const cf::bundles::TlbResp pod = plugin_->read_response(payload_node_);
  ch_resp.transaction_id.write(0);
  ch_resp.paddr.write(static_cast<uint64_t>(pod.paddr));
  ch_resp.hit.write(static_cast<uint8_t>(pod.hit ? 1 : 0));
  ch_resp.perms.write(static_cast<uint8_t>(pod.perms));
  ch_resp.exception_code.write(static_cast<uint8_t>(pod.fault_code));
  return ch_resp;
}

}  // namespace bridge
}  // namespace plugin
}  // namespace cf
