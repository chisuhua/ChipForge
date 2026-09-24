# mmu-cpptlm-bridge Specification

## Purpose
TBD - created by archiving change mmu-tlb-ptw-impl. Update Purpose after archive.
## Requirements
### Requirement: MMUTLMBridge MUST expose Plugin-style test API

The `MMUTLMBridge` MUST expose test API that delegates to `MMUPlugin::issue_request()` / `read_response()`, mirroring `L1CacheTLMBridge` structure. The bridge MUST call `pb_.run()` at the end of `tick()` per D1' contract.

#### Scenario: Bridge issue_request delegates to MMUPlugin
- **WHEN** `MMUTLMBridge::issue_request({transaction_id=0x1, vaddr=0x1000, asid=0x5, access_type=LOAD})` is called
- **THEN** `MMUPlugin::issue_request()` MUST be called with the same `vaddr`/`asid`; bridge converts `TlbReqBundle` ch_uint fields (`.vaddr.read()/.asid.read()`) to `cf::bundles::TlbReq` POD and stores it in `mmu_keys<T>::VADDR` payload key + `last_vaddr_/current_asid_` internal state (replacing the stub implementation in commit 9b)

#### Scenario: Bridge read_response returns MMUPlugin response
- **WHEN** `MMUTLMBridge::read_response()` is called after `tick()`
- **THEN** `MMUPlugin::read_response()` MUST be called (replacing the stub `return {};` in commit 9b); bridge converts plugin's `TlbResp` POD back to `TlbRespBundle` ch_uint fields and returns it

#### Scenario: Bridge tick calls pb.run()
- **WHEN** `MMUTLMBridge::tick()` is called
- **THEN** at end of tick, `pb_.run()` MUST be called (D1' contract); `pb_run_count()` increments by 1 (unchanged from commit 9b spec)

#### Scenario: MMUPlugin::issue_request feeds at_stage closures
- **WHEN** `MMUPlugin::issue_request(node, req)` is called with `req.vaddr = 0x1000`
- **THEN** `mmu_keys<T>::VADDR` SHALL be written to `node` with value 0x1000; `last_vaddr_` internal state SHALL be set to 0x1000; subsequent `pb.run()` SHALL consume this vaddr in `at_stage("tlb_lookup_*")` closures (replacing the `last_vaddr_ = 0` initial value issue that made Bridge end-to-end non-functional)

#### Scenario: MMUTLMBridge converts TlbReqBundle to TlbReq POD
- **WHEN** `MMUTLMBridgeAdapter::tick()` calls `bridge_->issue_request(pod_req)` with `::bundles::TlbReqBundle` (ch_uint<64> vaddr, ch_uint<16> asid)
- **THEN** bridge SHALL convert `pod_req.vaddr.read()` → `cf::bundles::TlbReq.vaddr`, `pod_req.asid.read()` → `cf::bundles::TlbReq.asid`, and call `plugin_->issue_request(lookup_node_, cf_tlb_req)` (mirror L1CacheTLMBridge POD-to-Bundle inversion at l1_cache_bridge.cpp:62-65)

### Requirement: MMUTLMBridgeAdapter MUST register with cpptlm ModuleFactory via ChStreamAdapterFactory

The `MMUTLMBridgeAdapter` MUST register with `cpptlm::ChStreamAdapterFactory` (NOT `StreamAdapterFactory` — that name does not exist) for instantiation from JSON configs, mirroring `L1CacheTLMBridgeAdapter` structure. **UPDATED (mmu-cache-integration commit 4)**: adapter is independent class — see archived spec.

#### Scenario: Adapter registration uses ChStreamAdapterFactory
- **WHEN** `cpptlm::ChStreamAdapterFactory::registerAdapter<MMUTLMBridgeAdapter, ::bundles::TlbReqBundle, ::bundles::TlbRespBundle>()` is called
- **THEN** `ChStreamAdapterFactory` MUST recognize "MMUTLMBridgeAdapter" as a valid adapter type for JSON instantiation

#### Scenario: JSON instantiateAll recognizes adapter
- **WHEN** JSON config has `{"type": "MMUTLMBridge", ...}` and `cpptlm::instantiateAll(config)` is called
- **THEN** MMUTLMBridgeAdapter MUST be instantiated and `tick()` MUST propagate vaddr to MMUPlugin (no longer a stub as of this change)

#### Scenario: Adapter is independent class
- **WHEN** `src/cf_plugin/bridge/mmu_bridge_adapter.h` declares the adapter class
- **THEN** it MUST declare exactly ONE underlying Bridge (`MMUTLMBridge`) as member, not multiple Bridges (NOT `MMUCacheBridgeAdapter` with both `L1CacheTLMBridge` and `MMUTLMBridge` members)

### Requirement: MMUTLMBridgeAdapter MUST register ch_stream 4-field narrow bridge

The `MMUTLMBridgeAdapter` MUST register a `ch_stream` 4-field narrow bridge (`req_out`, `req_in`, `resp_out`, `resp_in`) using `cpptlm::ChStreamAdapterFactory`, mirroring `L1CacheTLMBridgeAdapter` pattern.

#### Scenario: Adapter sets up 4 stream ports
- **WHEN** MMUTLMBridgeAdapter is constructed with TlbReqBundle/TlbRespBundle types
- **THEN** 4 `ch_stream` ports (req_out/req_in/resp_out/resp_in) MUST be registered with cpptlm

#### Scenario: Adapter ticks bridge
- **WHEN** ch_stream receives a request on req_in port and Adapter tick fires
- **THEN** request MUST propagate to MMUPlugin::issue_request; response MUST emit on resp_out port

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

### Requirement: HDL-friendly constraint enforced via existing CI gates

Bridge implementation MUST preserve HDL 1:1 mapping. No `std::optional`, no `std::variant`, no `virtual`, no dynamic allocation. Verified by existing CI gate `tools/check_plugin_portability.sh`.

#### Scenario: Bridge HDL constraint enforced
- **WHEN** `bash tools/check_plugin_portability.sh` inspects `src/cf_plugin/bridge/mmu_bridge.cpp` and `mmu_bridge_adapter.cpp` after this change
- **THEN** script MUST report no `std::optional`, no `ch_mem`/`ch_reg`/`ch_uint` leakage, no early-return pattern

