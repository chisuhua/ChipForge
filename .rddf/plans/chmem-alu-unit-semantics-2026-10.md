# Plan — CH_MEM ALU 单元级语义锚 (P2)

> **来源**: `chmem-mext-comb-datapath` 验收发现的 known issue —— AGENTS.md:215 自述
> "CH_MEM ALU **无单元级语义测试**（`m3_poc_alu_elaborate` 只验 `alu.v` 生成，非语义锚）"。
> **目标**: 给 `ip/cpu/arch/riscv/int_alu_chmem.h` 的 select 树建**真正跑真实 datapath** 的单元语义锚，
> 覆盖 8 条 M-op + RV32I 回归 + **判别互斥**（M-op 不被同 funct3 的 base op 误捕）。
> **日期**: 2026-10-20

---

## 0. 现状（为何需要）

`tests/cpu/test_cpu_rtl_regfile_alu.cpp` 现有 4 个"ALU 测试"里：

| case | 实际测什么 | 是否跑真实 select 树 |
|---|---|---|
| `m3_poc_alu_elaborate` | 只验 `alu.v` 非空 + 含 "module" | ❌ 无语义 |
| `m3_poc_alu_byte_equal` | 比对 `tlm_alu_compute` vs `chmem_ref_alu`（**两份手写 C++ 镜像**） | ❌ 与真实实现无关 |
| `m3_poc_5stage_byte_equal` | 比对 `tlm_alu_compute_op` vs `chmem_alu_compute_op`（**又是两份手写 C++ 镜像**） | ❌ 与真实实现无关 |
| `m3_poc_regfile_alu_combined` | 只验 elaboration 不崩 + verilog 非空 | ❌ 无语义 |

→ **现有"byte-equal"是 C++ 镜像自比**，对 `int_alu_chmem.h` 的 select 树零覆盖，却给出"已对标"的虚假信心。
`tests/framework/test_chmem_arith_poc.cpp` Part D 虽覆盖 8 条 M-op 表达式，但那是**裸 `ch_uint` 表达式**，
不经 `RiscvIntAluPlugin`，**不锚 datapath 接线**（gate / select 顺序 / RD_DATA 写者）。

**风险场景（当前无任何测试能发现）**：
- `is_mext` gate 写错（如漏 `funct7==0x01`）→ M-op 被 base op 误捕
- M-op select 层插错位置（插在 base 树**之前**）→ 被 base op 覆盖
- `f7_00` 加固误加到 `is_opimm` 侧 → 负立即数 I-type 指令全错
- `safe_divisor` 宽度退化 → 除零/取模结果静默错

## 1. 机制（可行性已初步核实）

1. `pb.build()` → ALU 的 `build()` 调 `pb.at_stage("execute", NORMAL, closure)` → `PipeBuilder::at_stage`
   **自动创建** "execute" 节点（`pipe_builder.h:116-118`）。
2. 闭包在 `pb.elaborate()` 期执行一次，从 payload 读 `RS1`/`RS2`/`PC` + `DECODED_INST`。
3. ⇒ **在 `elaborate()` 之前**把 payload 填成 **literal 值**，`elaborate()` 时 select 树即按该组
   opcode/funct3/funct7 + 操作数建 DAG。
4. `pb.create_simulator()` → `sim->tick()` → `sim->get_value(n->operator()(KT::RD_DATA))` 取回数值。

**已验证的先例**：`test_chmem_arith_poc.cpp` Part A 用 `ch::Simulator(ctx)` + `sim.get_value(...)` 对
**纯组合 DAG** 取值成功（含 `sext`/`zext`/`bits`/128-bit 乘积）；`m3_poc_5stage_byte_equal` 用
`pb->create_simulator()` + `sim->tick()` 成功。

## 2. 改动清单

