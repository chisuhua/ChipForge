#!/usr/bin/env bash
# tools/run_chipforge_tests.sh
#
# 功能描述: 运行 ChipForge 自身测试
#
# 默认路径 (Oracle 2026-10-06 修订): delegate 到 build/bin/chipforge_tests (TLM Catch2 binary)
#   — TLM 全部 ~515 cases, 无 whitelist, 无 silent-failure
#   — **不**覆盖 chipforge_tests_chmem (CH_MEM/Verilator), 见 --all
#
# ★★★ C1（2026-10-28）「旧产物顶替」假绿通道修复 —— ★ 三条硬约束：
#   ① **默认先 build**（★ 增量）⇒ ★ 产物必是「本轮源码」的 ⇒ ★ 编译失败
#      ⇒ ★ **构建失败即退出**（★ 不跑旧产物）⇒ ★ `--no-build` 是逃生门
#   ② **产物新鲜度 fail-closed**：★ 跳过 build 时校验 binary 的 mtime
#      是否晚于全部编译输入 ⇒ ★ 否则**拒绝运行**（★ 并列出陈旧的源文件）
#   ③ **baseline-aware 判定**：★ 委托 `tools/run_catch2_with_baseline.sh`
#      ⇒ ★ 判定「是否有**新**失败」而非「是否有任何失败」
#      ⇒ ★ 清单 = `tools/baseline_known_failures.txt`
# --all:    调 ctest, 包含 TLM + CH_MEM (含 [verilator]/[mmu-verilator] 4 cases)
#           + cpu_verilator_sim_add (Phase 6d.5 E8 standalone runner smoke)
#           + verify_plugin_decision (PA-4b 缓解: 139 CppHDL internal 预期 'Not Run')
# --tag:    按 Catch2 tag 过滤 (仅作用于默认的 chipforge_tests binary;
#           [verilator]/[mmu-verilator] family 仅在 chipforge_tests_chmem 中,
#           需要配合 --all 或直接跑 ./build/bin/chipforge_tests_chmem [tag])
# 作者: ChipForge Build System
# 最后修改日期: 2026-10-06
#
# 用法:
#   tools/run_chipforge_tests.sh                       # TLM 全部 (411 cases, ~0.6s)
#   tools/run_chipforge_tests.sh --build               # 先 build 再 test (调 tools/build.sh)
#   tools/run_chipforge_tests.sh --rebuild-deps        # 重建 deps + build + test
#   tools/run_chipforge_tests.sh --asan                # ASan 构建 + test
#   tools/run_chipforge_tests.sh --source-deps         # 源码嵌入模式 build + test
#   tools/run_chipforge_tests.sh --tag "[cache]"       # 按 Catch2 tag 过滤 (TLM only)
#   tools/run_chipforge_tests.sh --tag "[cpu]"         # 仅 CPU plugin 测试
#   tools/run_chipforge_tests.sh --exclude "[mmu]"     # 排除某 tag (~[mmu] 传给 Catch2)
#   tools/run_chipforge_tests.sh --verbose             # 详细输出 (--success)
#   tools/run_chipforge_tests.sh --all                 # ctest (TLM + CH_MEM + verilator smoke + gate)
#
# Verilator 后端完整测试 (Phase 6d.5 E8 + 6d P2):
#   tools/run_chipforge_tests.sh --all                 # 推荐 (1 个 ctest + 4 个 Catch2 cases)
#   或:
#   ./build/bin/chipforge_tests_chmem [verilator],[mmu-verilator]   # 4 Catch2 cases (10+26 assertions)
#   ./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-add  # 开发者 debug 入口
#
# 退出码:
#   0 = 全部通过
#   非 0 = 有测试失败
#
# 详见:
#   - `.rddf/roadmap.md` (migrated 2026-10-09: roadmap-status.md 已废弃) §3 PA-4b
#   - CMakeLists.txt (CTest 聚合点)
#   - tools/build.sh (构建入口)
#   - AGENTS.md §CH_MEM 编译开关 + 测试家族表格 ([verilator]/[mmu-verilator] 归属 chipforge_tests_chmem)

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
# ★ C1①：★ 默认**先 build**（增量）⇒ ★ --no-build 是逃生门
SKIP_BUILD=""

# ---- 参数解析 ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)
            DO_BUILD=1
            shift
            ;;
        --no-build)
            SKIP_BUILD=1
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

# ---- 1. build（★ C1①：★ **默认执行**；★ 编译失败即退出 ⇒ 不跑旧产物）----
if [[ -n "$SKIP_BUILD" ]]; then
    echo "[run_chipforge_tests] --no-build: ★ 跳过 build ⇒ 将执行**产物新鲜度校验**"
