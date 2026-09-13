// src/cf_plugin/bridge/mmu_bridge_adapter.h
//
// 功能描述: MMUTLMBridgeAdapter —— MMUTLMBridge 的 cpptlm 适配包装 (mmu-cache-integration commit 4/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 设计动机:
//   mirror src/cf_plugin/bridge/l1_cache_bridge_adapter.h 模式
//   - cpptlm::ModuleFactory::registerObject<T> 要求 (string, EventQueue*)
//   - Adapter 是薄包装: 创建默认 MMUPlugin + MMUTLMBridge, 暴露 ModuleFactory 兼容接口
//
// 范围限制 (Oracle Q3 决策: 独立 adapter, 不做统一 MMUCacheBridgeAdapter):
//   - Adapter 内部使用默认 MMUPlugin 几何 (2-level TLB 8/8 + LRU + Sv39)
//   - 4 字段窄桥: TlbReqBundle/TlbRespBundle 字段映射 (transaction_id/vaddr/asid/access_type)
//   - req_out/resp_in 静态 dummy (被动响应模块, 与 l1_cache_bridge_adapter.h:105-112 对称)

#ifndef CF_PLUGIN_BRIDGE_MMU_BRIDGE_ADAPTER_H
#define CF_PLUGIN_BRIDGE_MMU_BRIDGE_ADAPTER_H

#include <memory>
#include <string>

#include "bundles/tlb_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"

namespace cf {
namespace plugin {
namespace bridge {

class MMUTLMBridge;

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

class EventQueue;
namespace cpptlm { class StreamAdapterBase; }

namespace cf {
namespace plugin {
namespace bridge {

// MMUTLMBridgeAdapter —— ChStreamModuleBase 子类, ModuleFactory 兼容
class MMUTLMBridgeAdapter : public ::ChStreamModuleBase {
 public:
  explicit MMUTLMBridgeAdapter(const std::string& name, ::EventQueue* eq);

  ~MMUTLMBridgeAdapter() override = default;

  MMUTLMBridgeAdapter(const MMUTLMBridgeAdapter&) = delete;
  MMUTLMBridgeAdapter& operator=(const MMUTLMBridgeAdapter&) = delete;

  // ChStreamModuleBase 接口
  void set_stream_adapter(::cpptlm::StreamAdapterBase* adapter) override;

  // SimObject 接口
  void tick() override;

  // ch_stream 访问器 (供 cpptlm::StreamAdapter<ModuleT, ...> 使用)
  ::cpptlm::InputStreamAdapter<::bundles::TlbReqBundle>& req_in() {
    return req_in_;
  }
  ::cpptlm::OutputStreamAdapter<::bundles::TlbRespBundle>& resp_out() {
    return resp_out_;
  }
  // 被动响应模块: req_out/resp_in 返回静态 dummy (与 l1_cache_bridge_adapter 一致)
  ::cpptlm::OutputStreamAdapter<::bundles::TlbReqBundle>& req_out() {
    static ::cpptlm::OutputStreamAdapter<::bundles::TlbReqBundle> dummy;
    return dummy;
  }
  ::cpptlm::InputStreamAdapter<::bundles::TlbRespBundle>& resp_in() {
    static ::cpptlm::InputStreamAdapter<::bundles::TlbRespBundle> dummy;
    return dummy;
  }

  // 单元测试 API
  MMUTLMBridge* bridge() const { return bridge_.get(); }

 private:
  std::unique_ptr<MMUTLMBridge> bridge_;
  ::cpptlm::StreamAdapterBase* adapter_ = nullptr;

  ::cpptlm::InputStreamAdapter<::bundles::TlbReqBundle>   req_in_;
  ::cpptlm::OutputStreamAdapter<::bundles::TlbRespBundle> resp_out_;
};

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_BRIDGE_MMU_BRIDGE_ADAPTER_H
