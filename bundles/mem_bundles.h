// bundles/mem_bundles.h
//
// 功能描述: 共享 Bundle 定义 (Phase 1.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 详见:
//   - docs/roadmap/phases/phase-1-tlm-foundation.md §1.1
//   - .omo/drafts/decision-plugin-framework-2026-06-08.md (D4 Plugin-style 强制)
//
// 设计目标:
//   - 共享 Bundle 类型 (MemReq / MemResp / CacheReq / CacheResp / IntBundle)
//   - L1Cache 内部状态 Bundle (L1CachePluginBundle)
//   - 所有字段使用 cf::plugin::uint_t<N> (D4 合规: 编译期 TLM/RTL 切换)
//
// 设计选择:
//   - Phase 1.1 (TLM 模式): POD struct, 无虚函数, 无 bundle_base 继承
//   - 字段直接公开 (struct aggregate), 便于 TLM 事务直接读写
//   - Phase 5 (RTL 协同) 时, BundleMapper 可转换为 ch_uint<N> + bundle_base 派生
//   - 所有字段均 static_assert 类型, 编译期阻止 raw uint64_t / ch_uint<>
//
// 约束:
//   - 头文件为主 (无 .cpp)
//   - C++17 兼容
//   - 不依赖 cpptlm / cpphdl (Phase 0 独立验证)

#ifndef CF_BUNDLES_MEM_BUNDLES_H
#define CF_BUNDLES_MEM_BUNDLES_H

#include "cf/plugin/uint_t.h"

namespace cf {
namespace bundles {

// ============================================================================
// MemReq —— 内存请求 Bundle (CPU -> Memory via interconnect)
// AXI-style 字段布局:
//   - address: 64-bit 物理地址
//   - data:    64-bit 写数据 (读操作时未使用)
//   - is_write: true=write, false=read
//   - burst_len: AXI burst 长度 (0 = 1 beat, 255 = 256 beats)
//   - id: 请求 ID (用于乱序响应匹配)
// ============================================================================
struct MemReq {
  cf::plugin::uint_t<64> address{0};
  cf::plugin::uint_t<64> data{0};
  cf::plugin::bool_t     is_write{false};
  cf::plugin::uint_t<8>  burst_len{0};
  cf::plugin::uint_t<8>  id{0};
};

// ============================================================================
// MemResp —— 内存响应 Bundle (Memory -> CPU via interconnect)
//   - data: 64-bit 读数据 (写响应时未使用)
//   - id:   响应 ID (匹配发起请求的 id)
//   - error: true=总线错误
//   - last:  burst 最后一拍 (AXI RLAST/WLAST 信号)
// ============================================================================
struct MemResp {
  cf::plugin::uint_t<64> data{0};
  cf::plugin::uint_t<8>  id{0};
  cf::plugin::bool_t     error{false};
  cf::plugin::bool_t     last{false};
};

// ============================================================================
// CacheReq —— 缓存请求 Bundle (CPU -> L1Cache)
//   包含 op 编码, 区分 read/write/invalidate/flush 操作
//   - address:  64-bit 物理地址
//   - data:     64-bit 写数据
//   - is_write: true=write (冗余, 但便于快速分支)
//   - op:       2-bit 操作编码
//     00=Read, 01=Write, 10=Invalidate, 11=Flush
//   - id:       请求 ID
// ============================================================================
struct CacheReq {
  cf::plugin::uint_t<64> address{0};
  cf::plugin::uint_t<64> data{0};
  cf::plugin::bool_t     is_write{false};
  cf::plugin::uint_t<2>  op{0};
  cf::plugin::uint_t<8>  id{0};
};

// ============================================================================
// CacheResp —— 缓存响应 Bundle (L1Cache -> CPU)
//   - data: 读返回数据 / 写确认
//   - hit:  缓存命中 (false 表示 refill in progress)
//   - error: 总线错误传播
//   - id:   响应 ID
// ============================================================================
struct CacheResp {
  cf::plugin::uint_t<64> data{0};
  cf::plugin::bool_t     hit{false};
  cf::plugin::bool_t     error{false};
  cf::plugin::uint_t<8>  id{0};
};

// ============================================================================
// L1CachePluginBundle —— L1Cache 内部状态 Bundle
//   用于 PipeBuilder 阶段间通信 (lookup -> refill), 不直接出现在外部 IO
//   - tag:       20-bit 物理 tag (40-bit 物理地址去掉 8-bit idx + 12-bit offset)
//   - idx:       8-bit set index (256 sets)
//   - line_data: 512-bit cache line (64 bytes)
//   - valid:     1-bit 有效位
//   - dirty:     1-bit 脏位 (write-back 策略)
// ============================================================================
struct L1CachePluginBundle {
  cf::plugin::uint_t<20>  tag{0};
  cf::plugin::uint_t<8>   idx{0};
  cf::plugin::uint_t<512> line_data{0};
  cf::plugin::bool_t      valid{false};
  cf::plugin::bool_t      dirty{false};
};

// ============================================================================
// IntBundle —— 中断接口 Bundle (PLIC/CLINT -> CPU)
//   极简实现, 预留 Phase 2+ RTOS 扩展
//   - irq: 中断请求 (level-sensitive)
//   - ack: 中断确认 (1-cycle pulse)
// ============================================================================
struct IntBundle {
  cf::plugin::bool_t irq{false};
  cf::plugin::bool_t ack{false};
};

}  // namespace bundles
}  // namespace cf

#endif  // CF_BUNDLES_MEM_BUNDLES_H
