#!/usr/bin/env bash
# ============================================================================
# verify_adr.sh - Architecture Decision Register Verification
# ============================================================================
# 用途：验证 CppTLM/CppHDL/ChipForge 代码实现是否与 docs/architecture/adr.md
#       中的架构设计决策保持一致
#
# 用法：
#   bash tools/verify_adr.sh                    # 验证全部
#   bash tools/verify_adr.sh --category=A       # 仅验证某类别
#   bash tools/verify_adr.sh --only=ADR-001     # 仅验证单条
#   VERBOSE=1 bash tools/verify_adr.sh          # 详细输出
#
# 退出码：
#   0 - 所有 ✅ 决策通过；无 Critical 漂移
#   1 - 至少一个 ✅ 决策验证失败（Critical 漂移）
#   2 - 脚本自身错误
#
# 对应文档：docs/architecture/adr.md
# ============================================================================

set -uo pipefail

# ----------------------------------------------------------------------------
# 配置
# ----------------------------------------------------------------------------
CHIPFORGE_ROOT="${CHIPFORGE_ROOT:-/workspace/project/ChipForge}"
CPPTLM="$CHIPFORGE_ROOT/CppTLM"
CPPHDL="$CHIPFORGE_ROOT/CppHDL"
INCLUDE_CPPTLM="$CPPTLM/include"
INCLUDE_CPPHDL="$CPPHDL/include"
ADR_DOC="$CHIPFORGE_ROOT/docs/architecture/adr.md"

# 类别定义
declare -A CATEGORY_NAMES=(
  [A]="框架架构"
  [B]="CppTLM 模块/接口"
  [C]="CppHDL 模块/接口"
  [D]="注册与发现"
  [E]="端口与信号"
  [F]="Bundle 与协议"
  [G]="声明式 Plugin 模型"
  [H]="流水线抽象"
  [I]="验证框架"
  [J]="目录与组织"
)

# 颜色（仅在 TTY 时启用）
if [[ -t 1 ]]; then
  C_GREEN='\033[0;32m'
  C_YELLOW='\033[0;33m'
  C_RED='\033[0;31m'
  C_BLUE='\033[0;34m'
  C_GRAY='\033[0;90m'
  C_RESET='\033[0m'
else
  C_GREEN=''; C_YELLOW=''; C_RED=''; C_BLUE=''; C_GRAY=''; C_RESET=''
fi

VERBOSE="${VERBOSE:-0}"
CATEGORY_FILTER=""
ONLY_FILTER=""

# ----------------------------------------------------------------------------
# 参数解析
# ----------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --category=*) CATEGORY_FILTER="${1#*=}" ;;
    --only=*)     ONLY_FILTER="${1#*=}" ;;
    --verbose|-v) VERBOSE=1 ;;
    --help|-h)
      sed -n '2,30p' "$0"
      exit 0
      ;;
    *)
      echo -e "${C_RED}Unknown argument: $1${C_RESET}" >&2
      exit 2
      ;;
  esac
  shift
done

# ----------------------------------------------------------------------------
# 计数器
# ----------------------------------------------------------------------------
PASS=0
EXPECTED_MISSING=0
STALE=0
FAILED=0
declare -a RESULTS=()

# ----------------------------------------------------------------------------
# 日志函数
# ----------------------------------------------------------------------------
log_pass() {
  local id="$1" title="$2"
  echo -e "${C_GREEN}[$id] ✅ PASS${C_RESET}  $title"
  RESULTS+=("PASS|$id|$title")
  ((PASS++))
}

log_expected_missing() {
  local id="$1" title="$2" reason="${3:-符合 Phase 1 提案}"
  echo -e "${C_BLUE}[$id] 🚧 EXPECTED_MISSING${C_RESET}  $title ($reason)"
  RESULTS+=("EXPECTED|$id|$title")
  ((EXPECTED_MISSING++))
}

log_stale() {
  local id="$1" title="$2" detail="$3"
  echo -e "${C_YELLOW}[$id] ⚠️  STALE${C_RESET}  $title"
  [[ -n "$detail" ]] && echo "    ${C_GRAY}→ $detail${C_RESET}"
  RESULTS+=("STALE|$id|$title|$detail")
  ((STALE++))
}

