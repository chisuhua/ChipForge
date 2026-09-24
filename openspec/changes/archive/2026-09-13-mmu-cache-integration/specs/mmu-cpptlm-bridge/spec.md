## MODIFIED Requirements

### Requirement: MMUTLMBridgeAdapter MUST register with cpptlm ModuleFactory via ChStreamAdapterFactory

The `MMUTLMBridgeAdapter` MUST register with `cpptlm::ChStreamAdapterFactory` (NOT `StreamAdapterFactory` — that name does not exist) for instantiation from JSON configs, mirroring `L1CacheTLMBridgeAdapter` structure. **NEW CONSTRAINT (added by mmu-cache-integration)**: the adapter MUST be an independent class, NOT a unified `MMUCacheBridgeAdapter` that wraps both cache and MMU Bridges — isolation is the design priority.

#### Scenario: Adapter registration uses ChStreamAdapterFactory
- **WHEN** `cpptlm::ChStreamAdapterFactory::registerAdapter<MMUTLMBridgeAdapter, ::bundles::TlbReqBundle, ::bundles::TlbRespBundle>()` is called
- **THEN** `ChStreamAdapterFactory` MUST recognize "MMUTLMBridgeAdapter" as a valid adapter type for JSON instantiation

#### Scenario: JSON instantiateAll recognizes adapter
- **WHEN** JSON config has `{"type": "MMUTLMBridge", ...}` and `cpptlm::instantiateAll(config)` is called
- **THEN** MMUTLMBridgeAdapter MUST be instantiated and `tick()` MUST propagate vaddr to MMUPlugin

#### Scenario: Adapter is independent class (NEW)
- **WHEN** `src/cf_plugin/bridge/mmu_bridge_adapter.h` declares the adapter class
- **THEN** it MUST declare exactly ONE underlying Bridge (`MMUTLMBridge`) as member, not multiple Bridges (NOT `MMUCacheBridgeAdapter` with both `L1CacheTLMBridge` and `MMUTLMBridge` members)

### Requirement: SoC JSON topology sample MUST integrate MMUTLMBridge

The change MUST include a sample SoC JSON topology demonstrating MMUTLMBridge integration (`soc/mmu_minimal.json`), enabling users to instantiate and run a minimal MMU configuration.

#### Scenario: Sample SoC JSON validates with full chain (4 modules)
- **WHEN** `cpptlm::instantiateAll(soc/mmu_minimal.json)` is called
- **THEN** all 4 modules (tg, mmu, l1, mem) MUST instantiate without error; 3 connections (tg→mmu, mmu→l1, l1→mem) MUST resolve

#### Scenario: Sample SoC JSON path is soc/mmu_minimal.json (NOT soc/cpu/configs/)
- **WHEN** the repository tree is searched for `mmu_minimal.json`
- **THEN** exactly one file MUST match at `soc/mmu_minimal.json`; the path `soc/cpu/configs/mmu_minimal.json` MUST NOT exist (that directory does not exist in the repo)

#### Scenario: Sample SoC JSON structural validation
- **WHEN** `tests/soc/test_mmu_minimal_json.cpp` runs structural validation (4 cases: top-level fields, module count + required params, connection count + src/dst, params schema strict validation)
- **THEN** all 4 test cases SHALL PASS; full instantiation-time simulation (MMUPlugin dual-write → L1CachePlugin VIPT path end-to-end) is **deferred** to a follow-up change requiring traffic_gen with retry support — this change verifies only structural correctness and compile-time invariants
