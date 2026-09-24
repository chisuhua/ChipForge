# Change: `riscv-tests-rv32ui` — RV32I 客观合规基线（Wave 1 of Phase 1.5）

> **Schema**: spec-driven
> **Date**: 2026-09-15
> **Status**: PROPOSED
> **Priority**: Wave 1 of Phase 1.5（详见 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`）
> **Depends on**: `plugin-framework-stall` (v0.1.3, archived 2026-09-15)
> **Unblocks**: `soc-cpu-l1-mmu-demo` · `cache-dse-sweep` · `cpu-pipeline-fix-rv32ui-N` · `cpu-pipeline-multi-cycle`

## Why

> Wave 1 of Phase 1.5 — 建客观 RV32I 合规基线。当前 5 个 pre-existing RISC-V 仿真失败无客观根因分析；1 周（实际 1.5 周含 ecall/memory model 修复）投入换来全 Phase 1.5 周期的可量化矩阵。详见 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`。

## What Changes

> **Capabilities**: 1 MODIFIED + 2 NEW（详见 Capability 段）
> - `cpu-real-fetch-and-memory` MODIFIED: base address window + multi-section loader + e_entry + DBusPlugin STORE funct3 dispatch
> - `riscv-tests-fixture` NEW: Catch2 runner + 41 TEST_CASE 契约（excl fence_i/ma_data）
> - `rv32ui-p-triage-output` NEW: CSV 产物契约（Wave 2 消费）

详细动机 / 决策 / 实施见后续 §1-§8。

## 1. Why（背景与动机）

### 1.1 当前状态（2026-09-15, post-plugin-framework-stall v0.1.3）

**Plugin pipeline 真跑 add.elf → tohost=1**（5 cycles，CPU 整数 ALU 路径已贯通）：
- `tools/cpu_sim/main.cpp:147-148` 通过 `cf::tools::load_elf_text(path, base_addr)` 加载 ELF 至 `PicolibcHostMemory`
- `main.cpp:181` 在每个 cycle 后检查 `mem.exited()`（tohost 非零即 break）
- `tests/cpu/manual_elf/add.S`（6 指令 addi/add/sw/jal 循环）作为唯一端到端 ELF 测试

**但合规模性无客观基线**：
- 仅 add.elf 一根烟囱测试，无法判断其他 RV32I 指令路径正确性
- "通过率" 的 5 个 pre-existing 失败（`test_3stage_riscv` × 4 + `test_cpu_sim_real_tohost`）是 toolchain 配置问题（per `tests/README.md:78`），与代码无关，被误归为 "真 bug" 类别
- Wave 2 `soc-cpu-l1-mmu-demo` 的 5 个 `riscv-tests` 用例选题 gate 缺失客观矩阵

### 1.2 为什么是 Now

**Phase 1.5 战略判断（`phase-1.5-stall-and-validate.md` §0/§1）**：
- **"先验证后堆叠"**：先建客观合规基线，再做 SoC demo；否则 demo 跑不动时回溯根因（feature stub vs 真 bug vs toolchain）耗时翻倍
- 1 周评估 demo 工作（CHANGELOG Pending 顺序）已反转：Wave 1 先建 riscv-tests 矩阵，Wave 2 同时做 demo + 真 bug 修复

**Oracle R1 + 三方调研（4 路 background tasks）发现 3 个隐性 blocker，必须在 Wave 1 一并修**：

| # | 阻塞点 | 现象 | 修法 |
|---|--------|------|------|
| 1 | **完整内存映射错配**（不只是 tohost） | `picolibc_host_memory.h:46,54` bounds check 在 `check_tohost` 之前；写 0x80001000 静默丢弃；fetch 读 0x80000000 永远返回 0 | 参数化 base address window（base=0 保持 add.elf；riscv-tests base=0x80000000） |
| 2 | **ELF loader 只取首个 PROGBITS section** | `elf_loader.h:99-105` 遇 PROGBITS 即 `break`；riscv-tests ELF 含 .text.init/.tohost/.data 三个 SHF_ALLOC，.data 不加载 → lw/sw 读到全 0 | 遍历所有 SHF_ALLOC PROGBITS 各按 sh_addr 加载 |
| 3 | **ELF e_entry 跳过 + PC 初始化缺失** | `elf_loader.h:63-64` 显式跳过 e_entry；fetch PC 初值 0；base=0x80000000 后 PC=0 落在窗口外 → 41/41 全部超时 | loader 返回 e_entry，cpu_sim 写入 fetch 节点 PC |