log_failed() {
  local id="$1" title="$2" detail="${3:-}"
  echo -e "${C_RED}[$id] ❌ FAILED${C_RESET}  $title"
  [[ -n "$detail" ]] && echo "    ${C_GRAY}→ $detail${C_RESET}"
  RESULTS+=("FAILED|$id|$title|$detail")
  ((FAILED++))
}

log_verbose() {
  [[ "$VERBOSE" -eq 1 ]] && echo -e "    ${C_GRAY}→ $1${C_RESET}" || true
}

should_run() {
  local id="$1"
  [[ -z "$ONLY_FILTER" || "$ONLY_FILTER" == "$id" ]] || return 1
  if [[ -n "$CATEGORY_FILTER" ]]; then
    local cat
    cat=$(category_of_adr "$id")
    [[ "$cat" == "$CATEGORY_FILTER" ]] || return 1
  fi
  return 0
}

category_of_adr() {
  case "$1" in
    ADR-001|ADR-002|ADR-003) echo A ;;
    ADR-004|ADR-005|ADR-006|ADR-007|ADR-008) echo B ;;
    ADR-009|ADR-010|ADR-011|ADR-012|ADR-013|ADR-014) echo C ;;
    ADR-015|ADR-016|ADR-017) echo D ;;
    ADR-018|ADR-019|ADR-020) echo E ;;
    ADR-021|ADR-022|ADR-023|ADR-024) echo F ;;
    ADR-025|ADR-026|ADR-027|ADR-028|ADR-029) echo G ;;
    ADR-030|ADR-031|ADR-032|ADR-033) echo H ;;
    ADR-034|ADR-035|ADR-036) echo I ;;
    ADR-037|ADR-038) echo J ;;
    *) echo "?" ;;
  esac
}

# ----------------------------------------------------------------------------
# 工具函数
# ----------------------------------------------------------------------------
file_exists() { test -e "$1"; }
file_contains() {
  local pattern="$1" file="$2"
  grep -qE "$pattern" "$file" 2>/dev/null
}

# ----------------------------------------------------------------------------
# ADR 验证函数
# ----------------------------------------------------------------------------

# === A. 框架架构 ===

verify_adr_001() {
  should_run ADR-001 || return
  local detail=""
  local ok=true
  [[ -d "$CPPTLM" ]] || { ok=false; detail+="missing: $CPPTLM; "; }
  [[ -d "$CPPHDL" ]] || { ok=false; detail+="missing: $CPPHDL; "; }
  [[ -d "$CHIPFORGE_ROOT/ip" ]] || { ok=false; detail+="missing: ip/; "; }
  if $ok; then
    log_pass "ADR-001" "三层框架分工"
  else
    log_failed "ADR-001" "三层框架分工" "$detail"
  fi
}

verify_adr_002() {
  should_run ADR-002 || return
  local f="$INCLUDE_CPPTLM/core/event_queue.hh"
  if file_exists "$f" && file_contains "class EventQueue" "$f" && \
     file_contains "createModule|run\(" "$f"; then
    log_pass "ADR-002" "CppTLM 事件驱动调度"
  else
    log_failed "ADR-002" "CppTLM 事件驱动调度" "missing: $f"
  fi
}

verify_adr_003() {
  should_run ADR-003 || return
  local ok=true
  local detail=""
  for f in core/lnode.h codegen_verilog.h simulator.h; do
    [[ -e "$INCLUDE_CPPHDL/$f" ]] || { ok=false; detail+="missing: $f; "; }
  done
  if $ok; then
    log_pass "ADR-003" "CppHDL LogicNode DAG + 多后端"
  else
    log_failed "ADR-003" "CppHDL LogicNode DAG + 多后端" "$detail"
  fi
}

# === B. CppTLM 模块/接口 ===

verify_adr_004() {
  should_run ADR-004 || return
  local ok=true
  local detail=""
  # SimObject
  if ! file_contains "class SimObject" "$INCLUDE_CPPTLM/core/sim_object.hh"; then
    ok=false; detail+="SimObject missing; "
  fi
  # SimModule : public SimObject
  if ! file_contains "class SimModule.*: public SimObject" "$INCLUDE_CPPTLM/core/sim_module.hh"; then
    ok=false; detail+="SimModule inheritance wrong; "
  fi
  # TLMModule (全大写) : public SimObject（注意：不通过 SimModule）
  if ! file_contains "class TLMModule.*: public SimObject" "$INCLUDE_CPPTLM/core/tlm_module.hh"; then
    ok=false; detail+="TLMModule missing or wrong inheritance; "
  fi
  # ChStreamModuleBase : public SimObject
  if ! file_contains "class ChStreamModuleBase.*: public SimObject" "$INCLUDE_CPPTLM/core/chstream_module.hh"; then
    ok=false; detail+="ChStreamModuleBase inheritance wrong; "
  fi
  if $ok; then
    log_pass "ADR-004" "SimObject 类层次（TLMModule/CHStreamModuleBase 直接继承 SimObject）"
  else
    log_failed "ADR-004" "SimObject 类层次" "$detail"
  fi
}

