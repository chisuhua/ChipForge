# ChipForge

> CppTLM + CppHDL-based RISC-V Virtual Prototyping Platform

## Status

Active development. See [docs/architecture/overview.md](docs/architecture/overview.md) for the current architecture and roadmap.

## Quick Links

- [Architecture Overview](docs/architecture/overview.md)
- [Development Setup](docs/DEVELOPMENT_SETUP.md)
- [Contributing](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)
- [Glossary](docs/GLOSSARY.md)
- [IP Library](ip/README.md)

## Project Structure

```
.
├── docs/         # Design notes, setup, glossary, architecture
├── ip/           # Reusable IP library; pulls in CppTLM and CppHDL
├── soc/          # SoC integration, configs, and platform builds
└── tools/        # Build scripts, helpers, and CI glue
```

## 1. Getting Started

1. Clone the repo, including submodules.
2. Follow [DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md) to install dependencies and configure the CMake build.
3. Read [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow, coding style, and review expectations.

## 2. How It Works

ChipForge builds a RISC-V virtual prototype by composing reusable IP blocks. CppTLM models bus and memory traffic at the transaction level. CppHDL describes the synthesizable hardware. Both layers share a single C++17/20 codebase, which keeps simulation fast and refactors cheap.

The [Glossary](docs/GLOSSARY.md) covers the project-specific vocabulary. Newcomers should skim it before diving in.

## 3. External Dependencies

The build expects CppTLM and CppHDL to be present. They are typically pulled in as siblings or submodules and symlinked into [ip/](ip/). See [DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md) for the exact layout.

## 4. Building an SoC

SoC configs live under [soc/](soc/). A typical config, like [soc/riscv_virt.json](soc/riscv_virt.json), lists the IPs to instantiate, the memory map, and the platform build flags. The CMake build reads these and produces an executable for the chosen target.

## 5. Contributing

Pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening one. Small, focused changes are easier to review and merge. Add a [CHANGELOG.md](CHANGELOG.md) entry for any user-visible change.

## 6. Release Notes

See [CHANGELOG.md](CHANGELOG.md) for the history of releases, fixes, and breaking changes.

## 7. Security

Report vulnerabilities per [SECURITY.md](SECURITY.md). Do not file public issues for security bugs.

## 8. License

See [LICENSE](LICENSE).
