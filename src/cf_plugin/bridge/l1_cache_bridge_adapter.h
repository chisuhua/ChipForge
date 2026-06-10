// src/cf_plugin/bridge/l1_cache_bridge_adapter.h
//
// 功能描述: L1CacheTLMBridgeAdapter —— L1CacheTLMBridge 的 cpptlm 适配包装
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 设计动机 (Phase 1.3d):
//   cpptlm::ModuleFactory::registerObject<T> 要求 T 构造签名为 (string, EventQueue*),
//   但 L1CacheTLMBridge 构造签名为 (unique_ptr<L1CachePlugin>) —— 不兼容。
//   Adapter 是薄包装: 继承 ChStreamModuleBase, 内部创建默认 L1CachePlugin +
//   L1CacheTLMBridge, 暴露 ModuleFactory 兼容接口。
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §4 (D1' 契约)
//   - CppTLM/include/core/chstream_module.hh (ChStreamModuleBase API)
//   - CppTLM/include/core/module_factory.hh:69-78 (registerObject 签名约束)

#ifndef CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H
#define CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H

#include <memory>
#include <string>

#include "core/chstream_module.hh"

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
//   2. set_stream_adapter(adapter): ModuleFactory 注入适配器 (Phase 1.3d-extras 预留)
//   3. tick(): EventQueue 每周期调用, 委托给 bridge_.tick()
//
// 限制 (Phase 1.3d 范围):
//   - Adapter 内部使用默认 L1CachePlugin 几何 (256 sets, 64B line)
//   - 不支持从 JSON params 读取几何参数 (Phase 1.3d-extras 扩展)
//   - set_stream_adapter 仅存指针, ch_stream 协议转换未实现
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
  // 单元测试 API (供 e2e test 验证 Bridge 真的存在)
  // ──────────────────────────────────────────────────────────────────
  L1CacheTLMBridge* bridge() const { return bridge_.get(); }

 private:
  std::unique_ptr<L1CacheTLMBridge> bridge_;
  ::cpptlm::StreamAdapterBase* adapter_ = nullptr;
};

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_ADAPTER_H