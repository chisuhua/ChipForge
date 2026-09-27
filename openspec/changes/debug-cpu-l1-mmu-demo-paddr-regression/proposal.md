---
initiative: wave5-isa-coverage-and-bp
priority: P0
version_target: v0.10.0
depends_on:
  - 2026-09-26-mmu-paddr-consume-and-real-memory
---

# debug-cpu-l1-mmu-demo-paddr-regression — 修 [cpu-l1-mmu-demo] 5/6 FAIL（PoC-1 hard prereq #2）

> **性质**：rdd-quick / hotfix 量级 change（单文件为主，估时 ≤ 2 周）。
> **战略位置**：v0.10.0 启动硬前置 PoC-1（MUL/DIV 多周期）必须先收官；本 change 是 PoC-1 启动的**先决条件**。
> **关联**：rdd-quick 流程（≤3 文件 / ≤3 任务 / 无公共 API 变更），ADR-082 不受影响（这是独立的 debug change）。

## Why

`[cpu-l1-mmu-demo]` 5/6 FAIL 事实（2026-09-26 实测）：

```
test cases:  6 |  1 passed | 5 failed
assertions: 35 | 30 passed | 5 failed
```

5 个失败 case 全部为 `cpu_l1_mmu_demo_{add,addi,auipc,jal,beq}`，失败信息：

```
cycles=10000 exited=0 exit_code=-1
```

— 程序运行 10000 cycle 但 `mem.exited()` 永远 false（`tohost=1` 未被写入）。**这是流水线语义级 bug**，不是构建或测试框架问题。

对比：相同 ELF 在 `[riscv-tests]` 路径（test_rv32ui_runner.cpp）**全部 40/40 PASS**（无 MMU 路径）。因此 bug 在 MMU+CPU 集成路径上——也就是 commit `8a14402`（2026-09-26 `mmu-paddr-consume-and-real-memory` archive）引入的 PADDR 消费改造路径。

后果：
- v0.10.0 PoC-1（MUL/DIV）无法 launch（主控 `execution-roadmap.md §5` 验收硬条件"6/6 PASS 才允许启动"）
- v1.0.0 信誉期（CoreMark ≥2.3）阻断
- v0.8.0 引入的 ADR-049（MMU PADDR Contract）+ ADR-040 v2.0（双模约束）契约落地不完整

## What Changes

### 1. 加 trace print 到 IBus fetch（debug 一行）

在 `ip/cpu/plugins/ibus.h` 的 `at_stage("fetch", NORMAL)` 闭包顶部加：

```cpp
std::cerr << "[IBus] PC=0x" << std::hex << pc
          << " PADDR_VALID=" << (node)(pl::PADDR_VALID)
          << " PADDR=0x" << std::hex << (node)(pl::PADDR)
          << " INSN=0x" << std::hex << (node)(pl::INSTRUCTION)
          << std::endl;
```

注：仅 debug build 启用（`#ifdef CF_DEBUG_IBUS_FETCH`），release build 无副作用。

### 2. 加 trace print 到 PicolibcHostMemory（debug 一行）

在 `ip/cpu/picolibc_host_memory.h` 的 `read_word`/`write_word` 入口加：

```cpp
#ifdef CF_DEBUG_PICOLIBC_MEM
  std::cerr << "[Picolibc] " << (is_write ? "W" : "R")
            << " addr=0x" << std::hex << addr
            << " data=0x" << std::hex << value
            << std::endl;
#endif
```

### 3. 跑 `[cpu-l1-mmu-demo]` 抓 trace（5 个 FAIL case）

```bash
./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" --reporter compact 2>&1 | grep -E "\[IBus\]|\[Picolibc\]"
```

### 4. 二分定位真凶（按概率顺序）

