# Phase 2 baremetal-riscv-tests — Problems

> **Work**: phase-2-baremetal-riscv-tests
> **Date**: 2026-06-13
> **Status**: 启动, 任务 1 (工具链准备) blocked by external dependency

## P-1: RISC-V 工具链缺失 (任务 1 blocked by external dependency)

**日期**: 2026-06-13 13:55
**症状**: 4/5 工具未预装:
- `riscv64-unknown-elf-gcc` (RISC-V 交叉编译器) — 未安装
- `spike` (性能参考 ISA simulator) — 未安装
- `sail-riscv` (RISC-V 黄金参考实现) — 未安装
- `riscof` (RISC-V 合规认证框架) — 未安装
- `python3` (3.12.3) ✅ 已装

**根因**:
- Phase 1 ORCHESTRATION COMPLETE 时, 这些工具未被预装
- F2 决策"工具链策略 = 直接用预装"假设不成立
- 实际环境是仅有 Python 工具链

**影响**:
- 任务 1 (工具链准备) 无法完成 → 任务 2/3/4/5 全部 blocked
- M2 里程碑 (ISA 全覆盖) 无法在当前环境达成
- Phase 2 启动门槛 = NOT met

**需要 user 决策** (在 task 1 `- [~]` blocker):
1. **安装 RISC-V 工具链**:
   ```bash
   sudo apt-get update
   sudo apt-get install -y gcc-riscv64-unknown-elf spike
   # sail-riscv 需从源码或预编译下载
   pip3 install riscof
   ```
2. **接受部分工具链缺失** (修改 F2 决议): 仅用 Python 验证 + L1CachePlugin as memory model, 不依赖完整 RISC-V 工具链
3. **暂停 Phase 2** (等其他环境 ready)

**Why not 自动执行**:
- 工具链安装需 sudo + 系统级操作, 超出 atlas 范围
- 修改 F2 决策需 Prometheus 重新起草, 是新决策
- 暂停是用户决策, atlas 不应自动决策

**预防** (未来 Phase 3+):
- 在 Phase 启动前, 先运行 `which riscv64-...` 等 5 个工具, 确认环境就绪
- 若工具链缺失, 立即报告用户, 不启动 Phase
- Phase 2 计划 §Verification Strategy 应增加"工具链预检"作为 Phase 启动门槛

