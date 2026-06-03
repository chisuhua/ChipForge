# Cache IP 设计文档

## 1. 功能概述

### 1.1 模块定位
Cache IP 位于 CPU 与主存之间，提供低延迟的数据/指令缓存服务。支持多级缓存（L1I/L1D/L2）配置。

### 1.2 核心功能
- 可配置容量、关联度、行大小
- 支持多种替换策略（LRU、PLRU、Random、FIFO）
- 支持多种写策略（Write-Back、Write-Through）
- 支持预取策略（Stride、Next-Line、Stream）
- MESI/MOESI 一致性协议支持（多核场景）
- 性能统计（命中率、Miss 延迟、带宽利用率）

### 1.3 性能目标
| 指标 | 目标值 | 说明 |
|------|--------|------|
| Hit Latency | 1-3 cycles | 取决于缓存级别 |
| Miss Penalty | 10-100 cycles | 取决于下级存储延迟 |
| Hit Rate | > 90% (L1) | 典型工作负载 |

## 2. 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 缓存模型（周期精确） |
| `rtl/` | CppHDL RTL 实现 |
| `test/` | 缓存验证套件 |
| `configs/` | 容量/策略配置 |

## 3. 接口设计

### 3.1 端口定义
| 端口名 | 方向 | 类型 | Bundle | 说明 |
|--------|------|------|--------|------|
| cpu_req | in | ch_stream | MemReqBundle | CPU 侧请求 |
| cpu_resp | out | ch_stream | MemRespBundle | CPU 侧响应 |
| mem_req | out | ch_stream | MemReqBundle | 下级存储请求 |
| mem_resp | in | ch_stream | MemRespBundle | 下级存储响应 |
| snoop | in/out | ch_stream | SnoopBundle | 一致性侦听（多核） |

### 3.2 握手协议
- valid/ready 背压握手
- 请求-响应配对（通过 tag/id 关联）

## 4. 可插拔策略

| 策略类型 | 可选实现 | 默认值 | 说明 |
|---------|---------|--------|------|
| 替换策略 | LRU, PLRU, Random, FIFO | LRU | 缓存行淘汰算法 |
| 写策略 | WriteBack, WriteThrough | WriteBack | 写命中处理方式 |
| 分配策略 | WriteAllocate, NoWriteAllocate | WriteAllocate | 写缺失处理方式 |
| 预取策略 | None, NextLine, Stride, Stream | None | 硬件预取算法 |

## 5. 配置参数

| 参数 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| capacity_kb | int | 32 | 1-4096 | 容量 (KB) |
| associativity | int | 4 | 1-16 | 关联度（路数） |
| line_size_bytes | int | 64 | 16-256 | 缓存行大小 |
| num_mshr | int | 4 | 1-16 | MSHR 条目数 |
| hit_latency | int | 1 | 1-10 | 命中延迟（周期） |
| replacement_policy | string | "lru" | 见策略表 | 替换算法 |
| write_policy | string | "write_back" | 见策略表 | 写策略 |
| prefetch_policy | string | "none" | 见策略表 | 预取策略 |

## 6. DSE 参数化

### 可探索维度
| 参数 | 扫描范围 | 影响指标 |
|------|---------|---------|
| capacity_kb | [4, 8, 16, 32, 64] | 命中率 vs 面积 |
| associativity | [1, 2, 4, 8] | 命中率 vs 延迟 |
| replacement_policy | [LRU, PLRU, Random] | 命中率 vs 复杂度 |
| prefetch_policy | [None, NextLine, Stride] | 带宽 vs 命中率 |

## 7. 性能统计

| 统计项 | 类型 | 说明 |
|--------|------|------|
| hit_count | counter | 命中次数 |
| miss_count | counter | 缺失次数 |
| hit_rate | rate | 命中率 |
| avg_miss_latency | histogram | 平均缺失延迟 |
| eviction_count | counter | 淘汰次数 |
| prefetch_hit_count | counter | 预取命中次数 |

## 8. 相关文档
- [项架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../../docs/architecture/testing-and-dse.md)
