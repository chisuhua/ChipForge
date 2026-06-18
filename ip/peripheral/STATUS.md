# STATUS: PLANNED (0 LOC)

This IP is **planned but not yet implemented**. No source code exists.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PLANNED variant) -->

## Implementation Roadmap
- 实施预计: Phase 3+ (见 docs/roadmap/phases/phase-3-rtos.md)
- 依赖: interconnect (Phase 2+); 为 cpu 提供中断
- 状态: 🔴 规划中
- 子模块: PLIC (Platform-Level Interrupt Controller) / CLINT (Core Local Interruptor, mtime+mtimecmp) / UART (NS16550A) / Timer

## 已知限制
- 所有子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- 当前 IP 根仅含 `README.md` (无任何源码)
- Phase 2 之前不会有任何实施
- 暂时复用 CppHDL 的 `axi4/peripherals/axi_uart.h` 等参考实现
