#!/usr/bin/env bash
# tools/run_chipforge_tests.sh
#
# 功能描述: 运行 ChipForge 自身测试 (排除 CppHDL 内部测试)
# 状态: PA-4b 缓解 — CppHDL 内部测试在父项目 C++17 模式下 add_executable 失败
# 作者: ChipForge Build System
# 最后修改日期: 2026-07-02
#
# 用法:
#   tools/run_chipforge_tests.sh                       # 运行 ChipForge 测试 (默认)
#   tools/run_chipforge_tests.sh --build               # 先 build 再 test (调 tools/build.sh)
#   tools/run_chipforge_tests.sh --rebuild-deps        # 重建 deps + build + test
#   tools/run_chipforge_tests.sh --asan                # ASan 构建 + test
#   tools/run_chipforge_tests.sh --source-deps         # 源码嵌入模式 build + test
#   tools/run_chipforge_tests.sh --verbose             # 详细输出
#   tools/run_chipforge_tests.sh --all                 # 运行全部测试 (包括 CppHDL, 可能大量 Not Run)
#   tools/run_chipforge_tests.sh --tag "[cache]"       # 按 ctest label 过滤
#   tools/run_chipforge_tests.sh --exclude "[mmu]"     # 排除某 label
#
# 退出码:
#   0 = ChipForge 测试全部通过
#   非 0 = 有测试失败
#
# 详见:
#   - docs/roadmap/roadmap-status.md §3 PA-4b
#   - CMakeLists.txt (CTest 聚合点)
#   - tools/build.sh (构建入口)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_SCRIPT="${SCRIPT_DIR}/build.sh"

# ---- 默认参数 ----
VERBOSE=""
INCLUDE_ALL=""
DO_BUILD=""
REBUILD_DEPS=""
SOURCE_DEPS=""
ASAN=""
CTEST_LABEL_REGEX=""
CTEST_LABEL_EXCLUDE=""

# ---- 参数解析 ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)
            DO_BUILD=1
            shift
            ;;
        --rebuild-deps|-r)
            DO_BUILD=1
            REBUILD_DEPS="--rebuild-deps"
            shift
            ;;
        --source-deps|-s)
            DO_BUILD=1
            SOURCE_DEPS="--source-deps"
            shift
            ;;
        --asan)
            DO_BUILD=1
            ASAN="--asan"
            shift
            ;;
        --verbose|-v)
            VERBOSE="--output-on-failure"
            shift
            ;;
        --all)
            INCLUDE_ALL=1
            shift
            ;;
        --tag)
            if [[ -z "$2" ]]; then
                echo "ERROR: --tag requires a label argument"
                exit 1
            fi
            CTEST_LABEL_REGEX="$2"
            shift 2
            ;;
        --exclude)
            if [[ -z "$2" ]]; then
                echo "ERROR: --exclude requires a label argument"
                exit 1
            fi
            CTEST_LABEL_EXCLUDE="$2"
            shift 2
            ;;
        --help|-h)
            sed -n '2,29p' "$0"
            exit 0
            ;;
        *)
            echo "ERROR: Unknown arg: $1"
            echo "Run '$0 --help' for usage."
            exit 1
            ;;
    esac
done

# ---- 1. 必要时先 build ----
if [[ -n "$DO_BUILD" ]]; then
    echo "[run_chipforge_tests] --build: invoking tools/build.sh"
    BUILD_FLAGS=()
    [[ -n "$REBUILD_DEPS" ]] && BUILD_FLAGS+=("$REBUILD_DEPS")
    [[ -n "$SOURCE_DEPS" ]]  && BUILD_FLAGS+=("$SOURCE_DEPS")
    [[ -n "$ASAN" ]]         && BUILD_FLAGS+=("$ASAN")
    "$BUILD_SCRIPT" "${BUILD_FLAGS[@]}"
fi

# ---- 2. 检查 build 目录 ----
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build/ not found. Run: $BUILD_SCRIPT"
    echo "  or:  $0 --build"
    exit 1
fi

cd "$BUILD_DIR"

# ---- 3. 决定 ctest 范围 ----
# 仅匹配 ChipForge 自身测试 (cf_plugin 8 个 + verify_plugin_decision 1 个)
# 139 个 CppHDL 内部测试因 PA-4b 已知问题在父项目 C++17 下不编译,默认排除
# 恢复: 移除 -R 参数或传 --all,详见 docs/roadmap/roadmap-status.md §3 PA-4b
CHIPFORGE_TEST_REGEX='(test_(payload|pipe_node|pipe_builder|ctrl_link|hello_plugin|coexistence|plugin_lifecycle|mem_bundles|l1_cache_plugin_unit|l1_cache_bridge|soc_l1_cache_minimal_json|cache_params_schema_json|l1_cache_plugin_e2e))|(verify_plugin_decision)'

# ctest 参数
CTEST_FLAGS=()
[[ -n "$VERBOSE" ]] && CTEST_FLAGS+=("$VERBOSE")
[[ -n "$CTEST_LABEL_REGEX" ]]    && CTEST_FLAGS+=("-L" "$CTEST_LABEL_REGEX")
[[ -n "$CTEST_LABEL_EXCLUDE" ]]  && CTEST_FLAGS+=("-LE" "$CTEST_LABEL_EXCLUDE")

# ---- 4. 运行 ctest ----
if [ -n "$INCLUDE_ALL" ]; then
    echo "[run_chipforge_tests] Running ALL tests (139 CppHDL internal expected 'Not Run')..."
    ctest "${CTEST_FLAGS[@]}"
else
    echo "[run_chipforge_tests] Running ChipForge tests only (10 tests, expected 100% pass)..."
    CTEST_FLAGS+=("-R" "$CHIPFORGE_TEST_REGEX")
    ctest "${CTEST_FLAGS[@]}"
fi
