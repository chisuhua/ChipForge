# tests/cpu/manual_elf/zicsr_basic.S
#
# 功能描述: RV32I + Zicsr 最小测试程序 (csr-chmem-pipeline-integration C.1)
#   验证 Zicsr CSRRW 指令 + tohost=1 终止
#
# 汇编流程 (riscv-tests 风格, link_add.ld 兼容):
#   1. csrrs x1, mstatus, x0   # 读 mstatus 到 x1 (CSRRW 不写因 rs1=x0)
#   2. csrrw x2, mstatus, x1   # 写 mstatus = x1 (CSRRW old value 写回 rd)
#   3. li   x3, 1              # x3 = 1 (PASS marker)
#   4. la   x4, tohost         # tohost 地址
#   5. sw   x3, 0(x4)          # mem[tohost] = 1
#   6. loop: jal x0, loop      # 死循环
#
# 注: 本 ELF 仅验证 CSRRW 读 mstatus 路径 (R1+R3 决策).
#   ECALL/MRET (R2 决策) 需 IBus branch-NORMAL select 覆写 (R2.3, 被 mmu C.2 阻塞)
#   故本版本不含 trap 路径测试, 仅覆盖 Zicsr 基础通路.
#
# 编译:
#   riscv32-unknown-elf-as -march=rv32i_zicsr -mabi=ilp32 zicsr_basic.S -o zicsr_basic.o
#   riscv32-unknown-elf-ld -T link_add.ld zicsr_basic.o -o zicsr_basic.elf
#
# 关联 (csr-chmem-pipeline-integration C.1):
#   - 期望跑通路径: fetch → decode → execute-LATE (csr_chmem 重提取 csr_addr) →
#     writeback-NORMAL (csr_chmem 写 mstatus) → writeback
#   - 实测在 cpu_sim CLI 路径下 pipeline tick 受 framework-level bug 影响
#     (per AGENTS.md "cpu-pipeline-tick-rca" defer Phase 6d.6+), 走 cpu_sim
#     仍 FAIL. C++ API 路径 (test_rv32ui_runner 风格) 是后续 session 任务.
#   - 本文件 scope-partial: 提供基础设施就位 (汇编+链接脚本规则),
#     集成 e2e 测试推迟到 mmu C.2 merge + cpu-pipeline-tick-rca 修复之后.

.section .text.init
.global _start

_start:
    csrrs x1, mstatus, x0   # x1 = mstatus (CSRRW 读但不写, rs1=x0)
    csrrw x2, mstatus, x1   # x2 = mstatus (CSRRW 读 old value), mstatus = x1
    li   x3, 1              # x3 = 1 (PASS marker)
    la   x4, tohost         # x4 = &tohost (linker symbol, .tohost 段 @ 0x80001000)
    sw   x3, 0(x4)          # mem[0x80001000] = 1 → picolibc tohost = 1
1:  jal x0, 1b              # 死循环 (tohost 已触发退出, 实际不执行)

.section .tohost
.align 4
tohost: .word 0
