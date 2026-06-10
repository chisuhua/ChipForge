// ip/cache/tlm/L1CachePlugin.cpp
//
// 功能描述: L1CachePlugin 实现 (Phase 1.2) —— lookup + refill 两阶段
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 实现概要:
//   - lookup 阶段 (Phase::NORMAL):
//       1. 从 addr Payload 提取 idx (addr[11:4]) 和 tag (addr[31:12])
//       2. 命中判定: storage tags_[idx] == tag && valid_[idx]
//       3. 写回 hit / data 到 Payload, 供 caller / refill 阶段读
//   - refill 阶段 (Phase::LATE):
//       从 MemResp Payload 写入 storage tags_[idx] / data_[idx], valid_[idx]=true
//
// D4 合规:
//   - 无 tick() 业务重写 (PluginBase::tick() 是 private deleted)
//   - 无显式状态机 (State 枚举 + switch 调度模式)
//   - 所有 Bundle 字段使用 cf::plugin::uint_t<N>
//   - 所有阶段通过 pb.at_stage() 注册
//   - 跨阶段通信通过 Payload<T> Key
//
// 详见:
//   - L1CachePlugin.h
//   - docs/roadmap/phases/phase-1-tlm-foundation.md §1.2

#include "ip/cache/tlm/L1CachePlugin.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_node.h"

