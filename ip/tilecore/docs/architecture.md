# 双层脉动矩阵乘 IP 架构文档

> **版本**: v1.0  
> **日期**: 2026-06-14  
> **状态**: 初始架构设计  
> **项目**: ChipForge  
> **作者**: CTO (基于用户设计草案)

---

## 目录

1. [架构概述](#1-架构概述)
2. [计算层次结构](#2-计算层次结构)
3. [内存层次与一致性模型](#3-内存层次与一致性模型)
4. [双层脉动数据流模型](#4-双层脉动数据流模型)
5. [指令集架构 ISA](#5-指令集架构isa)
6. [Warp 角色与 FlashAttention-2 流水线映射](#6-warp-角色与-flashattention-2-流水线映射)
7. [无气泡调度条件与性能模型](#7-无气泡调度条件与性能模型)
8. [非矩阵运算支持 CUDA Core 详细设计](#8-非矩阵运算支持cuda-core-详细设计)
9. [量化与稀疏格式支持](#9-量化与稀疏格式支持)
10. [配置参数与面积功耗模型](#10-配置参数与面积功耗模型)
11. [软件栈与编程模型](#11-软件栈与编程模型)
12. [验证与调试建议](#12-验证与调试建议)
13. [演进路线图](#13-演进路线图)
14. [附录A术语表](#附录a术语表)
15. [附录B参考架构对比](#附录b参考架构对比)

---

## 1. 架构概述

### 1.1 定位

- **目标工作负载**：LLM 推理（Prefill/Decode）、MoE、Diffusion、Vision Transformer
- **核心矛盾**：算法演进速度（新量化、新稀疏、新Attention变体） vs. ASIC 能效与面积效率
- **解决方案**：以**显式异步数据流**替代隐式缓存一致性，以**软件定义 Tile 策略**替代固定脉动拓扑，以**Warp Specialization**实现计算-存储-控制的三级流水线

### 1.2 设计哲学

提取 PTX 显式异步编程模型（TMA / tcgen05.mma / mbarrier / TMEM / Warp Specialization）的精髓，将其注入极简、低功耗、省面积的专用推理芯片。保留 CUDA Core 以支持非矩阵运算（Softmax、激活、量化/反量化、稀疏解码），通过双层（Tile-Intra 微脉动 + Tile-Inter 宏脉动）类脉动数据流实现无气泡高效执行。

### 1.3 与 GPU/TPU 的本质差异

| 维度 | NVIDIA Blackwell | Google TPU | 本架构 (Systolic-Flow DSA) |
|------|----------------|------------|--------------------------|
| 编程模型 | SIMT + Warp Spec | XLA 隐式映射 | **显式数据流 + 微码控制** |
| 控制流 | Warp 级发散/汇聚 | 无（纯数据流） | **Warp 角色固定，无发散** |
| 存储一致性 | 复杂 Cache/Coherence | 无（软件管理） | **显式 mbarrier，无隐式 Cache** |
| 通用性 | 图形+AI+科学计算 | 仅 GEMM | **AI 推理 + 轻量通用（CUDA Core）** |
| 面积/功耗 | 高（通用开销） | 极低 | **接近 TPU 能效，保留 GPU 灵活性** |

---

## 2. 计算层次结构

### 2.1 顶层：Cluster（协作簇）

- **组成**：4 ~ 8 个 SM（Streaming Module）
- **互联**：片上 NoC + **DSMEM（Distributed Shared Memory）**，延迟 ~10-20 周期
- **作用**：实现 Tile-Inter 宏脉动，多 SM 协同处理超长序列或超大 Head Dim

### 2.2 中层：SM（流处理模块）

每个 SM 是独立的**异步数据流引擎**，内部包含：

| 单元 | 数量/规模 | 功能 | 面积优化策略 |
|------|----------|------|------------|
| **Control Warp** | 1 个（32 线程） | 指令取指、调度、barrier 管理 | 极简标量控制，无 SIMT 堆栈 |
| **TMA Engine** | 1 个 | 异步张量搬运（GMEM↔SMEM，SMEM↔SMEM） | 固定功能 DMA，无地址翻译单元 |
| **MMA Engine** | 1 个 | 脉动阵列 / 可重构 MAC 阵列 | 位可配置乘法器，支持混合精度 |
| **CUDA Core Array** | 2~4 个 Warp | 非矩阵运算（Softmax、激活、量化） | 轻量 SIMD，无完整 FP64 |
| **TMEM** | 256 KB | 矩阵累加器、中间结果驻留 | 专用 SRAM，无通用寻址 |
| **SMEM** | 228 KB | 操作数缓冲、Ping-Pong Buffer | 多 Bank 显式管理 |
| **Register File** | 128 KB | 仅 CUDA Core 和 Control 使用 | 大幅削减（GPU 的 1/4） |

### 2.3 底层：Warp（硬件线程组）

- **Warp Size**：32 线程（兼容 CUDA 抽象，但硬件极简）
- **关键差异**：Warp **角色在编译时静态绑定**，运行时无上下文切换、无发散/汇聚逻辑
- **Warp Group**：4 个 Warp = 128 线程，用于大规模数据并行（如 Epilogue）

---

## 3. 内存层次与一致性模型

### 3.1 存储层级

```
DRAM (GMEM)
 ↑↓ async [TMA Engine]
SMEM (每 SM 228KB，多 Bank)
 ↑↓ async [cp.async / 显式拷贝]
TMEM (每 SM 256KB，累加专用)
 ↑↓ sync [mma.sync / tmem.op]
Register (仅 CUDA Core 使用)
```

### 3.2 显式一致性（无隐式 Cache）

- **无 L1/L2 Cache**：完全消除 Cache Tag/Coherence 硬件开销
- **一致性原语**：`mbarrier` 风格的显式到达-等待屏障

| 原语 | 语义 |
|------|------|
| `barrier.arrive id, count` | 到达并递增计数 |
| `barrier.wait id, target` | 阻塞直到计数达到目标 |
| `barrier.try_wait id, target, timeout` | 非阻塞轮询（用于尾部延迟隐藏） |

- **作用域**：
  - `scope_cta`：SM 内所有 Warp
  - `scope_cluster`：Cluster 内所有 SM（通过 DSMEM 传递 barrier 状态）
  - `scope_device`：全局原子操作（仅用于 Kernel 启动/结束）

---

## 4. 双层脉动数据流模型

### 4.1 核心抽象

将 FlashAttention-2 的内循环映射为**分形流水线**：
- **微脉动（Micro-Systolic）**：单个 SM 内，Tile 级数据在 MMA Engine、TMEM、SMEM 之间按节拍流动
- **宏脉动（Macro-Systolic）**：Cluster 内，多个 SM 协作，A(Q) 广播流入，B(K/V) 驻留，Partial O 逐级归约

### 4.2 微脉动层（Tile-Intra）：SM 内部

#### 数据流拓扑

```
[SMEM_A] ──→ [MMA Engine] ──→ [TMEM_D]
 ↑ ↓ │
[SMEM_B] ───── [TMEM_A/B] ←──────┘ (原地累加)
```

- **A（Q Tile）**：流式进入，来自 SMEM 或 TMEM，每个周期推进一列/一行
- **B（K/V Tile）**：驻留 SMEM，整个内循环期间复用
- **D（结果）**：驻留 TMEM，支持 Read-Modify-Write 原地累加

#### 无气泡条件（稳态）

```
T_multicast(A[i+1]) + T_smem2mma ≤ T_mma(A[i], B_local) + T_softmax_overlap
```

| 参数 | 说明 |
|------|------|
| `T_mma` | 由 Tile Shape 和 K-depth 决定，Blackwell-like 异步 MMA 约 100-200 周期 |
| `T_softmax_overlap` | CUDA Core 执行 Softmax 的时间，可与下一轮的 `T_mma` 重叠 |

### 4.3 宏脉动层（Tile-Inter）：Cluster 间协作

#### 数据流拓扑（FlashAttention-2 多 SM 协作）

```
SM0: Q_tile ──→ MMA ──→ Partial_O0 ──┐
SM1: Q_tile ──→ MMA ──→ Partial_O1 ──┼──→ [Online Softmax Reduction] ──→ Final_O
SM2: Q_tile ──→ MMA ──→ Partial_O2 ──┘
SM3: Q_tile ──→ MMA ──→ Partial_O3 ──┘
```

- **Q Tile**：通过 TMA Multicast 广播到 Cluster 内所有 SM（或每个 SM 自取相同 Q）
- **K/V Tiles**：**分片驻留**在不同 SM 的 SMEM 中（SM0 持 K0/V0，SM1 持 K1/V1...）
- **Partial O**：每个 SM 计算本地 K/V 分片对应的 Partial O，携带 `row_max` 和 `row_sum`
- **归约**：通过 DSMEM 或专用 Reduction Bus 执行 Online Softmax 归约，合并 Partial O

#### 宏脉动无气泡条件

```
T_reduction(i+1) ≤ T_mma(i) + T_softmax(i)
```

要求 Cluster 级归约延迟被隐藏在本 SM 的计算+Softmax 时间内。

---

## 5. 指令集架构 ISA

### 5.1 指令格式（定长 64bit）

```
[OpCode 8bit] [Role Mask 8bit] [Dst 12bit] [SrcA 12bit] [SrcB 12bit] [Flags 12bit]
```

- `Role Mask`：指定该指令由哪个 Warp 角色执行（Producer/MMA/Softmax/Epilogue）

### 5.2 核心指令集

#### A. 异步搬运（TMA-like）

| 指令 | 语义 | 操作数 |
|------|------|--------|
| `tma.load` | GMEM → SMEM 异步张量加载 | `dst(SMEM_desc), src(GMEM_desc), barrier_id, dtype_conv` |
| `tma.store` | SMEM → GMEM 异步张量存储 | `dst(GMEM_desc), src(SMEM_desc), barrier_id` |
| `tma.multicast` | 单加载 → Cluster 多 SM 广播 | `dst(SMEM_desc[]), src(GMEM_desc), cluster_mask, barrier_id` |
| `cp.async` | SMEM → SMEM / SMEM → TMEM 异步拷贝 | `dst, src, size, barrier_id` |

#### B. 矩阵运算（tcgen05-like）

| 指令 | 语义 | 操作数 |
|------|------|--------|
| `mma.sync` | 同步 MMA（阻塞直到完成） | `dst(TMEM), srcA(SMEM/TMEM), srcB(SMEM/TMEM), shape, dtype, acc_flag` |
| `mma.async` | 异步 MMA（发射即返回） | 同上，需后续 `mma.wait` |
| `mma.wait` | 等待指定 MMA 完成 | `group_id` |
| `tmem.zero` | TMEM 清零（初始化累加器） | `dst(TMEM_desc)` |
| `tmem.rescale` | TMEM 内原地 rescaling（Softmax 用） | `dst(TMEM), scale(TMEM/SMEM), row_mask` |

#### C. 通用计算（CUDA Core-like）

| 指令 | 语义 | 操作数 |
|------|------|--------|
| `cuda.fma` | 浮点乘加 | `dst(Reg), srcA, srcB, srcC` |
| `cuda.exp2` | 指数（Softmax 用） | `dst(Reg), src(Reg)` |
| `cuda.max` | 向量最大值 | `dst(Reg), srcA(Reg), srcB(Reg)` |
| `cuda.sum` | 向量求和 | `dst(Reg), src(Reg)` |
| `cuda.dequant` | 反量化（INT4/FP4 → FP16） | `dst(Reg), src(SMEM), scale(Reg)` |
| `cuda.quant` | 量化（FP16 → INT4/FP8） | `dst(SMEM), src(Reg), scale(Reg)` |

#### D. 同步与屏障（mbarrier-like）

| 指令 | 语义 | 操作数 |
|------|------|--------|
| `bar.init` | 初始化屏障 | `id, count, scope` |
| `bar.arrive` | 到达屏障 | `id` |
| `bar.wait` | 等待屏障 | `id, target` |
| `cluster.sync` | Cluster 级全同步 | `barrier_id` |
| `cluster.reduce` | Cluster 级归约（Online Softmax） | `dst(SMEM), src(SMEM), op(max/sum), scope` |

---

## 6. Warp 角色与 FlashAttention-2 流水线映射

### 6.1 角色定义（5-Way Specialization）

每个 SM 内部，Warp 被静态划分为 5 个角色：

| 角色 | Warp ID | 数量 | 职责 | 关键指令 |
|------|---------|------|------|----------|
| **Producer** | 0 | 1 Warp (32 threads) | TMA 加载 Q/K/V，管理双缓冲 SMEM | `tma.load`, `tma.multicast`, `bar.arrive` |
| **MMA** | 1 | 1 Warp (32 threads) | 发射 MMA 指令，管理 TMEM 累加器 | `mma.async`, `mma.wait`, `tmem.zero` |
| **Softmax** | 2 | 1 Warp (32 threads) | CUDA Core 执行 row-wise softmax (max, exp, sum) | `cuda.max`, `cuda.exp2`, `cuda.sum`, `tmem.rescale` |
| **Correction** | 3 | 1 Warp (32 threads) | 当 row_max 更新时，对 TMEM 内 O 进行 rescaling | `tmem.rescale`, `cp.async` |
| **Epilogue** | 4-7 | 4 Warps (128 threads) | TMEM → SMEM → GMEM 写回，支持量化/反量化 | `cp.async`, `tma.store`, `cuda.quant` |

### 6.2 双层流水线时序（无气泡执行）

#### 阶段 1：初始化（第 0 个 K Tile）

```asm
; Producer
bar.init B0, 2, scope_cta ; Producer + MMA 握手
tma.load SMEM_Q, GMEM_Q, B0, fp16
tma.load SMEM_K0, GMEM_K, B0, fp16
bar.arrive B0

; MMA (等待 Q/K 到达)
bar.wait B0, 2
mma.async TMEM_S, SMEM_Q, SMEM_K0, shape=(Br,Bc,d), dtype=fp16, acc=zero
mma.wait 0

; Softmax (等待 S 完成)
bar.init B1, 2, scope_cta ; MMA + Softmax 握手
bar.arrive B1

; Softmax Warp
cuda.max REG_m, TMEM_S, ROW ; 计算 row max
cuda.exp2 REG_exp, TMEM_S - REG_m ; exp(S - m)
cuda.sum REG_l, REG_exp ; sum(exp)
tmem.rescale TMEM_O, REG_exp, ROW ; 写入 O (初始)
bar.arrive B1
```

#### 阶段 2：稳态流水线（K Tile i 与 i+1 重叠）

```asm
; Producer (加载 K[i+1] 和 V[i+1]，与计算 K[i] 重叠)
bar.wait B2, 2 ; 等待 Epilogue 释放缓冲
tma.load SMEM_K1, GMEM_K[i+1], B2, fp16
tma.load SMEM_V1, GMEM_V[i+1], B2, fp16
bar.arrive B2

; MMA (计算 S[i+1] = Q × K[i+1]^T，与 Softmax[i] 重叠)
bar.wait B2, 2 ; 等待 K[i+1] 到达
mma.async TMEM_S1, SMEM_Q, SMEM_K1, shape=(Br,Bc,d), acc=zero
mma.wait 0
bar.arrive B3 ; 通知 Softmax

; Softmax (处理 S[i]，同时 MMA 计算 S[i+1])
bar.wait B3, 2
cuda.max REG_m_new, TMEM_S, ROW
cuda.sub REG_delta, REG_m_new, REG_m_old ; delta = m_new - m_old
cuda.exp2 REG_scale, -REG_delta ; scale = exp(-delta)
tmem.rescale TMEM_O, REG_scale, ROW ; O_old *= scale (Correction 逻辑)
cuda.exp2 REG_exp, TMEM_S - REG_m_new
cuda.sum REG_l_new, REG_exp
; 更新 l = l_old * scale + l_new
bar.arrive B3

; MMA (计算 O += P × V，P = softmax(S))
mma.async TMEM_O, TMEM_P, SMEM_V, shape=(Br,d,Bc), acc=TMEM_O
mma.wait 0
bar.arrive B4 ; 通知 Epilogue

; Epilogue (当所有 K Tiles 完成，写回 O)
bar.wait B4, 2
cp.async SMEM_O, TMEM_O, size=Br*d*2
bar.init B5, 2, scope_cta
tma.store GMEM_O, SMEM_O, B5
bar.arrive B5
```

#### 阶段 3：宏脉动（Cluster 级协作）

```asm
; Cluster 内 4 个 SM 各自处理 K/V 的不同分片
; SM0: K[0:31], SM1: K[32:63], SM2: K[64:95], SM3: K[96:127]

; 每个 SM 完成本地 Partial O 后，进入归约
cluster.sync C0 ; 等待所有 SM 到达

; 指定一个 SM（如 SM0）的 Correction Warp 执行 Online Softmax 归约
cluster.reduce SMEM_O_final, SMEM_O_partial[], op=online_softmax, scope=cluster
; online_softmax 归约：比较 row_max， rescaling 较小者，累加 sum 和 O

cluster.sync C1
; 最终结果由 SM0 的 Epilogue Warp 写回 GMEM
```

---

## 7. 无气泡调度条件与性能模型

### 7.1 微脉动无气泡（SM 内）

**关键不等式**（每级 K Tile 流水线）：

```
T_load(K[i+1]) + T_smem_setup ≤ T_mma(S[i]) + T_softmax(S[i-1])
```

| 参数 | 定义 |
|------|------|
| `T_load` | TMA 从 GMEM 加载 K/V Tile 到 SMEM 的时间：`T_load = (Br × Bc × dtype_size) / TMA_BW` |
| `T_mma` | MMA Engine 计算 Q×K^T 或 P×V 的时间：`T_mma = (Br × Bc × d) / (MAC_array_size × freq)` |
| `T_softmax` | CUDA Core 执行 row-wise softmax 的时间：`T_softmax = Br × (T_max + T_exp + T_sum + T_rescale)` |

**满足条件**：
1. **K-depth 足够大**：`Bc`（K Tile 的序列维度）越大，`T_mma` 越长，越容易掩盖加载延迟
2. **双缓冲 SMEM**：SMEM 分为 Bank0/Bank1，Producer 写 Bank1 时 MMA 读 Bank0
3. **TMEM 原地累加**：消除 MMA→SMEM→MMA 的往返延迟
4. **Softmax 与 MMA 重叠**：Softmax Warp 处理 Tile[i] 时，MMA Warp 计算 Tile[i+1]

### 7.2 宏脉动无气泡（Cluster 间）

**关键不等式**（多 SM 协作）：

```
T_cluster_reduce(i+1) ≤ T_mma(i) + T_softmax(i) + T_local_epilogue(i)
```

| 参数 | 定义 |
|------|------|
| `T_cluster_reduce` | 通过 DSMEM 或 Reduction Bus 归约 Partial O 的时间：`T_cluster_reduce = (Br × d × dtype_size) / DSMEM_BW + T_rescale_latency` |

**满足条件**：
1. **Q Broadcast 提前完成**：Q Tile 在 Cluster 内通过 `tma.multicast` 一次性广播到所有 SM
2. **K/V 分片驻留**：每个 SM 的 K/V 分片在整个 Q Tile 处理期间驻留 SMEM，不重复加载
3. **归约与计算重叠**：SM[i] 计算当前分片时，Cluster 归约网络归约前一个分片的结果
4. **流水线深度 ≥ 3**：至少 3 级 K Tile 在飞（Flight），确保总有计算掩盖通信

### 7.3 气泡来源与缓解

| 气泡来源 | 原因 | 缓解策略 |
|----------|------|----------|
| **TMA 尾部延迟** | 最后一个 K Tile 加载慢于预期 | `barrier.try_wait` + 超时回退 |
| **SMEM Bank Conflict** | K/V 布局导致多 Warp 同时访问同一 Bank | Swizzle 布局（`permute` 模式） |
| **TMEM 容量不足** | 大 Head Dim 时无法容纳双缓冲 A + 累加器 D | 采用 TMEM 分时复用； spill 到 SMEM |
| **Softmax 依赖链** | row_max 更新触发全局 rescaling | 条件 rescaling（仅当 delta > τ） |
| **Cluster 归约延迟** | DSMEM 带宽竞争 | 增加 Reduction Bus 宽度；树形归约 |

---

## 8. 非矩阵运算支持 CUDA Core 详细设计

### 8.1 设计原则

- **不追求通用性**：无完整 IEEE-754 FP64，无图形渲染管线
- **专用化**：针对 LLM 推理中的高频非矩阵运算优化

### 8.2 CUDA Core 微架构

- **每 SM 配置**：2~4 个 Warp × 32 线程 = 64~128 个轻量 SIMD 通道
- **每通道功能**：
  - FP16/BF16/FP32 FMA（用于 Softmax、LayerNorm、激活）
  - INT8/INT32 整数 ALU（用于索引计算、量化）
  - 专用函数单元：EXP2、LOG2、RCP（用于 Softmax、SwiGLU）
  - 位操作：用于低比特反量化（INT4 → FP16 查表/插值）

### 8.3 与 MMA Engine 的协作数据流

```
[MMA Engine] ──→ [TMEM] ──→ [CUDA Core Array]
 │ │
 └── [Softmax] ├──→ [TMEM rescaling]
 ├──→ [GMEM store]
```

**关键机制**：
- **零拷贝访问**：CUDA Core 可直接读取 TMEM 的指定行（通过 `tmem.ld` 到 Register）
- **异步触发**：MMA 完成信号通过 `mbarrier` 自动触发 Softmax Warp 启动
- **结果原位写回**：Softmax 结果可直接写回 TMEM（下一级 MMA 的输入）或 SMEM

### 8.4 典型非矩阵运算映射

| 运算 | 映射方式 | 执行单元 | 延迟（估计） |
|------|----------|----------|-------------|
| Row-wise Softmax | 32 线程并行处理 32 rows，每 row 向量化的 max/exp/sum | CUDA Core | ~50-100 周期 |
| LayerNorm (x - mean) / sqrt(var + eps) | 2-pass mean/var，然后 fused element-wise | CUDA Core | ~80-150 周期 |
| SwiGLU (SiLU(xW) ⊙ yW) | 矩阵部分走 MMA，SiLU 走 CUDA Core，element-mul 走 CUDA Core | MMA + CUDA Core | 与 MMA 重叠 |
| FP4/INT4 Dequant | 查表法（LUT）或 bit-serial 展开 | CUDA Core | ~20-40 周期 |
| MoE Top-k Routing | 轻量矩阵乘（MMA）+ Argmax/Softmax（CUDA Core） | MMA + CUDA Core | 与主 GEMM 重叠 |

---

## 9. 量化与稀疏格式支持

### 9.1 可配置数值单元（CNU：Configurable Numerical Unit）

MMA Engine 和 CUDA Core 均支持通过配置寄存器动态切换精度：

| 精度模式 | 配置位 | MMA 吞吐量 | 适用场景 |
|----------|--------|-----------|----------|
| FP16 × FP16 → FP32 | `0b000` | 1.0× | 训练/高精度推理 |
| BF16 × BF16 → FP32 | `0b001` | 1.0× | 训练/推理 |
| FP8 (E4M3) × FP8 → FP32 | `0b010` | 2.0× | 推理（权重+激活） |
| FP8 (E5M2) × FP8 → FP32 | `0b011` | 2.0× | 推理（梯度） |
| INT8 × INT8 → INT32 | `0b100` | 2.0× | 量化推理 |
| FP4 (NVFP4) × FP4 → FP32 | `0b101` | 4.0× | 极限量化推理 |
| 2:4 结构化稀疏 FP16 | `0b110` | 2.0× (等效) | 稀疏推理 |
| 保留 | `0b111` | - | 未来扩展 |

### 9.2 量化数据流（加载时转换）

```
GMEM (FP4/INT4 packed)
 ↓ tma.load + dtype_conv
SMEM (FP16/BF16 unpacked)
 ↓ mma.async
TMEM (FP32 accum)
 ↓ cuda.quant (Epilogue)
GMEM (FP4/INT4 store)
```

- **TMA Engine 集成解压**：加载时从 DRAM 读取 4-bit/8-bit 数据，硬件解压为 16-bit 写入 SMEM，消除 SMEM 容量压力
- **Per-channel / Per-token Scale**：Scale 向量存储于独立 SMEM 区域，由 CUDA Core 在 Epilogue 阶段应用

### 9.3 稀疏格式支持

#### 结构化稀疏（2:4）

- **硬件支持**：MMA Engine 的 MAC 阵列支持通过 `sparse_mask` 寄存器跳过零值乘法
- **数据布局**：K/V Cache 以 2:4 压缩格式存储，TMA 加载时自动展开为稠密 Tile（或 MMA 直接消费压缩格式）
- **收益**：等效 2× 吞吐量，或 50% SMEM 节省

#### 非结构化 / 块稀疏

- **第一版硬件**：不直接支持，通过软件预处理（重排、分块）适配
- **ISA 预留**：`sparse_desc` 字段预留，未来可支持块稀疏索引（Block Sparse Index）

---

## 10. 配置参数与面积功耗模型

### 10.1 典型配置（推理专用）

| 参数 | 配置 A（边缘） | 配置 B（云端） | 配置 C（集群节点） |
|------|---------------|---------------|------------------|
| Cluster 数量 | 2 | 8 | 32 |
| 每 Cluster SM 数 | 4 | 8 | 8 |
| 总 SM 数 | 8 | 64 | 256 |
| MMA Engine | 8× 64×64 MAC | 64× 128×128 MAC | 256× 128×128 MAC |
| CUDA Core/Warp | 2 Warps/SM | 4 Warps/SM | 4 Warps/SM |
| SMEM/SM | 128 KB | 228 KB | 228 KB |
| TMEM/SM | 128 KB | 256 KB | 256 KB |
| 峰值 (FP16) | 50 TFLOPS | 800 TFLOPS | 3.2 PFLOPS |
| 峰值 (FP8) | 100 TFLOPS | 1.6 PFLOPS | 6.4 PFLOPS |
| TDP (估计) | 15W | 300W | 1200W |
| 制程 | 5nm | 4nm | 4nm |

### 10.2 面积估算（相对 GPU）

| 优化项 | 节省面积 |
|--------|----------|
| 无 L1/L2 Cache | ~25% |
| 无通用 RF（大幅削减） | ~20% |
| 无 SIMT 堆栈/发散逻辑 | ~10% |
| TMA/MMA 专用化（非通用） | ~15% |
| **总计** | **同工艺下，单位 SM 面积约为 GPU SM 的 30-40%** |

同等面积下可集成 **2.5-3 倍 SM 数量**。

### 10.3 功耗优化

- **GALS（全局异步局部同步）**：各 SM 独立时钟域，空闲时自动降频
- **TMA 与 MMA 时钟解耦**：数据搬运与计算可运行在不同频率
- **细粒度电源门控**：TMEM 未使用时可部分关闭
- **稀疏模式动态关断**：2:4 稀疏时，50% MAC 阵列自动断电

---

## 11. 软件栈与编程模型

### 11.1 层级抽象

```
PyTorch / JAX / TensorFlow
 ↓
Triton / TileLang / CUTLASS-like DSL
 ↓
Systolic-Flow Runtime (SFRT)
 ├─ Kernel Launcher (管理 Cluster/SM 分配)
 ├─ TMA Descriptor 构建器
 ├─ Barrier 管理器
 └─ Profile-Guided Tuning (Tile Size / K-depth / Pipeline Stage)
 ↓
Systolic-Flow ISA (本文档)
 ↓
硬件微码
```

### 11.2 Kernel 编写示例（FlashAttention-2）

```python
# 伪代码：类似 Triton/CuTe 的 DSL
@sf_kernel
def flash_attention_fwd(Q, K, V, O):
    # 角色绑定（编译时静态）
    producer = bind_warp(role=PRODUCER, id=0)
    mma = bind_warp(role=MMA, id=1)
    softmax = bind_warp(role=SOFTMAX, id=2)
    epilogue = bind_warp_group(role=EPILOGUE, ids=[4,5,6,7])

    # Tile 配置（软件定义）
    Br, Bc, d = 128, 128, 128  # 可配置

    # Cluster 协作配置
    cluster = Cluster(size=4)  # 4 SM 协作

    # TMA 描述符
    q_desc = TMA_Desc(Q, shape=(Br, d), dtype=fp16)
    k_desc = TMA_Desc(K, shape=(Bc, d), dtype=fp16)

    # 流水线启动
    pipeline = AsyncPipeline(stages=3, scope=scope_cluster)

    # Producer: Q Broadcast + K/V 分片加载
    producer.async_load(SMEM_Q, q_desc, multicast=cluster.mask)
    for kv_block in range(N // Bc):
        producer.async_load(SMEM_K[kv_block % 3], k_desc[kv_block])
        producer.async_load(SMEM_V[kv_block % 3], v_desc[kv_block])
        producer.arrive(pipeline.barrier_load)

    # MMA: 双层脉动执行
    mma.async_mma(TMEM_S, SMEM_Q, SMEM_K[0], acc=ZERO)
    for kv_block in range(N // Bc):
        mma.wait_group(kv_block - 1)  # 异步等待
        mma.async_mma(TMEM_O, TMEM_P, SMEM_V[kv_block % 3], acc=TMEM_O)
        mma.async_mma(TMEM_S, SMEM_Q, SMEM_K[(kv_block+1) % 3], acc=ZERO)

    # Softmax: 与 MMA 重叠
    softmax.online_softmax(TMEM_S, TMEM_O, TMEM_m, TMEM_l)

    # Cluster 归约（宏脉动）
    cluster.online_softmax_reduce(TMEM_O, TMEM_m, TMEM_l)

    # Epilogue: 写回 + 量化
    epilogue.async_store(O, TMEM_O, quant=fp8)
```

### 11.3 自动调优空间

编译器/运行时自动搜索以下参数：

| 参数 | 范围 | 说明 |
|------|------|------|
| `Br, Bc, d` | 受 SMEM/TMEM 容量约束 | Tile Shape |
| `pipeline_stages` | 2 ~ 5 | 流水线级数 |
| `cluster_size` | 1/2/4/8 SM | 受归约延迟约束 |
| `mma_shape` | 64×64, 128×128 等 | MAC 阵列的有效配置 |
| `sparse_mode` | 稠密 / 2:4 / 块稀疏 | 稀疏模式 |

---

## 12. 验证与调试建议

### 12.1 硬件验证指标

| 指标 | 定义 | 目标 |
|------|------|------|
| **MMA 利用率** | `mma_active_cycles / total_cycles` | > 95% |
| **TMA 带宽** | `bytes_transferred / TMA_active_cycles` | > 80% |
| **气泡率** | `barrier_wait_cycles / total_cycles` | < 3% |
| **TMEM 冲突** | `tmem_bank_conflict_stalls` | < 1% |

### 12.2 软件模拟器

建议构建周期级模拟器（Cycle-level Simulator），支持：

- 精确建模 `mma.async` 的延迟与吞吐
- 模拟 TMA 的异步完成与 barrier 触发
- 验证 Cluster 级归约的时序正确性
- 支持 "what-if" 分析（不同 Tile Size、不同 Cluster Size）

### 12.3 验证层次

```
┌─────────────────────────────────────────┐
│           A. 单元测试层                  │
│  - PE MAC 功能正确性                    │
│  - 微脉动数据流无气泡                   │
│  - 指令语义正确性                       │
├─────────────────────────────────────────┤
│           B. 集成测试层                  │
│  - SM 内部 5-Way Warp 流水线            │
│  - Cluster 内多 SM 协作                 │
│  - TMEM/SMEM 数据一致性                 │
├─────────────────────────────────────────┤
│           C. 端到端测试层                │
│  - FlashAttention-2 前向/反向           │
│  - 完整 LLM 推理 trace                  │
│  - 性能指标回归测试                      │
└─────────────────────────────────────────┘
```

---

## 13. 演进路线图

| 版本 | 特性 | 目标 |
|------|------|------|
| **v1.0** | 双层脉动 + 5-Way Warp Spec + FP16/BF16/FP8 | FlashAttention-2/3 满血运行 |
| **v1.5** | 增加 MLA (Multi-head Latent Attention) 原生支持 | DeepSeek 系列模型优化 |
| **v2.0** | 支持 FP4/INT4 + 结构化稀疏 | 极限量化推理 |
| **v2.5** | 增加 Speculative Decoding 硬件支持 | 推理延迟优化 |
| **v3.0** | 支持 MoE 专家路由硬件加速 | Mixtral / Qwen-MoE 优化 |

---

## 附录A 术语表

| 术语 | 定义 |
|------|------|
| **TMA** | Tensor Memory Accelerator，异步张量搬运引擎 |
| **TMEM** | Tensor Memory，专用矩阵累加器存储 |
| **SMEM** | Shared Memory，SM 内多 Bank SRAM |
| **DSMEM** | Distributed Shared Memory，Cluster 内 SM 间共享存储 |
| **MMA** | Matrix Multiply-Accumulate，矩阵乘加引擎 |
| **mbarrier** | Memory Barrier，显式到达-等待同步原语 |
| **Warp Specialization** | Warp 角色静态绑定，实现生产者-消费者流水线 |
| **Tile** | 张量分块，如 Q Tile (Br×d)、K Tile (Bc×d) |
| **Micro-Systolic** | Tile-Intra 层，SM 内数据流式计算 |
| **Macro-Systolic** | Tile-Inter 层，Cluster 内多 SM 协作归约 |
| **Online Softmax** | 增量计算 softmax，避免物化完整 S 矩阵 |
| **2:4 Sparse** | 每 4 个元素中 2 个非零的结构化稀疏 |

---

## 附录B 参考架构对比

| 特性 | Hopper H100 | Blackwell B200 | 本架构 (Systolic-Flow DSA) |
|------|-------------|----------------|--------------------------|
| TMA | ✅ | ✅ (增强) | ✅ (简化，无虚拟内存) |
| TMEM | ❌ | ✅ (256KB/SM) | ✅ (256KB/SM，简化寻址) |
| tcgen05.mma | ❌ | ✅ | ✅ (语义兼容，硬件简化) |
| 2-CTA MMA | ❌ | ✅ | ✅ (Cluster 协作) |
| Warp Spec | ✅ (FA3) | ✅ (FA4, 5-way) | ✅ (5-way，角色固定) |
| CUDA Core | ✅ (通用) | ✅ (通用) | ⚠️ (轻量，仅非矩阵运算) |
| L1/L2 Cache | ✅ (复杂) | ✅ (复杂) | ❌ (完全无 Cache) |
| SIMT | ✅ (完整) | ✅ (完整) | ❌ (无发散，无上下文切换) |
| 面积效率 | 基准 | ~1.3× | **~2.5-3× (估计)** |

---

## 文档版本历史

| 版本 | 日期 | 作者 | 变更内容 |
|------|------|------|----------|
| v1.0 | 2026-06-14 | CTO | 初始架构设计文档 |

---

**本文档为 ChipForge 项目双层脉动矩阵乘 IP 的架构设计文档，位于 `ip/double_layer_systolic_matmul/docs/` 目录下。**