# MMU IP 文档索引

| 文档 | 说明 |
|------|------|
| [README.md](../README.md) | IP 总览（模块定位/目录结构/接口设计/可插拔策略/配置参数） |
| [STATUS.md](../STATUS.md) | 当前状态（PARTIAL 骨架阶段，2026-06-29） |
| [architecture.md](architecture.md) | 详细微架构（多级 TLB 数据流 + lib/tlm 分层 + Plugin 范式映射） |
| [mmu-ptw-architecture.md](mmu-ptw-architecture.md) | **MMU_PTW 架构设计**（设计正文：背景 / 职责边界 / 决策索引 / 数据流与时序 / 替代方案 / 后果 / 实施清单 / 验证命令） |
| [configuration.md](configuration.md) | 参数 Knobs 详表（对应 `configs/params_schema.json`） |
| [integration.md](integration.md) | CPU/SoC 集成契约（MMUPlugin at_stage 示例） |
| [adr/ADR-085-mmu-ptw-architecture.md](adr/ADR-085-mmu-ptw-architecture.md) | **ADR-085 伞索引**（MMU_PTW 架构决策组，登记 D01–D21） |
| [adr/ADR-085-D01..D21](adr/) | **21 个子 ADR**（一决策一文件：`ADR-085-D01-mmu-tlb-ptw-split.md` … `ADR-085-D21-physical-mem-handle-ownership.md`） |

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
    ├── architecture.md       # 既有微架构（部分内容被 ADR-085 细化）
    ├── mmu-ptw-architecture.md  # ADR-085 组的设计正文
    └── adr/                  # ADR-085 伞索引 + 21 个子 ADR（一决策一文件）
```
