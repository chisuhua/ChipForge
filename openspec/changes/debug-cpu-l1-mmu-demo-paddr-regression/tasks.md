# Tasks

> **TDD 5 步**：每条任务标 [RED → GREEN → REFACTOR] 阶段

## Phase A — Trace 加注（**[RED]** — 观察失败模式）

- [x] A.1 加 `#ifdef CF_DEBUG_IBUS_FETCH` trace 到 `ip/cpu/plugins/ibus.h` at_stage fetch 闭包（输出 PC/PADDR_VALID/PADDR/INSTRUCTION）
- [x] A.2 加 `#ifdef CF_DEBUG_PICOLIBC_MEM` trace 到 `ip/cpu/picolibc_host_memory.h` read_word/write_word 入口（输出 R/W + addr + data）
- [x] A.3 构建 + 跑 `[cpu-l1-mmu-demo]`，抓 trace 输出到 `docs/notes/cpu-l1-mmu-demo-trace.txt`（note 文件不入 git，lifecycle 短）

## Phase B — 二分定位真凶

- [x] B.1 对 5 个 FAIL case（add/addi/auipc/jal/beq）各跑 100 cycle 抓 trace
- [x] B.2 排序 4 嫌疑（A→D）按 trace 证据：嫌疑 A 看 `PADDR_VALID=0/1`、嫌疑 B 看 `read` 是否返回 0、嫌疑 C 看首条指令 fetch 时 PADDR_VALID、嫌疑 D 看 trace 时序
- [x] B.3 记录 RCA 结论：trace v3 显示 `CPU 0 IBus 5 fires + 50000 Picolibc R addr=0x0` → 根因 vaddr_for_stage 返回 0 + MMU Bare shortcut 未覆盖 satp_ppn_=0 (2 bugs: vaddr_for_stage KeyType mismatch + MMU Bare shortcut 不够广)

## Phase C — 修复

- [x] C.1 [GREEN] MMUPlugin::do_lookup 扩展 Bare shortcut 覆盖 `satp_ppn_==0` (`ip/mmu/tlm/MMUPlugin.cpp:64`)
- [x] C.1b [GREEN] RiscvMMUPlugin::vaddr_for_stage 双 KeyType 查找 (rv32 + rv64 keys 兼容) (`ip/cpu/plugins/mmu.cpp:143`)
- [x] C.1c [GREEN] RiscvMMUPlugin::build() set pb_for_vaddr_ (`ip/cpu/plugins/mmu.h:80` + `ip/cpu/plugins/mmu.cpp:84`)
- [x] C.1d [GREEN] MMUPlugin::set_satp_value() + satp_value_ 字段 (严格区分 Bare vs Sv+PPN=0) (`ip/mmu/tlm/MMUPlugin.h:64`)
- [x] C.1e [GREEN] RiscvMMUPlugin::csr_write_satp 调 set_satp_value (传递完整 CSR) (`ip/cpu/plugins/mmu.cpp:34`)
- [x] C.1f [GREEN] tests/mmu/test_ptw_tlb_refill_integration.cpp make_test_context 显式 set_satp_ppn(non_zero) 让 PTW walk 不被 Bare shortcut 短路
- [ ] C.2 跑 `[cpu-l1-mmu-demo]` 期望 6/6 PASS — **只达 1/6 (5/6 仍 fail, 独立 follow-up, 与本次 hotfix 不同根因: CPU mis-execute riscv-tests 断言 fail 分支)**

## Phase D — 回归保证

- [x] D.1 跑 `[riscv-tests]` 验证 40/40 不退化
- [x] D.2 跑 `[cpu-integration]` 验证 ≥77/81 不退化 — **实际 81/81 (4 个 stage 集成测试全修复, 超目标)**
- [x] D.3 跑 `[mmu]` 验证 ≥53/53 不退化 — **实际 53/53 (test_ptw_tlb_refill 加显式 set_satp_ppn 旁路 Bare shortcut)**
- [x] D.4 跑 `[cpu]` 验证 ≥116/117 不退化 — **实际 117/117 (cpu_sim_real_tohost 修复, 超目标)**

## Phase E — 清理

- [x] E.1 [REFACTOR] 移除所有 `#ifdef CF_DEBUG_*` trace 代码（保留宏定义便于未来复用）
- [x] E.2 跑 `[cpu-l1-mmu-demo]` `[riscv-tests]` `[mmu]` `[cpu]` `[cpu-integration]` 全套再次确认不退化

## Phase F — 归档

- [x] F.1 全部 AC 完成 → `openspec validate debug-cpu-l1-mmu-demo-paddr-regression`
- [ ] F.2 写 commit message 含 "Fix: cpu-l1-mmu-demo regression after mmu-paddr-consume archive"
- [ ] F.3 `openspec archive debug-cpu-l1-mmu-demo-paddr-regression`（归档时序：先 archive 本 hotfix，再 archive `mfc-cpu-pipeline-multi-cycle-fsm`）
- [x] F.4 update CHANGELOG.md v0.10.0 entry

## 依赖关系

```
2026-09-26-mmu-paddr-consume-and-real-memory ✅ (已 archive)
        ↓
debug-cpu-l1-mmu-demo-paddr-regression (本 change) → archive
        ↓
mfc-cpu-pipeline-multi-cycle-fsm (P3#8) 可以启动 Phase A → B → C
```

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