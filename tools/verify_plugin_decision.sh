#!/bin/bash
# tools/verify_plugin_decision.sh
#
# 功能描述: D4 决策静态检查 (Plugin-style 强制) + ADR-040 移植性检查 + ADR-082 决策门禁
# 作者: ChipForge Build System
# 最后修改日期: 2026-09-27 (新增 Check 5: 核内禁 FPGA 平台宏, 对应 ADR-082 决策 2)
#
# 验证 (D4 from .omo/drafts/decision-plugin-framework-2026-06-08.md):
#   1. 业务代码无 void tick() 重写
#   2. 业务代码无状态机 (enum class State + switch state_)
#   3. Bundle 字段用 cf::plugin::uint_t<N> (非 ch_uint<N> 或 uint64_t 直接)
#
# 验证 (ADR-040 §2.1, 调用 check_plugin_portability.sh):
#   4. at_stage 回调内无 `if (cond) return;` 早返
#   5. ip/*/tlm/ 无 ch_mem / ch_reg / ch_uint / ch::core::context 渗透
#   6. Plugin::build() 内不调用 pb.run()
#   7. 存储声明优先 array_store ([WARN] 鼓励)
#
# 验证 (ADR-082 决策 2 — ASIC 友好, FPGA 用于验证):
#   8. 核内 (ip/*/) 禁止 #ifdef FPGA / XILINX / ALTERA / VIVADO / QUARTUS 平台宏
#      MemoryInterface 抽象是 FPGA/ASIC 差异的唯一收纳点
#
# 退出码: 0 = 全部通过, 1 = 至少一项失败
# 总计: 3 项 D4 检查 + 4 项 ADR-040 可移植性检查 + 1 项 ADR-082 ASIC 检查 = 8 项

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIRS="${ROOT_DIR}/ip"
STRICT_FIRST=1
FAIL_COUNT=0

echo "=== D4 Plugin-style 决策静态检查 ==="
echo "目标目录: ${TARGET_DIRS}"
echo ""

