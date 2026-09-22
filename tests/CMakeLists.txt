# tests/CMakeLists.txt
# 单二进制 Catch2 测试套件（与 CppTLM 模式一致）
# 设计原则: 测试自动发现, 新增 test_*.cpp 无需修改本文件

# 收集所有测试源文件（含子目录, RECURSE 处理 cpu/integration/ 和 cpu/configs/）
file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*/test_*.cpp"
)

# 关键: cache + mmu 测试需要的实现 .cpp 源文件
# 这些 .cpp 之前在 src/cf_plugin/CMakeLists.txt 中被显式编译进独立 test target
# 单二进制模式下, 必须显式列出以确保链接找到符号
# (cf_plugin 是 INTERFACE 库, 不包含 .cpp 实现)
set(CACHE_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/policies/replacement_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/mmu_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/mmu_bridge_adapter.cpp  # mmu-cache-integration commit 4
)
list(APPEND TEST_SOURCES ${CACHE_IMPL_SOURCES})

# MMU 库实装源文件 (mmu-tlb-ptw-impl change 解阻塞)
# cf_plugin 是 INTERFACE 库, 不编译 .cpp; mmu 库实现必须显式加入
# 注: MMUPlugin.cpp 含 skeleton 阶段 stub 代码 (auto* n = pb.node_of_logic_stage(...) API 错用)
#     commit 4 才完整重写 at_stage 闭包. commit 2 仅加入 lib/ + policies/ 4 个 .cpp
# commit 4 加入 MMUPlugin.cpp (at_stage 闭包实装)
set(MMU_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/multi_level_tlb.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/ptw.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/tlb_factory.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/policies/tlb_replacement_policy.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/tlm/MMUPlugin.cpp  # commit 7 (mmu-tlb-ptw-impl) 加入
    ${CMAKE_SOURCE_DIR}/ip/cpu/plugins/mmu.cpp    # mmu-cache-integration commit 6 (sfence_vma + csr_write_satp 实装)
)
list(APPEND TEST_SOURCES ${MMU_IMPL_SOURCES})

# mmu tests re-enabled in mmu-tlb-ptw-impl commit 10 (skeleton stub completed)

# 把 catch_amalgamated.cpp 加入编译（提供默认 main）
list(APPEND TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch_amalgamated.cpp"
)

# 单二进制 chipforge_tests
add_executable(chipforge_tests ${TEST_SOURCES})
# CppHDL 使用 C++20+ 特性 (std::source_location, std::bit_width, consteval)
# chipforge_tests 通过 cf_plugin_link_cpphdl 链接 CppHDL 头, 所以本 target 需 C++23 (v0.6 ADR-047)
target_compile_features(chipforge_tests PRIVATE cxx_std_23)
target_compile_definitions(chipforge_tests PRIVATE
    TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
    RV32UI_CSV_PATH="${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv"
)

# tests/ 子目录加入 include 路径（让 #include "catch_amalgamated.hpp" 解析）
# catch_amalgamated.hpp 在 tests/catch2/ 子目录, 必须显式加进去
target_include_directories(chipforge_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/catch2
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)

# 链接 cf_plugin 框架 (头文件 only INTERFACE 库)
target_link_libraries(chipforge_tests PRIVATE
    cf_plugin
)
# bridge .cpp 已经在 CACHE_IMPL_SOURCES 中显式编译进 chipforge_tests,
# 不需要链接 cf_plugin_bridge 库 (它不是真实 target)
# 通过 helper 函数附加 CppTLM 和 CppHDL 依赖
# (这些函数同时链接库和添加 include 路径, 避免与 target_link_libraries 重复)
cf_plugin_link_cpptlm(chipforge_tests)
cf_plugin_link_cpphdl(chipforge_tests)

# 启用 ASan (可选, 跟随父选项)
if(ENABLE_ASAN)
    target_compile_options(chipforge_tests PRIVATE --coverage)
    target_link_options(chipforge_tests PRIVATE --coverage)
endif()

# 让测试可执行文件找到项目根目录（soc JSON, manual_elf 等）
target_compile_definitions(chipforge_tests PRIVATE
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# 设置运行时输出目录
set_target_properties(chipforge_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到 ctest（单测试）
add_test(NAME chipforge_tests COMMAND chipforge_tests)
set_tests_properties(chipforge_tests PROPERTIES LABELS "unit;integration;catch2")

# 让 ctest 可执行文件找到项目根
set_tests_properties(chipforge_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ----------------------------------------------------------------------------
# chipforge_tests_chmem: 第二测试 target, 编译选项 -DCF_PLUGIN_USE_CH_MEM=ON
# 验证框架在 CH_MEM 模式下也能 build 通过 (Phase 6c M2 Spike 3)
# 注: 仅编译 framework/ 测试 (M2/W3 之前 CPU/Cache 测试未适配 CH_MEM)
# ----------------------------------------------------------------------------
set(CHMEM_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_cppHDL_hello_poc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_elaborate_connector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_elaborate_pipeline2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_payload_store_miss_throws.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_rtl_regfile_alu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_5stage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_decoder_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_memory_model_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_decoded_inst_migration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_vendored_elf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_verilator_sim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_mmu_ptw_fsm_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/test_l1cache_refill_fsm_chmem.cpp
)
# 同样需要 catch_amalgamated.cpp 提供 main
list(APPEND CHMEM_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch_amalgamated.cpp"
)

add_executable(chipforge_tests_chmem ${CHMEM_TEST_SOURCES})
target_compile_features(chipforge_tests_chmem PRIVATE cxx_std_23)
target_compile_definitions(chipforge_tests_chmem PRIVATE
    CF_PLUGIN_USE_CH_MEM
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
    CF_VERILATOR_SIM_BIN="${CMAKE_BINARY_DIR}/bin/cpu_verilator_sim"
)
target_include_directories(chipforge_tests_chmem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/catch2
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(chipforge_tests_chmem PRIVATE cf_plugin)
cf_plugin_link_cpptlm(chipforge_tests_chmem)
cf_plugin_link_cpphdl(chipforge_tests_chmem)

set_target_properties(chipforge_tests_chmem PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LINK_FLAGS "-Wl,--allow-multiple-definition"
)
add_test(NAME chipforge_tests_chmem COMMAND chipforge_tests_chmem)
set_tests_properties(chipforge_tests_chmem PROPERTIES LABELS "unit;chmem")
set_tests_properties(chipforge_tests_chmem PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)