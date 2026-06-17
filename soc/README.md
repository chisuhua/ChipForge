# SoC 配置说明

> **当前状态 (Phase 1.3, 2026-06-17)**: 本目录目前有 **2 个 L1Cache 验证配置**。RISC-V virt SoC 配置 (`soc/riscv_virt.json`) 已在 2026-06-17 移除 (详见 [CHANGELOG](../CHANGELOG.md) v0.0.2)，RISC-V virt 完整装配推迟到 **Phase 2+** 实施。
>
> **本目录文件作用**:机器可读的 SoC 集成清单,描述一个 SoC 由哪些 IP、按什么参数、什么地址映射组成。
> **本目录文档位置**: [`docs/`](../docs/) 子目录,说明如何读、改、创建这些 .json 文件。

## 速查

| 文件 | 用途 | 关键 IP | 仿真用途 | 状态 |
| --- | --- | --- | --- | --- |
| `l1_cache_minimal.json` | 最小 L1 cache SoC | 1×L1Cache Plugin + 1×TrafficGen | 单元级 cache 行为验证 | ✅ 可用 |
| `l1_cache_adapter_e2e.json` | L1 cache 端到端适配 | 1×L1Cache Plugin + CppTLM CacheTLM 适配层 | ACT4 DUT 集成测试 | ✅ 可用 |
| ~~`riscv_virt.json`~~ | ~~基线 RISC-V 虚拟平台~~ | ~~`cpu` + `cache` + `memory` + `peripheral`~~ | ~~通用 RTL/TLM 仿真入口~~ | ❌ **已删除** (2026-06-17, 引用 7 个不存在的 IP 类 + `impl_mode` 字段无消费者) |

## 命名约定

`<feature>_<variant>[_e2e].json`

- **`<feature>`**: 该 SoC 突出验证的功能 (例如 `l1_cache`)
- **`<variant>`**: 子变种 (`minimal` / `full` / `e2e` / 其它)
- **`_e2e`**: 表明端到端场景,常含适配层

## Phase 2+ 待实施

完整的 RISC-V virt SoC（CPU + 多级 Cache + 总线 + 内存 + 外设）将在 Phase 2 重新装配，目标 JSON 将基于已注册的标准 CppTLM 模块（`CPUTLM` / `CacheTLM` / `MemoryTLM` / `CrossbarTLM`），并移除不存在的 `impl_mode` 字段。详见 [docs/architecture/overview.md §"SoC 层是 IP 组合器"](../docs/architecture/overview.md)。
