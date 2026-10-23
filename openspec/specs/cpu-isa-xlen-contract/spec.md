# cpu-isa-xlen-contract Specification

## Purpose
Defines the verifiable contract between `CPUConfig::isa` and `CPUConfig::xlen` for the ChipForge CPU (ADR-087 §D1 three-state semantics: implemented / declared-reserved / unsupported, plus the §D2 isa→xlen mapping). The contract is enforced at runtime (fail-fast on unsupported ISA, on implemented-state xlen conflicts, and on kernel-T ↔ xlen mismatch) and at compile time (template-entry `static_assert` guards in the TLM semantic layer, so `CpuFactory<uint64_t>` fails to compile rather than silently computing wrong results). Declaration surfaces (JSON configs, schema, `tools/cpu_sim`) MUST state the actually-implemented ISA level, and the RV64 readiness gap table (`ip/cpu/docs/rv64_readiness_gap.md`) tracks the remaining gaps.

## Requirements

### Requirement: cpu-isa-xlen-contract

`CPUConfig::isa` 与 `CPUConfig::xlen` 必须构成可校验契约（ADR-087 §D1 三态 + §D2 映射；**适用范围 = TLM 侧 `cf::cpu::CpuFactory`** —— CH_MEM 侧 `CpuFactoryChmem` 不接受 `isa`（`ip/cpu/cpu_factory_chmem.h:99`），其契约由 B1 承载力阶段另行定义）：默认 `isa == "rv32im"`、`xlen == 32`；unsupported 取值在配置装载/工厂实例化路径**运行时 fail-fast**；implemented 态与 `xlen` 一致性冲突 fail-fast；`build_cpu<T>` 交叉校验 `xlen != sizeof(T)*8` 时 fail-fast；declared-reserved 取值（含 `rv64i/rv64gc`）被接受但必须 emit 一次显式 warning（禁止静默）。

#### Scenario: 声明与实现一致（implemented 态默认值）

- **WHEN** 使用 `CPUConfig{}` 默认构造（或装载 `isa="rv32im"` 的 JSON，未显式设置 xlen）
- **THEN** `isa == "rv32im"` 且 `xlen == 32`，`build_cpu()` 正常完成，无 warning 无异常

#### Scenario: unsupported 取值 fail-fast

- **WHEN** 装载 `isa="rv128i"`（schema enum 与 reserved 全集之外）
- **THEN** `CpuFactory::build_cpu()` 抛 `std::invalid_argument`，错误信息指出 unsupported isa（不静默、不降级）；`tools/cpu_sim` 经 `build_cpu()` 间接同效（校验 SSOT 在 build_cpu 入口）

#### Scenario: implemented 态一致性冲突 fail-fast

- **WHEN** 显式装载 `isa="rv32im"`（implemented）且 `xlen=64`
- **THEN** `CpuFactory::build_cpu()` 抛 `std::invalid_argument`（implemented ⇒ xlen 必须 32）

#### Scenario: 模板 T 与 xlen 交叉校验 fail-fast

- **WHEN** `xlen=64`（如 reserved 值派生态）且 `CpuFactory<uint32_t>::build_cpu()` 被调用
- **THEN** `build_cpu()` 抛 `std::invalid_argument`（运行时层兜底；`CpuFactory<uint64_t>` 编译层面由 int_alu/branch `static_assert` 拒绝，见下一条 Requirement）

#### Scenario: declared-reserved 不静默

- **WHEN** 装载 `isa="rv32imac"`（或运行时直接构造）并在 CpuFactory<uint32_t> 下 build
- **THEN** 配置被接受、build 正常完成，且 emit 恰好一次 warning（`std::cerr` 单行，含 isa 值与 ADR-087 §D1 指引），不 throw、不重复刷屏

### Requirement: cpu-isa-compile-time-template-guard

在 TLM 语义层模板入口（`ip/cpu/arch/riscv/int_alu.h` / `branch.h`）对 `sizeof(T) == 4` 加编译期护栏：`CpuFactory<uint64_t>` 实例化必须编译失败；护栏必须位于模板运算入口函数体内（实例化点触发），不得放文件作用域。`branch.h` 需覆盖**两处**（静态入口 `evaluate_branch` + `build()` 闭包内的真实分支判定），否则真实 BLT/BGE 路径不受护栏覆盖。

**前置条件（不可省略）**：现存 T64 实例化点必须先迁移（`tests/cpu/test_cpu_factory.cpp` 删 `build_cpu_rv64` case + 余下改 T32；`tests/cpu/test_branch_predictor_runtime_btb.cpp` 3 case 改 `uint32_t`），否则「`CpuFactory<uint64_t>` 编译失败」与「既有测试 0 退化」互斥（`nm -C` 实测 `RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` 今天已被实例化）。

#### Scenario: RV64 实例化在编译期被拒绝

- **WHEN** 在编译期实例化 `CpuFactory<uint64_t>`（T=int64 路径触发 int_alu/branch 模板入口）
- **THEN** 编译失败，错误信息指向 ADR-087 §8 G3（RV64 未实现准入）；变异对照：将护栏改为 `sizeof(T) == 8` 时现有 `[cpu]` 用例编译失败

