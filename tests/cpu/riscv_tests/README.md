# riscv-tests rv32ui-p ELF vendor

**Source**: https://github.com/riscv-software-src/riscv-tests @ commit TBD (锁定于 commit D 实装时)
**Build container**: riscv-gnu-toolchain docker image SHA256: TBD
**License**: BSD-3-Clause (上游兼容)

## 状态

本目录为 Wave 1 of Phase 1.5 (`riscv-tests-rv32ui` OpenSpec) 的 ELF vendor 占位。
**实装时**需要：

1. 克隆 riscv-tests 仓库: `git clone https://github.com/riscv-software-src/riscv-tests`
2. 子模块: `git submodule update --init --recursive` (含 riscv-test-env 提供 link.ld)
3. 锁定 commit hash (在 commit message 中记录)
4. 运行 `./build_rv32ui.sh` 生成 41 个 ELF（排除 fence_i + ma_data）
5. `git add tests/cpu/riscv_tests/elf/` 把 ~270KB prebuilt 二进制 commit (plain git，<5MB LFS 阈值)

## 排除清单

- `fence_i` (需 Zifencei 扩展，feature stub 推到 Phase 5+)
- `ma_data` (misaligned access 行为平台未定义)

## 许可证

[riscv-tests](https://github.com/riscv-software-src/riscv-tests) BSD-3-Clause 与 ChipForge 项目协议兼容。
