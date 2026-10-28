// ip/cpu/core/branch_iface_chmem.h
//
// 功能描述: CH_MEM 开放核的**分支反馈契约** —— `BRANCH_TAKEN` /
//           `BRANCH_TARGET` / `FLUSH`
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-28
//
// ============================================================================
// 归属 (ADR-091 v1.10 §D3a.5 —— S3.2b 分支重定向 + flush)
// ============================================================================
// ★ 本文件是 ADR-091「CH_MEM 开放核形态与端口架构」§S3.2b 的**契约落地**：
//   · **取指侧**（`ibus_port_chmem.h`）消费 `BRANCH_TAKEN` / `BRANCH_TARGET`
//     ⇒ 驱动 `pc` 的 next（分支重定向）
//   · **访存侧**（`dmem_port_chmem.h`）消费 `FLUSH` ⇒ 抑制错路 store
//
// ★★ **为什么是核内 payload 而不是顶层端口**：
//   · 顶层端口（`ch_in`/`ch_out`）的语义 = **对外接口 + 观测**（ADR-091 §D6 H-2）
//   · 分支反馈是**核内 stage 间信号**（branch → fetch / memory）
//     ⇒ ★ 用 `Payload<T>`（ADR-091 §D3a.1：核内 stage 间用 `Payload<T>`）
//     ⇒ ★ **不污染顶层端口**（否则网表会多出 3 个无对外意义的端口）
//   · ★ 且 ★ 若走顶层端口 ⇒ ★ 装配层（A4）必须逐条接线
//     ⇒ ★ 而核内 payload 由 `PipeBuilder` 的 stage 顺序**天然保证可见性**
//
// ============================================================================
// ★★ 与预载核的关系（★ 同名同义，但**独立定义**）
// ============================================================================
// ★ key 名与语义**与预载核一致**（`ip/cpu/plugins/branch_chmem.h` 的
//   `BRANCH_TAKEN` / `BRANCH_TARGET` / `BRANCH_FLUSHED`，以及
//   `ip/cpu/plugins/ibus_chmem.h` 的 `FLUSH`）⇒ ★ 便于 H-3 等价性对照。
// ★★ 但本文件 **MUST NOT include** 那些文件：
//   · `branch_chmem.h` / `ibus_chmem.h` 都 include `<ch.hpp>`
//     ⇒ ★ `module.h:13` 的 `build_hierarchical_name` **缺 `inline`**
//     ⇒ ★ 任何 2+ TU include `<ch.hpp>` 即**链接失败**（ADR-091 §D5.1 G-1b）
//   · ★ 且它们属 ADR-092 的**冻结参考实现**（语义已冻结，不再演进）
//   ⇒ ★ 因此本文件**只 include 具体头**，key **独立定义**
//   ⇒ ★ `PayloadStore` 按 **key 对象地址**索引 ⇒ ★ 同名不同地址**不冲突**
//
// ============================================================================
// ★★★ 语义契约（★ MUST 逐条遵守 —— 违反即静默错算）
// ============================================================================
//
//  ① `BRANCH_TAKEN` —— ★ **已被 flush 掩码**的分支结果
//     ★ 语义：`taken & !flush`（★ 与预载核 `branch_chmem.h` 的 `taken` 同款）
//     ★ ★ **为什么必须掩码**：★ 错路指令（`flush=1`）的分支结果
//       **MUST NOT** 重定向 `pc` ⇒ ★ 否则会跳回错路目标（死循环）
//     ★ 生产者：branch 侧（A4 装配时 = `BranchPlugin`）
//     ★ 消费者：`ibus_port_chmem.h` 的 `branch LATE`（pc mux）
//
//  ② `BRANCH_TARGET` —— ★ 分支目标地址（字节地址，32 bit）
//     ★ 消费者：同上（pc mux 的 taken 臂）
//
//  ③ `FLUSH` —— ★ 「**本拍发布的指令**是错路指令」⇒ 其副作用 MUST 被抑制
//     ★ ★ **1 拍对齐（★ 最容易搞错的一条）**：
//       · 拍 N：`BRANCH_TAKEN(N) = 1` ⇒ ★ 拍 N 的 `pc_reg` 取到的是
//         **顺序地址 A+4**（错路，已在飞）
//       · 拍 N+1：发布该错路指令的 `(PC, INSTRUCTION)`
//       ⇒ ★ **`FLUSH` MUST 在拍 N+1 有效** ⇒ ★ 生产者 MUST 打 1 拍
//         （`flush_reg <<= branch_taken`）
//     ★ 生产者：★ **`ibus_port_chmem.h`**（它在 `branch LATE` 拿到 taken）
//     ★ 消费者：★ **`dmem_port_chmem.h`**（`is_store & !flush`）
//     ★ ★ **顺序契约**（ADR-040 v3.0 §10.6）：★ `FLUSH` 由 `fetch` stage
//       **写**、由 `memory` stage **读** ⇒ ★ `fetch` 的 stage 首现序早于
//       `memory` ⇒ ★ 可见性天然成立（★ 与 dmem 读 `RESULT` 同款）
//
//  ④ ★ **`FLUSH` 缺失时的行为**（★ 诚实标注）：★ `dmem` 读不到 `FLUSH`
//     ⇒ ★ 视为 `flush = 0`（**fail-open**）
//     ★ 理由：★ A2/A4 分阶段装配 ⇒ ★ 只装 dmem 不装 ibus 的形态合法
//       （★ dmem 单测即是）⇒ ★ 抛异常会让合法装配失败
//     ★ 代价：★ 若**装配漏接 ibus** ⇒ ★ 错路 store **不会被抑制**
//       ⇒ ★ 该风险由 **A6 的 H-3 等价性自检**兜底（★ 终值 `memsig` 会不等）
//     ★ ★ 与预载核同口径（`ibus_chmem.h` 的 `brn == nullptr ⇒ flushed=false`）
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0 / ADR-091 §D5.1)
// ============================================================================
//   · CH_MEM-only ⇒ 不引 `cf::plugin::uint_t` / `cpptlm`
//   · ★ **MUST NOT** include `<ch.hpp>`（G-1b）
//   · ★ payload key 的 value type MUST 是**模式化类型**（`ch_bool` /
//     `ch_uint<N>`）⇒ ★ **禁裸 `bool` / `uint32_t`**
//     （★ 裸类型在 CH_MEM 下只能装 POD 常量 ⇒ 信号退化为常量 ⇒ 假绿）
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_BRANCH_IFACE_CHMEM_H
#define CF_IP_CPU_CORE_BRANCH_IFACE_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <core/bool.h>
#include <core/uint.h>

#include "cf/plugin/payload.h"

namespace cf {
namespace cpu {
namespace core {
namespace branch_keys {

/// ① ★ **已被 flush 掩码**的分支结果（`taken & !flush`）
inline const cf::plugin::Payload<ch::core::ch_bool> BRANCH_TAKEN{
    "cpu.branch_taken"};

/// ② 分支目标地址（字节地址，32 bit）
inline const cf::plugin::Payload<ch::core::ch_uint<32>> BRANCH_TARGET{
    "cpu.branch_target"};

/// ③ ★ 「**本拍发布的指令**是错路」⇒ 副作用 MUST 被抑制
/// ★ **生产者 MUST 打 1 拍**（见文件头 ③ 的 1 拍对齐推导）
inline const cf::plugin::Payload<ch::core::ch_bool> FLUSH{"cpu.flush"};

}  // namespace branch_keys
}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_BRANCH_IFACE_CHMEM_H
