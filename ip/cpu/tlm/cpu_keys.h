// ip/cpu/tlm/cpu_keys.h
//
// 功能描述: CPU → MMU IPC Payload Key 集合 (mmu-cache-integration commit 2/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 设计:
//   - 4 个 Payload Key 让 CPU 写 / MMU 读
//   - SAT (satp CSR value): CPU csrrw 指令写入 → MMUPlugin::csr_write_satp 闭包读取
//   - SFENCE_VADDR (rs1): CPU SFENCE.VMA 指令写入 → MMUPlugin::sfence_vma 闭包读取
//   - SFENCE_ASID (rs2): CPU SFENCE.VMA 指令写入 → MMUPlugin::sfence_vma 闭包读取
//   - CPU_EXCEPTION_CODE: MMU 异常码 12/13/15 传播到 CPU exception path
//   - 放 ip/cpu/ 而非 ip/mmu/: MMU 闭包依赖 Key identity (全局 static), mmu 不反向依赖 CPU 头
//   - mmu_keys.h::EXCEPTION_CODE 是 MMU 输出, CPU_EXCEPTION_CODE 是 CPU 输入, 方向不同
//
// ⚠ payload key 模态 (ADR-040 v3.0 §10.5, openspec change chmem-cpu-keys-modality):
//   - **模板参数 T 保持 raw** —— PayloadStore 按 `&key` 指针身份索引, 换 T 会得到
//     不同静态实例 ⇒ 消费端 has() 恒 false ⇒ 静默回退 (与 mmu_keys 同理)。
//   - **value type 用 cf::plugin::uint_t<N>** —— TLM 模式解析为裸整型 (零回归);
//     CH_MEM 模式解析为 ch::core::ch_uint<N> ⇒ cell 能承载**运行期 DAG 节点**。
//     裸类型 cell 在 CH_MEM 下只能装 elaboration 期常量 ⇒ "测试全绿但语义为零"。
//   - `static_assert` 必须加 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 —— 否则 T 永远被
//     锁死在裸整型, 上面的 value type 就无从谈起。
//
// D4 合规: 0 业务 tick(), 0 状态机, 闭包驱动

#ifndef CF_IP_CPU_TLM_CPU_KEYS_H
#define CF_IP_CPU_TLM_CPU_KEYS_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace cpu {
namespace tlm {
namespace payload {

template <typename T = std::uint64_t>
struct cpu_keys {
  // 编译期位宽 (TLM 与 CH_MEM 同值): 32 (RV32) / 64 (RV64)
  static constexpr unsigned kBits = sizeof(T) * 8;

#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");
#endif

  // CPU → MMU: satp CSR value (low 4 bits = MODE, high bits = PPN)
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SAT{"cpu.sat"};

  // CPU → MMU: SFENCE.VMA rs1 (vaddr; 0 means x0 register, invalidate-all-vaddr)
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_VADDR{
      "cpu.sfence_vaddr"};

  // CPU → MMU: SFENCE.VMA rs2 (asid; 0 means x0 register, invalidate-all-asid)
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_ASID{
      "cpu.sfence_asid"};

  // MMU → CPU: exception code 12/13/15 (page fault)
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>> CPU_EXCEPTION_CODE{
      "cpu.exception_code"};
};

}  // namespace payload
}  // namespace tlm
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_TLM_CPU_KEYS_H
