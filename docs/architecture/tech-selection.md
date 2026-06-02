# 核心技术选型

| 维度 | 选型 | 角色 | 适用阶段 |
|------|------|------|---------|
| TLM 框架 | CppTLM (`ch_stream<T>`) | 高速功能建模，事务追踪 | 全程 |
| RTL 框架 | CppHDL (`Component`) | 周期精确建模 + Verilog 生成 | Phase 5+ |
| 共享接口 | Bundle (`ch_uint<N>`, `ch_bool`) | TLM/RTL 统一数据结构 | 全程 |
| ISS 参考 | Spike | TLM ISS 核 + co-sim 黄金参考 | 全程 |
| 合规测试 | riscv-arch-test + RISCOF | 官方认证框架 | Phase 2 |
| 随机测试 | riscv-dv | 随机指令生成 + 执行迹对比 | Phase 5 |
| RTOS | FreeRTOS + Zephyr | 实时操作系统验证 | Phase 3 |
| Linux 引导 | OpenSBI + U-Boot + Linux | 完整 Linux 启动链 | Phase 4 |
| RTL 仿真 | CppHDL 直仿 -> Verilator | 先 C++ 仿真，后 Verilog | Phase 5 |
| RTL 参考 | VexRiscv / CVA6 | 实现参考与对标 | Phase 5 |
| 构建系统 | CMake >= 3.16 | 统一构建 | 全程 |
| 可扩展性 | 独立组件库 + SoC 组合层 | 支持 GPU 等多芯片形态 | 架构设计 |

