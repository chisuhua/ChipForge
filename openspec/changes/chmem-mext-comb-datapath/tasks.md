# Tasks — chmem-mext-comb-datapath

> **Path**: rdd-builder（1 业务文件 + PoC 表达式定稿 + spec delta + 文档）
> **Est. total**: 1-1.5 天
> **Owner**: ip/cpu
> **Spec refs**: `specs/chmem-mext-datapath/spec.md` + `specs/chmem-rv32um-coverage/spec.md` + `specs/verilator-rv32um-coverage/spec.md`
> **Design refs**: `design.md`
> **Improvement draft**: `.rddf/improvements/chmem-mext-comb-datapath.md`
> **Decision basis**: oracle 2026-10-20 + metis 2026-10-20 + PoC `tests/framework/test_chmem_arith_poc.cpp` (commit `63bfc8a`/`03667fa`)

## 0. Pre-flight（文档层，必须最先做）

- [ ] 0.1 补 improvement 草案 `.rddf/improvements/chmem-mext-comb-datapath.md`（5 段：Why / What Changes / Acceptance / Capabilities / Impact）+ rdd-builder P0 落档
      **注意**：`.rddf/state/.planner-handoff.json` 是 2026-10-09 的**陈旧 handoff**（`proposals_ready` 只含 `mfc-extract-fsm-h`），**不得**当作覆盖本 change 的 planner 交接；本 change 已自带完整 proposal/design/tasks/spec，P0 以"user 批准既有 proposal"落档。
- [ ] 0.2 新增 2 个 delta spec（`## MODIFIED Requirements`，requirement 标题**逐字一致**，整块拷贝原 requirement 后仅改 cycle scenario）：
      `specs/chmem-rv32um-coverage/spec.md`（`### Requirement: CH_MEM Simulator 路径必须覆盖全部 8 个 rv32um ELF`）
      `specs/verilator-rv32um-coverage/spec.md`（`### Requirement: Verilator 仿真路径必须覆盖全部 8 个 rv32um ELF`）
      + 在 `specs/chmem-mext-datapath/spec.md` 追加"cycle 不可比"ADDED Requirement + ELF 特例覆盖映射
- [ ] 0.3 记录 baseline：`./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` → 当前 **10855 assertions / 18 failed**（8 rv32um 真 FAIL，tohost=3；38 rv32ui PASS）
- [ ] 0.4 记录 baseline：`./build/bin/chipforge_tests_chmem "[verilator]"` → 当前 **88 assertions / 8 failed**（8 rv32um **真 FAIL**，TOHOST=3；40 rv32ui PASS）
- [ ] 0.5 确认 `git status --short` 干净（PoC 已提交）
- [ ] 0.6 `openspec change validate chmem-mext-comb-datapath` → 0 error（改完文档先验一次，避免后面白做）

## 1. M-op 表达式定稿（PoC 层 —— 本 change 的地基，先于业务文件）

> **顺序理由**：PoC 是表达式的唯一真值来源。先在 PoC 把 8 条 M-op 定稿（含 safe divisor + 边界 case 全绿），再移植到 `int_alu_chmem.h`；反过来做会在移植途中二次改语义。

- [ ] 1.1 **8 条 M-op 表达式加 safe divisor（除零 guard）**：`tests/framework/test_chmem_arith_poc.cpp`
      - 无符号：`b_safe = select(b == 0, 1, b)` → 除/模吃 `b_safe`，输出再 `select(b == 0, …)` 覆盖特例值
      - 有符号：`b_safe_mag = select(abs32(b) == 0, 1, abs32(b))` → `mag = abs32(a) / b_safe_mag`；输出 `select(b == 0, all_ones / a, …)`
      - **禁止照抄现状**：`chmem_arith_poc_mext_divu`（`:272-280`）/ `_remu`（`:282-290`）是裸 `a/b` / `a%b`，**无 guard**
- [ ] 1.2 **补 5 个边界 case**（17 → 22 case），每个含 `INFO` 期望值：
      `divu(14,0) == 0xFFFFFFFF` / `remu(14,0) == 14` / `div(INT_MIN,-1) == 0x80000000` / `rem(INT_MIN,0) == 0x80000000` / `neg32(0x80000000) == 0x80000000`
      **注意**：PoC 现有 `chmem_arith_poc_mext_div`（`:292-306`）/ `_rem`（`:308-320`）只跑 `-14 / 4`，其除零/INT_MIN 分支**从未被驱动** → 边界 case 是首次覆盖
