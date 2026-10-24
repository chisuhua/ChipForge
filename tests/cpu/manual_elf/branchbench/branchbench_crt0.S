/* tests/cpu/manual_elf/branchbench/branchbench_crt0.S
 *
 * 栈初始化 stub (与 dhrystone_crt0.S 同构):
 *   _start 入口 sp=0 (RegFilePlugin default T{0}), g++ prologue 的 addi sp
 *   会让 sp 下溢到窗口外 => 8 个 sw 写窗口外被 drop => 程序死循环.
 *   解法: 接管 _start 入口, 设 sp=0x8000F000 后 jal _cstart, 再 tail tohost_pass.
 */
.section .text.init
.global _start
_start:
    li sp, 0x8000F000       /* sp = base + 60K (64K dmem 窗口内) */
    jal _cstart              /* C++ 实现 */
    tail tohost_pass         /* 兜底: 写 tohost=1 */
