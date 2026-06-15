# TileCopy IP

> **类型**: 加速器 / 数据搬运  
> **功能**: Tile 级异步数据搬运引擎（类 NVIDIA TMA）  
> **可独立使用**: 其他 IP 可复用 tilecopy 进行数据加载

---

## 核心功能

- **TMA 异步加载**: GMEM → SMEM 异步张量加载，支持数据类型转换
- **TMA 广播**: 单 SMEM → 多 SMEM (Cluster 级 multicast)
- **cp.async**: SMEM → SMEM / SMEM → TMEM 异步拷贝
- **mbarrier 同步**: 显式到达-等待屏障，支持 scope_cta / scope_cluster
- **dtype 转换**: 加载时解压 FP4/INT4 → FP16/BF16

---

## 目录结构

```
tilecopy/
├── tlm/                    # TLM 事务级模型
│   ├── tilecopy.h          # 主类
│   ├── tilecopy.cpp
│   ├── tma_engine.h        # TMA 异步加载引擎
│   ├── barrier_controller.h # mbarrier 同步控制
│   ├── multicast_router.h  # Cluster 级广播路由
│   └── dtype_converter.h  # 数据类型转换/解压
├── rtl/                    # RTL 实现 (Phase 5+)
├── configs/                 # 参数配置 JSON Schema
├── policies/               # 可插拔策略
│   ├── ping_pong_policy.lua     # 双缓冲策略
│   └── swizzle_layout_policy.lua # Bank Swizzle 布局
├── .test/                  # 单元/集成测试
│   ├── test_tma_load.cpp
│   ├── test_barrier.cpp
│   └── test_multicast.cpp
└── docs/
    └── architecture.md     # 架构文档
```

---

## 核心接口

```cpp
class TileCopy : public tlm::TransactionPayloadExtensionBase {
public:
    // TMA 异步加载：GMEM → SMEM
    void tma_load(
        SMEM_ADDR dst,
        GMEM_ADDR src,
        TileDesc desc,
        mbarrier_id bar
    );
    
    // TMA 广播：单 SMEM → 多 SMEM
    void tma_multicast(
        SMEM_ADDR dst[],
        GMEM_ADDR src,
        ClusterMask mask,
        mbarrier_id bar
    );
    
    // cp.async：SMEM → SMEM / SMEM → TMEM
    void cp_async(ADDR dst, ADDR src, Size size, mbarrier_id bar);
    
    // mbarrier 同步原语
    void bar_init(mbarrier_id id, Count count, Scope scope);
    void bar_arrive(mbarrier_id id);
    void bar_wait(mbarrier_id id, Target target);
};
```

---

## 使用场景

| 场景 | 描述 |
|------|------|
| **被 tilecore 调用** | tilecore 的 MMA 计算前，通过 tilecopy 加载 A/B Tile 数据 |
| **独立数据预取** | 其他加速器 IP 使用 tilecopy 进行异步数据预取 |
| **Cluster 数据广播** | Q Tile 通过 tma_multicast 广播到 Cluster 内所有 SM |

---

## 文档

- [架构文档](./docs/architecture.md) - 完整架构文档（待补充）