- [ ] 1.3 补 PoC 注释：说明"除零 guard 是 ELF 强制要求"（`divu`/`remu` test_8/9/10 以 `rs2 == 0` 驱动，见 spec 特例映射表）
- [ ] 1.4 `./build/bin/chipforge_tests_chmem "[arith-poc]"` → 22/22 PASS（**本 change 的表达式基线**）

## 2. IntAlu CH_MEM M-op 组合层（spec: chmem-mext-datapath）

- [ ] 2.1 在 `int_alu_chmem.h` 加 `is_mext = is_op && f7_01` + 8 个 M-op 判别（复用现有 `f3_0..f3_7`）
- [ ] 2.2 加符号-幅值 helper（`neg32` / `neg_flag` / `abs32`）—— **必须 `inline` 或放匿名 namespace**（`int_alu_chmem.h` 被多个 TU 包含；PoC 的 `test_chmem_arith_poc.cpp:213-221` 用的是匿名 namespace，移植时保持同等保护）
- [ ] 2.3 计算 8 条 M-op 结果 —— **移植 §1 定稿的表达式**（MUL / MULHU / MULH / MULHSU / DIVU / REMU / DIV / REM），含 safe divisor 与除零/INT_MIN 特例（design D4）
- [ ] 2.4 在现有 11 层 select 树（`int_alu_chmem.h:243` 之后、`addr_result` 之前）追加 8 层 M-op select
- [ ] 2.5 **必选**：给 6 个无 gate 的 base R-type op 的 `is_op` 侧补 `f7_00`（`is_sll`/`is_slt`/`is_sltu`/`is_xor`/`is_or`/`is_and`，`int_alu_chmem.h:155-169`），使 base 与 M 判别互斥、不依赖 select 顺序
      **注意**：只对 `is_op` 侧加，**不能**加到 `is_opimm`（I-type bits[31:25] 是立即数，见 `int_alu_chmem.h:148-150`）
- [ ] 2.6 `cmake --build build --target chipforge_tests_chmem -j$(nproc)` 编译过
- [ ] 2.7 **快速反馈**：单跑 1 个 ELF `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1" -c rv32um-p-mul`
      （Verilator codegen 路径的 128-bit 乘积 / `bits<63,32>` 未实测 —— PoC 只覆盖 interpreter。不要等全套 48 ELF 才发现宽度问题）

## 3. 功能验证

- [ ] 3.1 `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` → 8/8 rv32um 真 PASS + 38 rv32ui 不回归（46 total）
- [ ] 3.2 `./build/bin/chipforge_tests_chmem "[arith-poc]"` → 22/22 不回归
      （17 原有 + 5 新增边界 case。**计数口径**：5 个新 case 是纯组合式（无 `ch_reg`），**同时编入两个 binary** → CH_MEM 17→22、TLM 15→20；tasks 5.1 的 TLM 全量看到的是 20，不是 17）
- [ ] 3.3 记录 rv32um 每 ELF 的 cycle 数（INFO / stdout 输出）
      **不与 TLM radix-2 baseline 做 parity 比对**（组合式 1 拍 vs TLM DIV=33c，**不同构**，比对无意义 —— 见 delta spec）
      cycle 数记录进 commit message 供人工检视

## 4. Verilator 后端

- [ ] 4.1 `./build/bin/chipforge_tests_chmem "[verilator]"` → 8/8 rv32um 真 PASS（**真 PASS，不是 mask 假绿** —— runner 已于 commit `8aa0314` 改为 done/pass 双判据），`[mmu-verilator]` 3/3 不回归
- [ ] 4.2 `./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32um-p-div` 目检 work_dir 下 `.v` 含 `/`、`%`
- [ ] 4.3 若 `[verilator]` 8 rv32um 仍 FAIL → **按真 FAIL 定位根因**（不得回退到 mask 协议）
      **注意**：runner 协议已改 done/pass 双判据 + TIMEOUT 三态（`tools/verilator_runner/sim_main.cpp:95-99`）；本 change **不修改** `sim_main.cpp` 的判据，若确需修改须另立 change

## 5. Regression

- [ ] 5.1 `./build/bin/chipforge_tests` 全量 TLM → 无新增 FAIL（`7stage_topology_from_config` pre-existing infra drift 除外）
- [ ] 5.2 `./build/bin/chipforge_tests_chmem "[chmem]"` 全套 → 0 FAIL（**注意 >300s**）
- [ ] 5.3 记录 `[chmem]` family 耗时 before/after（补 CHANGELOG family 耗时口径的量化缺口；CI timeout 360s+）
- [ ] 5.4 `bash tools/run_chipforge_tests.sh --all` 全量（可选，~167s+）

