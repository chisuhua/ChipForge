# RISC-V CPU SoC 实施路线图

> **属主**：`soc/cpu/` — RISC-V CPU 中心 SoC（CPU + MMU + Cache + Memory + Interconnect + Peripheral）  
> **框架级 Phase（0、6）在**：`docs/roadmap/phases/`  
> **全局路线图入口**：`docs/roadmap/README.md`

## 阶段总览

| Phase | 文档 | 状态 | 关键目标 |
|-------|------|------|---------|
| Phase 1 | [基础 TLM 平台（Hello World = L1CachePlugin）](phase-1-tlm-foundation.md) | 🚧 进行中 (~95%) | 第一个 Plugin-style IP 验证 |
| Phase 2 | [Bare-metal 测试套件](phase-2-baremetal.md) | 未开始 | riscv-tests RV64GC |
| Phase 3 | [RTOS 测试套件](phase-3-rtos.md) | 未开始 | FreeRTOS + Zephyr |
| Phase 4 | [Linux 启动支持](phase-4-linux.md) | 未开始 | OpenSBI + Linux Kernel |
| Phase 5 | [RTL 协同验证 + Verilog 生成](phase-5-rtl.md) | 未开始 | CppHDL RTL 与 TLM 对比 |

## 当前进展

**Phase 1.3 已完成**：
- CPU Core：11 Plugin 套件（decode/execute/load-store/branch/CSR）+ CpuFactory 装配
- L1Cache：lookup + refill 两阶段 + Bridge + Adapter e2e（5 测试文件, 21 test cases）
- MMU：骨架（TLB 模板 + MultiLevelTLB + 4 替换策略 + PTW stub）

**Phase 1.4 已完成**：✅ L1CachePlugin 设计方法学复盘（[`docs/methodology/plugin-style-design-methodology-v1.md`](../../../docs/methodology/plugin-style-design-methodology-v1.md)，2026-06-13）。

**Phase 1.5 下一步**：`mmu-tlb-ptw-impl` — TLB/PTW 算法实装 + MMU↔Cache 集成 + L1Cache VIPT 升级（ADR-044）。

详见 [`soc/cpu/docs/architecture.md`](../architecture.md) §5"建议后续计划"。

## 相关文档

| 文档 | 内容 |
|------|------|
| [`../architecture.md`](../architecture.md) | SoC 系统架构（数据流、IP 集成状态、跨 IP 缺口） |
| [`../../../../ip/mmu/docs/architecture.md`](../../../../ip/mmu/docs/architecture.md) | MMU 内部微架构 |
| [`../../../../docs/architecture/plugin-framework.md`](../../../../docs/architecture/plugin-framework.md) | Plugin 框架设计 |
| [`../../../../docs/roadmap/README.md`](../../../../docs/roadmap/README.md) | 全局路线图入口（含 Phase 0/6） |
