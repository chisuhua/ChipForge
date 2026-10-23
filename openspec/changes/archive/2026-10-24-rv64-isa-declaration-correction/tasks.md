---
change: rv64-isa-declaration-correction
priority: P0
initiative: wave5-isa-coverage-and-bp
adr:
  - ADR-087
plan: .rddf/plans/rv64-isa-declaration-correction.md
status: in-planning
---

# Tasks — rv64-isa-declaration-correction

> 实施细节在 plan（TDD 5-step）；本文件是 OpenSpec 跟踪台账。execute 阶段逐 Task 勾选。
> **本版已按 critic 审查（6 BLOCKER / 7 SHOULD_FIX / 6 NITS，session `ses_e9879fb37ffeBO2wBIropJMaDe`）+ 用户 B1 裁决（迁移 T64 调用点，保留编译期护栏）修订**。

## Phase 0 备忘

- P0 决策：case-1 approve（auto-decision ADR-0049：advisory=complex + LLM=complex + AC=10 → 完整 P1–P3）
- 语义裁决（proposal/spec/design 同步）：fail-fast 三层分解（design.md Decision 1）+ schema enum 收窄（Decision 4）+ **T64 实例化点迁移（Decision 7，用户裁决）**
- 计数口径（HEAD `bcc88aa` 实测）：`[cpu]` 129 → **137**（删 `build_cpu_rv64` 1 case + 5 契约用例 + 4 完备性补全用例）；TLM 全量 481 → **493**；`[cpu-integration]` 96 cases / 95 PASS / **1 pre-existing FAIL**（`test_7stage_riscv.cpp:124` node_count `9 == 17`，口径 = 不新增失败）；chmem binary 130（`[chmem]` tag 89）

## Implementation

- [x] Task 1 基线快照（禁退化锚：TLM 全量 481/480+1pre-existing · `[cpu]` 129/461 · `[cpu-integration]` 96/95+1 · `[riscv-tests],[mmu],[cache]` 132 · chmem binary 130 / tag 89）
- [x] Task 2 新测试 `tests/cpu/test_cpu_factory_isa_xlen.cpp`（5 用例先红：默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved 单发 warn；含 `using namespace cf::cpu;` + re-cmake 步骤）—— 实测 **5 passed / 9 assertions**
- [x] Task 3 `cpu_factory.h`：`isa` 默认 `rv32im` + `xlen` 字段 + `cf::cpu::detail::validate_isa_xlen()`（**后置重开 namespace detail**）+ `build_cpu` 首行接线 + `#include <iostream>` → 新测试转绿
- [x] Task 4 **T64 实例化点迁移**（B1 用户裁决）：删 `test_cpu_factory.cpp` 的 `build_cpu_rv64` case；余下 2 处 T64 → T32；`test_branch_predictor_runtime_btb.cpp` 3 case → `uint32_t`；integration 侧 **7 处** `cfg.isa` → `rv32im` + `test_topology_builder.cpp:35` 残留清理 ⇒ `nm` 无 T64 残留、tests/ 无 reserved 残留（`[cpu]` 137 PASS）
- [x] Task 5 schema：`xlen` 属性 + `isa` enum 收窄 `[rv32i,rv32im,rv32imac]`（rv64* 入 description）+ default `rv32im` + sweep 示例纠偏 + 核查 `:210` 条件约束
- [x] Task 6 JSON ×4 纠偏：`isa` → `rv32im`（含 cpu_embedded `rv32imac`→`rv32im`）+ 3 个 `mmu_mode` → `sv32` + Draft7 校验 4×OK（`_comment` pre-existing 基线已登记）+ AC3 零命中 + `[mmu],[cpu-l1-mmu-demo]` 65 PASS
- [x] Task 7 `tools/cpu_sim/main.cpp`：删 `cfg.isa="rv32i"` pin（含注释，AC4 grep = 0）+ `load_config()` 补 `xlen` 解析 + 证实无 `--isa` flag + `[cpu-integration]` 96/95+1 回归
- [x] Task 8 `int_alu.h` `compute()` + `branch.h` `evaluate_branch()` **与** `branch.h` `build()` 闭包内 —— 3 处 `static_assert(sizeof(T)==4)`（函数体首行，实例化点触发）+ 双 binary 全量 build（TLM + CH_MEM 均 exit 0）
- [x] Task 9 变异对照：`branch.h` 护栏临时改 8 → **编译失败**（`static assertion failed: RV32 TLM path assumes 32-bit XLEN`）→ 还原（计数 2）→ 编译恢复 —— ADR-087 R4 证据
- [x] Task 10 删 `ip/cpu/plugins/reg_file.cpp`（0 CMake 引用 + 0 `#include` 命中；`git rm`；build OK；`[cpu]` 137 PASS；AC5 grep = 0）
- [x] Task 11 缺口表 `rv64_readiness_gap.md` §2 状态列翻转（0.1–0.6 / 0.9 / 0.10 → ✅）+ §1 `cpu_sim` 行标注 + 行号口径 blockquote 更新 + `doc_link_check.sh` 0 broken
- [x] Task 12 CI 软门禁 `tools/check_xlen_hygiene.sh`（warn-only，allowlist = 缺口表 §5 的 **9 处定义点 / 9 个文件**）+ `architecture-gates.yml` 接入（continue-on-error）+ `tools/README.md` 登记 + 变异自检（基线 warn=0 / 探针 warn=1 / STRICT exit 1）
- [x] Task 14 **测试完备性补全**（3 个缺口：helper 直调分支 / cpu_sim xlen 解析 e2e / schema+JSON 声明值断言；3 个变异对照全部被抓住 ⇒ 新增 `[isa-xlen]` 9 cases / 20 assertions + `[cpu-configs]` 4 cases）
- [x] Task 13 全门禁收口：`run_chipforge_tests.sh --all`（4 entry：verify_plugin_decision PASS / chipforge_tests_chmem PASS 633.93s / cpu_verilator_sim_add PASS / **chipforge_tests Failed = 1 pre-existing**，失败集合实测恰为 `{test_7stage_riscv.cpp:124}`，493 = 492 passed + 1）+ `ctest -N -E '^chipforge_tests$'` = 3 条 + 6 门禁全绿 + AC 逐条 grep 复核 7/7 + 计数终验（`[cpu]`137 / `[cpu-integration]`96=95+1 / `[riscv-tests],[mmu],[cache]`132 / chmem binary 130）+ `openspec change validate` valid + **分 6 批 commit（`git commit -- <paths>` 限定 pathspec；批 4 不带 `git add`）**

