# soc-cpu-mmu-demo-topology Specification

## Purpose
TBD - created by archiving change soc-cpu-l1-mmu-demo. Update Purpose after archive.
## Requirements
### Requirement: demo runner SHALL construct topology in C++ (no JSON instantiator)

The `tests/soc/test_cpu_l1_mmu_demo.cpp` runner SHALL construct the CPU+MMU+Memory topology manually in C++, following the `test_rv32ui_runner.cpp` pattern:
1. Load ELF via `cf::tools::load_elf_full` → `entry_addr`, `tohost_addr`, sections
2. Construct `PicolibcHostMemory` (window=0x80000000, tohost_addr=elf.tohost_addr)
3. Build CPU via `CpuFactory` (config=`cpu_default`, `enable_mmu=true`)
4. Inject `PicolibcHostMemory` into IBusPlugin + DBusPlugin
5. Set fetch PC = entry_addr
6. Loop `pb->run()` up to 10000 cycles until `mem.exited()`

The runner SHALL **NOT** call any `cf::soc::Topology::from_json` API (which does not exist).

#### Scenario: runner builds CPU+MMU+Memory manually
- **WHEN** `tests/soc/test_cpu_l1_mmu_demo.cpp` executes
- **THEN** it SHALL instantiate CpuFactory + MMUPlugin + PicolibcHostMemory directly in C++ (no JSON parsing)

#### Scenario: structural JSON is validated separately
- **WHEN** the `tests/soc/` structural test runs against `soc/cpu_l1_mmu_demo.json`
- **THEN** it SHALL `nlohmann::json::parse` the file and field-check the topology (memory_map + 4 components), mirroring `test_mmu_minimal_json.cpp`

### Requirement: demo runner SHALL plant identity stub PTEs before fetch

The demo runner SHALL plant identity-mapped PTEs via `ptw()->stub_write_pte(...)` (the existing stub-backend API, `ptw.h:78`) BEFORE the first fetch, so that the MMU's PTW walk resolves VAs to PAs (identity mapping) and the TLB-refill mechanism (companion spec `mmu-ptw-tlb-refill`) is exercised with valid PTEs.

#### Scenario: stub PTEs are planted before first fetch
- **WHEN** the demo runner initializes the MMU
- **THEN** it SHALL plant identity PTEs covering the demo's memory region (VA==PA for 0x80000000 window) via `stub_write_pte`

#### Scenario: PTW walk + TLB refill is observable
- **WHEN** the demo runs with `enable_mmu=true` and planted stub PTEs
- **THEN** the first access to each VPN SHALL miss → walk → refill (via `refill_from_ptw`), and subsequent accesses to the same VPN SHALL hit the TLB

### Requirement: memory map SHALL be a 64KB window at 0x80000000

The demo SHALL use a single memory region at base `0x80000000`, size `64KB`, matching the riscv-tests env/p linker script (`. = 0x80000000`) so vendored riscv-tests ELFs (commit feb602a) load without modification.

#### Scenario: memory base aligned to riscv-tests linker script
- **WHEN** the demo is instantiated
- **THEN** the memory base SHALL equal `0x80000000` so riscv-tests ELFs with `e_entry=0x80000000` load at their expected address

### Requirement: CPU component SHALL use the default 5-stage config

The CPU SHALL use `ip/cpu/configs/cpu_default.json` (pipeline_stages=5, `enable_mmu=true`, dispatch_width=1). The superscalar 7-stage config SHALL NOT be used (pre-existing segfault, deferred to `cpu-pipeline-7stage-superscalar-fix`).

#### Scenario: default 5-stage CPU works
- **WHEN** the CPU is instantiated with `cpu_default.json`
- **THEN** it SHALL instantiate the 5-stage pipeline with MMU enabled, passing through v0.2.2 pipeline fixes

### Requirement: MMU SHALL use Sv32 with unified TLB

The MMU SHALL be configured via the existing `CpuFactory::build_cpu(enable_mmu=true)` path (registers RiscvMMUPlugin + MMUPlugin). The PTW SHALL populate the unified TLB after walk completion (per companion spec `mmu-ptw-tlb-refill`).

#### Scenario: Sv32 identity translation works (structural)
- **WHEN** the CPU executes with MMU enabled and stub PTEs planted
- **THEN** the PTW resolves VAs to PAs (identity), the TLB-refill mechanism is exercised, and the demo reaches tohost=1

### Requirement: L1 cache SHALL be declared but NOT instantiated (deferred)

The `soc/cpu_l1_mmu_demo.json` SHALL declare an L1CachePlugin component (4KB, 1-way) for topology completeness, but the demo runner SHALL **NOT** instantiate it. L1-in-pipeline validation is deferred to Wave 3 `cache-dse-sweep`.

#### Scenario: L1 is structural only
- **WHEN** a future reader inspects the demo
- **THEN** they SHALL understand L1 is declared in JSON but not wired in the runner (instantiation deferred to Wave 3 `cache-dse-sweep`)

### Requirement: demo SHALL run ≥5 riscv-tests ELFs to `tohost=1`

The demo runner SHALL execute these 5 ELFs from the Wave 1 PASS matrix (pure integer ALU/branch, avoiding LOAD width OOS):
1. `rv32ui-p-add`
2. `rv32ui-p-addi`
3. `rv32ui-p-auipc`
4. `rv32ui-p-jal`
5. `rv32ui-p-beq`

Each ELF SHALL run for up to 10000 cycles and SHALL terminate with `mem.exited() == true && mem.exit_code() == 0` (`tohost=1`).

#### Scenario: all 5 demo ELFs reach tohost=1
- **WHEN** the demo runner executes all 5 ELFs
- **THEN** each SHALL reach `tohost=1` within 10000 cycles and REQUIRE all 5 `exit_code == 0`

#### Scenario: cycle cap exceeded is a hard failure
- **WHEN** any demo ELF does not reach `tohost` within 10000 cycles
- **THEN** the demo runner SHALL REQUIRE `mem.exited() == true` and fail with cycle-count info

### Requirement: demo SHALL NOT regress the Phase 1.5 baseline

After Wave 2 commits land, `chipforge_tests "[riscv-tests]"` SHALL continue to report 30 PASS / 10 FAIL (LOAD-family feature stub unchanged), and the 4 architecture gates SHALL all pass.

#### Scenario: baseline unchanged, gates green
- **WHEN** the demo is committed
- **THEN** `chipforge_tests "[riscv-tests]"` SHALL produce the same 30/10 result as v0.2.2, and all 4 architecture gates SHALL pass

