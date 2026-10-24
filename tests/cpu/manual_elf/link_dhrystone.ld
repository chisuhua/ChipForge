/* tests/cpu/manual_elf/link_dhrystone.ld
 *
 * chmem-verilator-perf-harness (交付点 A) 修订版 —— 2026-10-24
 *
 * 【为什么要 PHDRS 双段】
 * CH_MEM 的 ELF 段路由 (ip/cpu/cpu_factory_chmem.h:304-310):
 *     is_exec && !is_writable  -> IBus   (取指)
 *     is_writable              -> DMem   (数据)
 *     其余 (纯 R 段)            -> 无分支 => 整段丢弃 (静默!)
 * 旧链接脚本产出**单个 RWE LOAD 段** => is_exec && is_writable => 整段进 DMem,
 * **IBus 为空 => 取指失败** (实测: cpu_verilator_sim 输出 TOHOST=1498564676
 * (= 0x59524F48 = ASCII "HORY", 即 .rodata 文本) + TIMEOUT)。
 * vendored rv32ui ELF 能跑是因为它是 **R E + RW 两段**。
 *
 * 故本脚本用 PHDRS 显式产出两段:
 *   PT_LOAD#0  R E   .text            -> IBus (取指)
 *   PT_LOAD#1  RW    .tohost + data   -> DMem (数据 + tohost probe word)
 *
 * 【tohost 位置约定】(不可改)
 * riscv-tests env/p 约定: tohost 是 4KB 对齐后的位置 (1 word picolibc location)
 *   - .tohost 段首 word = 0x80001000 = DMem word 0x400
 *   - 与 ip/cpu/cpu_factory_chmem.h:313 的 set_tohost_probe_word(0x400)
 *     及 tools/verilator_runner/sim_main.cpp:35 的 kToHostWord = 0x400 一致
 *   - 故 .tohost 必须是 RW 段的**第一个** output section (否则段 vaddr 偏移,
 *     tohost 会落到被加载区之外, 写入被丢弃)
 *
 * 【tohost 与 .rodata 不重叠】旧脚本 `tohost = .;` 紧接 `.rodata : {...}` =>
 * tohost 与 .rodata[0] 同址 (0x80001000) => 写 tohost=1 会覆盖只读数据首字。
 * 现在 .tohost 自占 1 word, .rodata 从 0x80001004 起 => 不重叠。
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

  /* ---- PT_LOAD#0 (R E): 代码 -> IBus ---- */
  .text : {
    *(.text.init)
    *(.text)
    *(.text.*)
  } :text

  . = ALIGN(0x1000);

  /* ---- PT_LOAD#1 (RW): tohost + 数据 -> DMem ---- */
  /* .tohost 自占 1 word (初始值 0; 程序结束时写 1) */
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