else
    echo "[run_chipforge_tests] building (★ C1：保证产物 = 本轮源码) ..."
    BUILD_FLAGS=()
    [[ -n "$REBUILD_DEPS" ]] && BUILD_FLAGS+=("$REBUILD_DEPS")
    [[ -n "$SOURCE_DEPS" ]]  && BUILD_FLAGS+=("$SOURCE_DEPS")
    [[ -n "$ASAN" ]]         && BUILD_FLAGS+=("$ASAN")
    if [[ ${#BUILD_FLAGS[@]} -gt 0 ]]; then
        # ★ 显式要求「全量重建 deps / 切编译模式 / ASan」⇒ ★ 走 tools/build.sh
        #   （★ 该路径会 build `all` ⇒ ★ 可能触发 ExternalProject re-configure
        #   ⇒ ★ 既存环境脆性 ⇒ ★ 失败是**响亮的**（set -e ⇒ 退出）⇒ ★ fail-closed ✔）
        "$BUILD_SCRIPT" "${BUILD_FLAGS[@]}"
    else
        # ★ 默认路径：★ configure（幂等）+ **只 build 测试 target**
        #   ⇒ ★ 避开 `all` 的 ExternalProject re-configure 脆性（★ 实测 EXIT=0）
        #   ⇒ ★ 编译失败 ⇒ set -e ⇒ 本脚本退出（★ 不跑旧产物）★ fail-closed ✔
        JOBS_FLAG="-j$(command -v nproc >/dev/null 2>&1 && nproc || echo 2)"
        cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$BUILD_DIR" --target chipforge_tests chipforge_tests_chmem $JOBS_FLAG
    fi
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

# ---- 3b-0. ★ C1②：产物新鲜度 fail-closed ----
# ★ 目的：★ 即使跳过了 build（如 --no-build），★ 也**拒绝**运行陈旧产物
#   ⇒ ★ 因为「旧 binary 静默顶替」= 报告失真的**假绿通道**（★ 本轮实测命中：
#   ★ TLM binary 自 ADR-091 S1 起编译失败 ⇒ 回归跑的是旧产物）。
# ★ 判据：★ binary 的 mtime 必须**晚于全部编译输入**。
if [[ -n "$SKIP_BUILD" ]]; then
    STALE_INPUTS=$(find "$PROJECT_ROOT/include" "$PROJECT_ROOT/ip" "$PROJECT_ROOT/src" \
                        "$PROJECT_ROOT/tests" "$PROJECT_ROOT/bundles" \
                        "$PROJECT_ROOT/CMakeLists.txt" \
                        -newer "$TEST_BINARY" -type f 2>/dev/null | head -20)
    if [[ -n "$STALE_INPUTS" ]]; then
        echo "ERROR: ★ 产物陈旧（★ C1 fail-closed）：★ 以下编译输入比 binary 更新："
        echo "$STALE_INPUTS" | sed 's/^/    /'
        echo "  ⇒ ★ 拒绝运行旧产物（★ 旧产物顶替 = 假绿通道）"
        echo "  ⇒ ★ 请先执行: $0 --build   （或直接 cmake --build build）"
        exit 1
    fi
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
TOTAL_CASES=$(./build/bin/chipforge_tests --list-tests 2>&1 | grep -cE "^  [A-Za-z0-9]")
echo "[run_chipforge_tests] Running Catch2 binary: $TEST_BINARY"
echo "[run_chipforge_tests] Total registered test cases: $TOTAL_CASES"
if [[ ${#CATCH2_FILTERS[@]} -gt 0 ]]; then
    CATCH2_FILTER_STR=$(IFS=','; echo "${CATCH2_FILTERS[*]}")
    echo "[run_chipforge_tests] Catch2 tag filter: $CATCH2_FILTER_STR"
fi

CATCH2_FLAGS=()
[[ -n "$VERBOSE" ]] && CATCH2_FLAGS+=("--success")

# ★ C1③：★ 委托 baseline-aware 运行器（★ 判定「是否有新失败」）
BASELINE_RUNNER="${SCRIPT_DIR}/run_catch2_with_baseline.sh"
if [[ ${#CATCH2_FILTERS[@]} -gt 0 ]]; then
    "$BASELINE_RUNNER" "$TEST_BINARY" "$CATCH2_FILTER_STR" "${CATCH2_FLAGS[@]}"
else
    "$BASELINE_RUNNER" "$TEST_BINARY" "${CATCH2_FLAGS[@]}"
fi
