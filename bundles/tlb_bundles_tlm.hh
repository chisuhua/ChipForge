// bundles/tlb_bundles_tlm.hh
//
// TLB Bundle 定义（轻量级，镜像 cache_bundles_tlm.hh）
// 功能描述：定义 TLB 请求/响应 Bundle，4 字段窄桥
//           MMUTLMBridgeAdapter 使用这些 Bundle 与 ch_stream 对接
//           (mmu-tlb-ptw-impl commit 9/11)
//
// 设计原则（镜像 cache_bundles_tlm.hh）：
//   - 轻量级：仅 POD 字段，无 CppHDL AST 依赖
//   - C++17 兼容：可在 cpptlm_core（C++17 静态库）中使用
//   - 全局 `bundles::` 命名空间（与 cache_bundles_tlm.hh 一致）
//
// 字段：
//   TlbReq:  transaction_id + vaddr + asid + access_type
//   TlbResp: transaction_id + paddr + perms + exception_code

#ifndef BUNDLES_TLB_BUNDLES_TLM_HH
#define BUNDLES_TLB_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

struct TlbReqBundle : public bundle_base {
  ch_uint<64> transaction_id;
  ch_uint<64> vaddr;
  ch_uint<16> asid;
  ch_uint<8>  access_type;  // 0=exec, 1=load, 2=store
};

struct TlbRespBundle : public bundle_base {
  ch_uint<64> transaction_id;
  ch_uint<64> paddr;
  ch_uint<8>  hit;             // 1=hit, 0=miss
  ch_uint<8>  perms;           // R/W/X/U bitmask
  ch_uint<8>  exception_code;  // 0=none, 12/13/15=page fault
};

}  // namespace bundles

#endif  // BUNDLES_TLB_BUNDLES_TLM_HH
