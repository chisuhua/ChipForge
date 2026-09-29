---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.0
depends_on:
  - 2026-09-28-debug-cpu-l1-mmu-demo-paddr-regression
---

# debug-cpu-l1-mmu-demo-deep-rca — 独立 follow-up，调查 5 个 riscv-tests ELF 在 10000 cycle 内未写 tohost=1 的根因

> **性质**：rdd-quick 量级 deep RCA change (独立 follow-up, 与 debug-cpu-l1-mmu-demo-paddr-regression hotfix 不同根因)
> **战略位置**: v0.10.0 PoC-1 (MUL/DIV FSM) 启动硬前置 #2 (debug-cpu-l1-mmu-demo-paddr-regression 已 archive 修复 #1)
> **超本 change scope**: 不动 MMU/PTW 算法; 不改 ADR-049 PADDR 契约; 不改 v0.8.0 P1#3 任何路径

## Why

debug-cpu-l1-mmu-demo-paddr-regression hotfix (v0.10.0 archive, 2026-09-28) 修复了 5 个回归 ([cpu] 1 + [cpu-integration] 4), 但 [cpu-l1-mmu-demo] 仍 5/6 FAIL (add/addi/auipc/jal/beq 5 个 riscv-tests ELF)。

实测 (2026-09-28 ctest):

```
[cpu-l1-mmu-demo] test cases:  6 |  1 passed | 5 failed
assertions: 35 | 30 passed | 5 failed
5 failed case 错误: cycles=10000 exited=0 exit_code=-1
```

trace (cpu_l1-mmu_demo-trace.txt, CF_DEBUG_IBUS_FETCH + CF_DEBUG_PICOLIBC_MEM):
- PC=0x80000000 → 0x80000004 → ... → 0x8000036c (卡在 `bne a4, t2, 80000560 <fail>` 分支)
- INSN 真实指令正常执行 (CPU pipeline 工作正常)
- 5 ELF 跑不到 `_finish` write-to-tohost = 1

**这不是 MMU/PADDR 问题** (本次 hotfix 已确认)。trace 显示 CPU 跑到 riscv-tests 的 test_25 断言, a4 != 26 → 跳到 fail 分支 → 卡死。

根因候选:
1. **CPU mis-execute 某条 add/branch 指令** (a4 没正确计算)
3. **riscv-tests ELF 加载顺序 vs PTE chain 不一致** (PTE 在不同 base, load_section 错位)
5. **kMaxCycles=10000 不够** (实际需要更多 cycle)

## What Changes

### 1. 单独 trace 5 个 FAIL case (vs hotfix 的整体 trace)

```bash
cd /workspace/project/ChipForge
mkdir -p docs/notes
for elf in add addi auipc jal beq; do
  ./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" \
    2> docs/notes/cpu_l1_mmu_demo_${elf}.trace.stderr | tail -3
  # 找该 ELF 的 trace 段 (grep INSN 序列)
done
```

### 2. 分析 a5==4 vs a4==26 的中间指令

定位到 riscv-tests 的 test_25 段, 看 a4 怎么算, 找 CPU 哪一步 mis-execute。

### 3. 候选修法 (按概率排序)

| 嫌疑 | 修法 |
|------|------|
| **(A) `kMaxCycles` 不足** | 改 `tests/soc/test_cpu_l1_mmu_demo.cpp:110` 的 `kMaxCycles` 从 10000 → 50000, 跑一次看是否过 |
| **(B) riscv-tests ELF 加载顺序错位** | 在 `tests/soc/test_cpu_l1_mmu_demo.cpp` 加 log, 看 ELF entry addr vs PTE 加载 base_addr 是否一致 |
| **(C) CPU mis-execute add/branch** | 单独跑 riscv-tests ELF (`tests/cpu/integration/test_rv32ui_runner.cpp` 已 PASS 40/40, 对照哪个 case 差异最大) |
| **(D) PTE base_addr != ELF base_addr** | PicolibcHostMemory::Config.base_addr 与 ELF VMA 不一致 (默认 0x80000000 vs ELF section 实际 VMA) |

### 4. 验证

```bash
./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" --reporter compact 2>&1 | tail -3
# 期望: test cases:  6 |  6 passed | 0 failed
```

## Acceptance Criteria

- [ ] `[cpu-l1-mmu-demo]` 6/6 PASS（`cycles ≤ kMaxCycles, exited=true, exit_code=0`）
- [ ] `[cpu]`, `[cpu-integration]`, `[mmu]`, `[riscv-tests]` 不退化（≥当前数字）
- [ ] 修复 commit 写明 "RCA: cpu-l1-mmu-demo 5/6 follow-up" 在 commit message

## 关联 change / ADR 锚点

- **debug-cpu-l1-mmu-demo-paddr-regression** (v0.10.0 archive, 2026-09-28): 父 hotfix, 修 [cpu] + [cpu-integration] 5 回归。本次 follow-up 是其下游。
- **ADR-049** (v0.8.0): MMU PADDR Consumption Contract — 本 change 不动契约
- **CHANGELOG v0.10.0** (commit ad48fcf): "Known Follow-up" 节已记录 5/6 fail + 跟踪方法

## 不在 scope (与父 hotfix 一致)

- 不动 MMU/PTW 算法
- 不改 ADR-049 PADDR 契约
- 不改 riscv-tests 路径 (test_rv32ui_runner.cpp 仍 40/40)
- 不动 mfc-cpu-pipeline-multi-cycle-fsm (PoC-1 主路径)

## 实施窗口

- **启动条件**: 无 (本次 hotfix 已 archive, dependency 已满足)
- **估时**: 1-3 天 (deep RCA + 单点修复)
- **顺序**: 单独 trace → 排序 4 嫌疑 → 修 → 回归 → archive
- **风险**: 候选 (C) CPU mis-execute 风险较高, 可能需深度挖单条指令执行链

## 失败 → 砍分叉动作

- 4 个嫌疑都排除但仍失败 → 提交 debug change follow-up `debug-cpu-l1-mmu-demo-isa-execution-rca`, 附完整单条指令 trace, 邀请 Oracle 深度 RCA
- 修复 commit 后 [cpu-integration] / [cpu] / [mmu] / [riscv-tests] 退化 → 回滚 commit, 标记为永久 follow-up (影响 v0.10.0 启动但不动 v0.10.0 时间窗)