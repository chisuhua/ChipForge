// ip/cpu/arch/riscv/decode.cpp
//
// 功能描述: RiscvDecodePlugin 实现 (M3.3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16

#include "ip/cpu/arch/riscv/decode.h"

#include <cstdint>

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// 显式模板实例化
template class RiscvDecodePlugin<std::uint32_t>;
template class RiscvDecodePlugin<std::uint64_t>;

// build() 在头文件中实现 (模板方法)

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf
