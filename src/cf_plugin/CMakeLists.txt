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

# Phase 0 §2.3 集成标准: 与 CppTLM ChStreamModuleBase + CppHDL ch::Component 共存
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_coexistence.cpp)
  add_executable(test_coexistence tests/test_coexistence.cpp)
  target_link_libraries(test_coexistence
    PRIVATE
      cf_plugin
      cpptlm_core
      cpphdl
  )
  target_include_directories(test_coexistence
    PRIVATE
      ${CMAKE_SOURCE_DIR}/CppTLM/include
      ${CMAKE_SOURCE_DIR}/CppTLM/external/json
      ${CMAKE_SOURCE_DIR}/CppHDL/include
  )
  # CppHDL uses C++20 (std::source_location); CppTLM is C++17 compatible
  set_target_properties(test_coexistence PROPERTIES CXX_STANDARD 20)
  add_test(NAME test_coexistence COMMAND test_coexistence)
endif()

# Phase 1.1: 共享 Bundle 定义单元测试
# bundles/ 位于仓库根,需要把项目根加入 include 路径
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mem_bundles.cpp)
  add_executable(test_mem_bundles tests/test_mem_bundles.cpp)
  target_link_libraries(test_mem_bundles PRIVATE cf_plugin)
  target_include_directories(test_mem_bundles
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  add_test(NAME test_mem_bundles COMMAND test_mem_bundles)
endif()

# Phase 1.2: L1CachePlugin 单元测试 (lookup + refill 两阶段, D4 Plugin-style)
# 依赖:
#   - bundles/ (Phase 1.1) - 需 CMAKE_SOURCE_DIR include 路径
#   - ip/cache/tlm/L1CachePlugin.h (Phase 1.2 主实现) - 需 ip/ include 路径
#   - ip/cache/tlm/L1CachePlugin.cpp - 直接编译进测试可执行 (Phase 1.2 单文件 lib)
# TDD: 测试无条件注册, RED 时编译失败暴露缺失实现 (D4 §2.2)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_l1_cache_plugin_unit.cpp)
  add_executable(test_l1_cache_plugin_unit
    tests/test_l1_cache_plugin_unit.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
  )
  target_link_libraries(test_l1_cache_plugin_unit PRIVATE cf_plugin)
  target_include_directories(test_l1_cache_plugin_unit
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  add_test(NAME test_l1_cache_plugin_unit COMMAND test_l1_cache_plugin_unit)
endif()

# Phase 1.3: TLM 存储抽象 + commit 钩子测试 (ADR-040)
# 验证 array_store<T, N> 与 PipeBuilder::register_commit_hook 的集成
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_storage.cpp)
  add_executable(test_storage tests/test_storage.cpp)
  target_link_libraries(test_storage PRIVATE cf_plugin)
  add_test(NAME test_storage COMMAND test_storage)
endif()

# Phase 1.3a: L1CacheTLMBridge 单元测试 (D1' 末尾挂载契约)
# TDD: 测试无条件注册, RED 时编译失败暴露缺失实现
# 依赖:
#   - bundles/ (Phase 1.1) - 需 CMAKE_SOURCE_DIR include 路径
#   - ip/cache/tlm/L1CachePlugin.h + .cpp - 需 ip/ include 路径, 直接编译
#   - src/cf_plugin/bridge/l1_cache_bridge.h + .cpp - 1.3a 待实现
#   - CppTLM cpptlm::StreamAdapterBase (Phase 1.3d 预留接口) - 需 CppTLM include
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_l1_cache_bridge.cpp)
  add_executable(test_l1_cache_bridge
    tests/test_l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
  )
  target_link_libraries(test_l1_cache_bridge PRIVATE cf_plugin cpptlm_core)
  target_include_directories(test_l1_cache_bridge
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/src  # for bridge/ subdir
      ${CMAKE_SOURCE_DIR}/CppTLM/include  # for cpptlm::StreamAdapterBase
  )
  add_test(NAME test_l1_cache_bridge COMMAND test_l1_cache_bridge)
endif()

# Phase 1.3b: soc/l1_cache_minimal.json 结构验证 (v2 决策 §4 拓扑)
# TDD: 验证 JSON 结构符合 v2 草案 (traffic_gen → L1CacheTLMBridge → Memory)
# 依赖: soc/l1_cache_minimal.json (本目录上级) + nlohmann/json (CppTLM bundled)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_soc_l1_cache_minimal_json.cpp)
  add_executable(test_soc_l1_cache_minimal_json
    tests/test_soc_l1_cache_minimal_json.cpp
  )
  target_link_libraries(test_soc_l1_cache_minimal_json PRIVATE cf_plugin cpptlm_core)
  target_include_directories(test_soc_l1_cache_minimal_json
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/CppTLM/external/json  # for nlohmann/json.hpp
  )
  add_test(NAME test_soc_l1_cache_minimal_json
    COMMAND test_soc_l1_cache_minimal_json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}  # so soc/l1_cache_minimal.json is found
  )
endif()

# Phase 1.3c: ip/cache/configs/params_schema.json 结构验证
# TDD: 验证 JSON Schema 符合 v2 §3 (D1=C) + L1CachePlugin geometry
# 依赖: ip/cache/configs/params_schema.json + nlohmann/json
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cache_params_schema_json.cpp)
  add_executable(test_cache_params_schema_json
    tests/test_cache_params_schema_json.cpp
  )
  target_link_libraries(test_cache_params_schema_json PRIVATE cf_plugin cpptlm_core)
  target_include_directories(test_cache_params_schema_json
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/CppTLM/external/json
  )
  add_test(NAME test_cache_params_schema_json
    COMMAND test_cache_params_schema_json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
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
