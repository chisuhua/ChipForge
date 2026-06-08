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

| Phase | 文档 | 状态 | 关键目标 |
|-------|------|------|---------|
| **Phase 0** | [Plugin 最小脚手架](phases/phase-0-plugin-scaffolding.md) | Not Started | PluginBase / Payload<T> / PipeNode / PipeBuilder / CtrlLink |
| Phase 1 | [基础 TLM 平台（Hello World = L1CachePlugin）](phases/phase-1-tlm-foundation.md) | Not Started | 第一个 Plugin-style IP 验证 Plugin 风格可行性 |
| Phase 2 | [Bare-metal 测试套件](phases/phase-2-baremetal.md) | Not Started | riscv-tests RV64GC |
| Phase 3 | [RTOS 测试套件](phases/phase-3-rtos.md) | Not Started | FreeRTOS + Zephyr |
| Phase 4 | [Linux 启动支持](phases/phase-4-linux.md) | Not Started | OpenSBI + Linux Kernel |
| Phase 5 | [RTL 协同验证 + Verilog 生成](phases/phase-5-rtl.md) | Not Started | CppHDL RTL 与 TLM 对比 |
| **Phase 6** | [完整 PipeBuilder 框架 + RTL 生成](phases/phase-6-declarative.md) | Not Started | 完整调度算法 + RTL 生成（推迟的 Phase 1a/1b/1c）|

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

### 关键概念：Plugin 风格范式（决策 D4）

> **Plugin 是设计范式，不是工具**。所有 IP 业务逻辑从 Phase 1 开始必须采用 Plugin-style（无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`）。Phase 5/6 升级 RTL 时业务代码不重写。
>
> 详细决策依据：[`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)

### Phase 6 说明（v2.0 拆分）

原路线图 v2.0 中"Phase 1（Plugin 模型）"被拆分为两个阶段：
- **Phase 0**：Plugin 最小**脚手架**（5 个 P0 组件，2-3 周）—— 让 Plugin-style 业务逻辑能跑起来
- **Phase 6**：完整 **PipeBuilder 框架** + RTL 生成（12-20 周）—— 完整调度算法、JSON 解析、CompareDriver、ScoreBoard、RTL 集成

这种拆分确保：
- Phase 0 完成后，Phase 1-5 业务逻辑（L1CachePlugin 等）已采用 Plugin-style
- Phase 6 启动时，业务代码不需要重写
- 完整框架的投入有 Phase 1-5 的反馈