| # | 文件 | 动作 | 说明 |
|---|------|------|------|
| 1 | `tests/cpu/test_cpu_chmem_alu_semantics.cpp` | **新增** | 真正的 ALU 单元语义测试（`#ifdef CF_PLUGIN_USE_CH_MEM`；**不用 `<ch.hpp>`**，避免 module.h ODR） |
| 2 | `tests/cpu/test_cpu_rtl_regfile_alu.cpp` | 改（仅注释） | 在 `m3_poc_alu_byte_equal` / `m3_poc_5stage_byte_equal` 头加警告注释：比对的是 **C++ 镜像**，非真实 select 树，勿当回归锚 |
| 3 | `AGENTS.md:215` | 改 | known issue 段：ALU 单元语义测试已补（指向新 case），并说明 byte-equal 是 C++ 镜像自比 |
| 4 | `openspec/specs/chmem-alu-semantics/spec.md` | 新增 | 新 capability：CH_MEM ALU 必须有跑真实 select 树的单元语义测试 |
| 5 | `openspec/changes/chmem-alu-unit-semantics/{proposal,design,tasks}.md` | 新增 | change 包（archive 时 apply 到 4） |
| 6 | `improvement-suggestions.md` | 改 | 登记 improvement（若走 add-improve 流程） |

**不改动**：`ip/cpu/arch/riscv/int_alu_chmem.h`（**只测不改**）、`ip/cpu/cpu_factory_chmem.h`、TLM 路径。

## 3. 测试设计

### 3.1 驱动辅助（每个 case 独立 context）

```
eval_alu(opcode, funct3, funct7, rs1, rs2, imm, reads_rs2, pc) -> uint32 rd_data
  1. context ctx + ctx_swap
  2. PipeBuilder pb(&ctx)
  3. register RiscvIntAluPlugin<ch_uint<32>>
  4. pb.build()                       // 创建 "execute" 节点
  5. 填 payload: KT::RS1 / KT::RS2 / KT::PC + DecodedInst{opcode,funct3,funct7,
     imm, reads_rs1=true, reads_rs2, writes_rd=true, rd_idx=1, rs1_idx=2, rs2_idx=3}
     (所有 ch_uint 成员用 ch_literal 真实初始化 —— null impl 会 SEGV)
  6. REQUIRE(pb.elaborate().has_value())
  7. sim = pb.create_simulator(); sim->reset(); sim->tick()
  8. return sim->get_value(n->operator()(KT::RD_DATA))
```

### 3.2 用例矩阵

| 组 | 数量 | 内容 | 锚定的风险 |
|---|---|---|---|
| **G1 M-op 语义** | 8 op × 2 向量 = 16 | MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU | 表达式本身（含 MULH 64-bit 中间、sext/zext） |
| **G2 M-op 边界** | 6 | div/divu by 0（→全 1）、rem/remu by 0（→被除数）、div(INT_MIN,-1)（→INT_MIN）、rem(INT_MIN,-1)（→0） | safe divisor + 特例覆写 |
| **G3 判别互斥**（核心价值） | 8 对 = 16 | 对每个 funct3 ∈ {0..7}：funct7=0x01 → M-op 结果；funct7=0x00 → base op 结果 | **M-op 不被同 funct3 的 base op 误捕** + `f7_00` 加固生效 |
| **G4 RV32I 回归** | 10 | ADD/SUB/SLL/SLT/SLTU/XOR/SRL/SRA/OR/AND（R-type, funct7=0x00/0x20） | base 树逐位不变 |
| **G5 I-type 不被误 gate** | 3 | ADDI（funct3=0, 负立即数）/ SLLI / SRLI —— I-type bits[31:25] 是立即数，**不得**被 `f7_00` gate 误伤 | `is_opimm` 侧未被误加固 |
| **G6 顶层分支** | 2 | LUI / AUIPC | select 树顶层分支未被 M-op 层破坏 |
| 合计 | **~43** | | |

### 3.3 期望值来源

- G1/G2：**RISC-V ISA 规范**（独立手算，不复用被测实现）—— 避免"用实现算期望"的自证
- G3：base op 期望值 = 同一操作数的 RV32I 定义值
- G4/G5/G6：同左

## 4. 验证计划

