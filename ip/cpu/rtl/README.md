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
- ★ `cf::bundles::MemReq`：内存请求
- ★ `cf::bundles::MemResp`：内存响应

> ★ **本目录当前为空**（仅本 README）⇒ **手写 RTL 副本已废止**（ADR-083 §4.1「拒绝分别维护 TLM 副本 + RTL 副本」）；★ Verilator 后端由 **Plugin-style 代码经 `ch::toVerilog()` 生成**（ADR-088 §10 目标态）⇒ **本目录不得落地手写 `.v`**。
> ★ **命名更正（ADR-092 §D7）**：旧文档写的 `MemReqBundle` / `MemRespBundle` 在代码中不存在。
- 流水线内部：通过 CppHDL 的 `Wire<T>` 表达
