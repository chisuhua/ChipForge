// src/cf_plugin/bridge/mmu_bridge_adapter.cpp
//
// 功能描述: MMUTLMBridgeAdapter 实现 (mmu-cache-integration commit 4/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// mirror src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp 模式
// - ch_stream 4 字段窄桥 (TlbReqBundle/TlbRespBundle): tick() 时,
//   提取 transaction_id/vaddr/asid/access_type → cf::bundles::TlbReq → bridge_->issue_request()
// - ChStreamAdapterFactory 静态注册: "MMUTLMBridgeAdapter" → TlbReqBundle/TlbRespBundle
// - 范围限制: 独立 adapter (Oracle Q3), 不做统一 MMUCacheBridgeAdapter

#include "cf_plugin/bridge/mmu_bridge_adapter.h"

#include <memory>

#include "bundles/tlb_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "core/stream_adapter_base.hh"
#include "framework/chstream_adapter_factory.hh"

#include "ip/mmu/tlm/MMUPlugin.h"

#include "cf_plugin/bridge/mmu_bridge.h"

namespace cf {
namespace plugin {
namespace bridge {

MMUTLMBridgeAdapter::MMUTLMBridgeAdapter(const std::string& name, ::EventQueue* eq)
    : ::ChStreamModuleBase(name, eq),
      bridge_(nullptr),
      adapter_(nullptr),
      req_in_(),
      resp_out_() {
  // mmu-cache-integration Decision Q3: 独立 adapter, 范围窄, 默认几何
  // 2-level TLB 8/8 + LRU + Sv39 (VIPT-safe, 与 baseline 测试一致)
  using mmu_cfg_t = cf::ip::mmu::MMUPlugin::TLBConfig;
  std::vector<mmu_cfg_t> levels = {
    {"L0", 8, 8, 1, 1, "LRU"},
    {"L1", 8, 8, 1, 2, "LRU"}
  };
  auto plugin = std::make_unique<cf::ip::mmu::MMUPlugin>(
      cf::ip::mmu::SvMode::Sv39, levels, cf::ip::mmu::MMUPlugin::PTWConfig{2});
  bridge_ = std::make_unique<MMUTLMBridge>(std::move(plugin));
}

void MMUTLMBridgeAdapter::set_stream_adapter(::cpptlm::StreamAdapterBase* adapter) {
  adapter_ = adapter;
  if (bridge_) {
    bridge_->set_stream_adapter(adapter);
  }
}

void MMUTLMBridgeAdapter::tick() {
  // ch_stream → Bridge POD 4 字段窄桥
  if (req_in_.valid() && req_in_.ready()) {
    const auto& ch_req = req_in_.data();
    ::bundles::TlbReqBundle pod_req{};
    pod_req.transaction_id.write(ch_req.transaction_id.read());
    pod_req.vaddr.write(ch_req.vaddr.read());
    pod_req.asid.write(static_cast<uint16_t>(ch_req.asid.read()));
    pod_req.access_type.write(ch_req.access_type.read());
    if (bridge_) {
      bridge_->issue_request(pod_req);
    }
    req_in_.consume();
  }

  // Bridge tick (D1' 契约: 末尾调 plugin pb.run())
  if (bridge_) {
    bridge_->tick();
  }

  // Bridge POD → ch_stream 响应
  if (bridge_ && !resp_out_.valid()) {
    ::bundles::TlbRespBundle pod_resp = bridge_->read_response();
    ::bundles::TlbRespBundle ch_resp{};
    ch_resp.transaction_id.write(static_cast<uint64_t>(pod_resp.transaction_id));
    ch_resp.paddr.write(static_cast<uint64_t>(pod_resp.paddr));
    ch_resp.hit.write(static_cast<uint8_t>(pod_resp.hit ? 1 : 0));
    ch_resp.perms.write(static_cast<uint8_t>(pod_resp.perms));
    ch_resp.exception_code.write(static_cast<uint8_t>(pod_resp.exception_code));
    resp_out_.write(ch_resp);
  }
}

// ChStreamAdapterFactory 静态注册 (mirror l1_cache_bridge_adapter.cpp 模式)
namespace {
struct MMUBridgeAutoRegister {
  MMUBridgeAutoRegister() {
    ChStreamAdapterFactory::get().registerAdapter<
        MMUTLMBridgeAdapter,
        ::bundles::TlbReqBundle,
        ::bundles::TlbRespBundle>("MMUTLMBridgeAdapter");
  }
};
static MMUBridgeAutoRegister mmu_bridge_auto_register_;
}  // namespace

}  // namespace bridge
}  // namespace plugin
}  // namespace cf