| 嫌疑 | 证据需求 |
|------|---------|
| **(A) `pl::PADDR_VALID` 在 demo 配置下永假** | trace 输出 `PADDR_VALID=0`，但 MMU 期望 true |
| **(B) PicolibcHostMemory 未在 PADDR 区间提供服务** | trace 显示 PADDR=0x80001234 但 Picolibc read 返回 0 |
| **(C) PTW walk 期间 fallback 路径触发但 PADDR_VALID 清除时机错** | trace 显示首条指令 fetch 走 vaddr fallback，但 demo memory map 仍按 vaddr 加载（pad mismatch） |
| **(D) MMUPlugin::at_stage 时序晚于 IBus fetch 消费 PADDR** | trace 显示 IBus 消费 PADDR 时 MMU 还没填充 PADDR_VALID |

### 5. 按嫌疑修复（按概率顺序，估时 ≤ 2 周）

| 嫌疑 | 修法 |
|------|------|
| (A) | 在 `RiscVMMUPlugin::at_stage` 中加 PADDR_VALID 早期置位（在 fetch 进入 PC 计算后立即置，PTW 完成前显式清零） |
| (B) | 改 `soc/cpu_l1_mmu_demo.json` memory_map base_addr 为 `0x80000000` PADDR 等价地址，或在 PicolibcHostMemory::Config 加 pad_offset 字段 |
| (C) | 在 `IBusPlugin::at_stage` 中检查 PADDR_VALID=true 后才能使用 PADDR；否则走 PC fallback（修复 v0.8.0 已 commit 但未生效的 fallback 路径） |
| (D) | 调换 at_stage 注册顺序（IBus 靠后注册，MMU 先消费 PC → 翻译 → 写 PADDR） |

### 6. 验证

```bash
./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" --reporter compact 2>&1 | tail -5
# 期望: test cases:  6 |  6 passed | 0 failed
# 期望: assertions: 35 | 35 passed | 0 failed
```

### 7. 清理 trace

修复确认后，移除所有 `#ifdef CF_DEBUG_*` trace 代码（保留头文件宏定义以便未来复用）。

## Acceptance Criteria

- [ ] `[cpu-l1-mmu-demo]` 6/6 PASS（`cycles ≤ 10000, exited=true, exit_code=0`）
- [ ] `[riscv-tests]` 40/40 PASS（保持不退化）
- [ ] `[cpu-integration]` 77/81 PASS（4 FAIL 是已知 5stage/7stage tohost=1 模板缺失，与本 change 无关）
- [ ] 修复 commit 写明 "Fix: cpu-l1-mmu-demo regression after mmu-paddr-consume archive" 在 commit message
- [ ] trace 代码全部清理（无 `#ifdef CF_DEBUG_*` 残留业务代码）

## ADR 锚点

- **ADR-049**（v0.8.0 已落地）：MMU PADDR Consumption Contract — 本 change 是其契约完整化
- **ADR-040 v2.0**（v0.3.0 已落地）：TLM↔CH_MEM 双模约束 — 本 change 不动 CH_MEM 路径

## 不在 scope

- 不动 ADR-049 PADDR 契约本身
- 不动 MMU/PTW 算法
- 不动 IBus/DBus 主路径（仅 debug trace 加/移除）
- 不改 `cpu-pipeline-multi-cycle` / `mfc-cpu-pipeline-multi-cycle-fsm` / 任何 v0.10.0 PoC

## 失败 → 砍分叉动作

- 所有 4 个嫌疑都排除但仍失败 → 提交 debug change follow-up：`debug-cpu-l1-mmu-demo-deep-rca`，附完整 trace，邀请 Oracle 深度 RCA
- 修复 commit 后 `[cpu-integration]` 退化 → 回滚 commit，标记 `[cpu-l1-mmu-demo]` 为永久 follow-up（影响 PoC-1 启动但不动 v0.10.0 时间窗）

## 实施窗口

- **启动条件**：无（hotfix 紧急）
- **估时**：≤ 2 周（binary search 嫌疑 A→D 通常 2-3 天定位，修复 + 验证 1 周）
- **顺序**：trace 加 → 跑测试 → 定位 → 修 → 验证 → 清理 trace → archive