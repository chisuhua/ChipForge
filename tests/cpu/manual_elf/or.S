// tests/cpu/manual_elf/or.S
//
// 功能描述: RV32I OR 测试程序
// x3 = 0xF0 | 0x0F = 0xFF, 写 1 到 tohost

.section .text
.global _start

_start:
    li x1, 0xF0
    li x2, 0x0F
    or x3, x1, x2
    li x4, 1
    sw x4, 0(x0)
1:  jal x0, 1b