verify_adr_005() {
  should_run ADR-005 || return
  local ok=true
  local detail=""
  # 基类已迁出 framework/，应在 core/
  if ! [[ -e "$INCLUDE_CPPTLM/core/stream_adapter_base.hh" ]]; then
    ok=false; detail+="missing: core/stream_adapter_base.hh; "
  elif ! file_contains "class StreamAdapterBase" "$INCLUDE_CPPTLM/core/stream_adapter_base.hh"; then
    ok=false; detail+="StreamAdapterBase class not found; "
  fi
  # 检测陈旧路径
  if [[ -e "$INCLUDE_CPPTLM/framework/stream_adapter_base.hh" ]]; then
    log_stale "ADR-005" "StreamAdapter 类型擦除" \
      "陈旧路径 framework/stream_adapter_base.hh 应已删除（类已迁至 core/）"
    return
  fi
  # 具体适配器（应在 framework/）
  for cls in InputStreamAdapter OutputStreamAdapter; do
    if ! file_contains "class ${cls}\b" "$INCLUDE_CPPTLM/framework/stream_adapter.hh"; then
      ok=false; detail+="${cls} missing; "
    fi
  done
  for adapter in multi_port_stream_adapter dual_port_stream_adapter bidirectional_port_adapter; do
    [[ -e "$INCLUDE_CPPTLM/framework/${adapter}.hh" ]] || { ok=false; detail+="missing: ${adapter}.hh; "; }
  done
  if $ok; then
    log_pass "ADR-005" "StreamAdapter 类型擦除 + 协议转换"
  else
    log_failed "ADR-005" "StreamAdapter 类型擦除 + 协议转换" "$detail"
  fi
}

verify_adr_006() {
  should_run ADR-006 || return
  local f="$INCLUDE_CPPTLM/framework/chstream_adapter_factory.hh"
  if ! [[ -e "$f" ]]; then
    log_failed "ADR-006" "ChStreamAdapterFactory" "missing: $f"
    return
  fi
  if ! file_contains "class ChStreamAdapterFactory" "$f"; then
    log_failed "ADR-006" "ChStreamAdapterFactory" "class not found"
    return
  fi
  local missing=""
  for method in registerFactory registerAdapter registerMultiPortAdapter registerDualPortAdapter registerBidirectionalPortAdapter create knows; do
    file_contains "(\\b${method}\\b|::${method}\\b)" "$f" || missing+="${method} "
  done
  if [[ -z "$missing" ]]; then
    log_pass "ADR-006" "ChStreamAdapterFactory"
  else
    log_failed "ADR-006" "ChStreamAdapterFactory" "missing methods: $missing"
  fi
}

verify_adr_007() {
  should_run ADR-007 || return
  # 预期：通用 TLM↔RTL 桥接未实现（仅 HybridCacheWrapper）
  if [[ ! -e "$INCLUDE_CPPTLM/framework/tlm_rtl_bridge.hh" ]] && \
     [[ -e "$INCLUDE_CPPTLM/rtl/hybrid_cache_wrapper.hh" ]]; then
    log_expected_missing "ADR-007" "StreamAdapter 跨 TLM↔RTL 通用桥接" \
      "仅 HybridCacheWrapper 局部实现"
  elif [[ -e "$INCLUDE_CPPTLM/framework/tlm_rtl_bridge.hh" ]]; then
    log_stale "ADR-007" "StreamAdapter 跨 TLM↔RTL 通用桥接" \
      "通用桥接已实现但 ADR 仍标 🚧 — 需更新 ADR 状态"
  else
    log_expected_missing "ADR-007" "StreamAdapter 跨 TLM↔RTL 通用桥接" "未实现"
  fi
}

