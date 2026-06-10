// ip/cache/tlm/L1CachePlugin.h
//
// 功能描述: L1 Cache Plugin (Phase 1.2) —— 第一个 Plugin-style IP
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 设计目标:
//   - 验证 Plugin-style 设计在 TLM 模式下的可行性 (M1 milestone)
//   - lookup + refill 两阶段 pipeline, 全部用 at_stage() 注册
//   - D4 强制: 无 tick(), 无状态机, Bundle 字段用 uint_t<N>, 阶段间通信用 Payload<T>
//
// 详见:
//   - docs/roadmap/phases/phase-1-tlm-foundation.md §1.2
//   - .omo/drafts/decision-plugin-framework-2026-06-08.md (D4)
//   - bundles/mem_bundles.h (CacheReq / CacheResp / MemResp 输入)
//
// 架构:
//   - 256 sets × 64-byte cache line = 32KB L1 (8-way 不在 Phase 1 范围, 简化为 direct-mapped)
//   - 地址位宽: 64-bit 物理
//     - bit[11:4]   -> idx (8-bit, 256 sets)
//     - bit[31:12]  -> tag (20-bit)
//     - bit[3:0]    -> offset (4-bit, 16-byte 粒度 —— Phase 0 限制, 实际 cache line 64B)
//
// D4 合规说明:
//   - 所有 Bundle 字段使用 cf::plugin::uint_t<N>
//   - 所有阶段用 pb.at_stage() 注册
//   - 跨阶段通信通过 cf::plugin::Payload<T> Key, 无显式成员变量做 IPC
//   - 业务代码无 tick() (PluginBase::tick() 已是 private deleted)
//
// Phase 0 限制 (uint_t<512> 退化为 uint64_t):
//   - 当前 data_ 数组每个 entry 是 uint64_t (不是完整 64-byte line)
//   - 测试只覆盖 64-bit 写入/读出场景; Phase 6 将升级为 __int128 / multiprecision
//
// 约束:
//   - TLM 模式 (Phase 1) 不引入 ch::core::context, 内部存储用 std::array
//   - Phase 5/6 将 ch_mem 引入; std::array 是 ch_mem 在 TLM 模式下的等价物
//   - 头文件为主, 关键实现在 .cpp

#ifndef CF_IP_CACHE_TLM_L1_CACHE_PLUGIN_H
#define CF_IP_CACHE_TLM_L1_CACHE_PLUGIN_H

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/storage.h"
#include "cf/plugin/uint_t.h"
#include "bundles/mem_bundles.h"

namespace cf {
namespace ip {
namespace cache {
namespace tlm {

// ----------------------------------------------------------------------------
// L1CachePlugin —— Plugin-style L1 Data Cache (Phase 1.2 最小验证形态)
//
// 派生自 cf::plugin::PluginBase:
//   - setup()  跨 Plugin 引用声明 (默认空, 不重写)
//   - build()  at_stage 注册 (lookup + refill 两阶段)
//   - 公开辅助 API (issue_request / refill_from_memory / read_response)
//     仅供单元测试使用; 生产路径应通过 Bundle 流式接口 (Phase 1.3+)
// ----------------------------------------------------------------------------
class L1CachePlugin : public cf::plugin::PluginBase {
 public:
  // 缓存几何参数 (编译期常量, 便于 ch_mem 升级时复用)
  static constexpr unsigned kNumSets = 256;       // 8-bit idx
  static constexpr unsigned kTagBits  = 20;        // addr[31:12]
  static constexpr unsigned kIdxBits  = 8;         // addr[11:4]
  static constexpr unsigned kOffsetBits = 4;       // addr[3:0] (Phase 0 简化)
  static constexpr unsigned kLineDataBits = 512;   // 64-byte line (Phase 0 退化为 uint64)
  static constexpr unsigned kAddrBitsRaw = 64;     // 物理地址位宽 (helper 用)

  L1CachePlugin();
  ~L1CachePlugin() override = default;

  L1CachePlugin(const L1CachePlugin&) = delete;
  L1CachePlugin& operator=(const L1CachePlugin&) = delete;

  // PluginBase 接口
  void setup(cf::plugin::PipeBuilder& pb) override;
  void build(cf::plugin::PipeBuilder& pb) override;

  // ------------------------------------------------------------------------
  // 单元测试辅助 API
  //
  // 生产环境应通过 Bundle 流式接口 + Payload Key 传递 CacheReq/CacheResp;
  // 单元测试为了避免注册外部 traffic generator, 直接调用以下方法:
  //   issue_request()    -> 模拟 CPU 发起 CacheReq (写入 lookup 阶段 Payload)
  //   refill_from_memory()-> 模拟 MemResp 到达 (写入 refill 阶段 Payload)
  //   read_response()    -> 读取 lookup 阶段计算出的 CacheResp
  //   is_set_valid()     -> 验证 refill 是否填充目标 set
  //   read_tag()         -> 验证 tag 是否被写入
  // ------------------------------------------------------------------------

  // 将 CacheReq 写入 lookup 阶段 input payload
  // n 必须指向 node_of_logic_stage("lookup"), pb.build() 后才能调用
  void issue_request(const std::shared_ptr<cf::plugin::PipeNode>& n,
                     const cf::bundles::CacheReq& req);

