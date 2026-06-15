# M3 — RISC-V ISA 特有 Plugin 套件 (6 个 P0/P1/P2)

> **本文件位置**: `ip/cpu/docs/implementation-plan/M3-riscv-plugins.md`
> **状态**: 🟡 待启动 (依赖 M2 完成)
> **估算**: 4-5 d
> **总体任务清单**: 见 [`README.md` §6 M3 行](README.md)

## 1. 目标

实施 6 个 RISC-V ISA 特有 Plugin (Decode / IntAlu / Mul / Branch / LSU / CSR), 全部位于 `ip/cpu/arch/riscv/`。M3 是最大单一阶段, 是 1220 行 v2.0 文档的核心落地。

## 2. 任务清单

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M3.1** | 实施 `decoder_table.h` (P0: funct3/funct7 → OpCode 表) | `ip/cpu/arch/riscv/decoder_table.h` | test_decode PASS | 0.5d |
| **M3.2** | 实施 `payload_riscv.h` (P0: RiscvDecodeDetail 结构) | `ip/cpu/arch/riscv/payload_riscv.h` | test_decode PASS | 0.3d |
| **M3.3** | 实施 `RiscvDecodePlugin` (P0: 同时填 pl::DECODE + pl::RISCV_DETAIL) | `ip/cpu/arch/riscv/decode.h` + `.cpp` | test_decode 4-6 用例 PASS | 0.7d |
| **M3.4** | 实施 `RiscvIntAluPlugin` (P0: RV32I/RV64I 整数运算) | `ip/cpu/arch/riscv/int_alu.h` + `.cpp` | test_int_alu 4-6 用例 PASS (ADD/SUB/AND/OR/SLL/SRL) | 0.7d |
| **M3.5** | 实施 `RiscvBranchPlugin` (P1: 分支跳转 + 链接) | `ip/cpu/arch/riscv/branch.h` + `.cpp` | test_branch 4-6 用例 PASS (BEQ/BNE/JAL/JALR) | 0.5d |
| **M3.6** | 实施 `RiscvMulPlugin` (P1: M 扩展乘除法, 3 级子流水) | `ip/cpu/arch/riscv/mul.h` + `.cpp` | test_mul 4-6 用例 PASS | 0.5d |
| **M3.7** | 实施 `RiscvLsuPlugin` (P1: load/store 含地址生成) | `ip/cpu/arch/riscv/lsu.h` + `.cpp` | test_lsu 4-6 用例 PASS | 0.5d |
| **M3.8** | 实施 `RiscvCsrPlugin` (P2 stub: 目录 + .h 占位, .cpp 写 `// TODO: M3+`) | `ip/cpu/arch/riscv/csr.h` + `.cpp` | 占位存在 | 0.05d |
| **M3.9** | 创建 `fpu.h` P3+ 占位 | `ip/cpu/arch/riscv/fpu.h` + `.cpp` | 占位存在 | 0.05d |
| **M3.10** | 6 × 单元测试 PASS | `ip/cpu/arch/riscv/tests/` | ctest 6/6 PASS | (累计) |
| **M3.11** | RV32I 译码正确性: 全 35+ 条基础指令译码用例 PASS | `ip/cpu/arch/riscv/tests/test_decode_full.cpp` | 35+ 用例 PASS | (累计) |
| **M3.12** | RV32I 整数运算: ADD/SUB/AND/OR/XOR/SLL/SRL/SRA/SLT/SLTU 全 PASS | `ip/cpu/arch/riscv/tests/test_int_alu_full.cpp` | 10 用例 PASS | (累计) |

## 3. 依赖

- ✅ M1 完成 (DecodePayload + 通用 Payload Key)
- ✅ M2 完成 (5 个 P0 Plugin + 3 个 P3+ 占位)

## 4. 完成判据

- [ ] M3.1-M3.7 全部 6 个 RISC-V Plugin 代码 + 单元测试 commit
- [ ] M3.8-M3.9 占位文件存在
- [ ] M3.10-M3.12: ctest 全部 PASS (含 RV32I 全指令覆盖)
- [ ] 16/16 ctest 全局不退化
- [ ] 译码表覆盖 RV32I 全指令 (35+ 条)
- [ ] 整数运算覆盖 RV32I 全部 10 条算术指令

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| `RiscvMulPlugin` 3 级子流水实现复杂 | 用 M1.2 `declare_substage` API; 参考 multi_isa v2.0 §6.3 流水图 |
| `RiscvCsrPlugin` 270+ CSR 字段实现工作量大 | 议题 2 选 B: P2 阶段仅 stub; 表格驱动推迟到 Phase 5+ |
| `RiscvLsuPlugin` 地址生成 + 对齐 + 异常处理复杂 | 单元测试先覆盖对齐场景; 异常处理推迟 |
| 译码表覆盖不全 (漏指令) | M3.11 RV32I 全指令覆盖用例; 用 Spike diff 校验 (待 Spike 引入) |

## 6. 任务编号约定

`M3.x` 其中 x = 1..12 (与本文件 §2 表格 # 列对应)

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M3 行
- Plugin 套件 (RISC-V 特有): [`../blueprint.md`](../blueprint.md) §4.2
- multi_isa 流水图: [`../multi_isa_architecture.md`](../multi_isa_architecture.md) §6.3
- 任务状态: [`../status.md`](../status.md)
