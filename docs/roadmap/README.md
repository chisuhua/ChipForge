# RISC-V TLM 建模与验证平台 — 实施路线

> 基于 [CppTLM](https://github.com/chisuhua/CppTLM) + [CppHDL](https://github.com/chisuhua/CppHDL)
> 支持 Bare-metal / RTOS / Linux 三级测试框架，可扩展至 GPU 等多芯片形态

---

## 文档导航

### 架构设计文档

| 文档 | 说明 |
|------|------|
| [背景与目标](../architecture/background-and-goals.md) | 项目目标、核心约束、参考项目调研 |
| [核心技术选型](../architecture/tech-selection.md) | TLM/RTL 框架、ISS、测试工具等选型决策 |
| [总体架构设计](../architecture/overview.md) | 架构总览、设计原则、工程目录结构 |
| [接口设计详解](../architecture/interface-design.md) | Bundle 定义、TLM/RTL 组件接口、SoC 组合层、内存地图 |
| [测试与 DSE 框架](../architecture/testing-and-dse.md) | 三级测试分层、HTIF 协议、设计空间探索框架 |

### 实施阶段

| Phase | 文档 | 状态 |
|-------|------|------|
| Phase 1 | [基础框架搭建](phases/phase-1-foundation.md) | Not Started |
| Phase 2 | [Bare-metal 测试套件](phases/phase-2-baremetal.md) | Not Started |
| Phase 3 | [RTOS 测试套件](phases/phase-3-rtos.md) | Not Started |
| Phase 4 | [Linux 启动支持](phases/phase-4-linux.md) | Not Started |
| Phase 5 | [RTL 协同验证 + Verilog 生成](phases/phase-5-rtl.md) | Not Started |

### 其他

| 文档 | 说明 |
|------|------|
| [参考资源](references.md) | 核心框架、RISC-V 实现、验证工具、软件栈、技术标准 |

---

## 里程碑定义

| 编号 | 名称 | 完成标准 | 对应 Phase |
|------|------|----------|-----------|
| **M1** | Hello World | TLM 平台运行 bare-metal，UART 输出正常，HTIF 退出正常 | Phase 1 |
| **M2** | ISA 全覆盖 | riscv-tests RV64GC 全部 PASS；RISCOF 合规认证通过 | Phase 2 |
| **M3** | FreeRTOS 运行 | 多任务调度正常；定时器中断、信号量、队列测试全部通过 | Phase 3 |
| **M4** | Zephyr 运行 | 内核启动；UART/Timer 驱动测试通过；线程调度稳定 | Phase 3 |
| **M5** | Linux 启动 | OpenSBI -> Linux 完整引导；Shell 可交互；VirtIO Block 挂载 rootfs | Phase 4 |
| **M6** | RTL 协同验证 | CppHDL L1 Cache RTL 与 TLM 在 COMPARE 模式下执行迹完全一致 | Phase 5 |
| **M7** | Verilog 生成 | CppHDL -> Verilog -> Verilator 仿真；与 C++ 直仿执行迹一致 | Phase 5 |
| **M8** | 多芯片扩展 | 复用组件库装配 GpuSoC；验证跨芯片形态可复用性（可选） | Phase 5 |
| **M9** | DSE 框架可用 | 参数扫描工作流端到端运行；缓存策略 DSE 产出 Pareto 分析报告 | Phase 2-3 |
| **M10** | 多 ISA 支持 | 新增 CPU IP 暴露统一 ch_stream 接口；同一 SoC JSON 配置可切换不同 ISA 核心 | Phase 5 |

