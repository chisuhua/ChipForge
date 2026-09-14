# RISC-V CPU SoC 实施路线图

> **属主**：`soc/cpu/` — RISC-V CPU 中心 SoC（CPU + MMU + Cache + Memory + Interconnect + Peripheral）  
> **框架级 Phase（0、6）在**：`docs/roadmap/phases/`  
> **全局路线图入口**：`docs/roadmap/README.md`  
> **最后更新**：2026-09-14（mmu-tlb-ptw-impl / mmu-cache-integration / cpu-mmu-integration / ptw-walk-bridge-fix 全部归档后）

## 阶段总览

| Phase | 文档 | 状态 | 关键目标 |
|-------|------|------|---------|
| Phase 1 | [基础 TLM 平台（Hello World = L1CachePlugin）](phase-1-tlm-foundation.md) | ✅ 核心完成（L1Cache + MMU + CPU Pipeline） | TLB/PTW 实装 + VIPT 集成 + CpuFactory 注册 |
| Phase 1.5 | 见 [MMU/Cache 演进](#当前进展与下一里程碑) | 🚧 进行中 | 4-way VIPT / Sv32/Sv48 / DSE / stall 框架 |
| Phase 2 | [Bare-metal 测试套件](phase-2-baremetal.md) | 未开始 | SoC 完整装配 + riscv-tests RV64GC |
| Phase 3 | [RTOS 测试套件](phase-3-rtos.md) | 未开始 | FreeRTOS + Zephyr |
| Phase 4 | [Linux 启动支持](phase-4-linux.md) | 未开始 | OpenSBI + Linux Kernel |
| Phase 5 | [RTL 协同验证 + Verilog 生成](phase-5-rtl.md) | 未开始 | CppHDL RTL 与 TLM 对比 |

## 当前进展与下一里程碑

**2026-09 已归档 4 个 change（317/317 tests PASS + 4 architecture gates 全绿）：**

| Change | 归档日期 | 交付内容 |
|--------|---------|---------|
| `mmu-tlb-ptw-impl` | 2026-09-12 | TLB lookup/insert/invalidate + PTW Sv39 walk + MultiLevelTLB coherence + TLBFactory 11 组特化 + MMUPlugin at_stage 5 闭包 + RiscvMMUPlugin satp/SFENCE.VMA/exception hook + MMUTLMBridge 核心 |
| `mmu-cache-integration` | 2026-09-13 | L1CachePlugin 消费 `pl::MMU_VADDR` VIPT 索引 + PIPT fallback + mmu_bridge_adapter cpptlm 集成 + soc/mmu_minimal.json 全链 + cache_keys.h + 29→39 mmu tests |
| `cpu-mmu-integration` | 2026-09-14 | RiscvMMUPlugin 注册到 CpuFactory（`enable_mmu=true`）+ at_stage 3 substage（csr_write_satp/sfence_vma/mmu_exit）+ cpu_keys.h IPC + 8 集成测试 |
| `ptw-walk-bridge-fix` | 2026-09-14 | PTW at_stage 闭包接线（修复 walk 永远 busy）+ MMUTLMBridge issue_request/read_response 实装（修复 stub）+ 3 tests |

**当前 IP 状态：**
- ✅ **CPU Core**：11 Plugin 套件 + RiscvMMUPlugin 条件注册（`enable_mmu=true` 时插入 PluginOrder + 3 substage）
- ✅ **L1Cache**：lookup + refill 两阶段 + Bridge + Adapter e2e + **VIPT 索引消费**（ADR-044 §3.2 契约双端完成）
- ✅ **MMU**：TLB + PTW（Sv39 端到端走通）+ VIPT 双写 + RISC-V hook + MMUTLMBridge/Adapter 真实工作
- 🧩 **Memory / Interconnect / Peripheral**：规划中（Phase 2+，cpptlm MemoryTLM 暂时服务 SoC JSON）

**Phase 1.5 下一步候选（按优先级）：**

| # | Change | 范围 | 前置 |
|---|--------|------|------|
| 1 | `plugin-framework-stall` | CtrlLink::should_halt 框架消费 + PTW stall 真接线（替换 PTW_ACTIVE RETRY workaround） | 无（框架级，mmu-cache-integration Decision Q7 预留） |
| 2 | `cache-phase1.5-4way` | L1Cache 256×1 → 64×4（ADR-044 §2.5 VIPT 正式安全）+ megapage/gigapage PTW | 无 |
| 3 | `mmu-sv32-sv48-ext` | Sv32/Sv48 PTW 解码（当前仅 Sv39） | 无 |
| 4 | `cache-dse-sweep` | 12-case Pareto DSE 配置扫描 | 推荐 #2 之后（4-way 才有真实扫描对象） |
| 5 | `soc-cpu-l1-mmu-demo` | CPU+MMU+L1+Memory 完整 SoC demo（riscv_virt.json 重建，tohost 退出） | cpu-mmu-integration 已完成 ✓ 可启动 |
| 6 | `ip-memory` | 真实 SRAM/DRAM 模型（替换 cpptlm MemoryTLM） | 无 |

**推荐启动顺序**：`#5 soc-cpu-l1-mmu-demo`（最高价值，直接解锁"真 RISC-V 跑程序"）→ `#2 cache-phase1.5-4way`（VIPT 正式化）→ `#1 plugin-framework-stall`（框架补全）→ `#3/#4`（算法扩展 + DSE）。

详见 [`soc/cpu/docs/architecture.md`](../architecture.md) §5"建议后续计划"。

## 相关文档

| 文档 | 内容 |
|------|------|
| [`../architecture.md`](../architecture.md) | SoC 系统架构（数据流、IP 集成状态、跨 IP 缺口） |
| [`../../../../ip/mmu/docs/architecture.md`](../../../../ip/mmu/docs/architecture.md) | MMU 内部微架构 |
| [`../../../../docs/architecture/plugin-framework.md`](../../../../docs/architecture/plugin-framework.md) | Plugin 框架设计 |
| [`../../../../docs/roadmap/README.md`](../../../../docs/roadmap/README.md) | 全局路线图入口（含 Phase 0/6） |
