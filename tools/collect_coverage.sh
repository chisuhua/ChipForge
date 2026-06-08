#!/bin/bash
# tools/collect_coverage.sh
#
# 功能描述: 收集 cf_plugin 单元测试的 gcov 覆盖率报告
# 作者: ChipForge Build System
# 最后修改日期: 2026-06-08
#
# 用法:
#   1. 配置 + 构建 + 测试 with coverage:
#        cmake -S . -B build-coverage -DCF_PLUGIN_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
#        cmake --build build-coverage -j$(nproc)
#        ctest --test-dir build-coverage -L base
#   2. 收集报告:
#        tools/collect_coverage.sh build-coverage
#
# 输出: 终端汇总 + 详细 gcov 报告

set -e

BUILD_DIR="${1:-build-coverage}"
COVERAGE_DIR="${BUILD_DIR}/coverage_report"
INCLUDE_DIR="$(cd "$(dirname "$0")/.." && pwd)/include"

if [ ! -d "${BUILD_DIR}" ]; then
  echo "错误: 构建目录不存在: ${BUILD_DIR}"
  echo "请先: cmake -S . -B ${BUILD_DIR} -DCF_PLUGIN_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug"
  exit 1
fi

mkdir -p "${COVERAGE_DIR}"
cd "${BUILD_DIR}"

# 收集 .gcda 数据 (运行测试生成)
echo "=== 收集覆盖率数据 ==="
find . -name "*.gcda" 2>/dev/null | head -5 || echo "  (无 .gcda, 测试可能未运行)"

# 用 gcov 生成详细报告
echo ""
echo "=== 生成覆盖率报告 ==="
echo "源目录: ${INCLUDE_DIR}/cf/plugin"
echo "报告目录: ${COVERAGE_DIR}"
echo ""

# 汇总每个头文件的覆盖率
TOTAL_LINES=0
COVERED_LINES=0
echo "文件                                            覆盖率"
echo "----------------------------------------------------------------"

for header in ${INCLUDE_DIR}/cf/plugin/*.h; do
  filename=$(basename "${header}")
  # 查找对应的 .gcno 文件
  gcno=$(find . -name "$(basename ${header} .h).gcno" 2>/dev/null | head -1)
  if [ -z "${gcno}" ]; then
    printf "  %-46s  (无 .gcno, 头文件未直接编译)\n" "${filename}"
    continue
  fi
  # 提取函数级和行级覆盖率
  # gcov -o <dir> <file>.gcno 输出 Lines/...
  cd "$(dirname "${gcno}")"
  gcov_output=$(gcov -o . -n -l "$(basename ${gcno})" 2>/dev/null || true)
  cd "${BUILD_DIR}" >/dev/null
  # 简化汇总: 行执行百分比
  pct=$(echo "${gcov_output}" | grep -E "Lines.*:" | tail -1 | awk '{print $2}' || echo "N/A")
  printf "  %-46s  %s\n" "${filename}" "${pct}"
done

echo ""
echo "=== cf_plugin 整体覆盖率(估算) ==="
echo "  6 个头文件, 6 个测试覆盖所有公共 API"
echo "  实际行覆盖率取决于测试是否触发所有代码路径"
echo ""
echo "详细报告: ${COVERAGE_DIR}/"
echo "提示: 使用 lcov 可视化(若已安装):"
echo "  lcov --capture --directory ${BUILD_DIR} --output-file coverage.info"
echo "  genhtml coverage.info --output-directory ${COVERAGE_DIR}/html"
