# MMU IP 文档索引

| 文档 | 说明 |
|------|------|
| [README.md](../README.md) | IP 总览（模块定位/目录结构/接口设计/可插拔策略/配置参数） |
| [STATUS.md](../STATUS.md) | 当前状态（PARTIAL 骨架阶段，2026-06-29） |
| [architecture.md](architecture.md) | **MMU 总架构**（跨 **L0 TLB / L1 TLB / PTW** 三单元的数据流与时序 + 外部对接：satp CSR / fetch-stall protocol / 物理内存 / SoC JSON / TLB↔PTW 接口）|
| [mmu-l0-tlb-architecture.md](mmu-l0-tlb-architecture.md) | **L0 TLB 架构**（8 项全相联 · 多粒度 · perms + `page_shift` · 1 拍快路径）|
| [mmu-l1-tlb-architecture.md](mmu-l1-tlb-architecture.md) | **L1 TLB 架构**（256 set × 8 way 组相连 · XOR interleave（基准 16MiB）· 容量与节点预算）|
| [mmu-ptw-architecture.md](mmu-ptw-architecture.md) | **MMU_PTW 架构**（ADR-085 设计正文：walk FSM / PTWCache 目录项缓存 / 编码判据 / superpage）|
| [mmu-tlm-architecture.md](mmu-tlm-architecture.md) | **TLM 侧微架构**（MultiLevelTLB 编排 · lib/tlm 分层 · HDL 友好性 · 工厂模式 / 替换策略）|
| [configuration.md](configuration.md) | 参数 Knobs 详表（对应 `configs/params_schema.json`） |
| [integration.md](integration.md) | CPU/SoC 集成契约（MMUPlugin at_stage 示例） |
| [adr/ADR-085-mmu-ptw-architecture.md](adr/ADR-085-mmu-ptw-architecture.md) | **ADR-085 伞索引**（MMU_PTW 架构决策组，登记 D01–D21） |
| [adr/ADR-085-D01..D21](adr/) | **21 个子 ADR**（一决策一文件：`ADR-085-D01-mmu-tlb-ptw-split.md` … `ADR-085-D21-physical-mem-handle-ownership.md`） |
| [adr/ADR-086-l0-l1-tlb-architecture.md](adr/ADR-086-l0-l1-tlb-architecture.md) | **ADR-086 伞索引**（L0 / L1 TLB 架构决策组，登记 D01–D12；ADR-085 的 CH_MEM 侧增量） |
| [adr/ADR-086-D01..D12](adr/) | **12 个子 ADR**（一决策一文件：`ADR-086-D01-l0-l1-leaf-cache-split.md` … `ADR-086-D12-verification-criteria.md`） |

## 状态标签

| 标签 | 含义 |
|------|------|
| 🟡 骨架阶段 | 目录/接口/Config 落地，算法推迟到 `mmu-tlb-ptw-impl` |
| 🟢 实施中 | TLB/PTW 算法实装（下一 change） |
| ✅ 稳定 | 算法实装 + 集成测试通过 + HDL 转换可行 |

## 目录结构（与 `docs/templates/IP_TEMPLATE.md` 对齐）

```
ip/mmu/
├── README.md                # 总览
├── STATUS.md                # 状态
├── lib/                     # 纯 C++ 算法层（HDL 友好）
│   ├── tlb_entry.h
│   ├── tlb_lookup.h
│   ├── tlb_base.h
│   ├── tlb.h                # 模板化
│   ├── tlb_factory.h/.cpp
│   ├── multi_level_tlb.h
│   └── ptw.h
├── plugins/                 # CH_MEM 后端（ADR-083 §2.4 双文件分离；CI 只扫此目录）
│   ├── MMUPlugin_chmem.h    # MMU_TLB 主体（8 项全相联 TLB）
│   └── mmu_ptw_chmem.h      # MMU_PTW（PTWCache + walk FSM）—— 规划中，见 ADR-085
├── tlm/                     # TLM 后端（Plugin 层）
│   ├── mmu_keys.h
│   └── MMUPlugin.h/.cpp
├── rtl/                     # CppHDL（Phase 5+）
├── configs/
│   └── params_schema.json   # JSON Schema draft-07
├── policies/                # 替换策略
│   ├── tlb_replacement_policy.h
│   ├── tlb_replacement_policy.cpp
│   ├── no_replacement_policy.h
│   ├── fifo_policy.h
│   ├── lru_policy.h
│   └── rrip_policy.h
├── test/                    # 预留
└── docs/                    # 本目录
    ├── architecture.md       # MMU 总架构（跨 L0/L1/PTW 数据流 + 外部对接）
    ├── mmu-l0-tlb-architecture.md   # L0 TLB 架构（ADR-086 组）
    ├── mmu-l1-tlb-architecture.md   # L1 TLB 架构（ADR-086 组）
    ├── mmu-ptw-architecture.md      # MMU_PTW 架构（ADR-085 组）
    ├── mmu-tlm-architecture.md      # TLM 侧微架构（MultiLevelTLB + lib/tlm 分层）
    └── adr/                  # ADR-085 / ADR-086 伞索引 + 33 个子 ADR（一决策一文件）
```
