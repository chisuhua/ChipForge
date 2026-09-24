# cpu-m4g-payload-extensibility Specification

## Purpose
Extends `cf::cpu::core::payload::keys<T, XLEN>` with three additional Payload Keys (`UID` #12, `THREAD_ID` #13, `IID_PC` #14) supporting Phase 5+ OoO ROB indexing, SMT thread identification, and superscalar PC tagging. Append-only — existing 11 Keys remain byte-identical, preserving Phase 1 ABI.
## Requirements
### Requirement: CPU Payload extensibility for OoO and SMT

The system SHALL provide three additional Payload identifiers in `cf::cpu::core::payload::keys<T, XLEN>` to support Phase 5+ OoO (ROB index) and SMT (thread ID) identification.

The Payload identifiers SHALL be:

- `UID`: `cf::plugin::Payload<cf::plugin::uint_t<8>>` with key name `"cpu.uid"`, default value 0, representing instruction unique identifier for OoO ROB indexing (range 0-255). Declared as **#12** in the `keys<T, XLEN>` struct (after the existing 11 Keys).
- `THREAD_ID`: `cf::plugin::Payload<cf::plugin::uint_t<2>>` with key name `"cpu.tid"`, default value 0, representing thread identifier for SMT (range 0-3). Declared as **#13** in the `keys<T, XLEN>` struct.
- `IID_PC`: `cf::plugin::Payload<T>` with key name `"cpu.iid_pc"`, representing PC tagged to instruction ID for superscalar lane distinction. Declared as **#14** in the `keys<T, XLEN>` struct.

The system SHALL NOT modify the existing 11 Payload Keys (PC, INSTRUCTION, RS1, RS2, RD_DATA, RD_IDX, DECODE, RESULT, MEM_ADDR, MEM_DATA, MEM_SIZE) in name, type, or declaration order. New Keys are appended after them.

In Phase 1, `UID` SHALL default to 0 and `THREAD_ID` SHALL default to 0. No plugin SHALL be required to consume these Payloads in Phase 1.

#### Scenario: Default UID Payload exists

- **WHEN** a test accesses `cf::cpu::core::payload::keys<uint32_t, 32>::UID`
- **THEN** the Payload object SHALL exist with key name `"cpu.uid"` and type `cf::plugin::uint_t<8>`

#### Scenario: Default THREAD_ID Payload exists

- **WHEN** a test accesses `cf::cpu::core::payload::keys<uint32_t, 32>::THREAD_ID`
- **THEN** the Payload object SHALL exist with key name `"cpu.tid"` and type `cf::plugin::uint_t<2>`

#### Scenario: Default IID_PC Payload exists

- **WHEN** a test accesses `cf::cpu::core::payload::keys<uint32_t, 32>::IID_PC`
- **THEN** the Payload object SHALL exist with key name `"cpu.iid_pc"` and type `uint32_t`

#### Scenario: Existing Payloads unchanged

- **WHEN** a test verifies the existing 11 Payload Keys
- **THEN** all 11 keys SHALL remain unchanged in name, type, and declaration order, and the 3 new keys SHALL be appended after them as #12 (UID), #13 (THREAD_ID), #14 (IID_PC)

### Requirement: CPU plugins are templated on N_REGS and N_THREADS

The system SHALL provide template parameters `N_REGS` and `N_THREADS` on `RegFilePlugin<T>`, `HazardPlugin<T>`, and `BranchPredictorPlugin<T>` to support Phase 5+ superscalar and SMT configurations.

Template parameters SHALL have the following defaults that preserve the current ABI:

- `RegFilePlugin<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- `HazardPlugin<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- `BranchPredictorPlugin<typename T, std::size_t BTB_SIZE = 16, std::size_t BIMODAL_SZ = 16, std::size_t GSHARE_SZ = 16, std::uint8_t GHR_BITS = 8, std::size_t N_THREADS = 1>`

The system SHALL enforce compile-time constraints via `static_assert`:

- `T` MUST be unsigned (`std::is_unsigned<T>::value`)
- `N_REGS` MUST be in range [1, 128]
- `N_THREADS` MUST be in range [1, 4]
- `N_REGS` MUST be a power of 2 OR equal to 1

Storage layout SHALL be `std::array<array_store<T, N_REGS>, N_THREADS>` for RegFile and `std::array<std::array<bool, N_REGS>, N_THREADS>` for Hazard scoreboard.

All public methods that access per-thread state SHALL accept a `tid` parameter of type `std::uint8_t` with default value 0.

#### Scenario: Default template parameters preserve ABI

- **WHEN** existing code declares `RegFilePlugin<uint32_t>` without explicit template arguments
- **THEN** the system SHALL instantiate `RegFilePlugin<uint32_t, 32, 1>` and the behavior SHALL be identical to the pre-M4G version

#### Scenario: Custom N_REGS compiles

- **WHEN** code declares `RegFilePlugin<uint32_t, 8>` with non-default N_REGS
- **THEN** the system SHALL compile successfully and the instance SHALL have 8 registers

#### Scenario: Multi-thread configuration compiles

- **WHEN** code declares `HazardPlugin<uint32_t, 32, 2>` with N_THREADS=2
- **THEN** the system SHALL compile successfully and the scoreboard SHALL have per-thread isolation

#### Scenario: Invalid N_REGS rejected at compile time

- **WHEN** code declares `RegFilePlugin<uint32_t, 200>` with N_REGS > 128
- **THEN** the compiler SHALL emit a `static_assert` failure error

#### Scenario: Invalid N_THREADS rejected at compile time

- **WHEN** code declares `HazardPlugin<uint32_t, 32, 8>` with N_THREADS > 4
- **THEN** the compiler SHALL emit a `static_assert` failure error

#### Scenario: Per-thread register isolation

- **WHEN** a test writes register 5 with value 100 on thread 0 and value 200 on thread 1
- **THEN** `read_reg(5, 0)` SHALL return 100 and `read_reg(5, 1)` SHALL return 200

#### Scenario: Per-thread scoreboard isolation

- **WHEN** a test marks register 3 in-flight on thread 0
- **THEN** `has_raw(3, 0)` SHALL return true and `has_raw(3, 1)` SHALL return false

### Requirement: BranchPredictor tid parameter threading

The system SHALL provide a `tid` parameter on `BranchPredictorPlugin::predict` and `BranchPredictorPlugin::update` methods to support per-thread global history register (GHR).

The `tid` parameter SHALL be of type `std::uint8_t` with default value 0.

The `global_history_` field SHALL be `std::array<std::uint8_t, N_THREADS>` indexed by `tid`.

Existing test cases that call `predict(pc)` and `update(pc, taken, target)` without explicit `tid` SHALL continue to pass without modification (default `tid=0`).

#### Scenario: Default tid preserves behavior

- **WHEN** a test calls `bp.predict(0x1000)` without explicit tid
- **THEN** the system SHALL use tid=0 and behavior SHALL be identical to pre-M4G

#### Scenario: Per-thread GHR isolation

- **WHEN** a test updates the predictor with `(pc=0x2000, taken=true, target=0x3000, tid=1)`
- **THEN** `global_history_[0]` SHALL remain unchanged and `global_history_[1]` SHALL be updated

### Requirement: HazardKind enum discriminates hazard types

The system SHALL provide an `enum class HazardKind : std::uint8_t` with values `NONE`, `RAW_RS1`, `RAW_RS2`, `WAW` to allow Phase 5+ issue logic to distinguish hazard sources.

`HazardPlugin::has_hazard` SHALL return `HazardKind` (replacing previous `bool` return type).

The `has_hazard` method SHALL accept a `tid` parameter of type `std::uint8_t` with default value 0.

The function body SHALL detect:

- `HazardKind::RAW_RS1`: `dec.reads_rs1 && has_raw(dec.rs1_idx, tid)`
- `HazardKind::RAW_RS2`: `dec.reads_rs2 && has_raw(dec.rs2_idx, tid)`
- `HazardKind::WAW`: `dec.writes_rd && has_waw(dec.rd_idx, tid)`
- `HazardKind::NONE`: no hazard detected

#### Scenario: No hazard returns NONE

- **WHEN** a test calls `has_hazard(dec)` with empty DecodePayload
- **THEN** the system SHALL return `HazardKind::NONE`

#### Scenario: RAW on RS1 detected

- **WHEN** register 5 is marked in-flight and `dec.reads_rs1=true, dec.rs1_idx=5`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::RAW_RS1`

#### Scenario: RAW on RS2 detected

- **WHEN** register 7 is marked in-flight and `dec.reads_rs2=true, dec.rs2_idx=7`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::RAW_RS2`

#### Scenario: WAW detected

- **WHEN** register 10 is marked in-flight and `dec.writes_rd=true, dec.rd_idx=10`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::WAW`

#### Scenario: In-tree caller updated

- **WHEN** the `HazardPlugin::build()` callback calls `has_hazard`
- **THEN** the caller SHALL check `h != HazardKind::NONE` and dispatch to the appropriate stall logic

### Requirement: Forward compatibility tests cover D.1-D.4

The system SHALL provide a test file `tests/cpu/test_forward_compat.cpp` with at least 8 GoogleTest cases verifying D.1-D.4 behaviors.

The test file SHALL be registered in `tests/cpu/CMakeLists.txt` and SHALL be runnable via `ctest -R ForwardCompat`.

Required test cases (minimum):

- `D1_UidPayloadExists`: Verify UID Payload object exists with name `"cpu.uid"`
- `D1_ThreadIdPayloadExists`: Verify THREAD_ID Payload object exists with name `"cpu.tid"`
- `D1_IidPcPayloadExists`: Verify IID_PC Payload object exists with name `"cpu.iid_pc"`
- `D2_RegFilePluginTemplated`: Verify `RegFilePlugin<uint32_t, 8>` compiles and behaves correctly
- `D2_RegFilePluginMultiThread`: Verify per-thread register isolation with `N_THREADS=2`
- `D2_HazardPluginMultiThread`: Verify per-thread scoreboard isolation with `N_THREADS=2`
- `D3_HazardKindEnum`: Verify all 4 enum values via `has_hazard` scenarios
- `D4_BranchPredictorTidParam`: Verify `tid` parameter passes through to `global_history_`
- `ExistingRegFileTests`: Verify default parameters preserve pre-M4G behavior

#### Scenario: ForwardCompat tests pass

- **WHEN** developer runs `ctest -R ForwardCompat`
- **THEN** all 8+ test cases SHALL pass

#### Scenario: Existing tests not regressed

- **WHEN** developer runs full `ctest` suite
- **THEN** existing 35 ctest cases SHALL continue to pass without modification

### Requirement: Documentation reflects M4G lock state

The system SHALL update three documentation files to reflect M4G completion:

- `ip/cpu/docs/blueprint.md` §5 (CpuFactory) SHALL annotate "M4G 后已锁定 N_REGS/N_THREADS 模板参数"
- `ip/cpu/docs/status.md` SHALL add §4.2 "M4G 子阶段" with task status table referencing D.1-D.4
- `ip/cpu/docs/README.md` index SHALL include link to `implementation-plan/M4G-forward-compat-locks.md`

The existing `dse_architecture_v2_locks.md` SHALL NOT be modified (already Oracle-reviewed).

#### Scenario: Blueprint references M4G

- **WHEN** a reader opens `ip/cpu/docs/blueprint.md` §5
- **THEN** the section SHALL contain the M4G lock annotation with link to `implementation-plan/M4G-forward-compat-locks.md`

#### Scenario: Status board tracks M4G

- **WHEN** a reader opens `ip/cpu/docs/status.md`
- **THEN** §4.2 SHALL exist with M4G sub-task progress (G.1-G.8)

#### Scenario: README index lists M4G

- **WHEN** a reader opens `ip/cpu/docs/README.md`
- **THEN** the implementation plans table SHALL include a row for `M4G-forward-compat-locks.md`

### Requirement: Phase 1 framework spine is unchanged

The system SHALL NOT modify any file in `include/cf/plugin/` or `ip/cpu/core/payload_common.h`'s existing 11 Payload declarations.

The Phase 1 framework spine that SHALL remain unchanged includes:

- `cf::plugin::PipeBuilder` (`include/cf/plugin/pipe_builder.h`)
- `cf::plugin::PluginBase` (`include/cf/plugin/plugin_base.h`)
- `cf::plugin::PipeNode` (`include/cf/plugin/pipe_node.h`)
- `cf::plugin::Payload<T>` and `PayloadStore` (`include/cf/plugin/payload.h`)
- `cf::plugin::CtrlLink` (`include/cf/plugin/ctrl_link.h`)
- `cf::plugin::PipeArbitration` (`include/cf/plugin/pipe_arbitration.h`)

#### Scenario: No framework spine modifications

- **WHEN** developer runs `git diff include/cf/plugin/` after M4G merge
- **THEN** the diff SHALL be empty

#### Scenario: Existing Payload declarations unchanged

- **WHEN** developer inspects `ip/cpu/core/payload_common.h`
- **THEN** the existing 11 Payload declarations (PC, INSTRUCTION, RS1, RS2, RD_DATA, RD_IDX, DECODE, RESULT, MEM_ADDR, MEM_DATA, MEM_SIZE) SHALL be unchanged in name, type, and order, and the 3 new keys SHALL be appended after them as #12 (UID), #13 (THREAD_ID), #14 (IID_PC)
