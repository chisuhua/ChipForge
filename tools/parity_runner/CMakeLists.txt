# tools/parity_runner/CMakeLists.txt
#
# Double-backend parity gate — Simulator-side CLI runner.
# Mirrors tools/verilator_runner so both backends can be driven with the same
# knobs (contract: docs/architecture/parity-gate-contract.md).

add_executable(parity_sim_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/parity_sim_runner.cpp
)
target_compile_features(parity_sim_runner PRIVATE cxx_std_23)
target_compile_definitions(parity_sim_runner PRIVATE
    CF_PLUGIN_USE_CH_MEM
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_include_directories(parity_sim_runner PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(parity_sim_runner PRIVATE cf_plugin)
cf_plugin_link_cpptlm(parity_sim_runner)
cf_plugin_link_cpphdl(parity_sim_runner)

set_target_properties(parity_sim_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ★ deps 依赖边（★ L1 · change `ci-deps-install-dependency`）★ 详见根 CMakeLists.txt 的
#   「deps 聚合目标」段 ⇒ ★ 等 CppTLM/CppHDL **安装**完成 ⇒ 依赖头文件/库在编译期就位
#   （★ 否则 `cmake --build` 并行 ⇒ 本 target 与 deps-install 无序 ⇒ 竞态失败）
if(TARGET chipforge_deps_install)
    add_dependencies(parity_sim_runner chipforge_deps_install)
endif()
