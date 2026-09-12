// src/cf_plugin/bridge/mmu_bridge.h
//
// 功能描述: MMUTLMBridge —— Plugin-style MMU 的 ChStream 适配桥接 (mmu-tlb-ptw-impl commit 9b)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-07-02
//
// 设计依据: 镜像 src/cf_plugin/bridge/l1_cache_bridge.h (Phase 1.3a, D1' 决策)
//   - D1'=末尾: Bridge tick() 末尾调用 plugin_->pb.run()
//   - D2=B: Bridge 在 src/cf_plugin/bridge/ (框架层, 不受 ip/ D4 检查)

#ifndef CF_PLUGIN_BRIDGE_MMU_BRIDGE_H
#define CF_PLUGIN_BRIDGE_MMU_BRIDGE_H

#include <memory>

#include "bundles/tlb_bundles_tlm.hh"
#include "cf/plugin/pipe_builder.h"
#include "ip/mmu/tlm/MMUPlugin.h"

class EventQueue;
namespace cpptlm { class StreamAdapterBase; }

namespace cf {
namespace plugin {
namespace bridge {

class MMUTLMBridge {
 public:
  explicit MMUTLMBridge(std::unique_ptr<cf::ip::mmu::MMUPlugin> plugin);
  ~MMUTLMBridge();

  MMUTLMBridge(const MMUTLMBridge&) = delete;
  MMUTLMBridge& operator=(const MMUTLMBridge&) = delete;

  void set_stream_adapter(cpptlm::StreamAdapterBase* adapter);
  void tick();

  // Unit test API: 4 字段窄桥 (vaddr/asid/access_type/transaction_id)
  void issue_request(const ::bundles::TlbReqBundle& req);
  ::bundles::TlbRespBundle read_response() const;

  int pb_run_count() const { return pb_run_count_; }

 private:
  cf::plugin::PipeBuilder pb_;
  cf::ip::mmu::MMUPlugin* plugin_;
  std::shared_ptr<cf::plugin::PipeNode> payload_node_;
  cpptlm::StreamAdapterBase* adapter_ = nullptr;
  int pb_run_count_ = 0;
};

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_BRIDGE_MMU_BRIDGE_H
