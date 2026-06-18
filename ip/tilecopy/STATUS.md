# STATUS: INITIAL DESIGN (0 LOC src, design docs exist)

This IP has **architectural documentation** but no source code yet.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (INITIAL DESIGN variant) -->

## Existing Assets
- 设计文档: `docs/architecture.md` (5.8 KB, 微架构 + 接口)
- 政策框架: `policies/` (空目录, 待实施, 见 `ip/README.md` 标准子结构)

## Implementation Roadmap
- 实施预计: Phase 5+ (见 docs/roadmap/phases/phase-5-rtl.md, GPU 形态)
- 依赖: `interconnect` (Phase 2+); `memory` (Phase 2+)
- 状态: 🟡 初始设计
- 角色: Tile 级异步数据搬运 (类 TMA / Tile 级 DMA)

## 已知限制
- 所有源码子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- 当前 IP 根仅含 `README.md` + `docs/architecture.md` (无任何源码)
- `policies/` 目录保留但为空 (设计文档已规划但未落地)
- `tilecore` 强依赖 `tilecopy`，两者需协同实施
