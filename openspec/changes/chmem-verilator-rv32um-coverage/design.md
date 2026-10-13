# Design: chmem-verilator-rv32um-coverage

> **Date**: 2026-10-13  
> **Author**: ChipForge Plugin Team (build agent, radix-2 实装确认期)  
> **Status**: 📋 PROPOSED (rdd-builder 轻量)

## Context

### 1.1 现状

`mfc-extract-fsm-h`（commit `df3bcea` merge, 2026-10-10）落地后：

- **`ip/cpu/arch/riscv/mul_div_fsm.h:207-274`** 实装真 radix-2 iterative（mul=1 / mulh=3 / div=33 cycle）
- **`ip/cpu/plugins/mul_div_fsm_chmem.h`**（CH_MEM 配对 Plugin, FsmBase 模板继承）
- **TLM 路径**：`[riscv-tests]` 48/48 PASS（rv32um 8/8 + rv32ui 40/40）
- **CH_MEM 路径**：`[chmem] chmem_vendored_elf_tohost1` 仅 38 rv32ui ELF，rv32um 0/8
- **Verilator 路径**：`[verilator] cpu_verilator_sim_tohost1` 仅 40 rv32ui ELF，rv32um 0/8

### 1.2 缺口

CH_MEM 路径下 `mul_div_fsm_chmem.h` 的 `ch_state_machine` DSL FSM 行为未在 rv32um 验证；Verilator 路径下 multi-cycle FSM 编译到 Verilog `@(posedge clk)` 时序正确性未验证。TLM 8/8 PASS 不代表跨后端一致性。

### 1.3 约束

- **不依赖** 任何 active change（precondition: mfc-extract-fsm-h 已 archive，2026-10-10，目录 `openspec/changes/archive/2026-10-10-mfc-extract-fsm-h/` 已确认）
- **与 mfc-extract-fsm-restore 关系**：该 change 尚未在 `openspec/changes/` 创建（仅 AGENTS.md 文字预告），两者**实为 sibling**——本 change 不消费 mfc-extract-fsm-restore 的任何产物，`depends_on: []` 准确；仅版本窗口（v0.11.0）重合
- **不改** 业务代码、Plugin、Simulator、Verilator runner、toolchain
- **仅扩展** Catch2 kCases 列表 + 文档数字刷新
- **目标版本** v0.11.0（mfc-extract-fsm-restore 承接期）

## Goals / Non-Goals

### Goals
- [chmem] family 下 rv32um 8/8 PASS（CH_MEM Simulator 路径覆盖）
- [verilator] family 下 rv32um 8/8 PASS（Verilator 编译 + sim 路径覆盖）
- cycles 数字与 TLM 路径基线一致（容差 ±5%）
- 0 regression：原有 38 rv32ui + 40 rv32ui verilator 维持 PASS
- AGENTS.md + CHANGELOG 数字与实测 commit 绑定（数字维护原则）

### Non-Goals
- 不改 `mul_div_fsm.h` / `mul_div_fsm_chmem.h` 算法
- 不改 Simulator / Verilator runner / `cpu_verilator_sim` tool
- 不增 cycle precision（`mfc-cpu-pipeline-multi-cycle-fsm` Phase F scope）
- 不做 DSE / performance sweep（独立 follow-up）
- **不扩 `[mmu-verilator]` family** —— 该 family 是 MMU plumbing-only 端到端（`tests/mmu/test_mmu_bare_plumbing_verilator.cpp`，3 cases / 5 rv32ui ELF），与 M 扩展（`mul`/`div` 指令）正交；M 扩展测试是计算单元验证，MMU plumbing 测试是地址翻译验证，两者关注点独立

## Decisions

### D1. 纯 kCases 列表扩展，0 算法变更，0 测试逻辑变更

**选择**：在 `tests/cpu/test_cpu_chmem_vendored_elf.cpp:110` 和 `tests/cpu/test_cpu_verilator_sim.cpp:88` 末尾各追加 8 个 `rv32um-p-*` 条目。

