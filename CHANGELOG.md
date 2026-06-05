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