verify_adr_008() {
  should_run ADR-008 || return
  local f1="$INCLUDE_CPPTLM/rtl/hybrid_cache_wrapper.hh"
  local f2="$INCLUDE_CPPTLM/chstream_register.hh"
  if [[ -e "$f1" ]] && file_contains "HybridCacheWrapper" "$f2"; then
    log_pass "ADR-008" "HybridCacheWrapper TLM↔RTL 协同仿真"
  else
    log_failed "ADR-008" "HybridCacheWrapper TLM↔RTL 协同仿真" \
      "missing: $f1 或未在 chstream_register.hh 注册"
  fi
}

# === C. CppHDL 模块/接口 ===

verify_adr_009() {
  should_run ADR-009 || return
  local f="$INCLUDE_CPPHDL/core/uint.h"
  if [[ -e "$f" ]] && file_contains "class ch_uint|using ch_uint|template.*ch_uint" "$f"; then
    log_pass "ADR-009" "ch_uint<N> 硬件整数"
  else
    log_failed "ADR-009" "ch_uint<N> 硬件整数" "missing or malformed: $f"
  fi
}

verify_adr_010() {
  should_run ADR-010 || return
  local f="$INCLUDE_CPPHDL/core/reg.h"
  if [[ -e "$f" ]] && file_contains "class ch_reg" "$f"; then
    log_pass "ADR-010" "ch_reg<T> 时序寄存器"
  else
    log_failed "ADR-010" "ch_reg<T> 时序寄存器" "missing: $f"
  fi
}

verify_adr_011() {
  should_run ADR-011 || return
  local f="$INCLUDE_CPPHDL/core/mem.h"
  if [[ -e "$f" ]] && file_contains "class ch_mem" "$f"; then
    log_pass "ADR-011" "ch_mem<T,D> SRAM"
  else
    log_failed "ADR-011" "ch_mem<T,D> SRAM" "missing: $f"
  fi
}

verify_adr_012() {
  should_run ADR-012 || return
  local f="$INCLUDE_CPPHDL/component.h"
  if [[ -e "$f" ]] && file_contains "virtual void describe\(\) = 0" "$f"; then
    log_pass "ADR-012" "Component::describe() 纯虚入口"
  else
    log_failed "ADR-012" "Component::describe() 纯虚入口" "pure virtual describe() not found"
  fi
}

verify_adr_013() {
  should_run ADR-013 || return
  local f1="$INCLUDE_CPPHDL/component.h"
  local f2="$INCLUDE_CPPHDL/core/io.h"
  if [[ -e "$f1" ]] && file_contains "virtual void create_ports" "$f1" && \
     [[ -e "$f2" ]] && file_contains "^#define __io\(" "$f2"; then
    log_pass "ADR-013" "Component::create_ports() + __io 宏"
  else
    log_failed "ADR-013" "Component::create_ports() + __io 宏" \
      "create_ports 或 __io 宏缺失"
  fi
}

verify_adr_014() {
  should_run ADR-014 || return
  local ok=true
  local detail=""
  for f in lnode.h lnodeimpl.h node_builder.h; do
    [[ -e "$INCLUDE_CPPHDL/core/$f" ]] || { ok=false; detail+="missing: core/$f; "; }
  done
  # 模板实现在顶层 lnode/
  if [[ ! -d "$INCLUDE_CPPHDL/lnode" ]]; then
    ok=false; detail+="missing: lnode/ dir; "
  else
    local tpp_count=$(ls "$INCLUDE_CPPHDL/lnode/"*.tpp 2>/dev/null | wc -l)
    if [[ "$tpp_count" -lt 6 ]]; then
      ok=false; detail+="lnode/*.tpp count=$tpp_count (expect ≥6); "
    fi
  fi
  if $ok; then
    log_pass "ADR-014" "lnode 节点构建（位于 core/）"
  else
    log_failed "ADR-014" "lnode 节点构建（位于 core/）" "$detail"
  fi
}

# === D. 注册与发现 ===

verify_adr_015() {
  should_run ADR-015 || return
  local f="$INCLUDE_CPPTLM/core/module_factory.hh"
  if [[ -e "$f" ]] && file_contains "class ModuleFactory" "$f" && \
     file_contains "(create|registerObject)" "$f"; then
    log_pass "ADR-015" "ModuleFactory JSON 拓扑装配"
  else
    log_failed "ADR-015" "ModuleFactory JSON 拓扑装配" "missing: $f"
  fi
}