## Acceptance

- [x] AC1 三层 fail-fast（5 用例 `[isa-xlen]` 全绿：默认契约 / unsupported / implemented 冲突 / T↔xlen 交叉 / reserved 单发 warn）
- [x] AC2 `CpuFactory<uint64_t>` 实例化编译失败（int_alu/branch 守卫；**前置**：T64 调用点已迁移，`grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>" tests/` = 0；Task 9 变异对照证据登记）
- [x] AC3 `grep -rn '"rv64' ip/cpu/configs/ soc/ tools/dse/sweep_config.example.json` = 0；schema enum 只含 `rv32i/rv32im/rv32imac`、default `rv32im`、rv64* 仅 description；3 JSON `mmu_mode = sv32` 满足 `:204` 条件
- [x] AC4 cpu_sim 无 `isa="rv32i"` 硬编码（含注释，grep = 0）；`--isa` flag 不存在；`load_config()` 已解析 `xlen`
- [x] AC5 `reg_file.cpp` 不存在；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` = 0
- [x] AC6 缺口表 §2 状态列与实态一致（0.1–0.6/0.9/0.10 ✅）
- [x] AC7 CI 软门禁上线（warn 级，基线 `warn=0`，变异自检过）
- [x] AC8 6 个门禁全 PASS（verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check / roadmap-r9-check / check_xlen_hygiene）
- [x] AC9 0 退化（实测基线口径）：`[cpu]` **137** · `[cpu-integration]` 96 cases / 95 PASS / 1 pre-existing FAIL（不新增）· `[riscv-tests]` 48/48 · `[mmu]` 59/59 · `[cpu-configs]` 13/13 · chmem binary 130（`[chmem]` tag 89）全 PASS
- [x] AC10 `openspec change validate` 0 error
      - ⚠️ **归档后原命令不可复现**（`Change ... not found`）⇒ post-archive 等价校验：`openspec validate cpu-isa-xlen-contract`（期望 `is valid`，exit 0）

### 实测留证（2026-10-23，HEAD `749ed2c` 之后的实施结果）

| 项 | 实测 |
|---|---|
| 新增测试 | `tests/cpu/test_cpu_factory_isa_xlen.cpp` **9 cases / 20 assertions** PASS（含 3 个 helper 直调用例）+ `tests/cpu/test_cpu_sim_xlen_parse.cpp` 1 case（e2e）+ `tests/cpu/schema/test_cpu_isa_xlen_schema.cpp` 4 cases |
| `[cpu]` | **137 cases / 480 assertions PASS**（129 − 1 删 case + 5 契约用例 + 4 完备性补全用例） |
| TLM 全量 | 493 cases = **492 passed + 1 failed**（`test_7stage_riscv.cpp:124` node_count `9 == 17`，pre-existing） |
| `[cpu-integration]` | 96 cases / 95 PASS / 1 pre-existing FAIL |
| `[riscv-tests],[mmu],[cache]` | 132 cases PASS（48 + 59 + 25） |
| CH_MEM | `chipforge_tests_chmem` 130 cases（`[chmem]` tag 89）PASS，ctest entry 633.93s |
| ctest 4 entry | verify_plugin_decision PASS · chipforge_tests_chmem PASS · cpu_verilator_sim_add PASS · **chipforge_tests Failed（1 pre-existing）** |
| 护栏变异对照 | `branch.h` 改 8 → `static assertion failed: RV32 TLM path assumes 32-bit XLEN` → 还原计数 2 → 编译恢复 |
| T64 残留 | `nm` 扫全部 `.o`：`RiscvIntAluPlugin<unsigned long` / `RiscvBranchPlugin<unsigned long` / `BranchPredictorPlugin<unsigned long` **0 命中** |
| reserved 残留 | `grep -rn 'isa = "rv64\|isa = "rv32imac' tests/ --include='*.cpp' \| grep -v test_cpu_factory_isa_xlen.cpp` = **0**（专用 reserved 语义测试除外） |
| hygiene 门禁 | 基线 `warn=0` / 探针 `warn=1`（恰 1 条） / `STRICT_XLEN_GATE=yes` exit 1 |
| 6 门禁 | verify_adr ✅ · verify_plugin_decision 8/8 ✅ · check_plugin_portability 13/13 ✅ · doc_link_check 0 broken ✅ · roadmap-r9-check 11 fragments ✅ · check_xlen_hygiene warn=0 ✅ |
| commit 拆分 | 6 批，全部 `git commit -- <paths>` 限定 pathspec；批 4（`reg_file.cpp` 删除）不带 `git add`；index 无残留 |
