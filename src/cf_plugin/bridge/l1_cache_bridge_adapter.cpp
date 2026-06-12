// src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
//
// 功能描述: L1CacheTLMBridgeAdapter 实现 (Phase 1.3d + 1.3d-extras)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-13
//
// Phase 1.3d-extras 增补 (DECISION-2026-06-13-01 F1.A + F2 + F3):
//   - ch_stream 4 字段窄桥 (F1.A): tick() 时, 如 req_in_.valid() && req_in_.ready(),
//     提取 addr/data/is_write/id 4 字段 → cf::bundles::CacheReq → bridge_->issue_request()
//   - ChStreamAdapterFactory 静态注册 (F2): "L1CacheTLMBridgeAdapter" →
//     ::bundles::CacheReqBundle/RespBundle (4 字段窄桥, op/burst_len/fragment_* 走 default)
//   - full JSON instantiateAll e2e (F3): test_l1_cache_json_instantiate 验证路径打通

#include "cf_plugin/bridge/l1_cache_bridge_adapter.h"

#include <memory>

#include "bundles/cache_bundles_tlm.hh"
#include "bundles/mem_bundles.h"
#include "core/event_queue.hh"
#include "core/stream_adapter_base.hh"
#include "framework/chstream_adapter_factory.hh"
#include "ip/cache/tlm/L1CachePlugin.h"

#include "cf_plugin/bridge/l1_cache_bridge.h"

namespace cf {
namespace plugin {
namespace bridge {

L1CacheTLMBridgeAdapter::L1CacheTLMBridgeAdapter(const std::string& name,
                                                 ::EventQueue* eq)
    : ::ChStreamModuleBase(name, eq),
      bridge_(nullptr),
      adapter_(nullptr),
      req_in_(),
      resp_out_() {
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
  // ch_stream → Bridge POD 4 字段窄桥 (F1.A, DECISION-2026-06-13-01)
  if (req_in_.valid() && req_in_.ready()) {
    const auto& ch_req = req_in_.data();
    cf::bundles::CacheReq pod_req{};
    pod_req.address  = static_cast<uint64_t>(ch_req.address.read());
    pod_req.data     = static_cast<uint64_t>(ch_req.data.read());
    pod_req.is_write = static_cast<bool>(ch_req.is_write.read());
    // id 字段: ch_stream 优先用 transaction_id (4 字段窄桥), 无 transaction_id 时
    // 走 default 0 (R6 风险, Phase 2+ 评估)
    pod_req.id       = static_cast<uint8_t>(ch_req.transaction_id.read() & 0xFF);
    if (bridge_) {
      bridge_->issue_request(pod_req);
    }
    req_in_.consume();
  }

  // Bridge tick (D1' 契约: 末尾调 plugin pb.run())
  if (bridge_) {
    bridge_->tick();
  }

  // Bridge POD → ch_stream 响应 (F1.A 对称, 4 字段窄桥)
  if (bridge_ && !resp_out_.valid()) {
    cf::bundles::CacheResp pod_resp = bridge_->read_response();
    ::bundles::CacheRespBundle ch_resp{};
    ch_resp.data.write(static_cast<uint64_t>(pod_resp.data));
    ch_resp.is_hit.write(static_cast<bool>(pod_resp.hit));
    // error (POD 1-bit bool) → error_code (ch_uint<8>, 0=OK, 1=error)
    ch_resp.error_code.write(static_cast<uint8_t>(pod_resp.error ? 1 : 0));
    ch_resp.transaction_id.write(static_cast<uint64_t>(pod_resp.id));
    ch_resp.parent_id.write(static_cast<uint64_t>(0));
    ch_resp.fragment_id.write(static_cast<uint64_t>(0));
    ch_resp.fragment_total.write(static_cast<uint64_t>(1));
    ch_resp.first.write(true);
    ch_resp.last.write(true);
    resp_out_.write(ch_resp);
  }
}

// ──────────────────────────────────────────────────────────────────────
// ChStreamAdapterFactory 静态注册 (Phase 1.3d-extras F2)
//
// 触发时机: Adapter .cpp 加载时 (ModuleFactory 链接自动包含)
// 注册内容: "L1CacheTLMBridgeAdapter" → factory 函数, 构造
//           cpptlm::StreamAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>
//           (4 字段窄桥 addr/data/is_write/id; op/burst_len/parent_id/fragment_* 走 default)
// 范围限制: 仅 L1Cache 拓扑; 通用 StreamAdapter 模板推迟 Phase 5
//
// 注意: ::bundles 前缀 (全局), 避免与 cf::bundles (ChipForge POD 命名空间) 冲突
// ──────────────────────────────────────────────────────────────────────
namespace {
struct L1CacheBridgeAutoRegister {
  L1CacheBridgeAutoRegister() {
    ChStreamAdapterFactory::get().registerAdapter<
        L1CacheTLMBridgeAdapter,
        ::bundles::CacheReqBundle,
        ::bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter");
  }
};
static L1CacheBridgeAutoRegister l1_cache_bridge_auto_register_;
}  // namespace

}  // namespace bridge
}  // namespace plugin
}  // namespace cf