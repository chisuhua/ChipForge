## ADDED Requirements

### Requirement: MMUTLMBridge MUST expose Plugin-style test API

The `MMUTLMBridge` MUST expose test API that delegates to `MMUPlugin::issue_request()` / `read_response()`, mirroring `L1CacheTLMBridge` structure. The bridge MUST call `pb_.run()` at the end of `tick()` per D1' contract.

#### Scenario: Bridge issue_request delegates to MMUPlugin
- **WHEN** `MMUTLMBridge::issue_request({vaddr=0x1000, asid=0x5, access_type=LOAD})` is called
- **THEN** MMUPlugin::issue_request MUST be called with same args; bridge stores request in payload node_

#### Scenario: Bridge read_response returns MMUPlugin response
- **WHEN** `MMUTLMBridge::read_response()` is called after `tick()`
- **THEN** MMUPlugin::read_response MUST be called; bridge returns MMUPlugin's response {hit, paddr, perms, exception_code}

#### Scenario: Bridge tick calls pb.run()
- **WHEN** `MMUTLMBridge::tick()` is called
- **THEN** at end of tick, `pb_.run()` MUST be called (D1' contract); `pb_run_count()` increments by 1

### Requirement: MMUTLMBridgeAdapter MUST register with cpptlm ModuleFactory via ChStreamAdapterFactory

The `MMUTLMBridgeAdapter` MUST register with `cpptlm::ChStreamAdapterFactory` (NOT `StreamAdapterFactory` — that name does not exist) for instantiation from JSON configs, mirroring `L1CacheTLMBridgeAdapter` structure.

#### Scenario: Adapter registration uses ChStreamAdapterFactory
- **WHEN** `cpptlm::ChStreamAdapterFactory::registerAdapter<MMUTLMBridgeAdapter, ::bundles::TlbReqBundle, ::bundles::TlbRespBundle>()` is called
- **THEN** `ChStreamAdapterFactory` MUST recognize "MMUTLMBridgeAdapter" as a valid adapter type for JSON instantiation

#### Scenario: JSON instantiateAll recognizes adapter
- **WHEN** JSON config has `{"type": "MMUTLMBridge", ...}` and `cpptlm::instantiateAll(config)` is called
- **THEN** MMUTLMBridgeAdapter MUST be instantiated and `tick()` MUST propagate vaddr to MMUPlugin

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

#### Scenario: Sample SoC JSON validates
- **WHEN** `cpptlm::instantiateAll(soc/mmu_minimal.json)` is called
- **THEN** all modules instantiate without error; traffic_gen → mmu_bridge → memory topology resolves

#### Scenario: Sample SoC JSON runs a translation
- **WHEN** traffic_gen issues vaddr request and mmu_bridge translates
- **THEN** response paddr MUST be readable by traffic_gen (MMUPlugin end-to-end via stub memory)

### Requirement: HDL-friendly constraint enforced via existing CI gates

Bridge implementation MUST preserve HDL 1:1 mapping. No `std::optional`, no `std::variant`, no `virtual`, no dynamic allocation. Verified by existing CI gate `tools/check_plugin_portability.sh`.

#### Scenario: Bridge HDL constraint enforced
- **WHEN** `bash tools/check_plugin_portability.sh` inspects `src/cf_plugin/bridge/mmu_bridge.cpp` and `mmu_bridge_adapter.cpp` after this change
- **THEN** script MUST report no `std::optional`, no `ch_mem`/`ch_reg`/`ch_uint` leakage, no early-return pattern
