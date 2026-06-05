# Interconnect IP 设计文档

> ⚠️ **Status: Planning** — Code not yet implemented. See [Phase 1 roadmap](../../docs/roadmap/phases/phase-1-foundation.md) for planned structure.

## 1. 功能概述

### 1.1 模块定位
Interconnect IP 负责 SoC 内各组件（CPU、Cache、Memory、Peripheral）之间的数据路由和仲裁。支持总线和 NoC 两种拓扑模式。

### 1.2 核心功能
- 地址路由和译码（基于内存地图）
- 多种仲裁策略（Round-Robin、Priority、TDMA）
- 总线模式（单主/多主共享总线）
- NoC 模式（Mesh/Ring 拓扑，支持虫洞路由）
- 流量整形和 QoS 支持
- 事务 ID 管理和乱序完成

### 1.3 性能目标
| 指标 | 目标值 | 说明 |
|------|--------|------|
| 路由延迟 | 1-3 cycles | 单跳延迟 |
| 仲裁延迟 | 0-1 cycle | 无竞争时 |
| 吞吐量 | 线速 | 无阻塞时 |

## 2. 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 总线/NoC 模型 |
| `rtl/` | CppHDL RTL 实现 |
| `test/` | 互连验证套件 |
| `configs/` | 拓扑和仲裁配置 |

## 3. 接口设计

### 3.1 端口定义
| 端口名 | 方向 | 类型 | Bundle | 说明 |
|--------|------|------|--------|------|
| master_port[N] | in | ch_stream | MemReqBundle | 主设备请求 |
| master_resp[N] | out | ch_stream | MemRespBundle | 主设备响应 |
| slave_port[M] | out | ch_stream | MemReqBundle | 从设备请求 |
| slave_resp[M] | in | ch_stream | MemRespBundle | 从设备响应 |

### 3.2 地址路由
基于 SoC 内存地图进行地址译码，将请求路由到正确的从设备。

## 4. 可插拔策略

| 策略类型 | 可选实现 | 默认值 | 说明 |
|---------|---------|--------|------|
| 仲裁策略 | RoundRobin, Priority, TDMA | RoundRobin | 主设备竞争仲裁 |
| 路由算法 | XY, YX, Adaptive | XY | NoC 路由策略 |
| 拓扑 | Bus, Crossbar, Mesh, Ring | Bus | 互连拓扑 |

## 5. 配置参数

| 参数 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| topology | string | "bus" | bus/crossbar/mesh/ring | 拓扑类型 |
| num_masters | int | 1 | 1-16 | 主设备端口数 |
| num_slaves | int | 4 | 1-32 | 从设备端口数 |
| data_width_bits | int | 64 | 32-512 | 数据位宽 |
| arbitration | string | "round_robin" | 见策略表 | 仲裁策略 |
| max_outstanding | int | 4 | 1-16 | 最大未完成事务数 |

## 6. DSE 参数化

| 参数 | 扫描范围 | 影响指标 |
|------|---------|---------|
| topology | [bus, crossbar, mesh] | 吞吐量 vs 面积 |
| arbitration | [RR, Priority, TDMA] | 公平性 vs 延迟 |
| data_width_bits | [32, 64, 128] | 带宽 vs 面积 |
| max_outstanding | [1, 2, 4, 8] | 吞吐量 vs 缓冲区面积 |

## 7. 性能统计

| 统计项 | 类型 | 说明 |
|--------|------|------|
| total_transactions | counter | 总事务数 |
| avg_latency | histogram | 平均路由延迟 |
| arbitration_stalls | counter | 仲裁等待次数 |
| bandwidth_per_port | rate | 各端口带宽利用率 |

## 8. 相关文档
- [项目架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
