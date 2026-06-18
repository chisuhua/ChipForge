# STATUS: PLANNED (0 LOC)

This IP is **planned but not yet implemented**. No source code exists.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PLANNED variant) -->

## Implementation Roadmap
- 实施预计: Phase 2+ (见 docs/roadmap/phases/phase-2-baremetal.md)
- 依赖: 无直接依赖；为 cpu/cache/peripheral 提供总线/NoC
- 状态: 🔴 规划中

## 已知限制
- 所有子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- 当前 IP 根仅含 `README.md` (无任何源码)
- Phase 1.4 之前不会有任何实施
- 与 CppTLM 内置的 `CrossbarTLM` (4 端口) 是不同抽象层级；本 IP 将提供完整 AXI/TileLink 协议