namespace cf {
namespace ip {
namespace cache {
namespace tlm {

// ============================================================================
// 文件作用域 Payload<T> Key 声明 (跨阶段 IPC, D4 强制)
//
// 这些 Key 是文件作用域的静态对象, 跨翻译单元共享 (相同 Key 指针身份匹配).
// 在 at_stage 闭包中按指针身份访问 PayloadStore 的 cell.
// ============================================================================
namespace {

// 地址位宽常量 (与物理地址空间一致)
constexpr unsigned kAddrBits = 64;

// 地址 -> idx / tag 提取的掩码与移位 (集中维护, helper 使用)
constexpr uint64_t kIdxMask =
    (1ULL << L1CachePlugin::kIdxBits) - 1;  // 0xFF
constexpr unsigned kIdxShift = L1CachePlugin::kOffsetBits;  // 4
constexpr uint64_t kTagMask =
    (1ULL << L1CachePlugin::kTagBits) - 1;  // 0xFFFFF
constexpr unsigned kTagShift =
    L1CachePlugin::kOffsetBits + L1CachePlugin::kIdxBits;  // 12

// lookup 阶段 input (由 issue_request() 写入)
cf::plugin::Payload<cf::plugin::uint_t<kAddrBits>> g_addr{"l1cache.addr"};
cf::plugin::Payload<cf::plugin::bool_t>            g_is_write{"l1cache.is_write"};
cf::plugin::Payload<cf::plugin::uint_t<2>>         g_op{"l1cache.op"};
cf::plugin::Payload<cf::plugin::uint_t<8>>         g_id{"l1cache.id"};

// lookup 阶段 output / refill 阶段 input (跨阶段 IPC)
cf::plugin::Payload<cf::plugin::uint_t<L1CachePlugin::kIdxBits>>     g_idx{"l1cache.idx"};
cf::plugin::Payload<cf::plugin::uint_t<L1CachePlugin::kTagBits>>     g_tag{"l1cache.tag"};
cf::plugin::Payload<cf::plugin::bool_t>                              g_hit{"l1cache.hit"};
cf::plugin::Payload<cf::plugin::uint_t<L1CachePlugin::kLineDataBits>> g_data{"l1cache.data"};
cf::plugin::Payload<cf::plugin::bool_t>                              g_error{"l1cache.error"};

// refill 阶段 input (由 refill_from_memory() 写入)
cf::plugin::Payload<cf::plugin::uint_t<L1CachePlugin::kLineDataBits>> g_mem_data{"l1cache.mem_data"};
cf::plugin::Payload<cf::plugin::uint_t<8>>                           g_mem_id{"l1cache.mem_id"};

}  // namespace

// ============================================================================
// 位提取 helper (ADR-040 §2.5: 替代 shift+mask 内联模式)
// ============================================================================
cf::plugin::uint_t<L1CachePlugin::kIdxBits> L1CachePlugin::extract_idx(
    cf::plugin::uint_t<kAddrBitsRaw> addr) {
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (static_cast<uint64_t>(addr) >> kIdxShift) & kIdxMask);
}

cf::plugin::uint_t<L1CachePlugin::kTagBits> L1CachePlugin::extract_tag(
    cf::plugin::uint_t<kAddrBitsRaw> addr) {
  return static_cast<cf::plugin::uint_t<kTagBits>>(
      (static_cast<uint64_t>(addr) >> kTagShift) & kTagMask);
}

// ============================================================================
// L1CachePlugin 实现
// ============================================================================

L1CachePlugin::L1CachePlugin() {
  invalidate_all_sets();
}

void L1CachePlugin::setup(cf::plugin::PipeBuilder& /*pb*/) {
  // 跨 Plugin 引用声明 (Phase 1.2 无依赖, 保留接口供 Phase 1.3+ 扩展)
}

void L1CachePlugin::build(cf::plugin::PipeBuilder& pb) {
  pb.declare_substage("lookup", "refill", 1);

  // lookup 与 refill 共享同一个 PipeNode (payload_node_): 跨阶段 Payload Key 必须
  // 命中同一 PayloadStore 才能传递 idx/tag/hit/data; 否则 refill 读到的是默认
  // 构造值 (hit=false 但 idx=0/数据=0), 导致写入错的 set.
  // D4 §3.1: 这只是实现细节 (PipeBuilder API 约束), 不参与跨阶段 IPC 语义.
  lookup_node_ = pb.node_of_logic_stage("lookup");
  refill_node_ = lookup_node_;
  payload_node_ = lookup_node_;  // 测试 API 也使用同一节点

  // ------------------------------------------------------------------------
  // ADR-040 存储 commit 钩子注册
  //
  // 当前 TLM 模式下, array_store::commit() 是 no-op (单缓冲, 即写即读).
  // Phase 6 切到 RTL 时, 改 array_store 内部实现, 钩子自动生效,
  // 业务调用方 (lookup / refill at_stage 闭包) 不变.
  //
  // 注意: 注册顺序 = commit 顺序; tags_ 先于 data_/valid_ 提交.
  // ------------------------------------------------------------------------
  pb.register_commit_hook([this] { tags_.commit(); });
  pb.register_commit_hook([this] { data_.commit(); });
  pb.register_commit_hook([this] { valid_.commit(); });

  // lookup 阶段 (Phase::NORMAL)
  // ADR-040 §2.3: 闭包内不早返  (HDL 中无法直接表达).
  //              build() 已 assert lookup_node_ 非空, 闭包内无须 defensive check.
  pb.at_stage("lookup", cf::plugin::Phase::NORMAL, [this]() {
    auto* n = lookup_node_.get();
    assert(n && "build() must initialize lookup_node_ before at_stage callback runs");

    // 1. 从 addr 提取 idx / tag (使用 helper, 替代 shift+mask 内联 — ADR-040 §2.5)
    cf::plugin::uint_t<kAddrBits> addr = n->operator()(g_addr);
    auto idx = extract_idx(addr);
    auto tag = extract_tag(addr);
    n->put(g_idx, idx);
    n->put(g_tag, tag);

    // 2. 命中判定: storage tags_[idx] == tag && valid_[idx]
    bool hit = is_set_valid(idx) && (read_tag(idx) == tag);
    n->put(g_hit, hit);

    // 3. data: 命中返回存储, 未命中返回 0
    // ADR-040 §2.3: 用 if/else 全分支展开, 不早返 (早返在 HDL 中无法直接表达)
    if (hit) {
      n->put(g_data, read_data(idx));
    } else {
      n->put(g_data, cf::plugin::uint_t<L1CachePlugin::kLineDataBits>{0});
    }
    n->put(g_error, false);
  });

  // refill 阶段 (Phase::LATE) — 仅 miss 时执行
  // ADR-040 §2.3: 同样用 if/else 全分支, 不早返
  pb.at_stage("refill", cf::plugin::Phase::LATE, [this]() {
    auto* n = refill_node_.get();
    assert(n && "build() must initialize refill_node_ before at_stage callback runs");

    cf::plugin::bool_t hit = n->operator()(g_hit);
    // 读取所有需要的 Payload (无论 hit 与否, 保证没有跨阶段 RAW 死读)
    cf::plugin::uint_t<L1CachePlugin::kLineDataBits> mem_data =
        n->operator()(g_mem_data);
    cf::plugin::uint_t<L1CachePlugin::kIdxBits> idx =
        n->operator()(g_idx);
    cf::plugin::uint_t<L1CachePlugin::kTagBits> tag =
        n->operator()(g_tag);

    if (!hit) {  // 全分支 if/else, 不早返
      write_set(idx, tag, mem_data);
    }
    // hit 分支: no-op (命中 set 已有正确 data, 无需 refetch)
  });
}

// ============================================================================
// 单元测试辅助 API
// ============================================================================

void L1CachePlugin::issue_request(
    const std::shared_ptr<cf::plugin::PipeNode>& /*n*/,
    const cf::bundles::CacheReq& req) {
  if (!payload_node_) return;
  payload_node_->put(g_addr, static_cast<cf::plugin::uint_t<kAddrBits>>(req.address));
  payload_node_->put(g_is_write, req.is_write);
  payload_node_->put(g_op, req.op);
  payload_node_->put(g_id, req.id);
}

void L1CachePlugin::refill_from_memory(
    const std::shared_ptr<cf::plugin::PipeNode>& /*n*/,
    const cf::bundles::MemResp& resp) {
  if (!payload_node_) return;
  payload_node_->put(g_mem_data, static_cast<cf::plugin::uint_t<L1CachePlugin::kLineDataBits>>(resp.data));
  payload_node_->put(g_mem_id, resp.id);
}

cf::bundles::CacheResp L1CachePlugin::read_response(
    const std::shared_ptr<cf::plugin::PipeNode>& /*n*/) const {
  cf::bundles::CacheResp resp{};
  if (!payload_node_) return resp;
  resp.data  = payload_node_->operator()(g_data);
  resp.hit   = payload_node_->operator()(g_hit);
  resp.error = payload_node_->operator()(g_error);
  resp.id    = payload_node_->operator()(g_id);
  return resp;
}

bool L1CachePlugin::is_set_valid(std::size_t set) const {
  if (set >= kNumSets) return false;
  return valid_[set];
}

cf::plugin::uint_t<L1CachePlugin::kTagBits> L1CachePlugin::read_tag(
    std::size_t set) const {
  if (set >= kNumSets) return cf::plugin::uint_t<kTagBits>{0};
  return tags_[set];
}

cf::plugin::uint_t<L1CachePlugin::kLineDataBits>
L1CachePlugin::read_data(std::size_t set) const {
  if (set >= kNumSets) return cf::plugin::uint_t<kLineDataBits>{0};
  return data_[set];
}

void L1CachePlugin::write_set(
    std::size_t set,
    cf::plugin::uint_t<kTagBits> tag,
    cf::plugin::uint_t<kLineDataBits> line_data) {
  if (set >= kNumSets) return;
  tags_[set] = tag;
  data_[set] = line_data;
  valid_[set] = true;
}

void L1CachePlugin::reset_storage() {
  invalidate_all_sets();
}

void L1CachePlugin::invalidate_all_sets() {
  for (std::size_t i = 0; i < kNumSets; ++i) {
    tags_[i] = cf::plugin::uint_t<kTagBits>{0};
    data_[i] = cf::plugin::uint_t<kLineDataBits>{0};
    valid_[i] = false;
  }
}

}  // namespace tlm
}  // namespace cache
}  // namespace ip
}  // namespace cf