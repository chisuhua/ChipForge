# tests/cpu/manual_elf/add.S
#
# 功能描述: RV32I ADD 最小测试程序 (cpu-sim-add-elf-tohost-fix change)
#   写 1 到 tohost 表示测试通过
#
# 汇编流程 (riscv-tests 风格, picolibc 兼容):
#   1. li x1, 5        # x1 = 5
#   2. li x2, 3        # x2 = 3
#   3. add x3, x1, x2  # x3 = 8 (5+3)
#   4. li x4, 1        # x4 = 1 (PASS marker)
#   5. la x5, tohost   # tohost 地址 (linker symbol, .tohost 段起点 @ 0x80001000)
#   6. sw x4, 0(x5)    # mem[tohost] = 1 → picolibc tohost = 1
#   7. loop: jal x0, loop  # 死循环 (实际不会执行, 因 tohost 已触发退出)
#
# 编译:
#   riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 add.S -o add.o
#   riscv32-unknown-elf-ld -T link_add.ld add.o -o add.elf
#
# 关键变更 (cpu-sim-add-elf-tohost-fix):
#   - 从 `.text` 改为 `.text.init` (riscv-tests 风格约定)
#   - tohost 写入从 `sw x4, 0(x0)` 改为 `la x5, tohost; sw x4, 0(x5)`
#   - 新增 `.section .tohost` 提供 tohost 地址 (linker symbol `tohost`)
#   - linker 切换到 link_add.ld (base=0x80000000, .tohost @ 0x80001000)
#   - 原因: 旧版 base=0x0 + 无 .tohost section 时 pipeline 不 tick (根因 defer Phase 6d.6+)

.section .text.init
.global _start

_start:
    li x1, 5           # x1 = 5
    li x2, 3           # x2 = 3
    add x3, x1, x2     # x3 = 8 (验证 ADD 指令)
    li x4, 1           # x4 = 1 (PASS marker)
    la x5, tohost      # 加载 tohost 地址 (linker symbol, default 0x80001000)
    sw x4, 0(x5)       # mem[tohost] = 1 → picolibc tohost = 1
1:  jal x0, 1b         # 死循环 (halt)
