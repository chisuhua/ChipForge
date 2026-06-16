// ip/cpu/plugins/branch_predictor.cpp
//
// 功能描述: BranchPredictorPlugin 实现 (M2.3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16

#include "ip/cpu/plugins/branch_predictor.h"

#include <cstdint>

namespace cf {
namespace cpu {
namespace plugins {

// 显式模板实例化
template class BranchPredictorPlugin<std::uint32_t>;
template class BranchPredictorPlugin<std::uint64_t>;

// build() 在头文件中实现 (模板方法)

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
