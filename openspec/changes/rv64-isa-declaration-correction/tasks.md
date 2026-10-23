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
- 计数口径（HEAD `bcc88aa` 实测）：`[cpu]` 129 → **133**（删 `build_cpu_rv64` 1 case + 新增 5）；`[cpu-integration]` 96 cases / 95 PASS / **1 pre-existing FAIL**（`test_7stage_riscv.cpp:124` node_count `9 == 17`，口径 = 不新增失败）；chmem binary 130（`[chmem]` tag 89）

## Implementation

- [ ] Task 1 基线快照（禁退化锚：TLM 快返 + `[cpu]` 129/461 + `[cpu-integration]` 96/95+1pre-existing + `[riscv-tests],[mmu]` 107 + `[chmem]` 89）
- [ ] Task 2 新测试 `tests/cpu/test_cpu_factory_isa_xlen.cpp`（5 用例先红：默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved 单发 warn；含 `using namespace cf::cpu;` + re-cmake 步骤）
- [ ] Task 3 `cpu_factory.h`：`isa` 默认 `rv32im` + `xlen` 字段 + `cf::cpu::detail::validate_isa_xlen()`（**后置重开 namespace**）+ `build_cpu` 首行接线 → 新测试转绿
- [ ] Task 4 **T64 实例化点迁移**（B1 用户裁决）：删 `test_cpu_factory.cpp` 的 `build_cpu_rv64` case；余下 2 处 T64 → T32；`test_branch_predictor_runtime_btb.cpp` 3 case → `uint32_t`；integration 侧 **7 处** `cfg.isa` → `rv32im` + `test_topology_builder.cpp:35` 残留清理 ⇒ `nm` 无 T64 残留、tests/ 无 reserved 残留
- [ ] Task 5 schema：`xlen` 属性 + `isa` enum 收窄 `[rv32i,rv32im,rv32imac]`（rv64* 入 description）+ default `rv32im` + sweep 示例纠偏 + 核查 `:204` 条件约束
- [ ] Task 6 JSON ×4 纠偏：`isa` → `rv32im`（含 cpu_embedded `rv32imac`→`rv32im`）+ 3 个 `mmu_mode` → `sv32`（schema `:204` 条件）+ Draft7 校验（过滤 `_comment` pre-existing 基线）+ `[mmu],[cpu-l1-mmu-demo]` 回归
- [ ] Task 7 `tools/cpu_sim/main.cpp`：删 `cfg.isa="rv32i"` pin（**含注释，不得留字面量**）+ `load_config()` 补 `xlen` 解析 + 证实无 `--isa` flag + AC4 grep 自检 + `[cpu-integration]` 回归
- [ ] Task 8 `int_alu.h:88` / `branch.h:111` + `branch.h` `build()` 闭包内 —— 模板入口 `static_assert(sizeof(T)==4)`（函数体首行，实例化点触发，**branch 两处**）+ 双 binary 全量 build
- [ ] Task 9 变异对照：`branch.h` 护栏临时改 8 → 编译失败 → 还原（ADR-087 R4 证据；含 `grep -n` 预演防误伤既有 `is_unsigned` 断言）
- [ ] Task 10 删 `ip/cpu/plugins/reg_file.cpp`（0 CMake 引用终次确认 + build + grep=0）
- [ ] Task 11 缺口表 `rv64_readiness_gap.md` §2 状态列翻转（0.1–0.6 / 0.9 / 0.10 → ✅；0.7/0.8 已 ✅）+ `doc_link_check.sh` 实跑核验
- [ ] Task 12 CI 软门禁 `tools/check_xlen_hygiene.sh`（warn-only，allowlist = 缺口表 §5 的 **9 处定义点 / 9 个真实文件**）+ `architecture-gates.yml` 接入（continue-on-error）+ `tools/README.md` 登记 + dry 探针自检（`XLEN_SCAN_ROOT` 指探针目录，**不 stash、不写 ip/**）
- [ ] Task 13 全门禁收口：`run_chipforge_tests.sh --all`（timeout 1200；**`chipforge_tests` ctest entry 基线即红**（1 pre-existing FAIL）⇒ 机判口径 = 失败集合恰为 `{test_7stage_riscv.cpp:124}` + `ctest -E '^chipforge_tests$'` 3 条全绿）+ 6 门禁 + AC 逐条 grep 复核 + 计数终验 + `openspec change validate` 0 error + **分 5 批 commit（必须用 `git commit -- <paths>` 限定 pathspec，防 `git rm` 残留被夹带）**

## Acceptance

- [ ] AC1 三层 fail-fast（5 用例 `[isa-xlen]` 全绿：默认契约 / unsupported / implemented 冲突 / T↔xlen 交叉 / reserved 单发 warn）
- [ ] AC2 `CpuFactory<uint64_t>` 实例化编译失败（int_alu/branch 守卫；**前置**：T64 调用点已迁移，`grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>" tests/` = 0；Task 9 变异对照证据登记）
- [ ] AC3 `grep -rn '"rv64' ip/cpu/configs/ soc/ tools/dse/sweep_config.example.json` = 0；schema enum 只含 `rv32i/rv32im/rv32imac`、default `rv32im`、rv64* 仅 description；3 JSON `mmu_mode = sv32` 满足 `:204` 条件
- [ ] AC4 cpu_sim 无 `isa="rv32i"` 硬编码（含注释，grep = 0）；`--isa` flag 不存在；`load_config()` 已解析 `xlen`
- [ ] AC5 `reg_file.cpp` 不存在；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` = 0
- [ ] AC6 缺口表 §2 状态列与实态一致（0.1–0.6/0.9/0.10 ✅）
- [ ] AC7 CI 软门禁上线（warn 级，基线 `warn=0`，变异自检过）
- [ ] AC8 6 个门禁全 PASS（verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check / roadmap-r9-check / check_xlen_hygiene）
- [ ] AC9 0 退化（实测基线口径）：`[cpu]` **133** · `[cpu-integration]` 96 cases / 95 PASS / 1 pre-existing FAIL（不新增）· `[riscv-tests]` 48/48 · `[mmu]` 59/59 · chmem binary 130（`[chmem]` tag 89）全 PASS
- [ ] AC10 `openspec change validate` 0 error
