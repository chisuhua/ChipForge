// rvmodel_macros.h — ChipForge chipforge-tlm-rv64
// Adapted from CVA6 cv32a65x minimal template (no PLIC/CLINT/PMP).
// Decision: .omo/drafts/decision-phase-2-baremetal-v1.1-toolchain-guide-2026-06-15.md §三 3.1
// SPDX-License-Identifier: Apache-2.0

#ifndef _RVMODEL_MACROS_H
#define _RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION \
        .pushsection .tohost,"aw",@progbits;                \
        .align 8; .global tohost; tohost: .dword 0;         \
        .align 8; .global fromhost; fromhost: .dword 0;     \
        .popsection;

/* ChipForge has no custom boot (no memory controller). */
#define RVMODEL_BOOT
#define RVMODEL_BOOT_TO_MMODE

/* HALT_PASS writes 1 to tohost; HALT_FAIL writes 3. */
#define RVMODEL_HALT_PASS  \
  li x1, 1                ;\
  la t0, tohost           ;\
  write_tohost_pass:      ;\
    sw x1, 0(t0)          ;\
    sw x0, 4(t0)          ;\
  self_loop_pass:         ;\
    j self_loop_pass      ;\

#define RVMODEL_HALT_FAIL  \
  li x1, 3                ;\
  la t0, tohost           ;\
  write_tohost_fail:      ;\
    sw x1, 0(t0)          ;\
    sw x0, 4(t0)          ;\
  self_loop_fail:         ;\
    j self_loop_fail      ;\

/* ChipForge Phase 2 has no UART. RVMODEL_IO_WRITE_STR drops chars silently. */
#define RVMODEL_IO_INIT
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR) \
  1: lbu _R1, 0(_STR_PTR);                            \
     beqz _R1, 3f;                                     \
     addi _STR_PTR, _STR_PTR, 1;                      \
     j 1b;                                            \
  3:

/* Phase 2 has no PLIC / CLINT / mtime. priv tests excluded via UDB config. */
#define RVMODEL_MTIMECMP_ADDRESS 0
#define RVMODEL_MTIME_ADDRESS 0
#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)
#define RVMODEL_SET_MSW_INT(_R1, _R2)
#define RVMODEL_CLR_MSW_INT(_R1, _R2)
#define RVMODEL_INTERRUPT_LATENCY 0

/* No PMP / trap signature region (NUM_PMP_ENTRIES=0 in UDB config). */
#define RVMODEL_TRAP_SIG_ADDRESS 0
#define RVMODEL_TRAP_SIG_SIZE 0

#endif
