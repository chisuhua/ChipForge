// src/cf_plugin/bridge/l1_cache_bridge.h
//
// 功能描述: L1CacheTLMBridge —— Plugin-style L1Cache 的 ChStream 适配桥接
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 设计依据 (Phase 1.3a, D1' 决策草案 §4):
//   - D1=C: Phase 1.3 保持 cf::bundles::* POD, Bridge 做 4 字段窄桥
//   - D1'=末尾: Bridge tick() 末尾调用 plugin_->pb.run()
//   - D2=B: Bridge 在 src/cf_plugin/bridge/ (框架层, 不受 ip/ D4 检查)
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md
//   - docs/architecture/declarative-hybrid-framework.md:443-447 §4.8 开放问题 1
//
// 阶段范围:
//   - Phase 1.3a (当前): 内部 tick() 末尾调用 pb.run(), 4 字段 test API 转发
//   - Phase 1.3d (后续): cpptlm StreamAdapter 集成 + ModuleFactory JSON

#ifndef CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_H
#define CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_H

#include <memory>

#include "bundles/mem_bundles.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cache/tlm/L1CachePlugin.h"

// 前向声明: cpptlm 类型, 避免在头文件中拉入 CppTLM 依赖
// EventQueue 是全局类 (CppTLM/include/core/event_queue.hh:31, 无 namespace 前缀)
// StreamAdapterBase 是 cpptlm 命名空间内类
class EventQueue;
namespace cpptlm {
class StreamAdapterBase;
}

namespace cf {
namespace plugin {
namespace bridge {

// ─────────────────────────────────────────────────────────────────────
// L1CacheTLMBridge —— Plugin-style L1Cache 的 cpptlm 适配桥接
//
// 生命周期:
//   1. 构造: 持有 unique_ptr<L1CachePlugin>, 在内部 PipeBuilder 注册 + build
//   2. set_stream_adapter(): ModuleFactory 调用, 注入 StreamAdapter
//   3. tick(): EventQueue 调用, 末尾调用 plugin pb.run()
//
// 公开 test API (Phase 1.3a 单元测试用):
//   - issue_request(req): 写入 CacheReq 4 字段到 payload_node_
//   - read_response(): 从 payload_node_ 读取 CacheResp
//
// 不在 Phase 1.3a 范围 (Phase 1.3d 处理):
//   - cpptlm StreamAdapter 内部 ch_stream<CacheReqBundle> ↔ payload_node_ 转换
//   - ModuleFactory + JSON 注册
// ─────────────────────────────────────────────────────────────────────
class L1CacheTLMBridge {
 public:
  // 构造: 接管 plugin 所有权, 在内部 PipeBuilder 注册并 build
  explicit L1CacheTLMBridge(std::unique_ptr<cf::ip::cache::tlm::L1CachePlugin> plugin);

  ~L1CacheTLMBridge();

  L1CacheTLMBridge(const L1CacheTLMBridge&) = delete;
  L1CacheTLMBridge& operator=(const L1CacheTLMBridge&) = delete;

  // ──────────────────────────────────────────────────────────────────
  // cpptlm ModuleFactory 接口 (Phase 1.3d 完善)
  // 当前实现: 仅保存 adapter 指针, 后续 phase 注入 StreamAdapter 转换逻辑
  // ──────────────────────────────────────────────────────────────────
  void set_stream_adapter(cpptlm::StreamAdapterBase* adapter);

  // tick: EventQueue 每周期调用
  //   Phase 1.3a 范围: 末尾调用 plugin pb.run()
  //   Phase 1.3d 范围: 加入 ch_stream 协议转换
  void tick();

  // ──────────────────────────────────────────────────────────────────
  // 单元测试 API (Phase 1.3a TDD)
  // ──────────────────────────────────────────────────────────────────
  void issue_request(const cf::bundles::CacheReq& req);
  cf::bundles::CacheResp read_response() const;

  // pb_run_count: 累计 tick() 调用 pb.run() 的次数
  // 仅用于 Phase 1.3a TDD 验证 D1' 契约 (tick() 末尾调用 pb.run())
  int pb_run_count() const { return pb_run_count_; }

 private:
  cf::plugin::PipeBuilder pb_;
  cf::ip::cache::tlm::L1CachePlugin* plugin_;  // non-owning, lives inside pb_
  std::shared_ptr<cf::plugin::PipeNode> payload_node_;
  cpptlm::StreamAdapterBase* adapter_ = nullptr;
  int pb_run_count_ = 0;
};

}  // namespace bridge
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_BRIDGE_L1_CACHE_BRIDGE_H