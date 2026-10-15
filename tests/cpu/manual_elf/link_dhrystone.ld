/* tests/cpu/manual_elf/link_dhrystone.ld
 *
 * feat-dhrystone-dmips-gate: Dhrystone benchmark harness repair (B1).
 *   - .text @ 0x80000000 (in-window 0x80000000-0x80010000)
 *   - tohost symbol @ 0x80001000 (1 word picolibc convention)
 *   - entry: _start (= 0x80000000)
 *
 * 跟 add.elf 的 link_add.ld 同模式 (cpu-pipeline-tick-fix precedent).
 * 关键差异: dhrystone 是 C++ 源码 (extern "C" + C++ runtime), 需 g++ 编译 (-O2 -fno-inline)
 *           而不是 riscv-as/ld (add.elf 的纯汇编路径).
 *
 * 旧 link.ld base=0x0 + window_base=entry (0x710) → 整段 .text@0x0 被 load_section 静默 drop (H1+H2 根因)
 * 改 base=0x80000000 + tohost @ 0x80001000 → 整段 .text + tohost 都在 PicolibcHostMemory 窗口内.
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  .text : {
    *(.text.init)
    *(.text)
    *(.text.*)
  }
  . = ALIGN(0x1000);
  /* riscv-tests env/p 约定: tohost 是 4KB 对齐后的位置 (1 word picolibc location) */
  tohost = .;
  /* 显式 PROVIDE(tohost) 即使未引用也保留 — dhrystone.c 用 extern char tohost 引用 */
  PROVIDE(tohost = .);

  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss : { *(.bss) *(.bss.*) *(COMMON) }
  _end = .;
}
