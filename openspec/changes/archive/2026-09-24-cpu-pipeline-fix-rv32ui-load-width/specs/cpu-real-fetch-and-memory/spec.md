## ADDED Requirements

### Requirement: DBusPlugin SHALL implement full LOAD width extraction (LB/LH/LW/LBU/LHU) per funct3

`DBusPlugin`'s `at_stage("memory", NORMAL)` closure SHALL dispatch LOAD operations by decoded funct3 to extract the correct width from `PicolibcHostMemory`:

- funct3=000 (LB): `mem_->read_byte(addr)` then `static_cast<int32_t>(static_cast<int8_t>(byte))` for sign extension to 32 bits
- funct3=001 (LH): `mem_->read_half(addr)` then `static_cast<int32_t>(static_cast<int16_t>(half))` for sign extension
- funct3=010 (LW): `mem_->read_word(addr)` (no extension needed)
- funct3=100 (LBU): `mem_->read_byte(addr)` zero-extend (read_byte returns zero-extended uint8_t)
- funct3=101 (LHU): `mem_->read_half(addr)` zero-extend (read_half returns zero-extended uint16_t)

The dispatch SHALL use an if-else chain (NOT switch-on-state, NOT early-return) per ADR-040 Tier-1 #4.

#### Scenario: LB sign-extends byte to 32 bits
- **WHEN** DBusPlugin runs LOAD with funct3=000 and `MEM_ADDR = 0x80001200`
- **THEN** `mem_->read_byte(0x80001200)` returns byte (zero-extended to 0x000000XX)
- **AND** the result is sign-extended via `static_cast<int8_t>` then promoted to int32_t
- **AND** the resulting `RD_DATA` reflects negative values correctly (e.g., 0xFF → 0xFFFFFFFF)

#### Scenario: LW returns full word unchanged
- **WHEN** DBusPlugin runs LOAD with funct3=010 and `MEM_ADDR = 0x80002000`
- **THEN** `mem_->read_word(0x80002000)` returns the 32-bit word directly
- **AND** no sign or zero extension is applied

### Requirement: LOAD value MUST propagate to RD_DATA for writeback consumption

When `DBusPlugin` executes a LOAD instruction and the load value is written to `MEM_DATA` Payload Key on the memory node, the SAME load value SHALL also be written to the `RD_DATA` Payload Key on the memory node. This is required so that `RegFilePlugin`'s writeback stage can read `RD_DATA` and write the loaded value to the destination register. Without this propagation, `RD_DATA` would carry stale data from the previous instruction, causing BNE/BEQ/BLT/BGE comparisons against the loaded register to incorrectly fail.

#### Scenario: LW propagates to both MEM_DATA and RD_DATA
- **WHEN** DBusPlugin runs LOAD with funct3=010 and `MEM_ADDR = 0x80002000` returning word `0x55555555`
- **THEN** `MEM_DATA` on the memory node SHALL equal `0x55555555`
- **AND** `RD_DATA` on the memory node SHALL also equal `0x55555555`
- **AND** `RegFilePlugin`'s writeback stage SHALL read `RD_DATA = 0x55555555` and write it to the destination register

#### Scenario: Subsequent BNE uses loaded value
- **WHEN** LW loads `0x55555555` into x5 (per the previous scenario)
- **AND** a subsequent BNE instruction compares x5 against x6 (which holds `0xAAAAAAAA`)
- **THEN** the BNE SHALL observe `x5 = 0x55555555 != 0xAAAAAAAA` and take the branch
- **AND** the loaded value MUST have been correctly propagated through writeback