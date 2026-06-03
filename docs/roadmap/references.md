# 参考资源

## 核心框架

| 资源 | 说明 |
|------|------|
| [CppTLM](https://github.com/chisuhua/CppTLM) | 事务级建模框架（本项目 TLM 层） |
| [CppHDL](https://github.com/chisuhua/CppHDL) | C++ 硬件描述框架（本项目 RTL 层） |

## RISC-V 参考实现

| 资源 | 说明 |
|------|------|
| [VexRiscv](https://github.com/SpinalHDL/VexRiscv) | 插件化 RV32，FPGA 优化 |
| [CVA6](https://github.com/openhwgroup/cva6) | RV64GC 应用级核，完整验证环境 |
| [Spike](https://github.com/riscv-software-src/riscv-isa-sim) | 官方 ISS，黄金参考 |

## 验证工具

| 资源 | 说明 |
|------|------|
| [riscv-tests](https://github.com/riscv/riscv-tests) | 官方 ISA 单元测试 |
| [riscv-arch-test](https://github.com/riscv/riscv-arch-test) | 官方合规测试套件 |
| [RISCOF](https://riscof.readthedocs.io) | 合规认证框架文档 |
| [riscv-dv](https://github.com/chipsalliance/riscv-dv) | 随机指令生成器 |
| [core-v-verif](https://github.com/openhwgroup/core-v-verif) | CVA6/CV32E40P 验证框架参考 |

## 软件栈

| 资源 | 说明 |
|------|------|
| [OpenSBI](https://github.com/riscv-software-src/opensbi) | M-mode 固件，SBI 规范实现 |
| [FreeRTOS RISC-V](https://github.com/FreeRTOS/FreeRTOS) | FreeRTOS 官方 RISC-V 移植 |
| [Zephyr](https://github.com/zephyrproject-rtos/zephyr) | Zephyr RTOS，HWMv2 架构 |
| [Buildroot](https://buildroot.org) | Linux 根文件系统构建 |

## 技术标准

| 资源 | 说明 |
|------|------|
| RISC-V ISA Spec v20191213 | 非特权级 ISA 规范 |
| RISC-V Privileged Spec v1.12 | 特权级规范（M/S/U mode，PMP，虚拟内存） |
| SBI Spec v1.0 | Supervisor Binary Interface 规范 |
| OSCI TLM-2.0 LRM | TLM 标准参考（CppTLM 设计参考） |

