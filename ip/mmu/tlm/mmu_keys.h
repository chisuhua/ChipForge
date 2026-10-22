// ip/mmu/tlm/mmu_keys.h
//
// 功能描述: MMU Plugin Payload<T> Key 集合 (mmu-ip-skeleton, 8.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-22
//
// 设计:
//   - inline Payload<T> Key, 命名空间 cf::ip::mmu::payload
//   - 与 ip/cpu/arch/riscv/payload_riscv.h 同构
//   - 用于 MMUPlugin::at_stage 闭包内跨阶段数据传递
//
// ⚠ **模式化 (mode-aware) 约定** (openspec change chmem-mmu-keys-modality, 2026-10-22):
//   Payload key 的 value type **必须**用 `cf::plugin::uint_t<N>` / `bool_t`,
//   **禁止**裸 `bool` / `uintN_t`. 原因:
//     - TLM 模式: `uint_t<N>` ≡ 裸整型 (uint_t.h:73-87) ⇒ 行为完全等价
//     - CH_MEM 模式: `uint_t<N>` = `ch::core::ch_uint<N>` ⇒ cell 里装的是 **ch 代理**,
//       可承载**运行期 DAG 节点**; 而裸类型在 CH_MEM 下只能装 **POD 常量**
//       (`PayloadStore::put` 是精确类型匹配, payload.h:122-128)
//   ⇒ 裸类型 key 会让 CH_MEM 侧的 sv32 翻译退化成 elaboration 期常量
//     (消费端 `ch_bool(cell)` 把节点折成常量), 表现为 **测试全绿但翻译从未生效**.
//   ⇒ 本文件在 CH_MEM 模式会经 `cf::plugin::uint_t` 实例化 `ch_uint`
//     ⇒ **不是 TLM-only 文件** (check_plugin_portability.sh Check 7 只匹配字面
//     `ch_reg<ch_uint|ch_mem<`, 本文件用别名故不命中).
//
//   其余 10 个 key (VADDR / PERMS / PTW_VADDR / PTW_ASID / PTW_L0_RAW / PTW_L1_RAW /
//   PTW_L2_RAW / PTW_FAULT / SATP_PPN / SATP_MODE) 仍是裸类型 —— 归姊妹 change
//   `chmem-cpu-keys-modality` / `chmem-mmu-keys-modality` 后续批次 (CH_MEM 侧消费点
//   尚未落地, 现在改无消费点可验证).

#ifndef CF_IP_MMU_TLM_MMU_KEYS_H
#define CF_IP_MMU_TLM_MMU_KEYS_H

#include <cstdint>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace ip {
namespace mmu {

namespace payload {

template <typename T = std::uint64_t>
struct mmu_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  // 模式化宽度常量 —— T 始终是**裸整型** (模板参数不随编译模式变化)
  // ⇒ kBits 在 TLM / CH_MEM 两模式下同值 ⇒ `uint_t<kBits>` 解析为
  //    TLM: uint32_t / uint64_t (与旧 `Payload<T>` **逐字节等价**)
  //    CH_MEM: ch_uint<32> / ch_uint<64> (可承载运行期 DAG 节点)
  static constexpr unsigned kBits = sizeof(T) * 8;

  // ── 裸类型 key (未模式化, 见文件头说明) ──
  static inline cf::plugin::Payload<T>       VADDR{"mmu.vaddr"};
  static inline cf::plugin::Payload<std::uint8_t>  PERMS{"mmu.perms"};
  static inline cf::plugin::Payload<T>       PTW_VADDR{"mmu.ptw_vaddr"};
  static inline cf::plugin::Payload<std::uint16_t> PTW_ASID{"mmu.ptw_asid"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L0_RAW{"mmu.ptw_l0_raw"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L1_RAW{"mmu.ptw_l1_raw"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L2_RAW{"mmu.ptw_l2_raw"};
  static inline cf::plugin::Payload<std::uint8_t>  PTW_FAULT{"mmu.ptw_fault"};
  static inline cf::plugin::Payload<T>       SATP_PPN{"mmu.satp_ppn"};
  static inline cf::plugin::Payload<std::uint8_t>  SATP_MODE{"mmu.satp_mode"};

  // ==========================================================================
  // 模式化 key (chmem-mmu-keys-modality, 2026-10-22)
  //
  // 这 5 个是 CH_MEM 侧 sv32 翻译链路的**输出** key, 必须能承载**运行期 DAG 节点**:
  //   MMU_VADDR / PADDR  —— 翻译结果 (地址)
  //   PADDR_VALID        —— 翻译有效标志 (消费端 sel 的条件)
  //   PTW_ACTIVE         —— PTW 进行中标志 (Phase 3.5 fetch-stall 的前提)
  //   EXCEPTION_CODE     —— 异常码 (12/13/15)
  //
  // ⚠ 实例化参数必须与消费端一致 (`mmu_keys<std::uint64_t>`) —— PayloadStore 按
  //   **指针身份**索引 (payload.h:124,136,153 全是 cells_.find(&key)) ⇒ key 对象
  //   不同 ⇒ 消费端 has() 恒 false ⇒ 静默回退 Bare ⇒ 测试全绿但翻译从未生效.
  // ⚠ TLM 侧语义等价: uint_t<64> ≡ uint64_t, uint_t<8> ≡ uint8_t,
  //   uint_t<1> ≡ uint8_t (bool→uint8_t, 值语义 0/1; 全部读点均为
  //   static_cast<bool> 或 = true/false ⇒ 行为等价).
  // ==========================================================================
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> MMU_VADDR{"mmu.mmu_vaddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> PADDR{"mmu.paddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>>       EXCEPTION_CODE{"mmu.exception_code"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PTW_ACTIVE{"mmu.ptw_active"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PADDR_VALID{"mmu.paddr_valid"};
};

}  // namespace payload
}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_TLM_MMU_KEYS_H
