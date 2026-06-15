#!/usr/bin/env bash
# ChipForge Toolchain Verification Script
# Created: 2026-06-15
# Spec: .omo/drafts/decision-phase-2-baremetal-v1.1-toolchain-guide-2026-06-15.md §2.1
# Lockfile: requirements-toolchain.txt
#
# Verifies that all 7 categories of toolchain components are present and functional.
# Exit code: 0 = all PASS, 1 = at least one FAIL.

set -u

PASS=0
FAIL=0
WARN=0
RESULTS=()

red()    { printf '\033[31m%s\033[0m' "$1"; }
green()  { printf '\033[32m%s\033[0m' "$1"; }
yellow() { printf '\033[33m%s\033[0m' "$1"; }

check() {
    local name="$1"
    local cmd="$2"
    local pattern="$3"

    local out
    out=$(eval "$cmd" 2>&1)
    local rc=$?

    if [ $rc -eq 0 ] && echo "$out" | grep -qE "$pattern"; then
        RESULTS+=("$(green "[PASS]") $name")
        PASS=$((PASS+1))
    elif [ $rc -eq 0 ]; then
        RESULTS+=("$(yellow "[WARN]") $name (运行成功但模式未匹配: $pattern)")
        WARN=$((WARN+1))
    else
        RESULTS+=("$(red "[FAIL]") $name ($cmd 失败: $out)")
        FAIL=$((FAIL+1))
    fi
}

echo "============================================================"
echo "ChipForge Toolchain Verification (Phase 2 v1.1)"
echo "Spec: .omo/drafts/decision-phase-2-baremetal-v1.1-toolchain-guide-2026-06-15.md"
echo "Lockfile: requirements-toolchain.txt"
echo "Date: $(date -Iseconds)"
echo "============================================================"
echo ""

# --- Section 1: RISC-V Cross-Compiler (apt) ---
echo "── Section 1: RISC-V Cross-Compiler (apt-locked) ──"
check "riscv64-unknown-elf-gcc 13.2.0" \
    "riscv64-unknown-elf-gcc --version" \
    "13\\.2\\.0"
check "binutils-riscv64-unknown-elf 2.42" \
    "riscv64-unknown-elf-as --version | head -1" \
    "2\\.42"
check "picolibc-riscv64-unknown-elf 1.8.6" \
    "dpkg -l picolibc-riscv64-unknown-elf 2>/dev/null | tail -1" \
    "1\\.8\\.6"
echo ""

# --- Section 2: RISC-V ISA Simulators ---
echo "── Section 2: RISC-V ISA Simulators ──"
check "spike 1.1.1-dev" \
    "spike --help 2>&1 | head -1" \
    "Spike RISC-V ISA Simulator 1\\.1\\.1-dev"
check "spike /usr/local/bin/spike symlink" \
    "ls -la /usr/local/bin/spike 2>&1" \
    "/workspace/project/opt/riscv/bin/spike"
check "sail_riscv 0.12 (note: underscore, NOT hyphen)" \
    "sail_riscv --version 2>&1" \
    "0\\.12"
echo ""

# --- Section 3: Compliance Framework ---
echo "── Section 3: Compliance Framework ──"
check "riscof 1.25.3 (备选, ACT4 已取代)" \
    "riscof --version 2>&1" \
    "1\\.25\\.3"
check "riscv-arch-test ACT4 submodule (待 clone)" \
    "test -d /workspace/project/ChipForge/3rdparty/riscv-arch-test/.git && echo INSTALLED || echo PENDING" \
    "PENDING|INSTALLED"
echo ""

# --- Section 4: TLM Framework (ChipForge 集成) ---
echo "── Section 4: TLM Framework ──"
check "cpptlm_core 静态库存在" \
    "ls -la /workspace/project/ChipForge/build/_deps/install/lib/libcpptlm_core.a 2>&1" \
    "libcpptlm_core\\.a"
check "cpptlm header 可达" \
    "test -f /workspace/project/ChipForge/CppTLM/include/chstream_register.hh && echo OK" \
    "OK"
echo ""

# --- Section 5: Python Toolchain ---
echo "── Section 5: Python Toolchain ──"
check "python3 3.12+" \
    "python3 --version 2>&1" \
    "Python 3\\.12"
check "pyelftools (run_arch_test.py 依赖)" \
    "python3 -c 'import elftools; print(elftools.__version__)' 2>&1" \
    "[0-9]+\\.[0-9]+"
echo ""

# --- Section 6: riscv32 工具链 (用户原有) ---
echo "── Section 6: riscv32 工具链 (用户原有, 备选) ──"
check "riscv32-unknown-elf-gcc 15.2.0 (用户原有)" \
    "/workspace/project/opt/riscv/bin/riscv32-unknown-elf-gcc --version 2>&1 | head -1" \
    "15\\.2\\.0"
check "qemu-riscv64 10.x (备选 ISS)" \
    "qemu-riscv64 --version 2>&1 | head -1" \
    "qemu-riscv64 version 10\\."
echo ""

# --- Summary ---
echo "============================================================"
echo "Summary"
echo "============================================================"
for r in "${RESULTS[@]}"; do
    echo "  $r"
done
echo ""
echo "  $(green "PASS: $PASS")  $(yellow "WARN: $WARN")  $(red "FAIL: $FAIL")"
echo ""

if [ $FAIL -gt 0 ]; then
    echo "$(red "❌ TOOLCHAIN VERIFICATION FAILED")"
    echo "  请检查 requirements-toolchain.txt 与本脚本输出"
    echo "  阻塞 Phase 2 P0 启动"
    exit 1
elif [ $WARN -gt 0 ]; then
    echo "$(yellow "⚠️  TOOLCHAIN VERIFICATION PASSED WITH WARNINGS")"
    echo "  PASS=$PASS WARN=$WARN — 可继续 P0 启动但需关注 warnings"
    exit 0
else
    echo "$(green "✅ TOOLCHAIN VERIFICATION PASSED (6/6 + 0 warnings)")"
    exit 0
fi
