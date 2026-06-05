# RISC-V CPU IP

## 概述
基于 CppTLM 的 RISC-V CPU IP 核，采用 Plugin + Stageable 架构设计，支持 RV32I/RV64GC 指令集。

## 设计方案
本 IP 采用 **VexRiscv-on-CppTLM Stageable 方案**（详见 [VexRiscvOnCppTLM.md](docs/riscv/VexRiscvOnCppTLM.md)）作为最终实现规范。

核心设计原则：
- **水平功能切片**：每个 Plugin 横跨多个流水线阶段
- **PipeLink 跨阶段声明**：Plugin 间通过 PipeLink 共享数据
- **PipeBuilder DSL 编译式调度**：Plugin 在 setup 阶段声明/发现能力
- **权威设计**: 详见 [multi_isa_architecture.md](docs/multi_isa_architecture.md) v2.0

> 📖 [VexRiscvArch.md](docs/riscv/VexRiscvArch.md) 为早期设计参考文档，仅供理解设计演进使用。

## 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 事务级模型实现 |
| `rtl/` | CppHDL RTL 级实现（Phase 5） |
| `test/` | 验证测试套件（Level A/B/C） |
| `configs/` | JSON 配置文件和参数 Schema |
| `docs/` | 设计参考文档 |

## 接口概要

### 对外端口
- **IBus**（指令总线）：`ch_stream<MemReqBundle>` / `ch_stream<MemRespBundle>`
- **DBus**（数据总线）：`ch_stream<MemReqBundle>` / `ch_stream<MemRespBundle>`
- **IRQ**（中断输入）：外部中断信号接口

### ImplMode 支持
- TLM_ONLY：纯事务级 ISS 高速仿真
- RTL_ONLY：纯 RTL 精确仿真（Phase 5）
- COMPARE：TLM/RTL 对比验证模式
- SHADOW：RTL 主导 + TLM 监控模式

> **注意**：ImplMode 在顶层 SoC 配置（如 `soc/riscv_virt.json`）中指定，不在此 IP 级配置中。IP 配置仅定义功能参数。

## 可配置参数
详见 [configs/cpu_params_schema.json](configs/cpu_params_schema.json)

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| isa | string | "rv64gc" | 指令集架构 |
| pipeline_stages | int (1-12) | 5 | 流水线级数 |
| clock_freq_mhz | int (1-2000) | 100 | 时钟频率 (MHz) |
| enable_pmp | bool | true | 物理内存保护 |
| enable_mmu | bool | true | 虚拟内存开关 |
| mmu_mode | enum | "sv39" | sv32/sv39/sv48 |
| branch_predictor | enum | "gshare" | static/bimodal/gshare/tournament |
| btb_entries | enum | 64 | 16/32/64/128/256 |
| icache_latency_cycles | int (0-32) | 1 | ICache 命中延迟 |
| dcache_latency_cycles | int (0-32) | 1 | DCache 命中延迟 |

## 快速开始

```bash
# 编译 TLM 版本
mkdir build && cd build
cmake .. -DCPU_IMPL_MODE=TLM_ONLY
make

# 运行最小测试
./test/cpu_unit_test
```

## 相关文档
- [项目架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../../docs/architecture/testing-and-dse.md)
