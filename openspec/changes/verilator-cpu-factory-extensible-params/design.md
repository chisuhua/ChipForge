## Context

### 现状（2026-10-04 v0.10.4 hotfix 后）

`ip/cpu/cpu_factory_chmem.h:74` 的 `CpuFactoryChmem::build_cpu` 是 7-plugin 5-stage 流水线工厂（IBus+Decoder+RegFile+Hazard+IntAlu+Branch+DMem），**无 MMU/L1Cache 注册**。`tools/verilator_runner/cpu_verilator_sim.cpp:129` 硬编码调 `build_cpu(&ctx, nullptr, kElfBase, true, elf)`,**唯一** CH_MEM → Verilog → Verilator 仿真路径。

### 7-plugin 工厂全 list（不带 cache_row_t/列宽 for plumbing）

```cpp
// ip/cpu/cpu_factory_chmem.h:74
static std::unique_ptr<PipeBuilder> build_cpu(
    ch::core::context* elaboration_ctx,
    T* memory = nullptr,
    T initial_pc = T{0x80000000},
    bool preload_elf = false,
    const std::vector<uint8_t>& elf_image = {})
```

6 文件 / 12 call sites（grep `CpuFactoryChmem.*build_cpu` 实测，修复 S1 Oracle 三轮复审）:
1. `tools/verilator_runner/cpu_verilator_sim.cpp:129` (1 call, 4 参数硬编码)
2. `tests/cpu/test_cpu_chmem_vendored_elf.cpp:94` (1 call, 4 参数)
3. `tests/cpu/test_cpu_5stage.cpp:53,75,122,152,193` (5 calls, 4 参数)
4. `tests/cpu/test_cpu_decoded_inst_migration.cpp:322` (1 call, 4 参数)
5. `tests/cpu/test_cpu_memory_model_chmem.cpp:127,177,395` (3 calls, 4 参数)
6. `tests/cpu/test_cpu_rtl_regfile_alu.cpp:541` (1 call, 4 参数)

### `RiscvMMUPlugin` 当前可用性（v0.10.4 hotfix 收官）

- 位置: `ip/cpu/plugins/mmu.cpp` + `ip/mmu/tlm/MMUPlugin.{h,cpp}`
- TLM 模式: `MMUPlugin::do_lookup` Bare shortcut 用 `satp_value_` MODE 字段判定（v0.10.4 修复点,v0.10.0 hotfix 前用 `satp_ppn_` 误判）
- CH_MEM 模式: **未实装 `mmu_chmem.h`**——`ip/mmu/tlm/` 仅有 `MMUPlugin.cpp/.h` + `mmu_keys.h`,无任何 `_chmem.h`（修复 C-C Oracle 复审: 真 sv32 CH_MEM 实装在 4 个 active change 中无人认领 — Change 2a 显式 plumbing-only 不实装, `cpu-pipeline-mmufault-handler` 只做 exception handler 也不实装, **本 change hook 的 log 错误信息** 指向 Change 2a 是死路, 建议新加 follow-up change `mmu-chmem-impl` 或在 `cpu-pipeline-mmufault-handler` 扩展 scope 显式实装）
- 与 CPU pipeline 集成: TLM 模式 `cpu_factory.h:390` 硬写 `/*satp_value=*/0` 让 `cfg.mmu_mode="sv32"` config inert,Change 2a 后续需修复 cpu_factory.h 才真翻转 sv32 e2e

### `L1CachePlugin` 当前可用性

- 位置: `ip/cache/tlm/L1CachePlugin.{h,cpp}` + `l1_cache_refill_fsm_chmem.h` (独立 refill FSM PoC, 6d.7 已实装)
- TLM 模式: 完整实装,256 sets × 1 way direct-mapped
- CH_MEM 模式: **仅独立 refill FSM**（`l1_cache_refill_fsm_chmem.h` 6d.7,229 行）,**无完整 `l1_cache_chmem.h`**（这是 Change 2b 的硬阻塞,本 change 仅留 hook）

### `cpu_verilator_sim` 当前 CLI