#### Scenario: RV32 路径零影响

- **WHEN** 既有全部 RV32 测试（T=uint32_t）编译与运行
- **THEN** 守卫恒通过；`[cpu]` / `[cpu-integration]` / `[riscv-tests]` / `[mmu]` / `chipforge_tests_chmem` 0 退化（T64 迁移 + 测试完备性补全导致的 `[cpu]` 计数变化 129 → **137** 属预期，已在 AC9 校准）

### Requirement: cpu-isa-declaration-surface-correction

集成声明面不得声称未实现的 ISA 能力：`ip/cpu/configs/*.json` 与 `soc/docs/integration-guide.md` 的 `isa` 示例值回正确实现水平（`rv32im`），`grep -rn '"rv64' ip/cpu/configs/ soc/` 返回 0；schema `isa` enum 只含 `rv32i`/`rv32im`/`rv32imac`（`rv64i`/`rv64gc` 以 **description 文本**保留为 declared-reserved 文档化，**不再是 enum 项**），default `rv32im`。同时 3 个 JSON 的 `mmu_mode` 归位 `sv32`（XLEN=32 下只有 Sv32 合法，满足 schema `:204` 条件约束 `isa ∈ {rv32i,rv32im,rv32imac} ⇒ mmu_mode const "sv32"`）。

#### Scenario: 配置与文档无虚假声明

- **WHEN** 执行 `grep -rn '"rv64' ip/cpu/configs/ soc/` 与 schema default 检查
- **THEN** 返回 0 条 `isa` 实际配置值含 rv64；schema default 为 `rv32im`；schema enum 不含 `rv64*`（reserved 值仅在 description 文本中）

#### Scenario: 配置满足 schema 条件约束

- **WHEN** 用 Draft7 validator 校验 4 个配置 JSON（`isa` 已纠偏为 `rv32im`）
- **THEN** 除 pre-existing 基线（顶层 `_comment` 撞 `additionalProperties: false`）外**无新增**违规；`mmu_mode` 为 `sv32`（或不存在），不触发 `:204` 条件分支

### Requirement: cpu-isa-cpusim-pin-removal

`tools/cpu_sim/main.cpp` 不得再用 `isa="rv32i"` 硬编码覆盖配置（**含注释**）；行为改为跟随 config；`load_config()` 必须解析 schema 已声明的 `xlen`（不得静默丢弃）；既有 5 个 cpu_sim CLI e2e 用例（exec_cmd 约定）0 退化。

#### Scenario: cpu_sim 跟随配置语义

- **WHEN** 运行 5 个 cpu_sim e2e 用例（`test_cpu_sim_real_tohost` + 4 个 `{3,5,7,10}stage_add_elf_end_to_end`）
- **THEN** 全部 PASS 且 `grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp` 返回 0

#### Scenario: cpu_sim 解析 xlen

- **WHEN** 装载含 `"xlen": 32` 的 JSON 到 `CPUConfig`
- **THEN** `cfg.xlen == 32`（`load_config()` 显式解析，不依赖结构体默认值）

### Requirement: cpu-isa-deadcode-regfile-removal

`ip/cpu/plugins/reg_file.cpp`（显式实例化 `RegFilePlugin<std::uint64_t>` 却混用 `keys_rv32::*` 的死代码）需删除；该文件无任何 CMake 引用，删除后任一 binary 编译不受影响。

#### Scenario: 死代码删除后编译与测试零退化

- **WHEN** 删除 `ip/cpu/plugins/reg_file.cpp` 并全量 build 双 binary
- **THEN** `grep -rn "RegFilePlugin<std::uint64_t>" ip/` 返回 0；`chipforge_tests` 与 `chipforge_tests_chmem` 构建成功、全量测试 0 退化

### Requirement: cpu-isa-gap-table-status-sync

实施完成后 `ip/cpu/docs/rv64_readiness_gap.md`（ADR-087 §D6 SSOT）§2 状态列同步更新为实际状态，且 CI 软门禁（"新增 XLEN 假设必须登记缺口表"，warn 级）上线。

#### Scenario: 缺口表与实现状态一致

- **WHEN** 核对缺口表 §2 的 0.1–0.10 各条与代码实态
- **THEN** 契约修复条目标 ✅（0.7/0.8 历史项保持 ✅），未修复项保持 🟡/🔴，无新漂移

#### Scenario: CI 软门禁可执行

- **WHEN** 人为在未登记缺口表的情况下新增一处 XLEN 硬编码假设（`kXlenBits =` 定义点）并跑门禁脚本
- **THEN** 门禁输出 warn（`文件:行` + 提示登记缺口表 §5），不阻塞 CI；基线态（allowlist = 缺口表 §5 的 9 处定义点 / **9 个文件**，实测 `grep -rnE "kXlenBits *=" --include='*.h' ip/` = 9 命中 9 文件）输出 `warn=0`
