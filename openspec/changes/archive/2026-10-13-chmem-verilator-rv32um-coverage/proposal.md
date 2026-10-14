---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0
governance: rdd-builder-lite  # P0 approve → P1 plan 已有 → P2 原地执行
depends_on: []  # 不依赖任何 active change（precondition: mfc-extract-fsm-h archived on 2026-10-10）
supersedes: []
precondition: |
  mfc-extract-fsm-h (commit df3bcea) must be archived before this change's
  P2 execution. Status verified 2026-10-13: archived at
  openspec/changes/archive/2026-10-10-mfc-extract-fsm-h/ ✓
---

# chmem-verilator-rv32um-coverage — CH_MEM + Verilator 路径补全 rv32um 覆盖

> **范围 (scope)**：纯测试覆盖扩展，不改业务代码。
> **触发 (trigger)**：2026-10-13 radix-2 实装确认（commit `df3bcea` mfc-extract-fsm-h）后实测发现 CH_MEM + Verilator 路径下 rv32um 覆盖 = 0。
> **目标版本**：v0.11.0（`mfc-extract-fsm-h` archived 兑现期，本 change 与之 sibling）。
> **治理路径**：rdd-builder 轻量（4 artifacts 已就绪，P0 approve 后 P2 原地执行，6.x 文档修订独立 commit 兜底数字维护原则）。**非 rdd-quick 旁路**（scope 4 文件 + 27 tasks 超 ADR-0047 阈值）。

## Why

### 1.1 当前状态 (2026-10-13 实测)

mfc-extract-fsm-h 落地（commit `df3bcea` merge, 2026-10-10）后：

- **TLM 路径**：`[riscv-tests]` 48/48 PASS（rv32um 8/8），`ip/cpu/arch/riscv/mul_div_fsm.h:207-274` 真 radix-2 iterative 实装经 TLM Plugin run 路径验证 ✅
- **CH_MEM 路径**：`[chmem] chmem_vendored_elf_tohost1` (`tests/cpu/test_cpu_chmem_vendored_elf.cpp:121`) **仅覆盖 rv32ui 38 ELF，rv32um 0/8** ❌
- **Verilator 路径**：`[verilator] cpu_verilator_sim_tohost1` (`tests/cpu/test_cpu_verilator_sim.cpp:105`) **仅覆盖 rv32ui 40 ELF，rv32um 0/8** ❌

### 1.2 风险分析

当前缺口意味着：

1. **CH_MEM 路径下 `mul_div_fsm_chmem.h` 的 ch_state_machine DSL FSM 行为未在 rv32um 验证**——理论上 TLM 实装正确不代表 CH_MEM elaboration + CppHDL simulator 行为也正确
2. **Verilator 路径下 multi-cycle FSM (33-cycle div) 编译到 Verilog `@(posedge clk)` 时序正确性未验证**——radix-2 逐 bit 恢复除法在 CH_MEM elaboration 后的 Verilog 输出与 TLM 行为是否一致未知
3. **v0.11.0 launch gate 判据 #1**（`objective-v0110-launch.md §8 Sprint 1`）仅 TLM 口径——CH_MEM/Verilator 覆盖属 v0.11.0 启动期 follow-up

### 1.3 推迟到 v0.11.0 的理由

- **mfc-extract-fsm-h 已 archive**：CH_MEM 配对 Plugin `mul_div_fsm_chmem.h` 稳定可测，但需要 follow-up change 单独跟踪测试覆盖扩展（不改业务代码）
- **Wave 5 P2 优先级**：当前活跃 wave5 工作（btb-bp-coordination、dhrystone baseline 收尾）优先级更高，CH_MEM/Verilator rv32um 覆盖是"补完"性质工作
- **CH_MEM/Verilator 单 case 时长**：Verilator 单 case 5-8 min（含 verilator --cc + g++ -shared + dlopen + 2000 cycle sim），40 ELF cycle cap 时长已 5min；扩展 8 rv32um ELF 估时 1-2 min/ELF，CH_MEM 估时 1-2 s/ELF

## What Changes

### 1. `[chmem] chmem_vendored_elf_tohost1` kCases 扩展 8 项

**文件**：`tests/cpu/test_cpu_chmem_vendored_elf.cpp`

**改动**：line 110 后追加 8 个 `rv32um-p-*` 测试用例（mul / mulh / mulhsu / mulhu / div / divu / rem / remu），总 case 数 38 → 46。**不改** simulator/elaboration 路径，纯 kCases 列表扩展。

**预期**：8 个新 DYNAMIC_SECTION 全 PASS，CH_MEM Simulator 路径下 rv32um 8/8 ✅。

### 2. `[verilator] cpu_verilator_sim_tohost1` kCases 扩展 8 项

**文件**：`tests/cpu/test_cpu_verilator_sim.cpp`

**改动**：line 88 后追加 8 个 `rv32um-p-*` 测试用例，总 case 数 40 → 48。**不改** verilator runner 路径。

**预期**：8 个新 DYNAMIC_SECTION 全 PASS，Verilator 编译 + sim 路径下 rv32um 8/8 ✅。cycles 数字预期与 TLM 一致（mul=455 / mulh=504 / div=380 等）。

