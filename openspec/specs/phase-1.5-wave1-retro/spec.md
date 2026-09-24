# phase-1.5-wave1-retro Specification

## Purpose
TBD - created by archiving change soc-cpu-l1-mmu-demo. Update Purpose after archive.
## Requirements
### Requirement: L1 — rv32ui-p matrix deviation (30 PASS / 10 FAIL all feature stub)

The Wave 1 `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` SHALL record 30 PASS / 10 FAIL where:
- 30 PASS: add, addi, and, andi, auipc, beq, bge, bgeu, blt, bltu, bne, jal, jalr, lui, or, ori, simple, sll, slli, slt, slti, sltiu, sltu, sra, srai, srl, srli, sub, xor, xori
- 10 FAIL (all `category=feature stub`, not `真 bug`): `lb, lbu, ld_st, lh, lhu, lw, sb, sh, st_ld, sw` — all fail due to LOAD width extraction OOS (DBusPlugin LOAD path returns `read_word` for all widths)

The roadmap `phase-1.5-stall-and-validate.md` originally anticipated the `cpu-pipeline-fix-rv32ui-N` change to fix "≥3 真 bug". The actual Wave 1 data shows **0 真 bug** in the FAIL set. This invalidates the trigger condition for `cpu-pipeline-fix-rv32ui-N` as originally scoped.

#### Scenario: future session queries "is fix-rv32ui-N still needed?"
- **WHEN** a future session reads this spec
- **THEN** it SHALL understand that:
  - The 10 FAILs are all `feature stub` (LOAD width OOS), not 真 bug
  - `cpu-pipeline-fix-rv32ui-N`'s original scope (fix X real bugs) was already mostly covered by d279635's opportunistic 5-bug fix
  - The remaining scope of `fix-rv32ui-N` is ONLY LOAD width extraction → which should be renamed to `cpu-pipeline-rv32ui-load-width`

### Requirement: L2 — d279635 opportunistic 5-bug fix exceeded `cpu-pipeline-fix-rv32ui-N` original scope

The commit `d279635 fix(cpu): 5 pipeline execution bugs blocking riscv-tests rv32ui baseline` (2026-09-15) SHALL be understood by future sessions as fixing 5 real CPU bugs that were NOT in the roadmap's anticipated `cpu-pipeline-fix-rv32ui-N` scope:

1. `ip/cpu/plugins/hazard.h`: execute CtrlLink `halt_when(has_active_hazard())` → `last_decoded_hazard_ != NONE` (resolves permanent stall deadlock)
2. `ip/cpu/arch/riscv/branch.h`: JAL/JALR/B-type dispatch by `rv.opcode` instead of `funct7==0` (resolves B-type misclassification as JAL)
3. `ip/cpu/arch/riscv/int_alu.h`: LUI/AUIPC dispatch by `rv.opcode` instead of `infer_opcode` (resolves U-type misclassification as ADD)
4. `ip/cpu/arch/riscv/decode.h`: `writes_rd` computed per-OpCode instead of `(rd != x0)` (resolves B-type/STORE phantom RAW hazard)
5. `tests/cpu/riscv_tests/env-p/`: ChipForge p-env harness with direct-tohost sw exit (replaces upstream `ecall`-based exit that requires trap machine, Wave 4 scope)

Plus `ip/cpu/arch/riscv/payload_riscv.h`: added `opcode` field to enable branch/int_alu dispatch.

The roadmap anticipated `fix-rv32ui-N` scope was "jalr/auipc/lui U-type fail / fence/i fence / hazard stall" — d279635 actually fixed all of these (and more) in commit B of the apply phase. The `fix-rv32ui-N` change as planned SHALL be considered substantially completed.

#### Scenario: future session reads this spec and roadmap
- **WHEN** evaluating whether to start `fix-rv32ui-N`
- **THEN** the session SHALL recognize that the only remaining work is LOAD width extraction (the `feature stub` items), and SHALL consider renaming/repurposing `fix-rv32ui-N` to `riscv-tests-rv32ui-load-width`

