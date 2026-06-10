// src/cf_plugin/bridge/l1_cache_bridge.cpp
//
// 功能描述: L1CacheTLMBridge 实现 (Phase 1.3a)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 设计依据 (Phase 1.3a, D1' 决策草案 §4):
//   - D1=C: 4 字段窄桥 (addr/data/is_write/id)
//   - D1'=末尾: Bridge tick() 末尾调用 pb.run()
//   - D2=B: src/cf_plugin/bridge/ (框架层)
//
// Phase 1.3a 范围:
//   - 构造 + PipeBuilder 注册
//   - tick() 末尾调 pb.run()
//   - 4 字段 test API 转发 (issue_request/read_response)
//   - pb_run_count() 用于 TDD 验证 tick() 真的跑了
//
// Phase 1.3d 范围 (后续):
//   - set_stream_adapter() 实际注入 cpptlm StreamAdapter
//   - ch_stream<CacheReqBundle> ↔ payload_node_ 协议转换
//   - ModuleFactory + JSON 注册

#include "cf_plugin/bridge/l1_cache_bridge.h"

#include "core/stream_adapter_base.hh"
#include "ip/cache/tlm/L1CachePlugin.h"

namespace cf {
namespace plugin {
namespace bridge {

L1CacheTLMBridge::L1CacheTLMBridge(
    std::unique_ptr<cf::ip::cache::tlm::L1CachePlugin> plugin)
    : pb_(),
      plugin_(plugin.get()),
      payload_node_(nullptr),
      adapter_(nullptr) {
  pb_.register_plugin(std::move(plugin));
  pb_.build();
  payload_node_ = pb_.node_of_logic_stage("lookup");
}

L1CacheTLMBridge::~L1CacheTLMBridge() = default;

void L1CacheTLMBridge::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
  adapter_ = adapter;
}

void L1CacheTLMBridge::tick() {
  if (adapter_) {
    adapter_->tick();
  }
  pb_.run();
  pb_run_count_++;
}

void L1CacheTLMBridge::issue_request(const cf::bundles::CacheReq& req) {
  if (!plugin_ || !payload_node_) return;
  plugin_->issue_request(payload_node_, req);
}

cf::bundles::CacheResp L1CacheTLMBridge::read_response() const {
  if (!plugin_ || !payload_node_) return {};
  return plugin_->read_response(payload_node_);
}

}  // namespace bridge
}  // namespace plugin
}  // namespace cf