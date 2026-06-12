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
# 子模块集成 — 隔离构建 (ExternalProject + install)
# ════════════════════════════════════════════════════════════════════
# CppTLM / CppHDL 都在并行开发,且与 ChipForge 是兄弟仓库(通过 symlink 接入)。
# 早期方案是 add_subdirectory() 直接嵌入父项目 — 这导致每次 `cmake --build build`
# 都会递归遍历 CppTLM/CppHDL 源码,如果开发者在 CppTLM 切到不兼容分支就会污染
# ChipForge build,而且 CppTLM/CppHDL 的 build 输出与 ChipForge 混在同一棵
# ctest 树里,无法独立运行。
#
# 新方案:用 ExternalProject_Add 把 CppTLM/CppHDL 隔离到 build/_deps/ 子目录,
# 首次 configure 时 build + install 到 build/_deps/install/,之后默认跳过
# 重 build(检测到 .deps-built marker),仅消费 install/ 下的 lib + headers。
# 主动重 build 方式:cmake -DCHIPFORGE_REBUILD_DEPS=ON -B build
#
# 优势:
#   - CppTLM/CppHDL 源码变动不触发 ChipForge 重 build(除非显式 -D flag)
#   - CppTLM/CppHDL 自身的 tests/examples 完全脱离 ChipForge ctest 树
#   - CppTLM/CppHDL install/ 产物可被其他下游项目(Phase 5+ RISC-V SoC)共享
#
# 限制:
#   - 首次 build 比 add_subdirectory 慢(多一次 cmake configure + 链接)
#   - 调试 CppTLM/CppHDL 时需要 source 模式,见下方 SOURCE_DEPS 选项

option(CHIPFORGE_REBUILD_DEPS "Force rebuild + reinstall CppTLM/CppHDL deps (default: skip if installed)" OFF)
option(CHIPFORGE_SOURCE_DEPS   "Use add_subdirectory() for CppTLM/CppHDL (debugging; bypasses install isolation)" OFF)

set(_deps_install_dir "${CMAKE_BINARY_DIR}/_deps/install")
# 用 install 目录的 lib 文件作为"已完成"marker — 比自建 .deps-built 文件更可靠
# (文件存在 = install 成功,无需维护额外状态)
set(_deps_cpptlm_lib "${_deps_install_dir}/lib/libcpptlm_core.a")
set(_deps_cpphdl_lib "${_deps_install_dir}/lib/libcpphdl.a")
set(_deps_cpptlm_src "${CMAKE_SOURCE_DIR}/CppTLM")
set(_deps_cpphdl_src "${CMAKE_SOURCE_DIR}/CppHDL")
set(_deps_cpptlm_bld "${CMAKE_BINARY_DIR}/_deps/cpptlm-build")
set(_deps_cpphdl_bld "${CMAKE_BINARY_DIR}/_deps/cpphdl-build")

# ─── 源码调试模式:绕过 ExternalProject,直接 add_subdirectory ───
# 仅在显式 -DCHIPFORGE_SOURCE_DEPS=ON 时生效(开发 CppTLM/CppHDL 源码时)
if(CHIPFORGE_SOURCE_DEPS)
    message(STATUS "[ChipForge] SOURCE_DEPS=ON: using add_subdirectory() for CppTLM/CppHDL")
    # CppTLM
    set(BUILD_TESTS       OFF CACHE BOOL "CppTLM tests"        FORCE)
    set(BUILD_EXAMPLES    OFF CACHE BOOL "CppTLM examples"     FORCE)
    set(BUILD_DOCS        OFF CACHE BOOL "CppTLM docs"         FORCE)
    set(BUILD_RTL         OFF CACHE BOOL "CppTLM RTL bridge"   FORCE)
    set(USE_SYSTEMC_STUB  ON  CACHE BOOL "CppTLM SystemC stub" FORCE)
    add_subdirectory(${_deps_cpptlm_src} cpptlm_build)
    # CppHDL
    set(CPPHDL_BUILD_TESTS    OFF CACHE BOOL "ChipForge: build CppHDL tests subtree"    FORCE)
    set(CPPHDL_BUILD_EXAMPLES OFF CACHE BOOL "ChipForge: build CppHDL examples subtree" FORCE)
    set(CH_JIT_ENABLE     OFF CACHE BOOL "CppHDL JIT (requires LLVM)" FORCE)
    add_subdirectory(${_deps_cpphdl_src} cpphdl_build)
