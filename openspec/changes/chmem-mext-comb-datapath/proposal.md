---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: draft
depends_on: []  # 无 active change 前置; 前例 chmem-verilator-rv32um-coverage 已 archived 2026-10-13
supersedes: []
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义)
  - ADR-083 (Plugin-style 单一 source of truth)
  - ADR-046 v2.0 (多周期 FSM 豁免 — 本 change 不采用, 记于 Non-Goals)
  - feat-rv32um-m-extension.md (phase-4, 本 change = 其 CH_MEM datapath 侧实现)
  - openspec/specs/chmem-rv32um-coverage/spec.md (本 change 让其 Requirement 真正成立)
---

# chmem-mext-comb-datapath — CH_MEM 路径 RV32UM M 扩展组合式数据通路

> **范围 (scope)**：改 `ip/cpu/arch/riscv/int_alu_chmem.h` 一个业务文件（追加 M-op 组合层）+ DAG 快照基线 + 文档。**不新增 plugin、不改 `cpu_factory_chmem.h` 注册表、不引入 FSM/stall。**
> **触发 (trigger)**：`[chmem] chmem_vendored_elf_tohost1` 的 8 个 rv32um ELF FAIL；根因经 PoC 定位（见 §1）。
> **目标版本**：v0.11.0。
> **治理路径**：rdd-builder（1 业务文件 + 快照基线，非 rdd-quick —— 触及共享 datapath select 树 + Verilog 生成，需 gate 验证）。

## Why

### 1.1 当前状态 (2026-10-20 实测)

CH_MEM 路径**完全没有 RV32UM M 扩展处理**：

- `ip/cpu/arch/riscv/int_alu_chmem.h:105-258` 是 11 层 select 树，仅 RV32I base op，**无 `funct7==0x01` 判别**。
- `ip/cpu/plugins/mul_div_fsm_chmem.h:149-171` 的 `result_reg_`/`busy_reg_` 用 `ch_literal<12>`/`<3>`/`<33>` **写死占位**，根本不计算 —— 它只是 `test_chmem_multi_cycle_fsm.cpp` 的 DSL 机制 fixture。
- 症状：`[chmem] chmem_vendored_elf_tohost1` 8 个 rv32um ELF FAIL（tohost != 1）。
- 连带：`[verilator]` 的 8 个 rv32um 现为"mask 假绿"（`verilator-mul-div-fsm-followup` 的 `& 0x1u` mask 掩盖了真 FAIL，见 AGENTS.md `[chmem][m-ext-gap]`）。

### 1.2 PoC 结论（2026-10-20，commit `63bfc8a` + `03667fa`）

`tests/framework/test_chmem_arith_poc.cpp`（双 binary，TLM 15 case / CH_MEM 17 case 全 PASS）推翻两条旧结论：

- **~~"ch_uint 算术语义异常（14/4=1, 3*4=16）"~~ 不存在**：Part A 证明 `14/4=3`、`3*4=12`、`14%4=2`、MULHU 高位正确；Part A' 用 CHANGELOG 报错的整数构造器形式 `ch_uint<32>(14)` 复现，同样正确。归因：上游 `f790ca6`（interpreter ADD/MUL/SHL/NEG 宽度截断）已修且在 HEAD，install 库 2026-10-18 重建。
- **~~"ch_reg lock 顺序 race"~~ 不存在**：Part B 用正确 tick 序列得 `result=12`/`=3`；既有 fixture `test_chmem_multi_cycle_fsm.cpp:188` 早就断言 `result==3` 且 PASS。旧结论源于 rev1 PoC 在 WRITE_BACK 拍把 opcode 归零。
- **Part D（决定性）**：用 `ch_uint` + `sext`/`zext` 组合式实现全部 8 条 M-op（含符号-幅值转换，CppHDL 无 `ch_int`；含除零/INT_MIN 特例），**8/8 PASS** → 组合式路径语义可行。

### 1.3 风险分析（为何现在做）

- v0.11.0 launch gate 的 `[chmem]`/`[verilator]` 口径下 rv32um 不真绿，`openspec/specs/chmem-rv32um-coverage/spec.md` 的 Requirement 实际未成立。
- `feat-rv32um-m-extension`（phase-4）的 CH_MEM datapath 侧一直缺 owner；本 change 补齐。
- 组合式实现 1 拍出结果，与 CH_MEM "全组合数据通路"结构同构；测试只断言 `tohost==1`（无 cycle 断言），无需 stall。

## What Changes

| # | 文件 | 改动 |
|---|---|---|
| 1 | `ip/cpu/arch/riscv/int_alu_chmem.h` | 在现有 11 层 select 树**尾部**追加 `funct7==0x01` gate 的 8 层 M-op select（移植 PoC Part D 表达式）。可选双保险：给 6 个无 funct7 gate 的 base R-type op 的 `is_op` 侧补 `f7_00` gate。 |
| 2 | DAG 快照基线（如存在 node_count 断言） | 同步更新（select 树 11→19 层，node 数必然变化）。 |
| 3 | `AGENTS.md` `[chmem][m-ext-gap]` 条目 | 修正/标记 resolved（含"算术语义异常/lock race"旧结论更正）。 |
| 4 | `CHANGELOG.md` v0.10.3 Known Limitations | 加更正注解（旧 PoC 结论已证伪）。 |

**Non-Goals（明确不做）**：

- **不注册 `MulDivFsmPlugin`** 到 `cpu_factory_chmem.h`：stub 输出不进 RD_DATA 通路，注册只会产生死节点。stub 文件**保留不改**（DSL 机制 fixture），仅加一行注释标明。
- **不做 cycle-accurate 多周期 FSM**：降级为独立 follow-up change `chmem-mext-fsm-cycle-parity`，前置 = 解 `hazard_chmem.h:163-167` 记录的 CtrlLink API gap（`ctrl-link-chmem-reg-handle`）。本 change 不夹带框架级 API 扩展。
- **不改 TLM 路径**：`mul_div_fsm.h`（真 radix-2）与 `cpu_factory.h` 注册表不动。

## Impact

- **受益**：`[chmem]` 8 rv32um 真 PASS；`[verilator]` 8 rv32um 由假绿转真绿；`chmem-rv32um-coverage` spec Requirement 真正成立。
- **回归面**：RV32I 合法指令 `funct7 ∈ {0x00,0x20}` → `is_mext` 恒 false → 38 rv32ui 执行路径逐位不变（语义不变量）。DAG node 数变化仅影响快照断言，不影响功能。
- **风险**：组合除法 emit Verilog `/`（Verilator OK，可综合差 → 由 FSM follow-up 长期兜底）；MULH 64-bit 中间宽度；除零/INT_MIN 特例 —— 均由 PoC Part D 已覆盖的精确表达式直接移植缓解。

## Acceptance

见 `specs/chmem-mext-datapath/spec.md`。摘要：

1. `[chmem] chmem_vendored_elf_tohost1` 8/8 rv32um 真 PASS（tohost==1，非 mask 产物），38 rv32ui 不回归。
2. `[verilator]` 8/8 rv32um 真 PASS，`[mmu-verilator]` 3/3 不回归。
3. 3 架构 gate 全 PASS。
4. 全量 TLM binary 无新增 FAIL（`7stage_topology_from_config` pre-existing infra drift 除外）。
5. 文档更正（AGENTS.md + CHANGELOG）。
