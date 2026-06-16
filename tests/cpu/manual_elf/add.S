// tests/cpu/manual_elf/add.S
//
// 功能描述: RV32I ADD 最小测试程序 (M4.7)
// 写 1 到 tohost 表示测试通过
//
// 汇编流程:
//   1. li x1, 5        # x1 = 5
//   2. li x2, 3        # x2 = 3
//   3. add x3, x1, x2  # x3 = 8 (5+3)
//   4. li x4, 1        # x4 = 1 (PASS)
//   5. sw x4, 0(x0)    # 写入 tohost (地址 0)
//   6. loop: jal x0, loop  # 死循环 (或 halt)
//
// 编译:
//   riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 add.S -o add.o
//   riscv32-unknown-elf-ld -T link.ld add.o -o add.elf

.section .text
.global _start

_start:
    li x1, 5           # x1 = 5
    li x2, 3           # x2 = 3
    add x3, x1, x2     # x3 = 8 (验证 ADD 指令)
    li x4, 1           # x4 = 1 (PASS marker)
    sw x4, 0(x0)       # mem[0] = 1 → tohost = 1
1:  jal x0, 1b         # 死循环 (halt)
