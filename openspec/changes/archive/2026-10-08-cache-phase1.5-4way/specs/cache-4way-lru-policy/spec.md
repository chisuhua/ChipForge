## ADDED Requirements

### Requirement: L1Cache SHALL support 4-way set-associative organization with LRU replacement

The `L1CachePlugin` SHALL be upgraded from 256 sets × 1 way (direct-mapped) to 64 sets × 4 ways (set-associative) with LRU replacement policy. The storage layout SHALL be:

```cpp
static constexpr size_t kNumSets = 64;
static constexpr size_t kWays = 4;
array_store<ch_mem<cf::plugin::uint_t<kTagBits>, kNumSets>, kWays> tags_;
array_store<ch_mem<cf::plugin::uint_t<kLineDataBits>, kNumSets>, kWays> data_;
array_store<ch_mem<cf::plugin::bool_t, kNumSets>, kWays> valid_;
array_store<cf::plugin::uint_t<8>, kNumSets> lru_counter_[kWays];  // per-set LRU counter (0=MRU)
```

The `LRUPolicy4Way` SHALL select the LRU way (highest counter value) as victim on cache miss.

#### Scenario: Lookup hits in way 0 and returns data
- **WHEN** L1Cache receives `CacheReq{address = paddr}` matching tag in set 5, way 0 (with `valid_[5][0] = true`)
- **THEN** L1Cache SHALL respond with `CacheResp{data = data_[5][0], hit = true}`
- **AND** `lru_counter_[5][0]` SHALL be decremented to indicate recent use (MRU position)

#### Scenario: Cache miss selects LRU way as victim
- **WHEN** L1Cache receives `CacheReq{address = paddr}` that misses in all 4 ways of the target set
- **THEN** the victim way SHALL be the one with the highest `lru_counter_[set][way]` value (LRU position)
- **AND** the new tag SHALL be written to that way
- **AND** the new data SHALL be fetched from main memory and written to `data_[set][victim_way]`

### Requirement: VIPT aliasing safety SHALL be verified under 4-way set-associative organization

Under 4-way set-associative organization with `kIdxBits=6` and `kOffsetBits=6` (8-bit index, 6-bit line offset, total 14 bits used for VIPT indexing), VIPT safety SHALL be verified against the 4KB page offset (12 bits). With 4-way organization, the index bits (8) exceed page offset bits (12 - 6 = 6), so aliasing IS theoretically possible, but the 4-way associativity SHALL prevent cache line corruption by storing aliases in separate ways of the same set.

#### Scenario: Two virtual aliases map to same set but different physical addresses
- **WHEN** vaddr `0x80000000` and vaddr `0x80100000` both map to cache set 0 (because their indices `[13:6]` are equal) but to different physical addresses
- **AND** L1Cache receives both addresses in sequence (first 0x80000000, then 0x80100000)
- **THEN** the first address SHALL be cached in set 0, way 0 (with valid bit set)
- **AND** the second address SHALL be cached in set 0, way 1 (NOT evict way 0, because 4-way associativity allows both)
- **AND** a subsequent fetch of vaddr 0x80000000 SHALL hit in set 0, way 0 (the first cache line is not corrupted)

#### Scenario: Five aliases force eviction in 4-way cache
- **WHEN** five virtual aliases all map to cache set 0 with different physical addresses
- **AND** L1Cache receives all five in sequence
- **THEN** the first four SHALL occupy set 0, ways 0-3
- **AND** the fifth SHALL evict the LRU way (highest lru_counter_[0][way]) and occupy that way
- **AND** the eviction SHALL be governed by LRU policy (the most recently used way stays)