else()
    # ─── 默认:ExternalProject 隔离模式 ───
    # 1) 决定是否需要触发 ExternalProject build
    #    触发条件:任一 lib 不存在 OR CHIPFORGE_REBUILD_DEPS=ON
    set(_rebuild_deps FALSE)
    if(CHIPFORGE_REBUILD_DEPS)
        set(_rebuild_deps TRUE)
        message(STATUS "[ChipForge] CHIPFORGE_REBUILD_DEPS=ON: forcing CppTLM/CppHDL rebuild")
    elseif(NOT EXISTS "${_deps_cpptlm_lib}" OR NOT EXISTS "${_deps_cpphdl_lib}")
        set(_rebuild_deps TRUE)
        message(STATUS "[ChipForge] CppTLM/CppHDL install incomplete, will build on first run")
    else()
        message(STATUS "[ChipForge] Using cached CppTLM/CppHDL install at ${_deps_install_dir}")
        message(STATUS "[ChipForge] Pass -DCHIPFORGE_REBUILD_DEPS=ON to force rebuild")
    endif()

    include(ExternalProject)

    # 2) CppTLM 隔离构建 (产物:libcpptlm_core.a + include/cpp-tlm/)
    ExternalProject_Add(cpptlm_external
        SOURCE_DIR       "${_deps_cpptlm_src}"
        BINARY_DIR       "${_deps_cpptlm_bld}"
        INSTALL_DIR      "${_deps_install_dir}"
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_TESTS=OFF
            -DBUILD_EXAMPLES=OFF
            -DBUILD_DOCS=OFF
            -DBUILD_RTL=OFF
            -DUSE_SYSTEMC_STUB=ON
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        BUILD_ALWAYS      FALSE
        BUILD_BYPRODUCTS  "${_deps_install_dir}/lib/libcpptlm_core.a"
        STEP_TARGETS      build install
    )

    # 3) CppHDL 隔离构建 (产物:libcpphdl.a + include/cpp-hdl/)
    ExternalProject_Add(cpphdl_external
        SOURCE_DIR       "${_deps_cpphdl_src}"
        BINARY_DIR       "${_deps_cpphdl_bld}"
        INSTALL_DIR      "${_deps_install_dir}"
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCPPHDL_BUILD_TESTS=OFF
            -DCPPHDL_BUILD_EXAMPLES=OFF
            -DCH_JIT_ENABLE=OFF
            -DBUILD_VERILATOR=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        BUILD_ALWAYS      FALSE
        BUILD_BYPRODUCTS  "${_deps_install_dir}/lib/libcpphdl.a"
        STEP_TARGETS      build install
    )

    # 暴露给 cf_plugin 消费的路径变量(不用 IMPORTED target,因为它会立即验证
    # 路径存在性 — 首次 configure 时 _deps/install/ 还不存在,会让 configure 失败)
    # 注意: CppTLM/CppHDL 头文件内部用 #include "xxx.h" 风格(无子目录前缀),
    # 必须在每个子目录(include/cpp-tlm/core, include/cpp-hdl/core 等)分别
    # 加到 include 路径,否则嵌套 #include 找不到兄弟头文件
    set(CHIPFORGE_CPPTLM_INCLUDE_DIRS
        "${_deps_install_dir}/include/cpp-tlm"
        "${_deps_install_dir}/include/cpp-tlm/core"
        "${_deps_install_dir}/include/cpp-tlm/bundles"
        "${_deps_install_dir}/include/cpp-tlm/tlm"
        "${_deps_install_dir}/include/cpp-tlm/external/json"
        "${_deps_install_dir}/include/cpp-tlm/external"
    )
    set(CHIPFORGE_CPPTLM_LIBRARY "${_deps_install_dir}/lib/libcpptlm_core.a")
    set(CHIPFORGE_CPPHDL_INCLUDE_DIRS
        "${_deps_install_dir}/include/cpp-hdl"
        "${_deps_install_dir}/include/cpp-hdl/core"
        "${_deps_install_dir}/include/cpp-hdl/ast"
        "${_deps_install_dir}/include/cpp-hdl/bundle"
        "${_deps_install_dir}/include/cpp-hdl/axi4"
        "${_deps_install_dir}/include/cpp-hdl/utils"
        "${_deps_install_dir}/include/cpp-hdl/bv"
        "${_deps_install_dir}/include/cpp-hdl/cpu"
        # 不加 chlib/: 它里面有 assert.h,与 system <assert.h> 冲突
        #  (gcc 13.3 <cassert> 内部 include <assert.h>,会被 -I chlib 路径误命中)
        "${_deps_install_dir}/include/cpp-hdl/lnode"
    )
    set(CHIPFORGE_CPPHDL_LIBRARY "${_deps_install_dir}/lib/libcpphdl.a")

    # 4) 控制实际 build 时机
    #    _rebuild_deps=TRUE → 加 add_dependencies 让 `cmake --build build` 时
    #                          真正构建 CppTLM/CppHDL
    #    _rebuild_deps=FALSE → 跳过(默认,让 ChipForge 自身 target 构建即可)
    if(_rebuild_deps)
        # 当 CHIPFORGE_REBUILD_DEPS=ON 或首次 build,强制把 ExternalProject 的
        # build/install 阶段加入 all target
        ExternalProject_Add_StepTargets(cpptlm_external build install)
        ExternalProject_Add_StepTargets(cpphdl_external build install)
    else()
        # 已 install,创建空目标,占位让 add_dependencies 通过
        if(NOT TARGET chipforge_deps_skip)
            add_custom_target(chipforge_deps_skip ALL
                COMMAND ${CMAKE_COMMAND} -E echo "CppTLM/CppHDL install cached at ${_deps_install_dir}, skipping rebuild. Pass -DCHIPFORGE_REBUILD_DEPS=ON to override."
            )
        endif()
    endif()
