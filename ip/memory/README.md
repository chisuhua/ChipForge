# Memory IP 设计文档

> ⚠️ **Status: Planning** — Code not yet implemented. See [Phase 1 roadmap](../../docs/roadmap/phases/phase-1-foundation.md) for planned structure.

## 1. 功能概述

### 1.1 模块定位
Memory IP 提供主存储器建模，包括 SRAM（片上）和 DRAM（片外）两种模型，支持时序精确仿真和带宽建模。

### 1.2 核心功能
- 可配置容量和带宽
- DRAM 时序模型（tCAS、tRAS、tRP、tRCD 等）
- Bank 并行度和调度策略建模
- 地址映射可配置（Row-Bank-Column / Bank-Row-Column）
- DMI（Direct Memory Interface）加速模式
- 内存初始化和预加载（ELF/Binary）

### 1.3 性能目标
| 指标 | 目标值 | 说明 |
|------|--------|------|
| SRAM 延迟 | 1-2 cycles | 片上存储 |
| DRAM 延迟 | 50-200 ns | 取决于时序参数 |
| 带宽 | 可配置 | GB/s 级 |

## 2. 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 内存模型 |
| `rtl/` | CppHDL SRAM 控制器 |
| `test/` | 内存验证套件 |
| `configs/` | 时序参数配置 |

## 3. 接口设计

### 3.1 端口定义
| 端口名 | 方向 | 类型 | Bundle | 说明 |
|--------|------|------|--------|------|
| req | in | ch_stream | MemReqBundle | 内存请求 |
| resp | out | ch_stream | MemRespBundle | 内存响应 |

### 3.2 DMI 加速
- TLM_ONLY 模式支持 DMI lookup
- 跳过时序模型直接访问存储区域
- 适用于非性能关键路径（如 ROM 加载）

## 4. 配置参数

| 参数 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| type | string | "dram" | sram/dram | 存储类型 |
| capacity_mb | int | 256 | 1-16384 | 容量 (MB) |
| bus_width_bits | int | 64 | 32-512 | 总线宽度 |
| num_banks | int | 8 | 1-32 | Bank 数量 |
| tCAS | int | 14 | 1-30 | CAS 延迟（周期） |
| tRAS | int | 33 | 1-60 | RAS 延迟（周期） |
| tRP | int | 14 | 1-30 | 预充电延迟 |
| tRCD | int | 14 | 1-30 | RAS-CAS 延迟 |
| scheduling | string | "fr_fcfs" | fr_fcfs/fcfs | 调度策略 |

## 5. DSE 参数化

| 参数 | 扫描范围 | 影响指标 |
|------|---------|---------|
| num_banks | [4, 8, 16] | 并行度 vs 面积 |
| tCAS | [10, 14, 18] | 延迟 vs 频率 |
| bus_width_bits | [32, 64, 128] | 带宽 vs 面积 |
| scheduling | [fcfs, fr_fcfs] | 吞吐量 vs 公平性 |

## 6. 性能统计

| 统计项 | 类型 | 说明 |
|--------|------|------|
| read_count | counter | 读请求数 |
| write_count | counter | 写请求数 |
| avg_latency | histogram | 平均访问延迟 |
| bandwidth_util | rate | 带宽利用率 |
| bank_conflict_count | counter | Bank 冲突次数 |

## 7. 相关文档
- [项目架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