**理由**：
- CH_MEM Simulator 路径对 8 个 rv32um ELF 已是通用驱动（`build_cpu(elf) → run → check dmem[0x1000]==1`），不需要为 rv32um 写新代码
- Verilator runner `cpu_verilator_sim` 是 ELF 无关的通用 5-stage CPU 仿真器（接收 `--elf` 参数），加 kCases 不需要改 runner
- 已有 8 个 `rv32um-p-*` ELF 在 `tests/cpu/riscv_tests/elf/` 下，build_rv32um.sh 已被 mfc tasks 引用过
- **不动 cycle assertion 逻辑** —— CH_MEM 路径现有 cycle 数字仅 INFO 打印（`test_cpu_chmem_vendored_elf.cpp:150-160`），不修改为 `REQUIRE`；Verilator 路径 cycle 数字由 runner stdout 输出（`CYCLES=<n>`），C++ 测试只 REQUIRE `TOHOST=1` 字符串。cycle 数字比对走"人工 + commit message 记录"路径（spec Scenario 2 明示）

**替代方案**：
- 写独立的 `test_rv32um_chmem.cpp` / `test_rv32um_verilator.cpp`（新文件）—— 拒绝。重复 infrastructure，与现有 kCases 模式冲突，违反"最小改动"原则
- 复用 `test_rv32um_runner.cpp` 模式（独立 .cpp + DYNAMIC_SECTION）—— 拒绝。引入新 binary 链接路径，CI 配置变更
- 在 kCases 追加同时加 cycle assertion —— 拒绝。超出"纯 kCases append"声明，跨入测试逻辑变更

### D2. cycle cap 2000 不变

**选择**：复用现有 `--cycles 2000` cap（来自 `tests/cpu/test_cpu_verilator_sim.cpp:128`）。

**理由**：
- rv32um 最长 cycle = 504 (mulh)，2000 已是 4× headroom
- 与现有 rv32ui 40 ELF 共用 cap，单 case 改动零

**替代方案**：
- 单独给 rv32um 设 cap = 1000（更紧）—— 拒绝。不必要的 cap 异质化
- 扩 cap = 5000（更松）—— 拒绝。增加回归基线时间，无实际收益

### D3. 走 rdd-builder 轻量（**非 rdd-quick**）

**选择**：走 rdd-builder 完整 OpenSpec 流程（proposal/design/specs/tasks 4 artifacts），但跳过 worktree 重型模式，原地执行。**显式排除 rdd-quick 旁路**（ADR-0047）。

**理由**：
- 实际 scope = 4 文件（2 test cpp + AGENTS.md + CHANGELOG.md）+ 27 tasks + 0 公开 API 变更
- rdd-quick 阈值（ADR-0047）：≤2 文件 + ≤3 任务 + 0 API 变更 —— **3 条件破 2**
- rdd-quick 还要求"no openspec change"——本 change 目录已存在，混搭治理不可行
- 4 artifacts 已就绪，删除重建成本 > 走 rdd-builder 轻量成本
- 17min verilator 回归在 rdd-builder 完整流程下可接受（不阻塞 P0/P1/P2/P3 任何环节）

**替代方案**：
- **rdd-quick 旁路**：scope 超阈 2 倍，违反 ADR-0047，且 rdd-quick 禁止 openspec change
- **完整 rdd-builder + worktree**：4 文件 + 27 tasks 规模 worktree 流程开销不必要

### D4. CH_MEM/Verilator 数字记录约定

**选择**：AGENTS.md 数字格式 `[chmem] chmem_vendored_elf_tohost1: 46 DYNAMIC_SECTION` + `[verilator] cpu_verilator_sim_tohost1: 48 DYNAMIC_SECTION`，附实测 commit hash + 日期。

**理由**：
- 数字维护原则（`AGENTS.md:56`）：引用 HEAD commit 实测，禁止快照引用
- 与现有 432 / 411 catch2 instances 表述一致

**替代方案**：
- 写 "[chmem] 1/1 PASS（46 DYNAMIC_SECTION 嵌套）"—— 接受但补充"46"明示总数

## Risks / Trade-offs

