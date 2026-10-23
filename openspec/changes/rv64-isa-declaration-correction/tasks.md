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

## Phase 0 备忘

- P0 决策：case-1 approve（auto-decision ADR-0049：advisory=complex + LLM=complex + AC=10 → 完整 P1–P3）
- proposal/spec/designed 语义修正（P1 起）：fail-fast 三层分解（design.md Decision 1）+ schema enum 收窄（D4）——critic review 通过后锁定

## Implementation

- [ ] Task 1 基线快照（禁退化锚：`[cpu]` 计数 / TLM 快返 / CH_MEM 抽查）
- [ ] Task 2 新测试 `tests/cpu/test_cpu_factory_isa_xlen.cpp`（5 用例先红：默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved 单发 warn）
- [ ] Task 3 `cpu_factory.h`：`isa` 默认 `rv32im` + `xlen` 字段 + `detail::validate_isa_xlen()`（唯一 SSOT）+ `build_cpu` 首行接线 → 新测试转绿 + `[cpu]/[cpu-integration]/[riscv-tests]` 回归
- [ ] Task 4 schema：`xlen` 属性 + `isa` enum 收窄 `[rv32i,rv32im,rv32imac]`（rv64* 入 description）+ default `rv32im` + sweep 示例纠偏
- [ ] Task 5 JSON ×4：`isa` → `rv32im`（含 cpu_embedded `rv32imac`→`rv32im`，A 扩展零实现）
- [ ] Task 6 `tools/cpu_sim/main.cpp`：删 `cfg.isa="rv32i"` pin（:187-188）+ 注释更新 + 证实无 `--isa` flag + 5 个 e2e 回归
- [ ] Task 7 `int_alu.h:88` / `branch.h:112` 模板入口 `static_assert(sizeof(T)==4)`（函数体首行，实例化点触发）+ 双 binary 全量 build（R6 编译面）
- [ ] Task 8 变异对照：`branch.h` 护栏临时改 8 → 编译失败 → 还原（ADR-087 R4 证据）
- [ ] Task 9 删 `ip/cpu/plugins/reg_file.cpp`（0 CMake 引用终次确认 + 双 binary build + grep=0）
- [ ] Task 10 缺口表 `rv64_readiness_gap.md` §2 状态列翻转（0.1–0.6 / 0.9 / 0.10 → ✅；0.7/0.8 已 ✅）
- [ ] Task 11 CI 软门禁 `tools/check_xlen_hygiene.sh`（warn-only，allowlist=缺口表 §5 九处）+ `architecture-gates.yml` 接入（continue-on-error）
- [ ] Task 12 全门禁收口：`run_chipforge_tests.sh --all` + 5 架构门禁 + hygiene + AC 逐条 grep 复核 + `openspec change validate` 0 error

## Acceptance

- [ ] AC1 三层 fail-fast（5 用例 [isa-xlen] 全绿：默认契约 / unsupported / implemented 冲突 / T↔xlen 交叉 / reserved 单发 warn）
- [ ] AC2 `CpuFactory<uint64_t>` 实例化编译失败（int_alu/branch 守卫；Task 8 变异对照证据登记）
- [ ] AC3 `grep -rn '"rv64' ip/cpu/configs/ soc/` = 0；schema default `rv32im`
- [ ] AC4 cpu_sim 无 `isa="rv32i"` pin；`--isa` flag 不存在（口径证实）
- [ ] AC5 `reg_file.cpp` 不存在；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` = 0
- [ ] AC6 缺口表 §2 状态列与实态一致（0.1–0.6/0.9/0.10 ✅）
- [ ] AC7 CI 软门禁上线（warn 级，变异自检过）
- [ ] AC8 架构门禁 5 脚本 + hygiene 全 PASS
- [ ] AC9 0 退化：`[cpu]`(+5) · `[cpu-integration]` 36/36 · `[riscv-tests]` 48/48 · `[chmem]` 130/130 · `[mmu]` 59/59
- [ ] AC10 `openspec change validate` 0 error
