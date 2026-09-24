## MODIFIED Requirements

### Requirement: DBusPlugin SHALL do real memory access when PicolibcHostMemory is injected

`DBusPlugin` SHALL accept the same optional `PicolibcHostMemory* mem_` constructor parameter (default `nullptr`). When `mem_` is non-null and `dec.op_class == LOAD`, the `at_stage("memory", NORMAL)` closure SHALL read `MEM_ADDR`, call `mem_->read_word(addr)`, and write the result to `MEM_DATA` Payload Key (LOAD width extraction is OUT OF SCOPE for Wave 1 — lb/lh/lbu/lhu tests may fail with `category=feature stub` classification; Wave 2 fix candidate). When `dec.op_class == STORE` and `mem_` is non-null, the closure SHALL dispatch by decoded funct3: funct3 == SB → `mem_->write_byte(addr, val)`; funct3 == SH → `mem_->write_half(addr, val)`; funct3 == SW → `mem_->write_word(addr, val)`. The dispatch SHALL use an if-else chain (NOT switch-on-state, NOT early-return) per ADR-040 Tier-1 #4. When `mem_` is null (legacy), the closure SHALL preserve existing stub behavior (LOAD returns 0, STORE is no-op).

This requirement SUPERSEDES the v0.1.3 STORE behavior (which unconditionally called `write_word`). The LOAD behavior remains unchanged in Wave 1 (only STORE is widened).

#### Scenario: LOAD reads from injected memory
- **WHEN** `DBusPlugin` is constructed with `mem = &host_memory`, `MEM_ADDR = 0x2000`, `op_class == LOAD`, and `pb.run()` is called
- **THEN** `MEM_DATA` on the memory node SHALL equal `host_memory.read_word(0x2000)` (unchanged from v0.1.3)

#### Scenario: SB writes one byte only
- **WHEN** `DBusPlugin` runs STORE with funct3 == SB and `MEM_ADDR = 0x80001200, MEM_DATA = 0xAB`
- **THEN** `mem_->read_byte(0x80001200) == 0xAB` and adjacent bytes at `0x80001201, 0x80001202, 0x80001203` SHALL remain unchanged

#### Scenario: SH writes two bytes little-endian
- **WHEN** `DBusPlugin` runs STORE with funct3 == SH and `MEM_ADDR = 0x80001200, MEM_DATA = 0xCAFE`
- **THEN** `mem_->read_byte(0x80001200) == 0xFE` and `mem_->read_byte(0x80001201) == 0xCA`, and bytes at `0x80001202, 0x80001203` SHALL remain unchanged

#### Scenario: SW writes four bytes (path unchanged)
- **WHEN** `DBusPlugin` runs STORE with funct3 == SW and `MEM_ADDR = 0x80001200, MEM_DATA = 0xCAFEBABE`
- **THEN** `mem_->read_word(0x80001200) == 0xCAFEBABE` (no behavioral change from v0.1.3 SW path)

#### Scenario: legacy null mem still preserves stub behavior
- **WHEN** `DBusPlugin` is constructed with default `mem = nullptr`
- **THEN** LOAD returns 0 and STORE is no-op, preserving v0.1.3 stub behavior

## ADDED Requirements

### Requirement: PicolibcHostMemory SHALL support a parameterized base address window

`PicolibcHostMemory` SHALL accept a configuration struct with three parameters: `base_addr` (default `0x0`), `size` (default `64 * 1024`), and `tohost_addr` (default `0x0`). All address arithmetic (`write_byte`, `write_word`, `write_half`, `read_byte`, `read_word`, `check_tohost`) SHALL be performed relative to `base_addr_` using `offset = addr - base_addr_` (uint64 arithmetic with explicit `if (addr < base_addr_) return;` underflow guard). `check_tohost` SHALL compare the absolute `addr` against the runtime-configured `tohost_addr_` member instead of the previous `kTohostAddr` constant. Multiple instances with different `base_addr` values SHALL coexist (e.g., `add.elf` fixture at `base=0` and `riscv-tests` fixture at `base=0x80000000`).

#### Scenario: base=0 default preserves add.elf behavior
- **WHEN** `PicolibcHostMemory mem;` (default-constructed, base=0, tohost=0)
- **THEN** `mem.write_word(0, 1)` SHALL set `mem.exited() == true` with `exit_code() == 0`, byte-identical to v0.1.3 behavior

#### Scenario: base=0x80000000 window accepts riscv-tests access
- **WHEN** `PicolibcHostMemory mem({base=0x80000000, size=64KB, tohost=0x80001000})`
- **THEN** `mem.read_word(0x80000000)` SHALL return the loaded ELF's first instruction word, NOT zero

#### Scenario: out-of-window access is silently dropped (preserves debug visibility)
- **WHEN** `mem.write_byte(0x10000, 0xFF)` is called on a base=0, size=64KB instance
- **THEN** the write SHALL be silently dropped (no crash, no throw); reads SHALL return 0 — matching v0.1.3 behavior to keep test breakage minimal

### Requirement: PicolibcHostMemory SHALL provide write_half for store-half instructions

`PicolibcHostMemory` SHALL expose `void write_half(std::uint64_t addr, std::uint16_t val)` writing little-endian across two bytes at `addr` and `addr+1`. The function SHALL bounds-check both addresses against `size_` and SHALL call `check_tohost(addr, val & 0xFF)` and `check_tohost(addr+1, (val >> 8) & 0xFF)` independently. This unblocks riscv-tests `sh` (store-half) instructions which are heavily used in `sb`, `sh`, `sw` test variants.

#### Scenario: write_half writes 2 bytes little-endian
- **WHEN** `mem.write_half(0x100, 0xCAFE)` is called
- **THEN** `mem.read_byte(0x100) == 0xFE` and `mem.read_byte(0x101) == 0xCA`

