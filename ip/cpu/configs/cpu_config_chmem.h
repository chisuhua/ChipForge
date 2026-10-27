// ip/cpu/configs/cpu_config_chmem.h
//
// 功能描述: CpuConfigChmem —— CH_MEM 模式 CPU 的配置结构 (P2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-27
//
// ============================================================================
// 为什么需要这个结构 (而不是复用 cpu_factory.h 的 CPUConfig)
// ============================================================================
// 1. **能力集不同**: CPUConfig 有 33 个字段, 覆盖 TLM 侧的多级流水线 / superscalar /
//    Cache 延迟 / SMT 等。CH_MEM 工厂只消费其中 3 个 (enable_mmu / mmu_mode /
//    btb_entries) + 2 个 CH_MEM 独有旋钮 (enable_bp / bp_uncond)。
//    ⇒ 复用会制造「以为能配其实不能」的假等价陷阱。
//
// 2. **依赖隔离**: cpu_factory.h 属于 TLM 侧 (即将随 B3 退役), CH_MEM 工厂
//    (cpu_factory_chmem.h:98) 明确声明 **不引 CpuConfig 依赖** (NG1/NG2/NG3)。
//    本文件为 **零外部依赖** (仅 <string>/<cstdint>), 不拉 nlohmann 进工厂头。
//
// 3. **字段名不统一是刻意的**: TLM 的 `branch_predictor` 是策略名
//    (static/bimodal/gshare/tournament), CH_MEM 侧 BP 只有 Bimodal 一档实装
//    （GShare 增量 3 未签批 ⇒ 不暴露开关）。强行同名会让 "bmimodal" vs
//    "gshare=false" 的映射隐藏在一层转换里 ⇒ 配错时不报错。
//    ⇒ **CH_MEM 用自己的名字**。
//
// ★★ **`bp_gshare` 旋钮的缺省状态（2026-10-27 用户裁决「P2 剥离 bp_gshare」）**：
//    GShare 增量 3 处于**净负收益**（ADR-088 §5.3.3 C-3: 822514 vs 794478 ⇒ 慢
//    2001 拍 ⇒ 不满足 C-3）且 **D8/D10 签批未到** ⇒ 其数据通路**不进 main**。
//    ⇒ 配置面**不暴露** `bp_gshare` 旋钮（暴露一个未签批功能的开关 = 假绿通道）。
//    ⇒ GShare 签批后**增量加回**该旋钮（不是返工）。
//
// ============================================================================
// → JSON 解析在 cpu_config_chmem_json.h (仅需要 JSON 的调用方 include)
// ============================================================================

#ifndef CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_H
#define CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_H

#include <cstdint>
#include <string>

namespace cf {
namespace cpu {

// ============================================================================
// CpuConfigChmem —— CH_MEM 模式的可配置面
//
// ★ 默认值 MUST 与 cpu_factory_chmem.h::build_cpu 的默认参数**逐位等价**
//   (2026-10-27 P2 落地时核对; tests/cpu/schema/test_cpu_config_chmem.cpp
//    有专门的 default-equivalence 断言钉住这一点)
// ============================================================================
struct CpuConfigChmem {
  // ── 元数据 (不影响行为; DSE 结果标识用) ──
  std::string name = "RiscvCpuChmem";

  // ── 程序加载 ──
  // ★ 不含 preload_elf: 由 build_cpu 从 elf_image 是否为空推导 (空 ⇒ 不预载)。
  //   保留一个 bool 会产生 "preload_elf=true 但 elf_image 空" 的歧义态。
  std::uint32_t initial_pc = 0x80000000u;

  // ── 设计参数 (DSE 真正可扫的维度) ──
  bool enable_bp = true;            // false ⇒ 不注册 BranchPredictorPlugin
  std::uint32_t btb_entries = 256;  // 逻辑条目数 ∈ {16,32,64,128,256}
  bool bp_uncond = true;            // jal/j 建 BTB 项且恒 taken
  // ★ **不含 bp_gshare** —— 见文件头「`bp_gshare` 旋钮的缺省状态」：
  //   GShare 增量 3 未签批 ⇒ 配置面不暴露其开关。

  // ── 未实装 (true ⇒ build_cpu 内 fail-fast, 与 P2 前的行为一致) ──
  bool enable_mmu = false;              // plumbing-only no-op log
  std::string mmu_mode = "bare";        // bare/sv32/sv39/sv48 (仅 log)
  bool enable_cache = false;            // true ⇒ throw (L1Cache CH_MEM 未实装)
};

// ============================================================================
// 值域校验 —— 只查**值合法性**, 不查**是否实装**
//
// ★ 分工:
//   - 本函数: 值域 (如 btb_entries 必须在 {16,32,64,128,256})
//   - build_cpu: 实装状态 (enable_cache=true ⇒ throw; enable_mmu=true ⇒ log)
//   ⇒ 不重复判定, 避免两处口径漂移
//
// 返回: 空串 = 通过; 非空 = 人类可读的错误原因 (fail-fast, 不降级)
// ============================================================================
inline std::string validate_cpu_config_chmem(const CpuConfigChmem& cfg) {
  switch (cfg.btb_entries) {
    case 16u:
    case 32u:
    case 64u:
    case 128u:
    case 256u:
      break;
    default:
      return "btb_entries must be one of {16,32,64,128,256} (got " +
             std::to_string(cfg.btb_entries) + ")";
  }

  if (cfg.mmu_mode != "bare" && cfg.mmu_mode != "sv32" &&
      cfg.mmu_mode != "sv39" && cfg.mmu_mode != "sv48") {
    return "mmu_mode must be one of {bare,sv32,sv39,sv48} (got \"" +
           cfg.mmu_mode + "\")";
  }

  // initial_pc 必须是 4 字节对齐 (取指地址按字索引, 非对齐无意义)
  if ((cfg.initial_pc & 0x3u) != 0u) {
    return "initial_pc must be 4-byte aligned (got 0x" +
           std::to_string(cfg.initial_pc) + ")";
  }

  return {};
}

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_H
