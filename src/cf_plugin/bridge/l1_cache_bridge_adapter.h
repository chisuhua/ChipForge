// src/cf_plugin/bridge/l1_cache_bridge_adapter.h
//
// 功能描述: L1CacheTLMBridgeAdapter —— L1CacheTLMBridge 的 cpptlm 适配包装
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-13
//
// 设计动机 (Phase 1.3d):
//   cpptlm::ModuleFactory::registerObject<T> 要求 T 构造签名为 (string, EventQueue*),
//   但 L1CacheTLMBridge 构造签名为 (unique_ptr<L1CachePlugin>) —— 不兼容。
//   Adapter 是薄包装: 继承 ChStreamModuleBase, 内部创建默认 L1CachePlugin +
//   L1CacheTLMBridge, 暴露 ModuleFactory 兼容接口。
//
// Phase 1.3d-extras 增补 (DECISION-2026-06-13-01 F1.A + F3):
//   暴露 req_in() / resp_out() ch_stream 访问器 (CacheTLM 风格, 4 字段窄桥):
//   - req_in_:  cpptlm::InputStreamAdapter< ::bundles::CacheReqBundle>
//   - resp_out_: cpptlm::OutputStreamAdapter< ::bundles::CacheRespBundle>
//   ch_stream → Bridge POD 4 字段 (addr/data/is_write/id) 转换在 req_in().process() 时发生
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §4 (D1' 契约)
//   - .omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md (F1-F5)
//   - CppTLM/include/core/chstream_module.hh (ChStreamModuleBase API)
//   - CppTLM/include/core/module_factory.hh:69-78 (registerObject 签名约束)
//   - CppTLM/include/framework/stream_adapter.hh (StreamAdapter<ModuleT, ...> 期望 req_in/resp_out)

#ifndef CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H
#define CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H

#include <memory>
#include <string>

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"

namespace cf {
namespace plugin {
namespace bridge {

// 前向声明
class L1CacheTLMBridge;

}
}
}

class EventQueue;
namespace cpptlm { class StreamAdapterBase; }

namespace cf {
namespace plugin {
namespace bridge {

// ─────────────────────────────────────────────────────────────────────
// L1CacheTLMBridgeAdapter —— ChStreamModuleBase 子类, ModuleFactory 兼容
//
// 生命周期:
//   1. 构造 (string, EventQueue*): 创建默认 L1CachePlugin + L1CacheTLMBridge
//   2. set_stream_adapter(adapter): ModuleFactory 注入适配器 (Phase 1.3d-extras 落地)
//   3. tick(): EventQueue 每周期调用, 委托给 bridge_.tick()
//
// ch_stream 协议 (Phase 1.3d-extras 落地):
//   - req_in():   InputStreamAdapter<CacheReqBundle>&  (StreamAdapter<>::process_request_input 用)
//   - resp_out(): OutputStreamAdapter<CacheRespBundle>& (StreamAdapter<>::tick 用)
//
// 范围限制 (F3 + v2 D3=A):
//   - Adapter 内部使用默认 L1CachePlugin 几何 (256 sets, 64B line)
//   - 不支持从 JSON params 读取几何参数 (Phase 1.3d-extras 显式不做)
//   - 4 字段窄桥: op/burst_len/parent_id/fragment_* 走 CppTLM default 值 (0/false/1)
// ─────────────────────────────────────────────────────────────────────
class L1CacheTLMBridgeAdapter : public ::ChStreamModuleBase {
 public:
  explicit L1CacheTLMBridgeAdapter(const std::string& name,
                                   ::EventQueue* eq);

  ~L1CacheTLMBridgeAdapter() override = default;

  L1CacheTLMBridgeAdapter(const L1CacheTLMBridgeAdapter&) = delete;
  L1CacheTLMBridgeAdapter& operator=(const L1CacheTLMBridgeAdapter&) = delete;

  // ──────────────────────────────────────────────────────────────────
  // ChStreamModuleBase 接口
  // ──────────────────────────────────────────────────────────────────
  void set_stream_adapter(::cpptlm::StreamAdapterBase* adapter) override;

  // ──────────────────────────────────────────────────────────────────
  // SimObject 接口
  // ──────────────────────────────────────────────────────────────────
  void tick() override;

  // ──────────────────────────────────────────────────────────────────
  // ch_stream 访问器 (供 cpptlm::StreamAdapter<ModuleT, ...> 使用)
  // DECISION-2026-06-13-01 F1.A: 4 字段窄桥, addr/data/is_write/id
  // ──────────────────────────────────────────────────────────────────
  ::cpptlm::InputStreamAdapter<::bundles::CacheReqBundle>& req_in() {
    return req_in_;
  }
  ::cpptlm::OutputStreamAdapter<::bundles::CacheRespBundle>& resp_out() {
    return resp_out_;
  }
  // P0-5b 兼容: 被动响应模块 (L1CacheTLMBridgeAdapter 包装 L1CacheTLMBridge,
  //   自身不发起请求/接收响应)。StreamAdapter<>::tick 与 process_request_input
  //   仍会调用 req_out()/resp_in(), 返回静态 dummy (valid=false) 即可 ——
  //   与 cpp-tlm CacheTLM/MemoryTLM 官方模式一致 (cache_tlm.hh:133-141)。
  ::cpptlm::OutputStreamAdapter<::bundles::CacheReqBundle>& req_out() {
    static ::cpptlm::OutputStreamAdapter<::bundles::CacheReqBundle> dummy;
    return dummy;
  }
  ::cpptlm::InputStreamAdapter<::bundles::CacheRespBundle>& resp_in() {
    static ::cpptlm::InputStreamAdapter<::bundles::CacheRespBundle> dummy;
    return dummy;
  }

  // ──────────────────────────────────────────────────────────────────
  // 单元测试 API (供 e2e test 验证 Bridge 真的存在)
  // ──────────────────────────────────────────────────────────────────
  L1CacheTLMBridge* bridge() const { return bridge_.get(); }

 private:
  std::unique_ptr<L1CacheTLMBridge> bridge_;
  ::cpptlm::StreamAdapterBase* adapter_ = nullptr;

  // ch_stream 适配器 (Phase 1.3d-extras 增补)
  ::cpptlm::InputStreamAdapter<::bundles::CacheReqBundle>   req_in_;
  ::cpptlm::OutputStreamAdapter<::bundles::CacheRespBundle> resp_out_;
};

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H