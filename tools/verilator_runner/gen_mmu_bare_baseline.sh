#!/bin/bash
# tools/verilator_runner/gen_mmu_bare_baseline.sh
# Phase 6d verilator-mmu-bare-plumbing-e2e cycle baseline generator.
# Per tasks.md §2: 5 ELF × 5 runs × median → CSV at tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv
set -uo pipefail

CPU_VERILATOR_SIM="${CPU_VERILATOR_SIM:-./build/bin/cpu_verilator_sim}"
ELF_DIR="${ELF_DIR:-tests/cpu/riscv_tests/elf}"
ELFS=(rv32ui-p-add rv32ui-p-addi rv32ui-p-auipc rv32ui-p-beq rv32ui-p-jal)
MODE="bare_mmu"
RUNS=5

# Verilator version probe (allow override for CI portability).
if command -v verilator &>/dev/null; then
    VERILATOR_VERSION="$(verilator --version 2>/dev/null | head -1 | awk '{print $2}')"
else
    for candidate in /workspace/main/opt/verilator/bin/verilator /usr/local/bin/verilator; do
        if [ -x "$candidate" ]; then
            VERILATOR_VERSION="$("$candidate" --version 2>/dev/null | head -1 | awk '{print $2}')"
            break
        fi
    done
    VERILATOR_VERSION="${VERILATOR_VERSION:-unknown}"
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

for elf in "${ELFS[@]}"; do
    elf_path="${ELF_DIR}/${elf}"
    if [ ! -f "$elf_path" ]; then
        echo "ERR ${elf} not found at ${elf_path}" >&2
        exit 2
    fi
    : > "$TMP"
    for _ in $(seq 1 "$RUNS"); do
        line=$("$CPU_VERILATOR_SIM" --elf "$elf_path" --enable-mmu --mmu-mode bare --cycles 2000 2>/dev/null \
                | grep -E '^TOHOST=[01] CYCLES=[0-9]+ ELF=' || true)
        cyc=$(echo "$line" | sed -nE 's/.*CYCLES=([0-9]+).*/\1/p')
        if [ -n "$cyc" ]; then
            echo "$cyc" >> "$TMP"
        fi
    done
    sort -n "$TMP" -o "$TMP"
    n=$(wc -l < "$TMP")
    if [ "$n" -lt 3 ]; then
        echo "ERR ${elf}: only ${n} runs collected (need ≥3)" >&2
        exit 3
    fi
    median=$(awk -v n="$n" 'NR==int((n+1)/2) {print; exit}' "$TMP")
    echo "${elf},${MODE},${median},${VERILATOR_VERSION}"
done