### Requirement: L3 — pre-existing RISC-V simulation failures were misattributed

Future sessions SHALL understand that the `AGENTS.md` "已知测试状态" section (pre-v0.2.2) attribution was incorrect. The pre-v0.2.2 `AGENTS.md` "已知测试状态" section stated:

> "5 个 RISC-V 仿真测试预先存在失败（`test_*stage_riscv` + `test_cpu_sim_real_tohost`）：因 RISC-V 工具链配置（tohost 字符串、riscv64 assembler path），与代码无关。"

This attribution SHALL be understood by future sessions as **incorrect**. Of the 5 pre-existing failures, **4 were cured by d279635's CPU pipeline fixes**:

| Test | Pre-v0.2.2 | Post-v0.2.2 (after d279635) |
|------|------------|--------------------------|
| `test_3stage_riscv` | FAIL | PASS |
| `test_5stage_riscv` | FAIL | PASS |
| `test_7stage_riscv` (`7stage_add_elf_end_to_end`) | FAIL | **STILL FAILS** (segfault, pre-existing superscalar path bug, NOT toolchain) |
| `test_10stage_riscv` | FAIL | PASS |
| `test_cpu_sim_real_tohost` | FAIL | PASS |

The root cause of the original 4 failures was the same HazardPlugin deadlock (`halt_when(has_active_hazard())` causing permanent stall) that d279635 fixed — NOT toolchain config.

#### Scenario: future session sees "pre-existing RISC-V sim failures"
- **WHEN** a future session reads this spec
- **THEN** it SHALL understand that:
  - Only 1 of the original 5 pre-existing failures remains (7-stage superscalar segfault, pre-existing, unrelated to toolchain)
  - The other 4 were genuine CPU pipeline bugs (same root cause as the rv32ui matrix timeout)
  - Future toolchain-attribution for any new pre-existing failure MUST be verified via `git stash` toggle before being trusted

### Requirement: L4 — 7-stage superscalar segfault is pre-existing and out of Wave 2 scope

The test `tests/cpu/integration/test_7stage_riscv.cpp::7stage_add_elf_end_to_end` SHALL be known by future sessions to be a pre-existing failure that:

```bash
./build/src/cf_plugin/cpu_sim \
  --config ip/cpu/configs/cpu_superscalar.json \
  --elf build/add.elf --cycles 200
```

This segfaults (exit code 139) with no stdout/stderr output. Verified via `git stash` toggle on d279635 that the segfault exists on the baseline (pre-d279635) as well — i.e., d279635 did NOT introduce this regression. The segfault is a pre-existing bug in the 7-stage superscalar path.

The segfault SHALL be considered **out of Wave 2 scope** (no production code change required for `soc-cpu-l1-mmu-demo`). It SHALL be planned as a standalone follow-up change `cpu-pipeline-7stage-superscalar-fix` (1-2 day scope).

#### Scenario: future session triages 7-stage segfault
- **WHEN** a future session sees `7stage_add_elf_end_to_end` FAIL
- **THEN** it SHALL:
  - NOT assume this is a regression from recent CPU fixes (verified via `git stash`)
  - Start a dedicated `cpu-pipeline-7stage-superscalar-fix` change (separate from `soc-cpu-l1-mmu-demo`)
  - Use the superscalar config only after the dedicated fix lands

### Requirement: L5 — OpenSpec workflow self-correction record

This spec SHALL be archived automatically when the `soc-cpu-l1-mmu-demo` change is archived (via `openspec archive`). Future Phase 1.5/Phase 2 changes SHOULD reference this spec when evaluating scope (to avoid duplicating the 5-bug fix that was already done, or misattributing toolchain issues to code).

#### Scenario: spec survives change archive
- **WHEN** `openspec archive soc-cpu-l1-mmu-demo --yes` is executed
- **THEN** `phase-1.5-wave1-retro` SHALL be moved to `openspec/specs/` as a permanent spec
- **AND** it SHALL remain queryable via `openspec show phase-1.5-wave1-retro`

