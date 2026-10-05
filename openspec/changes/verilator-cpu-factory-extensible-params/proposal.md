---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.0
depends_on: []
---

# verilator-cpu-factory-extensible-params — `CpuFactoryChmem::build_cpu` + `cpu_verilator_sim` 可选参数化扩展（v0.10.0 基础设施）

## Why

`tools/verilator_runner/cpu_verilator_sim.cpp` 当前硬编码调用 `CpuFactoryChmem::build_cpu(&ctx, nullptr, kElfBase, true, elf)`,**无任何 MMU/L1Cache 配置开关**；CLI 也仅支持 `--elf/--verilog/--work-dir/--cycles/--help`。结果:`tests/cpu/test_cpu_verilator_sim.cpp` 唯一 Verilator e2e 测试仅 5 个 RV32UI ELF × tohost=1,**MMU/L1Cache 端到端验证覆盖率为 0**（CPU + Bare translation + 无 Cache 唯一一条路径）。

后果:
1. v0.10.4 hotfix 修复的 `MMUPlugin::do_lookup` Bare shortcut 误判（`satp_value_` MODE 字段 vs `satp_ppn_`）类回归**只有 TLM 层防护,Verilator 链路零防护**;
2. `mfc-cpu-pipeline-multi-cycle-fsm` Phase G 计划的"DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)"硬门禁**当前被现状 runner 卡住**——MFC pipeline 不能开 enable_mmu 测 cycle-equal;
3. `cache-phase1.5-4way` Oracle #8 "5 ELF cycle-identical 不回归" 约束**期望 Cache CH_MEM 路径可跑 Verilator**,但当前 L1Cache 路径在 Verilator 链路不可达。

`CpuFactoryChmem::build_cpu` 是 7-plugin 5-stage 流水线工厂（`ip/cpu/cpu_factory_chmem.h:74`）,**未暴露 MMU/L1Cache 注册 hook**——扩展 API 即可解锁下游所有 MMU/Cache Verilator 测试。

## What Changes

### 1. `CpuFactoryChmem::build_cpu` 可选参数扩展 (向后兼容)

**位置**: `ip/cpu/cpu_factory_chmem.h:74`

新增三个 `std::optional`-wrapper 可选参数(放在现有参数之后,保持 **6 文件 / 12 call sites** (grep 验证) 中除 `cpu_verilator_sim.cpp` 外的 5 文件 / 11 call sites 零修改):

```cpp
static std::unique_ptr<PipeBuilder> build_cpu(
    ch::core::context* elaboration_ctx,
    T* memory = nullptr,
    T initial_pc = T{0x80000000},
    bool preload_elf = false,
    const std::vector<uint8_t>& elf_image = {},
    // === 新增 (P1) ===
    std::optional<bool> enable_mmu = std::nullopt,        // 三态: nullopt=原 4 参数行为 (no-op), true/false 显式 (修复 C-A Oracle 复审: 不读 cfg, build_cpu 签名无 cfg 参数)
    std::optional<std::string> mmu_mode = std::nullopt,   // "sv32"/"sv39"/"sv48"/"bare", 默认 "bare"
    std::optional<bool> enable_cache = std::nullopt);     // 三态: 同 enable_mmu
```

**实装策略**（修复 C-A Oracle 复审，与 spec.md Scenario 严格对齐）:
- `enable_mmu.value_or(false) == true` 时 **emit stderr/log "MMU hook enabled (mode=<mmu_mode>) — mmu_chmem.h not yet implemented, see change verilator-mmu-bare-plumbing-e2e"** (修复 C-A: **不**注册任何 MMU plugin; `mmu_chmem.h` 不在本 change scope, 且 4 个 active change 中无人认领真 sv32 CH_MEM 实装 — 详见 design.md §C-C);
- `enable_cache.value_or(false) == true` 时 `throw std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage")` (fail-fast, 由 Change 2b task group 1.5 翻转);
- `enable_mmu` 默认 nullopt 时 **行为等价于原 4 参数调用**（修复 C-A: **不**读 cfg; `build_cpu` 签名无 cfg 参数 — `ip/cpu/cpu_factory_chmem.h:74-79` 仅 5 参数, 未来如需读 cfg 应另加 `CpuFactoryOptions` struct, 不属本 change scope)。

### 6 文件 / 12 call sites ABI 兼容保证 (修复 S1 Oracle 三轮复审, grep `CpuFactoryChmem.*build_cpu` 实测):
| 调用方 | call 数 | 现状 | 修改 |
|--------|---------|------|------|
| `tools/verilator_runner/cpu_verilator_sim.cpp:129` | 1 | 4 参数 | 改用新参数 (本 change 内部) |
| `tests/cpu/test_cpu_chmem_vendored_elf.cpp:94` | 1 | 4 参数 | 零修改 (默认 nullopt 走原 4 参数行为) |
| `tests/cpu/test_cpu_5stage.cpp:53,75,122,152,193` | 5 | 4 参数 | 零修改 |
| `tests/cpu/test_cpu_decoded_inst_migration.cpp:322` | 1 | 4 参数 | 零修改 |
| `tests/cpu/test_cpu_memory_model_chmem.cpp:127,177,395` | 3 | 4 参数 | 零修改 |
| `tests/cpu/test_cpu_rtl_regfile_alu.cpp:541` | 1 | 4 参数 | 零修改 |

