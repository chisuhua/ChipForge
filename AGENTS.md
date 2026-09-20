# ChipForge — Agent 简明手册

本项目是 CppTLM + CppHDL 上的 RISC-V 虚拟验证平台。使用声明式 Plugin 范式（D4）构建硬件 IP。

---

## 构建与测试

```bash
# 标准配置 + 构建（首次自动 ExternalProject build CppTLM/CppHDL 到 build/_deps/install/）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 跑全部测试（单二进制 chipforge_tests）
ctest --test-dir build --output-on-failure

# 按 family tag 过滤
./build/bin/chipforge_tests "[framework]"       # Plugin 框架
./build/bin/chipforge_tests "[cache]"            # L1Cache IP
./build/bin/chipforge_tests "[cpu-integration]"  # RISC-V 集成
./build/bin/chipforge_tests "~[mmu]"             # 排除某 family
```

### 已知测试状态

- **MMU 测试已重新启用**（`tests/CMakeLists.txt`，mmu-tlb-ptw-impl commit 10）：47 个 `[mmu]` 测试全部 PASS（110 assertions），含新增 `[tlb-refill]` 2 个 PTW TLB refill 集成测试（v0.2.3）。
- **`7stage_add_elf_end_to_end` 预先存在失败**（`tests/cpu/integration/test_7stage_riscv.cpp`）：superscalar 路径 cpu_sim segfault（`--config cpu_superscalar.json`），与代码逻辑无关的既有问题（v0.2.2 stash 验证非本次修复引入）。其余 4 个 RISC-V 仿真测试（3/5/10-stage + `test_cpu_sim_real_tohost`）已随 v0.2.2 CPU pipeline 修复转绿。
- **`[riscv-tests]` 10 个 LOAD-family 用例失败**（`test_rv32ui_runner.cpp`，category=feature stub）：DBusPlugin LOAD width extraction（LB/LH/LBU/LHU）显式 OUT OF SCOPE，follow-up change `riscv-tests-rv32ui-load-width` 路由；`[riscv-tests]` 其余 30 个 PASS（基线 `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`）。
- **`[cpu-l1-mmu-demo]` 6 个用例 PASS**（`tests/soc/test_cpu_l1_mmu_demo.cpp`，v0.2.3）：CPU+MMU+Memory 结构验证 demo（enable_mmu=true, sv32），5 个 riscv-tests（add/addi/auipc/jal/beq）端到端 tohost=1；L1CachePlugin 仅 JSON 声明不实例化（deferred to Wave 3 cache-dse-sweep）。

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

**所有业务 Plugin 必须遵守**（CI 强制执行，`tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` 8/8 PASS）：

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
| `[cache]` | `tests/cache/` | L1CachePlugin + Bridge + Adapter（5 个） |
| `[cpu]` | `tests/cpu/` | CPU Plugin 单元测试 |
| `[cpu-integration]` | `tests/cpu/integration/` | RISC-V 多 stage 集成（4 个） |
| `[soc]` | `tests/soc/` | SoC JSON 拓扑 |
| `[bundles]` | `tests/bundles/` | Bundle 定义测试 |

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
- ✅ `pipeline2_stall_matrix` 16/16 PASS (2-stage + stall/flush + Verilog `always_ff @(posedge)` 真实生成)
- ✅ M3 PoC (`m3_poc_regfile_elaborate`/`m3_poc_alu_elaborate`) 13/13 PASS
- ✅ `m4_poc_5stage_simulator_tick` 12/12 PASS (v0.3.1 M6 SEGV 修复后)
- ⚠️ 1 known issue: `cpphdl_poc_chbool_contextual_conversion` 全量跑受 context pollution 影响失败 (单独跑 PASS), Phase 6d PoC follow-up change 跟踪修复

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

**ADR-037 v2.0 状态 (Oracle 2026-09-20 发现)**: `docs/architecture/adr.md:1235` 已标 `✅ v2.0 Accepted (Phase 6c M5 落地, 2026-09-17, D4 elaboration 语义兑现)`。Phase 6d prereqs change 仅验证内容完整性 + 修正 adr.md:1251 拆分描述不一致 (原 6c/6d/6e 与现行 6a/6b/6c/6d), 不重新写 v2.0。

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

### 验证命令

```bash
bash tools/verify_adr.sh                           # ADR 漂移检查
bash tools/verify_plugin_decision.sh                # D4 业务代码检查（~7 项）
bash tools/check_plugin_portability.sh              # ADR-040 移植性检查（~4 项）
bash tools/doc_link_check.sh                        # 文档死链检查
bash tools/run_chipforge_tests.sh                   # 完整 ctest 运行
python tools/doc_checker.py --format text --verbose  # 文档健康检查
pre-commit run --all-files                          # 格式化/空白/JSON 检查
```

3 个验证脚本（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability`）作为 **PR 阻塞门禁**（`.github/workflows/architecture-gates.yml`，ADR-043）。

---

## CI 细节

| Workflow | 触发 | 阻塞？ |
|----------|------|--------|
| `architecture-gates.yml` | PR to main/develop | ✅ 3 脚本全阻塞 |
| `doc_check.yml` | PR + push（docs/ip/变更时） | ❌ smoke-only |

CI 会自动 checkout CppTLM/CppHDL 仓库（`${{ vars.CPPTLM_REPO || 'chisuhua/CppTLM' }}`）。

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
- 6d.1: DecoderPlugin 完整 CH_MEM (新建 `decode_chmem.h`, Oracle 修正: 原"重启用 disabled"措辞错误, 该文件不存在)
- 6d.2: BranchPlugin + HazardPlugin 完整 CH_MEM
- 6d.3: CpuFactoryChmem 5-stage 集成
- 6d.4: riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` 端到端 CppHDL sim PASS
- 6d.5: Verilator 集成 (Verilog → VL1Cache/VRegFile 替代 C++ sim)
- 6d.6: MMU/PTW FSM (Oracle 修正: sv32 对齐现有 MMUPlugin, 不是 sv39)
- 6d.7: L1Cache refill FSM (同 ch_state_machine)
- 6d.8: Harness 迁移 (`pb.run()` → CppHDL sim runner / Verilator)
