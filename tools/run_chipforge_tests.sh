#!/usr/bin/env bash
# tools/run_chipforge_tests.sh
#
# 功能描述: 运行 ChipForge 自身测试 (排除 CppHDL 内部测试)
# 状态: PA-4b 缓解 — CppHDL 内部测试在父项目 C++17 模式下 add_executable 失败
# 作者: ChipForge Build System
# 最后修改日期: 2026-06-10
#
# 用法:
#   tools/run_chipforge_tests.sh              # 运行 ChipForge 测试
#   tools/run_chipforge_tests.sh --verbose    # 详细输出
#   tools/run_chipforge_tests.sh --all        # 运行全部测试 (包括 CppHDL, 可能大量 Not Run)
#
# 退出码:
#   0 = ChipForge 测试全部通过
#   非 0 = 有测试失败
#
# 详见:
#   - docs/roadmap/roadmap-status.md §3 PA-4b
#   - CMakeLists.txt (CTest 聚合点)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# 参数解析
VERBOSE=""
INCLUDE_ALL=""
for arg in "$@"; do
    case "$arg" in
        --verbose|-v) VERBOSE="--output-on-failure" ;;
        --all)        INCLUDE_ALL=1 ;;
        --help|-h)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *) echo "Unknown arg: $arg"; exit 1 ;;
    esac
done

# 检查 build 目录
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build/ not found. Run: cmake -B build -S ."
    exit 1
fi

cd "$BUILD_DIR"

# 仅匹配 ChipForge 自身测试 (cf_plugin 8 个 + verify_plugin_decision 1 个)
# 139 个 CppHDL 内部测试因 PA-4b 已知问题在父项目 C++17 下不编译,默认排除
# 恢复: 移除 -R 参数或传 --all,详见 docs/roadmap/roadmap-status.md §3 PA-4b
CHIPFORGE_TEST_REGEX='(test_(payload|pipe_node|pipe_builder|ctrl_link|hello_plugin|coexistence|plugin_lifecycle|mem_bundles|l1_cache_plugin_unit|l1_cache_bridge))|(verify_plugin_decision)'

if [ -n "$INCLUDE_ALL" ]; then
    echo "[run_chipforge_tests] Running ALL 148 tests (139 CppHDL internal expected 'Not Run')..."
    ctest $VERBOSE
else
    echo "[run_chipforge_tests] Running ChipForge tests only (10 tests, expected 100% pass)..."
    ctest $VERBOSE -R "$CHIPFORGE_TEST_REGEX"
fi
