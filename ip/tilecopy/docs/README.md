# TileCopy IP 文档索引

> 本目录收录 **TileCopy IP 自身** 的设计文档。
> 跨 IP/SoC 视角的内容见 [`docs/architecture/`](../../../docs/architecture/) 和 [`soc/docs/`](../../../soc/docs/)。

## 文档清单

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [`architecture.md`](architecture.md) | 微架构、接口、时序、与 `tilecore` 的协作模型 | **Active (v1.0, 2026-06-15)** |

## 推荐子结构 (本目录应最终包含)

| 文件 | 作用 | 必备 |
| --- | --- | --- |
| `README.md` | 本文件:文档索引 + 状态标签 | ✓ |
| `architecture.md` | 微架构 / 接口 / 时序 | ✓ |
| `configuration.md` | 可调参数 (Knobs) 及默认值,对应 `configs/params_schema.json` | 推荐 |
| `integration.md` | 如何被 `tilecore` / SoC 集成 (总线挂点、地址映射、依赖 IP) | 可选 |

## 相关文档

- [项目架构总览 - IP 目录](../../../docs/architecture/ip-catalog.md)
- [TileCore IP 文档](../tilecore/docs/README.md) — TileCopy 的主要调用方
- [SoC 集成指南](../../../soc/docs/integration-guide.md) — 如何在 SoC 配置中引用此 IP
