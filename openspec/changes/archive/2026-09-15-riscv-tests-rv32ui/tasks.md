# Tasks: `riscv-tests-rv32ui` (Wave 1 of Phase 1.5)

> **Schema**: spec-driven
> **Date**: 2026-09-15
> **Schedule**: 1.5 weeks（5 commits + 1 archive commit）
> **Strategy**: TDD 5 步（write failing test → verify fail → implement → verify pass → commit）

---

## 1. Commit A: PicolibcHostMemory base address window + write_half + write_word 修复

### 1.1 Framework 测试（Write Failing Test）
- [ ] 1.1 创建 `tests/framework/test_picolibc_memory_base_window.cpp`
  - 测试 1: `PicolibcHostMemory` default 构造保持 base=0 行为（add.elf 兼容）
  - 测试 2: `PicolibcHostMemory({base=0x80000000, size=64KB, tohost=0x80001000})` 接受读写该地址空间
  - 测试 3: `write_half(addr, val)` 写 2 字节 little-endian
  - 测试 4: `write_word(tohost, val)` 对每个字节调 `check_tohost`（修复 partial-byte bug）
  - 测试 5: base=0x80000000 时写 0x0 静默 drop（underflow guard）

### 1.2 Verify Test Fails
- [ ] 1.2 编译 + 运行测试 → 确认 5 个测试均 FAIL（无 base window + 无 write_half + write_word partial-byte bug 未修）

### 1.3 实现（Implement）
- [ ] 1.3 在 `ip/cpu/picolibc_host_memory.h` 加 `Config` 结构体（base_addr/size/tohost_addr）
- [ ] 1.4 实现 `explicit PicolibcHostMemory(Config cfg)` 构造函数（保留 default `= {}` 兼容旧代码）
- [ ] 1.5 改 `write_byte/read_byte` 等为 base-relative 算术 + 显式 underflow guard
- [ ] 1.6 修复 `write_word` partial-byte bug：改为对 addr/+1/+2/+3 各调 `check_tohost`
- [ ] 1.7 新增 `write_half(addr, val)` 函数
- [ ] 1.8 新增 `set_tohost_addr(addr)` 供 ELF loader 调
- [ ] 1.8a 新增 `void load_section(std::uint64_t sh_addr, const std::vector<std::uint8_t>& bytes)` 用于多段加载（riscv-tests 多 SHF_ALLOC PROGBITS）
- [ ] 1.9 添加 Doxygen 注释（base window 使用契约、underflow 安全性）

### 1.4 Verify Pass
- [ ] 1.10 重跑 5 个测试 → 全部 PASS
- [ ] 1.11 重跑现有 `test_cpu_sim_real_tohost` 确认 add.elf 仍 PASS（base=0 兼容）
- [ ] 1.12 重跑所有 332 测试 → 全 PASS（无 regression）

### 1.5 DBusPlugin store width 分发（合并入 commit A，因为同属"内存模型"语义层）
- [ ] 1.13 在 `ip/cpu/plugins/dbus.h` 的 `at_stage("memory", NORMAL)` STORE 路径增加 funct3 检查
- [ ] 1.14 funct3 == SB → `mem_->write_byte(addr, val)`；funct3 == SH → `mem_->write_half(addr, val)`；funct3 == SW → `mem_->write_word(addr, val)`（保留）
- [ ] 1.15 不早返（遵守 ADR-040 Tier-1 #4）：用 if-else 链替换；测试 `test_dbus_dispatch.cpp` 覆盖 SB/SH/SW 三条路径
- [ ] 1.16 commit A 测试新增 store width 分发的 4 case（SB/SH/SW + missing funct3 error）

### 1.6 Commit
- [ ] 1.17 提交 `feat(cpu): PicolibcHostMemory base address window + write_half + write_word 修复 + DBusPlugin store width 分发`（commit A）

---

## 2. Commit B: ELF loader 升级（multi-section + e_entry + .tohost 解析）