### 2. `cpu_verilator_sim.cpp` CLI 扩展

**位置**: `tools/verilator_runner/cpu_verilator_sim.cpp:91-123`

新增 `Options` 字段 + `parse_args` 处理:
- `--enable-mmu` (无值 bool flag,默认 false)
- `--mmu-mode <string>` (默认 "bare")
- `--enable-cache` (无值 bool flag,默认 false)

`--help` 输出新增这 3 项。`generate_verilog()` 调用 `build_cpu` 时透传 3 个新字段。

### 3. ELF vendor 框架扩展 (为下游 Change 2a/2b 铺路)

**位置**: `tests/cpu/manual_elf/`

新增 MMU/Cache-aware 汇编模板(本 change 仅提供 vendor 入口,**不为 Change 2a/2b 实装测试**):
- `build_mmu_bare.S` - sv32 + satp_ppn=0 边界配置,触发 v0.10.4 那类 `satp_value_`/`satp_ppn_` 误判路径(给 Change 2a 用)
- `build_l1cache_basic.S` - cache hit + miss 路径(给 Change 2b 用)
- `build_manual_elf.sh` 扩展支持新模板

**本 change 范围**: 仅 vendor 脚本 + 编译产物 (`*.elf` artifact),**不**新增 `[mmu-verilator]` / `[cache-verilator]` 测试 cases。

### 4. CHANGELOG/AGENTS.md 同步

遵守 `AGENTS.md §honesty_audit` 数字维护原则:
- CHANGELOG v0.10.x §Verification 段新增本 change 条目
- AGENTS.md "已知测试状态" 段增加 `[verilator]` 测试 1 → 1 (新增 CLI flags 不增 case) 备注
- 不修改任何"实测数字"(本 change 零新增 PASS/FAIL,仅扩展 CLI flags)

### 5. ADR 锚点

- **ADR-040 v2.0** (✅ Accepted) - 双模约束: 新增 CH_MEM 文件须遵守 9 项 CI 检查 (`check_plugin_portability.sh`)
- **ADR-082** (🚧 Drafting) - `Plugin::negotiate()` capability 协商: 本 change **不强制**使用,仅在 MMU/L1Cache plugin 自决需要时启用 (留 hook, 给下游 change 决定)
- **ADR-037 v2.0** (✅ Accepted) - D14 (Verilator sim cycle-identical): 本 change 不直接实施 D14,但通过扩展 CLI flags 为 D14 闭环铺路

## Capabilities

### New Capabilities

- `verilator-cpu-factory-params`: 定义 `CpuFactoryChmem::build_cpu` 的可选 MMU/Cache 参数 contract,以及 `cpu_verilator_sim` CLI 的 `--enable-mmu`/`--mmu-mode`/`--enable-cache` 三个 flag 行为契约。

### Modified Capabilities

无 (本 change 仅扩展参数语义,不修改现有 spec 行为)。

## Impact

- **修改代码** (~250 LOC):
  - `ip/cpu/cpu_factory_chmem.h` (+30 LOC: 3 个 std::optional 参数 + if 分支 hook)
  - `tools/verilator_runner/cpu_verilator_sim.cpp` (+50 LOC: Options 字段 + parse_args)
  - `tools/verilator_runner/CMakeLists.txt` (~5 LOC: 链接 cf_plugin::mmu / cf_plugin::cache 若需)
  - `tests/cpu/manual_elf/build_manual_elf.sh` (+20 LOC: 模板注册)
  - `tests/cpu/manual_elf/build_mmu_bare.S` (新建, ~30 LOC)
  - `tests/cpu/manual_elf/build_l1cache_basic.S` (新建, ~30 LOC)
- **新增测试**: 无 (本 change 仅扩展基础设施,测试属下游 Change 2a/2b)
- **新增文档**:
  - `openspec/changes/verilator-cpu-factory-extensible-params/design.md` (~150 LOC)
  - `openspec/changes/verilator-cpu-factory-extensible-params/tasks.md` (~100 LOC, TDD 5 步)
  - `openspec/changes/verilator-cpu-factory-extensible-params/specs/verilator-cpu-factory-params/spec.md` (~80 LOC)
  - CHANGELOG.md v0.10.x 段 +3-5 行
  - AGENTS.md "已知测试状态" 段 +2-3 行
- **依赖关系**:
  - 被 Change 2a `verilator-mmu-bare-plumbing-e2e` (wave5 P2) 阻塞依赖
  - 被 Change 2b `verilator-l1cache-e2e-coverage` (wave4 P2 占位) 阻塞依赖
  - 与 `mfc-cpu-pipeline-multi-cycle-fsm` Phase G Verilator DMIPS gate **soft-dep** (mfc 不显式 requires 本 change,但 Phase G 启动前 archive 才能让 mfc 走 enable_mmu 路径)
- **风险**:
  - `std::optional<bool>` 三态语义对当前 scope 已足够清晰。**注意**：未来若 CpuConfig 字段需被 build_cpu 读取, 应另加 `CpuFactoryOptions` struct 参数（不改 build_cpu 5 参签名）, 不属本 change scope（修复 C-A: 显式承诺不读 cfg）。
  - L1Cache CH_MEM 实装缺失时 `enable_cache=true` 行为——本 change 显式 fail-fast 提示 (不静默退化为 bare)
- **估时**: 2-3 周