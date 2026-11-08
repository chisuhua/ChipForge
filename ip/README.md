# ChipForge IP 库

## STATUS 约定

每个 IP 根目录应有 `STATUS.md` 标明当前状态。零代码 IP 使用模板：
`docs/templates/IP_STATUS_TEMPLATE.md`（由 CHANGE-005 在 task 5.2 创建）。

| IP | 状态 | 实施预计 |
|----|------|----------|
| ★ **见 [`ip/ip-catalog.md`](ip-catalog.md)** | ★ **单一维护点** | ★ 状态权威在该文件 |

> ★ ★ **单一维护点声明（2026-11-08 · ADR-095 / change `architecture-truth-boundary`）**：
> ★ **IP 状态与目录的权威 = [`ip/ip-catalog.md`](ip-catalog.md)** ⇒ ★ 本文件**不保留副本**（★ 原状态表已移除，避免双维护漂移）。
> ★ 本文件只做「IP 库入口 + 统一目录结构说明」；★ 要查状态 / 全目录 ⇒ 去 `ip/ip-catalog.md`。

## 概述
本目录包含 ChipForge 平台的所有硬件 IP 核心，每个 IP 遵循统一的目录结构和接口规范。

## IP 模块列表

| IP | 状态 | 说明 |
|----|------|------|
| [cpu/](cpu/) | 🟡 设计中 | RISC-V CPU（Plugin + Stageable 架构） |
| [cache/](cache/) | 🟡 TLM 实现中 (Phase 1.2 L1D) | 多级缓存（L1I/L1D/L2） |
| [mmu/](mmu/) | 🟡 骨架阶段 | 多级 TLB + Page Table Walker（lib/tlm 分层） |
| [memory/](memory/) | 🔴 规划中 | 主存储器（SRAM/DRAM 模型） |
| [interconnect/](interconnect/) | 🔴 规划中 | 总线/NoC 互连 |
| [peripheral/](peripheral/) | 🔴 规划中 | 系统外设（PLIC/CLINT/UART/Timer） |

## 统一目录结构

每个 IP 遵循以下标准结构：

```
ip/{name}/
├── README.md            # IP 设计文档
├── tlm/                 # CppTLM 事务级模型
├── rtl/                 # CppHDL RTL 模型
├── test/                # 验证测试套件
├── configs/             # JSON 配置和参数 Schema
└── docs/                # 补充设计文档
```

## 通信接口
所有 IP 通过 ★ **CppHDL 的 `ch_stream<Bundle>`** 接口通信（★ **IP 之间 / 核 ↔ 从设备**；核内 stage 间另用 `Payload<T>` ⇒ ADR-092 §D4）：
- ★ **`cf::bundles::MemReq`**：内存请求（address / data / is_write / burst_len / id）★ **代码里的实际名**（旧文档写的 `MemReqBundle` 不存在）
- **MemRespBundle**：内存响应（数据、状态）
- **SnoopBundle**：一致性侦听（多核场景）

## 配置驱动
- 每个 IP 提供 `configs/params_schema.json` 定义可配置参数
- SoC 层通过 JSON 配置文件实例化和连接 IP
- 支持 DSE 参数扫描

## 文档模板
新 IP 开发请参考 [IP 文档模板](../docs/templates/IP_TEMPLATE.md)

## `ip/<name>/docs/` 标准子结构

每个 IP 的 `docs/` 目录建议采用如下最小子结构 (便于跨 IP 一致性):

| 文件 | 作用 | 必备 |
| --- | --- | --- |
| `README.md` | 文档索引 + 状态标签 + 子结构说明 | ✓ |
| `architecture.md` | 微架构 / 接口 / 时序 | ✓ |
| `configuration.md` | 可调参数 (Knobs) 及默认值, 对应 `configs/params_schema.json` | 推荐 |
| `integration.md` | 如何被 SoC / 其他 IP 集成 (总线挂点、地址映射、依赖 IP) | 可选 |

`ip/cpu/` 是历史最久的 IP, 当前未完全遵守此结构; 新增 IP 应当按此标准落地。

## 相关文档
- [项目架构总览](../docs/architecture/overview.md)
- [接口设计详解](../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../docs/architecture/mixed/testing-and-dse.md)