### 2.1 Framework 测试（Write Failing Test）
- [ ] 2.1 创建 `tests/framework/test_elf_loader_full.cpp`
  - 测试 1: `load_elf_full` 返回多 PROGBITS section（造一个 3-section mock ELF）
  - 测试 2: 返回 `entry_addr = 0x80000000`（造一个 mock ELF header）
  - 测试 3: 返回 `tohost_addr` 从 .tohost section sh_addr 解析（造一个含 .tohost 的 mock ELF）
  - 测试 4: section overlap → throw
  - 测试 5: ELF 无 .tohost section → tohost_addr = UINT64_MAX

### 2.2 Verify Test Fails
- [ ] 2.2 编译 + 运行 → 5 个测试均 FAIL（旧 loader 仍 break 早返 + skip e_entry + 无 .tohost 解析）

### 2.3 实现（Implement）
- [ ] 2.3 在 `tools/cpu_sim/elf_loader.h` 加 `ElfLoadResult` 结构体
- [ ] 2.4 加 `cf::tools::load_elf_full(path) → ElfLoadResult` 函数
- [ ] 2.5 实现 ELF header 解析（e_entry 提取，不再 skip）
- [ ] 2.6 改 section 遍历：移除 `break` 早返，收集所有 SHT_PROGBITS + SHF_ALLOC
- [ ] 2.7 加 shstrtab 解析（找 `.shstrtab` section，遍历所有 section header 取名）
- [ ] 2.8 加 `.tohost` section 名称匹配 → 记录其 sh_addr
- [ ] 2.9 加 section overlap 检测 → throw
- [ ] 2.10 保留 `load_elf_text` 旧函数（ad.elf 兼容）

### 2.4 Verify Pass
- [ ] 2.11 重跑 5 个测试 → 全部 PASS
- [ ] 2.12 集成测试：用 `load_elf_full` 加载 `build/add.elf`，确认 `entry_addr=0, sections=[1], tohost=0` 与旧 `load_elf_text` 结果等价

### 2.5 Commit
- [ ] 2.13 提交 `feat(tools): ELF loader multi-section + e_entry + .tohost sh_addr 解析`（commit B）

---

## 3. Commit C: cpu_sim --base-addr flag + e_entry PC init

### 3.1 Framework 测试（Write Failing Test）
- [ ] 3.1 创建 `tests/framework/test_cpu_sim_base_addr.cpp`
  - 测试 1: `--base-addr` flag 接受 hex 字符串并构造 PicolibcHostMemory
  - 测试 2: cpu_sim 主循环在 build_cpu 后、首次 run 前将 entry_addr 写入 fetch PC
  - 测试 3: `add.elf` 默认 base=0 时 fetch PC 写入 0（byte-identical）
  - 测试 4: 解析 `0x80000000` 形式与 `0x80000000` 形式两种输入
  - 测试 5: `--cycles N` 与 cycle cap 一致（无关此 change，但确认非回归）

### 3.2 Verify Test Fails
- [ ] 3.2 编译 + 运行 → 5 个测试均 FAIL（旧 cpu_sim 无 --base-addr flag + 无 PC 写入）

### 3.3 实现（Implement）
- [ ] 3.3 在 `tools/cpu_sim/main.cpp` CLI 解析阶段加 `--base-addr` flag
- [ ] 3.4 改 ELF load 路径：用 `load_elf_full` 替代 `load_elf_text`
- [ ] 3.5 `PicolibcHostMemory` 构造用 `Config{base=cli_base_addr, tohost=result.tohost_addr}`
- [ ] 3.6 在 `CpuFactory::build_cpu` 完成后、`pb->run()` 循环前写入 `pb->node_of_logic_stage("fetch")->operator()(KeyType::PC) = result.entry_addr`
- [ ] 3.7 多段加载：遍历 `result.sections` 各 `mem.load_section(sh_addr, bytes)`

