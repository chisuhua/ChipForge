# mmu-cache-bridge-adapter Specification

## Purpose
TBD - created by archiving change mmu-cache-integration. Update Purpose after archive.
## Requirements
### Requirement: MMUTLMBridgeAdapter SHALL register via cpptlm::ChStreamAdapterFactory

`MMUTLMBridgeAdapter` (in `src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`) MUST inherit `cpptlm::ChStreamModuleBase` and MUST register itself via `cpptlm::ChStreamAdapterFactory::get().registerAdapter<MMUTLMBridgeAdapter, ::bundles::TlbReqBundle, ::bundles::TlbRespBundle>(...)` (NOT `cpptlm::StreamAdapterFactory` — that name is incorrect).

#### Scenario: Adapter registration succeeds
- **WHEN** chipforge_tests binary loads and `MMUTLMBridgeAdapter::register_factory()` runs
- **THEN** `cpptlm::ChStreamAdapterFactory::get().getAdapter("MMUTLMBridgeAdapter")` SHALL return a valid adapter pointer

### Requirement: MMUTLMBridgeAdapter SHALL be independent from L1CacheTLMBridgeAdapter

`MMUTLMBridgeAdapter` MUST be a standalone class (NOT a unified `MMUCacheBridgeAdapter` that wraps both `L1CacheTLMBridge` and `MMUTLMBridge`). Isolation is the design priority — cache-side tests must be able to instantiate without MMU, and MMU-side tests must be able to instantiate without cache.

#### Scenario: MMUTLMBridgeAdapter is independent class
- **WHEN** `src/cf_plugin/bridge/mmu_bridge_adapter.h` declares the adapter class
- **THEN** it MUST declare exactly ONE underlying Bridge (`MMUTLMBridge`) as member, not multiple Bridges

### Requirement: MMUTLMBridgeAdapter SHALL expose 4-field narrow bridge

`MMUTLMBridgeAdapter` MUST expose `req_in()` / `req_out()` / `resp_in()` / `resp_out()` accessors using `ch_stream<::bundles::TlbReqBundle>` and `ch_stream<::bundles::TlbRespBundle>`. The `req_out()` and `resp_in()` MUST be dummy/narrow bridges (no real protocol conversion in this change — defer to SoC integration layer).

#### Scenario: Adapter exposes narrow bridge ports
- **WHEN** `MMUTLMBridgeAdapter` is instantiated and `bind_port_pair(other_adapter)` is called
- **THEN** `req_in().bind(req_out_of_other)` SHALL succeed via `ch_stream` protocol

