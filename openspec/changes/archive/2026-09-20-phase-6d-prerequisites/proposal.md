# Phase 6d Prerequisites (Phase 6d 启动前置)

> **关联 OpenSpec**: `openspec/changes/phase-6d-prerequisites/`
> **关联 phase doc**: [docs/roadmap/phases/phase-6d-rtl-verification.md](../../docs/roadmap/phases/phase-6d-rtl-verification.md) §1.2

## Why

Phase 6c `plugin-elaboration-substrate` 已完成 (v0.3.0 + v0.3.1, 2026-09-20), Phase 6d `phase-6d-rtl-verification` 待启动。Phase 6d 目标 = riscv-tests RV32I 5 指令端到端 `tohost=1` CppHDL sim + Verilator 集成 + MMU/PTW 多周期 FSM。**Phase 6d 启动前硬前置 (Metis 2026-09-20 评审修正)**:

1. **riscv64-unknown-elf-gcc 工具链** — **当前 build env 已预装** (`/workspace/main/opt/riscv/bin/riscv64-unknown-elf-gcc`, GNU 16.1.0), 5 ELF 文件已 vendor (`tests/cpu/riscv_tests/elf/rv32ui-p-{add,addi,auipc,jal,beq}.elf`)。Phase 6d 启动阻塞项 = **CI 集成** (`.github/workflows/architecture-gates.yml` 在 install 阶段加 riscv64-elf-gcc apt step, 避免依赖 build env 预装), 而非工具链安装。
2. **Verilator ≥ 5.020** — **当前 build env 已预装** (`/workspace/main/opt/verilator/bin/verilator`, v5.052 ≥ 5.020)。但 **Yosys + iverilog 仍缺失** (综合验证需 yosys, 但 Phase 6d 最小退出标准 E7 仅要求 `verilator --lint-only`, 不依赖 yosys)。**Phase 6d 启动阻塞项 = 缺失工具 CI 集成**, 而非主工具链安装。
3. **ADR-037 v2.0** ✅ 已完成 (Oracle 2026-09-20 确认: adr.md:1235 已标 v2.0 Accepted; D10/D11/D12 已写完; Prereq #3 本次验证 + 修 adr.md:1251 拆分描述)
4. **PoC follow-up fixes** (`poc-follow-up-fixes` change, 独立) → Phase 6d.4 5-stage byte-equal 验证基础不稳
5. **M3 HazardPlugin 完整 CH_MEM 版** (PoC follow-up 中) → Phase 6d 端到端验证需要完整 RAW 检测

**Why now**: Phase 6d 启动门槛。如果工具链 CI 集成 + ADR-037 + PoC follow-up 不解决, Phase 6d 6d.4-6d.5 立即 SEGV / 不可编译 / 验证失败。Phase 6c 推迟项 (CHANGELOG v0.3.0 §M5 已知剩余问题) 全部在此 prerequisites change 闭环。

**Metis 修正 (2026-09-20)**: 原稿假设工具链缺失是真实紧迫阻塞, 但实际 build env 已经预装, 主要工作是 CI 集成和文档同步, 估时应从 5-7 天压缩到 2-3 天。

## What Changes

### Prereq #1: riscv64-unknown-elf-gcc 工具链安装 (Oracle 修正: 当前 build env 已预装)

- **检查现状 (Metis 2026-09-20 实测)**: `/workspace/main/opt/riscv/bin/riscv64-unknown-elf-gcc` 已预装, GNU 16.1.0
- **Phase 6d 启动阻塞项**: CI 集成, 而非工具链安装
- **安装方式** (CI 集成):
  - 选项 A (推荐): apt install gcc-riscv64-unknown-elf (`apt list --installed | grep riscv`)
  - 选项 B: 从 https://github.com/riscv-collab/riscv-gnu-toolchain 下载预编译包
- **验证**: 跑 `riscv64-unknown-elf-gcc --version` 输出 GNU 14+ + riscv-tests 5 指令 ELF 编译通过
- **CI 集成**: `.github/workflows/architecture-gates.yml` + `.github/workflows/build.yml` (如有) 在 install 阶段加 riscv64-elf-gcc apt step

### Prereq #2: Verilator CI 集成 (Oracle 2026-09-20 修正: yosys/iverilog descope 为 optional)

**Oracle 2026-09-20 修正**: 最小退出标准 E1-E6 完全不需要 yosys/iverilog; `tasks.md 5.6` 已标 yosys 可选, iverilog 在 6d 全部任务中**零引用**。Prereq #2 收缩为仅 Verilator CI 集成 (~0.5 天)。

- **检查现状 (Metis 2026-09-20 实测)**: Verilator 5.052 已预装 (≥5.020); yosys / iverilog 未装
- **Phase 6d 启动阻塞项**: 仅 Verilator CI 集成 (E7 `verilator --lint-only`); yosys + iverilog **移出阻塞项, 标注 optional**
- **安装方式 (CI runner, 不阻塞本地开发)**:
  - **Ubuntu 24.04 noble runner (推荐)**: `apt install verilator` 提供 ≥5.020
  - **Ubuntu 22.04 jammy runner (备选)**: apt 源 verilator 4.038 < 5.020 ❌; **必须源码 build** Verilator ≥5.020
- **验证** (CI runner):
  - `verilator --version` ≥ 5.020
- **CI 集成**: `.github/workflows/architecture-gates.yml` 扩 install step (按发行版分支), 仅 Verilator 必装; yosys/iverilog 标 optional
- **本地开发**: build env 已预装 Verilator 5.052, 无需任何安装

### Prereq #3: ADR-037 v2.0 修订 (Oracle 发现: adr.md:1235 已标 v2.0 Accepted)

- **当前状态 (Oracle 2026-09-20 发现)**: `docs/architecture/adr.md:1235` 已标 `✅ v2.0 Accepted (Phase 6c M5 落地, 2026-09-17, D4 elaboration 语义兑现)`, D10/D11/D12 v2.0 内容已写入 (§1233)
- **状态冲突**: 原 AGENTS.md + phase-6d-rtl-verification.md + phase-6d-prerequisites/proposal.md 三处称 "ADR-037 v2.0 待修订"（已同步修正: Oracle 2026-09-20 确认 v2.0 Accepted）
- **需要决策 (Oracle #1)**: 要么 prereq #3 是重复劳动 (adr.md 已完成), 要么 adr.md 提前虚标 → 二选一澄清
- **实际工作**:
  - 验证 adr.md:1233-1269 的 v2.0 内容是否满足"7 大借鉴点 + 8 项 CI 检查 + CH_MEM 模式契约"
  - 修 adr.md:1251 "Phase 6c (M1-M5, 9 周 RTL 兑现) + Phase 6d (MMU/Cache 多周期 FSM) + Phase 6e (ScoreBoard/CompareDriver)" 与现行 6a/6b/6c/6d 拆分不一致
- **关联更新**: `docs/architecture/adr.md` 注册表 ADR-037 v2.0 + cross-reference ADR-040 v2.0 + ADR-046

### Prereq #4: PoC follow-up fixes (独立 change `poc-follow-up-fixes`)

详见 [`openspec/changes/poc-follow-up-fixes/proposal.md`](../poc-follow-up-fixes/proposal.md)。

**依赖关系**: `poc-follow-up-fixes` 必须先完成 (archive), 然后 `phase-6d-prerequisites` 才能 archive。

### Prereq #5: M3 HazardPlugin 完整 CH_MEM 版 (在 PoC follow-up 内)

`poc-follow-up-fixes` change 内 Fix #3。完成后 HazardPlugin 与 `id_decode` 集成, 5-stage 流水线 RAW 检测正确。

### Prereq #6: vendored ELF 测试接线 (Oracle 2026-09-20 修订: 原"编写 4 .S"方案已撤销)

**Oracle 关键发现 (2026-09-20)**: 原稿假设 `tests/cpu/manual_elf/` 缺 addi/auipc/jal/beq 4 条 ELF。但 `tests/cpu/riscv_tests/elf/` 已 vendor **40 个 `rv32ui-p-*.elf`** (含 add/addi/auipc/beq/jal 等 5 条目标指令及 30+ 其它), 由 riscv-software-src/riscv-tests @ `2ebecad` 上游源码 + ChipForge `env-p/` harness (直接 `sw TESTNUM, tohost`, 无 trap handler, 配 PicolibcHostMemory) 构建, 2026-09-15 commit `feb602a` 落地。TLM 5-stage 上 5 ELF 已 PASS (见 `tests/cpu/test_rv32ui_runner.cpp:88-106` + `load_elf_full` + `PicolibcHostMemory::Config{base_addr, tohost_addr}`)。

**实际工作** (取代原"手写 4 .S"):

1. **CMake define 注入**: 在 `tests/CMakeLists.txt:124-127` 给 `chipforge_tests_chmem` target 加 `TEST_RV32UI_ELF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/riscv_tests/elf"` compile define (TLM target 已有 `tests/cpu/riscv_tests/README.md:5` 模式)
2. **6d.4 测试用 vendored ELFs**: `tests/cpu/test_cpu_chmem_riscv_tests.cpp` 沿用 `load_elf_full(elf_path)` + `elf.tohost_addr` + `mem.exited()/exit_code()` 约定, 不读 `0x80000000` 硬地址
3. **接受路径扩展**: vendored ELF 涉及 `lui/sw/bne` 额外指令 (INIT_XREG + RVTEST_PASS 路径), 6d.4 acceptance 措辞放宽为"~8-9 条指令路径 (add/addi/auipc/jal/beq + lui/sw/bne)" 而非纯 5 条
4. **解串行**: 此 prereq 不再依赖 Prereq #1 (工具链); vendored ELF 已 commit, 无需重编

**附注**: Option B (vendor riscv-tests submodule) 仍不推荐, 大仓库与现有 manual_elf + vendored 双轨 pattern 不一致。

## Capabilities

### New Capabilities

- `riscv64-toolchain-install`: riscv64-unknown-elf-gcc 工具链安装 + CI 集成
- `verilator-toolchain-install`: Verilator ≥ 5.020 + Yosys + iverilog 安装 + CI 集成
- `adr-037-v2-elaboration`: ADR-037 v2.0 修订 + docs/architecture/adr.md 同步
- `poc-follow-up-fixes-archive`: PoC follow-up fixes change 归档 (独立 OpenSpec change 跟踪)

### Modified Capabilities

(无 — 此 change 是 prereqs 跟踪, 不修改 spec-level 行为)

## Impact

- **影响文件**:
  - `.github/workflows/architecture-gates.yml` (Prereq #1 riscv64 + #2 Verilator CI 集成; yosys/iverilog optional)
  - `.github/workflows/build.yml` (如有, Prereq #1 + #2 CI 集成)
  - `docs/architecture/adr.md` (Prereq #3 验证 + 修正 1251 行拆分描述 + 补"7 借鉴点 + 8 CI 检查"段)
  - `openspec/changes/poc-follow-up-fixes/` (Prereq #4 + #5 独立 change)
  - `tests/CMakeLists.txt` (Prereq #6 给 `chipforge_tests_chmem` target 加 `TEST_RV32UI_ELF_DIR` define)
  - `DEVELOPMENT_SETUP.md` (Prereq #1 + #2 安装指南更新)
  - **(移除)** `docs/architecture/adr/ADR-037-plugin-as-design-paradigm.md` — 该独立文件**不存在**, ADR-037 v2.0 内容在 `adr.md:1233-1269` 内联 (无需新建独立文件)
- **测试**: 全部现有测试 PASS (prereqs 不修改业务代码)
- **依赖**: PoC follow-up fixes change 必先 archive
- **不破坏**: ADR-040 v2.0 + ADR-046 + `check_plugin_portability.sh` 8/8 PASS

## Tasks

1. **Prereq #1: riscv64-unknown-elf-gcc 安装** (1-2 天)
   - 验证当前 build env 工具链缺失
   - 选 apt vs 源码, 安装 + 验证版本
   - CI 集成 (`apt install gcc-riscv64-unknown-elf`)
   - 跑 `riscv-tests/rv32ui-p-add` ELF 编译 + 跑通 (sanity check)
2. **Prereq #2: Verilator + Yosys + iverilog 安装** (1-2 天)
   - 验证当前 build env 缺失
   - apt install verilator + yosys + iverilog
   - 版本验证
   - CI 集成
   - `verilator --lint-only` 一个 hello.v 测试
3. **Prereq #3: ADR-037 v2.0 验证 + 修正** (1 天)
   - 验证 `docs/architecture/adr.md:1233-1269` 已含 D10/D11/D12 v2.0 内容 (Oracle 2026-09-20 已确认 Accepted)
   - 修 `adr.md:1251` "Phase 6c (M1-M5, 9 周 RTL 兑现) + Phase 6d (MMU/Cache 多周期 FSM) + Phase 6e (ScoreBoard/CompareDriver)" 与现行 6a/6b/6c/6d 拆分不一致
   - 同步 AGENTS.md + `docs/roadmap/phases/phase-6d-rtl-verification.md` + 本 proposal 三处原"ADR-037 v2.0 待修订"措辞（已修正: Oracle 2026-09-20 确认 v2.0 Accepted）
   - 补 `adr.md:1233-1269` 显式枚举"7 大借鉴点 + 8 项 CI 检查 + CH_MEM 模式契约"三段 (与 prereq design.md §Prereq#3 模板对齐)
   - cross-reference ADR-040 v2.0 + ADR-046
   - `bash tools/verify_adr.sh` 验证 cross-reference 无 broken
4. **Prereq #4 + #5: PoC follow-up fixes 跟踪** (0.5 天协调)
   - 确保 `poc-follow-up-fixes` change 启动
   - 等其 archive 后此 prerequisites change 才能 archive
5. **DEVELOPMENT_SETUP.md 更新** (0.5 天)
   - 添加 riscv64 + Verilator + Yosys + iverilog 安装指南
   - 添加 build env prerequisites 章节
6. **CI 验证 + archive** (0.5 天)
   - 全部 5 项 prereqs 闭环验证
   - commit + `openspec archive phase-6d-prerequisites`
   - CHANGELOG v0.3.x patch (Prereq #1 + #2 工具链就绪记录)

## 风险与回退

| 风险 | 回退 |
|------|------|
| apt 源无 riscv64-unknown-elf-gcc | **回退**: 从 https://github.com/riscv-collab/riscv-gnu-toolchain 下载预编译 |
| Verilator apt 源版本 < 5.020 | **回退**: 源码 build Verilator 5.020+ (cmake + make) |
| ADR-037 v2.0 修订 scope creep | **回退**: 仅添加 §v2 翻转摘要, 其他章节推迟到 v3 |
| PoC follow-up fixes 延期 | **回退**: Phase 6d main 启动推迟, prereqs 不 archive |

## 时间盒

| 子任务 | 估时 |
|------|------|
| Prereq #1 riscv64 工具链 | 1-2 天 |
| Prereq #2 Verilator 工具链 | 1-2 天 |
| Prereq #3 ADR-037 v2.0 | 1 天 |
| Prereq #4 + #5 协调 | 0.5 天 |
| DEVELOPMENT_SETUP.md 更新 | 0.5 天 |
| CI 验证 + archive | 0.5 天 |
| **总计** | **~5-7 天 (1-1.5 周)** |

**Phase 6d main 启动依赖**: 5 项 prereqs 全部 archive + verify_adr 0 FAILED + check_plugin_portability 8/8 PASS + chipforge_tests_chmem 全量 PASS。
