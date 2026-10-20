# Tasks — chmem-alu-unit-semantics

> **Path**: rdd-builder（测试专用 + spec delta，无业务代码改动）
> **Owner**: ip/cpu
> **Spec refs**: `specs/chmem-alu-semantics/spec.md`
> **Plan**: `.rddf/plans/chmem-alu-unit-semantics-2026-10.md`（含完整性自检 §7）

## 0. Pre-flight

- [x] 0.1 读 `int_alu_chmem.h` 19 层 select 树 + `DecodedInst` 字段 + `PipeBuilder::at_stage` 节点自动创建机制
- [x] 0.2 核对既有 "byte-equal" 实况（两份手写 C++ 镜像，不跑 select 树）—— 确认 known issue 成立
- [x] 0.3 确认 `CHMEM_TEST_SOURCES` 是**显式列表**（非 GLOB）→ 新文件必须手动加
- [x] 0.4 记录 baseline：`[chmem]` 66 case / 65 PASS / 1 failed（10 个 pre-existing rv32ui load/store FAIL）

## 1. 机制 Spike（V0）

- [x] 1.1 新建 `tests/cpu/test_cpu_chmem_alu_semantics.cpp`（`#ifdef CF_PLUGIN_USE_CH_MEM`；**不用** `<ch.hpp>` 直接引入）
- [x] 1.2 实现 `eval_alu()`：ctx → PipeBuilder → register ALU → build → **elaborate 前**填 payload（RS1/RS2/PC + DecodedInst）→ elaborate → create_simulator → tick → `get_value(RD_DATA)`
- [x] 1.3 Spike case `chmem_alu_semantics_spike_mul`：MUL(3,4) == 12 → **机制成立**
- [x] 1.4 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 加入新文件（0.3 的手动项）
- [x] 1.5 解决 2 个编译错：(a) `decode_chmem.h` **自身不 include** `decoder_table.h` → 需 includer 先提供 `cf::cpu::arch::riscv::opcode` 命名空间（其 lambda 里有同名局部变量 `opcode`）；(b) `Simulator::get_value` 返回 `sdata_type`（`explicit operator uint64_t`），需先转 uint64
- [x] 1.6 V7 确认新 TU 真被编译（`.o` 存在）

## 2. 用例矩阵（V1）

- [x] 2.1 G1 M-op 语义：8 op × 2 向量 = 16（期望值取自 RISC-V ISA，独立手算）
- [x] 2.2 G2 M-op 边界：除零（DIV/DIVU→全 1, REM/REMU→被除数）+ INT_MIN÷-1（→INT_MIN / →0）= 6
- [x] 2.3 G3 判别互斥：每 funct3 验 funct7=0x01 走 M-op、0x00 走 base op，且两者结果必须不同 = 8 组
- [x] 2.4 G4 RV32I 回归：10 条 base R-type op
- [x] 2.5 G5 I-type 不误 gate：负立即数 I-type（bits[31:25]=0x20）XORI/ANDI/ORI/SLTI/SLTIU + SLLI = 6
- [x] 2.6 G6 顶层分支：LUI / AUIPC = 2
- [x] 2.7 全部 PASS：6 TEST_CASE / 288 assertions / 0.42s
- [x] 2.8 **期望值自查**：实测 1 处手算错误（`ANDI(0xF0,-4096)` 我算 0xF0，实为 0x00 —— ALU 是对的）→ 修正期望值（独立手算的价值体现）

## 3. 变异对照（V2 —— 自证测试强度）

- [x] 3.1 V2a：去掉 `is_mext` 的 `f7_01` gate → **必须 FAIL** → ✅ 被抓住（G4 `ADD(5,3)` 得 15 = MUL 误捕 + G3 base op 得 48）
- [x] 3.2 V2b：把 8 层 M-op select 移到 base 树**之前**（加固仍在）→ 实测 **PASS** —— 这是**正确的**：f7_00 加固使 base/M 判别互斥 → 顺序无关（D2 双保险生效）
- [x] 3.3 V2c：删除整个 8 层 M-op select 块 → **必须 FAIL** → ✅ 被抓住（G1+G2+G3 FAIL，`MUL(3,4)` 得 0）
- [x] 3.4 V2b'：移层 **且** 去 `f7_00` 加固 → **必须 FAIL** → ✅ 被抓住（3 case FAIL）—— 证明无加固时 select 顺序敏感性真实存在
- [x] 3.5 还原 `int_alu_chmem.h`（`git diff` 为空）+ 复跑 288 assertions 全 PASS

## 4. 回归（V4）

- [x] 4.1 `[chmem]` 全套 → 零新增 FAIL（预期 72 case / 71 PASS / 1 failed，10 个 pre-existing load/store FAIL 不变）
- [x] 4.2 `[verilator]` 48 ELF → 96 assertions 全 PASS 不变
- [x] 4.3 `[mmu-verilator]` 3/3 不变
- [x] 4.4 TLM 全量 → 481 case / 1 failed（pre-existing `7stage_topology_from_config` infra drift）不变
- [x] 4.5 V9：`m3_poc_alu_elaborate` 仍 PASS（Verilog 生成未被打断）

## 5. 门禁（V5）

- [x] 5.1 `verify_adr.sh` → 0 fail
- [x] 5.2 `verify_plugin_decision.sh` → 8/8
- [x] 5.3 `check_plugin_portability.sh` → 12/12
- [x] 5.4 `doc_link_check.sh` → 0 broken
- [x] 5.5 `roadmap-r9-check.sh` → 0 fail

## 6. 文档与 spec

- [x] 6.1 `tests/cpu/test_cpu_rtl_regfile_alu.cpp` 加警告注释（byte-equal 是 C++ 镜像自比，不可当回归锚 + 指向新文件）
- [x] 6.2 `AGENTS.md` known issue 段：ALU 单元语义测试已补（指向新文件）+ `[chmem]` family 计数 66 → 72 + CH_MEM 模式状态段同步
- [x] 6.3 `openspec change validate chmem-alu-unit-semantics` → 0 error
- [x] 6.4 `openspec archive chmem-alu-unit-semantics -y` → apply 到 `openspec/specs/chmem-alu-semantics/spec.md`
- [x] 6.5 `openspec validate --specs --strict` → 新 spec PASS（既有 4 个 pre-existing failed 不变）
- [x] 6.6 `CHANGELOG.md` 新增条目

---

**总计**: ≈ 28 tasks
**关键锚**: 288 assertions 白盒语义 + 4 个变异对照（V2b 是"正确的 PASS"，需在记录中说明）
