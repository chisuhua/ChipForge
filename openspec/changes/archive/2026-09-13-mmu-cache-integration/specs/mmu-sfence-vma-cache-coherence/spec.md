## ADDED Requirements

### Requirement: SFENCE.VMA invalidate TLB → L1Cache tag natural miss

When `RiscvMMUPlugin::at_stage("sfence_vma")` invalidates TLB entries (via `invalidate_vaddr`, `invalidate_asid`, or `invalidate_all`), the L1Cache tags for affected physical addresses MUST remain valid (no need to flush cache). The cache lookup with new paddr (after TLB refill on next access) will have a different tag, naturally missing the old tag entry.

#### Scenario: SFENCE.VMA rs1=valid triggers vaddr invalidate
- **WHEN** `RiscvMMUPlugin::at_stage("sfence_vma")` runs with rs1=vaddr (non-zero) and rs2=asid
- **THEN** `multi_level_tlb_->invalidate_vaddr(vaddr, asid)` SHALL be called, removing the matching TLB entry across all levels

#### Scenario: SFENCE.VMA rs1=x0 (zero) triggers all invalidate
- **WHEN** `RiscvMMUPlugin::at_stage("sfence_vma")` runs with rs1=x0 (RISC-V x0 register convention for "all vaddrs")
- **THEN** `multi_level_tlb_->invalidate_all()` SHALL be called

### Requirement: Boundary test SHALL verify SFENCE.VMA → cache natural miss

A regression test SHALL verify the SFENCE.VMA → cache tag miss semantic: after invalidating TLB entry, the next access with same vaddr re-translates via PTW, gets a (possibly different) paddr, and cache lookup with new paddr tag misses the old entry.

#### Scenario: SFENCE.VMA invalidates TLB entry
- **WHEN** test constructs MMUPlugin with Sv39 + 2-level TLB, inserts entry `vaddr=0x4000_0000 → paddr=0x8000_0000`, then calls `mmu.sfence_vma(0x4000_0000, 0)`
- **THEN** `mmu.lookup(0x4000_0000, 0)` SHALL miss (TLB entry invalidated)