**Hidden bug 1（顺带修）**：
- `write_word` 用 `val & 0xFF` 传给 `check_tohost`（`picolibc_host_memory.h:65`）— 只查低字节，高字节 tohost 写入会漏检
- **没有 `write_half`**（`sh` 指令无处理函数）— riscv-tests 大量使用

### 1.3 战略价值（Phase 1.5 毕业标准的根基）

**Wave 1 输出 = 客观基线 → Wave 2/3/4 据此决策**：
- Wave 2 demo 选题 gate：从 41 个里选"已 pass" 子集
- Wave 2 `cpu-pipeline-fix-rv32ui-N`：分类报告 → 决定 N（真 bug 数）
- Wave 3 DSE：在已稳定 CPU 上做 sweep
- Phase 1.5 毕业标准（§8）：RV32I rv32ui-p ≥85% pass

**Phase 5 RTL co-verification 长期复用**：riscv-tests 合规矩阵可直接作为 RTL 黄金向量（4 阶段影响，文档未捕获但 Oracle 调研佐证）

## 2. What Changes（变更范围）

### 2.1 框架层（`include/cf/plugin/` + `tools/cpu_sim/` + `ip/cpu/`）

| 改动 | 位置 | 行数估计 |
|------|------|----------|
| `PicolibcHostMemory` 构造函数参数化 `(base_addr, size)` + 成员 `base_addr_/size_/tohost_addr_` | `ip/cpu/picolibc_host_memory.h` | +20 |
| `write_byte/word/half` bounds check 改为 `if (off < size_)` 算术；`write_word` partial-byte bug 修复；新增 `write_half` | `ip/cpu/picolibc_host_memory.h` | +25 |
| `DBusPlugin` STORE 路径按 funct3（LB/LH/LW/SB/SH/SW）分发 `write_byte/write_half/write_word` | `ip/cpu/plugins/dbus.h` | +20 |
| `cf::tools::load_elf_text` 升级：所有 PROGBITS SHF_ALLOC 加载，返回 `entry_addr` + `tohost_addr`（通过 shstrtab 解析 .tohost section） | `tools/cpu_sim/elf_loader.h` | +50 |
| `cpu_sim --elf` 主循环：load 后将 `entry_addr` 写入 fetch 节点 PC | `tools/cpu_sim/main.cpp` | +5 |
| `cpu_sim` 新增 `--base-addr` flag（默认 0x0 保持 add.elf） | `tools/cpu_sim/main.cpp` | +3 |
| 文档化 `PicolibcHostMemory` 使用契约（base_addr 唯一性、tohost_addr 单调性） | header comments | +30 |

### 2.2 ELF 资源（`tests/cpu/riscv_tests/`）

| 内容 | 位置 |
|------|------|
| 41 个 prebuilt `rv32ui-p-*.elf`（~270KB） | `tests/cpu/riscv_tests/elf/rv32ui-p-*` |
| `README.md`：构建脚本、commit hash、容器镜像 digest | `tests/cpu/riscv_tests/README.md` |
| `.gitignore` / LFS 配置（如需） | 视大小决定（270KB << 5MB LFS 阈值，plain git 即可） |

**Source 排除清单**：fence_i（需 Zifencei 扩展，feature stub）+ ma_data（misaligned 行为平台未定义）= **41 个 ELF**

**Include**：ld_st / st_ld（RV32-valid load/store hazard 测试，直接验证 HazardPlugin scoreboard，Phase 1.5 §11.1 风险探针）

### 2.3 测试层（`tests/cpu/` + `tests/framework/`）

| 新增/修改 | 内容 |
|-----------|------|
| `tests/cpu/integration/test_rv32ui_runner.cpp` | Catch2 fixture；macro 循环 41 个 TEST_CASE；JUnit reporter 输出至 `build/rv32ui-baseline-junit.xml` |
| `tests/cpu/riscv_tests/CMakeLists.txt` | 引用 ELF 路径（相对 `${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf/`） |
| Triage 输出 | `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`（列：elf, status, fail_stage, category=[真 bug\|feature stub\|toolchain]） |

**5 pre-existing RISC-V failures 分类输出**（`test_3stage_riscv` × 4 + `test_cpu_sim_real_tohost`）— 通过本 change 的 ELF loader 升级 + 工具链无关的 vendored ELF，预计全部转 green，作为 Wave 1 附产品

### 2.4 文档（ADR + CHANGELOG + STATUS + roadmap sync）

| 文档 | 改动 |
|------|------|
| `openspec/specs/cpu-real-fetch-and-memory/spec.md` | **delta**：新增 base-address window + multi-section loader + e_entry 写入 PC requirement |
| `CHANGELOG.md` | 新增 `## v0.2.0 (2026-XX-XX) - riscv-tests-rv32ui` |
| `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` | §2 Wave 1 估时 1 周 → 1.5 周；§2 添加 decision points A/B/C/e_entry/loader/multi-section |
| `ip/cpu/STATUS.md` | 新增"RV32I 合规模性已建立"段落 |

