# riscv-tests rv32ui-p ELF vendor (vendored 2026-09-15)

**Source**: https://github.com/riscv-software-src/riscv-tests @ commit `2ebecad997fa58cd9e5724340ba75aa4b59bd1d0`
**Harness**: ChipForge `env-p/` variant (`riscv_test.h` + `link.ld`, 见下文)
**Build toolchain**: riscv32-unknown-elf-gcc 15.2.0 (`/workspace/project/opt/riscv/bin/`), `-march=rv32i_zicsr -mabi=ilp32 -nostdlib -static`
**License**: BSD-3-Clause (riscv-tests 上游兼容)

## 状态

40 个 `rv32ui-p-*.elf` 已 build 并 commit (2026-09-15)。测试体 (.S) 100% 上游, harness 为 ChipForge 自供变体。

### 实装记录

1. `git clone --depth 1 https://github.com/riscv-software-src/riscv-tests` @ `2ebecad`
2. `git submodule update --init` 获取 riscv-test-env (上游 `env/p` 语义, 用于参考)
3. 运行 `./build_rv32ui.sh` 生成 40 个 ELF (42 个 rv32ui .S 源排除 fence_i + ma_data)
4. `git add tests/cpu/riscv_tests/elf/` commit prebuilt 二进制 (~500KB, plain git <5MB LFS 阈值)

### env-p/ harness 说明 (ChipForge 变体)

上游 riscv-test-env v2 (`6de71edb`) 的 `RVTEST_PASS/FAIL` 走 `ecall` (a7=93 SYS_exit) → trap handler → `sw TESTNUM, tohost`。该路径依赖完整 trap 机 (mtvec/mcause/mret), 属 Wave 4 scope。

ChipForge Wave 1 决策 (design.md §Decision 5/6) 为**直接 `sw TESTNUM, tohost`** 退出, 与 `PicolibcHostMemory` 的 tohost 约定 (`exit_code = (val==1)?0:1`) 一致。因此:

- 测试体 (.S) 保持 100% 上游不变
- `env-p/riscv_test.h` 仅替换 harness 宏:
  - `RVTEST_PASS`: `li TESTNUM,1; la t5,tohost; sw TESTNUM,0(t5)` + spin
  - `RVTEST_FAIL`: `ori TESTNUM,TESTNUM,1; ...; sw TESTNUM,0(t5)` + spin (exit_code 1)
  - `RVTEST_CODE_BEGIN`: 最小 reset (INIT_XREG + 落入测试体), 无 trap_vector/CSR setup/mret
- `env-p/link.ld` 与上游一致 (`.tohost` @ 0x80001000)

## 排除清单

- `fence_i` (需 Zifencei 扩展, feature stub 推到 Phase 5+)
- `ma_data` (misaligned access 行为平台未定义)

## 基线矩阵 (2026-09-15, Wave 1)

`soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` — 40 个用例:

| 结果 | 数量 | 说明 |
|------|------|------|
| PASS | 30 | 全部 RV32I 整数/分支/跳转指令族通过 (add/addi/and/auipc/beq/bge/bgeu/blt/bltu/bne/jal/jalr/lui/or/ori/simple/sll/slli/slt/slti/sltiu/sltu/sra/srai/srl/srli/sub/xor/xori) |
| FAIL (feature stub) | 10 | LOAD width extraction (lb/lbu/lh/lhu/lw) 为 Wave 2 显式 OOS; sb/sh/sw/st_ld/ld_st 因内嵌 load 验证失败同源 |

## 许可证

[riscv-tests](https://github.com/riscv-software-src/riscv-tests) BSD-3-Clause 与 ChipForge 项目协议兼容。
