// tests/cpu/manual_elf/sub.S
//
// 功能描述: RV32I SUB 测试程序
// x3 = 5 - 3 = 2, 写 1 到 tohost

.section .text
.global _start

_start:
    li x1, 5
    li x2, 3
    sub x3, x1, x2
    li x4, 1
    sw x4, 0(x0)
1:  jal x0, 1b
