# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Architecture documentation (8 docs in `docs/architecture/`)
- Roadmap with 5 phases and 10 milestones (`docs/roadmap/`)
- IP library stubs: cpu, cache, memory, interconnect, peripheral
- CPU IP design documents (multi_isa_architecture.md, 1164 lines)
- JSON Schema for CPU configuration (`ip/cpu/configs/cpu_params_schema.json`)
- Documentation health check tool (`tools/doc_checker.py`)
- 3 regression tests for the doc_checker tool
- GitHub Actions CI for documentation health
- Root README, CONTRIBUTING, this CHANGELOG, CODEOWNERS

### Notes
- Project is in pure planning/scaffolding stage
- All 5 Phases are Not Started
- No source code committed yet (CppTLM/CppHDL as symlinks)

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
- Application layer (`bundles/`, `ip/*/tlm/`, `ip/*/rtl/`) is empty; Phase 1 will populate

### Status
- Phase 0: ✅ Completed (5/5 P0 components, 8/8 ChipForge tests PASS)
- Phase 1-5: Not Started (depends on Phase 1)
- Phase 6: Not Started (depends on 2-3 stable Plugin-style IPs)