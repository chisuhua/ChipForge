/* tests/cpu/manual_elf/link_add.ld
 *
 * cpu-sim-add-elf-tohost-fix: riscv-tests 风格 linker script (add.elf only)
 *   - .text @ 0x80000000 (含 .text.init + 普通 .text)
 *   - tohost symbol @ 0x80001000 (1 word picolibc convention)
 *   - entry: _start (=0x80000000)
 *
 * 关键约定 (per riscv-tests env/p/link.ld):
 *   - tohost 是符号 (linker symbol), 不是段 (section)
 *   - tohost 解析为 . = ALIGN(0x1000) 后的位置 (0x80001000)
 *   - add.S 用 `la x5, tohost` 加载 tohost 地址, sw 写 1 触发 PASS
 *
 * 约束:
 *   - 64KB 静态 RAM (PicolibcHostMemory 范围: 0x80000000 - 0x80010000)
 *   - PicolibcHostMemory cfg_.tohost_addr = tohost symbol = 0x80001000
 *   - 其他 .S (mul/div/build_mmu_bare/build_l1cache_basic) 仍用 link.ld
 *     (base=0x0, sw x4, 0(x0) → tohost_addr=0)
 *
 * elf_loader::load_elf_full 行为:
 *   - 当 .tohost section 不存在时, elf.tohost_addr = UINT64_MAX
 *   - cpu_sim main.cpp:382 当 tohost_addr_explicit=true 时用 CLI 值
 *     当 tohost_addr_explicit=false 时用 elf.tohost_addr (UINT64_MAX → 0)
 *   - 因此 add.elf 必须传 `--tohost-addr 0x80001000` 才能正确设置 tohost_addr
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

  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss : { *(.bss) *(.bss.*) *(COMMON) }
  _end = .;
}