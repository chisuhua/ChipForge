# tests/cpu/manual_elf/build_mmu_bare.S
#
# MMU Bare mode boundary probe ELF (Phase 6d P1 plumbing-only stub)
#
# Purpose:
#   Trigger v0.10.4-type `satp_value_` MODE field boundary conditions.
#   Even though mmu_chmem.h is NOT implemented in this change (plumbing-only
#   no-op stub), this ELF documents the eventual ABI when MMU CH_MEM is
#   implemented by verilator-mmu-bare-plumbing-e2e + cpu-pipeline-mmufault-handler.
#
# Behavior in this change:
#   - Loaded as raw ELF (no MMU interpretation in CH_MEM build_cpu)
#   - cpu_verilator_sim --enable-mmu emits plumbing-only log + still treats
#     all addresses as identity (Bare translation = no translation)
#   - To verify CLI plumbing flag is correctly forwarded: see tasks.md 5.6
#
# Program:
#   1. li x1, 0xCAFE0000   # sentinel value (write pattern)
#   2. sw x1, 0(x0)         # write to address 0 (would be MMU-translated normally)
#   3. li x2, 0xDEAD0000   # different sentinel
#   4. sw x2, 4(x0)         # write to address 4
#   5. li x3, 1             # PASS marker
#   6. sw x3, 8(x0)         # write to tohost+8 (Picolibc convention)
#   7. jal x0, .            # spin forever

.section .text
.global _start

_start:
    li   x1, 0xCAFE0000
    sw   x1, 0(x0)
    li   x2, 0xDEAD0000
    sw   x2, 4(x0)
    li   x3, 1
    sw   x3, 8(x0)
1:  jal  x0, 1b