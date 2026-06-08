# ChipForge 根 CMakeLists.txt
# Phase 0+ 集成: CppTLM (TLM 框架) + cf::plugin 框架
# 详见 docs/roadmap/roadmap-status.md 任务 PA-4
# 上一版本 (STUB 0.0.0) 已被本版本 (Phase 0+ 0.0.1) 取代
#
# 注意: CppHDL 集成已暂时禁用 — CppHDL/tests/CMakeLists.txt:38
# 有上游 bug (../../ 应为 ../),待 CppHDL 修复后重新启用。
# 阻塞项记录: docs/roadmap/roadmap-status.md §3 PA-4 + 决策日志

cmake_minimum_required(VERSION 3.20)

project(ChipForge
    VERSION 0.0.1
    DESCRIPTION "RISC-V Virtual Verification Platform (Phase 0+)"
    LANGUAGES CXX
)

# ════════════════════════════════════════════════════════════════════
# C++ 标准
# - 父项目用 C++17 (CppTLM 兼容; Phase 0 业务代码用 C++17 足够)
# - CppTLM 内部用 C++17
# - CppHDL 集成后,父项目将升级到 C++20
# ════════════════════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ════════════════════════════════════════════════════════════════════
# ccache 加速 (可选,自动检测)
# ════════════════════════════════════════════════════════════════════
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found — install for faster builds (apt install ccache)")
endif()

# ════════════════════════════════════════════════════════════════════
# AddressSanitizer (可选,推荐 Debug 模式启用)
# ════════════════════════════════════════════════════════════════════
option(ENABLE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "ASan enabled")
endif()

# ════════════════════════════════════════════════════════════════════
# 编译选项 — 显式 target 级别
# (子模块各自有自己的 add_compile_options;不在父级污染以免冲突)
# ChipForge 自身目标会在对应子目录的 CMakeLists.txt 中设置
# ════════════════════════════════════════════════════════════════════

# ════════════════════════════════════════════════════════════════════
# 子模块集成
# ════════════════════════════════════════════════════════════════════

# ─── CppTLM v2.1.0: TLM 框架 ✅ 已集成 ───
# 关闭 tests/examples/docs/RTL-bridge 以减少构建时间
# USE_SYSTEMC_STUB=ON 是默认(TLM stub,不依赖外部 SystemC)
set(BUILD_TESTS       OFF CACHE BOOL "CppTLM tests"        FORCE)
set(BUILD_EXAMPLES    OFF CACHE BOOL "CppTLM examples"     FORCE)
set(BUILD_DOCS        OFF CACHE BOOL "CppTLM docs"         FORCE)
set(BUILD_RTL         OFF CACHE BOOL "CppTLM RTL bridge"   FORCE)
set(USE_SYSTEMC_STUB  ON  CACHE BOOL "CppTLM SystemC stub" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/CppTLM cpptlm_build)

# ─── CppHDL v1.0.0: HDL 框架 ⏸️ 暂时禁用 ───
# 阻塞原因: CppHDL/tests/CMakeLists.txt:38 引用 `../../examples/...` 路径错误
#   (在 CppHDL 作为子目录时,../../ 出 CppHDL 范围;实际路径应在 ../examples/...)
# 状态: 上游 bug,见 CppHDL AGENTS.md 备注 "riscv-mini: API updates needed"
# 影响: CppHDL 整个 add_subdirectory 失败,阻塞 HDL 框架集成
# 决策: Phase 0 暂不需 CppHDL (Plugin 脚手架是纯 C++17 框架)
#       Phase 1+ 再评估 (可能采用独立构建 + find_package)
#
# 恢复方法 (CppHDL 修复后):
#   set(CH_JIT_ENABLE OFF CACHE BOOL "CppHDL JIT" FORCE)
#   add_subdirectory(${CMAKE_SOURCE_DIR}/CppHDL cpphdl_build)

# ════════════════════════════════════════════════════════════════════
# ChipForge 自身目标 (按 Phase 顺序逐步添加)
# ════════════════════════════════════════════════════════════════════
# Phase 0:  Plugin 脚手架 (cf::plugin::{PluginBase,Payload,PipeNode,PipeBuilder,CtrlLink})
# Phase 1:  ip/cache/ (L1CachePlugin)
# Phase 1+: ip/cpu/, ip/memory/, ip/interconnect/, ip/peripheral/
# Phase 2+: soc/ (RISC-V SoC 集成)

# 当前为占位: 实际 add_subdirectory 在对应 Phase 实施时启用
add_subdirectory(src/cf_plugin)   # Phase 0  ✅ 已就位 (空占位)
# add_subdirectory(ip)              # Phase 1+  待 ip/{cache,cpu,...} 内容填充后启用
# add_subdirectory(soc)             # Phase 1+  待 SoC 集成代码后启用

# CTest 聚合: 根级 enable_testing() 使 ctest 收集所有子目录的 add_test
# (子目录 src/cf_plugin/CMakeLists.txt 自身有 enable_testing(), 此处冗余声明为安全)
enable_testing()

# ════════════════════════════════════════════════════════════════════
# 输出目录
# ════════════════════════════════════════════════════════════════════
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ════════════════════════════════════════════════════════════════════
# 配置摘要
# ════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "ChipForge ${PROJECT_VERSION} Configuration")
message(STATUS "  C++ Standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "  CppTLM:          v2.1.0  (tests/examples/docs/rtl-bridge off)")
message(STATUS "  CppHDL:          DISABLED (upstream tests/CMakeLists.txt bug)")
message(STATUS "  ccache:          ${CCACHE_PROGRAM}")
message(STATUS "  ASan:            ${ENABLE_ASAN}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "")

# compile_commands.json (LSP / clangd 友好)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