| 步骤 | 命令 | 预期 |
|---|---|---|
| V0 可行性 spike | 新文件先写 1 个 MUL case（`3*4==12`）跑通 | 机制成立（payload 预填 + literal 折叠 + get_value 读回） |
| V1 全量 | `./build/bin/chipforge_tests_chmem "[alu][semantics]"` | ~43 case 全 PASS |
| V2 **变异对照**（测试强度自证） | 临时把 `int_alu_chmem.h` 的 `f7_01` gate 去掉 → 重跑 → 必须 FAIL | 证明测试真能抓到 gate 回归（而非恒真） |
| V2b 第二变异 | 临时把 8 层 M-op select **移到 base 树之前** → 重跑 → 必须 FAIL | 证明能抓到 select 顺序回归 |
| V3 变异还原 | `git checkout` 还原 + 重建 | `[alu][semantics]` 全 PASS |
| V4 零回归 | `./build/bin/chipforge_tests_chmem "[chmem]"` / `"[verilator]"` / TLM 全量 | 与 change 前一致（[chmem] 65/66，10 个 pre-existing load/store FAIL 不变） |
| V5 门禁 | 5 个架构 gate | 全 exit 0 |
| V6 spec | `openspec validate --specs --strict` | 新 spec PASS |

## 5. 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| **literal 折叠导致测试不敏感** | 中 | 变异对照 V2/V2b 证明测试能抓回归；且 gate 条件本身来自 literal 化的 DECODED_INST，仍被真实求值 |
| **每次 case 新建 context → 慢** | 低 | ALU-only DAG 很小；先测单 case 耗时，超 100ms/case 则改为批量化 |
| **payload 预填与 elaborate 时序** | 中 | V0 spike 先行；若失败则退化为"elaborate 后读 payload"或改用 `ch_in` 端口驱动 |
| **`ch_uint` null impl SEGV** | 中 | 所有 `DecodedInst` 成员用 `ch_literal` 真实初始化（对齐 `cpu_factory_chmem.h:187-199` 既有做法） |
| **新文件与既有 TU 的 ODR 冲突**（`<ch.hpp>` 的 module.h） | 中 | 新文件**不用** `<ch.hpp>`，只 include 最小头集（对齐 `test_chmem_arith_poc.cpp` 做法） |
| 旧 byte-equal 测试被误当回归锚 | 低 | 改注释显式警告；不删（避免动既有基线） |

## 6. 交付物

1. `tests/cpu/test_cpu_chmem_alu_semantics.cpp`（新，~43 case）
2. `tests/cpu/test_cpu_rtl_regfile_alu.cpp`（仅注释）
3. `AGENTS.md`（known issue 关闭 + 数字更新）
4. `openspec/specs/chmem-alu-semantics/spec.md`（新 capability）
5. `openspec/changes/chmem-alu-unit-semantics/`（change 包 → archive）
6. 变异对照实验记录（写进 commit message —— 证明测试强度）

---

## 7. 完整性自检（2026-10-20，metis/critic 模型不可用 → 自行逐条推演 + 实证）

### 7.1 推演结论：**无 BLOCKER**，但有 4 项计划必须修正

| # | 缺口 | 等级 | 修正 |
|---|---|---|---|
| **A1** | **`tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 是显式列表（非 GLOB）** —— 新测试文件若不手动加入，**根本不会被编译进 `chipforge_tests_chmem`**，测试会"静默不存在"（TLM 侧 `TEST_SOURCES` 是 `GLOB_RECURSE`，会自动纳入；因 `#ifdef CF_PLUGIN_USE_CH_MEM` 守卫 → 空 TU，与既有 `test_cpu_rtl_regfile_alu.cpp` 同模式） | **实施易漏点** | 计划加 Step 0：新建文件后**立即**加进 `CHMEM_TEST_SOURCES` 并确认 `cmake --build` 有该 TU |
| **A2** | 原计划第 6 项"登记 improvement 到 `improvement-suggestions.md`" —— 不必要（本 P2 是 `chmem-mext-comb-datapath` 的延续，其 improvement 已存在；再登记一个 = 噪音） | 可删 | 删除该项 |
| **A3** | G3（判别互斥）与 G1（M-op 语义）用例重叠 —— 同 funct3 同向量 | 冗余 | G3 只保留"同 funct3、funct7 0x01 ↔ 0x00 翻转"的**对照**用例，期望值取自 RV32I ISA 定义（**不复用被测实现**，故非自证） |
| **A4** | `tests/CMakeLists.txt:140` 注释仍引用**从未存在**的 `chmem-m-ext-integration`（CHANGELOG 已声明"已改指 `chmem-mext-comb-datapath`"但漏了此处） | 悬空引用 | 一并修正（该文件本次本来就要改） |

