// tests/cpu/manual_elf/and.S
//
// 功能描述: RV32I AND 测试程序
// x3 = 0xFF & 0x0F = 0x0F, 写 1 到 tohost

.section .text
.global _start

_start:
    li x1, 0xFF
    li x2, 0x0F
    and x3, x1, x2
    li x4, 1
    sw x4, 0(x0)
1:  jal x0, 1b
