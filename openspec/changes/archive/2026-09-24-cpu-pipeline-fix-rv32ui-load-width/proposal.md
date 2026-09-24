---
initiative: wave3-cpu-pipeline-debt
priority: P0
status: completed-ahead-of-schedule
completion_commit: 8909165
completion_version: v0.6.0
---

# cpu-pipeline-fix-rv32ui-load-width — 10 LOAD/STORE 用例 FAIL → PASS (回顾性归档)

> **状态**: ✅ 已于 v0.6.0 提前完成 (commit `8909165`)
> **原计划**: Wave 3 P0#2 (2026-10 ~ 2 周内)
> **实际完成**: 2026-09-22 09:00:48 +0800 (v0.6.0 release 前 2 周)
> **原因**: 实装 v0.6.0 静态配置期 Result 范式时同步发现 LOAD width 是 riscv-tests 阻塞项, 顺手实装

## Why

v0.2.2 时的 `[riscv-tests]` family 通过率 30/40 = 75%, 其中 10 个 LOAD/STORE 用例 (lb/lh/lbu/lhu + 内嵌 load 的 sb/sh/sw/st_ld/ld_st) 显示 `category=feature stub`, AGENTS.md 标注 OUT OF SCOPE 等 Wave 2 follow-up `riscv-tests-rv32ui-load-width` 处理。

根因 (commit `8909165` 分析):
1. **LOAD width extraction 缺失**: DBusPlugin 仅处理 LW 一种 width, LB/LH/LBU/LHU 未按 funct3 分发
2. **LOAD forwarding 缺失**: memory 阶段算出的 load value 只写 `MEM_DATA`, 未同步写 `RD_DATA` —— RegFilePlugin writeback 读 `RD_DATA` 拿不到 load value, 导致 BNE 永远 fail (LW 也 fail 是这个 root cause, 不只是 LB/LH width 缺失)

## What Changes (回顾性)

### 1. LOAD 路径按 funct3 分发 width extraction

- **位置**: `ip/cpu/plugins/dbus.h` `at_stage("memory", NORMAL)` 闭包
- **机制**:
  - funct3=000 (LB): `read_byte(addr)` + `static_cast<int8_t>` 符号扩展到 32 位
  - funct3=001 (LH): `read_half(addr)` + `static_cast<int16_t>` 符号扩展到 32 位
  - funct3=010 (LW): `read_word(addr)` (原已支持)
  - funct3=100 (LBU): `read_byte(addr)` zero-extend
  - funct3=101 (LHU): `read_half(addr)` zero-extend
- **代码风格**: `if-else` 链 (D4 §4 允许, 无状态机, 无早返, 已在 v0.2.2 commit `ec1713c` STORE 路径用同样模式)

### 2. LOAD forwarding 修复

- memory 阶段同时写 `n->operator()(pl::MEM_DATA) = ...` 和 `n->operator()(pl::RD_DATA) = ...`
- 与 STORE 路径对称 (STORE 只写 MEM_DATA)
- 这修复了 LW 测试 fail 的 root cause

### 3. 测试验证 (已 commit)

- commit `8909165` 同时验证: `bash tools/run_chipforge_tests.sh --tag "[riscv-tests]"` 40/40 PASS
- `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 同步更新 (现 41 行, 0 FAIL)

## Capabilities

### New Capabilities

- `cpu-real-fetch-and-memory`: 新增 "DBusPlugin MUST implement full LOAD width extraction (LB/LH/LW/LBU/LHU) per funct3 with correct sign/zero extension" requirement
- `cpu-stage-link-propagation`: 新增 "LOAD value MUST propagate to RD_DATA for writeback consumption" requirement (LOAD forwarding)

## Impact (回顾性)

- **实际修改**:
  - `ip/cpu/plugins/dbus.h` (+45 LOC: LOAD width 分发 + LOAD forwarding 双写)
- **无新增文档**: 跟随 commit `8909165` 的 commit message 已含详细分析
- **CSV 同步**: `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 0 FAIL
- **下游解锁**:
  - riscv-tests 30/40 → **40/40 PASS** (RV32I 整数子集 100% 合规)
  - Phase 2 RV64 切换有完整整数子集基线
  - Wave 3 P1#5 `cpu-pipeline-multi-cycle` 的 MUL/DIV 测试有 riscv-tests 全绿基线对照

## Acceptance (回顾性验证)

- [x] `[riscv-tests]` family 40/40 PASS (commit `8909165` 实测)
- [x] `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 41 行 0 FAIL
- [x] 现有 4 个 RISC-V 仿真测试 (`test_3stage/5stage/7stage/10stage_riscv.cpp`) 不回归
- [x] `[cpu-integration]` 4/4 PASS 不回归
- [x] 3 门禁 (`verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh`) 全 PASS (随 v0.6.0 release 验证)
- [x] CHANGELOG v0.6.0 段本 fix 条目

## Risk (回顾性评估)

- **R1 (已收敛)**: LOAD width 分发 if-else 链无早返、无状态机, D4 Tier-1 #4 合规
- **R2 (已收敛)**: LOAD forwarding 修 LW root cause 同时影响所有 load-store 序列, 现有 `[cpu-l1-mmu-demo]` 6 测试无回归
- **R3 (后续 watch)**: 内嵌 load 路径 (`sb`/`sh`/`sw` 等带内存地址计算的测试) 走 MMU 真路径需要 P1#3 `mmu-paddr-consume-and-real-memory` 落地

## Lessons Learned (本 change 元教训)

1. **根因分析的复合性**: LW fail 表面是 width 缺失, 实际根因是 forwarding 缺失 —— single bug fix 触发级联 PASS
2. **commit message 的诊断价值**: commit `8909165` 的 message 含 LOAD forwarding 分析, 后续 reviewer 可直接追溯
3. **Wave 1 → Wave 3 范围漂移**: 本 change 原计划在 Wave 3 启动 (2026-10), 实际在 v0.6.0 (2026-09-22) 提前 2 周完成 —— Wave 3 P0#2 现在**没有遗留工作**, 仅作回顾性归档

## 后续 Action (本 change 无 pending)

- ✅ 本 change tasks 全部 completed
- ✅ CSV 0 FAIL 反映真状态
- ⏭ Wave 3 P0#2 工作实际在 v0.6.0 完成, 不需 Wave 3 重复投入
- ⏭ Wave 3 启动门槛降低: 1 个 P0 change (canonical-ordering) + 3 个 P1 changes