# Phase 1：基础框架搭建

> **⚠️ 已废弃 (DEPRECATED) — 2026-06-08**
>
> 本文档已被 [phase-1-tlm-foundation.md](phase-1-tlm-foundation.md) 取代。
>
> **废弃原因**:
> - 原 Phase 1 任务基于"先做 CppTLM/CppHDL 框架集成,再做 L1Cache"线性推进
> - 新 Phase 1 任务基于 D4 决策,采用 **Plugin-style 设计**(无 `tick()`、无状态机、Bundle 用 `uint_t<N>`)
> - 新增 Phase 0(Plugin 最小脚手架)作为前置依赖
>
> **替代文档**: [phase-1-tlm-foundation.md](phase-1-tlm-foundation.md) — 第一个 Plugin-style IP = L1CachePlugin
>
> **保留本文件的原因**: 历史参考。如需了解原始 Phase 1 任务设计,可阅读本文档;但实施请遵循新文档。
>
> **决策依据**: [`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../../.omo/drafts/decision-plugin-framework-2026-06-08.md) (D1-D11)

---

# Phase 1：基础框架搭建

> **Status**: Not Started
> **Milestone**: M1 - Hello World
> **Depends on**: None

**目标**：构建可运行 bare-metal "Hello World" 的 TLM 虚拟平台

---

## 任务清单

### 1. Bundle 层定义

- [ ] `MemReqBundle` / `MemRespBundle`
- [ ] `CacheReqBundle` / `CacheRespBundle`
- [ ] `NoCPacketBundle`
- [ ] `IntBundle`
- [ ] `ImplMode` 枚举

### 2. CppTLM 基础组件

- [ ] `RiscvIssTlm`：封装 Spike ISS，暴露 `ch_stream` 指令/数据总线
- [ ] `DramTlm`：DRAM 模型，支持 DMI 加速大块内存访问
- [ ] `RomTlm`：只读程序/固件存储
- [ ] `BusMatrixTlm`：地址解码与路由
- [ ] `UartTlm`：NS16550A，支持轮询和中断两种模式
- [ ] `ClintTlm`：`mtime` 计数器 + `mtimecmp` 定时中断
- [ ] `PlicTlm`：多优先级外部中断，支持 S-mode

### 3. HTIF 退出机制

- [ ] 监控 `tohost` 地址写入
- [ ] `tohost = 1`：PASS，`tohost` 为奇数：FAIL（`val >> 1` 为失败测试号）
- [ ] Syscall 代理（`write` 系统调用用于 console 输出）

### 4. SoC 顶层装配

- [ ] `RiscvVirtSoC`：连接上述所有组件
- [ ] ELF 加载器（解析 ELF，加载到 DRAM）

### 5. 统计收集框架基础

- [ ] `metrics/statistics.h`：Scalar、Distribution 基本类型
- [ ] `metrics/stat_group.h`：层次化统计组
- [ ] 每个 TLM 组件注册基本统计（hits/misses/latency）
- [ ] JSON 格式统计导出

### 6. JSON 配置系统集成

- [ ] 利用 CppTLM ModuleFactory 实现配置驱动的 SoC 组装
- [ ] 编写 `configs/soc/riscv_virt.json` 默认配置
- [ ] 支持命令行指定配置文件启动仿真

### 7. 验证

- [ ] 运行第一个 bare-metal 程序，UART 输出 `Hello, RISC-V!`

