# ChipForge IP 库

## 概述
本目录包含 ChipForge 平台的所有硬件 IP 核心，每个 IP 遵循统一的目录结构和接口规范。

## IP 模块列表

| IP | 状态 | 说明 |
|----|------|------|
| [cpu/](cpu/) | 🟡 设计中 | RISC-V CPU（Plugin + Stageable 架构） |
| [cache/](cache/) | 🔴 规划中 | 多级缓存（L1I/L1D/L2） |
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
所有 IP 通过 `ch_stream<Bundle>` 接口通信：
- **MemReqBundle**：内存请求（地址、数据、操作类型）
- **MemRespBundle**：内存响应（数据、状态）
- **SnoopBundle**：一致性侦听（多核场景）

## 配置驱动
- 每个 IP 提供 `configs/params_schema.json` 定义可配置参数
- SoC 层通过 JSON 配置文件实例化和连接 IP
- 支持 DSE 参数扫描

## 文档模板
新 IP 开发请参考 [IP 文档模板](../docs/templates/IP_TEMPLATE.md)

## 相关文档
- [项目架构总览](../docs/architecture/overview.md)
- [接口设计详解](../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../docs/architecture/testing-and-dse.md)
