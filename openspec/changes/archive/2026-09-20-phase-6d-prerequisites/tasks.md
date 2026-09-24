## 1. Prereq #1: riscv64-unknown-elf-gcc 工具链 CI 集成 (Metis 修正: 工具链已预装)

- [ ] 1.1 验证当前 build env 工具链状态 (`which riscv64-unknown-elf-gcc` 显示已预装 GNU 16.1.0)
- [ ] 1.2 CI 集成 (`.github/workflows/architecture-gates.yml` 扩 apt install step)
  - [ ] 添加 `apt install gcc-riscv64-unknown-elf` (jammy/noble 均可用)
- [ ] 1.3 CI 编译 sanity check: 5 指令 ELF 编译通过 (依赖 Prereq #6)

## 2. Prereq #2: Verilator CI 集成 (Oracle 2026-09-20 descope: yosys/iverilog 移出阻塞)

- [ ] 2.1 验证本地 build env `which verilator` 已预装 5.052 (≥5.020); yosys/iverilog **非阻塞 optional**
- [ ] 2.2 CI runner 发行版检测 (`lsb_release -rs`):
  - [ ] Ubuntu 24.04 noble runner (推荐) → `apt install verilator` 提供 ≥5.020
  - [ ] Ubuntu 22.04 jammy runner (备选) → 源码 build Verilator ≥5.020
- [ ] 2.3 安装 + 版本验证 (CI runner):
  - [ ] `verilator --version` ≥ 5.020
  - [ ] ~~`yosys --version` 输出 ≥ 0.30~~ (Oracle descope, optional)
  - [ ] ~~`iverilog -V | head -3` 输出 12+~~ (Oracle descope, optional)
- [ ] 2.4 综合 sanity check: `verilator --lint-only /tmp/cpphdl_poc_full.v` 无 error
- [ ] 2.5 CI 集成 (`.github/workflows/architecture-gates.yml` 扩 install step, 按发行版分支, 仅 Verilator 必装)

## 3. Prereq #3: ADR-037 v2.0 验证 + 修正 (Oracle 修正: adr.md:1235 已标 v2.0 Accepted, 无新撰写)

- [ ] 3.1 验证 `docs/architecture/adr.md:1233-1269` 的 v2.0 内容是否满足"7 大借鉴点 + 8 项 CI 检查 + CH_MEM 模式契约"
- [ ] 3.2 修 adr.md:1251 "Phase 6c (M1-M5, 9 周 RTL 兑现) + Phase 6d (MMU/Cache 多周期 FSM) + Phase 6e (ScoreBoard/CompareDriver)" 与现行 6a/6b/6c/6d 拆分不一致
- [ ] 3.3 同步 AGENTS.md + phase-6d-rtl-verification.md 去掉 "ADR-037 v2.0 待修订" 措辞
- [ ] 3.4 补 `adr.md:1233-1269` 显式枚举"7 大借鉴点 + 8 项 CI 检查 + CH_MEM 模式契约"三段 (与 prereq design.md §Prereq#3 模板对齐)
- [ ] 3.5 `bash tools/verify_adr.sh` 验证 cross-reference 无 broken

## 4. Prereq #4 + #5: PoC follow-up fixes 协调

- [ ] 4.1 确认 `openspec/changes/poc-follow-up-fixes/` 已 archive
- [ ] 4.2 通知 phase-6d-prerequisites change: PoC follow-up 已 archive, 此 prerequisites 可继续推进

## 5. Prereq #6: vendored ELF 测试接线 (Oracle 2026-09-20 修订)

> **Oracle 关键发现**: `tests/cpu/manual_elf/` 仅 6 个手写 .S 是事实, 但 **`tests/cpu/riscv_tests/elf/` 已 vendor 40 个 rv32ui-p-*.elf** (含目标 5 条 + 30+ 其它), TLM 5-stage 链路已 PASS。原"手写 4 .S"方案撤销。

- [ ] 5.1 给 `chipforge_tests_chmem` target 加 `TEST_RV32UI_ELF_DIR` compile define (注入 `tests/cpu/riscv_tests/elf/` 路径)
  - [ ] 5.1.1 改 `tests/CMakeLists.txt:124-127`, 对齐 TLM target 的同名 define (52-53 行)
- [ ] 5.2 6d.4 测试 `tests/cpu/test_cpu_chmem_riscv_tests.cpp` 用 `load_elf_full(elf_path)` + `elf.tohost_addr` + `mem.exited()/exit_code()` 约定
  - [ ] 5.2.1 不读硬地址 `0x80000000`, 改用 vendored ELF 的 `.tohost @ 0x80001000`
- [ ] 5.3 验收措辞放宽: 6d.4 acceptance 写"~8-9 条指令路径 (add/addi/auipc/jal/beq + lui/sw/bne)" 而非纯 5 条 (vendored ELF INIT_XREG + RVTEST_PASS 路径需额外指令)
- [ ] 5.4 解除串行依赖: 此 prereq 不依赖 Prereq #1 (vendored ELF 已 commit, 无需工具链重编)
- [ ] 5.5 不选选项 B (vendor riscv-tests submodule), 理由: 大仓库, 与现有 manual_elf + vendored 双轨 pattern 不一致

## 6. DEVELOPMENT_SETUP.md 工具链安装指南

- [ ] 6.1 添加 RISC-V 工具链安装章节 (apt + 验证 + sanity check)
- [ ] 6.2 添加综合验证工具链安装章节 (Verilator + Yosys + iverilog, 按发行版分支)
- [ ] 6.3 添加 build env 当前状态章节 (已预装 vs 缺失)
- [ ] 6.4 cross-reference phase-6d-prerequisites OpenSpec change

## 7. CI 验证 + Archive

- [ ] 7.1 `bash tools/check_plugin_portability.sh` 仍 8/8 PASS
- [ ] 7.2 `bash tools/verify_adr.sh` 仍 0 FAILED (ADR-037 v2.0 PASS)
- [ ] 7.3 `bash tools/verify_plugin_decision.sh` 仍 3+4/3 PASS
- [ ] 7.4 `./bin/chipforge_tests` TLM baseline 0 回归
- [ ] 7.5 `./bin/chipforge_tests_chmem` 全量 PASS
- [ ] 7.6 commit + push: `chore(env): Phase 6d prerequisites (riscv64 + Verilator + ADR-037 v2.0 + test ELF)`
- [ ] 7.7 CHANGELOG v0.3.x patch (Prereq #1 + #2 工具链就绪记录)
- [ ] 7.8 `openspec archive phase-6d-prerequisites --skip-validation` (验证已通过)
- [ ] 7.9 通知 phase-6d-rtl-verification change: prerequisites 已 archive, Phase 6d main 可启动

## 8. 验证（Acceptance Criteria）

- [ ] `which riscv64-unknown-elf-gcc` 成功 (CI env)
- [ ] `which verilator && (which yosys || which iverilog)` 部分成功 (Verilator 必须)
- [ ] `bash tools/check_plugin_portability.sh` 8/8 PASS
- [ ] `bash tools/verify_adr.sh` 0 FAILED
- [ ] `docs/architecture/adr.md` 注册表 ADR-037 v2.0 (验证或修正)
- [ ] `DEVELOPMENT_SETUP.md` 工具链章节完整 (按发行版分支)
- [ ] `tests/cpu/manual_elf/{addi,auipc,jal,beq}.S` 存在
- [ ] `openspec/changes/poc-follow-up-fixes/` 已 archive
- [ ] `chipforge_tests_chmem` 全量 PASS
