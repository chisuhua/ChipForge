## ADDED Requirements

### Requirement: StageLinkPlugin SHALL propagate Payload Keys across pipeline stages

`StageLinkPlugin` (new file at `ip/cpu/plugins/stage_link.h/.cpp`) MUST register one `at_stage(stage, Phase::EARLY)` closure per stage boundary that copies Payload Keys from the previous stage's PipeNode to the current stage's PipeNode. This is required because per-stage PipeNode is an independent KV store (`pipe_node.h:165`) with no automatic data propagation; without these links, downstream stages (decode/execute/memory/writeback) cannot read keys written by upstream stages (fetch/decode/execute/memory).

The propagation order SHALL be:
- fetch→decode: copy `PC`, `INSTRUCTION`
- decode→execute: copy `PC`, `DECODE`, `RISCV_DETAIL`, `RS1`, `RS2`
- execute→memory: copy `PC`, `DECODE`, `MEM_ADDR`, `MEM_DATA`, `RD_DATA`
- memory→writeback: copy `PC`, `DECODE`, `RD_DATA`, `MEM_DATA`

`PC` MUST be propagated through all four boundaries (not only fetch→decode→execute) because the `cpu-pc-update-on-writeback` spec requires the writeback-stage `IBusPlugin` closure to read `PC` from the writeback node to compute `pc + 4` (non-branch path). Without `PC` in the execute→memory and memory→writeback propagation lists, the writeback closure reads the default value 0 and computes `pc + 4 = 4`, breaking sequential instruction flow.

Each propagation closure MUST run at `Phase::EARLY` of its destination stage (so it executes before any business closure at that stage in the canonical run() order from `cpu-stage-ordered-scheduling`).

#### Scenario: fetch→decode propagation makes INSTRUCTION available to decode
- **WHEN** `IBusPlugin` writes `INSTRUCTION=0x00000013` to the fetch node in `at_stage("fetch", NORMAL)`
- **THEN** in the same `pb.run()`, `RiscvDecodePlugin`'s `at_stage("decode", NORMAL)` closure MUST be able to read `INSTRUCTION=0x00000013` from the decode node (because the fetch→decode link ran in EARLY phase before decode NORMAL)

#### Scenario: decode→execute propagation makes DECODE/regfile values available to execute
- **WHEN** `RiscvDecodePlugin` writes `DECODE` + `RISCV_DETAIL` + `RS1/RS2` (from `RegFilePlugin::read`) to the decode node, and ALU/branch plugins read these from the execute node
- **THEN** the decode→execute link SHALL copy `DECODE`, `RISCV_DETAIL`, `RS1`, `RS2` to the execute node before any execute-stage closure runs

#### Scenario: execute→memory and memory→writeback chain
- **WHEN** `RiscvIntAluPlugin` writes `RD_DATA` to execute node, `RiscvLsuPlugin` writes `MEM_ADDR`+`MEM_DATA` to memory node, `RiscvBranchPlugin` writes `branch_taken`+`branch_target` to writeback node
- **THEN** the execute→memory and memory→writeback links SHALL propagate these keys so writeback-stage `RegFilePlugin::write` reads the correct `RD_DATA` from the writeback node

### Requirement: StageLinkPlugin propagation MUST be D4-compliant

`StageLinkPlugin`'s propagation closures MUST NOT use `if (cond) return;` early-return pattern (forbidden by ADR-040 §2.3 / `tools/check_plugin_portability.sh`). Each propagation SHALL be guarded with `node != nullptr` check but otherwise unconditional reads from upstream + unconditional writes to downstream. The link plugin MUST NOT introduce any `tick()` business override (link is purely declarative data movement, not state machine).

#### Scenario: Null node safety
- **WHEN** a stage's PipeNode is unavailable (`pb.node_of_logic_stage("X")` returns nullptr), the link closure SHALL skip that specific link without error
- **THEN** other links SHALL continue executing unaffected

#### Scenario: D4 compliance verified
- **WHEN** `bash tools/check_plugin_portability.sh` runs against `ip/cpu/plugins/stage_link.cpp`
- **THEN** the script MUST report no `std::optional` / `std::variant` / `virtual` / `dynamic_alloc` usage, no `tick()` override, no `enum class State` + `switch` pattern, and no `if (cond) return;` early-return in `at_stage()` closures