#### Scenario: write_half triggers check_tohost on tohost address
- **WHEN** `mem.write_half(0x80001000, 1)` is called on a base=0x80000000 instance with tohost=0x80001000
- **THEN** `mem.exited() == true` and `exit_code() == 0` (PASS)

### Requirement: PicolibcHostMemory::write_word SHALL fire check_tohost per-byte

`write_word` SHALL call `check_tohost(addr, val & 0xFF)`, `check_tohost(addr+1, (val >> 8) & 0xFF)`, `check_tohost(addr+2, (val >> 16) & 0xFF)`, and `check_tohost(addr+3, (val >> 24) & 0xFF)` instead of the previous single call with `val & 0xFF`. This fixes a latent bug where multi-byte writes to a tohost address at non-zero offset (e.g., riscv-tests writes `sw zero, tohost+4, t5` after the test result) would fail to fire `exited()`.

#### Scenario: write_word with non-zero low byte still triggers check_tohost
- **WHEN** `mem.write_word(0x80001000, 0xCAFEBABE)` is called on a base=0x80000000 instance with tohost=0x80001000
- **THEN** `mem.exited() == true` with `exit_code() == 1` (FAIL, since val != 1) — proving check_tohost fires on the lowest byte

### Requirement: cf::tools::load_elf_full SHALL load all SHF_ALLOC PROGBITS sections

`cf::tools::load_elf_full(const std::string& path)` SHALL return an `ElfLoadResult { vector<pair<uint64_t, vector<uint8_t>>> sections; uint64_t entry_addr; uint64_t tohost_addr; }`. The loader SHALL iterate ALL section headers and collect every section where `SHT_PROGBITS == sh_type && SHF_ALLOC == (flags & SHF_ALLOC)`. The previous `break` after the first PROGBITS section SHALL be removed. Section overlap (two sections whose `[sh_addr, sh_addr+sh_size)` ranges intersect) SHALL throw `runtime_error`. The `e_entry` field SHALL be parsed and returned as `entry_addr` (NOT skipped). The `.tohost` section's `sh_addr` SHALL be extracted via shstrtab lookup and returned as `tohost_addr`; if absent, `tohost_addr = UINT64_MAX`.

#### Scenario: multi-section ELF loads all PROGBITS
- **WHEN** `load_elf_full(rv32ui_p_add_elf_path)` is called on a riscv-tests ELF with .text.init + .tohost + .data sections
- **THEN** `result.sections.size() == 3` (or more) and each section's bytes SHALL match the ELF content at `sh_addr`

#### Scenario: entry_addr equals e_entry from ELF header
- **WHEN** `load_elf_full(rv32ui_p_add_elf_path)` is called
- **THEN** `result.entry_addr == 0x80000000` (riscv-tests p env link default)

#### Scenario: tohost_addr extracted from .tohost section sh_addr
- **WHEN** `load_elf_full(rv32ui_p_add_elf_path)` is called
- **THEN** `result.tohost_addr` SHALL equal the sh_addr of the `.tohost` section (NOT hardcoded to `base + 0x1000`)

#### Scenario: ELF with no .tohost section returns UINT64_MAX
- **WHEN** an ELF without `.tohost` section is loaded
- **THEN** `result.tohost_addr == UINT64_MAX` and callers SHALL detect via `REQUIRE(result.tohost_addr != UINT64_MAX)` (fixture contract)

### Requirement: cpu_sim SHALL write e_entry into fetch node PC after build_cpu

`cpu_sim`'s ELF load + run loop SHALL, after `CpuFactory::build_cpu(cfg, &mem)` returns and before the first `pb->run()`, write `result.entry_addr` to the fetch node's `KeyType::PC` Payload Key via `pb->node_of_logic_stage("fetch")->operator()(KeyType::PC) = result.entry_addr`. This ensures the fetch plugin reads from the ELF's entry point on cycle 0, not the payload default 0. For `add.elf` (entry=0) and base=0 instances, this SHALL be a no-op byte-identical to v0.1.3.

#### Scenario: riscv-tests ELF fetches from 0x80000000 on cycle 0
- **WHEN** `cpu_sim --elf rv32ui-p-add --base-addr 0x80000000` is invoked
- **THEN** cycle 0's fetch node INSTRUCTION SHALL equal the word at ELF sh_addr=0x80000000

#### Scenario: add.elf cycle 0 still fetches from 0
- **WHEN** `cpu_sim --elf add.elf` (default base-addr=0) is invoked
- **THEN** cycle 0's fetch node INSTRUCTION SHALL equal `mem.read_word(0)` — byte-identical to v0.1.3 add.elf → tohost=1 path

### Requirement: cpu_sim SHALL expose --base-addr flag

`cpu_sim` SHALL accept a new CLI flag `--base-addr HEX` (default `0x0`). When set, the value SHALL be passed to `PicolibcHostMemory::Config::base_addr` and to the `load_elf_full` caller (which uses it to compute the load window). Default behavior preserves v0.1.3 add.elf invocations.

#### Scenario: --base-addr 0x80000000 selects riscv-tests memory window
- **WHEN** `cpu_sim --elf rv32ui-p-add --base-addr 0x80000000 --cycles 100` is invoked
- **THEN** `PicolibcHostMemory` SHALL be constructed with `base_addr=0x80000000`, `tohost_addr` from the ELF's `.tohost` section, and run-to-exit detection SHALL work

#### Scenario: default base-addr=0 preserves add.elf
- **WHEN** `cpu_sim --elf add.elf` (no `--base-addr`)
- **THEN** behavior SHALL be byte-identical to v0.1.3 (default base=0)