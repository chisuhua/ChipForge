// tests/cpu/manual_elf/srli.S
//
// 功能描述: RV32I SRLI 测试程序
// x3 = 0xF0 >> 4 = 0x0F, 写 1 到 tohost

.section .text
.global _start

_start:
    lui x1, 0x10
    addi x1, x1, -16   # x1 = 0x10000 - 16 = 0xFFF0
    srli x3, x1, 4
    li x4, 1
    sw x4, 0(x0)
1:  jal x0, 1b
