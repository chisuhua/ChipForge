# TileCopy IP 架构文档

> **版本**: v1.0  
> **日期**: 2026-06-15  
> **状态**: 初始设计  
> **项目**: ChipForge  
> **功能**: Tile 级异步数据搬运引擎（类 NVIDIA TMA）

---

## 1. 架构概述

### 1.1 定位

TileCopy 是 ChipForge 加速器架构中的**数据搬运引擎**，负责：
- GMEM ↔ SMEM 之间的异步张量搬运
- Cluster 内的 TMA Multicast 广播
- mbarrier 同步原语管理

### 1.2 与 TileCore 的关系

```
                    ┌─────────────────────────────────────┐
                    │              Cluster                  │
                    │                                        │
                    │  ┌─────────┐    ┌─────────┐          │
                    │  │TileCore │◀──▶│TileCopy │          │
                    │  │ (MMA)   │    │  (TMA)   │          │
                    │  └────┬────┘    └────┬────┘          │
                    │       │              │               │
                    │  SMEM │◀────────────┘                │
                    │  (Tile Data)                         │
                    └─────────────────────────────────────┘
```

- **TileCopy**：数据准备（Producer），负责将数据从 GMEM 加载到 SMEM
- **TileCore**：数据消费（Consumer），从 SMEM 读取数据进行 MMA 计算

---

## 2. 核心功能

### 2.1 TMA Engine

| 功能 | 说明 |
|------|------|
| **tma.load** | GMEM → SMEM 异步张量加载，支持 dtype 转换 |
| **tma.store** | SMEM → GMEM 异步张量存储 |
| **tma.multicast** | 单 SMEM → 多 SMEM 广播（Cluster 级） |
| **cp.async** | SMEM → SMEM / SMEM → TMEM 异步拷贝 |

### 2.2 Barrier Controller

| 功能 | 说明 |
|------|------|
| **bar.init** | 初始化 barrier，指定 count 和 scope |
| **bar.arrive** | 到达 barrier，递增计数 |
| **bar.wait** | 阻塞等待直到计数达到目标 |
| **bar.try_wait** | 非阻塞轮询（用于尾部延迟隐藏） |

### 2.3 数据类型转换

| 输入格式 | 输出格式 | 说明 |
|----------|----------|------|
| FP4 (NVFP4) | FP16/BF16 | 4-bit 浮点解压 |
| INT4 | FP16 | 4-bit 整数解压 |
| INT8 | FP16/BF16 | 8-bit 整数解压 |
| FP8 (E4M3/E5M2) | FP16/BF16 | 8-bit 浮点转换 |
| FP16/BF16 | FP16/BF16 | 直通（无转换） |

---

## 3. 存储层次

```
DRAM (GMEM)
 ↑↓ async [TMA Engine]
SMEM (每 SM 228KB，多 Bank)
 ↑↓ async [cp.async]
TMEM (每 SM 256KB，累加专用)
```

---

## 4. 指令集

### 4.1 TMA 指令

```cpp
// GMEM → SMEM 异步加载
void tma_load(
    SMEM_ADDR dst,      // 目标 SMEM 地址
    GMEM_ADDR src,      // 源 GMEM 地址
    TileDesc desc,      // Tile 描述符
    mbarrier_id bar,    // 同步 barrier
    DtypeConv conv      // 数据类型转换
);

// SMEM → GMEM 异步存储
void tma_store(
    GMEM_ADDR dst,
    SMEM_ADDR src,
    mbarrier_id bar
);

// Cluster 级广播
void tma_multicast(
    SMEM_ADDR dst[],     // 目标 SMEM 地址数组
    GMEM_ADDR src,      // 源 SMEM 地址
    ClusterMask mask,   // Cluster 内目标 mask
    mbarrier_id bar
);
```

### 4.2 cp.async 指令

```cpp
// SMEM → SMEM 异步拷贝
void cp_async(
    ADDR dst,
    ADDR src,
    Size size,
    mbarrier_id bar
);
```

### 4.3 barrier 指令

```cpp
// 初始化 barrier
void bar_init(mbarrier_id id, Count count, Scope scope);
// scope: scope_cta / scope_cluster / scope_device

// 到达 barrier
void bar_arrive(mbarrier_id id);

// 等待 barrier
void bar_wait(mbarrier_id id, Target target);

// 非阻塞尝试等待
void bar_try_wait(mbarrier_id id, Target target, Timeout timeout);
```

---

## 5. 配置参数

```json
{
  "tma_engine": {
    "max_tile_dim": 256,
    "max_bytes_per Transaction": 4096,
    "async_depth": 4,
    "dtype_conv_support": ["fp4", "int4", "int8", "fp8_e4m3", "fp8_e5m2", "fp16", "bf16"]
  },
  "barrier_controller": {
    "max_barriers": 32,
    "scope_support": ["scope_cta", "scope_cluster", "scope_device"],
    "try_wait_timeout_cycles": 1024
  },
  "multicast_router": {
    "max_cluster_size": 8,
    "max_dst_count": 8,
    "broadcast_latency_cycles": 10
  }
}
```

---

## 6. 性能模型

### 6.1 TMA 带宽

| 参数 | 说明 |
|------|------|
| `T_load` | TMA 加载时间：`T_load = (Br × Bc × dtype_size) / TMA_BW` |
| `TMA_BW` | TMA 带宽（理论峰值 ~2 TB/s @ 4GHz） |

### 6.2 气泡来源与缓解

| 气泡来源 | 缓解策略 |
|----------|----------|
| TMA 尾部延迟 | `barrier.try_wait` + 超时回退 |
| SMEM Bank Conflict | Swizzle 布局策略 |
| Cluster 广播冲突 | 树形广播网络 |

---

## 7. 验证指标

| 指标 | 目标 |
|------|------|
| TMA 带宽利用率 | > 80% |
| barrier 同步精度 | 100% |
| 数据类型转换误差 | < 0.01% |

---

## 8. 演进路线图

| 版本 | 特性 | 目标 |
|------|------|------|
| v1.0 | 基础 TMA + barrier | 支持 tilecore 数据加载 |
| v1.5 | dtype 转换增强 | 支持 FP4/INT4 解压 |
| v2.0 | multicast 优化 | Cluster 级广播性能提升 |

---

## 附录：术语表

| 术语 | 定义 |
|------|------|
| **TMA** | Tensor Memory Accelerator，异步张量搬运引擎 |
| **SMEM** | Shared Memory，SM 内多 Bank SRAM |
| **mbarrier** | Memory Barrier，显式到达-等待同步原语 |
| **Tile** | 张量分块，如 Q Tile (Br×d)、K Tile (Bc×d) |
| **dtype conv** | 数据类型转换，如 FP4 → FP16 |

---

**本文档为 ChipForge 项目 TileCopy IP 的架构设计文档。**