### 3.4 Verify Pass
- [ ] 3.8 重跑 5 个测试 → 全部 PASS
- [ ] 3.9 手动跑 `cpu_sim --elf build/add.elf` → tohost=1 仍 5 cycles（byte-identical）
- [ ] 3.10 手动跑 `cpu_sim --elf <riscv-tests ELF> --base-addr 0x80000000` → cycle ≤100 终止（不超时）

### 3.5 Commit
- [ ] 3.11 提交 `feat(cpu): cpu_sim --base-addr flag + entry_addr → fetch PC`（commit C）

---

## 4. Commit D: 41 ELF vendor + Catch2 fixture + JUnit reporter + CSV triage

### 4.1 ELF vendor 准备
- [ ] 4.1 在 `tests/cpu/riscv_tests/` 创建目录
- [ ] 4.2 写 `tests/cpu/riscv_tests/build_rv32ui.sh` 脚本（基于 `tools/cpu_sim/...` 既有工具链路径）
- [ ] 4.3 运行 build 脚本生成 41 个 `rv32ui-p-*.elf`（exclude fence_i + ma_data）
- [ ] 4.4 写 `tests/cpu/riscv_tests/README.md`：commit hash + 容器镜像 digest + License
- [ ] 4.5 `git add` 41 ELF（plain git，约 270KB）
- [ ] 4.5a 修改 `tests/CMakeLists.txt` 注入两个 compile definition 到 `chipforge_tests` 目标：
  - `TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"`（绝对路径，规避 ctest 工作目录问题）
  - `RV32UI_CSV_PATH="${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv"`（绝对路径）
  - 确认 `tests/CMakeLists.txt:6-8` GLOB_RECURSE 自动拾取 `test_rv32ui_runner.cpp`，无需新增 subdir CMakeLists.txt
- [ ] 4.5b fixture 首次运行前确认 `soc/cpu/docs/dse/` 目录存在（fixture helper 内 `fs::create_directories(parent_path)`）

### 4.2 Catch2 fixture（Write Failing Test）
- [ ] 4.6 创建 `tests/cpu/integration/test_rv32ui_runner.cpp`
  - 41 个 TEST_CASE（macro 循环）+ JUnit reporter 路径 + CSV 写入 helper
  - 期望：跑时多数 TEST_CASE FAIL（vendor ELF 是 RV32 链接到 0x80000000，riscv-tests 用 jal/branch/lw/sw 真实指令，旧 CPU 多处不通过）

### 4.3 Verify Test Fails（runner-mechanics 应全 PASS，CSV 暴露 FAIL）
- [ ] 4.7 编译 → 全 41 测试编译通过
- [ ] 4.8 运行 → 41 个 TEST_CASE 在 Catch2 层全 PASS（runner-mechanics REQUIRE 全部满足：cycle 内到 tohost）；CSV 记录 `status` 列：PASS 数 ≥ 35；FAIL 行分类到 `category=真 bug/feature stub/timeout`，仅作 Wave 2 fix 候选

### 4.4 集成补充实现（Implement）
- [ ] 4.9 写 helper `run_rv32ui_test(name)` 实现 design §3 Decision 4
- [ ] 4.10 写 CSV append helper：使用 `std::once_flag` 首次截断写入 header；每个 TEST_CASE 在 REQUIRE 断言**之前** append 本行（含 status/category/cycles）；REQUIRE 失败/超时仍保证行落盘
- [ ] 4.11 写 pre-existing 5 测试 append helper：在 fixture SETUP 阶段用 `exec_cmd` 子进程跑 `./build/bin/cpu_sim --elf build/add.elf` 五次（每 stage 配置），解析 tohost，落 5 行 CSV（category=toolchain 或 empty 表示 PASS）
- [ ] 4.12 验证 pre-existing 5 测试通过 loader 升级转 PASS（若仍 fail，分类 toolchain 而非 真 bug）

