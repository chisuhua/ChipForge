---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on: []  # 无 active change 前置; 前例 chmem-verilator-rv32um-coverage 已 archived 2026-10-13
supersedes: []
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义)
  - ADR-083 (Plugin-style 单一 source of truth)
  - ADR-046 v2.0 (多周期 FSM 豁免 — 本 change 不采用, 记于 Non-Goals)
  - feat-rv32um-m-extension.md (phase-4, 本 change = 其 CH_MEM datapath 侧实现)
  - openspec/specs/chmem-rv32um-coverage/spec.md (本 change 以 MODIFIED delta 修订其 cycle 场景; 覆盖类 Requirement 由本 change 兑现)
---

# chmem-mext-comb-datapath — CH_MEM 路径 RV32UM M 扩展组合式数据通路

> **范围 (scope)**：改 `ip/cpu/arch/riscv/int_alu_chmem.h` 一个业务文件（追加 M-op 组合层）+ PoC 表达式定稿 + spec delta + 文档。**不新增 plugin、不改 `cpu_factory_chmem.h` 注册表、不引入 FSM/stall。**
> **触发 (trigger)**：`[chmem] chmem_vendored_elf_tohost1` 的 8 个 rv32um ELF FAIL；根因经 PoC 定位（见 §1）。
> **目标版本**：v0.11.0。
> **治理路径**：rdd-builder（1 业务文件 + PoC 表达式定稿 + spec delta，非 rdd-quick —— 触及共享 datapath select 树 + Verilog 生成，需 gate 验证）。

## Why

### 1.1 当前状态 (2026-10-20 实测)

CH_MEM 路径**完全没有 RV32UM M 扩展处理**：

- `ip/cpu/arch/riscv/int_alu_chmem.h:173-243` 是 11 层 select 树，仅 RV32I base op，**无 `funct7==0x01` 判别**。
- `ip/cpu/plugins/mul_div_fsm_chmem.h:149-171` 的 `result_reg_`/`busy_reg_` 用 `ch_literal<12>`/`<3>`/`<33>` **写死占位**，根本不计算 —— 它只是 `test_chmem_multi_cycle_fsm.cpp` 的 DSL 机制 fixture。
- 症状：`[chmem] chmem_vendored_elf_tohost1` 8 个 rv32um ELF FAIL（tohost != 1）。
- 连带：`[verilator]` 的 8 个 rv32um 现为**真 FAIL**（HEAD `3b4f6d9` 实测：88 assertions / 8 failed，`TOHOST=3`）。早期"mask 假绿"的说法**已过时** —— runner 协议已于 commit `8aa0314` 改为 done/pass 双判据 + TIMEOUT 三态（`tools/verilator_runner/sim_main.cpp:95-99`），当前 FAIL 是真实 FAIL。

### 1.2 PoC 结论（2026-10-20，commit `63bfc8a` + `03667fa`）

`tests/framework/test_chmem_arith_poc.cpp`（双 binary，TLM 15 case / CH_MEM 17 case 全 PASS）推翻两条旧结论：

- **~~"ch_uint 算术语义异常（14/4=1, 3*4=16）"~~ 不存在**：Part A 证明 `14/4=3`、`3*4=12`、`14%4=2`、MULHU 高位正确；Part A' 用 CHANGELOG 报错的整数构造器形式 `ch_uint<32>(14)` 复现，同样正确。归因：上游 `f790ca6`（interpreter ADD/MUL/SHL/NEG 宽度截断）已修且在 HEAD，install 库 2026-10-18 重建。
- **~~"ch_reg lock 顺序 race"~~ 不存在**：Part B 用正确 tick 序列得 `result=12`/`=3`；既有 fixture `test_chmem_multi_cycle_fsm.cpp:188` 早就断言 `result==3` 且 PASS。旧结论源于 rev1 PoC 在 WRITE_BACK 拍把 opcode 归零。
- **Part D（决定性）**：用 `ch_uint` + `sext`/`zext` 组合式实现全部 8 条 M-op（含符号-幅值转换，CppHDL 无 `ch_int`），**8/8 PASS** → 组合式路径的**正常路径**语义可行。
  **注意（实现前置）**：Part D 的 DIV/REM 只跑过 `-14 / 4`，其除零 / INT_MIN 特例分支**从未被驱动**；DIVU/REMU 是裸 `a/b`/`a%b`、**无除零 guard**（`tests/framework/test_chmem_arith_poc.cpp:272-290`）→ 落地顺序必须是"**先在 PoC 把表达式定稿**（加 safe divisor + 补 5 个边界 case），再移植到 `int_alu_chmem.h`"，而非逐字照抄 PoC。

### 1.3 风险分析（为何现在做）

- `[chmem]`/`[verilator]` 口径下 rv32um 不真绿，`openspec/specs/chmem-rv32um-coverage/spec.md` 的**覆盖类** Requirement 实际未成立。
  **紧迫性口径（不夸大）**：本项**不卡 v0.11.0 launch gate** —— `AGENTS.md:215` 明写 "v0.11.0 launch gate **不卡**"，且 launch gate 判据走 TLM 路径的 `[riscv-tests] 48/48`（已独立 PASS）。紧迫性来自 `feat-rv32um-m-extension` 的 **CH_MEM 侧 owner 长期缺口**，不是版本门禁。