verify_adr_016() {
  should_run ADR-016 || return
  local f="$INCLUDE_CPPTLM/chstream_register.hh"
  if ! [[ -e "$f" ]]; then
    log_failed "ADR-016" "REGISTER_CHSTREAM 批量注册宏" "missing: $f"
    return
  fi
  if ! file_contains "^#define REGISTER_CHSTREAM" "$f"; then
    log_failed "ADR-016" "REGISTER_CHSTREAM 批量注册宏" "macro not defined"
    return
  fi
  local missing=""
  for mod in CacheTLM MemoryTLM CrossbarTLM CPUTLM TrafficGenTLM; do
    file_contains "registerObject<${mod}>" "$f" || missing+="${mod} "
  done
  file_contains "^#define REGISTER_ALL" "$f" || missing+="REGISTER_ALL "
  if [[ -z "$missing" ]]; then
    log_pass "ADR-016" "REGISTER_CHSTREAM 批量注册宏"
  else
    log_failed "ADR-016" "REGISTER_CHSTREAM 批量注册宏" "missing registrations: $missing"
  fi
}

verify_adr_017() {
  should_run ADR-017 || return
  local f="$INCLUDE_CPPTLM/core/plugin_loader.hh"
  if [[ -e "$f" ]] && file_contains "class PluginLoader" "$f"; then
    log_pass "ADR-017" "PluginLoader dlopen 运行时 SO 加载"
  else
    log_failed "ADR-017" "PluginLoader dlopen 运行时 SO 加载" "missing: $f"
  fi
}

# === E. 端口与信号 ===

verify_adr_018() {
  should_run ADR-018 || return
  local f="$INCLUDE_CPPHDL/core/io.h"
  if [[ -e "$f" ]] && file_contains "^#define __io\(" "$f" && \
     file_contains "io_type\s*&\s*io\(\)" "$f"; then
    log_pass "ADR-018" "__io 宏定义 io_type struct + io() 访问器"
  else
    log_failed "ADR-018" "__io 宏" "__io 宏或 io() 访问器缺失"
  fi
}

verify_adr_019() {
  should_run ADR-019 || return
  local f="$INCLUDE_CPPHDL/core/io.h"
  if [[ -e "$f" ]] && file_contains "^#define __in\(" "$f" && \
     file_contains "^#define __out\(" "$f" && \
     file_contains "using ch_in.*=.*port<T, input_direction>" "$f" && \
     file_contains "using ch_out.*=.*port<T, output_direction>" "$f"; then
    log_pass "ADR-019" "__in / __out 端口声明宏"
  else
    log_failed "ADR-019" "__in / __out 端口声明宏" "宏或类型别名缺失"
  fi
}

verify_adr_020() {
  should_run ADR-020 || return
  local f="$INCLUDE_CPPHDL/core/io.h"
  if [[ -e "$f" ]] && file_contains "ch_logic_in" "$f" && \
     file_contains "ch_logic_out" "$f"; then
    log_pass "ADR-020" "ch_logic_in/out 旧 API 保留"
  else
    log_failed "ADR-020" "ch_logic_in/out 旧 API 保留" "missing in $f"
  fi
}

# === F. Bundle 与协议 ===

verify_adr_021() {
  should_run ADR-021 || return
  local f="$INCLUDE_CPPHDL/core/bundle/bundle_base.h"
  if [[ -e "$f" ]] && file_contains "template.*typename Derived.*class bundle_base" "$f" && \
     file_contains "bundle_base.*: public logic_buffer" "$f"; then
    log_pass "ADR-021" "bundle_base<Self> CRTP 模式"
  else
    log_failed "ADR-021" "bundle_base<Self> CRTP 模式" "malformed: $f"
  fi
}

verify_adr_022() {
  should_run ADR-022 || return
  local f="$INCLUDE_CPPHDL/core/bundle/bundle_meta.h"
  local missing=""
  for i in 1 2 3 4 5; do
    file_contains "^#define CH_BUNDLE_FIELDS_T_${i}\b" "$f" || missing+="_${i} "
  done
  file_contains "^#define CH_BUNDLE_FIELDS_T\(" "$f" || missing+="variadic "
  if [[ -z "$missing" ]]; then
    log_pass "ADR-022" "CH_BUNDLE_FIELDS_T 宏族"
  else
    log_failed "ADR-022" "CH_BUNDLE_FIELDS_T 宏族" "missing arities: $missing"
  fi
}

