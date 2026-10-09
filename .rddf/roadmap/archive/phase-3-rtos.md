> **⚠️ ARCHIVED 2026-09-26**：本文档作为**历史参考**保留（RTOS 移植任务清单溯源）。当前主控路线图是 [`./execution-roadmap.md`](./execution-roadmap.md)（v0.10.0 → v1.3.0）。原 Phase 3 内容覆盖映射：
>
> | 原 Phase 3 任务 | 对应 execution-roadmap.md 章节 |
> |-----------------|-------------------------------|
> | CLINT/PLIC IP | v1.0.0 PoC-7（FreeRTOS 3 任务 demo 必备） |
> | FreeRTOS 移植（多任务/信号量/中断/栈溢出） | v1.0.0 §3.2 + PoC-7（10M cycle 零异常 + 中断路径 ≥15 断言） |
> | Zephyr BSP | 推迟到 v1.x 之后远期（v1.3.0 Linux 跑通后再评估） |
> | ReplacementPolicy / PrefetchPolicy | L1CachePlugin 已支持 4 替换策略（含 RRIP）+ Wave 3 已评估；PrefetchPolicy 推迟 v1.4+ |
>
> **不再作为执行依据。**

# Phase 3：RTOS 测试套件

> **Status**: Not Started
> **Milestone**: M3 - FreeRTOS 运行 / M4 - Zephyr 运行
> **Depends on**: Phase 2

**目标**：FreeRTOS 和 Zephyr 在 TLM 平台上稳定运行

---

## 任务清单

### 1. 需要补充的外设建模

- [ ] CLINT IP（`ip/peripheral/clint/`）：精确的 `mtime` 计数器（按仿真时间递增），`mtimecmp` 定时中断触发
- [ ] PLIC IP（`ip/peripheral/plic/`）：完整多优先级中断，支持 32 个外部中断源，S-mode claim/complete 流程

### 2. FreeRTOS 移植

```c
// sw/rtos/freertos/FreeRTOSConfig.h
#define configCPU_CLOCK_HZ              50000000UL
#define configTICK_RATE_HZ              1000
#define configMTIME_BASE_ADDRESS        0x02000000UL  // CLINT mtime
#define configMTIMECMP_BASE_ADDRESS     0x02000008UL  // CLINT mtimecmp
```

测试用例：
- [ ] 多任务创建、切换、删除
- [ ] 信号量、互斥锁、队列
- [ ] 软件定时器
- [ ] 中断上下文任务唤醒
- [ ] 栈溢出检测

### 3. Zephyr BSP

```
sw/rtos/zephyr/boards/riscv/chipforge_virt/
+-- board.cmake
+-- chipforge_virt.yaml
+-- chipforge_virt_defconfig
+-- dts/chipforge_virt.dts   # 设备树：内存地图 + 外设绑定
```

测试用例：
- [ ] `kernel/` 系列测试（线程、信号量、消息队列）
- [ ] `drivers/uart` 串口驱动
- [ ] `drivers/timer` 定时器驱动

### 4. 可插拔策略实现

- [ ] 实现 `ReplacementPolicy` 接口及 LRU、PLRU、Random、FIFO 四种策略
- [ ] L1Cache Plugin 支持通过 JSON 配置切换替换策略
- [ ] 实现 `PrefetchPolicy` 接口及 Stride、NextLine 两种策略
- [ ] 统计框架完善：Distribution（延迟分布）、Formula（衍生指标）
- [ ] DSE 工具增强：`pareto_analyzer.py` 和 `sensitivity_plot.py`

