/* tests/cpu/manual_elf/dhrystone/dhrystone_crt0.S
 *
 * feat-dhrystone-dmips-gate H3: crt0 替代 (sp init).
 * _start 入口 sp=0 (RegFilePlugin default T{0}), g++ prologue `addi sp,sp,-112`
 * 会让 sp 下溢到 0xFFFFFF90, 8 个 sw 写窗口外 (drop), 程序死循环.
 * 解法: 用 .S 文件接管 _start 入口, 设 sp=0x8000F000 后 jal _cstart (C++ 实现).
 */
.section .text.init
.global _start
_start:
    li sp, 0x8000F000       /* sp = base + 60K (PicolibcHostMemory 64K 窗口内) */
    jal _cstart              /* 调 C++ 实现 _cstart (= 原 _start body) */
    tail tohost_pass         /* 跳到 tohost_pass() 写 tohost symbol */
