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
# 单元测试 (使用纯 main() + assert, 暂不引入 Catch2)
# ----------------------------------------------------------------------------
enable_testing()

# P0 #1: PluginBase 生命周期测试
add_executable(test_plugin_lifecycle
  ${CMAKE_SOURCE_DIR}/tests/framework/test_plugin_lifecycle.cpp
)
target_link_libraries(test_plugin_lifecycle PRIVATE cf_plugin)
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)

# P0 #2: Payload<T> 类型安全 Key 测试
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_payload.cpp)
  add_executable(test_payload ${CMAKE_SOURCE_DIR}/tests/framework/test_payload.cpp)
  target_link_libraries(test_payload PRIVATE cf_plugin)
  add_test(NAME test_payload COMMAND test_payload)
endif()

# P0 #3: PipeNode 节点测试
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_node.cpp)
  add_executable(test_pipe_node ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_node.cpp)
  target_link_libraries(test_pipe_node PRIVATE cf_plugin)
  add_test(NAME test_pipe_node COMMAND test_pipe_node)
endif()

# P0 #4: PipeBuilder 编排器测试
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_builder.cpp)
  add_executable(test_pipe_builder ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_builder.cpp)
  target_link_libraries(test_pipe_builder PRIVATE cf_plugin)
  add_test(NAME test_pipe_builder COMMAND test_pipe_builder)
endif()

# P0 #5: CtrlLink 控制 API 测试
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_ctrl_link.cpp)
  add_executable(test_ctrl_link ${CMAKE_SOURCE_DIR}/tests/framework/test_ctrl_link.cpp)
  target_link_libraries(test_ctrl_link PRIVATE cf_plugin)
  add_test(NAME test_ctrl_link COMMAND test_ctrl_link)
endif()

# Phase 0 退出标准: 最小验证 Plugin + 调度确定性
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_hello_plugin.cpp)
  add_executable(test_hello_plugin ${CMAKE_SOURCE_DIR}/tests/framework/test_hello_plugin.cpp)
  target_link_libraries(test_hello_plugin PRIVATE cf_plugin)
  add_test(NAME test_hello_plugin COMMAND test_hello_plugin)
endif()

# Phase 0 §2.3 集成标准: 与 CppTLM ChStreamModuleBase + CppHDL ch::Component 共存
# CppTLM/CppHDL 通过父 CMakeLists.txt 隔离构建到 build/_deps/install/,
# 路径与库文件以变量形式传入(避免 IMPORTED target 验证路径存在性的副作用)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_coexistence.cpp)
  add_executable(test_coexistence ${CMAKE_SOURCE_DIR}/tests/framework/test_coexistence.cpp)
  target_link_libraries(test_coexistence PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_coexistence)
  cf_plugin_link_cpphdl(test_coexistence)
  # CppHDL uses C++20 (std::source_location); CppTLM is C++17 compatible
  set_target_properties(test_coexistence PROPERTIES CXX_STANDARD 20)
  add_test(NAME test_coexistence COMMAND test_coexistence)
endif()

# Phase 1.1: 共享 Bundle 定义单元测试
# bundles/ 位于仓库根,需要把项目根加入 include 路径
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/bundles/test_mem_bundles.cpp)
  add_executable(test_mem_bundles ${CMAKE_SOURCE_DIR}/tests/bundles/test_mem_bundles.cpp)
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
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_plugin_unit.cpp)
  add_executable(test_l1_cache_plugin_unit
    ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_plugin_unit.cpp
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
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_storage.cpp)
  add_executable(test_storage ${CMAKE_SOURCE_DIR}/tests/framework/test_storage.cpp)
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
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_bridge.cpp)
  add_executable(test_l1_cache_bridge
    ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
  )
  target_link_libraries(test_l1_cache_bridge PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_l1_cache_bridge)
  target_include_directories(test_l1_cache_bridge
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/src  # for bridge/ subdir
  )
  add_test(NAME test_l1_cache_bridge COMMAND test_l1_cache_bridge)
endif()

# Phase 1.3b: soc/l1_cache_minimal.json 结构验证 (v2 决策 §4 拓扑)
# TDD: 验证 JSON 结构符合 v2 草案 (traffic_gen → L1CacheTLMBridge → Memory)
# 依赖: soc/l1_cache_minimal.json (本目录上级) + nlohmann/json (CppTLM bundled)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/soc/test_soc_l1_cache_minimal_json.cpp)
  add_executable(test_soc_l1_cache_minimal_json
    ${CMAKE_SOURCE_DIR}/tests/soc/test_soc_l1_cache_minimal_json.cpp
  )
  target_link_libraries(test_soc_l1_cache_minimal_json PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_soc_l1_cache_minimal_json)
  target_include_directories(test_soc_l1_cache_minimal_json
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  add_test(NAME test_soc_l1_cache_minimal_json
    COMMAND test_soc_l1_cache_minimal_json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}  # so soc/l1_cache_minimal.json is found
  )
endif()

