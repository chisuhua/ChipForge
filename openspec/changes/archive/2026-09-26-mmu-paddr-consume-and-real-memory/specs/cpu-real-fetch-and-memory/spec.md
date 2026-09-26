## MODIFIED Requirements

### Requirement: IBusPlugin MUST prefer `pl::PADDR` over `pl::PC` when MMU is enabled and `paddr_valid`

When MMU is enabled (`RiscvMMUPlugin` registered) and the `paddr_valid` flag is true (indicating the MMU has successfully translated the virtual address), `IBusPlugin`'s `at_stage("fetch", NORMAL)` closure SHALL read the instruction from `mem_->read_word(pl::PADDR)` instead of `mem_->read_word(pl::PC)`. When MMU is disabled or `paddr_valid` is false (e.g., during PTW walk or canonical ordering violation), the closure SHALL fall back to reading from `pl::PC` (vaddr) for back-compat.

#### Scenario: MMU translation success reads from physical address
- **WHEN** IBusPlugin runs fetch stage
- **AND** `RiscvMMUPlugin::at_stage("tlb_lookup_ifetch")` LATE phase has written `pl::PADDR = 0x1000` and `paddr_valid = true`
- **THEN** IBusPlugin SHALL call `mem_->read_word(0x1000)` (physical address)
- **AND** the INSTRUCTION Payload Key SHALL equal `mem_->read_word(0x1000)`
- **AND** `pl::PC` (vaddr) SHALL NOT be used as the read source

#### Scenario: MMU disabled or paddr_valid false falls back to vaddr
- **WHEN** IBusPlugin runs fetch stage
- **AND** MMU is disabled (`enable_mmu = false`) OR `paddr_valid = false`
- **THEN** IBusPlugin SHALL call `mem_->read_word(pl::PC)` (virtual address)
- **AND** the INSTRUCTION Payload Key SHALL equal `mem_->read_word(pl::PC)`

### Requirement: DBusPlugin MUST prefer `pl::PADDR` over `pl::MEM_ADDR` when MMU is enabled and `paddr_valid`

When MMU is enabled and `paddr_valid` is true, `DBusPlugin`'s `at_stage("memory", NORMAL)` closure SHALL access memory via `pl::PADDR` (physical address) instead of `pl::MEM_ADDR` (virtual address). Fallback to `pl::MEM_ADDR` when MMU is disabled or `paddr_valid` is false.

#### Scenario: MMU translation success reads from physical address
- **WHEN** DBusPlugin runs memory stage with `op_class == LOAD`
- **AND** `RiscvMMUPlugin::at_stage("tlb_lookup_loadstore")` LATE phase has written `pl::PADDR = 0x2000` and `paddr_valid = true`
- **THEN** DBusPlugin SHALL call `mem_->read_word(0x2000)` (physical address)
- **AND** the MEM_DATA Payload Key SHALL equal `mem_->read_word(0x2000)`

#### Scenario: STORE with MMU enabled writes to physical address
- **WHEN** DBusPlugin runs memory stage with `op_class == STORE` and funct3=SW
- **AND** `pl::PADDR = 0x3000` and `paddr_valid = true`
- **THEN** DBusPlugin SHALL call `mem_->write_word(0x3000, mem_data)`
- **AND** `pl::MEM_ADDR` SHALL NOT be used as the write address