### 3. AGENTS.md §已知测试状态更新

**文件**：`AGENTS.md:130` (family 表 `[chmem]` 段) + `:133` (family 表 `[verilator]` 段) + `:165-166` (CH_MEM 状态清单)

**改动**：
- `[chmem] chmem_vendored_elf_tohost1` 由 38 ELF → 46 ELF，rv32um 覆盖率明示
- `[verilator] cpu_verilator_sim_tohost1` 由 40 ELF → 48 ELF
- 数字维护原则（`AGENTS.md:56`）：引用本 change 实测 commit/日期

### 4. CHANGELOG.md §Verification 段更新

**文件**：`CHANGELOG.md`（**创建** v0.11.0 段，v0.10.x 段之后；当前 CHANGELOG 全部为 v0.10.x interim 段）

**改动**：记录 `[chmem] 46 ELF` + `[verilator] 48 ELF` 实测，引用 commit hash。

## Capabilities

### New Capabilities
- `chmem-rv32um-coverage`（kebab-case）：CH_MEM 路径下 rv32um 8/8 测试覆盖 — 业务 Plugin 行为契约增强
- `verilator-rv32um-coverage`：Verilator 路径下 rv32um 8/8 测试覆盖 — 跨仿真后端一致性契约

### Modified Capabilities
（无 — 纯测试覆盖扩展，不改业务代码 spec）

## Impact

- **代码**：`tests/cpu/test_cpu_chmem_vendored_elf.cpp`（+8 行 kCases）, `tests/cpu/test_cpu_verilator_sim.cpp`（+8 行 kCases）
- **docs**：`AGENTS.md`, `CHANGELOG.md`（数字更新）
- **依赖**：无新增 third-party；需要已 vendored 8 个 rv32um-p-* ELF（已存在 `tests/cpu/riscv_tests/elf/rv32um-p-*`）
- **runtime**：CH_MEM 单 case +1-2s（Simulator 跑 8 rv32um ELF 估时 ~16s 增量）；Verilator 单 case +1-2min（8 rv32um verilator rebuild + sim 估时 ~12min 增量）—— `[verilator]` 整 family 总时长从 5min 增至 ~17min
- **CI**：无影响（CH_MEM/Verilator 不在 PR 阻塞门禁，参考 `AGENTS.md:384`）
- **不破坏**：不改业务代码、Plugin、Simulator、Verilator runner、toolchain；纯 kCases 列表追加 + 文档数字刷新

## Acceptance Criteria

- [ ] `[chmem] chmem_vendored_elf_tohost1` 总 case 数 46（38 rv32ui + 8 rv32um），全 PASS
- [ ] `[verilator] cpu_verilator_sim_tohost1` 总 case 数 48（40 rv32ui + 8 rv32um），全 PASS
- [ ] rv32um 8 ELF 在 CH_MEM 路径下 cycles 数字与 TLM 路径一致（mul=455 / mulh=504 / div=380 / rem=380，容差 ±5%）
- [ ] rv32um 8 ELF 在 Verilator 路径下 cycles 数字与 TLM 路径一致
- [ ] 架构门禁 `verify_adr 0 fail` / `verify_plugin_decision 8/8` / `check_plugin_portability 12/12` 全 PASS
- [ ] AGENTS.md §已知测试状态 + CHANGELOG §Verification 数字与实测 commit 绑定
- [ ] 0 regression：原有 38 rv32ui + 40 rv32ui verilator 维持 PASS

## Risks

- **Risk 1**：CH_MEM Simulator 在 rv32um ELF 上可能遇到 elaboration bug（ch_state_machine DSL 边界）→ 预期是 trivial，但留 RCA 时间
- **Risk 2**：Verilator rebuild 在 rv32um ELF 上可能暴露 multi-cycle FSM 的 Verilog 时序 bug（cycle cap 2000 对 33-cycle div 应足够，但需验证）→ 若 FAIL，需扩 cycle cap 或 RCA
- **Risk 3**：[verilator] family 总时长从 5min 增至 ~17min，开发者本地回归时间增加 → 已纳入 `AGENTS.md:384` known long-runner 类别

## 引用

- 触发：实测报告 `/home/ubuntu/.opencode/plan/20261013-verify-radix2-regression.md` + 双复审
- 现状：`tests/cpu/test_cpu_chmem_vendored_elf.cpp:39-110`（38 rv32ui kCases）, `tests/cpu/test_cpu_verilator_sim.cpp:39-88`（40 rv32ui kCases）
- 上游：`mfc-extract-fsm-h`（archive/2026-10-10, commit `df3bcea`）
- 战略：`objective-v0110-launch.md §8 Sprint 1`（v0.11.0 launch gate 判据 #1 TLM 路径已兑现；本 change 补 CH_MEM/Verilator 路径覆盖）
- 治理：本 change scope = 4 文件（2 test cpp + AGENTS.md + CHANGELOG.md）+ 27 tasks，超 rdd-quick 阈值（`AGENTS.md:463` ADR-0047 ≤2 文件 + ≤3 任务），**走 rdd-builder 轻量**（4 artifacts 完整 + P2 原地执行，不开 worktree）
