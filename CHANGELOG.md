# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Phase 1.2: `L1CachePlugin` (Plugin-style first IP, lookup + refill two-stage pipeline)
  - `ip/cache/tlm/L1CachePlugin.h/.cpp` (256 sets, 64B line, direct-mapped)
  - `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp` (4 tests: miss / refill / hit-after-refill / D4 runtime)
  - 10/10 ChipForge ctest PASS in 2.30s; D4 verify_plugin_decision 3/3 PASS
  - All Bundle fields `cf::plugin::uint_t<N>`; no `tick()`, no state machine; at_stage-driven
  - Phase 0 limitation: `uint_t<512>` falls back to `uint64_t` (tracked; Phase 6 upgrade planned)

### Notes
- Phase 1.1 Bundle definitions shipped in `073402c` (Bundles 6 types + 9/9 unit tests)
- Phase 0 LSP false positives remain (cf/plugin namespace visibility); tracked, not blocking

## [0.0.1] - 2026-06-10

### Added
- Phase 0 Plugin scaffolding framework (`cf::plugin` namespace, 6 headers in `include/cf/plugin/`)
  - `PluginBase` (lifecycle interface)
  - `Payload<T>` (type-safe key for cross-stage IPC)
  - `PipeNode` (dataflow node)
  - `PipeBuilder` (orchestrator)
  - `CtrlLink` (4-control API: halt_when / throw_when / flush_when / bypass)
  - `uint_t<N>` (compile-time TLM/RTL switch)
- `cf_plugin` INTERFACE library (CMake target)
- 7 cf_plugin unit tests in `src/cf_plugin/tests/` (8/8 ctest PASS in 2.34s)
- CppTLM v2.1.0 integration (TLM framework, `cpptlm_core` target)
- CppHDL v1.0.0 integration (HDL framework, `cpphdl` target, JIT disabled)
- `tools/verify_plugin_decision.sh` (D4 static check, 3/3 PASS)
- `tools/run_chipforge_tests.sh` (wrapper for ChipForge-only ctest run)

### Fixed
- ctest exit code: 139 "Not Run" CppHDL internal tests excluded by `tools/run_chipforge_tests.sh` (PA-4b)
- Removed 4 stray experimental files (Scala/Chisel + params.py) backed up in `git stash@{0}`

### Known Issues
- CppHDL internal unit tests (~139) fail to build in parent project's C++17 mode; tracked as PA-4b
  - Workaround: `tools/run_chipforge_tests.sh` excludes them by name pattern
  - Permanent fix: Phase 5 (RTL co-simulation) will re-evaluate CppHDL integration
- Application layer (`bundles/` populated in 1.1; `ip/cache/tlm/L1CachePlugin.{h,cpp}` populated in 1.2; remaining dirs pending)

### Status
- Phase 0: ✅ Completed (5/5 P0 components, 8/8 ChipForge tests PASS)
- Phase 1: 🚧 In Progress, 1.1 ✅ + 1.2 ✅ + 1.3-1.5 pending (10/10 ChipForge tests PASS)
- Phase 2-5: Not Started (depends on Phase 1)
- Phase 6: Not Started (depends on 2-3 stable Plugin-style IPs)