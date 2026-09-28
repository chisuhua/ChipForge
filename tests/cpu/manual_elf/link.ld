/* tests/cpu/manual_elf/link.ld

   picolibc 链接脚本 (M4.8 简化版, 兼容 GNU Binutils 2.47)
   - 64KB 静态 RAM 布局 (PicolibcHostMemory 范围: 0x0 - 0x10000)
   - 所有段加载到 64KB RAM 起始 (0x0)
   - 不使用 MEMORY{} 语法 (GNU Binutils 2.47 不再支持 . = ORIGIN; + MEMORY 组合)
   - 不使用 ALIGN() (新版 ld 在某些情况下报 syntax error) */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x0;
  .text : { *(.text) *(.text.*) }
  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss : { *(.bss) *(.bss.*) *(COMMON) }
  _end = .;
}