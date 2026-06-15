# SoC 配置说明

> **本目录文件作用**:机器可读的 SoC 集成清单,描述一个 SoC 由哪些 IP、按什么参数、什么地址映射组成。
> **本目录文档位置**: [`docs/`](docs/) 子目录,说明如何读、改、创建这些 .json 文件。

## 速查

| 文件 | 用途 | 关键 IP | 仿真用途 |
| --- | --- | --- | --- |
| `riscv_virt.json` | 基线 RISC-V 虚拟平台 | `cpu` + `cache` + `memory` + `peripheral` | 通用 RTL/TLM 仿真入口 |
| `l1_cache_minimal.json` | 最小 L1 cache SoC | 1×`cpu` + 1×`cache`(L1) | 单元级 cache 行为验证 |
| `l1_cache_adapter_e2e.json` | L1 cache 端到端适配 | 1×`cpu` + 1×`cache` 适配层 | ACT4 DUT 集成测试 |

## 命名约定

`<feature>_<variant>[_e2e].json`

- **`<feature>`**: 该 SoC 突出验证的功能 (例如 `l1_cache`)
- **`<variant>`**: 子变种 (`minimal` / `full` / `e2e` / 其它)
- **`_e2e`**: 表明端到端场景,常含适配层
