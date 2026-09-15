// See LICENSE for license details.
// riscv-test-env env/p/riscv_test.h — ChipForge minimal p-env variant
//
// ChipForge rv32ui-p harness (riscv-tests-rv32ui Wave 1, 2026-09-15):
//   Upstream riscv-test-env v2 (submodule 6de71edb) drives RVTEST_PASS/FAIL
//   via `ecall` (a7=93 SYS_exit) → trap handler → `sw TESTNUM, tohost`.
//   That requires a full trap machine (mtvec/mcause/mret) which is Wave 4
//   scope. Wave 1 design decision (design.md §Decision 5/6) is direct
//   `sw TESTNUM, tohost` — no ecall/trap needed.
//
//   This file keeps the test BODIES 100% upstream (.S files unchanged) and
//   only replaces the harness macros:
//     - RVTEST_PASS : write 1  to tohost  (PicolibcHostMemory exit_code 0)
//     - RVTEST_FAIL : write TESTNUM|1 to tohost (exit_code 1)
//     - RVTEST_CODE_BEGIN: minimal reset (INIT_XREG + jump into body),
//       no trap_vector / CSR setup / mret (single-hart, no trap needed)
//   link.ld unchanged from upstream (`.tohost` at 0x80001000).
//
// SPDX-License-Identifier: BSD-3-Clause (matches riscv-test-env upstream)

#ifndef _ENV_PHYSICAL_SINGLE_CORE_H
#define _ENV_PHYSICAL_SINGLE_CORE_H

//-----------------------------------------------------------------------
// Begin Macro — RVTEST_RV*U only needs `init` to be a no-op here
//-----------------------------------------------------------------------

#define RVTEST_RV32U                                                    \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32UF                                                   \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32UV                                                   \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32UVX                                                  \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32M                                                    \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32S                                                    \
  .macro init;                                                          \
  .endm

#define RVTEST_RV64U  RVTEST_RV32U
#define RVTEST_RV64UF RVTEST_RV32U
#define RVTEST_RV64UV RVTEST_RV32U
#define RVTEST_RV64UVX RVTEST_RV32U
#define RVTEST_RV64M  RVTEST_RV32U
#define RVTEST_RV64S  RVTEST_RV32U

//-----------------------------------------------------------------------
// Register init (single-hart, no CSR state needed)
//-----------------------------------------------------------------------

#define INIT_XREG                                                       \
  li x1, 0; li x2, 0; li x3, 0; li x4, 0; li x5, 0; li x6, 0;           \
  li x7, 0; li x8, 0; li x9, 0; li x10, 0; li x11, 0; li x12, 0;        \
  li x13, 0; li x14, 0; li x15, 0; li x16, 0; li x17, 0; li x18, 0;     \
  li x19, 0; li x20, 0; li x21, 0; li x22, 0; li x23, 0; li x24, 0;     \
  li x25, 0; li x26, 0; li x27, 0; li x28, 0; li x29, 0; li x30, 0;     \
  li x31, 0;

#define RISCV_MULTICORE_DISABLE
#define INIT_RNMI
#define INIT_SATP
#define INIT_PMP
#define DELEGATE_NO_TRAPS
#define CHECK_XLEN
#define EXTRA_INIT
#define EXTRA_INIT_TIMER
#define FILTER_TRAP
#define FILTER_PAGE_FAULT
#define INTERRUPT_HANDLER

//-----------------------------------------------------------------------
// RVTEST_CODE_BEGIN — minimal reset: clear regs, then fall into test body.
//   No trap_vector / mtvec / mret (single hart, no trap machinery in Wave 1).
//-----------------------------------------------------------------------

#define RVTEST_CODE_BEGIN                                               \
        .section .text.init;                                            \
        .align  6;                                                      \
        .globl _start;                                                  \
_start:                                                                 \
        INIT_XREG;                                                      \
        li TESTNUM, 0;                                                  \
        /* fall through into test body */

//-----------------------------------------------------------------------
// End Macro
//-----------------------------------------------------------------------

#define RVTEST_CODE_END                                                 \
        unimp

//-----------------------------------------------------------------------
// Pass/Fail — direct tohost store (PicolibcHostMemory convention)
//   tohost == 1           → PASS (exit_code 0)
//   tohost == TESTNUM|1   → FAIL (exit_code 1, TESTNUM >= 2)
//-----------------------------------------------------------------------

#define TESTNUM gp

#define RVTEST_PASS                                                     \
        fence;                                                          \
        li TESTNUM, 1;                                                  \
        la t5, tohost;                                                  \
        sw TESTNUM, 0(t5);                                              \
1:      j 1b

#define RVTEST_FAIL                                                     \
        fence;                                                          \
        ori TESTNUM, TESTNUM, 1;                                        \
        la t5, tohost;                                                  \
        sw TESTNUM, 0(t5);                                              \
1:      j 1b

//-----------------------------------------------------------------------
// Data Section Macro — .tohost/.fromhost + signature region
//-----------------------------------------------------------------------

#define RVTEST_DATA_BEGIN                                               \
        .pushsection .tohost,"aw",@progbits;                            \
        .align 6; .global tohost; tohost: .dword 0; .size tohost, 8;    \
        .align 6; .global fromhost; fromhost: .dword 0; .size fromhost, 8;\
        .popsection;                                                    \
        .align 4; .global begin_signature; begin_signature:

#define RVTEST_DATA_END .align 4; .global end_signature; end_signature:

#endif  // _ENV_PHYSICAL_SINGLE_CORE_H
