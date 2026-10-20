---
id: chmem-mext-comb-datapath
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P1
manual_deps: []
review_by: 2026-10-27
---

# chmem-mext-comb-datapath

## 1. Why

### 项目上下文（v0.11.0 Wave 5 ISA 覆盖窗口）

`feat-rv32um-m-extension`（phase-4，AGENTS.md feature fragments 表）要求 RV32UM M 扩展在 **TLM 与 CH_MEM 两条路径**上都有真实实现。TLM 侧已由 `mfc-extract-fsm-h`（archived 2026-10-10）落地真 radix-2 iterative（`ip/cpu/arch/riscv/mul_div_fsm.h:207-274`，MUL=1c / MULH=3c / DIV=33c），`[riscv-tests]` rv32um 8/8 PASS。

**CH_MEM 侧一直是 0 实现**。

### 缺口（HEAD `3b4f6d9` 实测，非推断）

| 证据 | 位置 | 结论 |
|---|---|---|
| 11 层 select 树仅 RV32I base op，无 `funct7==0x01` 判别 | `ip/cpu/arch/riscv/int_alu_chmem.h:128-243` | M-op 无实现 |
| `MulDivFsmPlugin` 未注册 | `ip/cpu/cpu_factory_chmem.h:322-333`（TLM `ip/cpu/cpu_factory.h:473-493` 有 12 处注册） | CH_MEM 无 M 通路 |
| stub `result_reg_` 用 `ch_literal<12>` 占位，不做算术 | `ip/cpu/plugins/mul_div_fsm_chmem.h:149-165` | stub 仅是 DSL 机制 fixture |
| `[chmem] chmem_vendored_elf_tohost1` → 10855 assertions / **18 failed** | HEAD 实测 | 8 个 rv32um ELF 全 FAIL（`tohost=3`），38 rv32ui PASS |
| `[verilator]` → 88 assertions / **8 failed** | HEAD 实测 | 8 个 rv32um **真 FAIL**（`TOHOST=3`），40 rv32ui PASS |

> **口径更正（2026-10-20）**：AGENTS.md `[chmem][m-ext-gap]` 与早期 CHANGELOG 记的"8/8 PASS 是 mask 假绿"**已过时**。runner 协议已于 commit `8aa0314` 改为 done/pass 双判据 + TIMEOUT 三态（`tools/verilator_runner/sim_main.cpp:95-99`），当前 FAIL 是真实 FAIL。

### 两条被证伪的旧结论（PoC `63bfc8a` + `03667fa`）

- **"ch_uint 算术语义异常（14/4=1, 3*4=16）"** → 不存在。PoC Part A 实测 `14/4=3`、`3*4=12`、`14%4=2`；上游 `f790ca6` 已修且在 HEAD。
- **"ch_reg lock 顺序 race"** → 不存在。rev1 PoC 在 WRITE_BACK 拍把 opcode 归零所致。

**结论**：这不是"上游算术不可用"，而是**CH_MEM 数据通路缺 M-op 实现**。缺口是本项目自己的，不是 CppHDL 的。

### 紧迫性（不夸大）

本项**不卡 v0.11.0 launch gate** —— `AGENTS.md:215` 明写 "v0.11.0 launch gate **不卡**"，且 launch gate 判据走 TLM 路径的 `[riscv-tests] 48/48`（已独立 PASS）。紧迫性来自 `feat-rv32um-m-extension` 的 **CH_MEM 侧 owner 长期缺口**，不是版本门禁。

## 2. What Changes

### 变更范围

- **`ip/cpu/arch/riscv/int_alu_chmem.h`**（唯一业务文件）：在现有 11 层 select 树**尾部**追加 `funct7==0x01` gate 的 8 层 M-op select；**必选**给 6 个无 funct7 gate 的 base R-type op 的 `is_op` 侧补 `f7_00` gate（不依赖 select 顺序）。
- **`tests/framework/test_chmem_arith_poc.cpp`**：8 条 M-op 表达式**定稿** —— 全部加 safe divisor（除零 guard），补 7 个边界 case（`divu(14,0)` / `remu(14,0)` / `div(14,0)` / `div(INT_MIN,-1)` / `rem(INT_MIN,-1)` / `rem(INT_MIN,0)` / `neg32(0x80000000)`）。**先于**业务文件落地。
- **2 个 delta spec**：`specs/chmem-rv32um-coverage/spec.md` + `specs/verilator-rv32um-coverage/spec.md`，用 `## MODIFIED Requirements` 修订 cycle 比对场景。
- **文档**：`AGENTS.md` `[chmem][m-ext-gap]` 条目 + `CHANGELOG.md` 3 处旧结论更正。

### 非目标（Out of Scope）

- **不注册 `MulDivFsmPlugin`** 到 `cpu_factory_chmem.h`：stub 输出不进 RD_DATA 通路，注册只产生死节点。stub 文件保留（DSL 机制 fixture），仅加注释。
- **不做多周期 FSM / 不引入 stall**：CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器），execute 当拍必须出 `RD_DATA`；N 拍 FSM 的结果到达时该指令已流过写回点，且无 stall 机制把晚到结果写回正确的 rd → **组合与多周期之间无中间态**。cycle parity 移交 follow-up `chmem-mext-fsm-cycle-parity`。
- **不改 TLM 路径**：`mul_div_fsm.h` 与 `cpu_factory.h` 注册表不动。
- **不改框架**（无 CtrlLink API 扩展）。

## 3. Acceptance

