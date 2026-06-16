// tests/cpu/manual_elf/sll.S
//
// 功能描述: RV32I SLL 测试程序
// x3 = 1 << 4 = 16, 写 1 到 tohost

.section .text
.global _start

_start:
    li x1, 1
    li x2, 4
    sll x3, x1, x2
    li x4, 1
    sw x4, 0(x0)
1:  jal x0, 1b
