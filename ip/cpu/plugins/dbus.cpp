// ip/cpu/plugins/dbus.cpp
//
// 功能描述: DBusPlugin 实现 (M2.5)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16

#include "ip/cpu/plugins/dbus.h"

#include <cstdint>

namespace cf {
namespace cpu {
namespace plugins {

template class DBusPlugin<std::uint32_t>;
template class DBusPlugin<std::uint64_t>;

}  // namespace plugins
}  // namespace cpu
}  // namespace cf
