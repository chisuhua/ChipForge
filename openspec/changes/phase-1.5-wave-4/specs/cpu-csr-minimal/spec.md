## ADDED Requirements

### Requirement: CSR registers mstatus/mtvec/mepc/mcause/mtval/sstatus SHALL be readable and writable

The CPU SHALL implement a minimal CSR register file with at least these 6 registers: `mstatus` (machine status), `mtvec` (trap vector base), `mepc` (exception PC), `mcause` (exception cause), `mtval` (trap value), and `sstatus` (supervisor status). Each register SHALL support read via CSR read instructions (CSRRW/I/S/C) and write via the same instructions, with proper privilege level checks (m-mode CSR access from any mode, s-mode CSR access from s-mode or higher).

#### Scenario: Reading mstatus returns the current value
- **WHEN** mstatus holds value `0x00001800` (MPP=M-mode, MPIE=1)
- **AND** the CPU executes `csrr t0, mstatus` (CSR read from mstatus to t0)
- **THEN** t0 SHALL equal `0x00001800`
- **AND** no exception SHALL be raised

#### Scenario: Writing mepc updates the stored PC
- **WHEN** the CPU is in machine mode and executes `csrw mepc, t0` with t0 = `0x80000010`
- **THEN** mepc SHALL be updated to `0x80000010`
- **AND** a subsequent read of mepc SHALL return `0x80000010`

### Requirement: Trap delivery SHALL route exceptions to mcause/mepc/mtval and jump to mtvec

When an exception (synchronous or asynchronous) occurs, the CPU SHALL atomically:
1. Write the trap cause to `mcause` (e.g., 11 for ecall, 12/13/15 for page fault from MMU)
2. Write the faulting PC (or current PC for asynchronous traps) to `mepc`
3. Write the trap value (e.g., faulting address for page fault) to `mtval`
4. Update `mstatus.MPP` to the current privilege mode and `mstatus.MPIE` to `mstatus.MIE`, then clear `mstatus.MIE`
5. Set PC to `mtvec` (or `mtvec + 4 * (cause & 0x3F)` for vectored mode)

#### Scenario: ECALL raises environment call exception
- **WHEN** the CPU executes an `ecall` instruction in machine mode at PC `0x80000010`
- **THEN** mcause SHALL be set to 11 (environment call from M-mode)
- **AND** mepc SHALL be set to `0x80000010` (the ecall instruction's PC)
- **AND** PC SHALL be set to `mtvec` (or vectored entry)
- **AND** mstatus.MPP SHALL be set to M-mode (current privilege)

#### Scenario: Page fault from MMU routes to exception path
- **WHEN** MMU detects a page fault (PTE invalid or permission denied) for fetch at vaddr `0x80001000`
- **THEN** MMU SHALL signal the CPU via the `pl::MMU_EXCEPTION` Payload Key with code 12 (instruction page fault) or 1 (instruction access fault)
- **AND** the CPU's exception path SHALL trap with mcause = 12, mepc = current PC, mtval = `0x80001000`
- **AND** PC SHALL jump to mtvec