```
--elf: set ELF path (required)
--verilog: output Verilog path (default /tmp/cpu.v)
--work-dir: dir for verilator artifacts (default /tmp/cpu_vl_sim)
--cycles: max sim cycles (default 2000)
--help/-h: usage
```

5 个参数。无 MMU/Cache 开关。

### 相关 ADR + Spec 现状

- **ADR-040 v2.0** (✅): 双模约束 + 9 项 CI 检查 (`check_plugin_portability.sh`)
- **ADR-046 v2.0** (✅): FSM 豁免 + `chlib::ch_state_machine` DSL
- **ADR-082** (🚧 Drafting): `Plugin::negotiate()` capability 协商
- **ADR-037 v2.0** (✅): D14 Verilator sim cycle-identical 闭环条件（pending）
- **OpenSpec specs**: `cpu-factory-chmem-pre-population` (TBD)、`cpu-factory-satp-mapping` (TBD)——本 change 覆盖 factory API 但不动现有 spec 语义

### Stakeholders

- **下游 Change 2a** (`verilator-mmu-bare-plumbing-e2e`, wave5 P2): 直接消费 `--enable-mmu` + `enable_mmu=true`
- **下游 Change 2b** (`verilator-l1cache-e2e-coverage`, wave4 P2): 直接消费 `--enable-cache` + `enable_cache=true`（需 L1Cache CH_MEM 完整实装）
- **mfc-cpu-pipeline-multi-cycle-fsm** Phase G: 间接消费 Verilator runner 能力（但 Phase G 自身不显式 require 本 change）
- **3 架构门禁**: `verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` 必须保持 0 失败

## Goals / Non-Goals

### Goals

1. **G1**: `CpuFactoryChmem::build_cpu` API 向后兼容扩展（4 个现有调用方零修改）
2. **G2**: `cpu_verilator_sim` CLI 新增 3 个 flag (`--enable-mmu`/`--mmu-mode`/`--enable-cache`)
3. **G3**: `enable_mmu=true` 路径 **emit TODO log only**（CH_MEM 模式 no-op 行为）— 修复 C-A Oracle 复审: `RiscvMMUPlugin` 是 TLM-only class, CH_MEM 路径**不**引用它; Verilog 输出**不**含 MMU 电路
4. **G4**: `enable_cache=true` 路径**留 hook + 显式 fail-fast 提示**（"L1Cache CH_MEM not implemented, please refer to change verilator-l1cache-e2e-coverage"）
5. **G5**: 提供 MMU/Cache-aware 汇编模板 vendor 脚本入口（仅脚本,不实装测试用例）
6. **G6**: 3 架构门禁 0 失败
7. **G7**: AGENTS.md + CHANGELOG §honesty_audit 同步（不修改任何实测数字）

### Non-Goals

1. **NG1**: 不实装 `mmu_chmem.h` / `l1_cache_chmem.h`（属下游 Change 2a/2b scope）
2. **NG2**: 不新增 `[verilator]` 测试 cases（属 Change 2a/2b scope）
3. **NG3**: 不实装真 sv32 translation e2e（前置依赖 `cpu-pipeline-mmufault-handler`, 属 wave5 placeholder scope）
4. **NG4**: 不修改 `cpu_factory.h:390` 的 `/*satp_value=*/0` 硬编码（属 Change 2a scope）
5. **NG5**: 不升级 CI 门禁（保留 `skip-when-absent`, 不升级 `fail-when-absent`——Ubuntu 22.04 需源码 build Verilator ≥5.020 是独立大工程）
6. **NG6**: 不实装 ADR-082 negotiate capability（v2 新接口, Drafting 状态, 本 change 仅留 hook 给 MMU/L1Cache plugin 自决）
7. **NG7**: 不扩展 7-stage superscalar 路径（沿用 Phase 6d.8 显式排除 7stage config 模式）

## Decisions

### D1: 三态 `std::optional<bool>` 优于单 `bool` + default

**Decision**: `enable_mmu` / `enable_cache` 用 `std::optional<bool>` 而非裸 `bool`。

