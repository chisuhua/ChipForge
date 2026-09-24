---
initiative: wave3-cpu-pipeline-debt
priority: P0
status: completed
---

# Tasks — cpu-pipeline-fix-rv32ui-load-width (回顾性)

> **状态**: ✅ 所有 task 已在 v0.6.0 commit `8909165` 完成 (2026-09-22 09:00:48 +0800)
> 本文件保留作追溯, 用于 Wave 3 initiative 的 initiative status 派生

## 1. failing test (TDD red)

- [x] 1.1 跑 `[riscv-tests]` family 10 个 LOAD/STORE 用例记录 baseline FAIL (commit `8909165` 前状态: 30/40)
- [x] 1.2 验证 fail: `[riscv-tests]` 30/40 PASS, 10 FAIL 显示 `category=feature stub`

## 2. implement

- [x] 2.1 `ip/cpu/plugins/dbus.h` LOAD 路径按 funct3 分发 width extraction (LB/LH/LW/LBU/LHU)
- [x] 2.2 LB/LH 走 `static_cast<intN_t>` 符号扩展
- [x] 2.3 LBU/LHU 走 `read_byte/half` zero-extend (天然 zero-extend, 无需额外处理)
- [x] 2.4 LW 同路径但走 `read_word`
- [x] 2.5 LOAD forwarding: memory 阶段算出的 load value 同步写 `MEM_DATA` + `RD_DATA`

## 3. verify pass

- [x] 3.1 `bash tools/run_chipforge_tests.sh --tag "[riscv-tests]"` 40/40 PASS (commit `8909165`)
- [x] 3.2 `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 同步更新 (41 行, 0 FAIL)
- [x] 3.3 `[cpu-integration]` 4/4 PASS 不回归
- [x] 3.4 `[cpu-l1-mmu-demo]` 6/6 PASS 不回归

## 4. CI 门禁

- [x] 4.1 `bash tools/verify_adr.sh` PASS (无 ADR 变更)
- [x] 4.2 `bash tools/verify_plugin_decision.sh` PASS (D4 §4 if-else 合规)
- [x] 4.3 `bash tools/check_plugin_portability.sh` PASS (无 ch 渗透)

## 5. commit + archive

- [x] 5.1 commit `8909165` "fix(plugin): DBusPlugin LOAD width extraction (LB/LH/LW/LBU/LHU) + RD_DATA forwarding" — 1 原子 commit
- [x] 5.2 随 v0.6.0 release 合并
- [x] 5.3 `CHANGELOG.md` v0.6.0 段本 fix 条目
- [x] 5.4 (本回顾性 change) `openspec archive cpu-pipeline-fix-rv32ui-load-width -y` —— 待执行

## Acceptance (回顾性 — 工作在 v0.6.0 已完成)

- [x] `[riscv-tests]` family 40/40 PASS (commit `8909165` 实测)
- [x] CSV 0 FAIL
- [x] CI 门禁全 PASS
- [x] CHANGELOG v0.6.0 条目
- [x] 无 pending 工作 (Wave 3 P0#2 提前完成, 不需重复投入)
