## ADDED Requirements

### Requirement: PTW SHALL read PTE from real memory via MemoryInterface when SoC JSON specifies

The `PTW` (Page Table Walker) in `ip/mmu/lib/ptw.h/.cpp` SHALL support reading Page Table Entries (PTEs) from real physical memory via a new `MemoryInterface` abstraction class, in addition to the existing `advance_from_stub()` method that reads from a test-only `pte_stub_memory_` mock. When `MMUPlugin` is constructed with a non-null `MemoryInterface* mem_`, the at_stage closures SHALL call `ptw_->advance_from_real_memory(mem_)` instead of `advance_from_stub()`. The SoC JSON `mmu.memory_interface` field controls whether MMU is constructed with the SoC memory (e.g., `PicolibcHostMemory`) or with no memory (legacy mode, stub).

#### Scenario: PTW walk reads L0 PTE from real memory
- **WHEN** MMUPlugin is constructed with `MemoryInterface* mem = &picolibc_host_memory`
- **AND** PTW walks level 0 (L0) of a Sv32 page table
- **AND** root page table base is `0x80000000` (from satp.ppn)
- **AND** VPN[1] is `0x1`
- **THEN** `advance_from_real_memory(mem)` SHALL call `mem->read_word(0x80000000 + 0x1 * 4)` to read the L0 PTE
- **AND** `advance_from_stub()` SHALL NOT be called

#### Scenario: PTW walk without MemoryInterface falls back to stub
- **WHEN** MMUPlugin is constructed with `MemoryInterface* mem = nullptr` (legacy mode)
- **THEN** `advance_from_stub()` SHALL be called
- **AND** `pte_stub_memory_` SHALL provide the PTE values

### Requirement: MemoryInterface abstraction SHALL provide read_word and write_word operations

A new abstract class `cf::ip::mmu::MemoryInterface` SHALL be defined in `ip/mmu/lib/memory_interface.h` with at least these methods:

```cpp
class MemoryInterface {
 public:
  virtual uint32_t read_word(uint64_t addr) = 0;
  virtual void write_word(uint64_t addr, uint32_t val) = 0;
  virtual ~MemoryInterface() = default;
};
```

`PicolibcHostMemory` SHALL inherit from `MemoryInterface` (via modification of its class definition to add `: public cf::ip::mmu::MemoryInterface` and the override declarations).

#### Scenario: PicolibcHostMemory inherits from MemoryInterface
- **WHEN** `PicolibcHostMemory` is constructed with `Config{base_addr=0x80000000, size=65536}`
- **THEN** it SHALL be usable as a `MemoryInterface*` (polymorphism via virtual methods)
- **AND** `mem->read_word(addr)` SHALL return the 32-bit value stored at `addr - base_addr` (or `0` if out-of-range)

#### Scenario: PTW walks via MemoryInterface interface
- **WHEN** a Sv32 walk completes via `MemoryInterface*`
- **THEN** the walk SHALL terminate with the leaf PTE's PPN translated to physical address
- **AND** `pl::PADDR` SHALL be written with the translated physical address
- **AND** `multi_tlb_->refill_from_ptw(vaddr, asid, paddr, perms)` SHALL be called to populate the TLB