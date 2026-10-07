# ChipForge — Agent 简明手册

本项目是 CppTLM + CppHDL 上的 RISC-V 虚拟验证平台。使用声明式 Plugin 范式（D4）构建硬件 IP。

---

## 构建与测试

```bash
# 标准配置 + 构建（首次自动 ExternalProject build CppTLM/CppHDL 到 build/_deps/install/）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 跑全部测试（4 个 ctest entry：TLM + CH_MEM + Verilator smoke + 架构 gate）
ctest --test-dir build --output-on-failure
# 或封装脚本（默认 = TLM only; --all = 全量 ~167s）
bash tools/run_chipforge_tests.sh --all

# 按 family tag 过滤
./build/bin/chipforge_tests "[framework]"       # Plugin 框架 (TLM, ~0.6s)
./build/bin/chipforge_tests "[cache]"            # L1Cache IP
./build/bin/chipforge_tests "[cpu-integration]"  # RISC-V 集成
./build/bin/chipforge_tests "~[mmu]"             # 排除某 family

# CH_MEM family（必加 -DCF_PLUGIN_USE_CH_MEM 才 build 出此 binary）
./build/bin/chipforge_tests_chmem "[chmem]"
./build/bin/chipforge_tests_chmem "[verilator]","[mmu-verilator]"  # Verilator 后端完整 (4 cases)

# Phase 6d.5 E8 standalone Verilator runner（开发者 debug 入口，从仓库根运行，ELF 为相对路径）
./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-add
```

### 已知测试状态

> **数字维护原则 (v0.10.0 起)**：每次发布前由 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段重新核对；文档数字与 ctest 实测一致时再 commit。CHANGELOG §Verification 段数字必须基于 HEAD commit 实测，禁止快照引用。

