#!/usr/bin/env bash
# tools/check_xlen_hygiene.sh — ADR-087 §D6 软门禁 (warn-only, rv64-isa-declaration-correction)
# 新增 XLEN 硬编码假设必须登记 ip/cpu/docs/rv64_readiness_gap.md (缺口表 §5)。
# allowlist = 缺口表 §5 登记的 9 处 kXlenBits 定义点（分布于 9 个文件）:
#   3.1 ip/cpu/plugins/reg_file_chmem.h
#   3.2 ip/cpu/plugins/decode_chmem.h
#   3.3 ip/cpu/arch/riscv/int_alu_chmem.h
#   3.4 ip/cpu/plugins/branch_chmem.h
#   3.5 ip/cpu/plugins/ibus_chmem.h        (缺口表 3.5 描述未点名 kXlenBits, 但文件内确有定义点)
#   3.6 ip/cpu/plugins/dmem_chmem.h        (同上)
#   3.7 ip/cpu/plugins/mmu_ptw_chmem.h
#   3.8 ip/cpu/cpu_factory_chmem.h
#   3.10 ip/mmu/plugins/MMUPlugin_chmem.h
# 出现新定义点 → WARN (exit 0); STRICT_XLEN_GATE=yes 时升级 BLOCK (exit 1)。
# XLEN_SCAN_ROOT: 扫描根目录（默认 ip），供变异自检把探针目录指进来。
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
SCAN_ROOT="${XLEN_SCAN_ROOT:-ip}"

ALLOW="ip/cpu/plugins/reg_file_chmem.h
ip/cpu/plugins/decode_chmem.h
ip/cpu/arch/riscv/int_alu_chmem.h
ip/cpu/plugins/branch_chmem.h
ip/cpu/plugins/ibus_chmem.h
ip/cpu/plugins/dmem_chmem.h
ip/cpu/plugins/mmu_ptw_chmem.h
ip/cpu/cpu_factory_chmem.h
ip/mmu/plugins/MMUPlugin_chmem.h"

WARN=0
while IFS= read -r hit; do
  [ -z "$hit" ] && continue
  f="${hit%%:*}"
  case "$f" in *.h) ;; *) continue ;; esac
  # allowlist 用「路径后缀」匹配（而非全等）：探针模式下扫描根是绝对路径，
  # 全等比较会把 9 个已登记文件全部误报为 warn。
  registered=0
  while IFS= read -r a; do
    [ -z "$a" ] && continue
    case "$f" in *"$a") registered=1; break ;; esac
  done <<< "$ALLOW"
  if [ "$registered" -eq 0 ]; then
    echo "WARN: 未登记的 kXlenBits 定义点（须入缺口表 §5）: $hit"
    WARN=$((WARN + 1))
  fi
done < <(grep -rnE "kXlenBits *=" --include='*.h' "$SCAN_ROOT" || true)

echo "check_xlen_hygiene: warn=${WARN} (ADR-087 §D6, allowlist 口径见缺口表 §5, scan=${SCAN_ROOT})"
if [ "$WARN" -gt 0 ] && [ "${STRICT_XLEN_GATE:-}" = "yes" ]; then
  echo "STRICT_XLEN_GATE=yes → BLOCK"
  exit 1
fi
exit 0
