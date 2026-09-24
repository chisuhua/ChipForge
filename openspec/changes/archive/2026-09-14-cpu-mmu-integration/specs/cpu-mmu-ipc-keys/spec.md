## ADDED Requirements

### Requirement: CPU MUST write SAT/SFENCE_VADDR/SFENCE_ASID Payload Keys for MMU hook consumption

When CPU CPU pipeline writes to the `sat` CSR or executes SFENCE.VMA, it MUST write the corresponding Payload Key so that `RiscvMMUPlugin::at_stage("csr_write_satp")` / `at_stage("sfence_vma")` closures can read it. The Keys MUST live in `ip/cpu/tlm/cpu_keys.h` (NOT in `ip/mmu/tlm/mmu_keys.h`) to avoid mmu reverse-dependency on CPU.

#### Scenario: CPU writes SAT payload on CSR write
- **WHEN** CPU executes `csrrw sat, ...` instruction in `at_stage("execute")` closure
- **THEN** `(*lookup_node)(cpu_keys::SAT)` MUST be written with the new satp value (u64_t with low 4 bits = MODE, high bits = PPN)

#### Scenario: CPU writes SFENCE_VADDR/SFENCE_ASID on SFENCE.VMA instruction
- **WHEN** CPU executes SFENCE.VMA instruction in `at_stage("execute")` closure
- **THEN** `(*lookup_node)(cpu_keys::SFENCE_VADDR)` MUST be written with rs1 value (or 0 for x0 register); `(*lookup_node)(cpu_keys::SFENCE_ASID)` MUST be written with rs2 value (or 0 for x0 register)

#### Scenario: cpu_keys.h avoids mmu reverse-dependency
- **WHEN** `ip/cpu/tlm/cpu_keys.h` is inspected
- **THEN** it MUST NOT `#include` any header from `ip/mmu/` (no reverse-dependency); the MMU-side closures read these Keys via the `mmu_keys` namespace, not via mmu's own definitions

### Requirement: MMU MUST NOT depend on CPU Keys (unidirectional dependency)

The `ip/mmu/` subtree MUST NOT include any header from `ip/cpu/` or `ip/cpu/tlm/`. The IPC Keys for CPU→MMU communication are owned by CPU; MMU reads them at at_stage closure time via the global Payload Key identity match (not via includes).

#### Scenario: ip/mmu/ has no CPU includes
- **WHEN** `find ip/mmu -name "*.h" -exec grep -l "ip/cpu" {} \;` is run
- **THEN** zero matches SHALL be returned (MMU does not depend on CPU headers)