- **MMU 测试已重新启用**（`tests/CMakeLists.txt`，mmu-tlb-ptw-impl commit 10）：`[mmu]` **53/53 PASS**（131 assertions, v0.10.0 实测），含新增 `[tlb-refill]` 2 个 PTW TLB refill 集成测试（v0.2.3）。**v0.10.0 hotfix 后**：debug-cpu-l1-mmu-demo-paddr-regression archive 修复 MMU Bare shortcut 退化（sv_mode_==Bare || satp_ppn_==0），同时修 [cpu] + [cpu-integration] 5 个回归。**v0.10.4 完整收官**（2026-09-30 `debug-mmu-cache-end-to-end-bridge-regression`，uncommitted）：`ad48fcf` 仅在 `MMUPlugin` 引入 `satp_value_` 字段注释"Bare shortcut 严格判定用"，但 `do_lookup` 实际仍用 `satp_ppn_==0` 误判 —— Sv-mode + satp_ppn=0 也走 identity translation（vaddr 当 paddr），破坏 `tests/cache/test_mmu_cache_integration.cpp` `[cache][MMUCacheIntegration] EndToEndTranslationThroughBridge` (prefill TLB → lookup → hit → paddr=0x80000000)。完整修复：(1) `MMUPlugin::do_lookup` Bare shortcut 改用 `satp_value_` MODE 字段判定（与字段命名一致：Sv32=bit31、Sv39/48=bit[63:60]、Bare=0）；(2) `MMUPlugin` ctor 按 `sv_mode_` 自动编码 `satp_value_` 初值，让桥路径与 `RiscvMMUPlugin` ctor 路径行为一致。**实测验证**：TLM 432/432 case (67026 assertions) PASS, CHMEM 48/48 case PASS, `[mmu]` 53/53 0 不退化 (satp_ppn_ workaround `test_ptw_tlb_refill_integration` 仍生效), `[cpu-l1-mmu-demo]` 6/6 0 不退化, `[cpu-integration]` 81/81 0 不退化。
- **7stage 测试 PASS（已修复）**：`7stage_add_elf_end_to_end` 原 segfault 根因已由 commit `b82af0f`（CpuFactory 7stage superscalar `lane_counters` use-after-free）修复；`[cpu-integration]` 全 81/81 case PASS（含 3 个 `7stage*` Catch2 filter 命中 + 2 个大写 S 命中（`build_7stage_superscalar` / `EnableMMU7StageCommitRetire`），合计 5 个 case 名称含 "7stage"）。**CWD 约束**：`test_7stage_riscv.cpp` 通过 `exec_cmd("./build/src/cf_plugin/cpu_sim ...")` 调用 cpu_sim（相对路径），ctest `WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}`（`tests/CMakeLists.txt:101`）保证 PASS；从 CWD ≠ 仓库根（含 `build/` 子目录）直接执行 binary 会因相对路径解析失败误报 FAIL。务必用 ctest 或从仓库根运行。
- **`[riscv-tests]` 40/48 PASS**（`test_rv32ui_runner.cpp` 40/40 + `test_rv32um_runner.cpp` 0/8, 基线见 `soc/cpu/docs/dse/rv32{ui,um}-baseline-matrix.csv`, v0.10.x 实测）：P0#2 `cpu-pipeline-fix-rv32ui-load-width`（commit `8909165`，v0.6.0 提前归档）补完 DBusPlugin LOAD width extraction（funct3=000/001/010/100/101 LB/LH/LW/LBU/LHU + sign/zero extension + RD_DATA forwarding），原 10 个 LOAD-family 用例（lb/lbu/lh/lhu/lw + 内嵌 load 的 sb/sh/sw/ld_st/st_ld）从 stub FAIL 转为 PASS。**⚠️ rv32um 0/8 FAIL — 见下条**。
- **`[riscv-tests] rv32um 0/8 FAIL**（`test_rv32um_runner.cpp`, v0.10.4 hotfix 后续实测, defer v0.11.0）：8 ELF（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）全部 FAIL（cycles=43-55, exit_code=1, fail_stage=execute, baseline 见 `soc/cpu/docs/dse/rv32um-baseline-matrix.csv`）。**真 root cause**：`ip/cpu/arch/riscv/mul_div_fsm.h:475-500` `advance_fsm` 实装是 ad-hoc busy counter，不是真 radix-2 iterative（注释 "实装真 radix-2" 未实现）。**推迟路径**：`mfc-defer-v0.11.0` change (OpenSpec `mfc-defer-v0.11.0/proposal.md §v0.11.0 推迟 scope`, Oracle 双复审 2026-10-07 采纳 D' defer 决策) + v0.11.0 启动期 `mfc-extract-fsm-h` follow-up（advance_fsm 真 radix-2 iterative, 1-2 周）+ `wave5-bp-btb` 协同（fetch stall framework 扩展 1-2 周；BTB 分支预测 2-4 周，解锁 DMIPS/MHz ≥1.4 硬门禁）。**当前 `[riscv-tests]` 综合状态**：40/48 PASS（rv32ui 40/40 + rv32um 0/8 FAIL），不符合 v0.10.0 final launch 全绿要求，已诚实性记录。
- **`[cpu-l1-mmu-demo]` 6/6 PASS**（`tests/soc/test_cpu_l1_mmu_demo.cpp`，v0.10.2 实测维持 workaround）：CPU+MMU+Memory 结构验证 demo。**v0.10.1 deep-rca (`debug-cpu-l1-mmu-demo-deep-rca`)** 修复: 测试 `cfg.enable_mmu=false` (与 `test_rv32ui_runner.cpp:109` 对齐, 走 Bare translation); root cause `ip/cpu/cpu_factory.h:390` 硬写 `/*satp_value=*/0` 让 `cfg.mmu_mode="sv32"` config inert (MMU plugin 注册但 PTW 从不在-window 0x0 读 PTE, 退化为 Bare mode 但与无 MMU 路径产生语义差)。**v0.10.2 cpu-factory-satp-mapping** (`openspec/changes/archive/2026-09-28-cpu-factory-satp-mapping/`) 落地 infrastructure 修复 `cpu_factory.h:390` (helpers + ctor propagation + plant helper + unit test 9 assertions PASS)。**Part (a) MMU exception handler 已由 `cpu-pipeline-mmufault-handler` v1 archive 落地** (HazardPlugin::clear_mmufault + MmuExceptionHandlerPlugin TLM/CH_MEM)。`add`/`addi`/`auipc`/`jal`/`beq` 全部到 tohost=1 within 10000 cycles via workaround. L1CachePlugin 仅 JSON 声明不实例化（deferred to Wave 3 cache-dse-sweep）。**Follow-up**: 真 sv32 translation 测试 (b) flip `cfg.enable_mmu=true` 在本测试（独立跟踪，依赖 Part (a) v1 archive + 真 MMUPlugin CH_MEM 实装 Phase 6d.6）.
- **`[cpu-pipeline-mmufault-handler]` `1 change` (v0.10.x wave5 P1 owner, IN_PROGRESS)**：CPU pipeline 卸载 MMU exception handler. HazardPlugin::clear_mmufault() API + MmuExceptionHandlerPlugin TLM + cpu_factory 联动 (TLM + CH_MEM, CH_MEM 用 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护). Part (a) of v0.10.1 deep-rca follow-up, 为真 sv32 e2e 翻转铺路. Phase 7-12 (spec + doc + archive) 待 archive 时合并为最终 entry.
- **P0#1 `cpu-pipeline-canonical-ordering-assert` 已落地**（v0.7.0, ADR-048）：`register_early_plugins()` 现在在 IBusPlugin 之前注册 MMUPlugin，并通过 `check_canonical_ordering()` 运行时断言验证。4 个 `[cpu-integration]` canonical_ordering 测试 PASS（实现走运行时断言而非原 proposal D1=A `static_assert`，因 `inline int` 非 constexpr；spec.md / archived proposal §3 仍提 `static_assert`，属 archive drift，已在 §3 注释 + spec 标注）。

### 构建模式

| 场景 | 命令 | 说明 |
|------|------|------|
| 默认（快速） | `cmake -B build && cmake --build build` | CppTLM/CppHDL 已 install 时跳过 build |
| 重 build 依赖 | `-DCHIPFORGE_REBUILD_DEPS=ON` | CppTLM/CppHDL 源码修改后需要 |
| 源码调试 | `-DCHIPFORGE_SOURCE_DEPS=ON` | add_subdirectory 嵌入模式，改 API 即时生效 |
| ASan | `-DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug` | 调试 |
| 完全重置 | `rm -rf build/_deps` | 从零开始重建 deps |

---

## 架构核心约束（D4 / ADR-040 v2.0）

**所有业务 Plugin 必须遵守**（CI 强制执行，`tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` 9/9 PASS）：

1. **无 `void tick()`** — `PluginBase::tick() = delete`，bridge 适配层例外（`src/cf_plugin/bridge/`）
2. **无状态机** — 禁止 `enum class State` + `switch(state_)`；**多周期协议引擎**豁免（ADR-046：`CF_PLUGIN_USE_FSM_EXEMPT` + `chlib::ch_state_machine` DSL）
3. **Bundle 字段用 `cf::plugin::uint_t<N>`** — **TLM 模式**是 POD；**CH_MEM 模式**是 `ch::core::ch_uint<N>`（ADR-040 v2.0 翻转）
4. **`at_stage` 回调内无 `if (cond) return;` 早返** — 用 `if (cond) { ... }` 包裹主逻辑
5. **TLM 文件（`<name>.h`）不含 `ch_mem`/`ch_reg`/`ch_uint`/`ch::core`**；**CH_MEM 文件（`<name>_chmem.h`）必须含 `ch_*`**（`#ifdef CF_PLUGIN_USE_CH_MEM`）— `check_plugin_portability.sh` Check 2/7 强制
6. **`Plugin::build()` 内不调 `pb.run()`** — TLM 模式 `pb.run()` 每周期仿真；**CH_MEM 模式 `pb.elaborate(ctx)` 一次性发射 lnode DAG**（v0.3.0+ 是新正道）
7. **存储优先用 `cf::plugin::storage::array_store`** — TLM 单缓冲 / CH_MEM 双缓冲 commit swap（ADR-040 Tier-2 推荐）
8. **at_stage 回调内禁运行期 `if(ch_bool)`** — `ch_bool::explicit operator bool()` + C++17 contextual conversion **编译期通过**但**静默取 false**，靠 CI grep 兜底（`check_plugin_portability.sh` Check 5）
9. **PayloadStore fail-fast**: `const T& get(key)` 在 cell 缺失时**抛异常**（v0.3.1 M6 修复），写路径仍 emplace-on-miss（`check_plugin_portability.sh` Check 8）