## 6. Architecture Gates

- [ ] 6.1 `bash tools/verify_adr.sh` → 0 fail
- [ ] 6.2 `bash tools/verify_plugin_decision.sh` → 8/8 PASS
- [ ] 6.3 `bash tools/check_plugin_portability.sh` → 12/12 PASS（int_alu_chmem.h 本就含 ch_*，合规）
- [ ] 6.4 `bash tools/roadmap-r9-check.sh` → 0 fail

## 7. Documentation

- [ ] 7.1 `ip/cpu/plugins/mul_div_fsm_chmem.h` 文件头加注释：mechanism fixture, not a datapath candidate（见 spec Non-goals）
      **同时回收 `:149-150` 的旧注释**（"上游 CppHDL `ch_uint` 算术语义异常" —— 已被 PoC `63bfc8a` 证伪）
- [ ] 7.2 `AGENTS.md` `[chmem][m-ext-gap]` 条目更新：标记 CH_MEM M 扩展组合式落地 + 更正"算术语义异常 / lock race"旧结论
      **术语统一**：AGENTS.md 原文用 "path-B (ch_uint 算术) / fallback path-A (32 层 select 真算术)"，与本 change 的"路径 A = 组合式 / 路径 B = FSM"**相反** → 改用无歧义表述："组合式路径（本 change 落地）" / "FSM 路径（follow-up `chmem-mext-fsm-cycle-parity`）"
- [ ] 7.3 `CHANGELOG.md` 更正 **3 处**：
      ① `:443`（"ch_uint 算术语义异常" → 已由 PoC 证伪）
      ② `:456-457`（"ch_reg lock 顺序 race" → 已由 PoC 证伪）
      ③ 2026-10-20 Correction Note **自身**仍把失败归因于"因 CppHDL `ch_uint` 算术语义异常 → 真算术路径不通" → 归因更正为"CH_MEM 数据通路未实现 M-op"
- [ ] 7.4 `CHANGELOG.md` 引用的空目录 `openspec/changes/chmem-m-ext-integration/`（**不存在**）→ 修正引用指向本 change `chmem-mext-comb-datapath`
- [ ] 7.5 立项 follow-up（不启动）：`chmem-mext-fsm-cycle-parity`（blocked on `ctrl-link-chmem-reg-handle`）
      **方案候选备注**（供 follow-up 选型，不在本 change 展开）：① 真 FSM 33c（需 CtrlLink 句柄扩展）② 32 级 sub/shift 恢复除法纯组合网络（无 `div_op`/`mod_op` 节点 → 顺带解掉 stderr 刷屏与可综合性）
- [ ] 7.6 `.rddf/roadmap/features/feat-rv32um-m-extension.md`：标注 CH_MEM datapath 侧 owner 已由本 change 补齐
- [ ] 7.7（可选，弱约定）`.openspec.yaml` 记录本项目 OpenSpec 约定（delta spec 用 `## MODIFIED Requirements` 且 requirement 标题逐字一致）

## 8. Commit & Archive

- [ ] 8.1 `git add` int_alu_chmem.h + test_chmem_arith_poc.cpp + 2 个 delta spec + 文档 + openspec change 目录 + `.rddf/improvements/chmem-mext-comb-datapath.md`
- [ ] 8.2 `git commit`（message 含 cycle 记录 + 3 架构 gate 结果 + `[chmem]` 耗时 before/after）
- [ ] 8.3 `openspec change validate chmem-mext-comb-datapath` → 0 error
- [ ] 8.4 `openspec archive chmem-mext-comb-datapath -y`
      **验证 delta apply**：确认 `openspec/specs/chmem-rv32um-coverage/spec.md` 与 `openspec/specs/verilator-rv32um-coverage/spec.md` 的 cycle scenario 被**替换**（而非追加重复 requirement）

---

**总计**: ≈ 34 tasks，≈ 1-1.5 天
**关键回归锚**: 38 rv32ui ELF（RV32I 语义不变量，`is_mext` 恒 false）+ 8 rv32um ELF（功能，含 div/rem/divu/remu test_7~test_10 特例）+ `[arith-poc]` 22 case（表达式白盒基线）
**已知缺口（不在本 change 修）**: CH_MEM ALU **无单元级语义测试**（`m3_poc_alu_elaborate` 只断言 `alu.v` 生成非空，不是语义锚）→ 单元级 ALU 语义测试归 `chmem-mext-fsm-cycle-parity` 或独立 change
