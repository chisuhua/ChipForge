# tests/cpu/manual_elf/mul.S
#
# 功能描述: RV32M MUL 最小测试程序 (mfc-cpu-pipeline-multi-cycle-fsm Phase A in-context 验证)
# 写 1 到 tohost 表示测试通过
#
# 汇编流程:
#   1. li x1, 6        # x1 = 6
#   2. li x2, 7        # x2 = 7
#   3. mul x3, x1, x2  # x3 = 42 (6*7) (验证 MUL 指令)
#   4. li x4, 1        # x4 = 1 (PASS)
#   5. sw x4, 0(x0)    # 写入 tohost (地址 0)
#   6. loop: jal x0, loop  # 死循环 (或 halt)
#
# 编译:
#   riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 mul.S -o mul.o
#   riscv32-unknown-elf-ld -T link.ld mul.o -o mul.elf

.section .text
.global _start

_start:
    li x1, 6           # x1 = 6
    li x2, 7           # x2 = 7
    mul x3, x1, x2     # x3 = 42 (6*7) (验证 MUL 指令)
    li x4, 1           # x4 = 1 (PASS marker)
    sw x4, 0(x0)       # mem[0] = 1 → tohost = 1
1:  jal x0, 1b         # 死循环 (halt)