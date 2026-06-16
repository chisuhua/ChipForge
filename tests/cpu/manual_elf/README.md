# Manual ELF 测试程序 (M4.9)

> **状态**: 🟢 M4 实施
> **用途**: RV32I 最小程序, 写 1 到 tohost 表示测试通过
> **限制**: 64KB 静态 RAM (PicolibcHostMemory)

## 1. 文件清单

| 文件 | 描述 |
|------|------|
| `add.S` | RV32I ADD 测试: x3 = 5 + 3, 写 1 到 tohost |
| `link.ld` | 链接脚本: 64KB RAM, 代码从 0x0 加载 |

## 2. 编译步骤

需要 RISC-V 工具链 (riscv32-unknown-elf-gcc 或 riscv64-unknown-elf-gcc):

```bash
# 1. 汇编
riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 add.S -o add.o

# 2. 链接
riscv32-unknown-elf-ld -T link.ld add.o -o add.elf

# 3. 验证 (可选)
riscv32-unknown-elf-readelf -h add.elf
riscv32-unknown-elf-objdump -d add.elf
```

## 3. 验证流程

1. CPU 从 0x0 启动 (PC = 0)
2. 执行 `li x1, 5; li x2, 3; add x3, x1, x2` → x3 = 8
3. `li x4, 1; sw x4, 0(x0)` → mem[0] = 1 (tohost)
4. 仿真器检测 mem[0] == 1 → 退出, 返回 PASS
5. `jal x0, 1b` 死循环 (实际不会执行, 因 tohost 已触发退出)

## 4. tohost 机制

picolibc 约定: 程序把 1 写到 tohost 地址 (0x0) 表示 PASS, 仿真器轮询该地址。
本测试用 `sw x4, 0(x0)` 写入 tohost, 仿真器 (PicolibcHostMemory) 读取后返回 1。

## 5. 扩展

可添加更多测试程序:

| 程序 | 指令 | 预期 |
|------|------|------|
| `add.elf` | ADD | x3 = 5+3 = 8, tohost=1 |
| `sub.elf` | SUB | x3 = 5-3 = 2, tohost=1 |
| `and.elf` | AND | x3 = 0xFF & 0x0F = 0x0F, tohost=1 |
| `or.elf`  | OR  | x3 = 0xF0 \| 0x0F = 0xFF, tohost=1 |
| `sll.elf` | SLL | x3 = 1 << 4 = 16, tohost=1 |
| `mul.elf` | MUL | x3 = 6 * 7 = 42, tohost=1 |

## 相关文档

- **M4 详细**: [`../../docs/implementation-plan/M4-integration.md`](../../docs/implementation-plan/M4-integration.md)
- **picolibc 约定**: https://github.com/picolibc/picolibc
- **tohost 机制**: picolibc/doc/picolibc.md