# ----------------------------------------------------------------------------
# Check 1: 业务代码无 void tick() 重写
# 允许 PluginBase 自身定义 private tick() (D4 保护), 但派生类不应重写
# ----------------------------------------------------------------------------
echo "[1/8] 检查业务代码中 void tick() 重写 ..."
TICK_FILES=$(grep -rln "void tick()" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除 cf_plugin 框架本身 (它的 tick() 是 private deleted, 内部保护用)
TICK_FILES=$(echo "${TICK_FILES}" | grep -v "src/cf_plugin" || true)
if [ -z "${TICK_FILES}" ]; then
  echo "  [PASS] 无业务 tick() 重写"
else
  echo "  [FAIL] 发现业务 tick() 重写:"
  echo "${TICK_FILES}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 2: 业务代码无状态机 (enum class State + switch state_)
# D4 要求所有控制流用 at_stage + Payload, 不允许显式状态机
# ADR-046 豁免: 多周期协议引擎 (*_chmem.h 声明 CF_PLUGIN_USE_FSM_EXEMPT)
# 豁免后必须使用 ch_state_machine DSL (check_plugin_portability.sh Check 9 验证)
# ----------------------------------------------------------------------------
echo "[2/8] 检查业务代码中状态机模式 ..."
STATE_MACHINE=$(grep -rlnE "enum class.*State|switch \(state_?\)" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
STATE_MACHINE=$(echo "${STATE_MACHINE}" | grep -v "src/cf_plugin" || true)
# ADR-046: 过滤声明 FSM 豁免的多周期协议引擎文件 (文件内容含
# CF_PLUGIN_USE_FSM_EXEMPT → 用 ch_state_machine DSL, 非裸 enum+switch)
if [ -n "${STATE_MACHINE}" ]; then
  FILTERED=""
  for f in ${STATE_MACHINE}; do
    if ! grep -q "CF_PLUGIN_USE_FSM_EXEMPT" "$f" 2>/dev/null; then
      FILTERED="${FILTERED}${f}"$'\n'
    fi
  done
  STATE_MACHINE="${FILTERED}"
fi
if [ -z "${STATE_MACHINE}" ]; then
  echo "  [PASS] 无业务状态机 (enum class State / switch state_)"
else
  echo "  [FAIL] 发现业务状态机:"
  echo "${STATE_MACHINE}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 3: Bundle 字段必须用 cf::plugin::uint_t<N> (非 ch_uint<N> 或 uintN_t)
# 例外: ip/cpu/tlm/ 中旧式代码 (已废弃但保留兼容)
# ----------------------------------------------------------------------------
echo "[3/8] 检查 Bundle 字段类型 ..."
BUNDLE_VIOLATIONS=$(grep -rlnE "(ch_uint<\d+>|uint(8|16|32|64)_t) +(addr|data|tag|idx|valid|burst_len|is_write)" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除 ip/cpu/tlm/ 旧代码 (Phase 1+ 业务代码用 uint_t<N>)
BUNDLE_VIOLATIONS=$(echo "${BUNDLE_VIOLATIONS}" | grep -v "ip/cpu/tlm" || true)
if [ -z "${BUNDLE_VIOLATIONS}" ]; then
  echo "  [PASS] Bundle 字段用 uint_t<N> (无 ch_uint / uintN_t)"
else
  echo "  [FAIL] Bundle 字段违规 (应用 cf::plugin::uint_t<N>):"
  echo "${BUNDLE_VIOLATIONS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# ADR-040 TLM→HDL 移植性检查 (Tier-1/2 强制 + 鼓励)
# ----------------------------------------------------------------------------
echo "[4/8] 调用 check_plugin_portability.sh ..."
if bash "${ROOT_DIR}/tools/check_plugin_portability.sh"; then
  echo "  [PASS] 移植性检查通过"
else
  echo "  [FAIL] 移植性检查失败"
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 5 (ADR-082 决策 2): 核内禁 FPGA 平台宏
# 防止 FPGA 特定代码渗透到 IP 核内。所有 FPGA/ASIC 差异应通过 MemoryInterface
# 适配层统一处理。核内禁止 XILINX / ALTERA / VIVADO / QUARTUS / FPGA
# 等厂商平台宏。通用综合宏 `__SYNTHESIS__` 不在禁用列表（属于 CppHDL
# 框架通用约定, 非厂商特定）。核外 (soc/tools/cpu_sim/ bridge 适配层) 允许。
# ----------------------------------------------------------------------------
echo "[5/8] 检查核内禁 FPGA 平台宏 (ADR-082 决策 2) ..."
PLATFORM_MACROS=$(grep -rlnE "#[[:space:]]*ifdef[[:space:]]+(XILINX|ALTERA|VIVADO|QUARTUS|FPGA)|#[[:space:]]*ifndef[[:space:]]+(XILINX|ALTERA|VIVADO|QUARTUS|FPGA)" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
if [ -z "${PLATFORM_MACROS}" ]; then
  echo "  [PASS] 核内无 FPGA 平台宏 (XILINX/ALTERA/VIVADO/QUARTUS/FPGA)"
else
  echo "  [FAIL] 核内发现 FPGA 平台宏 (违反 ADR-082 决策 2 ASIC 友好):"
  echo "${PLATFORM_MACROS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 6-8 (ADR-082 决策 1): Plugin::negotiate() capability 协商
# (1) build() 内禁止 dynamic_cast (强制走 negotiate 拿 handle)
# (2) negotiate() 缺依赖 → elaboration fail-fast (covered by unit tests, not grep)
# (3) ADR-082 落地后 11 个 TLM Plugin 必须都有 negotiate() override (默认空实现)
# ----------------------------------------------------------------------------
echo "[6/8] 检查 build() 内禁止 dynamic_cast (ADR-082 决策 1) ..."
DYNAMIC_CAST=$(grep -rnE "dynamic_cast[[:space:]]*<" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除框架层 + bridge 层 (框架允许, 业务不允许)
DYNAMIC_CAST=$(echo "${DYNAMIC_CAST}" | grep -v "include/cf/plugin/" || true)
DYNAMIC_CAST=$(echo "${DYNAMIC_CAST}" | grep -v "src/cf_plugin/" || true)
DYNAMIC_CAST=$(echo "${DYNAMIC_CAST}" | grep -v "src/cf_plugin/bridge/" || true)
# 测试代码允许 (验证机制本身)
DYNAMIC_CAST=$(echo "${DYNAMIC_CAST}" | grep -v "/tests/" || true)
if [ -z "${DYNAMIC_CAST}" ]; then
  echo "  [PASS] 业务 build() 无 dynamic_cast (强制走 negotiate)"
else
  echo "  [FAIL] 业务代码发现 dynamic_cast (ADR-082 决策 1 违反 — 必须用 Plugin::negotiate 拿 handle):"
  echo "${DYNAMIC_CAST}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

echo "[7/8] 检查 Plugin override negotiate() 覆盖率 (ADR-082 决策 1) ..."
# 扫描 ip/ 下所有 .h, 找含 `class .*Plugin.* : public .*Plugin(Base)?` 的类文件
PLUGIN_PATTERN="class[[:space:]]+[A-Za-z_][A-Za-z0-9_]*Plugin[A-Za-z0-9_]*[[:space:]]*:([[:space:]]*public|public)[[:space:]]+[A-Za-z_:]*(PluginBase|Plugin)([[:space:]]|<)"
PLUGIN_TLM_HEADERS=$(grep -rlE "${PLUGIN_PATTERN}" ${TARGET_DIRS} \
  --include="*.h" --include="*.hpp" 2>/dev/null | grep -v "_chmem.h" | grep -v "/tests/" | sort -u || true)
PLUGIN_CHMEM_HEADERS=$(grep -rlE "${PLUGIN_PATTERN}" ${TARGET_DIRS} \
  --include="*.h" --include="*.hpp" 2>/dev/null | grep "_chmem.h" | grep -v "/tests/" | sort -u || true)
TLM_COUNT=$(printf '%s\n' "${PLUGIN_TLM_HEADERS}" | wc -l 2>/dev/null || echo 0)
CHMEM_COUNT=$(printf '%s\n' "${PLUGIN_CHMEM_HEADERS}" | wc -l 2>/dev/null || echo 0)
TOTAL_COUNT=$((TLM_COUNT + CHMEM_COUNT))
MISSING_NEGOTIATE=""
if [ "${TOTAL_COUNT}" -eq 0 ]; then
  echo "  [WARN] 未发现 Plugin 类文件 (ADR-082 框架尚未落地, 跳过覆盖率检查)"
elif [ "${TOTAL_COUNT}" -gt 30 ]; then
  echo "  [WARN] 扫描发现 ${TOTAL_COUNT} 个 Plugin 文件 (TLM=${TLM_COUNT}, CH_MEM=${CHMEM_COUNT}), 超过阈值 30, 检查覆盖率可能被扫描模式误伤"
else
  for f in ${PLUGIN_TLM_HEADERS} ${PLUGIN_CHMEM_HEADERS}; do
    if ! grep -qE "void[[:space:]]+negotiate[[:space:]]*\(" "$f" 2>/dev/null; then
      MISSING_NEGOTIATE="${MISSING_NEGOTIATE}${f}"$'\n'
    fi
  done
  MISSING_TRIMMED=$(echo "${MISSING_NEGOTIATE}" | tr -d '[:space:]')
  if [ -z "${MISSING_TRIMMED}" ]; then
    echo "  [PASS] 扫描 ${TOTAL_COUNT} 个 Plugin (TLM=${TLM_COUNT} + CH_MEM=${CHMEM_COUNT}), 全部 override negotiate()"
  else
    echo "  [WARN] 业务 Plugin 缺 negotiate() override (ADR-082 落地后必须修, 当前 soft → v1.0.0 升硬):"
    echo "${MISSING_NEGOTIATE}" | sed 's/^/    /'
  fi
fi
echo ""

echo "[8/8] 检查 ADR-082 完整度 (consistency check) ..."
ADR_082_FILE="${ROOT_DIR}/docs/architecture/adr/ADR-082-plugin-negotiate-capability.md"
if [ -f "${ADR_082_FILE}" ]; then
  echo "  [PASS] ADR-082 文档存在"
else
  echo "  [FAIL] ADR-082 文档缺失: ${ADR_082_FILE}"
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# 汇总
# ----------------------------------------------------------------------------
if [ ${FAIL_COUNT} -eq 0 ]; then
  echo "=== D4 + ADR-040 + ADR-082 检查全部通过 (8/8) ==="
  exit 0
else
  echo "=== D4 + ADR-040 + ADR-082 检查失败 (${FAIL_COUNT} 项失败) ==="
  exit 1
fi