### 7.2 机制推演（已用既有先例交叉验证）

| 疑点 | 结论 | 依据 |
|---|---|---|
| literal 折叠会不会让 gate 条件失去区分力？ | **不会**。gate 条件（`is_mext`/`is_mul`…）由 `decoded.funct3`/`funct7`（literal）计算，interpreter 在 tick 时真实求值；区分力来自"喂不同 literal 组合"而非信号。折叠若发生也遵循 select 语义 → 断言有效 | `test_chmem_arith_poc.cpp` Part A 同机制已实测 PASS |
| `DecodedInst` 哪些字段必设？ | **必设**：`opcode`/`funct3`/`funct7`/`imm`/`reads_rs2`（ALU 闭包直接读；`imm` 为 null impl 时 `op2 = select(reads_rs2, rs2_val, imm)` 会 SEGV）。其余（`rd_idx`/`rs1_idx`/`rs2_idx`/`reads_rs1`/`writes_rd`/`op_class`/`instr_format`）ALU 不读，但仍用 `ch_literal` 零值初始化（对齐 `cpu_factory_chmem.h:187-199` 既有范式，防 null impl） | `int_alu_chmem.h:112-127` 读字段清单 |
| `KT::RS1`/`KT::RS2`/`KT::PC` 必设？ | **是**（ALU 闭包无判空直读；null impl → SEGV） | `int_alu_chmem.h:112-124` |
| 断言 `RD_DATA` 还是 `RESULT`？ | 断言 **`RD_DATA`**（`= result`，ALU 算术结果）。`RESULT = select(is_store||is_load, rs1+imm, result)` 是给 DMem 的字节地址，两者对 ALU 语义不等价 | `int_alu_chmem.h:250-251` |
| `RD_DATA` 会不会被别的 plugin 覆写？ | **不会** —— 本单元测试只注册 ALU 一个 plugin；CSR LATE / JAL link 覆写发生在**别的 plugin** 的闭包里，未注册 → `RD_DATA` 就是 ALU 结果 | `int_alu_chmem.h:272-289`（CSR LATE 在 `csr_chmem.h`） |
| 期望值是否自证？ | **不自证**。G1/G2 期望值取自 RISC-V ISA 规范独立手算；G3 的 base op 期望值取自 RV32I 定义。**禁止**调用 `int_alu_chmem.h:384` 的 `compute_static`（那是实现内的 C++ 镜像） | `int_alu_chmem.h:384` `compute_static` |
| 每 case 一个 context 的开销 / 生命周期坑？ | ALU-only DAG 很小，开销可接受。坑：`ctx_swap guard` 须覆盖到 `elaborate()` 结束（本计划 helper 内全程覆盖）；`Simulator` 用显式 `ctx.get()` 不依赖 current context；CppHDL `node_builder` 是单例、node id 全局递增（无害） | `m3_poc_5stage_byte_equal_test2_dual_context` 专测跨 context 不污染 |

### 7.3 遗漏的验证（补进计划）

- **V7**：`cmake --build` 后**确认新 TU 真的被编译**（`build/tests/CMakeFiles/chipforge_tests_chmem.dir/cpu/test_cpu_chmem_alu_semantics.cpp.o` 存在），防 A1 静默失败
- **V8**：`[chmem]` family 计数变化 → `AGENTS.md:173` 的 "66 case" 需同步更新为新值（新增 N case）
- **V9**：`m3_poc_alu_elaborate` 仍 PASS（19 层树 + 128-bit 乘积的 Verilog 生成未被打断）

