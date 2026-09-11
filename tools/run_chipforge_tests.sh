#!/usr/bin/env bash
# tools/run_chipforge_tests.sh
#
# 功能描述: 运行 ChipForge 自身测试 (默认 259 cases, 66525 assertions)
# 默认路径: delegate 到 build/bin/chipforge_tests (Catch2 v3.7.0)
#           — 涵盖所有 Phase 0+ 测试, 无 whitelist, 无 silent-failure
# --all:    调 ctest (PA-4b 缓解: 139 个 CppHDL 内部测试预期 'Not Run')
# 作者: ChipForge Build System
# 最后修改日期: 2026-07-02
#
# 用法:
#   tools/run_chipforge_tests.sh                       # 运行 ChipForge 全部测试 (259 cases)
#   tools/run_chipforge_tests.sh --build               # 先 build 再 test (调 tools/build.sh)
#   tools/run_chipforge_tests.sh --rebuild-deps        # 重建 deps + build + test
#   tools/run_chipforge_tests.sh --asan                # ASan 构建 + test
#   tools/run_chipforge_tests.sh --source-deps         # 源码嵌入模式 build + test
#   tools/run_chipforge_tests.sh --tag "[cache]"       # 按 Catch2 tag 过滤
#   tools/run_chipforge_tests.sh --exclude "[mmu]"     # 排除某 tag (~[mmu] 传给 Catch2)
#   tools/run_chipforge_tests.sh --verbose             # 详细输出 (--success)
#   tools/run_chipforge_tests.sh --all                 # ctest (含 139 CppHDL internal 'Not Run')
#
# 退出码:
#   0 = 全部通过
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
TEST_BINARY="${BUILD_DIR}/bin/chipforge_tests"

# ---- 默认参数 ----
DO_BUILD=""
REBUILD_DEPS=""
SOURCE_DEPS=""
ASAN=""
VERBOSE=""
INCLUDE_ALL=""
TAG_REGEX=""
TAG_EXCLUDE=""

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
        --tag)
            if [[ -z "$2" ]]; then
                echo "ERROR: --tag requires a Catch2 tag argument (e.g. '[cache]')"
                exit 1
            fi
            TAG_REGEX="$2"
            shift 2
            ;;
        --exclude)
            if [[ -z "$2" ]]; then
                echo "ERROR: --exclude requires a Catch2 tag argument (e.g. '[mmu]')"
                exit 1
            fi
            TAG_EXCLUDE="$2"
            shift 2
            ;;
        --all)
            INCLUDE_ALL=1
            shift
            ;;
        --verbose|-v)
            VERBOSE="--success"
            shift
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

# ---- 2. 检查 build/ ----
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build/ not found. Run: $BUILD_SCRIPT"
    echo "  or:  $0 --build"
    exit 1
fi

# ---- 3a. --all: 调 ctest (PA-4b 缓解路径) ----
# 保留此路径以便观察 139 个 CppHDL 内部测试的 'Not Run' 状态;
# 当 CppHDL 在父项目 C++17 模式下编译修复后, 'Not Run' 会变成真失败, 提示有进展.
if [[ -n "$INCLUDE_ALL" ]]; then
    echo "[run_chipforge_tests] --all: running ctest (139 CppHDL internal expected 'Not Run')..."
    cd "$BUILD_DIR"
    CTEST_FLAGS=()
    [[ -n "$VERBOSE" ]] && CTEST_FLAGS+=("--output-on-failure")
    ctest "${CTEST_FLAGS[@]}"
    exit $?
fi

# ---- 3b. 默认: delegate 到 Catch2 binary ----
# 之前用 ctest -R '$CHIPFORGE_TEST_REGEX' 是个 whitelist, 随测试增长会腐烂 (silent failure).
# 改为直接调 Catch2 binary, 它覆盖所有 chipforge 测试 (无遗漏, 无虚假通过).
if [ ! -x "$TEST_BINARY" ]; then
    echo "ERROR: $TEST_BINARY not found. Run: $BUILD_SCRIPT"
    echo "  or:  $0 --build"
    exit 1
fi

# 构造 Catch2 tag filter
#   --tag "[cache]"            → [cache]
#   --exclude "[mmu]"          → ~[mmu]
#   --tag "[a]" --exclude "[b]" → [a],~[b]
CATCH2_FILTERS=()
[[ -n "$TAG_REGEX" ]]    && CATCH2_FILTERS+=("$TAG_REGEX")
[[ -n "$TAG_EXCLUDE" ]]  && CATCH2_FILTERS+=("~$TAG_EXCLUDE")

cd "$PROJECT_ROOT"  # Catch2 binary 用相对路径读 SoC JSON, 需在 project root

# 摘要打印 (用 --list-tests 拿总数, 不实际运行)
TOTAL_CASES=$(./build/bin/chipforge_tests --list-tests 2>&1 | grep -cE "^  [a-z]")
echo "[run_chipforge_tests] Running Catch2 binary: $TEST_BINARY"
echo "[run_chipforge_tests] Total registered test cases: $TOTAL_CASES"
if [[ ${#CATCH2_FILTERS[@]} -gt 0 ]]; then
    CATCH2_FILTER_STR=$(IFS=','; echo "${CATCH2_FILTERS[*]}")
    echo "[run_chipforge_tests] Catch2 tag filter: $CATCH2_FILTER_STR"
fi

CATCH2_FLAGS=()
[[ -n "$VERBOSE" ]] && CATCH2_FLAGS+=("--success")

if [[ ${#CATCH2_FILTERS[@]} -gt 0 ]]; then
    ./build/bin/chipforge_tests "$CATCH2_FILTER_STR" "${CATCH2_FLAGS[@]}"
else
    ./build/bin/chipforge_tests "${CATCH2_FLAGS[@]}"
fi
