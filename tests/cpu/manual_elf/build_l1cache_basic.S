# tests/cpu/manual_elf/build_l1cache_basic.S
#
# L1Cache basic hit + miss probe ELF (Phase 6d P2 placeholder)
#
# Purpose:
#   Exercise one cache hit + one cache miss path. Even though L1Cache CH_MEM
#   is NOT implemented in this change (fail-fast throw on --enable-cache),
#   this ELF documents the eventual ABI when L1Cache CH_MEM is implemented
#   by verilator-l1cache-e2e-coverage + cache-phase1.5-4way (wave4 P2).
#
# Behavior in this change:
#   - Loaded as raw ELF (no cache policy applied; all accesses go to memory)
#   - cpu_verilator_sim --enable-cache throws std::runtime_error with substring
#     "L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage"
#   - To verify CLI plumbing flag reach build_cpu: see tasks.md 5.5
#
# Program:
#   1. li x1, 0x12345678
#   2. sw x1, 0(x0)         # cold write → cache miss (eventually)
#   3. lw x2, 0(x0)         # hot read  → cache hit (eventually)
#   4. li x3, 1             # PASS marker
#   5. sw x3, 8(x0)         # write to tohost+8
#   6. jal x0, .            # spin forever

.section .text
.global _start

_start:
    li   x1, 0x12345678
    sw   x1, 0(x0)
    lw   x2, 0(x0)
    li   x3, 1
    sw   x3, 8(x0)
1:  jal  x0, 1b