**Rationale**:
- 三态语义清晰: `nullopt` = 原 4 参数行为（保持现状 7-plugin 默认, no-op）, `true` = 显式启用, `false` = 显式禁用（修复 C-A: **不**读 cfg, `build_cpu` 签名无 cfg 参数）
- **6 文件 / 12 call sites** 零修改（修复 S1 Oracle 复审数字: grep `tests/cpu tools/verilator_runner` 实测）
- 未来 `CpuConfig` 加新字段时不必改 `build_cpu` 签名

**Alternatives considered**:
- (A) 裸 `bool enable_mmu = false`: 语义不清 — `false` 是"显式禁"还是"未指定"?
- (B) 引入新 enum `class MmuState { Off, OnBare, OnSv32 }`: 过度设计, 4 个状态够用, 6 个状态会让 API 难看
- (C) 引入新 struct `CpuFactoryOptions`: 3 个 bool 字段 + 未来扩展字段, 但本 change 范围小,struct 太重

### D2: L1Cache 路径 fail-fast 而非 silent-disable

**Decision**: `enable_cache=true` 路径显式 throw `std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage")`。

**Rationale**:
- 与 `MMUPlugin::do_lookup` Bare shortcut 模式一致（v0.10.4 hotfix 教训: 静默退化会导致难以发现的回归）
- 提早失败, 不会让下游测试误以为 Cache 工作了
- 错误信息指向 change name, 给读者明确的 dependency hint

**Alternatives considered**:
- (A) 静默退化为 bare (无 cache): 与 v0.10.4 hotfix bug 同模式, ❌
- (B) 返回 `std::unexpected<PluginError>` (ADR-047): 过于工业, 本 change 是零新增 capability, 仅基础设施扩展
- (C) 警告 + 静默退化: 用户可能忽略警告, ❌

### D3: CLI flag 用 GNU long-option 风格 (`--enable-mmu`)

**Decision**: 沿用现有 `--elf` / `--verilog` / `--work-dir` / `--cycles` 风格, 新增 `--enable-mmu` (无值) / `--mmu-mode <string>` / `--enable-cache` (无值)。

**Rationale**:
- 与现有 parse_args 风格统一 (line 98-123)
- GNU 标准风格, 与 `verilator --cc --public-flat-rw` 子命令风格一致

**Alternatives considered**:
- (A) `--mmu on/off` (单 flag + 值): 不直观, "on" 是 bare 还是 sv32?
- (B) `--with-mmu` / `--with-cache` (带 --without-* 反义): 引入第三态语义, 复杂
- (C) `--cfg <path>` 读 JSON 配置: 与现状 `--elf` `--verilog` 单一 path 风格不一致

### D4: ELF vendor 模板用 .S + build_manual_elf.sh

**Decision**: 新增 `tests/cpu/manual_elf/build_mmu_bare.S` + `build_l1cache_basic.S`, 扩展 `build_manual_elf.sh` 注册。

**Rationale**:
- 与现有 8 个 `.S` (add/and/div/mul/or/sll/srli/sub) + 3 个 `.elf` 一致
- `build_manual_elf.sh` 已有 entry 模式, 添加新 entry 不破坏 ABI
- 不引入新 vendor 工具链 (避免 build_rv32ui.sh 路径冲突)

**Alternatives considered**:
- (A) 复用 `build_rv32ui.sh` (vendor 40 个 rv32ui ELF): 路径冲突, 工具链不同 (`riscv32-unknown-elf-gcc` vs `riscv64-unknown-elf-as`)
- (B) 直接 inline 在 CPUConfig JSON: 失去 source-level 跟踪, AGENTS.md 推荐 .S 模式
- (C) 用 LLVM IR 中间表示: 与现有 manual_elf 风格不一致

### D5: AGENTS.md §honesty_audit 数字维护原则

**Decision**: 不修改 AGENTS.md 任何"实测数字", 仅在"已知测试状态"段增加新行说明 CLI flags 增强。

