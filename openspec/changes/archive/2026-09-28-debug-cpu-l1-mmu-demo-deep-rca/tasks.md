# Tasks

> **TDD 5 步**：每条任务标 [RED → GREEN → REFACTOR] 阶段
> **父 change**: debug-cpu-l1-mmu-demo-paddr-regression (v0.10.0 archive, 2026-09-28)

## Phase A — 单独 trace 5 个 FAIL case

- [ ] A.1 重跑 [cpu-l1-mmu-demo] with trace enabled (CMAKE_CXX_FLAGS="-DCF_DEBUG_IBUS_FETCH -DCF_DEBUG_PICOLIBC_MEM")
- [ ] A.2 抓 5 个 FAIL case (add/addi/auipc/jal/beq) 单独 trace 到 docs/notes/cpu_l1_mmu_demo_{add,addi,auipc,jal,beq}.trace.stderr
- [ ] A.3 提取每 case 最后 100 行 trace (PC + INSN + ACCESS + write 序列)

## Phase B — 二分定位真凶 (4 嫌疑)

- [ ] B.1 **嫌疑 A: kMaxCycles 不足** → 临时改 `tests/soc/test_cpu_l1_mmu_demo.cpp:110` 的 `kMaxCycles` 从 10000 → 50000, 看是否过 (2 分钟验证)
- [ ] B.2 **嫌疑 B: riscv-tests ELF 加载顺序错位** → 在 `tests/soc/test_cpu_l1-mmu-demo.cpp` 加 log, 打印 ELF entry_addr vs PicolibcHostMemory::Config.base_addr + section VMA 列表
- [ ] B.3 **嫌疑 C: CPU mis-execute 某条 add/branch** → 在 test_25 段 trace 中看 a4 计算路径, 找 CPU mis-execute 哪条指令
- [ ] B.4 **嫌疑 D: PTE base_addr vs ELF base_addr 不一致** → 检查 plant_identity_page_table 的 PTE VMA 与 ELF .text VMA

## Phase C — 按嫌疑修复

- [ ] C.1 根据 Phase B 结论选 [GREEN] 修法 (按嫌疑概率排序)
- [ ] C.2 跑 [cpu-l1-mmu-demo] 期望 6/6 PASS

## Phase D — 回归保证

- [ ] D.1 跑 [cpu] 验证 ≥117/117 不退化 (本次 hotfix 已修)
- [ ] D.2 跑 [cpu-integration] 验证 ≥81/81 不退化 (本次 hotfix 已修)
- [ ] D.3 跑 [mmu] 验证 ≥53/53 不退化 (本次 hotfix 已修)
- [ ] D.4 跑 [riscv-tests] 验证 40/40 不退化 (riscv-tests 路径独立)

## Phase E — 清理

- [ ] E.1 移除所有 `#ifdef CF_DEBUG_*` trace 代码 (保留宏定义便于未来复用)
- [ ] E.2 跑全套 [cpu-l1-mmu-demo] [cpu] [cpu-integration] [mmu] [riscv-tests] 再次确认不退化

## Phase F — 归档

- [ ] F.1 全部 AC 完成 → `openspec validate debug-cpu-l1-mmu-demo-deep-rca`
- [ ] F.2 写 commit message 含 "RCA: cpu-l1-mmu-demo 5/6 follow-up"
- [ ] F.3 `openspec archive debug-cpu-l1-mmu-demo-deep-rca` (归档时序: 本 change 独立 archive, 不依赖 mfc-cpu-pipeline-multi-cycle-fsm)
- [ ] F.4 update CHANGELOG.md v0.10.1 entry 或 v0.11.0 entry (由 estimated 修复时间定)

## 依赖关系

```
debug-cpu-l1-mmu-demo-paddr-regression (v0.10.0 archive ✅)
        ↓
debug-cpu-l1-mmu-demo-deep-rca (本 change) → archive
        ↓
mfc-cpu-pipeline-multi-cycle-fsm (P3#8) PoC-1 可以启动 Phase A → B → C
```

## 注

本 change 是 rdd-quick 量级 (≤3 文件 / ≤3 任务 / 无公共 API 变更), 但因根因未知需 deep RCA, 任务拆细为 17 条以确保覆盖 4 嫌疑。

Estimates:
- 嫌疑 A: 2 分钟 (改 kMaxCycles 常量)
- 嫌疑 B: 15 分钟 (加 log + 重跑)
- 嫌疑 C: 2-3 小时 (单条指令执行链 trace)
- 嫌疑 D: 30 分钟 (PTE VMA 检查)

总估时: 1-3 天 (deep RCA)