- `feat-rv32um-m-extension`（phase-4）的 CH_MEM datapath 侧一直缺 owner；本 change 补齐。
- 组合式实现 1 拍出结果，与 CH_MEM "全组合数据通路"结构同构；测试只断言 `tohost==1`（无 cycle 断言），无需 stall。
- **cycle parity 不在本 change 范围**：组合式与 TLM radix-2 多周期 FSM **不同构**（`ip/cpu/arch/riscv/mul_div_fsm.h:144-148` — MUL=1c / MULH=3c / DIV=33c），归档 spec 的 "±5% / +50c" 比对判据在组合式路径**不适用** → 本 change 以 `## MODIFIED Requirements` delta spec 改判该场景，cycle parity 移交 follow-up `chmem-mext-fsm-cycle-parity`。

## What Changes

| # | 文件 | 改动 |
|---|---|---|
| 1 | `ip/cpu/arch/riscv/int_alu_chmem.h` | 在现有 11 层 select 树**尾部**追加 `funct7==0x01` gate 的 8 层 M-op select（移植 design D4 定稿表达式）。**必选**：给 6 个无 funct7 gate 的 base R-type op 的 `is_op` 侧补 `f7_00` gate。 |
| 2 | `tests/framework/test_chmem_arith_poc.cpp` | **表达式定稿**：8 条 M-op 全部加 safe divisor（除零 guard），补 5 个边界 case（17 → 22 case）。**先于** `int_alu_chmem.h` 改动落地 —— PoC 是表达式的唯一真值来源。 |
| 3 | `openspec/changes/chmem-mext-comb-datapath/specs/{chmem-rv32um-coverage,verilator-rv32um-coverage}/spec.md` | **新增** 2 个 `## MODIFIED Requirements` delta spec：修订 cycle 比对场景（组合式与 TLM radix-2 不同构 → parity 判据不适用，移交 follow-up）。requirement 标题须与归档 spec **逐字一致**。 |
| 4 | `AGENTS.md` `[chmem][m-ext-gap]` 条目 | 修正/标记 resolved（含"算术语义异常 / lock race"旧结论更正 + path 术语统一）。 |
| 5 | `CHANGELOG.md` | 更正 **3 处**旧结论（`:443` 算术语义异常、`:456-457` lock race、2026-10-20 Correction Note 自身的归因）+ 修正对不存在的 `openspec/changes/chmem-m-ext-integration/` 的引用。 |

**Non-Goals（明确不做）**：

- **不注册 `MulDivFsmPlugin`** 到 `cpu_factory_chmem.h`：stub 输出不进 RD_DATA 通路，注册只会产生死节点。stub 文件**保留不改**（DSL 机制 fixture），仅加一行注释标明。
- **不做 cycle-accurate 多周期 FSM**：降级为独立 follow-up change `chmem-mext-fsm-cycle-parity`，前置 = 解 `hazard_chmem.h:163-167` 记录的 CtrlLink API gap（`ctrl-link-chmem-reg-handle`）。本 change 不夹带框架级 API 扩展。
- **不改 TLM 路径**：`mul_div_fsm.h`（真 radix-2）与 `cpu_factory.h` 注册表不动。

## Impact

- **受益**：`[chmem]` 8 rv32um 真 PASS；`[verilator]` 8 rv32um 由**真 FAIL 转真 PASS**；`chmem-rv32um-coverage` / `verilator-rv32um-coverage` 的**覆盖类** Requirement 成立（cycle 场景经 delta spec 改判为"不适用 + 移交 follow-up"）。
- **回归面**：RV32I 合法指令 `funct7 ∈ {0x00,0x20}` → `is_mext` 恒 false → 38 rv32ui 执行路径逐位不变（语义不变量，由 38 rv32ui ELF 端到端锚定）。
  **无 DAG 快照需更新**：`node_count()` 是按 stage 去重的 PipeNode **拓扑计数**（`include/cf/plugin/pipe_builder.h:283`），不随 select 树层数（11→19）变化。
- **风险**：组合除法 emit Verilog `/`（Verilator OK，可综合差 → 由 FSM follow-up 长期兜底）；MULH 64-bit 中间宽度 —— 由 PoC Part D 已验证的表达式移植缓解；**除零 / INT_MIN 特例** —— PoC Part D **从未实测**这两条路径（只跑 `-14 / 4`），由 tasks §1 的 5 个边界 case 定稿 + vendored ELF 的 div/rem/divu/remu test_7~test_10 兜底。

## Acceptance

见 `specs/chmem-mext-datapath/spec.md`。摘要：

1. `[chmem] chmem_vendored_elf_tohost1` 8/8 rv32um 真 PASS（tohost==1，非 mask 产物），38 rv32ui 不回归。
2. `[verilator]` 8/8 rv32um 真 PASS，`[mmu-verilator]` 3/3 不回归。
3. 3 架构 gate 全 PASS。
4. 全量 TLM binary 无新增 FAIL（`7stage_topology_from_config` pre-existing infra drift 除外）。
5. 文档更正（AGENTS.md + CHANGELOG 3 处）。
6. `openspec change validate chmem-mext-comb-datapath` → 0 error；archive 后 2 个 delta spec 正确 apply 到 `openspec/specs/{chmem-rv32um-coverage,verilator-rv32um-coverage}/spec.md`（cycle scenario 被**替换**而非追加重复 requirement）。
7. `[arith-poc]` 22/22 PASS（17 原有 + 5 个边界 case）。