endif()

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

# ----------------------------------------------------------------------------
# 静态检查 (D4 决策强制): 无 test pass, exit code 反映合规
# ----------------------------------------------------------------------------
add_test(
  NAME verify_plugin_decision
  COMMAND ${CMAKE_SOURCE_DIR}/tools/verify_plugin_decision.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

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
if(CHIPFORGE_SOURCE_DEPS)
    message(STATUS "  CppTLM:          v2.1.0  (SOURCE_DEPS=ON; add_subdirectory)")
    message(STATUS "  CppHDL:          v1.0.0  (SOURCE_DEPS=ON; add_subdirectory)")
elseif(_rebuild_deps)
    message(STATUS "  CppTLM:          v2.1.0  (ExternalProject: building + installing)")
    message(STATUS "  CppHDL:          v1.0.0  (ExternalProject: building + installing)")
else()
    message(STATUS "  CppTLM:          v2.1.0  (ExternalProject: cached, no rebuild)")
    message(STATUS "  CppHDL:          v1.0.0  (ExternalProject: cached, no rebuild)")
endif()
message(STATUS "  Install prefix:  ${_deps_install_dir}")
message(STATUS "  ccache:          ${CCACHE_PROGRAM}")
message(STATUS "  ASan:            ${ENABLE_ASAN}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "")

# compile_commands.json (LSP / clangd 友好)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