## 3. Scope Boundaries（不做什么）

- ❌ **不实现 ecall/HTIF**（riscv-tests rv32ui-p env `p` 不使用 ecall；`RVTEST_PASS` 是 `sw TESTNUM, tohost, t5; j write_tohost` + 死循环，由 `cpu_sim` 既有 `mem.exited()` 轮询处理）
- ❌ **不修复 riscv-tests 真 bug**（Wave 1 职责是暴露，不修复。fix 归 Wave 2 `cpu-pipeline-fix-rv32ui-N`）
- ❌ **不改 `pipe_builder.h` stall loop**（`plugin-framework-stall` v0.1.3 已就绪）
- ❌ **不改 `int_alu.h` execute 范围**（ADD..AND 之外的 SYSTEM/FENCE 暂保持 NOP 行为）
- ❌ **不加 CSR file**（rv32ui-p 不写 CSR，CSR 需求推到 Wave 4 `cpu-pipeline-exception`）
- ❌ **不下载/构建 riscv-tests 源**（vendor prebuilt ELF 即可；规避工具链依赖）
- ❌ **不启用 SoC demo**（Wave 2 范围）
- ❌ **不改 CpuFactory 注册顺序**（ADR-045 canonical ordering MMUPlugin-before-IBusPlugin 保持）

## 4. Acceptance Criteria（验收标准）

### 4.1 测试

