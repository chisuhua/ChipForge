# TileCore IP

> **类型**: 加速器 / 计算核心  
> **功能**: 双层脉动矩阵乘 IP（Tile-Intra 微脉动 + Tile-Inter 宏脉动）  
> **依赖**: `tilecopy` (数据搬运)

---

## 核心功能

- **微脉动阵列 (Micro-Systolic Array)**: 单个 SM/Tile 内部的 PE 阵列，执行矩阵乘累加
- **宏脉动阵列 (Macro-Systolic Array)**: Cluster 内多 Tile 协作，通过 `tilecopy` 进行数据流动
- **5-Way Warp Specialization**: Producer / MMA / Softmax / Correction / Epilogue 静态角色分离
- **可配置数值单元 (CNU)**: FP16 / BF16 / FP8 / INT8 / FP4 动态切换

---

## 目录结构

```
tilecore/
├── tlm/                    # TLM 事务级模型
│   ├── tilecore.h
│   ├── tilecore.cpp
│   ├── warp_scheduler.h     # 5-Way Warp 调度器
│   └── mma_engine.h         # MMA 计算引擎
├── rtl/                    # RTL 实现 (Phase 5+)
├── configs/                 # 参数配置 JSON Schema
├── policies/               # 可插拔策略
├── .test/                  # 单元/集成测试
└── docs/
    └── architecture.md     # 完整架构文档
```

---

## 核心接口

```cpp
class TileCore : public tlm::TransactionPayloadExtensionBase {
public:
    // MMA 异步矩阵乘
    void mma_async(
        TMEM_ADDR dst,
        SMEM_ADDR srcA,
        SMEM_ADDR srcB,
        MMA_Shape shape,
        MMA_Dtype dtype,
        AccFlag acc = ACC_ZERO
    );
    
    // 等待 MMA 完成
    void mma_wait(GroupId group);
    
    // TMEM 原地操作
    void tmem_zero(TMEM_ADDR dst);
    void tmem_rescale(TMEM_ADDR dst, Scale scale, RowMask mask);
};
```

---

## 依赖关系

```
tilecopy (TMA Engine)
    ↓ 提供数据
tilecore (MMA Engine)
    ↓ 输出结果
tilecopy (写回 GMEM)
```

---

## 文档

- [架构文档](./docs/architecture.md) - 完整架构设计