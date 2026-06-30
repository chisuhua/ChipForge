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

# ----------------------------------------------------------------------------
# 注意: cf_plugin 的单元测试不在 src/cf_plugin/tests/ 下
#   实际位置: tests/framework/test_*.cpp
#   src/cf_plugin/tests/ 目录已删除 (CHANGE-005 empty-directory-cleanup)
#   CMake 注册在下方"单元测试"段（约 L80+）
# ----------------------------------------------------------------------------

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
# 依赖路径(从父 CMakeLists.txt 传入的隔离构建 install 路径)
# CppTLM/CppHDL 隔离构建到 build/_deps/install/,父级以变量形式暴露给本目录:
#   CHIPFORGE_CPPTLM_LIBRARY          = <install>/lib/libcpptlm_core.a
#   CHIPFORGE_CPPTLM_INCLUDE_DIRS     = <install>/include/cpp-tlm[;/external/json]
#   CHIPFORGE_CPPHDL_LIBRARY          = <install>/lib/libcpphdl.a
#   CHIPFORGE_CPPHDL_INCLUDE_DIRS     = <install>/include/cpp-hdl
# 用 helper 函数统一附加给所有需要的 test target,避免重复 target_include_directories
# ----------------------------------------------------------------------------
function(cf_plugin_link_cpptlm tgt)
  target_link_libraries(${tgt} PRIVATE ${CHIPFORGE_CPPTLM_LIBRARY})
  target_include_directories(${tgt} PRIVATE ${CHIPFORGE_CPPTLM_INCLUDE_DIRS})
endfunction()
function(cf_plugin_link_cpphdl tgt)
  target_link_libraries(${tgt} PRIVATE ${CHIPFORGE_CPPHDL_LIBRARY})
  target_include_directories(${tgt} PRIVATE ${CHIPFORGE_CPPHDL_INCLUDE_DIRS})
endfunction()

# 可选: 覆盖率测量 (gcov)
option(CF_PLUGIN_COVERAGE "Enable gcov coverage for cf_plugin tests" OFF)
if(CF_PLUGIN_COVERAGE)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "CF_PLUGIN_COVERAGE works best with Debug build")
  endif()
  target_compile_options(cf_plugin INTERFACE --coverage -O0 -g)
  target_link_libraries(cf_plugin INTERFACE --coverage)
  message(STATUS "  cf_plugin:        coverage enabled (gcov)")
endif()

# ----------------------------------------------------------------------------
# M5-DSE: cpu_sim CLI 二进制 (Section 7 / M5.16)
# DSE 工具链入口 — 解析 JSON 配置, 构建 CPU, 运行 N cycles, 输出 KEY=VALUE
# 不接入 ctest (CLI 工具, 非测试); nlohmann/json 来自 cf_plugin_link_cpptlm
# 依赖 cf_plugin (含 pipe_builder.h / cpu_factory.h 的传递 include 路径)
# ----------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/tools/cpu_sim/main.cpp)
  add_executable(cpu_sim
    ${CMAKE_SOURCE_DIR}/tools/cpu_sim/main.cpp
  )
  target_link_libraries(cpu_sim PRIVATE cf_plugin)
  target_include_directories(cpu_sim
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  cf_plugin_link_cpptlm(cpu_sim)  # nlohmann/json via CppTLM bundled
endif()

# ----------------------------------------------------------------------------
# 配置摘要
# ----------------------------------------------------------------------------
message(STATUS "  cf_plugin:        Phase 0 (5/5 P0 components) ✅ + M1 框架扩展 (PipeArbitration + arb_ + payload_common)")

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
# [x] M1.5 添加 include/cf/plugin/pipe_arbitration.h + 单元测试 (Phase 1.5)
# [x] M1.7 添加 ip/cpu/core/payload_common.h + 单元测试 (Phase 1.5)
# [ ] 转换 INTERFACE → STATIC (如出现非模板 .cpp) — Phase 0 暂不需要
# [ ] 启用 cpptlm_core 依赖 (Phase 1+ 业务代码需要)