verify_adr_023() {
  should_run ADR-023 || return
  local f="$INCLUDE_CPPHDL/bundle/stream_bundle.h"
  local f2="$INCLUDE_CPPHDL/bundle/flow_bundle.h"
  if [[ -e "$f" ]] && file_contains "struct ch_stream.*: public bundle_base<ch_stream" "$f" && \
     file_contains "using Stream\s*=\s*ch_stream" "$f" && \
     [[ -e "$f2" ]] && file_contains "struct ch_stream" "$f2"; then
    log_pass "ADR-023" "ch_stream<T> 协议层"
  else
    log_failed "ADR-023" "ch_stream<T> 协议层" "missing in $f or $f2"
  fi
}

verify_adr_024() {
  should_run ADR-024 || return
  local core_ok=true
  [[ -e "$INCLUDE_CPPHDL/core/bundle/bundle_base.h" ]] || core_ok=false
  [[ -e "$INCLUDE_CPPHDL/bundle/stream_bundle.h" ]] || core_ok=false
  if ! $core_ok; then
    log_failed "ADR-024" "Bundle 三层分层" "core bundle/stream missing"
    return
  fi
  if [[ -e "$INCLUDE_CPPHDL/core/bundle/bundle_mapper.h" ]]; then
    log_stale "ADR-024" "Bundle 三层分层" \
      "BundleMapper 已实现但 ADR 仍标 ⚠️ — 应升级到 ✅"
  else
    log_pass "ADR-024" "Bundle 三层分层（核心已实现，Mapper 缺失符合 ⚠️ 状态）"
  fi
  if [[ -e "$CHIPFORGE_ROOT/bundles/bundle_mapper.h" ]]; then
    log_failed "ADR-024" "Bundle 三层分层 drift 防护" \
      "bundles/bundle_mapper.h 已实现但 ADR 仍标 ⚠️ — canonical 设计推迟到 Phase5 (参见 bundles/README.md:102, plugin-framework.md:129, DECISION-2026-06-10-02 D1'')"
  else
    log_pass "ADR-024" "Bundle 三层分层 drift 防护 (bundles/bundle_mapper.h 未实现, 符合 Phase5 推迟约定)"
  fi
}

# === G. 声明式 Plugin 模型 (Phase 1) ===