**Rationale**:
- AGENTS.md 明确 "VERILATOR 实测数字按 HEAD 写入禁止快照引用"
- 本 change 零新增 PASS/FAIL cases, 无数字变化
- 仅文字注释扩展 (`[verilator]` 测试 case 数 +1 不变, CLI flags +3)

**Alternatives considered**:
- 无 — 这是 hard 约束 (AGENTS.md v0.10.0 起明确)

## Risks / Trade-offs

### R1: std::optional<bool> ABI 兼容性长期风险

**Risk**: 未来 `CpuConfig::enable_mmu` 字段被删除时,`build_cpu` 的 `std::optional<bool>` 参数语义可能与现状冲突。

**Mitigation**:
- 显式文档: "`nullopt` = 原 4 参数行为（no-op, 不读 cfg）" 在 proposal.md §1 + design.md §D1 + spec.md §Scenario "显式三态语义" 都写明（修复 C-A: 与 spec 严格对齐）
- 单元测试覆盖 3 种状态 (nullopt/true/false) — Change 2a 加 (本 change 不加 test)
- 若未来 CpuConfig 字段需被 build_cpu 读取, 应另加 `CpuFactoryOptions` struct 参数（不改 5 参签名）, 不属本 change scope

### R2: 6 文件 / 12 call sites ABI 兼容失败

**Risk**（修复 S1 Oracle 复审数字）: 若 6 文件 / 12 call sites 任一传错参数顺序（虽然默认参数不破坏）, 或编译警告被升级为错误, ABI 兼容失败。

**Mitigation**:
- tasks.md TDD Step 1 加 **ABI smoke test**: 6 文件 / 12 call sites 零修改, build 0 error 0 warning（`grep -rn 'CpuFactoryChmem.*build_cpu' tests/cpu tools/verilator_runner` 验证）
- 签名 hash **会改变**（加默认参数改 mangled name, 接受）— tasks.md §3.5 已正确标注; 本 Mitigation 修正原"签名不变"措辞

### R3: L1Cache fail-fast 信息不可达

**Risk**: CI 跑 `cpu_verilator_sim --enable-cache ...` 时 fail-fast, 但用户可能误以为是工具链问题而非 L1Cache CH_MEM 未实装。

**Mitigation**:
- fail-fast 错误信息明确指向 change name (`verilator-l1cache-e2e-coverage`)
- `--help` 输出新增 "Note: --enable-cache requires L1Cache CH_MEM (see change verilator-l1cache-e2e-coverage in wave4)"

### R4: AGENTS.md §honesty_audit 同步遗漏导致后续维护腐烂

**Risk**: v0.10.x release 时 reviewer 误以为 AGENTS.md 数字已更新, 实际本 change 未触发数字变化

**Mitigation**:
- tasks.md TDD Step 5 (archive 前) 加 **数字 audit**: 跑 `bash tools/v0100-bootstrap.sh review` 比对 §honesty_audit 数字是否一致
- 若不一致, 更新 AGENTS.md; 若一致, 在 CHANGELOG 写明 "本 change 零数字变化"

### R5: L1Cache CH_MEM 实装时间不确定, hook 可能长期闲置

**Risk**: `enable_cache=true` fail-fast 在 wave4 cache-phase1.5-4way archive 之前 (估时 6-12 月) 都不可用

**Mitigation**:
- fail-fast 信息明确指向 change name + wave 编号
- proposal §Non-Goals §Future Work 显式声明: "wave4 archive 后, Change 2b 启动"

### Trade-off: 三态 std::optional vs 单 bool 复杂度

**Trade-off**: `std::optional<bool>` 让 API 调用方需理解 3 种状态 (`nullopt`/`true`/`false`), 比单 `bool` 略复杂。

**Decision 接受**: 三态语义清晰性 > 单 bool 简洁性, 6 文件 / 12 call sites 仅需传默认参数 `std::nullopt`, 实际使用复杂度低

## Migration Plan

### Phase 1: ABI 兼容性 smoke test (TDD Step 1)

