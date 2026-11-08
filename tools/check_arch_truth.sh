#!/usr/bin/env bash
# tools/check_arch_truth.sh
#
# 门禁① 顶层纯净 lint —— 实施态时点语汇不得进 `docs/architecture/` 顶层。
#
# 治理依据: docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md §2.5①
# change   : architecture-truth-boundary-enforcement (AC-110 / AC-119)
#
# ★ ★ 语义与判据全部在 tools/check_arch_truth.py（★ 词表冻结在那里 = 单一真源）。
#   本文件只是 shell 入口，**不重复实现**任何判定逻辑（否则出现第二套判据）。
#
# 用法:
#   bash tools/check_arch_truth.sh                 # 检查顶层 9 份（0 = 通过）
#   bash tools/check_arch_truth.sh --show-exempt    # 附豁免明细（证据用）
#   bash tools/check_arch_truth.sh --self-test      # fixture 自检（4 类必备 + 4 类附加 + 真阳性）
#
# 退出码: 0 = 0 命中 · 1 = 有命中（报红）· 2 = usage / IO 错误

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

exec python3 "${SCRIPT_DIR}/check_arch_truth.py" --root "${PROJECT_ROOT}" "$@"