### 编译开关

| 模式 | 编译选项 | `uint_t<N>` 实际类型 | 适用场景 |
|------|----------|----------------------|----------|
| **TLM**（默认, 兼容） | 不加 flag | POD (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`) | Phase 0-1 业务代码、单元测试、TLM baseline |
| **CH_MEM**（v0.3.0+ 新正道） | `-DCF_PLUGIN_USE_CH_MEM` | `ch::core::ch_uint<N>` | Phase 6c+ 新 Plugin、elaboration 验证、Verilog 生成 |

业务代码**必须双文件分离**:
- `ip/<area>/<name>/<name>.h` — TLM 模式（总是编译）
- `ip/<area>/<name>/<name>_chmem.h` — CH_MEM 模式（仅 `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹）

详细纪律见 [`docs/lessons/phase-6c-elaboration-substrate.md`](docs/lessons/phase-6c-elaboration-substrate.md)（15 类陷阱 + 7 个模式 + 8 项检查清单）。

---

## 代码组织

### 项目骨架

| 目录 | 内容 | 状态 |
|------|------|------|
| `ip/{cpu,cache,mmu,memory,...}/` | 硬件 IP，每个独立，lib/ + tlm/ 双层切分 | 不同 IP 不同阶段 |
| `include/cf/plugin/` | Plugin 框架头文件（PluginBase/Payload/PipeNode/PipeBuilder/CtrlLink） | ✅ Phase 0 完成 |
| `src/cf_plugin/bridge/` | Bridge 适配层（CppTLM ↔ Plugin 桥接） | ✅ L1Cache 完成 |
| `tests/{framework,cache,cpu,mmu,soc,bundles}/` | 测试按 family 分目录，非按源码位置 | |
| `bundles/` | 共享 Bundle 定义（`mem_bundles.h`, `tlb_bundles_extension.h`） | |
| `soc/` | SoC 级 JSON 配置 + 系统架构文档（`soc/cpu/docs/architecture.md` + `soc/cpu/docs/roadmap/`） | |
| `docs/` | 架构文档、ADR、流程图、框架级路线图（Phase 0/6） | |
| `openspec/changes/` | OpenSpec 变更工作区 | |
| `openspec/specs/` | OpenSpec 跨 change 规范 | |
| `.omo/` | 历史决策草案、实施计划 | |
| `build/` | **CppTLM/CppHDL deps 在 `build/_deps/install/` 下** | |

### 测试家族

| Family tag | 目录 | 内容 | 数量 |
|-----------|------|------|------|
| `[framework]` | `tests/framework/` | Plugin 基础组件（TLM 模式） | 89/89 PASS, 275 assertions |
| `[chmem]` | `tests/framework/` + `tests/cpu/` | CH_MEM 模式 elaboration + PoC | 9/9 PASS, 69 assertions |
| `[cpphdl]` | `tests/framework/test_cppHDL_hello_poc.cpp` | CppHDL 完整链路（ch_device/toVerilog/Simulator） | 6/6 PASS, 15 assertions |
| `[elaborate]` | `tests/framework/` | PipeBuilder::elaborate() 4 API PoC | 全部 PASS |
| `[verilator]` | `tests/cpu/test_cpu_verilator_sim.cpp` *(在 `chipforge_tests_chmem`)* | Verilator 后端 5-ELF tohost=1 (Phase 6d.5 E8 子集) | 1/1 case PASS (5 ELF), 10 assertions |
| `[mmu-verilator]` | `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` *(在 `chipforge_tests_chmem`)* | Verilator 链路 MMU Bare 模式 plumbing (Phase 6d P2) | 3/3 case PASS (5 ELF cycle cap + elaboration + manual_elf), 26 assertions |
| `[cache]` | `tests/cache/` | L1CachePlugin + Bridge + Adapter（5 个） |
| `[cpu]` | `tests/cpu/` | CPU Plugin 单元测试 |
| `[cpu-integration]` | `tests/cpu/integration/` | RISC-V 多 stage 集成（4 个） |
| `[soc]` | `tests/soc/` | SoC JSON 拓扑 |
| `[bundles]` | `tests/bundles/` | Bundle 定义测试 |

> **测试 binary 归属（Oracle 2026-10-06 修订）**:
> - `[framework]`/`[cache]`/`[cpu]`/`[cpu-integration]`/`[soc]`/`[bundles]`/`[mmu]`/`[riscv-tests]`/`[cpu-l1-mmu-demo]`/`[tlb-refill]` — 全部在 `chipforge_tests` (TLM binary)
> - `[chmem]`/`[cpphdl]`/`[elaborate]`/`[verilator]`/`[mmu-verilator]` — **全部在 `chipforge_tests_chmem`** (CH_MEM binary, `-DCF_PLUGIN_USE_CH_MEM`)
> - **推荐运行**:
>   - TLM 快速回归：`./build/bin/chipforge_tests` (432 catch2 instances / 67026 assertions, ~0.6s)
>   - 全量 (TLM + CH_MEM + verilator + gate)：`bash tools/run_chipforge_tests.sh --all`
>   - Verilator 后端完整测试 (Phase 6d.5 E8 + 6d P2)：
>     ```bash
>     ./build/bin/chipforge_tests_chmem [verilator],[mmu-verilator]   # 4 Catch2 cases (10+26 assertions)
>     ./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-add  # 1 ELF smoke (ctest entry, 开发者 debug 入口)
>     ```
> - 详见 `tools/run_chipforge_tests.sh --help`

### CH_MEM 编译开关 + 第二个测试 target

```bash
# Phase 6c (v0.3.0+) 引入第二个测试 target
./build/bin/chipforge_tests              # TLM 模式（默认）
./build/bin/chipforge_tests_chmem        # CH_MEM 模式 (-DCF_PLUGIN_USE_CH_MEM)
ctest --test-dir build -R chipforge_tests_chmem --output-on-failure
```

**CH_MEM 模式状态** (2026-09-20 实测):
- ✅ `[cpphdl]` 6/6 PASS (W0 PoC 完整链路)
- ✅ `[chmem]` 9/9 PASS (PayloadStore + 4 elaboration PoC)
- ✅ `[verilator]` 1/1 case PASS (5 ELF tohost=1, verilator --cc 0 error, Phase 6d.5 E8 子集, 完整 6d.5 推迟到 Phase 6d) — **归属 `chipforge_tests_chmem` binary**，TLM binary 无 `[verilator]` 测试 (实测 `--list-tests [verilator]` 在 chipforge_tests 返回 0 case)
- ✅ `pipeline2_stall_matrix` 16/16 PASS (2-stage + stall/flush + Verilog `always_ff @(posedge)` 真实生成)
- ✅ M3 PoC (`m3_poc_regfile_elaborate`/`m3_poc_alu_elaborate`) 13/13 PASS
- ✅ `m4_poc_5stage_simulator_tick` 12/12 PASS (v0.3.1 M6 SEGV 修复后)
- ⚠️ 1 known issue: `cpphdl_poc_chbool_contextual_conversion` 全量跑受 context pollution 影响失败 (单独跑 PASS), Phase 6d PoC follow-up change 跟踪修复
- ⏱ **运行时长**：CH_MEM 全套 (6+9+16+13+12 + 1 known issue) 单二进制 `chipforge_tests_chmem` 实测 >300s（`elaborate(ctx)` DAG 发射 + Verilog `always_ff @(posedge)` 验证耗时）—— CI 设默认 ctest timeout 360s+；本地快速回归可 `--test-case` / `[chmem]` 单 family 过滤跳过 PoC 测试。

**v0.10.x `[verilator]` 扩展 (verilator-cpu-factory-extensible-params)**: CLI flags 扩展 3 项 (`--enable-mmu` / `--mmu-mode <s>` / `--enable-cache`), 不增 `[verilator]` test cases (CLI plumbing 扩展属本 change scope; 端到端 MMU/Cache Verilator 测试归 Change 2a / 2b). zero numerical change to `[verilator] 1/1 case PASS`.

**v0.10.x `[mmu-verilator]` 新增 (verilator-mmu-bare-plumbing-e2e)**: `3/3 PASS` (5 ELF × tohost=1 + cycle ≤ baseline×1.2 + elaboration 0 error + manual_elf full-chain) — **plumbing only — translation semantics NOT verified** — CLI plumbing only; v0.10.4 hotfix TLM 防护由 `[mmu]` family 负责. zero regression to `[verilator] 1/1`, `[mmu] 53/53`, `[cpu-l1-mmu-demo] 6/6`.

### `ip/{name}/` 标准结构

```
ip/{name}/
├── README.md       # IP 总览
├── STATUS.md       # 当前阶段/状态
├── tlm/            # CppTLM Plugin 层（D4 强制）
├── rtl/            # CppHDL RTL 层（Phase 5+）
├── lib/            # 纯 C++ 算法层（与 Plugin 框架解耦）
├── configs/        # JSON 配置 + params_schema.json
├── docs/           # 设计文档
│   ├── README.md   # 文档索引
│   ├── architecture.md
│   ├── configuration.md
│   ├── integration.md
│   └── adr/        # IP 级 ADR
├── policies/       # 替换策略（mmu 特有）
└── test/           # 预留（实际在 tests/{name}/）
```

### lib/ vs tlm/ 严格切分（核心架构规则）

- `lib/` — 纯 C++ 算法，**0 引用** `cf::plugin::PluginBase`/`PipeBuilder`/`Payload`（唯一例外：`cf::plugin/uint_t.h`）
- `tlm/` — Plugin 框架集成，依赖 `cf::plugin::*`，持 `lib/` 算法为成员
- `lib/` → HDL 1:1 转换，`tlm/` → Phase 6 才转换

---

## 文档组织

### ADR 归属判断

> 删除 `ip/{name}/` 后如果该 ADR 仍有意义 → **框架级**（`docs/architecture/adr/`）  
> 删除后变成死链接 → **IP 级**（`ip/{name}/docs/adr/`）

### 关键文档源

| 文件 | 内容 |
|------|------|
| `docs/architecture/adr.md` | ADR 注册表（全局索引，44 条） |
| `docs/DEVELOPMENT_SETUP.md` | 开发环境搭建（symlink + 3 种 CMake 模式） |
| `README.md` | 项目概述 |
| `CONTRIBUTING.md` | 贡献流程、commit 规范、PR SLA |
| `docs/architecture/overview.md` | 架构总览 |
| `docs/architecture/plugin-framework.md` | Plugin 框架详细设计 |
| `docs/methodology/plugin-style-design-methodology-v1.md` | **v2** D4 方法学（TLM + CH_MEM 双模, 含 Phase 6c elaboration chapter §8） |
| `docs/lessons/phase-6c-elaboration-substrate.md` | **Phase 6c 行级教训** (15 类陷阱 + 7 个模式 + 8 项检查) |
| `docs/research/phase6c-elaboration-pattern-study.md` | **Phase 6c 立项前研究** (SpinalHDL/VexRiscv/CppHDL 借鉴, 910 行) |
| `docs/roadmap/phases/phase-6-declarative.md` | Phase 6 总览 (v2.0.3, 6c 收官 + 6d 拆分) |
| `docs/roadmap/phases/phase-6d-rtl-verification.md` | **Phase 6d 独立 phase doc** (5-stage Pipeline CH_MEM + Verilator + MMU FSM) |
| `tests/README.md` | 测试框架详情（Catch2 使用、已知问题） |
| `tools/README.md` | CI 验证脚本详情（verify_adr.sh 等） |

### Phase 6c 新增 ADR 速查

| ADR | 标题 | 关键决策 |
|-----|------|---------|
| **ADR-040 v2.0** | TLM→HDL 移植性约束 | **CH_MEM 是新正道** (翻转 v1.0 ch 渗透禁令); `array_store` 双缓冲; `CtrlLink::halt_when(ch_bool)` |
| **ADR-046** | 多周期协议引擎豁免 D4 无状态机禁令 | `CF_PLUGIN_USE_FSM_EXEMPT` 标记 + `chlib::ch_state_machine` DSL |

**ADR-037 v2.0 状态 (Oracle 2026-09-20 确认)**: `docs/architecture/adr.md:1235` 已标 `✅ v2.0 Accepted (Phase 6c M5 落地, 2026-09-17, D4 elaboration 语义兑现)`。Phase 6d prereqs change 仅验证内容完整性 + 修正 adr.md:1251 拆分描述不一致 (原 6c/6d/6e 与现行 6a/6b/6c/6d), 不重新写 v2.0。

### 路线 / Roadmap 类文档（2026-09-28 统一导航）

> **⚠️ 新会话第一件事**: 项目里**有两个** `execution-roadmap.md`, 不要混淆!
> 详见 [`docs/MIGRATION_LOG.md`](docs/MIGRATION_LOG.md)（文档结构变更历史）。

#### 4 类路线文档职责对照

| 类型 | 文档 | 范围 | 何时读 |
|------|------|------|--------|
| **战略** | [`docs/roadmap/strategy/a-plus-c-hybrid.md`](docs/roadmap/strategy/a-plus-c-hybrid.md) | A+C Hybrid 战略选择（为什么做）| 任何 ADR / PoC 决策时看大方向 |
| **框架级执行** | [`docs/roadmap/strategy/execution-roadmap.md`](docs/roadmap/strategy/execution-roadmap.md) | v0.8.0 / v0.9.0 框架 + 所有 IP（Phase 6d → v0.9.0 架构图见 §6）| 当前冲刺阶段（v0.8.0 启动 / 实施） |
| **SoC 级执行** | [`soc/cpu/docs/roadmap/execution-roadmap.md`](soc/cpu/docs/roadmap/execution-roadmap.md) | v0.10.0 → v1.3.0 CPU SoC（产品化路径，架构图见 §3.5）| SoC 路线规划 / PoC 评估 / 商业化决策 |
| **Phase 独立 doc** | [`docs/roadmap/phases/phase-6d-rtl-verification.md`](docs/roadmap/phases/phase-6d-rtl-verification.md) 等 | 已确定阶段的端到端细节（tasks + 退出标准 + ADR）| 实施具体 phase 前 |

#### 当前活跃 versions + 关联架构图

| 版本 | 时间 | 文档位置 | 架构图 |
|------|------|---------|--------|
| **v0.7.0** | ✅ 2026-09-25 archive | strategy §3 + roadmap-status §2 | — |
| **v0.8.0** | 🟡 2026-12 中旬（Wave 3-mmu） | strategy execution-roadmap §2 + §6.3 | strategy §6.3 |
| **v0.9.0** | 📋 2027-02 下旬（Wave 4-csr-cache-dse） | strategy execution-roadmap §4 + §6.4 | strategy §6.4 |
| **v0.10.0** | 📋 2027 Q1（Wave 5-isa-coverage-and-bp） | soc/cpu execution-roadmap §3 + §3.5.2 | soc/cpu §3.5.2 |
| **v1.0.0** | 📋 2027 Q3（wave6-linux-and-productization） | soc/cpu execution-roadmap §3 + §3.5.3 | soc/cpu §3.5.3 |
| **v1.2.0** | 📋 2028 Q2 | soc/cpu execution-roadmap §3 + §3.5.4 | soc/cpu §3.5.4 |
| **v1.3.0** | 📋 2029 Q1 | soc/cpu execution-roadmap §3 + §3.5.5 | soc/cpu §3.5.5 |

#### 记忆口诀（防止新会话混淆）

- **"战略 execution-roadmap"** → 看 [`docs/roadmap/strategy/execution-roadmap.md`](docs/roadmap/strategy/execution-roadmap.md)
- **"SoC execution-roadmap"** → 看 [`soc/cpu/docs/roadmap/execution-roadmap.md`](soc/cpu/docs/roadmap/execution-roadmap.md)
- 两者通过 §6.6 / §3.5.7 **互相衔接**（2026-09-28 新增）
- 详细职责对照见 [`soc/cpu/docs/roadmap/README.md §与框架级 execution-roadmap 的区别`](soc/cpu/docs/roadmap/README.md)

#### Phase 文档拆分粒度判断标准

> 避免预测式文档腐烂。

| 状态 | 处理 |
|------|------|
| 已有 OpenSpec change + tasks.md + ADR 锚点 | **独立** phase doc（如 `phase-6d-rtl-verification.md`） |
| 仅"目标" + "PoC 列表"，未启动实施 | **留在** execution-roadmap.md 里（如 v1.0.0/v1.2.0/v1.3.0） |
| 历史已 archive | 移到 `soc/cpu/docs/roadmap/archive/` 或 `docs/roadmap/phases/` 标注历史 |

---

## 工作流

### OpenSpec 工作流（推荐用于多步变更）

```bash
# 探索 → 提案 → 实施 → 归档
# 技能: openspec-{explore|propose|apply|archive}
skill(name="openspec-explore")
skill(name="openspec-propose")
skill(name="openspec-apply-change")
skill(name="openspec-archive-change")
```

### Strategy / Initiative 工作流（2026-09-24 新增, A+C Hybrid 战略落地）

战略入口: [`docs/roadmap/strategy/a-plus-c-hybrid.md`](docs/roadmap/strategy/a-plus-c-hybrid.md)

```bash
# 查看当前 initiative 状态 (auto-derived from openspec changes)
bash tools/sync_strategy_status.sh --dry-run          # 仅打印 diff
bash tools/sync_strategy_status.sh                      # atomic 写入 strategy §7
bash tools/sync_strategy_status.sh --json               # 输出 JSON (供其他脚本消费)

# 查看 initiative 内容 (XDG managed)
openspec initiative list --store chipforge
openspec initiative show wave3-cpu-pipeline-debt --store chipforge

# Active changes → 提案 → 实施 → 归档
openspec list                                          # 列 active changes
openspec change validate <change-name>                 # 验证 change artifacts
openspec change show <change-name>                     # 查看 change 内容
openspec archive <change-name> -y                      # 归档到 openspec/changes/archive/
```

**当前活跃 initiatives** (2026-09-24, version 节点对齐 `docs/roadmap/strategy/a-plus-c-hybrid.md` §3/§6 — Phase 6d 已消费 v0.4.0/v0.5.0/v0.6.0):

| Initiative | Version | 状态 | Changes |
|---|---|---|---|
| `wave3-cpu-pipeline-debt` | **v0.7.0** | **已收官（archive）** | 2 changes (P0#1 canonical-ordering + P0#2 load-width 回顾性) |
| `wave3-mmu-real-memory-and-cycle` | **v0.8.0** | 进行中 | 3 changes (P1#3 mmu-paddr-consume + P1#4 cycle-precision + P1#5 multi-cycle) |
| `wave4-csr-cache-dse` | **v0.9.0 占位** | 未启动 | 2 changes (P2#6 phase-1.5-wave-4 + P2#7 cache-phase1.5-4way) |

**Change proposal 关联**: 每个 change 的 `proposal.md` 必须含 frontmatter `initiative:` 字段, sync_strategy_status.sh 据此派生 §7。

### 验证命令

```bash
bash tools/verify_adr.sh                           # ADR 漂移检查
bash tools/verify_plugin_decision.sh                # D4 业务代码检查（~7 项）
bash tools/check_plugin_portability.sh              # ADR-040 移植性检查（~4 项）
bash tools/doc_link_check.sh                        # 文档死链检查
bash tools/run_chipforge_tests.sh                   # 默认: TLM only (432 catch2 instances / 67026 assertions, ~0.6s, fast loop)
bash tools/run_chipforge_tests.sh --all             # 全量: ctest 4 entries (TLM + CH_MEM + Verilator smoke + gate, ~167s)
python tools/doc_checker.py --format text --verbose  # 文档健康检查
pre-commit run --all-files                          # 格式化/空白/JSON 检查
```

3 个验证脚本（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability`）作为 **PR 阻塞门禁**（`.github/workflows/architecture-gates.yml`，ADR-043）。

---

## CI 细节

| Workflow | 触发 | 阻塞？ |
|----------|------|--------|
| `architecture-gates.yml` | PR to main/develop | ✅ 3 脚本全阻塞 |
| `doc_check.yml` | PR + push（docs/ip/变更时） | ⚠️ smoke-only (实测全 PASS, 仅 ADR-024 强阻塞, 余 smoke) |

CI 会自动 checkout CppTLM/CppHDL 仓库（`${{ vars.CPPTLM_REPO || 'chisuhua/CppTLM' }}`）。

> **测试脚本不在 CI 自动跑**（Oracle 2026-10-06 决策）：当前 CI workflow 仅运行架构 gate 脚本（`verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh`）。`tools/run_chipforge_tests.sh --all` 与 `bash tools/run_chipforge_tests.sh` 需 PR 提交者**本地**手动跑并贴结果。原因：CH_MEM binary 已知有 `cpphdl_poc_chbool_contextual_conversion` context-pollution flake（单跑 PASS，全套件场景偶发 FAIL），若加入 PR 阻塞会引入间歇性 CI 失败。Verilator 完整测试 (`run_chipforge_tests.sh --all` 含 `cpu_verilator_sim_add` smoke + `chipforge_tests_chmem [verilator],[mmu-verilator]`) 由 PR 模板强制要求 + 开发者本地执行覆盖。

---

## 不显而易见的约定

- **`ip/cpu/` 是最老的 IP**，当前不完全遵守标准文档结构（无 `ip/cpu/docs/` 等）；新增 IP 必须对齐
- **`src/cf_plugin/tests/` 已删除**（CHANGE-005 empty-directory-cleanup），测试在 `tests/` 下
- **`ip/README.md` 中 mmu 行已存在两次**（STATUS 表 + IP 模块表），不再重复添加
- **`ip/mmu/STATUS.md`** 声明"TLB/PTW 算法 stub"，不可误用为生产
- **.cursorrules 与 AGENTS.md** 的 MCP 部分内容重复，维护时同步更新
- CppTLM/CppHDL 是 **独立仓库**（symlink 引入），git 操作需分别在各目录中执行
- 测试框架使用 **Catch2 v3.7.0**（vendored 在 `tests/catch2/`，与 CppTLM 同步）
- **`tests/CMakeLists.txt`** 用 `file(GLOB_RECURSE)` 自动发现测试文件，新增测试不需改 CMake
- C++17 项目，但 `chipforge_tests` target 需 C++20（因为链接了 CppHDL 头文件）
- **Phase 6c CH_MEM 模式约定**:
  - 业务代码必须**双文件分离**（`<name>.h` TLM + `<name>_chmem.h` CH_MEM），**禁止同一文件混合**
  - **`uint_t<N>` 在两种模式下是不同类型**（TLM=POD, CH_MEM=`ch_uint<N>`），业务代码不感知但 CI 静态检查会抓
  - **`array_store<T,N>::commit()` 在 TLM 模式是 no-op**（业务代码透明切换，无需 if/else）
  - **ch_reg / ch_mem 必须用 `ch::core::ch_literal<0, N>{}` 真实字面值初始化**（v0.3.1 M6 上游修复前 `ch_uint<N>(0)` 会产生 null impl → SEGV）
  - **at_stage 闭包内禁用运行期 `if(ch_bool)`**（C++17 contextual conversion 静默求值，靠 CI grep 兜底）
  - **PoC 测试必须显式管理 `ch::core::context` lifecycle**（`set_as_current_context()` 在每个 SECTION 入口，避免 thread_local 污染）

---

## Phase 6d 启动前必知 (2026-09-20 新增, Oracle/Metis 审查修正)

> Phase 6c 已完成 (`plugin-elaboration-substrate` 归档, v0.3.0 + v0.3.1 发布)。Phase 6d 待启动, 见 [`docs/roadmap/phases/phase-6d-rtl-verification.md`](docs/roadmap/phases/phase-6d-rtl-verification.md)。

**Phase 6d 范围**: 5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM (11-13 周, Oracle 估时上调 0.5 周因 6d.1 从零建)

**Phase 6d 启动前置 (OpenSpec `phase-6d-prerequisites`, 6 项 Oracle/Metis 审查修订后)**:
1. ⏸ **riscv64-unknown-elf-gcc 工具链 CI 集成** (Metis 发现: 当前 build env 已预装 GNU 16.1.0, 阻塞项 = CI 集成)
2. ⏸ **Verilator ≥ 5.020 + Yosys + iverilog** (Metis 发现: 当前 build env Verilator 5.052 已预装; jammy 22.04 verilator 4.038 < 5.020 需源码 build; yosys/iverilog 缺失)
3. ⏸ **ADR-037 v2.0 验证** (Oracle 发现: adr.md:1235 已标 v2.0 Accepted; 阻塞项 = 验证内容完整 + 修正 adr.md:1251 拆分描述)
4. ⏸ **PoC follow-up fixes** (`poc-follow-up-fixes` change, Metis 修正 Fix #2 用方案 B `TEST_CASE_METHOD` fixture)
5. ⏸ **M3 HazardPlugin 完整 CH_MEM 版** (在 PoC follow-up 内)
6. ⏸ **测试 ELF 获取** (Oracle 新增: `tests/cpu/manual_elf/` 仅含 add, 缺 addi/auipc/jal/beq 4 条, 阻塞 6d.4)

**Phase 6d 关键交付**:
- 6d.1: DecoderPlugin 完整 CH_MEM (新建 `decode_chmem.h`, Oracle 修正: 原"重启用 disabled"措辞错误, 该文件不存在; 估时 1.5 周)
- 6d.2: BranchPlugin + HazardPlugin 完整 CH_MEM
- 6d.3: CpuFactoryChmem 5-stage 集成 **+ Oracle 关键新增: CH_MEM fetch/memory model** (`ibus_chmem.h` + `dmem_chmem.h` 缺失, 估时 2 周)
- 6d.4: riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` 端到端 CppHDL sim PASS (**接受路径放宽**: 实际 ~8-9 指令路径, 含 lui/sw/bne)
- 6d.5: Verilator 集成 (Verilog → VL1Cache/VRegFile 替代 C++ sim; **E8 降级**: tohost=1 一致, 不强求 trace byte-equal)
- 6d.6: MMU/PTW FSM (**sv32 5 状态** `IDLE/L0_WAIT/L1_WAIT/DONE/FAULT`, Oracle 修正: 不是 sv39 3-level)
- 6d.7: L1Cache refill FSM (同 ch_state_machine)
- 6d.8: Harness 迁移 (`pb.run()` → CppHDL sim runner / Verilator; 显式排除 7stage superscalar config)

**Phase 6d 启动依赖图 (Oracle 2026-09-20 修订)**:
```
build env (GNU 16.1.0 + Verilator 5.052 已预装, yosys/iverilog optional)
  ↓
poc-follow-up-fixes ──┐  并行启动 (Oracle 2026-09-20 修订, 不串行)
phase-6d-prerequisites ─┘
       ↓
phase-6d-rtl-verification (硬门 = poc-follow-up-fixes archive)
```

**6d main 硬前置 (修订后)**:
1. `openspec/changes/poc-follow-up-fixes/` 已 archive (含 M3 HazardPlugin 完整 + byte-equal + chbool)
2. `openspec/changes/phase-6d-prerequisites/` 已 archive (CI 集成 + ADR-037 v2.0 验证)
3. `tests/cpu/manual_elf` + `tests/cpu/riscv_tests/elf/` 测试 ELF 就绪 (已 vendor 40 ELF, 无需重写)
