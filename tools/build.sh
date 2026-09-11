#!/usr/bin/env bash
# tools/build.sh
#
# 功能描述: ChipForge 构建入口 (cmake configure + build)
# 作者: ChipForge Build System
# 最后修改日期: 2026-07-02
#
# 用法:
#   tools/build.sh                          # 默认 Release configure + build
#   tools/build.sh --type Debug             # Debug 构建
#   tools/build.sh --asan                   # ASan 调试 (ENABLE_ASAN=ON + Debug)
#   tools/build.sh --rebuild-deps           # 重新构建 CppTLM/CppHDL 依赖
#   tools/build.sh --source-deps            # 源码嵌入模式 (CppTLM/CppHDL add_subdirectory)
#   tools/build.sh --clean                  # 仅清理 build/_deps (强制重建依赖)
#   tools/build.sh --reset                  # 清理整个 build/ (完全重置)
#   tools/build.sh --no-build               # 仅 configure, 不 build
#   tools/build.sh --jobs N | -j N          # 并行任务数 (默认 nproc)
#   tools/build.sh --verbose                # cmake --verbose 输出
#   tools/build.sh --help                   # 显示此帮助
#
# 退出码:
#   0 = 构建成功
#   非 0 = 构建失败
#
# 详见:
#   - AGENTS.md §构建与测试 (构建模式表)
#   - CMakeLists.txt (top-level)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# ---- 默认参数 ----
BUILD_TYPE="Release"
REBUILD_DEPS=""
SOURCE_DEPS=""
ASAN=""
CLEAN_DEPS=""
RESET=""
JOBS=""
VERBOSE=""
NO_BUILD=""

# ---- 参数解析 ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --asan)
            ASAN="-DENABLE_ASAN=ON"
            BUILD_TYPE="Debug"  # ASan 隐含要求 Debug, 覆盖 --type
            shift
            ;;
        --rebuild-deps|-r)
            REBUILD_DEPS="-DCHIPFORGE_REBUILD_DEPS=ON"
            shift
            ;;
        --source-deps|-s)
            SOURCE_DEPS="-DCHIPFORGE_SOURCE_DEPS=ON"
            shift
            ;;
        --clean)
            CLEAN_DEPS=1
            shift
            ;;
        --reset)
            RESET=1
            shift
            ;;
        --type)
            if [[ -z "$2" || "$2" == --* ]]; then
                echo "ERROR: --type requires an argument (Debug|Release|RelWithDebInfo|MinSizeRel)"
                exit 1
            fi
            # 如果同时 --asan, 不覆盖 ASan 决定的 Debug
            if [[ -z "$ASAN" ]]; then
                BUILD_TYPE="$2"
            fi
            shift 2
            ;;
        --no-build)
            NO_BUILD=1
            shift
            ;;
        --verbose|-v)
            VERBOSE="--verbose"
            shift
            ;;
        --jobs|-j)
            if [[ -z "$2" || "$2" == --* ]]; then
                echo "ERROR: --jobs requires an argument"
                exit 1
            fi
            JOBS="-j $2"
            shift 2
            ;;
        --help|-h)
            sed -n '2,33p' "$0"
            exit 0
            ;;
        *)
            echo "ERROR: Unknown arg: $1"
            echo "Run '$0 --help' for usage."
            exit 1
            ;;
    esac
done

# ---- 派生默认值 ----
if [[ -z "$JOBS" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS="-j$(nproc)"
    else
        JOBS="-j2"  # macOS/其他 fallback
    fi
fi

# ---- 主流程 ----
cd "$PROJECT_ROOT"

# 1. 完全重置
if [[ -n "$RESET" ]]; then
    echo "[build.sh] --reset: removing build/"
    rm -rf "$BUILD_DIR"
fi

# 2. 仅清理 deps (强制重建 CppTLM/CppHDL)
if [[ -n "$CLEAN_DEPS" ]]; then
    echo "[build.sh] --clean: removing build/_deps/"
    rm -rf "${BUILD_DIR}/_deps"
fi

# 3. cmake configure
CMAKE_FLAGS=(
    "-S" "."
    "-B" "$BUILD_DIR"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
)
[[ -n "$ASAN" ]]          && CMAKE_FLAGS+=("$ASAN")
[[ -n "$REBUILD_DEPS" ]]  && CMAKE_FLAGS+=("$REBUILD_DEPS")
[[ -n "$SOURCE_DEPS" ]]   && CMAKE_FLAGS+=("$SOURCE_DEPS")
[[ -n "$VERBOSE" ]]       && CMAKE_FLAGS+=("$VERBOSE")

echo "[build.sh] cmake configure: ${CMAKE_FLAGS[*]}"
cmake "${CMAKE_FLAGS[@]}"

# 4. cmake build (除非 --no-build)
if [[ -z "$NO_BUILD" ]]; then
    echo "[build.sh] cmake --build $JOBS"
    cmake --build "$BUILD_DIR" $JOBS
    echo "[build.sh] build complete: $BUILD_DIR"
else
    echo "[build.sh] --no-build set, configure only"
fi
