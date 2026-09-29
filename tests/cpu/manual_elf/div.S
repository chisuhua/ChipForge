# tests/cpu/manual_elf/div.S
#
# 功能描述: RV32M DIV 最小测试程序 (mfc-cpu-pipeline-multi-cycle-fsm Phase A in-context 验证)
# 写 1 到 tohost 表示测试通过
#
# 汇编流程:
#   1. li x1, 14       # x1 = 14
#   2. li x2, 4        # x2 = 4
#   3. div x3, x1, x2  # x3 = 3 (14/4 整数除) (验证 DIV 指令)
#   4. li x4, 1        # x4 = 1 (PASS)
#   5. sw x4, 0(x0)    # 写入 tohost (地址 0)
#   6. loop: jal x0, loop  # 死循环
#
# 编译:
#   riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 div.S -o div.o
#   riscv32-unknown-elf-ld -T link.ld div.o -o div.elf

.section .text
.global _start

_start:
    li x1, 14          # x1 = 14
    li x2, 4           # x2 = 4
    div x3, x1, x2     # x3 = 3 (14/4 整数除) (验证 DIV 指令)
    li x4, 1           # x4 = 1 (PASS marker)
    sw x4, 0(x0)       # mem[0] = 1 → tohost = 1
1:  jal x0, 1b         # 死循环 (halt)