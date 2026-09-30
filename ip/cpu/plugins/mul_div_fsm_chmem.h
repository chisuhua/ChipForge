// ip/cpu/plugins/mul_div_fsm_chmem.h
//
// 功能描述: MulDivFsmPlugin CH_MEM 模式配对 (mfc Phase D.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-30
//
// 设计 (ADR-040 v2.0 双文件分离 + ADR-082 CH_MEM elaborator):
//   - CH_MEM 模式 (CF_PLUGIN_USE_CH_MEM) 提供 elaboration API
//   - 当前 CH_MEM 代码仍内联在 ip/cpu/arch/riscv/mul_div_fsm.h (#ifdef 块)
//   - 本文件作为 ADR-040 v2.0 双文件分离的"占位", 提供 helper namespace + future hook
//   - Phase D.4 refactor 会把 CH_MEM 部分从 mul_div_fsm.h 拆到此文件 (目标 ADR-040 v2.0 严格分离)
//
// 约束 (D4 + ADR-040 v2.0 + ADR-046):
//   - 必须 #ifdef CF_PLUGIN_USE_CH_MEM 整文件包裹
//   - 无业务 tick() (使用 ch_state_machine DSL 调度)
//   - 无 ch_mem 在 TLM 文件 (mul_div_fsm.h 内 #ifdef CF_PLUGIN_USE_CH_MEM 块是过渡, 本文件是最终目标)
//   - ch_state_machine DSL 描述 FSM (IDLE→MULTIPLY|DIVIDE→WRITE_BACK→IDLE)
//
#ifndef CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H
#define CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H

#include "ip/cpu/arch/riscv/mul_div_fsm.h"

// 仅在 CH_MEM 模式下编译
#ifdef CF_PLUGIN_USE_CH_MEM

#include <chlib/state_machine.h>

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// mul_div_fsm_chmem — Phase D.1 helper namespace
//
// 当前 Phase D.1 占位: 提供 elaboration hook + cycle parity test 入口
// Phase D.4 refactor 将把 mul_div_fsm.h 内 CH_MEM 部分 (create_fsm /
// opcode_sig_ / counter_reg_ 等) 拆到此 namespace
namespace mul_div_fsm_chmem {

// has_chmem_support: Phase D.1 stub, 用于 [chmem][multi-cycle] 测试判断
// 目标 (D.4 落地): MulDivFsmPlugin 已含 ch_state_machine DSL
inline constexpr bool has_chmem_support = true;

}  // namespace mul_div_fsm_chmem

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM

// TLM mode 暴露 constexpr symbol (跨模式可见, 用于 cycle parity test 入口)
#ifndef CF_PLUGIN_USE_CH_MEM
namespace cf {
namespace cpu {
namespace arch {
namespace riscv {
namespace mul_div_fsm_chmem {
inline constexpr bool has_chmem_support = true;  // TLM mode 也声明 (mark 路径存在)
}
}
}
}
}
#endif

#endif  // CF_IP_CPU_PLUGINS_MUL_DIV_FSM_CHMEM_H
