# VexRiscvArch (Deprecated)

> ⚠️ **DEPRECATED** — 此文档记录早期设计探索,实现请以 [multi_isa_architecture.md](../multi_isa_architecture.md) 为准

## 历史定位

v1.x 时期的 VexRiscvArch 探索,记录 RISC-V CPU IP 设计的早期思路。

## 废弃原因

- 已被 [multi_isa_architecture.md](../multi_isa_architecture.md) v2.0 完全替代
- PipeNode/PipeLink/PipeBuilder 声明式模型更适合现代 C++ 硬件描述
- 项目处于纯规划阶段(无已发布产物),不保留兼容性

## 保留目的

供理解架构演进历史,**代码不复用**。