  // 模拟 MemResp 到达 refill 阶段
  // n 必须指向 node_of_logic_stage("refill"), pb.build() 后才能调用
  void refill_from_memory(const std::shared_ptr<cf::plugin::PipeNode>& n,
                          const cf::bundles::MemResp& resp);

  // 读取 lookup 阶段 output (CacheResp 视图)
  // n 必须指向 node_of_logic_stage("lookup")
  cf::bundles::CacheResp read_response(
      const std::shared_ptr<cf::plugin::PipeNode>& n) const;

  // 验证 set 是否被 refill 标记为 valid
  bool is_set_valid(std::size_t set) const;

  // 读取 set 的 tag (refill 后)
  cf::plugin::uint_t<kTagBits> read_tag(std::size_t set) const;

  // 重置内部存储 (供测试间隔离使用)
  void reset_storage();

  // ------------------------------------------------------------------------
  // 单元测试额外辅助 API
  // ------------------------------------------------------------------------
  // 读取 set 的 line_data (lookup 命中验证用)
  cf::plugin::uint_t<kLineDataBits> read_data(std::size_t set) const;
  // 写入 set (refill 实现细节; 单元测试也直接用)
  void write_set(std::size_t set,
                 cf::plugin::uint_t<kTagBits> tag,
                 cf::plugin::uint_t<kLineDataBits> line_data);

  // ------------------------------------------------------------------------
  // 位提取 helper (Phase 5/6 迁移友好)
  //
  // 替代 `static_cast((addr >> shift) & mask)` 这种 shift+mask 模式,
  // 集中维护位宽常量和移位逻辑. Phase 6 切到 CppHDL 时, 内部实现可
  // 改为 `ch::bits<MSB,LSB>(addr)`, 业务调用方不变.
  // ------------------------------------------------------------------------
  static cf::plugin::uint_t<kIdxBits> extract_idx(
      cf::plugin::uint_t<kAddrBitsRaw> addr);
  static cf::plugin::uint_t<kTagBits> extract_tag(
      cf::plugin::uint_t<kAddrBitsRaw> addr);

  // 内部存储类型别名 (Phase 1.3+ ADR-040 可移植性约束)
  // 用 array_store 包装 std::array, Phase 6 切 ch_mem 时只改 array_store 实现
  using TagStore   = cf::plugin::storage::array_store<cf::plugin::uint_t<kTagBits>,     kNumSets>;
  using DataStore   = cf::plugin::storage::array_store<cf::plugin::uint_t<kLineDataBits>, kNumSets>;
  using ValidStore  = cf::plugin::storage::array_store<cf::plugin::bool_t,            kNumSets>;

  // ------------------------------------------------------------------------
  // 内部: build 时缓存 lookup/refill 节点指针, 供 at_stage 闭包访问
  // ------------------------------------------------------------------------
  // 由于 pb.at_stage(StageCallback) 接受 std::function<void()> (无参),
  // 闭包无法直接接收 node. 解决方案: build() 内调用 node_of_logic_stage 获取,
  // 然后缓存到成员变量, at_stage 闭包通过 self->current_*_node() 获取.
  // D4 §3.1: 这些节点指针仅在 at_stage 闭包内临时使用, 不参与跨阶段 IPC.
  std::shared_ptr<cf::plugin::PipeNode> lookup_node_;
  std::shared_ptr<cf::plugin::PipeNode> refill_node_;
  std::shared_ptr<cf::plugin::PipeNode> payload_node_;  // 单元测试 API 使用

  std::shared_ptr<cf::plugin::PipeNode> current_lookup_node() const {
    return lookup_node_;
  }
  std::shared_ptr<cf::plugin::PipeNode> current_refill_node() const {
    return refill_node_;
  }

 private:
  // ------------------------------------------------------------------------
  // 内部存储 (TLM 模式: array_store = std::array 包装; Phase 5/6 切到 ch_mem 时
  //           改 array_store 内部实现, 调用方不变 — ADR-040 移植性约束)
  // D4 §3.1: 存储数组不参与跨阶段 IPC, 仅作为 Plugin 持有的"硬件资源"
  //          跨阶段通信必须走 Payload<T>
  // ------------------------------------------------------------------------
  TagStore   tags_{};
  DataStore  data_{};
  ValidStore valid_{};

  // 重置所有 set 为 invalid (构造时和 reset_storage() 调用)
  void invalidate_all_sets();
};

// ----------------------------------------------------------------------------
// 全局 Payload<T> Key 声明 (跨阶段 IPC, D4 强制)
//
// 使用方式:
//   n->operator()(L1CachePlugin::kAddr) = req.address;
//   auto idx = n->operator()(L1CachePlugin::kIdx);
//
// 这些 Key 是全局静态对象, 在 at_stage 闭包中按指针身份匹配, 跨 PipeNode 隔离
// (每个 PipeNode 有自己的 PayloadStore)
// ----------------------------------------------------------------------------

}  // namespace tlm
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_TLM_L1_CACHE_PLUGIN_H