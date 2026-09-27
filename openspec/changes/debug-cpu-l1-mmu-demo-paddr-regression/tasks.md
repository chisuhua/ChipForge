# Tasks

> **TDD 5 步**：每条任务标 [RED → GREEN → REFACTOR] 阶段

## Phase A — Trace 加注（**[RED]** — 观察失败模式）

- [ ] A.1 加 `#ifdef CF_DEBUG_IBUS_FETCH` trace 到 `ip/cpu/plugins/ibus.h` at_stage fetch 闭包（输出 PC/PADDR_VALID/PADDR/INSTRUCTION）
- [ ] A.2 加 `#ifdef CF_DEBUG_PICOLIBC_MEM` trace 到 `ip/cpu/picolibc_host_memory.h` read_word/write_word 入口（输出 R/W + addr + data）
- [ ] A.3 构建 + 跑 `[cpu-l1-mmu-demo]`，抓 trace 输出到 `docs/notes/cpu-l1-mmu-demo-trace.txt`（note 文件不入 git，lifecycle 短）

## Phase B — 二分定位真凶

- [ ] B.1 对 5 个 FAIL case（add/addi/auipc/jal/beq）各跑 100 cycle 抓 trace
- [ ] B.2 排序 4 嫌疑（A→D）按 trace 证据：嫌疑 A 看 `PADDR_VALID=0/1`、嫌疑 B 看 `read` 是否返回 0、嫌疑 C 看首条指令 fetch 时 PADDR_VALID、嫌疑 D 看 trace 时序
- [ ] B.3 记录 RCA 结论到 `docs/notes/cpu-l1-mmu-demo-rca.md`（不进 git）

## Phase C — 修复

- [ ] C.1 根据 Phase B 结论选 [GREEN] 修法（按嫌疑表选）
- [ ] C.2 跑 `[cpu-l1-mmu-demo]` 期望 6/6 PASS

## Phase D — 回归保证

- [ ] D.1 跑 `[riscv-tests]` 验证 40/40 不退化
- [ ] D.2 跑 `[cpu-integration]` 验证 ≥77/81 不退化（4 FAIL 是已知非本 change 范围）
- [ ] D.3 跑 `[mmu]` 验证 ≥53/53 不退化

## Phase E — 清理

- [ ] E.1 [REFACTOR] 移除所有 `#ifdef CF_DEBUG_*` trace 代码（保留宏定义便于未来复用）
- [ ] E.2 跑 `[cpu-l1-mmu-demo]` `[riscv-tests]` `[mmu]` 全套再次确认不退化

## Phase F — 归档

- [ ] F.1 全部 AC 完成 → `openspec validate debug-cpu-l1-mmu-demo-paddr-regression`
- [ ] F.2 写 commit message 含 "Fix: cpu-l1-mmu-demo regression after mmu-paddr-consume archive"
- [ ] F.3 `openspec archive debug-cpu-l1-mmu-demo-paddr-regression`（归档时序：先 archive 本 hotfix，再 archive `mfc-cpu-pipeline-multi-cycle-fsm`）
- [ ] F.4 update CHANGELOG.md v0.10.0 entry

## 依赖关系

```
2026-09-26-mmu-paddr-consume-and-real-memory ✅ (已 archive)
        ↓
debug-cpu-l1-mmu-demo-paddr-regression (本 change) → archive
        ↓
mfc-cpu-pipeline-multi-cycle-fsm (P3#8) 可以启动 Phase A → B → C
```

## 注

本 change 是 rdd-quick 量级（≤3 文件 / ≤3 任务 / 无公共 API 变更），但因依赖关系清晰（PoC-1 唯一前置）拆成独立 change 以便：
1. 归档后策略表 §7 准确反映"PoC-1 hard prereq 已收官"
2. 修复 commit 可独立 review / 回滚
3. 主控 §3.1 hard prereq 表行 2 可勾选 ✅