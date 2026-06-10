// src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
//
// 功能描述: L1CacheTLMBridgeAdapter 实现 (Phase 1.3d)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10

#include "cf_plugin/bridge/l1_cache_bridge_adapter.h"

#include <memory>

#include "core/event_queue.hh"
#include "core/stream_adapter_base.hh"
#include "ip/cache/tlm/L1CachePlugin.h"

#include "cf_plugin/bridge/l1_cache_bridge.h"

namespace cf {
namespace plugin {
namespace bridge {

L1CacheTLMBridgeAdapter::L1CacheTLMBridgeAdapter(const std::string& name,
                                                 ::EventQueue* eq)
    : ::ChStreamModuleBase(name, eq),
      bridge_(nullptr),
      adapter_(nullptr) {
  auto plugin = std::make_unique<cf::ip::cache::tlm::L1CachePlugin>();
  bridge_ = std::make_unique<L1CacheTLMBridge>(std::move(plugin));
}

void L1CacheTLMBridgeAdapter::set_stream_adapter(
    ::cpptlm::StreamAdapterBase* adapter) {
  adapter_ = adapter;
  if (bridge_) {
    bridge_->set_stream_adapter(adapter);
  }
}

void L1CacheTLMBridgeAdapter::tick() {
  if (bridge_) {
    bridge_->tick();
  }
}

}  // namespace bridge
}  // namespace plugin
}  // namespace cf