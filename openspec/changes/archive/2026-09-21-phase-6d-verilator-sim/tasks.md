## 1. 6d.5 E8 — Verilator sim 跑 vendored ELF

- [x] 1.1 `tools/verilator_runner/sim_main.cpp` dlopen Vtop__ALL.so + access API — **commit 1a99ed3 (89 行)**
- [x] 1.2 `tools/verilator_runner/cpu_verilator_sim.cpp` CLI + 两阶段 verilator --cc 编译 — **commit 1a99ed3 (221 行)**
- [x] 1.3 `verilator --cc --public-flat-rw cpu.v` Vtop__ALL.a 生成 — **实测成功**
- [x] 1.4 Verilator sim 跑 5 ELF (rv32ui-p-{add,addi,auipc,beq,jal}) — **5/5 PASS, cycle-identical 0% diff**
- [x] 1.5 验证: Verilator sim 与 CppHDL sim 行为一致 — **cycle 478=478 / 246=246 / 59=59 / 315=315 / 55=55**
- [x] 1.6 CppHDL codegen 修复 (type_mem / type_lit / ch_reg init) — **sibling commit 3a5284d**

## 2. 6d.8 — Harness 迁移

- [x] 2.1 `tools/verilator_runner/cpu_verilator_sim` 替代 `tools/cpu_sim` — **commit 1a99ed3 (CLI 兼容 --elf flag)**
- [ ] 2.2 `[cpu-integration]` 测试 (`test_*stage_riscv.cpp`) 同步迁移 — **Oracle 决策: 暂不迁移 (当前 4 stage tests 仍用 TLM harness, 不需 CH_MEM 迁移; 排除 7stage superscalar pre-existing segfault)**
- [x] 2.3 验证: `./cpu_verilator_sim --elf <path>` 跑 CH_MEM sim 行为与 TLM 一致 — **5/5 ELF tohost=1 PASS**

## 3. CI 门禁

- [x] 3.1 `check_plugin_portability.sh` 8/8 PASS (9/9 等 `phase-6d-fsm-chmem`)
- [x] 3.2 `verify_adr.sh` 0 FAILED
- [x] 3.3 `./bin/chipforge_tests_chmem` 37/37 PASS (含 10 新 verilator e2e assertions)
- [x] 3.4 `./bin/chipforge_tests` TLM baseline 0 回归
- [x] 3.5 `verilator --lint-only cpu.v` 0 errors / 0 warnings

## 4. Archive

- [x] 4.1 commit + push: 5 原子 commit — **1a99ed3 (ChipForge) + 3a5284d (CppHDL sibling)**
- [x] 4.2 `openspec archive phase-6d-verilator-sim -y --skip-specs`

## 5. Acceptance (本 change scope)

- [x] `tools/verilator_runner/cpu_verilator_sim --elf <path>` 跑 5 ELF tohost=1 PASS
- [x] Verilator sim cycle 数 ±10% — **实际 0% 差异, cycle-identical**
- [x] `[cpu-integration]` 排除 7stage superscalar (Oracle: pre-existing segfault 不影响判读)
- [x] TLM baseline 0 回归