# Phase 1.3c: ip/cache/configs/params_schema.json 结构验证
# TDD: 验证 JSON Schema 符合 v2 §3 (D1=C) + L1CachePlugin geometry
# 依赖: ip/cache/configs/params_schema.json + nlohmann/json
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/soc/test_cache_params_schema_json.cpp)
  add_executable(test_cache_params_schema_json
    ${CMAKE_SOURCE_DIR}/tests/soc/test_cache_params_schema_json.cpp
  )
  target_link_libraries(test_cache_params_schema_json PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_cache_params_schema_json)
  target_include_directories(test_cache_params_schema_json
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  add_test(NAME test_cache_params_schema_json
    COMMAND test_cache_params_schema_json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

# Phase 1.3d: L1CachePlugin e2e 测试 (ModuleFactory + JSON 集成)
# TDD: 验证 L1CacheTLMBridgeAdapter 通过 cpptlm::ModuleFactory 注册并实例化
# 依赖: soc/l1_cache_minimal.json + L1CachePlugin + L1CacheTLMBridge + Adapter
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_plugin_e2e.cpp)
  add_executable(test_l1_cache_plugin_e2e
    ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_plugin_e2e.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
  )
  target_link_libraries(test_l1_cache_plugin_e2e PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_l1_cache_plugin_e2e)
  target_include_directories(test_l1_cache_plugin_e2e
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/src
  )
  add_test(NAME test_l1_cache_plugin_e2e
    COMMAND test_l1_cache_plugin_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

# Phase 1.3d-extras: full JSON instantiateAll e2e (DECISION-2026-06-13-01 F3)
# 验证 ChStreamAdapterFactory::registerAdapter<...>("L1CacheTLMBridgeAdapter") 静态注册
# + soc/l1_cache_adapter_e2e.json → ModuleFactory::instantiateAll → 3 模块拓扑连通
# 依赖: soc/l1_cache_adapter_e2e.json + L1CachePlugin + L1CacheTLMBridge + Adapter + chstream_register.hh
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_json_instantiate.cpp)
  add_executable(test_l1_cache_json_instantiate
    ${CMAKE_SOURCE_DIR}/tests/cache/test_l1_cache_json_instantiate.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
  )
  target_link_libraries(test_l1_cache_json_instantiate PRIVATE cf_plugin)
  cf_plugin_link_cpptlm(test_l1_cache_json_instantiate)
  target_include_directories(test_l1_cache_json_instantiate
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/src
  )
  add_test(NAME test_l1_cache_json_instantiate
    COMMAND test_l1_cache_json_instantiate
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

# ----------------------------------------------------------------------------
# Phase 1.5: M1.5 PipeArbitration 单元测试 (蓝图 §6.2.3)
# 验证 cf::plugin::PipeArbitration POD 3-state 派生方法 + 与 PipeNode arb_ 字段集成
# 路径: tests/framework/ (M1 收官后重构, 详见 tests/README.md)
# ----------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_arbitration.cpp)
  add_executable(test_pipe_arbitration ${CMAKE_SOURCE_DIR}/tests/framework/test_pipe_arbitration.cpp)
  target_link_libraries(test_pipe_arbitration PRIVATE cf_plugin)
  add_test(NAME test_pipe_arbitration COMMAND test_pipe_arbitration)
endif()

# ----------------------------------------------------------------------------
# Phase 1.5: M1.7 ip/cpu/core/payload_common.h 单元测试
# 路径: tests/cpu/ (M1 收官后重构)
# ----------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cpu/test_payload_common.cpp)
  add_executable(test_payload_common ${CMAKE_SOURCE_DIR}/tests/cpu/test_payload_common.cpp)
  target_link_libraries(test_payload_common PRIVATE cf_plugin)
  target_include_directories(test_payload_common
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )
  add_test(NAME test_payload_common COMMAND test_payload_common)
endif()

# ----------------------------------------------------------------------------
# Phase 2: M2.9 5/5 P0 Plugin 单元测试
# 路径: tests/cpu/ (CPU 业务家族)
# ----------------------------------------------------------------------------
foreach(test_name IN ITEMS test_reg_file test_hazard test_branch_predictor test_ibus test_dbus)
  if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cpu/${test_name}.cpp)
    add_executable(${test_name} ${CMAKE_SOURCE_DIR}/tests/cpu/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE cf_plugin)
    target_include_directories(${test_name}
      PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endforeach()

# ----------------------------------------------------------------------------
# Phase 2: M3 RISC-V Plugin 单元测试
# ----------------------------------------------------------------------------
foreach(test_name IN ITEMS test_decode test_int_alu test_branch test_mul test_lsu test_decode_full test_int_alu_full test_cpu_factory)
  if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cpu/${test_name}.cpp)
    add_executable(${test_name} ${CMAKE_SOURCE_DIR}/tests/cpu/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE cf_plugin)
    target_include_directories(${test_name}
      PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endforeach()

# ----------------------------------------------------------------------------
# Phase 2: M4 集成测试 (tests/cpu/integration/)
# ----------------------------------------------------------------------------
foreach(test_name IN ITEMS test_5stage_riscv test_3stage_riscv test_demo_soc)
  if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cpu/integration/${test_name}.cpp)
    add_executable(${test_name} ${CMAKE_SOURCE_DIR}/tests/cpu/integration/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE cf_plugin)
    target_include_directories(${test_name}
      PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endforeach()

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
