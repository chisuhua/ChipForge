#!/bin/bash
# tests/cpu/riscv_tests/build_rv32ui.sh
#
# riscv-tests-rv32ui v0.2.1: build 40 rv32ui-p-* ELF binaries for Wave 1 compliance baseline.
#
# Prerequisites:
#   - riscv32-unknown-elf-gcc on PATH (or /workspace/project/opt/riscv/bin/)
#   - riscv-tests source at $RISCV_TESTS_SRC/isa/rv32ui/*.S
#   - local p-env harness at $(dirname $0)/env-p/ (riscv_test.h + link.ld)
#     (ChipForge variant: RVTEST_PASS/FAIL write tohost directly via sw,
#      no ecall/trap — matches PicolibcHostMemory tohost convention)
#
# Output: 40 ELF binaries (excluding fence_i, ma_data) in tests/cpu/riscv_tests/elf/
#
# SPDX-License-Identifier: BSD-3-Clause (matches riscv-tests upstream)
set -euo pipefail

RV32_TC="${RV32_TC:-/workspace/project/opt/riscv/bin/riscv32-unknown-elf-gcc}"
SRC="${RISCV_TESTS_SRC:?RISCV_TESTS_SRC must point to riscv-tests repo root}"
DEST="$(dirname "$0")/elf"
ENV_P="$(dirname "$0")/env-p"

mkdir -p "$DEST"

EXCLUDE="fence_i ma_data"
COUNT=0
for src in "$SRC"/isa/rv32ui/*.S; do
    name=$(basename "$src" .S)
    if echo "$EXCLUDE" | grep -qw "$name"; then
        echo "skip $name"
        continue
    fi
    out="$DEST/rv32ui-p-$name"
    if "$RV32_TC" -march=rv32i_zicsr -mabi=ilp32 -nostdlib -static \
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
