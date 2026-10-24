/* tests/cpu/manual_elf/link_branchbench.ld
 *
 * 分支微基准链接脚本 (chmem-branch-microbench)
 *
 * 与 link_dhrystone.ld 同构的 **PHDRS 双段**布局 —— 必需, 非可选:
 *   CH_MEM 的 ELF 段路由 (ip/cpu/cpu_factory_chmem.h:304-310):
 *     is_exec && !is_writable -> IBus (取指)
 *     is_writable             -> DMem (数据)
 *     其余 (纯 R 段)           -> 无分支 => 整段丢弃 (静默!)
 *   单 RWE 段 => 整段进 DMem => IBus 为空 => 取指失败 (实测 Dhrystone 的老症状).
 *
 *   PT_LOAD#0  R E   .text              -> IBus
 *   PT_LOAD#1  RW    .tohost + .rodata   -> DMem
 *
 * tohost 约定 (与 riscv-tests / dhrystone 一致, 不可改):
 *   .tohost 段首 word = 0x80001000 = DMem word 0x400
 *   => 与 ip/cpu/cpu_factory_chmem.h:313 的 set_tohost_probe_word(0x400)
 *      及 tools/verilator_runner/sim_main.cpp:35 的 kToHostWord = 0x400 一致
 *   => .tohost 必须是 RW 段的**第一个** output section (否则段 vaddr 偏移,
 *      tohost 落到被加载区之外 => 写入被丢弃)
 *   => .tohost 自占 1 word, .rodata 从 0x80001004 起 => 与 .rodata 不重叠
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5);   /* R E */
  data PT_LOAD FLAGS(6);   /* RW  */
}

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text.init)
    *(.text)
    *(.text.*)
  } :text

  . = ALIGN(0x1000);

  .tohost : {
    tohost = .;
    PROVIDE(tohost = .);
    LONG(0)
  } :data

  .rodata : { *(.rodata) *(.rodata.*) *(.srodata) *(.srodata.*) } :data
  .data   : { *(.data) *(.data.*) *(.sdata) *(.sdata.*) } :data
  .bss    : { *(.bss) *(.bss.*) *(.sbss) *(.sbss.*) *(COMMON) } :data

  _end = .;

  /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) }
}