verify_adr_025() {
  should_run ADR-025 || return
  # Plugin 基类应不存在
  local hits=$(grep -rlE "^class Plugin\s" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" --include="*.cpp" --include="*.cc" 2>/dev/null \
    | grep -v "PluginLoader" | head -3)
  if [[ -z "$hits" ]]; then
    log_expected_missing "ADR-025" "Plugin 基类无 tick" "class Plugin 不存在"
  else
    log_stale "ADR-025" "Plugin 基类无 tick" \
      "发现 class Plugin 定义: $hits — ADR 应升级或说明"
  fi
}

verify_adr_026() {
  should_run ADR-026 || return
  if ! grep -rqE "at_stage\s*\(" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" --include="*.cpp" --include="*.cc" 2>/dev/null | head -1; then
    log_expected_missing "ADR-026" "at_stage() 逻辑阶段名绑定" "方法不存在"
  else
    log_stale "ADR-026" "at_stage()" "方法已存在但 ADR 仍 🚧"
  fi
}

verify_adr_027() {
  should_run ADR-027 || return
  # 限定 cf::plugin 命名空间 (include/cf/plugin/)，不匹配其他命名空间
  if ! grep -rqE "enum class Phase\s*\{[^}]*EARLY" \
    "$CHIPFORGE_ROOT/include/cf/plugin" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-027" "Phase 子阶段 EARLY/NORMAL/LATE" "枚举不存在"
  else
    log_stale "ADR-027" "Phase 枚举" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_028() {
  should_run ADR-028 || return
  if ! grep -rqE "declare_substage\s*\(" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-028" "declare_substage() 动态扩展子流水线" "方法不存在"
  else
    log_stale "ADR-028" "declare_substage()" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_029() {
  should_run ADR-029 || return
  local hits=""
  hits+=$(grep -rE "enum class ImplMode" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1)
  hits+="|"
  hits+=$(grep -E "impl_mode_override" \
    "$INCLUDE_CPPTLM/core/module_factory.hh" 2>/dev/null | head -1)
  if [[ -z "$hits" ]] || [[ "$hits" == "|" ]]; then
    log_expected_missing "ADR-029" "模块级 ImplMode" "枚举与 JSON 字段均未实现"
  else
    log_stale "ADR-029" "模块级 ImplMode" "部分已实现: $hits"
  fi
}

# === H. 流水线抽象 (Phase 1) ===

verify_adr_030() {
  should_run ADR-030 || return
  if ! grep -rqE "class PipeNode" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-030" "PipeNode 三态握手" "class PipeNode 不存在"
  else
    log_stale "ADR-030" "PipeNode" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_031() {
  should_run ADR-031 || return
  # 拆分为三个独立子报告：每个 Link 类型单独判定
  if grep -rqE "class CtrlLink\b" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null; then
    log_pass "ADR-031" "CtrlLink"
  else
    log_expected_missing "ADR-031" "CtrlLink" "class CtrlLink 不存在"
  fi
  if grep -rqE "class StageLink\b" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null; then
    log_pass "ADR-031" "StageLink"
  else
    log_expected_missing "ADR-031" "StageLink" "class StageLink 不存在"
  fi
  if grep -rqE "class DirectLink\b" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null; then
    log_pass "ADR-031" "DirectLink"
  else
    log_expected_missing "ADR-031" "DirectLink" "class DirectLink 不存在"
  fi
}

verify_adr_032() {
  should_run ADR-032 || return
  if ! grep -rqE "class PipeBuilder" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-032" "PipeBuilder 统一编译器" "class PipeBuilder 不存在"
  else
    log_stale "ADR-032" "PipeBuilder" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_033() {
  should_run ADR-033 || return
  # 匹配调用形式 (link.halt_when) 与定义形式 (CtrlLink& halt_when)
  if ! grep -rqE "((\.|->|::)\s*|CtrlLink&\s+)(halt_when|flush_when|throw_when|bypass)\s*\(" \
    "$INCLUDE_CPPTLM" "$CHIPFORGE_ROOT/include/cf/plugin" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-033" "CtrlLink 四种控制 API" \
      "CtrlLink 方法不存在（注意：CppHDL chlib 已有 stream_halt_when / stream_throw_when 自由函数，但非 CtrlLink 对象方法）"
  else
    log_stale "ADR-033" "CtrlLink 控制 API" "方法已实现但 ADR 仍 🚧"
  fi
}

# === I. 验证框架 (Phase 1) ===

verify_adr_034() {
  should_run ADR-034 || return
  if ! grep -rqE "class (TransactionScoreBoard|CycleScoreBoard|TimingScoreBoard)" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-034" "ScoreBoard 三种变体" "类不存在"
  else
    log_stale "ADR-034" "ScoreBoard" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_035() {
  should_run ADR-035 || return
  if ! grep -rqE "class CompareDriver" \
    "$INCLUDE_CPPTLM" "$INCLUDE_CPPHDL" "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1; then
    log_expected_missing "ADR-035" "CompareDriver TLM↔RTL 协同驱动" "类不存在"
  else
    log_stale "ADR-035" "CompareDriver" "已实现但 ADR 仍 🚧"
  fi
}

verify_adr_036() {
  should_run ADR-036 || return
  # 限定源代码文件，避免匹配 verify_adr.sh 自身
  if ! grep -rqE "Level [ABC].*Test|level_a_test|level_b_test" \
    "$CHIPFORGE_ROOT" \
    --include="*.h" --include="*.hh" --include="*.hpp" \
    --include="*.cpp" --include="*.cc" --include="*.py" 2>/dev/null | head -1; then
    log_expected_missing "ADR-036" "三级测试金字塔" "Level A/B/C 框架不存在"
  else
    log_stale "ADR-036" "三级测试金字塔" "已实现但 ADR 仍 🚧"
  fi
}

# === J. 目录与组织 ===

verify_adr_037() {
  should_run ADR-037 || return
  # 预期失败：当前仍是 tlm/rtl 分离
  if [[ -d "$CHIPFORGE_ROOT/ip/cache/tlm" ]] && [[ -d "$CHIPFORGE_ROOT/ip/cache/rtl" ]]; then
    log_expected_missing "ADR-037" "统一目录结构" "ip/cache/{tlm,rtl} 仍分离"
  else
    log_stale "ADR-037" "统一目录结构" "目录已合并但 ADR 仍 🚧"
  fi
}

verify_adr_038() {
  should_run ADR-038 || return
  local f="$INCLUDE_CPPTLM/chstream_register.hh"
  if [[ -e "$f" ]] && file_contains "REGISTER_CHSTREAM" "$f"; then
    log_pass "ADR-038" "chstream_register.hh 集中注册入口"
  else
    log_failed "ADR-038" "chstream_register.hh 集中注册入口" "missing or malformed: $f"
  fi
}

# ----------------------------------------------------------------------------
# 主循环
# ----------------------------------------------------------------------------
main() {
  echo "================================================================"
  echo -e "${C_BLUE}Architecture Decision Register Verification${C_RESET}"
  echo "================================================================"
  echo "Root: $CHIPFORGE_ROOT"
  echo "ADR doc: docs/architecture/adr.md"
  echo ""
  if [[ -n "$CATEGORY_FILTER" ]]; then
    echo -e "Category filter: ${C_YELLOW}${CATEGORY_FILTER} - ${CATEGORY_NAMES[$CATEGORY_FILTER]:-未知}${C_RESET}"
  fi
  if [[ -n "$ONLY_FILTER" ]]; then
    echo -e "ADR filter: ${C_YELLOW}${ONLY_FILTER}${C_RESET}"
  fi
  if [[ "$VERBOSE" -eq 1 ]]; then
    echo -e "${C_GRAY}Verbose mode enabled${C_RESET}"
  fi
  echo ""

  # 按类别执行
  for cat in A B C D E F G H I J; do
    if [[ -n "$CATEGORY_FILTER" && "$CATEGORY_FILTER" != "$cat" ]]; then
      continue
    fi
    if [[ -z "$ONLY_FILTER" ]]; then
      echo -e "${C_BLUE}── ${cat}. ${CATEGORY_NAMES[$cat]} ──${C_RESET}"
    fi
  done

  # 实际执行（按 ID 顺序）
  verify_adr_001
  verify_adr_002
  verify_adr_003
  verify_adr_004
  verify_adr_005
  verify_adr_006
  verify_adr_007
  verify_adr_008
  verify_adr_009
  verify_adr_010
  verify_adr_011
  verify_adr_012
  verify_adr_013
  verify_adr_014
  verify_adr_015
  verify_adr_016
  verify_adr_017
  verify_adr_018
  verify_adr_019
  verify_adr_020
  verify_adr_021
  verify_adr_022
  verify_adr_023
  verify_adr_024
  verify_adr_025
  verify_adr_026
  verify_adr_027
  verify_adr_028
  verify_adr_029
  verify_adr_030
  verify_adr_031
  verify_adr_032
  verify_adr_033
  verify_adr_034
  verify_adr_035
  verify_adr_036
  verify_adr_037
  verify_adr_038

  # 汇总
  echo ""
  echo "================================================================"
  echo "Summary"
  echo "================================================================"
  echo -e "${C_GREEN}✅ PASS              : $PASS${C_RESET}"
  echo -e "${C_BLUE}🚧 EXPECTED_MISSING : $EXPECTED_MISSING${C_RESET}  (Phase 1 提案，符合预期)"
  echo -e "${C_YELLOW}⚠️  STALE             : $STALE${C_RESET}  (建议更新文档)"
  echo -e "${C_RED}❌ FAILED            : $FAILED${C_RESET}  (Critical 漂移)"
  echo ""

  # 详细报告（仅 FAILED 或 STALE）
  if [[ $FAILED -gt 0 || $STALE -gt 0 ]]; then
    echo "================================================================"
    echo "Issues Requiring Attention"
    echo "================================================================"
    for result in "${RESULTS[@]}"; do
      IFS='|' read -r status id title detail <<< "$result"
      if [[ "$status" == "FAILED" ]]; then
        echo -e "${C_RED}[$id] $title${C_RESET}"
        [[ -n "$detail" ]] && echo "    $detail"
      elif [[ "$status" == "STALE" ]]; then
        echo -e "${C_YELLOW}[$id] $title${C_RESET}"
        [[ -n "$detail" ]] && echo "    $detail"
      fi
    done
    echo ""
  fi

  # 退出码
  if [[ $FAILED -gt 0 ]]; then
    echo -e "${C_RED}❌ Architecture drift detected (Critical) — fix before merging${C_RESET}"
    exit 1
  fi
  echo -e "${C_GREEN}✓ All ✅ ADRs pass verification${C_RESET}"
  exit 0
}

main "$@"