- **Catch2 测试数：332 baseline → ≥373 tests**（+41 新增 rv32ui-p TEST_CASE；5 pre-existing RISC-V 测试**不**新增 Catch2 计数，由 fixture subprocess 跑出 CSV 行）
- **CSV 行数：47 行**（1 header + 41 rv32ui-p + 5 pre-existing subprocess）
- **41 rv32ui-p 中至少 35 PASS**（≥85% 满足 Phase 1.5 §8 毕业标准）
- 5/5 稳定连跑（41 rv32ui-p TEST_CASE 在 runner-mechanics 层全 REQUIRE 通过；FAIL 的合规信号仅记录到 CSV）
- 4 architecture gates 全 PASS（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check`）
- JUnit reporter 输出 `build/rv32ui-baseline-junit.xml` 含 41 `<testcase>` 元素（pass/fail/skip 计数正确）
- 41 rv32ui-p TEST_CASE 在 runner-mechanics 层全 PASS（REQUIRE 不被违反）；FAIL 信号走 CSV/JUnit 软断言，不破坏 Catch2 全绿合并

### 4.2 行为

- `PicolibcHostMemory(base=0x80000000, size=64KB, tohost_addr=0x80001000)` 与 base=0 实例共存，add.elf 默认 base=0 仍 PASS
- ELF loader 加载 `riscv-tests` ELF 后，`e_entry=0x80000000` 写入 fetch 节点 PC，cycle 0 即从 base 地址取指
- 41 ELF 中任意一个写入 tohost 后，`mem.exited()` 在下一 cycle 返回 true，loop 退出
- `write_word(tohost, 1)` 触发 PASS，`write_word(tohost, (TESTNUM<<1)|1)` 触发 FAIL（依 check_tohost 既有逻辑）

### 4.3 端到端验证

- 41/41 ELF 跑通：分类至 `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`
- 至少 35/41 (≥85%) PASS 满足 Phase 1.5 毕业门槛
- 5 pre-existing RISC-V failures 全部转为 PASS（loader 升级 + vendored ELF 副效果）

## 5. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| 多 SHF_ALLOC section loader 引入位址重叠风险 | loader 段间冲突显式报 throw（避免沉默覆盖） |
| `tohost_addr` 在 41 ELF 间不恒定（Oracle bg_e6ec3489 验证 link.ld 不强制 0x1000 offset） | 解析 shstrtab 取 `.tohost` section sh_addr，不用硬编码 |
| `cpu_default.json` 默认 `enable_mmu=true`，与 RV32 测试冲突（Explore bg_719a43e4 发现） | 测试 fixture 用 `--enable-mmu=false` 显式覆盖，与 `test_5stage_riscv.cpp:38` 现行做法对齐 |
| 41 ELF 中含 CSR/CSR 依赖（`simple` 可能含 mstatus 操作） | 分类报告 → Wave 2 fix 候选；不阻塞 Wave 1 |
| `write_word` partial-byte bug 修复可能影响现有 add.elf | add.elf 写 `sw x4, 0(x0)` 走 address 0，`write_word(0, x4)` 高字节=0 不影响 |
| e_entry PC 写入时机（caller-side 不在 build_cpu 内） | 在 `cpu_sim main.cpp` 或 fixture helper 的 `build_cpu(cfg, &mem)` 返回后、首次 `pb->run()` 循环前写入 fetch 节点的 `KeyType::PC` Payload Key |
| 4 个 hidden bug（write_word partial-byte, 缺 write_half, 单 PROGBITS loader, RV64 拒绝） | Wave 1 一并修，纳入 capability 1 delta |

## 6. Out of Scope（推迟到后续 change）

- `cpu-pipeline-fix-rv32ui-N` — Wave 1 分类的真 bug 修复
- `soc-cpu-l1-mmu-demo` — Wave 2 SoC demo（含 add.elf + 5 riscv-tests 用例）
- `cpu-pipeline-multi-cycle` — Wave 3 mul/div LATENCY>1 stall
- `cpu-pipeline-exception` — Wave 4 ECALL/scall 真实实装 + mstatus/mcause/mepc/mtvec CSR
- `cpu-pipeline-mispredict` — Wave 4 flush_when 真实消费者
- `cache-dse-sweep` — Wave 3 在稳定 CPU 上的 DSE

## 7. References（引用）

### 7.1 内部文档

- `tools/cpu_sim/main.cpp:117-118,143-154,177-197` — CLI flag + ELF load + run loop + output
- `tools/cpu_sim/elf_loader.h:32-113` — `load_elf_text` 当前实现（single PROGBITS + skip e_entry）
- `ip/cpu/picolibc_host_memory.h:45-67,102-107` — write_byte/word + check_tohost + bounds
- `ip/cpu/plugins/dbus.h:66-67` — STORE 路径需按 funct3 分发访存宽度（Critical #2）
- `ip/cpu/cpu_factory.h:220-226,234-236` — stage ordering + build_cpu signature
- `tests/cpu/integration/test_3stage_riscv.cpp:111-131` — exec_cmd + tohost=1 pattern
- `tests/cpu/manual_elf/add.S` + `link.ld` — base=0 现存惯例
- `tests/README.md:78` — 5 pre-existing failures 工具链根源
- `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` — Phase 1.5 4-wave plan + §6.2 Oracle R2 风险

### 7.2 ADR

- ADR-040 Tier-1 #4 — at_stage 无早返约束（`write_word` 修复必须遵守）
- ADR-040 Tier-1 #5 — `ip/*/tlm/` 无 ch_mem/ch_reg 渗透（本 change 不涉及）
- ADR-045 — CtrlLink 消费契约（`plugin-framework-stall` 已就绪，本 change 不动）

### 7.3 调研记录

- Oracle bg_ed3671ec — 6 决策推荐（base-window + 41 ELF + JUnit + 等）
- Oracle bg_2b2b4ed1 — 5 refinements + CRITICAL e_entry + 单 PROGBITS loader + ecall 移出
- Explore bg_719a43e4 — 代码验证（picolibc_host_memory bounds, int_alu SYSTEM gap, ELF loader single section）
- Librarian bg_a6d10a3e — HTIF @ 0x80001000, env/p vs env/v, QEMU memory window pattern, ADR-040 Tier-1 范围

### 7.4 外部参考

- `https://github.com/riscv-software-src/riscv-tests` — 41 ELF 来源（BSD-3-Clause）
- `https://github.com/riscv/riscv-test-env/blob/master/p/riscv_test.h` — env/p RVTEST_PASS/FAIL 宏
- `https://github.com/riscv/riscv-test-env/blob/master/p/link.ld` — tohost section 链接规则
- `https://github.com/catchorg/Catch2/blob/devel/docs/reporters.md` — JUnit reporter syntax
- QEMU `hw/riscv/spike.c` + `hw/riscv/riscv_htif.c` — memory window pattern 参考
- Spike issue #364 — HTIF 是 Berkeley 约定非 RISC-V spec

## 8. Next Steps

1. ✅ `proposal.md` — 本文件
2. ⏳ `design.md` — 详细设计（API 签名 + 数据流图 + 改动行数清单）
3. ⏳ `specs/cpu-real-fetch-and-memory/spec.md` — delta（新增 3 requirements + 1 修复）
5. ⏳ `specs/riscv-tests-fixture/spec.md` — new（runner + 41 TEST_CASE 契约）
6. ⏳ `specs/rv32ui-p-triage-output/spec.md` — new（CSV 产物契约）
7. ⏳ `tasks.md` — TDD 5 步结构（write fail → verify fail → implement → verify pass → commit）
8. ⏳ Momus R1 → Oracle R2 → apply → archive