| # | Risk | Mitigation |
|---|------|-----------|
| R1 | CH_MEM Simulator 在 rv32um ELF 上遇到 elaboration bug（ch_state_machine DSL 边界 case） | 单 case 隔离 → 单独重跑定位 → 暂列入"CPU pipeline tick RCA"change；本 change scope 仅扩 kCases，不调 algorithm。spec Scenario 4 (elaboration success) 已显式覆盖 |
| R2 | Verilator rebuild 在 rv32um ELF 上暴露 multi-cycle FSM 的 Verilog 时序 bug（33-cycle div 在 @posedge clock 累加溢出） | 复用现有 cycle cap 2000 + spec Scenario 2 容差规则（±5% 或 +50 cycles 取大）；如 FAIL 暴露 bug，列入"verilator-mul-div-fsm-followup"新 change |
| R3 | [verilator] family 总时长未实测估算（proposal 37 行原估时"5min→17min"数字自相矛盾，需首次实测校准） | P2 执行时先跑 1 个 rv32um ELF 校准（预计 30s-2min），再决定是否引入 `[verilator-rv32um]` 子 family tag 分批跑（呼应 design Q1 升级为 R3 mitigation） |
| R4 | 8 个 rv32um-p-* ELF 未来被替换/删除（risky-tests upstream 变更） | build_rv32um.sh 已有 vendor 脚本（`tests/cpu/riscv_tests/build_rv32um.sh`），rebuild 不依赖 upstream |
| R5 | 跨后端 cycle 数字可能略偏离（CH_MEM/Verilator 比 TLM 多几个 elaboration/setup cycle） | spec 容差规则"±5% 或 +50 cycles 取大"已涵盖；如超容差先确认是真实 bug 还是 setup overhead |

## Migration Plan

### P2 执行 — 0.5-1 天（解决数字维护原则 chicken-egg）

**关键顺序**：先 commit 2 cpp（hash X 产生），再在 X 上跑测试取数字，再 commit 文档（引用 X）。

1. `tests/cpu/test_cpu_chmem_vendored_elf.cpp:110` 末尾追加 8 行 kCases
2. `tests/cpu/test_cpu_verilator_sim.cpp:88` 末尾追加 8 行 kCases
3. `cmake --build build -j$(nproc)` 单次 rebuild（2 cpp 同属 `chipforge_tests_chmem` target，合并 2.2/3.2 旧两次 rebuild）
4. `git add` 这 2 个 cpp + `openspec/changes/chmem-verilator-rv32um-coverage/`（5 文件入 git 历史，否则 archive move 后仍是 untracked）
5. `git commit -m "test(chmem/verilator): extend rv32um kCases (mfc-extract-fsm-h follow-up)"` 产生 commit hash X
6. 在 HEAD X 上跑 `./build/bin/chipforge_tests_chmem "[chmem]"` 验 46 case 全 PASS
7. 在 HEAD X 上跑 `./build/bin/chipforge_tests_chmem "[verilator]"` 验 48 case 全 PASS
8. 收集每 rv32um ELF 的 cycle 数字（CH_MEM 走 INFO 打印，Verilator 走 `CYCLES=<n>` stdout）

### P3 文档 + Archive — 0.5 天

9. AGENTS.md §已知测试状态 更新（family 表 :130/133 + 状态清单 :165-166），**引用 commit hash X + 日期 + 实测 46/48 数字**
10. CHANGELOG.md **创建** v0.11.0 段并追加 entry，引用 X
11. 架构门禁：`bash tools/verify_adr.sh && tools/verify_plugin_decision.sh && tools/check_plugin_portability.sh`
12. 文档检查：`python tools/doc_checker.py --format text --verbose`
13. `git add` AGENTS.md + CHANGELOG.md，`git commit -m "docs(agents/changelog): chmem-verilator-rv32um-coverage 实测数字 (commit X)"`
14. `openspec archive chmem-verilator-rv32um-coverage -y` 把 change 目录 move 到 `openspec/changes/archive/2026-10-13-chmem-verilator-rv32um-coverage/`
15. `git add openspec/changes/archive/2026-10-13-chmem-verilator-rv32um-coverage/ && git commit -m "docs(openspec): archive chmem-verilator-rv32um-coverage"`（archive move 进 git 历史）

### 回滚
- kCases 是列表 append，回滚即删除 8 行；无破坏性
- archive 后 archive 目录仍可 revert（move 是 atomic rename）

## Open Questions

- **Q1（已升级为 R3 mitigation）**：`[verilator]` family 扩展后总时长**未实测**（proposal 37 行原估时"5min→17min"自相矛盾）。P2 执行时先跑 1 个 rv32um ELF 校准（30s-2min），如超 20min 才考虑 `[verilator-rv32um]` 子 tag（见 R3）
- **Q2（已解决为 D1 注释 + spec Scenario 2 deferred）**：CH_MEM cycle assertion 显式 deferred 到 DSE follow-up；本 change 走"人工比对 + commit message 记录"
- **Q3（已解决为 P3 步骤 14-15）**：archive 后 archive 目录进 git 历史（不依赖 cpu-sim-add-elf-tohost-fix 中断态）
