// ip/cpu/plugins/hazard.cpp
//
// 功能描述: HazardPlugin 实现 (M2.2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16

#include "ip/cpu/plugins/hazard.h"

#include <cstdint>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/pipe_node.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace plugins {

// 显式模板实例化
template class HazardPlugin<std::uint32_t>;
template class HazardPlugin<std::uint64_t>;

// build() 在头文件中实现 (模板方法)

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
