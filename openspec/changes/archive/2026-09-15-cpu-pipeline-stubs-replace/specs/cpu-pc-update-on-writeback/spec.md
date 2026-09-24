## ADDED Requirements

### Requirement: IBusPlugin SHALL update PC at writeback/LATE phase

`IBusPlugin`'s `at_stage("writeback", Phase::LATE)` closure SHALL read `DECODE.branch_taken` and `DECODE.branch_target` from the writeback node (propagated from execute via `cpu-stage-link-propagation`). It SHALL then write `PC = branch_taken ? branch_target : (pc + 4)` to the fetch node's `PC` Payload Key. The single-pass execution semantics mean no misprediction / flush is needed (next instruction fetch happens on the next `pb.run()` call).

When `mem_` is null (legacy mode), the PC update SHALL still occur (PC advance is independent of memory injection; without PC update, every cycle re-fetches the same address, which is harmless but pointless). For back-compat with `test_ibus.cpp`, PC update in legacy mode is acceptable (the test does not assert PC behavior).

#### Scenario: Non-taken branch advances PC by 4
- **WHEN** `RiscvBranchPlugin` writes `branch_taken = false` to `DECODE`, and current PC is 0x1000
- **THEN** after `pb.run()`, the fetch node's `PC` Payload Key SHALL equal 0x1004

#### Scenario: Taken branch redirects PC to branch_target
- **WHEN** `RiscvBranchPlugin` writes `branch_taken = true` and `branch_target = 0x2000` to `DECODE`, and current PC is 0x1000
- **THEN** after `pb.run()`, the fetch node's `PC` Payload Key SHALL equal 0x2000

#### Scenario: PC update independent of mem injection
- **WHEN** IBusPlugin is constructed with `mem = nullptr` (legacy mode) and the pipeline runs any instruction
- **THEN** the writeback/LATE PC update closure SHALL still execute and advance PC by 4 (or follow branch_target); only the INSTRUCTION fetch itself is stubbed to NOP
