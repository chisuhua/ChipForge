## ADDED Requirements

### Requirement: TLB lookup MUST perform fully-associative tag match

The TLB lookup algorithm MUST perform fully-associative parallel tag match across all `WAYS × (ENTRIES / WAYS)` cells within a single access cycle. The algorithm MUST compare incoming `vaddr` and `asid` against stored entries in parallel and return the first matching entry (or miss).

#### Scenario: TLB hit on valid entry
- **WHEN** `TLB::lookup(vaddr=0x1000, asid=0x5)` is called and entry {vpn=0x1000, asid=0x5, valid=true} exists
- **THEN** lookup MUST return `{hit=true, paddr=0x1000_0000, perms=R|W|U}` within the same cycle

#### Scenario: TLB miss on no matching entry
- **WHEN** `TLB::lookup(vaddr=0x2000, asid=0x5)` is called and no entry matches vaddr=0x2000 with asid=0x5
- **THEN** lookup MUST return `{hit=false, fault=NONE}` and trigger PTW walk via MMUPlugin

#### Scenario: TLB miss on asid mismatch (even with same vaddr)
- **WHEN** entry exists with {vpn=0x1000, asid=0x5, valid=true} and `TLB::lookup(vaddr=0x1000, asid=0x6)` is called
- **THEN** lookup MUST return `{hit=false}` (different ASID → treat as miss)

### Requirement: TLB insert MUST dispatch to replacement policy

The TLB insert algorithm MUST call the configured `ReplacementPolicy::select_victim` hook before writing the entry, allowing the policy to evict a victim entry if the target set is full. Real API: `insert(uint64_t vaddr, uint64_t paddr, uint16_t asid, uint8_t perms)`.

#### Scenario: Insert into empty TLB
- **WHEN** `TLB::insert(vaddr=0x1000, paddr=0x1000_0000, asid=0x5, perms=R|W|U)` is called on empty TLB
- **THEN** entry MUST be placed in first available slot

#### Scenario: Insert evicts victim when full
- **WHEN** TLB is full (all `ENTRIES` cells valid) and `TLB::insert(...)` is called
- **THEN** `policy_->select_victim(set_idx)` MUST identify a victim, victim MUST be evicted, and new entry MUST replace victim

#### Scenario: Replacement policy dispatch (LRU example)
- **WHEN** TLB is configured with `LRUReplacementPolicy` and full
- **THEN** victim MUST be the entry with smallest `timestamp` (least recently used)

### Requirement: TLB invalidate MUST support three granularities

The TLB MUST support three invalidation granularities via real API: `invalidate_vaddr(vaddr, asid)` (single entry), `invalidate_asid(asid)` (all entries for asid), and `invalidate_all()` (all entries).

#### Scenario: Invalidate by vaddr+asid
- **WHEN** `TLB::invalidate_vaddr(vaddr=0x1000, asid=0x5)` is called
- **THEN** only the entry matching {vpn=0x1000, asid=0x5} MUST have `valid` set to false; other entries unchanged

#### Scenario: Invalidate by asid (SFENCE.VMA without vaddr)
- **WHEN** `TLB::invalidate_asid(asid=0x5)` is called
- **THEN** all entries with `asid=0x5` MUST have `valid=false`; entries with other asids unchanged

#### Scenario: Invalidate all (SFENCE.VMA without args)
- **WHEN** `TLB::invalidate_all()` is called
- **THEN** all entries MUST have `valid=false`

### Requirement: TLB entry storage MUST be HDL-friendly

The TLB MUST store entries in `std::array<Entry, ENTRIES>` (contiguous memory, fixed compile-time size). MUST NOT use `std::optional`, `std::variant`, `std::vector`, or heap allocation for entry storage. Verified by existing CI gate `tools/check_plugin_portability.sh` (already PR-blocking per commit f0c15fd).

#### Scenario: Compile-time array size
- **WHEN** `TLB<64, 4, 40, 9, 1>` is instantiated
- **THEN** `sizeof(TLB<...>) == sizeof(std::array<Entry, 64>) + small_overhead` (no heap allocation)

#### Scenario: static_assert locks compile-time constants
- **WHEN** source code attempts `TLB<65, 3, ...>` (ENTRIES=65 not divisible by WAYS=3)
- **THEN** compilation MUST fail with `static_assert(ENTRIES % WAYS == 0, ...)`

#### Scenario: Banned types trigger CI gate failure
- **WHEN** source code adds `#include <optional>` in `ip/mmu/lib/tlb.cpp`
- **THEN** `bash tools/check_plugin_portability.sh` MUST report the violation

### Requirement: tlb.h const-correctness MUST be repaired (mutable keyword)

The `tlb.h` file currently has a compile error: `lookup()` is declared `const` but modifies `hits_`/`misses_`/`evicts_` counters. This change MUST fix the error by declaring these three fields `mutable`.

#### Scenario: Compile error pre-fix
- **WHEN** `git log -1 --format="%s" f217ae0` shows mmu-ip-skeleton commit (current state)
- **THEN** `grep -E '^[[:space:]]*uint64_t (hits_|misses_|evicts_)' ip/mmu/lib/tlb.h` returns 3 lines WITHOUT `mutable` (compile error)

#### Scenario: Compile error post-fix
- **WHEN** this change is committed
- **THEN** `grep -E '^[[:space:]]*mutable uint64_t (hits_|misses_|evicts_)' ip/mmu/lib/tlb.h` returns 3 lines (fixed)

#### Scenario: Tests compile after fix
- **WHEN** `tests/CMakeLists.txt` `list(REMOVE_ITEM)` is removed (this change's commit 2)
- **THEN** `cmake --build build` compiles `tests/mmu/test_tlb_unit.cpp` without const-correctness error