### 4.5 Verify Pass + 记录 baseline
- [ ] 4.13 重跑 41 tests，记录实际 pass/fail 数（目标：至少 ≥35/41）
- [ ] 4.14 重跑 pre-existing 5 tests，确认 5/5 PASS
- [ ] 4.15 全套 Catch2 测试 332 baseline + 41 新增 = 373（5 pre-existing 在 332 内已计，不新增 Catch2 测试数）；CSV 额外 5 行由 fixture subprocess 产生
- [ ] 4.16 检查 JUnit XML 输出（`build/rv32ui-baseline-junit.xml`）格式有效
- [ ] 4.17 检查 CSV `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 含 47 行（41+5+1 header）

### 4.6 Commit
- [ ] 4.18 提交 `feat(cpu): riscv-tests rv32ui-p baseline + 41 ELF vendor + CSV triage`（commit D）

---

## 5. Commit E: ADR + CHANGELOG + roadmap sync + STATUS + archive

### 5.1 文档同步
- [ ] 5.1 CHANGELOG 加 `## v0.2.0 (2026-09-XX) - riscv-tests-rv32ui` 条目（baseline 332 → ≥373 Catch2 tests + 47 行 CSV；41 rv32ui-p 中 ≥35 PASS）
- [ ] 5.2 同步 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`：
  - §2 Wave 1 估时 1 周 → 1.5 周
  - §6.1 risk 表补 vendor ELFs 风险（已实现缓解）
  - §11 风险表：Wave 1 缓解 §11.1（scoreboard lifecycle 探针）
- [ ] 5.3 同步 `ip/cpu/STATUS.md`：新增"RV32I 合规模性已建立 (v0.2.0)"段落
- [ ] 5.4 同步 `ip/cpu/picolibc_host_memory.h` Doxygen（base window 契约）
- [ ] 5.5 同步 `tools/cpu_sim/elf_loader.h` Doxygen（多 section 契约）
- [ ] 5.6 `git add` CSV `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`（commit artifact）

### 5.2 ADR（optional，按 design Decision 3 Decision 4 文档化）
- [ ] 5.7 评估是否需要 ADR-046（base address window），若需要写 `docs/architecture/adr/ADR-046-picolibc-base-window.md` + 注册到 `adr.md`

### 5.3 验证 + 归档
- [ ] 5.8 跑 `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh && bash tools/doc_link_check.sh` → 4 gates 全 PASS
- [ ] 5.9 5/5 稳定连跑测试
- [ ] 5.10 跑 `openspec validate riscv-tests-rv32ui` → PASS
- [ ] 5.11 归档 change：`openspec archive riscv-tests-rv32ui --yes` → `openspec/changes/archive/2026-09-15-riscv-tests-rv32ui/`

### 5.4 Commit
- [ ] 5.12 提交 `docs: CHANGELOG v0.2.0 + roadmap sync + STATUS + archive riscv-tests-rv32ui`（commit E）

---

## 6. Verify Apply Requirements

- [ ] 6.1 跑 `openspec status --change riscv-tests-rv32ui --json` → applyRequires: ["tasks"] ✓ status: done
- [ ] 6.2 所有 4 个 artifact（proposal/design/specs/tasks）status: done
- [ ] 6.3 Wave 1 交付清单：
  - [ ] 41 rv32ui-p ELF 在 `tests/cpu/riscv_tests/elf/`
  - [ ] 41 TEST_CASE 在 `tests/cpu/integration/test_rv32ui_runner.cpp`
  - [ ] JUnit 输出 `build/rv32ui-baseline-junit.xml`
  - [ ] CSV `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`
  - [ ] 5 pre-existing RISC-V 测试转 green
  - [ ] 4 architecture gates 全 PASS

---

## 附：TDD 5 步结构（commit-level）

每个 commit 严格遵循：

1. **Write failing test**：先写测试，测试必须在改代码前 FAIL
2. **Verify fail**：手动跑测试，确认 FAIL（不能改代码同时改测试）
3. **Implement**：实现功能
4. **Verify pass**：测试 PASS + 既有测试不回归
5. **Commit**：原子提交，commit message 描述改动 + 关联 phase-1.5 roadmap

不允许跳过任何步骤。commit D 的 TDD 步骤略有不同（基线暴露是设计意图），但仍需先 FAIL 后记录 PASS。