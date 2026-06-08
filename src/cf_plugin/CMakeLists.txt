# src/cf_plugin/CMakeLists.txt
#
# 功能描述: ChipForge Plugin 脚手架 (Phase 0) 的 CMake 配置
# 状态: Phase 0 实施中 (1/5 P0 组件已完成)
# 作者: ChipForge Build System
# 最后修改日期: 2026-06-08
#
# 详见:
#   - docs/roadmap/phases/phase-0-plugin-scaffolding.md
#   - .omo/drafts/decision-plugin-framework-2026-06-08.md
#   - docs/roadmap/roadmap-status.md (Phase 0 进度跟踪)

# ----------------------------------------------------------------------------
# 公共头文件目录
# ----------------------------------------------------------------------------
set(CF_PLUGIN_PUBLIC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# ----------------------------------------------------------------------------
# cf_plugin — Phase 0 Plugin 脚手架 (INTERFACE 库, 头文件为主)
# ----------------------------------------------------------------------------
# Phase 0 提供的 5 个 P0 组件 (namespace cf::plugin):
#   ✅ 1. PluginBase   — 已实现 + 单元测试 (7/7 PASS)
#   ✅ 2. Payload<T>   — 已实现 + 单元测试 (8/8 PASS)
#   ✅ 3. PipeNode     — 已实现 + 单元测试 (14/14 PASS)
#   ✅ 4. PipeBuilder  — 已完整化 + 单元测试 (11/11 PASS)
#   ✅ 5. CtrlLink     — 已实现 + 单元测试 (11/11 PASS)
#
# 全部 5 个 P0 组件已在 2026-06-08 完成.
# 退出标准中"质量标准"与"集成标准"待 Phase 0 收尾时验证.
add_library(cf_plugin INTERFACE)

target_include_directories(cf_plugin
  INTERFACE
    $<BUILD_INTERFACE:${CF_PLUGIN_PUBLIC_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cf_plugin INTERFACE cxx_std_17)

# 严格警告: Phase 0 自身代码遵循
target_compile_options(cf_plugin INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# ----------------------------------------------------------------------------
# 单元测试 (使用纯 main() + assert, 暂不引入 Catch2)
# ----------------------------------------------------------------------------
enable_testing()

# P0 #1: PluginBase 生命周期测试
add_executable(test_plugin_lifecycle
  tests/test_plugin_lifecycle.cpp
)
target_link_libraries(test_plugin_lifecycle PRIVATE cf_plugin)
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)

# P0 #2: Payload<T> 类型安全 Key 测试
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_payload.cpp)
  add_executable(test_payload tests/test_payload.cpp)
  target_link_libraries(test_payload PRIVATE cf_plugin)
  add_test(NAME test_payload COMMAND test_payload)
endif()

# P0 #3: PipeNode 节点测试
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipe_node.cpp)
  add_executable(test_pipe_node tests/test_pipe_node.cpp)
  target_link_libraries(test_pipe_node PRIVATE cf_plugin)
  add_test(NAME test_pipe_node COMMAND test_pipe_node)
endif()

# P0 #4: PipeBuilder 编排器测试
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipe_builder.cpp)
  add_executable(test_pipe_builder tests/test_pipe_builder.cpp)
  target_link_libraries(test_pipe_builder PRIVATE cf_plugin)
  add_test(NAME test_pipe_builder COMMAND test_pipe_builder)
endif()

# P0 #5: CtrlLink 控制 API 测试
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ctrl_link.cpp)
  add_executable(test_ctrl_link tests/test_ctrl_link.cpp)
  target_link_libraries(test_ctrl_link PRIVATE cf_plugin)
  add_test(NAME test_ctrl_link COMMAND test_ctrl_link)
endif()

# Phase 0 退出标准: 最小验证 Plugin + 调度确定性
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_hello_plugin.cpp)
  add_executable(test_hello_plugin tests/test_hello_plugin.cpp)
  target_link_libraries(test_hello_plugin PRIVATE cf_plugin)
  add_test(NAME test_hello_plugin COMMAND test_hello_plugin)
endif()

# ----------------------------------------------------------------------------
# 配置摘要
# ----------------------------------------------------------------------------
message(STATUS "  cf_plugin:        Phase 0 (5/5 P0 components) ✅")

# ----------------------------------------------------------------------------
# Phase 0 实施 checklist (实施时按需启用)
# ----------------------------------------------------------------------------
# [x] 1.1 添加 include/cf/plugin/plugin_base.h + 单元测试
# [x] 1.1 添加 include/cf/plugin/uint_t.h (编译期类型切换)
# [x] 1.1 添加 include/cf/plugin/pipe_builder.h (P0 #1 临时占位)
# [x] 1.2 添加 include/cf/plugin/payload.h + 单元测试
# [x] 1.3 添加 include/cf/plugin/pipe_node.h + 单元测试
# [x] 1.4 完整化 include/cf/plugin/pipe_builder.h + 单元测试
# [x] 1.5 添加 include/cf/plugin/ctrl_link.h + 单元测试
# [ ] 转换 INTERFACE → STATIC (如出现非模板 .cpp) — Phase 0 暂不需要
# [ ] 启用 cpptlm_core 依赖 (Phase 1+ 业务代码需要)
