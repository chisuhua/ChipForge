## ADDED Requirements

### Requirement: IBusPlugin SHALL fetch real instructions when PicolibcHostMemory is injected

`IBusPlugin` SHALL accept an optional `PicolibcHostMemory* mem_` constructor parameter (default `nullptr` for back-compat). When `mem_` is non-null, the `at_stage("fetch", NORMAL)` closure SHALL read the 32-bit instruction word at the current PC value via `mem_->read_word(pc)` and write it to the `INSTRUCTION` Payload Key on the fetch node. When `mem_` is null (legacy mode, preserves existing unit-test behavior including `test_ibus.cpp`), the closure SHALL continue to write the hardcoded `NOP` (0x00000013).

#### Scenario: Real fetch with PicolibcHostMemory injection
- **WHEN** `IBusPlugin` is constructed with `mem = &host_memory` (non-null PicolibcHostMemory), PC is set to 0x1000, and `pb.run()` is called
- **THEN** the `INSTRUCTION` Payload Key on the fetch node SHALL equal `host_memory.read_word(0x1000)` (e.g., an `addi` opcode 0x00A00093 if host_memory stores that at 0x1000)

#### Scenario: Legacy NOP behavior preserved
- **WHEN** `IBusPlugin` is constructed with default `mem = nullptr` (legacy path)
- **THEN** the `INSTRUCTION` Payload Key SHALL equal `0x00000013` (NOP), preserving `tests/cpu/test_ibus.cpp` behavior

### Requirement: DBusPlugin SHALL do real memory access when PicolibcHostMemory is injected

`DBusPlugin` SHALL accept the same optional `PicolibcHostMemory* mem_` constructor parameter (default `nullptr`). When `mem_` is non-null and `dec.op_class == LOAD`, the `at_stage("memory", NORMAL)` closure SHALL read `MEM_ADDR`, call `mem_->read_word(addr)`, and write the result to `MEM_DATA` Payload Key. When `dec.op_class == STORE` and `mem_` is non-null, the closure SHALL read `MEM_ADDR` and `MEM_DATA` and call `mem_->write_word(addr, data)`. When `mem_` is null (legacy), the closure SHALL preserve existing stub behavior (LOAD returns 0, STORE is no-op).

#### Scenario: LOAD reads from injected memory
- **WHEN** `DBusPlugin` is constructed with `mem = &host_memory`, `MEM_ADDR = 0x2000`, `op_class == LOAD`, and `pb.run()` is called
- **THEN** `MEM_DATA` on the memory node SHALL equal `host_memory.read_word(0x2000)`

#### Scenario: STORE writes to injected memory
- **WHEN** `DBusPlugin` is constructed with `mem = &host_memory`, `MEM_ADDR = 0x2004`, `MEM_DATA = 0xCAFEBABE`, `op_class == STORE`, and `pb.run()` is called
- **THEN** `host_memory.read_word(0x2004)` SHALL return `0xCAFEBABE` after the run completes

### Requirement: CpuFactory::build_cpu SHALL accept optional PicolibcHostMemory parameter

`CpuFactory::build_cpu<T>(config, PicolibcHostMemory* mem = nullptr)` SHALL gain an optional second parameter. When `mem` is non-null, the factory SHALL inject it into `IBusPlugin` and `DBusPlugin` constructors. When null, the factory SHALL NOT inject (back-compat default). All existing `build_cpu` call sites that pass only `config` SHALL continue to work unchanged (default null → all stubs).

#### Scenario: Back-compat default null
- **WHEN** existing test calls `CpuFactory<T>::build_cpu(config)` (no second arg)
- **THEN** IBusPlugin and DBusPlugin SHALL be constructed with `mem_ = nullptr`, preserving stub behavior

#### Scenario: cpu_sim passes mem
- **WHEN** `cpu_sim` constructs `PicolibcHostMemory mem;` then calls `CpuFactory<T>::build_cpu(cfg, &mem)`
- **THEN** the resulting IBusPlugin and DBusPlugin SHALL have `mem_ = &mem` and operate on the injected memory

### Requirement: RiscvLsuPlugin SHALL NOT write MEM_DATA for LOAD (delegates to DBusPlugin)

`RiscvLsuPlugin`'s `at_stage("memory", NORMAL)` closure (at `lsu.h:63-71`) for LOAD SHALL NOT write `MEM_DATA`. The `MEM_DATA` Payload Key is now exclusively written by `DBusPlugin` after memory read. This removes the double-write conflict between `lsu.h:66` and `dbus.h:56` (both writing `MEM_DATA = T{0}`).

#### Scenario: LOAD leaves MEM_DATA to DBusPlugin
- **WHEN** `RiscvLsuPlugin` at_stage("memory", NORMAL) closure runs for LOAD with `MEM_ADDR` already written
- **THEN** `MEM_DATA` on the memory node SHALL NOT be written by LSU (preserves the value from execute stage or leaves it unset for DBusPlugin to populate from real memory read)

#### Scenario: STORE writes MEM_DATA as before
- **WHEN** STORE path runs through LSU
- **THEN** `MEM_ADDR` and `MEM_DATA` SHALL both be written by LSU (DBusPlugin does not overwrite STORE data)
