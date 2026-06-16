// tests/cpu/manual_elf/link.ld
//
// 功能描述: picolibc 链接脚本 (M4.8)
// 64KB 静态 RAM 布局 (PicolibcHostMemory 范围: 0x0 - 0x10000)
//
// 内存布局:
//   0x0000 - 0x000F: tohost 区域 (16 字节)
//   0x0010 - 0xFFFF: 代码 + 数据 (64KB - 16 字节)
//
// 链接地址: 所有段加载到 64KB RAM 起始 (0x0)

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    . = ORIGIN;

    .text : ALIGN(4)
    {
        *(.text)
        *(.text.*)
    } > RAM

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    } > RAM

    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
    } > RAM

    .bss : ALIGN(4)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > RAM

    _end = .;
}
