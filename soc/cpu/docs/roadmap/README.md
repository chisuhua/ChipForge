# RISC-V CPU SoC 实施路线图

> **属主**：`soc/cpu/` — RISC-V CPU 中心 SoC（CPU + MMU + Cache + Memory + Interconnect + Peripheral）  
> **框架级 Phase（0、6）在**：`docs/roadmap/phases/`  
> **全局路线图入口**：`docs/roadmap/README.md`  
> **最后更新**：2026-09-15（`plugin-framework-stall` v0.1.3 归档 + Phase 1.5 wave 计划启动）

## 阶段总览

| Phase | 文档 | 状态 | 关键目标 |
|-------|------|------|---------|
| Phase 1 | [基础 TLM 平台（Hello World = L1CachePlugin）](phase-1-tlm-foundation.md) | ✅ 核心完成（L1Cache + MMU + CPU Pipeline） | TLB/PTW 实装 + VIPT 集成 + CpuFactory 注册 |
| Phase 1.5 | [Stall 兑现 + 端到端验证](phase-1.5-stall-and-validate.md) | 🚧 启动（plugin-framework-stall 归档后） | RV32I 合规基线 + SoC demo + DSE |
| Phase 2 | [Bare-metal 测试套件](phase-2-baremetal.md) | 未开始 | SoC 完整装配 + riscv-tests RV64GC |
| Phase 3 | [RTOS 测试套件](phase-3-rtos.md) | 未开始 | FreeRTOS + Zephyr |
| Phase 4 | [Linux 启动支持](phase-4-linux.md) | 未开始 | OpenSBI + Linux Kernel |
| Phase 5 | [RTL 协同验证 + Verilog 生成](phase-5-rtl.md) | 未开始 | CppHDL RTL 与 TLM 对比 |

## 当前进展与下一里程碑

**2026-09 已归档 6 个 change（332/337 tests PASS，5 RISC-V 仿真 pre-existing fail 待解决）：**

| Change | 归档日期 | 交付内容 |
|--------|---------|---------|
| `mmu-tlb-ptw-impl` | 2026-09-12 | TLB lookup/insert/invalidate + PTW Sv39 walk + MultiLevelTLB coherence + TLBFactory 11 组特化 + MMUPlugin at_stage 5 闭包 + RiscvMMUPlugin satp/SFENCE.VMA/exception hook + MMUTLMBridge 核心 |
| `mmu-cache-integration` | 2026-09-13 | L1CachePlugin 消费 `pl::MMU_VADDR` VIPT 索引 + PIPT fallback + mmu_bridge_adapter cpptlm 集成 + soc/mmu_minimal.json 全链 + cache_keys.h + 29→39 mmu tests |
| `cpu-mmu-integration` | 2026-09-14 | RiscvMMUPlugin 注册到 CpuFactory（`enable_mmu=true`）+ at_stage 3 substage（csr_write_satp/sfence_vma/mmu_exit）+ cpu_keys.h IPC + 8 集成测试 |
| `ptw-walk-bridge-fix` | 2026-09-14 | PTW at_stage 闭包接线（修复 walk 永远 busy）+ MMUTLMBridge issue_request/read_response 实装（修复 stub）+ 3 tests |
| `cpu-pipeline-stubs-replace` | 2026-09-15 | `cpu_sim --elf add.elf` → tohost=1（5 cycles，真 RISC-V 执行）+ StageLinkPlugin 跨阶段 Payload 传播 + PC 写回 fetch 节点 + 4 个隐藏 bug 修复（branch/ALU×2） |
| `plugin-framework-stall` | 2026-09-15 | `PipeBuilder::run()` CtrlLink stall loop + `PluginException` + IBus fetch PTW-busy CtrlLink + HazardPlugin execute RAW CtrlLink + MMU PTW_ACTIVE 原子清零 + ADR-045 |

**当前 IP 状态：**
- ✅ **CPU Core**：11 Plugin 套件 + RiscvMMUPlugin 条件注册（`enable_mmu=true` 时插入 PluginOrder + 3 substage）+ 真跑 RV32I add.elf
- ✅ **L1Cache**：lookup + refill 两阶段 + Bridge + Adapter e2e + **VIPT 索引消费**（ADR-044 §3.2 契约双端完成）
- ✅ **MMU**：TLB + PTW（Sv39 端到端走通）+ VIPT 双写 + RISC-V hook + MMUTLMBridge/Adapter 真实工作 + PTW-busy → fetch stall（plugin-framework-stall）
- ✅ **Plugin 框架**：CtrlLink 4 API 全部激活（halt/throw/flush/bypass），ADR-045 立
- 🧩 **Memory / Interconnect / Peripheral**：规划中（Phase 2+，cpptlm MemoryTLM 暂时服务 SoC JSON）

**Phase 1.5 wave 计划**（详见 [phase-1.5-stall-and-validate.md](phase-1.5-stall-and-validate.md)）：

| Wave | 内容 | 估时 | 目标 |
|------|------|------|------|
| **Wave 1** | `riscv-tests-rv32ui` 接入 | 1 周 | 客观 RV32I 合规基线（红/绿矩阵） |
| **Wave 2** | `cpu-pipeline-fix-rv32ui-N` + `soc-cpu-l1-mmu-demo`（双轨） | 2 周 | 修真 bug + riscv_virt.json 重建 |
| **Wave 3** | `cache-dse-sweep` + `cpu-pipeline-multi-cycle` + `plugin-framework-cycle-precision` | 1-2 周 | DSE + 多周期 + 真 cycle 精度 |
| **Wave 4** | `mmu-sv32-sv48-ext` + `cpu-pipeline-exception` + `cpu-pipeline-mispredict` | 2 周 | Sv32/Sv48 + trap + mispredict → Phase 2 |

**Wave 1 优先级论证**：当前 5 个 pre-existing RISC-V 仿真测试 fail（add.elf 之外）无客观根因分析。先建立 riscv-tests rv32ui-p 合规基线，所有后续 change 的红/绿都基于这套客观矩阵。**1 周投资换全周期可量化**。

## 相关文档

| 文档 | 内容 |
|------|------|
| [`../architecture.md`](../architecture.md) | SoC 系统架构（数据流、IP 集成状态、跨 IP 缺口） |
| [`../../../../ip/mmu/docs/architecture.md`](../../../../ip/mmu/docs/architecture.md) | MMU 内部微架构 |
| [`../../../../docs/architecture/plugin-framework.md`](../../../../docs/architecture/plugin-framework.md) | Plugin 框架设计 |
| [`../../../../docs/roadmap/README.md`](../../../../docs/roadmap/README.md) | 全局路线图入口（含 Phase 0/6） |