- [ ] `[chmem] chmem_vendored_elf_tohost1` → **46/46 PASS**（38 rv32ui + 8 rv32um 真 PASS，tohost==1 非 mask 产物）
- [ ] `[verilator]` → **48/48 PASS**（40 rv32ui + 8 rv32um 真 PASS）；`[mmu-verilator]` 3/3 不回归
- [ ] `[arith-poc]` → CH_MEM 24/24 PASS（TLM 22/22；17+7 与 15+7 —— 7 个新 case 是纯组合式，同时编入两个 binary），覆盖 design D4 边界自洽表全部 6 行 + neg32 宽度：`divu(14,0)==0xFFFFFFFF`、`remu(14,0)==14`、`div(14,0)==0xFFFFFFFF`、`div(INT_MIN,-1)==0x80000000`、`rem(INT_MIN,-1)==0`、`rem(INT_MIN,0)==0x80000000`、`neg32(0x80000000)==0x80000000`
- [ ] 3 架构 gate 全 PASS（`verify_adr` / `verify_plugin_decision` 8/8 / `check_plugin_portability` 12/12）+ `roadmap-r9-check` 0 fail
- [ ] 全量 TLM binary 无新增 FAIL（`7stage_topology_from_config` pre-existing infra drift 除外）
- [ ] `openspec change validate chmem-mext-comb-datapath` → 0 error；archive 后 2 个 delta spec 正确 apply 到 `openspec/specs/{chmem-rv32um-coverage,verilator-rv32um-coverage}/spec.md`
- [ ] 文档更正落地（AGENTS.md + CHANGELOG 3 处）

## 4. Capabilities

- **CH_MEM RV32UM 组合式数据通路**（新增 capability `chmem-mext-datapath`）：CH_MEM 路径 IntAlu 组合式实现全部 8 条 RV32M op（CppHDL 无 `ch_int` → sign-magnitude），无 FSM、无 stall、D4 合规（无 tick、无运行期 `if(ch_bool)`）。
- **RV32I 语义不变量**：M-op 判别 gate `funct7 == 0x01` 与 RV32I 的 `funct7 ∈ {0x00, 0x20}` 互斥 → `is_mext` 在 RV32I 路径恒 false，38 rv32ui ELF 端到端锚定"逐位不变"。
- **显式除零语义**：DIV/DIVU 除零 → 全 1；REM/REMU 除零 → 被除数；DIV(INT_MIN,-1) → INT_MIN；REM(INT_MIN,-1) → 0。**不依赖 CppHDL 默认除零行为**（`CppHDL/include/ast/instr_op_arith.h:52-73` 返回 0 并写 stderr）—— vendored ELF 的 `divu`/`remu` test_8/9/10 明确以 `rs2 == 0` 驱动。
- **cycle 不可比性显式声明**（新增）：组合式 CH_MEM M-op 与 TLM radix-2 FSM 不同构 → cycle parity 判据在组合式路径**不适用**，经 delta spec 改判并移交 `chmem-mext-fsm-cycle-parity`。

## 5. Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `ip/cpu/arch/riscv/int_alu_chmem.h` | 改（唯一业务文件） | 11 层 select 树 → 19 层；`is_mext` gate + 8 层 M-op select + 6 个 base op 的 `f7_00` 加固；RV32I 路径 `is_mext` 恒 false |
| `tests/framework/test_chmem_arith_poc.cpp` | 改 | 8 条表达式加 safe divisor；+7 边界 case（17 → 24 case） |
| `openspec/specs/chmem-rv32um-coverage/spec.md` | 改（archive 时） | cycle scenario 的 ±5%/+50c MUST → 不适用 + 移交 follow-up |
| `openspec/specs/verilator-rv32um-coverage/spec.md` | 改（archive 时） | 同上 |
| `AGENTS.md` | 改 | `[chmem][m-ext-gap]` 条目标记 resolved + 旧结论更正 + path 术语统一 |
| `CHANGELOG.md` | 改 | 3 处旧结论更正（`:443` 算术语义异常、`:456-457` lock race、2026-10-20 Correction Note 自身归因） |
| `ip/cpu/plugins/mul_div_fsm_chmem.h` | 改（仅注释） | 标注 mechanism fixture，非 datapath 候选 |

### 风险

- **R-DIV0（高）**：照抄 PoC 的裸 `a/b`/`a%b`（无 guard）→ 除零得 0 而非 0xFFFFFFFF → 8 ELF 继续 FAIL。缓解：safe divisor 落地 + 5 边界 case 先定稿 + ELF test_7~test_10 e2e 锚。
- **R-NEGWIDTH（中）**：`neg32(0x80000000)` 若宽度增长 → 0x80000001，INT_MIN÷-1 静默错。缓解：PoC 钉死断言。
- **R-SYNTH（中）**：组合除法 emit Verilog `/`/`%`，真实综合 = 巨型组合除法器。缓解：本 change 声明"组合 M-op = 仿真正确性基线；可综合版 = follow-up 的 FSM/32 级恢复除法"。
- **R-CYCLE（中）**：cycle parity 判据不适用 → 若不改归档 spec，本 change 落地后 spec 仍永久不可满足。缓解：2 个 delta spec（`## MODIFIED Requirements`）。
- **回退**：独立 commit，`git revert` 即可。

### 下游依赖

- `feat-rv32um-m-extension`（phase-4）：本 change 补齐其 CH_MEM datapath 侧 owner。
- `chmem-mext-fsm-cycle-parity`（follow-up，本 change 不启动）：cycle parity + 可综合除法器，blocked on `ctrl-link-chmem-reg-handle`（CtrlLink `halt_when` 只接 ch_bool，需扩展 `ch_reg<ch_bool>` 句柄，见 `ip/cpu/plugins/hazard_chmem.h:163-167`）。
