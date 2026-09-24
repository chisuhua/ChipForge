## ADDED Requirements

### Requirement: MultiLevelTLB MUST perform shallow-to-deep parallel lookup, return deepest hit

The `MultiLevelTLB` MUST perform lookup by querying all levels (L0, L1, ..., Ln-1) in parallel within a single cycle, returning **always the deepest** hit when multiple levels match (deepest = largest level index). Deepest hit is preferred for replacement stability and freshness.

#### Scenario: Hit on deepest level (Ln-1) only
- **WHEN** lookup(vaddr=0x1000, asid=0x5) and entry exists in Ln-1 only
- **THEN** lookup MUST return Ln-1's entry as result

#### Scenario: Hit on shallow level (L0) only
- **WHEN** lookup(vaddr=0x1000, asid=0x5) and entry exists in L0 only
- **THEN** lookup MUST return L0's entry; PTW not triggered

#### Scenario: Hit on multiple levels (deepest wins)
- **WHEN** lookup(vaddr=0x1000, asid=0x5) and entry exists in BOTH L0 (stale) AND L2 (fresh)
- **THEN** lookup MUST return L2's entry (deepest); AND L1 MUST be filled from L2's entry via shadow fill; L0 entry remains stale until next invalidate

### Requirement: MultiLevelTLB MUST perform shadow fill on deeper hit

The `MultiLevelTLB` MUST implement shadow fill: when a deeper level (Ln-k, k>0) hits during lookup, the matching entry MUST be inserted into all shallower levels (Ln-k-1, ..., L0).

#### Scenario: Shadow fill from L2 to L1+L0
- **WHEN** lookup hits in L2 with entry {vpn=0x1000, asid=0x5, paddr=0x1000_0000, perms=R|W|U}
- **THEN** L1 MUST receive new entry {vpn=0x1000, asid=0x5, paddr=0x1000_0000, perms=R|W|U} and L0 MUST receive same entry

#### Scenario: Shadow fill respects policy
- **WHEN** L0 is full and shadow fill triggers insertion
- **THEN** L0's replacement policy MUST select victim and evict; shadow fill MUST succeed

#### Scenario: Shadow fill idempotent
- **WHEN** lookup hits in L2, shadow fills L1 and L0, then lookup hits again immediately
- **THEN** L0 hit returns same paddr (no redundant walks); no eviction triggered

### Requirement: MultiLevelTLB MUST reverse-invalidate shallow levels on deep evict

When a deep level (Ln-k, k>0) evicts an entry, all shallower levels (Ln-k-1, ..., L0) MUST have their matching entry (same vaddr + asid) invalidated to prevent stale hits.

#### Scenario: Deep L2 evict invalidates L0 and L1
- **WHEN** L2 evicts entry {vpn=0x1000, asid=0x5} via LRU policy and L0+L1 have matching entries
- **THEN** L0's matching entry MUST be invalidated (valid=false); L1's matching entry MUST be invalidated

#### Scenario: Deep L2 evict with no shallow matches is no-op
- **WHEN** L2 evicts entry {vpn=0x2000, asid=0x7} and L0+L1 have no matching entries
- **THEN** reverse-invalidate MUST be no-op (no spurious invalidations)

### Requirement: MultiLevelTLB MUST support ASID/process switch invalidation

The `MultiLevelTLB` MUST support invalidate_asid(asid) and invalidate_all() operations that affect all levels coherently.

#### Scenario: invalidate_asid invalidates all levels
- **WHEN** `MultiLevelTLB::invalidate_asid(asid=0x5)` is called
- **THEN** all entries with `asid=0x5` in L0, L1, ..., Ln-1 MUST be invalidated

#### Scenario: invalidate_all clears everything
- **WHEN** `MultiLevelTLB::invalidate_all()` is called
- **THEN** all entries in L0, L1, ..., Ln-1 MUST have valid=false

#### Scenario: SFENCE.VMA maps to MultiLevelTLB ops
- **WHEN** `RiscvMMUPlugin::at_stage("sfence_vma")` is called with rs2=vaddr, rs1=asid
- **THEN** `MultiLevelTLB::invalidate_vaddr(vaddr, asid)` MUST be invoked
