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

TEMPLATE="${1:-all}"

# Phase 6d vendor 脚本: 默认 all 编译 add/mul/div (向后兼容), 新增 mmu_bare/l1cache_basic.

build_add() {
  echo "[build_add] 编译 add.S → add.elf (RV32I)"
  # cpu-sim-add-elf-tohost-fix: 使用 link_add.ld (riscv-tests 风格, base=0x80000000, .tohost@0x80001000)
  # 其他 .S (mul/div/build_mmu_bare/build_l1cache_basic) 仍用 link.ld (base=0x0) 保持向后兼容
  riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 add.S -o add.o
  riscv32-unknown-elf-ld -T link_add.ld add.o -o add.elf
  rm -f add.o
}

build_mul() {
  echo "[build_mul] 编译 mul.S → mul.elf (RV32IM)"
  riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 mul.S -o mul.o
  riscv32-unknown-elf-ld -T link.ld mul.o -o mul.elf
  rm -f mul.o
}

build_div() {
  echo "[build_div] 编译 div.S → div.elf (RV32IM)"
  riscv32-unknown-elf-as -march=rv32im -mabi=ilp32 div.S -o div.o
  riscv32-unknown-elf-ld -T link.ld div.o -o div.elf
  rm -f div.o
}

# Phase 6d P1 plumbing-only stub probe (verilator-cpu-factory-extensible-params)
build_mmu_bare() {
  echo "[build_mmu_bare] 编译 build_mmu_bare.S → mmu_bare.elf (RV32I, plumbing-only)"
  riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 build_mmu_bare.S -o mmu_bare.o
  riscv32-unknown-elf-ld -T link.ld mmu_bare.o -o mmu_bare.elf
  rm -f mmu_bare.o
}

# Phase 6d P2 placeholder (verilator-l1cache-e2e-coverage wave4 owner)
build_l1cache_basic() {
  echo "[build_l1cache_basic] 编译 build_l1cache_basic.S → l1cache_basic.elf (RV32I, cache probe)"
  riscv32-unknown-elf-as -march=rv32i -mabi=ilp32 build_l1cache_basic.S -o l1cache_basic.o
  riscv32-unknown-elf-ld -T link.ld l1cache_basic.o -o l1cache_basic.elf
  rm -f l1cache_basic.o
}

# csr-chmem-pipeline-integration C.1: Zicsr 最小测试程序 (CSRRW + tohost=1)
#   注: scope-partial. cpu_sim CLI 路径 framework-level bug 阻塞 e2e (defer
#   cpu-pipeline-tick-rca); 本函数仅提供 .elf 编译基础设施就位, 集成 e2e 测试
#   推迟到后续 session (走 C++ API 路径, 类似 test_rv32ui_runner)
build_zicsr_basic() {
  echo "[build_zicsr_basic] 编译 zicsr_basic.S → zicsr_basic.elf (RV32I + Zicsr)"
  riscv32-unknown-elf-as -march=rv32i_zicsr -mabi=ilp32 zicsr_basic.S -o zicsr_basic.o
  riscv32-unknown-elf-ld -T link_add.ld zicsr_basic.o -o zicsr_basic.elf
  rm -f zicsr_basic.o
}

case "$TEMPLATE" in
  all|"")
    build_add
    build_mul
    build_div
    ;;
  add)          build_add ;;
  mul)          build_mul ;;
  div)          build_div ;;
  mmu_bare)     build_mmu_bare ;;
  l1cache_basic) build_l1cache_basic ;;
  zicsr_basic)  build_zicsr_basic ;;
  *)
    echo "FAIL: 未知模板 '$TEMPLATE'"
    echo "      可用: all | add | mul | div | mmu_bare | l1cache_basic | zicsr_basic"
    exit 1
    ;;
esac

echo ""
echo "✅ 编译完成 (template=$TEMPLATE):"
ls -la *.elf 2>/dev/null || echo "(no .elf artifacts)"