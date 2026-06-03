# CPU RTL 实现

## 概述
基于 CppHDL 框架的 RISC-V CPU RTL 模型，与 TLM 版本共享 Bundle 定义。

## 开发阶段
**当前状态：Phase 5 规划中**

## 设计目标
- 与 TLM 版本行为一致（COMPARE 模式验证）
- 支持 Verilog 生成和 FPGA 综合
- Plugin 分层：纯组合逻辑 Plugin 优先迁移

## Bundle 共享
RTL 版本复用 TLM 定义的 Bundle 类型：
- `MemReqBundle`：内存请求
- `MemRespBundle`：内存响应
- 流水线内部：通过 CppHDL 的 `Wire<T>` 表达
