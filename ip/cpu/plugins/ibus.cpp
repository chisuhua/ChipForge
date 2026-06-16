// ip/cpu/plugins/ibus.cpp
//
// 功能描述: IBusPlugin 实现 (M2.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16

#include "ip/cpu/plugins/ibus.h"

#include <cstdint>

namespace cf {
namespace cpu {
namespace plugins {

template class IBusPlugin<std::uint32_t>;
template class IBusPlugin<std::uint64_t>;

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