```bash
# 验证 4 个现有调用方零修改通过 build
cmake --build build
./build/bin/chipforge_tests "[verilator]"           # 现有 1 case PASS
./build/bin/chipforge_tests "[cpu]" --list-tests | wc -l   # 数量不变
nm ./build/bin/cpu_verilator_sim | grep build_cpu   # 签名 hash 改变（接受, 默认参数改 mangled name）
```

### Phase 2: std::optional 参数扩展 (TDD Step 2)

`build_cpu` 签名扩展:
```cpp
build_cpu(elaboration_ctx, memory, initial_pc, preload_elf, elf_image,
          std::optional<bool> enable_mmu = std::nullopt,
          std::optional<std::string> mmu_mode = std::nullopt,
          std::optional<bool> enable_cache = std::nullopt)
```

### Phase 3: CLI flag 扩展 (TDD Step 3)

`cpu_verilator_sim.cpp::Options` + `parse_args`:
```cpp
struct Options {
  // ...
  bool enable_mmu = false;
  std::string mmu_mode = "bare";
  bool enable_cache = false;
};

if (a == "--enable-mmu") opts.enable_mmu = true;
else if (a == "--mmu-mode" && i+1<argc) opts.mmu_mode = argv[++i];
else if (a == "--enable-cache") opts.enable_cache = true;
```

`generate_verilog`:
```cpp
auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
    &ctx, nullptr, ch_uint<32>(kElfBase), true, elf,
    opts.enable_mmu, opts.mmu_mode, opts.enable_cache);
```

### Phase 4: ELF vendor 模板 (TDD Step 4)

`tests/cpu/manual_elf/build_manual_elf.sh` 新增 entry:
```bash
build_mmu_bare|6864  # 触发 sv32+ppn=0 边界
build_l1cache_basic|8100  # cache hit + miss
```

### Phase 5: 文档同步 + 架构门禁验证 (TDD Step 5)

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
# 全部 0 exit
# AGENTS.md §honesty_audit 数字无变化
# CHANGELOG.md v0.10.x 段新增本 change 条目
```

### Rollback Strategy

若 Step 2 触发 ABI 兼容失败 (6 文件 / 12 call sites 任一 build error):
1. Revert `cpu_factory_chmem.h` 改动
2. Re-run Phase 1 smoke
3. 重设计 (例如改用 struct `CpuFactoryOptions{ bool enable_mmu; std::string mmu_mode; bool enable_cache; }`)

## Open Questions

### Q1: AGENTS.md "已知测试状态" 段新增行措辞?

需 reviewer 决策:
- (A) "**[verilator]** `1/1 PASS` (5 ELF × tohost=1) — v0.10.x 扩展 CLI flags 3 项 (--enable-mmu/--mmu-mode/--enable-cache) 不增 case"
- (B) 单独段落"v0.10.x Verilator 基础设施扩展"独立于"已知测试状态"段
- (C) 不新增段, 仅在 CHANGELOG 写明

倾向 (A): 与现有段风格一致, 不引入新结构

### Q2: L1Cache fail-fast 错误信息措辞?

候选:
- (A) `"L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage"`
- (B) `"L1Cache CH_MEM not implemented; refer to wave4-csr-cache-dse change verilator-l1cache-e2e-coverage (planned for v0.2.0)"`
- (C) `"L1Cache CH_MEM not implemented (planned for v0.0.0; refer to change verilator-l1cache-e2e-coverage in docs/CHANGELOG.md)"`

倾向 (A): 简洁 + 指向 change name (跨 wave 通用), 不带版本号 (避免 version drift)

### Q3: 是否在 `cpu_verilator_sim` 输出 TOHOST 行新增 cycle baseline 表?

候选:
- (A) 新增 `--print-baseline` flag, 跑 baseline 5 ELF 输出 cycle 表
- (B) 不新增, 让下游 Change 2a/2b 自决 baseline 表生成
- (C) 在 sim_main.cpp 加 stderr 输出 cycle baseline 与 tsv 落盘

倾向 (B): 本 change 仅扩展 CLI flags, 不引入 baseline 表生成 (属 Change 2a/2b scope)