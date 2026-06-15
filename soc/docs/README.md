# `soc/docs/` 文档索引

> 本目录收录 SoC 集成**面向人**的文档。
> 机器读的 SoC 清单在 `soc/*.json`。

| 文档 | 内容 |
| --- | --- |
| [`integration-guide.md`](integration-guide.md) | 如何读、改、创建 SoC `.json` 配置; 字段语义; 常见错误 |

## 文档维护约定

- **新增内容前先看这里**:确认不属于 `ip/<name>/docs/` 的范围 (那是单 IP 视角),也不属于 `docs/architecture/` 的范围 (那是项目级)
- **添加文档时同步更新本 README 索引**
- **新 SoC `.json` 配置添加时**, 在 `soc/README.md` 速查表加一行, 而非在此
