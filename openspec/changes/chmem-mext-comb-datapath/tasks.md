# Tasks — chmem-mext-comb-datapath

> **Path**: rdd-builder（1 业务文件 + 快照基线 + 文档）
> **Est. total**: 1-1.5 天
> **Owner**: ip/cpu
> **Spec refs**: `specs/chmem-mext-datapath/spec.md`
> **Design refs**: `design.md`
> **Decision basis**: oracle 2026-10-20 + PoC `tests/framework/test_chmem_arith_poc.cpp` (commit `63bfc8a`/`03667fa`)

## 0. Pre-flight

- [ ] 0.1 `grep -rn "node_count" tests/` 盘点所有 DAG 快照断言，列出可能受 select 树 11→19 层影响者
- [ ] 0.2 记录 baseline：`./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` 当前 8 rv32um FAIL 状态（确认起点）
- [ ] 0.3 记录 baseline：`./build/bin/chipforge_tests_chmem "[verilator]"` 当前 8 rv32um 假绿状态
- [ ] 0.4 确认 `git status --short` 干净（PoC 已提交）

## 1. IntAlu CH_MEM M-op 组合层 (spec: chmem-mext-datapath)

- [ ] 1.1 在 `int_alu_chmem.h` 加 `is_mext = is_op && f7_01` + 8 个 M-op 判别（复用现有 `f3_0..f3_7`）
- [ ] 1.2 加符号-幅值 helper（`neg32`/`neg_flag`/`abs32`，文件内匿名 helper 或 local lambda）
- [ ] 1.3 计算 8 条 M-op 结果（移植 PoC Part D 精确表达式：MUL/MULHU/MULH/MULHSU/DIVU/REMU/DIV/REM，含除零 + INT_MIN 特例）
- [ ] 1.4 在现有 11 层 select 树（`int_alu_chmem.h:243` 之后、`addr_result` 之前）追加 8 层 M-op select
- [ ] 1.5 （可选加固）给 6 个无 gate 的 base R-type op 的 `is_op` 侧补 `f7_00`（**不动 `is_opimm` 侧**）
- [ ] 1.6 `cmake --build build --target chipforge_tests_chmem -j$(nproc)` 编译过

## 2. 功能验证

- [ ] 2.1 `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` → 8/8 rv32um 真 PASS + 38 rv32ui 不回归（46 total）
- [ ] 2.2 `./build/bin/chipforge_tests_chmem "[arith-poc]"` → 17/17 不回归
- [ ] 2.3 记录 rv32um 每 ELF 的 cycle 数（INFO 输出），与 `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` 对比记录在 commit message（组合式 1 拍，cycle 必然远小于 TLM radix-2 baseline — 属预期，非回归）

## 3. Verilator 后端

- [ ] 3.1 `./build/bin/chipforge_tests_chmem "[verilator]"` → 8/8 rv32um 真 PASS（非 mask 假绿），`[mmu-verilator]` 3/3 不回归
- [ ] 3.2 `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-div` 目检 work_dir 下 `.v` 含 `/`、`%`
- [ ] 3.3 若 `[verilator]` 8 rv32um 仍假绿（mask 掩盖），评估是否需同步修订 `verilator_runner/sim_main.cpp` 的 done/pass 判据（**注意**：该修订归 `verilator-mul-div-fsm-followup`，本 change 若触及需说明）

## 4. DAG 快照基线

- [ ] 4.1 若 0.1 发现受影响的 node_count 断言，同步更新为实测值
- [ ] 4.2 确认 `7stage_topology_from_config` 的 pre-existing infra drift（`9==17`）状态不变（本 change 不触碰 7stage 路径，应维持原样）

## 5. Regression

- [ ] 5.1 `./build/bin/chipforge_tests` 全量 TLM → 无新增 FAIL（`7stage_topology_from_config` pre-existing 除外）
- [ ] 5.2 `./build/bin/chipforge_tests_chmem "[chmem]"` 全套 → 0 FAIL（**注意 >300s**）
- [ ] 5.3 `bash tools/run_chipforge_tests.sh --all` 全量（可选，~167s+）

## 6. Architecture Gates

- [ ] 6.1 `bash tools/verify_adr.sh` → 0 fail
- [ ] 6.2 `bash tools/verify_plugin_decision.sh` → 8/8 PASS
- [ ] 6.3 `bash tools/check_plugin_portability.sh` → 12/12 PASS（int_alu_chmem.h 本就含 ch_*，合规）
- [ ] 6.4 `bash tools/roadmap-r9-check.sh` → 0 fail

## 7. Documentation

- [ ] 7.1 `mul_div_fsm_chmem.h` 文件头加注释：mechanism fixture, not a datapath candidate（见 spec Non-goals）
- [ ] 7.2 `AGENTS.md` `[chmem][m-ext-gap]` 条目更新：标记 CH_MEM M 扩展组合式落地 + 更正"算术语义异常/lock race"旧结论
- [ ] 7.3 `CHANGELOG.md` v0.10.3 Known Limitations 段加更正注解（PoC `63bfc8a`/`03667fa` 证伪）
- [ ] 7.4 `openspec/specs/chmem-rv32um-coverage/spec.md` 确认 Requirement 成立（无需改，archived spec 已描述目标）
- [ ] 7.5 立项 follow-up（不启动）：`chmem-mext-fsm-cycle-parity`（blocked on `ctrl-link-chmem-reg-handle`）

## 8. Commit & Archive

- [ ] 8.1 `git add` int_alu_chmem.h + 快照基线 + 文档 + openspec change 目录
- [ ] 8.2 `git commit`（message 含 cycle 对比记录 + gate 结果）
- [ ] 8.3 `openspec change validate chmem-mext-comb-datapath` → 0 error
- [ ] 8.4 `openspec archive chmem-mext-comb-datapath -y`

---

**总计**: ≈ 30 tasks，≈ 1-1.5 天
**关键回归锚**: 38 rv32ui ELF（语义不变量）+ 8 rv32um ELF（功能）+ `[arith-poc]` 17 case
