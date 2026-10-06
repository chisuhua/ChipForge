#!/bin/bash
# tests/cpu/riscv_tests/build_rv32um.sh
#
# riscv-tests-rv32um v0.2.1: build 8 rv32um-p-* ELF binaries for mfc-cpu-pipeline-multi-cycle-fsm
# Phase E (PoC-1 硬指标: rv32um 8/8 PASS).
#
# Prerequisites:
#   - riscv32-unknown-elf-gcc on PATH (or /workspace/project/opt/riscv/bin/)
#   - riscv-tests source at $RISCV_TESTS_SRC/isa/rv32um/*.S
#   - local p-env harness at $(dirname $0)/env-p/ (riscv_test.h + link.ld)
#     (ChipForge variant: RVTEST_PASS/FAIL write tohost directly via sw,
#      no ecall/trap — matches PicolibcHostMemory tohost convention)
#
# Output: 8 ELF binaries in tests/cpu/riscv_tests/elf/rv32um-p-*
#
# SPDX-License-Identifier: BSD-3-Clause (matches riscv-tests upstream)
set -euo pipefail

RV32_TC="${RV32_TC:-/workspace/project/opt/riscv/bin/riscv32-unknown-elf-gcc}"
SRC="${RISCV_TESTS_SRC:?RISCV_TESTS_SRC must point to riscv-tests repo root}"
DEST="$(dirname "$0")/elf"
ENV_P="$(dirname "$0")/env-p"

mkdir -p "$DEST"

# rv32um 全部 vendor (8 个: mul/mulh/mulhsu/mulhu/div/divu/rem/remu)
# 与 rv32ui 不同: -march=rv32im_zicsr (vs rv32i_zicsr) — 含 M 扩展
COUNT=0
for src in "$SRC"/isa/rv32um/*.S; do
    name=$(basename "$src" .S)
    out="$DEST/rv32um-p-$name"
    if "$RV32_TC" -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static \
        -Wl,-T,"$ENV_P/link.ld" \
        -I "$ENV_P" \
        -I "$SRC/isa/macros/scalar" \
        -o "$out" "$src" 2>/dev/null; then
        echo "build $name"
        COUNT=$((COUNT + 1))
    else
        echo "FAIL: $name"
    fi
done
echo "Total built: $COUNT ELFs in $DEST/"
