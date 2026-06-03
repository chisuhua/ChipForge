# Peripheral IP 设计文档

## 1. 功能概述

### 1.1 模块定位
Peripheral IP 包含 SoC 的基础外设模块，提供中断控制、定时器、串口通信等系统服务。

### 1.2 包含模块
| 外设 | 说明 | 标准 |
|------|------|------|
| PLIC | 平台级中断控制器 | RISC-V PLIC Spec |
| CLINT | 核心本地中断控制器 | RISC-V Privileged Spec |
| UART | 串口通信 | 16550 兼容 |
| Timer | 系统定时器 | mtime/mtimecmp |
| GPIO | 通用 I/O（可选） | - |

### 1.3 性能目标
| 指标 | 目标值 | 说明 |
|------|--------|------|
| 寄存器访问 | 1 cycle | 读写延迟 |
| 中断响应 | ≤ 3 cycles | PLIC → CPU |
| UART 波特率 | 115200-3M | 可配置 |

## 2. 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 外设模型 |
| `rtl/` | CppHDL RTL 实现 |
| `test/` | 外设验证套件 |
| `configs/` | 外设配置（基地址、中断号等） |

## 3. 接口设计

### 3.1 统一寄存器访问接口
| 端口名 | 方向 | 类型 | Bundle | 说明 |
|--------|------|------|--------|------|
| reg_req | in | ch_stream | MemReqBundle | 寄存器读写请求 |
| reg_resp | out | ch_stream | MemRespBundle | 寄存器读写响应 |

### 3.2 中断输出
| 端口名 | 方向 | 类型 | 说明 |
|--------|------|------|------|
| irq_out | out | signal | 中断请求信号 |
| timer_irq | out | signal | 定时器中断 |
| sw_irq | out | signal | 软件中断 |

## 4. 配置参数

### PLIC 配置
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| base_addr | hex | 0x0C000000 | 基地址 |
| num_sources | int | 32 | 中断源数量 |
| num_targets | int | 1 | 中断目标（核心）数量 |
| num_priorities | int | 7 | 优先级级别 |

### CLINT 配置
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| base_addr | hex | 0x02000000 | 基地址 |
| num_harts | int | 1 | Hart 数量 |
| timebase_freq | int | 10000000 | 时间基准频率 (Hz) |

### UART 配置
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| base_addr | hex | 0x10000000 | 基地址 |
| baud_rate | int | 115200 | 波特率 |
| fifo_depth | int | 16 | FIFO 深度 |

## 5. 测试方法

### Level A - 单元测试
- 各外设寄存器读写正确性
- 中断触发/清除逻辑
- 定时器计数精度

### Level B - 集成测试
- CPU 通过总线访问外设
- 中断响应完整流程
- UART 收发回环

### Level C - 系统测试
- RTOS 中断驱动任务调度
- Linux 设备驱动兼容性

## 6. 相关文档
- [项目架构总览](../docs/architecture/overview.md)
- [接口设计详解](../docs/architecture/interface-design.md)
