// bundles/csr_bundles.h
//
// CSR Plugin payload key 集合 (csr-chmem-pipeline-integration)
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-15
//
// 设计:
//   - 9 个 payload key: CSR 读值通路 (R3 决策) + Zicsr 判定 (R1/R3) + trap 路径
//     (R2 决策) 全部由 csr_chmem.h 写, int_alu_chmem 读 CSR_READ_* 覆写 RD_DATA
//   - 字段类型按 RV32 收窄: CSR_*VALUE/ADDR 用 32-bit, Zicsr_OP 用 uint8_t,
//     CSR_*EN/VALID/TRAP_PENDING 用 1-bit (uint_t<1>), TRAP_TARGET 用 32-bit
//   - Key identity 是 inline 静态指针, IP 间通过 PayloadStore 共享
//
// ⚠ payload key 模态 (ADR-040 v3.0 §10.5, openspec change chmem-cpu-keys-modality):
//   - 1-bit flag key 的 value type 用 `cf::plugin::uint_t<1>` —— TLM 模式 ≡ uint8_t
//     (值语义仍是 0/1), CH_MEM 模式 = ch::core::ch_uint<1> ⇒ cell 能承载**运行期
//     DAG 节点**。裸 `Payload<bool>` 在 CH_MEM 下只能装 elaboration 期常量。
//   - Zicsr_OP 用 `cf::plugin::uint_t<8>` —— TLM ≡ uint8_t (与原声明同型),
//     CH_MEM = ch::core::ch_uint<8>。
//   - 本文件这 5 个 key 的 value type **与模板参数 T 无关** ⇒ 同一静态对象被所有
//     `csr_keys<T>` 实例化共享 (与改动前的 `Payload<bool>` 身份语义一致)。
//
// 与 R1/R2/R3 决策对应:
//   - R1: Zicsr_VALID + Zicsr_OP (R3 写, R2.4 trap 优先级用)
//   - R2: TRAP_PENDING + TRAP_TARGET (R2 写, ibus_chmem.h NORMAL select 消费)
//     ⚠ 实测 (2026-10-22): CH_MEM 侧 `TRAP_PENDING` **零消费者** ——
//       `ibus_chmem.h` 并未读该 key (原注释与代码不符)。已登记
//       ADR-040 v3.0 §10.8; 接线归 mmu-chmem-phase-b-c-launch 的 C.1。
//   - R3: CSR_READ_VALUE + CSR_READ_VALID + CSR_WRITE_ADDR/VALUE/EN
//         (csr_chmem 写, int_alu_chmem 读)
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_BUNDLES_CSR_BUNDLES_H
#define CF_BUNDLES_CSR_BUNDLES_H

#include <cstdint>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace cpu {
namespace csr {
namespace payload {

template <typename T = std::uint32_t>
struct csr_keys {
  // R3 决策 (csr_chmem 写, int_alu_chmem execute-LATE 读): CSR 读值通路
  //   - CSR_READ_VALUE: 32-bit 当前 CSR 值 (CSRRW 的 old value / CSRRS always)
  //   - CSR_READ_VALID: 1-bit (uint_t<1>), true 时 RD_DATA 应被覆写为
  //                    CSR_READ_VALUE (mcycle read 走单独路径, valid=false)
  static inline cf::plugin::Payload<T> CSR_READ_VALUE{"csr.read_value"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>> CSR_READ_VALID{
      "csr.read_valid"};

  // R3.4 写 CSR 寄存器用 (csr_chmem writeback-NORMAL 闭包消费)
  static inline cf::plugin::Payload<T> CSR_WRITE_ADDR{"csr.write_addr"};     // 12-bit
  static inline cf::plugin::Payload<T> CSR_WRITE_VALUE{"csr.write_value"};   // 32-bit
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>> CSR_WRITE_EN{
      "csr.write_en"};

  // R1.3 Zicsr 指令判定 (csr_chmem execute-LATE 闭包写)
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>> Zicsr_VALID{
      "csr.zicsr_valid"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>> Zicsr_OP{"csr.zicsr_op"};

  // R2 决策 (csr_chmem branch-LATE 闭包写, ibus_chmem NORMAL select 消费)
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>> TRAP_PENDING{
      "csr.trap_pending"};
  static inline cf::plugin::Payload<T> TRAP_TARGET{"csr.trap_target"};
};

}  // namespace payload
}  // namespace csr
}  // namespace cpu
}  // namespace cf

#endif  // CF_BUNDLES_CSR_BUNDLES_H
