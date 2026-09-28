# tests/cpu/manual_elf/build_manual_elf.sh
#
# 功能描述: 用 RISC-V 工具链编译 manual_elf/*.S → manual_elf/*.elf
# 作者: ChipForge Plugin Team
# 最后修改日期: 2026-09-28
#
# 编译流程:
#   1. riscv32-unknown-elf-as -march=rv32i|m -mabi=ilp32 <src.S> -o <src.o>
#   2. riscv32-unknown-elf-ld -T link.ld <src.o> -o <src.elf>
#
# 约束:
#   - 工具链在 PATH 中 (构建环境已预装 /workspace/main/opt/riscv/bin)
#   - 链接脚本 link.ld 64KB RAM (PicolibcHostMemory 默认)
#   - RV32M 指令需要 -march=rv32im (mul.S / div.S) 或 -march=rv32i (基础指令)

#!/bin/bash
set -euo pipefail

# 工具链路径 (build env 已预装 GNU 16.1.0)
TOOLCHAIN_DIR="/workspace/main/opt/riscv/bin"
export PATH="${TOOLCHAIN_DIR}:${PATH}"

# 检查工具链可用
if ! command -v riscv32-unknown-elf-as &>/dev/null; then
  echo "FAIL: riscv32-unknown-elf-as not found in PATH"
  echo "      Expected: ${TOOLCHAIN_DIR}"
  exit 1
fi

# 切换到 manual_elf 目录
cd "$(dirname "$0")"

# 编译 add.S (RV32I)
echo "[1/3] 编译 add.S → add.elf (RV32I)"
riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 add.S -o add.o
riscv32-unknown-elf-ld -T link.ld add.o -o add.elf
rm -f add.o

# 编译 mul.S (RV32IM, MUL 指令)
echo "[2/3] 编译 mul.S → mul.elf (RV32IM)"
riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 mul.S -o mul.o
riscv32-unknown-elf-ld -T link.ld mul.o -o mul.elf
rm -f mul.o

# 编译 div.S (RV32IM, DIV 指令)
echo "[3/3] 编译 div.S → div.elf (RV32IM)"
riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 div.S -o div.o
riscv32-unknown-elf-ld -T link.ld div.o -o div.elf
rm -f div.o

echo ""
echo "✅ 全部 ELF 编译完成:"
ls -la *.elf