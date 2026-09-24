## Why

`mmu-cache-integration`（archived 2026-09-13）落地了 MMU+Cache VIPT 数据流 + RiscV hook 实装（`ip/cpu/plugins/mmu.cpp`），但**消费端**——RISC-V CPU Plugin——**尚未消费这些 hook**。`ip/cpu/cpu_factory.h` 仅有 `enable_mmu=true` 配置字段（`cpu_factory.h:65`），**没有** `RiscvMMUPlugin` 注册到 `PluginOrder` 数组；`ip/cpu/configs/cpu_params_schema.json` 已有 `enable_mmu`/`mmu_mode` 字段（`cpu_params_schema.json:42-52`）和条件依赖（`cpu_params_schema.json:203-204`），但 schema 字段无对应代码实现。

MMU+Cache VIPT 链路当前是**孤岛**：MMU 输出 `pl::MMU_VADDR`/`pl::PADDR` 给谁？CPU `tlb_lookup_ifetch`/`tlb_lookup_loadstore` 没有订阅这两个 Key；CPU 不写 `satp_value_`、不调 `sfence_vma()`。`tests/cpu/integration/test_5stage_riscv.cpp` 等 4 个 integration tests 全部跑通，但 `enable_mmu=true` 是 bit-identical 通过——MMU hook 没真正接到 CPU pipeline 上。

本 change 把 `RiscvMMUPlugin` 实接入 CPU Plugin pipeline：
- `cpu_factory.h` 条件注册（仅 `config.enable_mmu=true` 时插入 PluginOrder）
- `RiscvMMUPlugin::at_stage("csr_write_satp")` / `at_stage("sfence_vma")` 两个 substage 路由到 MMU hook（已实装）
- exception 12/13/15 从 `MMUPlugin` 传播到 CPU exception code 路径
- 5-stage + 7-stage + 10-stage RiscV integration tests 验证 MMU 启用/禁用双路径

解锁 **M5 demo critical path**：让 `soc/cpu_l1_picolibc/demo.json`（下一 change）真正能跑 RISC-V ELF → CPU → MMU → L1 → Memory 链。

## What Changes

### Plugin 注册（commit 1/6）

- **`ip/cpu/cpu_factory.h`**（修改）：`PluginOrder` 数组条件注册 `RiscvMMUPlugin`
  - 仅 `config.enable_mmu=true` 时插入；`config.enable_mmu=false` 保持 bit-identical baseline
  - MMU Plugin 自身仍兼容旧 API（`ip/cpu/plugins/mmu.h:64` `using MMUPlugin = RiscvMMUPlugin`），向后兼容 mmu-cache-integration commit 6 的 4 个 `[RiscV]` tests
  - `mmu_mode` 字段映射到 MMU Plugin 构造（sv32→`SvMode::Sv32`、sv39→`SvMode::Sv39`、sv48→`SvMode::Sv48`）
  - 默认 TLB 几何：2-level 8/8 + LRU + ptw_max_inflight=2（与 `mmu-cache-integration commit 5` SoC JSON 一致）
- **`ip/cpu/cpu_factory.h`**（修改）：`build_cpu()` 函数根据 `config.enable_mmu` 调 `pb.register_plugin<RiscvMMUPlugin>(...)`
- **`ip/cpu/configs/cpu_params_schema.json`**（无需修改）：`enable_mmu` + `mmu_mode` + 条件依赖已就位（line 42-52, 203-204）

### at_stage 路由（commit 2/6）

- **`ip/cpu/cpu_factory.h`**（修改）：CPU pipeline 增加 3 个 substage 声明（仅 `enable_mmu=true` 时）
  - `execute → csr_write_satp`（在 execute 阶段写 satp CSR 拦截）
  - `execute → sfence_vma`（在 execute 阶段写 SFENCE.VMA 拦截）
  - `memory → mmu_exit`（在 memory 阶段路由 page fault exception 12/13/15）
- **`ip/cpu/plugins/mmu.cpp`**（修改）：`RiscvMMUPlugin::at_stage()` 三个闭包实装
  - `csr_write_satp` 闭包：从 CPU 写入的 `pl::SAT` Payload Key 读取 satp_value，调 `csr_write_satp()` 实装
  - `sfence_vma` 闭包：从 CPU 写入的 `pl::SFENCE_VADDR` / `pl::SFENCE_ASID` Key 读取 rs1/rs2，调 `sfence_vma()` 实装（RISC-V Spec §6.2 4-way dispatch）
  - `mmu_exit` 闭包：从 `pl::EXCEPTION_CODE` Key 读取 12/13/15，写入 CPU exception 路径
- **`ip/mmu/tlm/mmu_keys.h`**（无需修改）：4 个 RiscV Key（`EXCEPTION_CODE`/`SATP_PPN`/`SATP_MODE`）+ 14 个原始 Key 已存在
- 新增 CPU ↔ MMU IPC Key：CPU 写 `pl::SAT` / `pl::SFENCE_VADDR` / `pl::SFENCE_ASID`（在 `ip/cpu/tlm/cpu_keys.h` 或扩展 `ip/mmu/tlm/mmu_keys.h`——本次 commit 选择放在 `ip/cpu/tlm/cpu_keys.h`，**避免** mmu 库反向依赖 CPU Key）

### 集成测试（commit 3/6）

- **`tests/cpu/integration/test_5stage_riscv.cpp`**（扩展）：+2 case
  - `EnableMMU5StageBuilds`: `cpu_factory` 用 `enable_mmu=true` 注册 MMU，verify 5 个 stage + MMU 3 substage 都在
  - `EnableMMUDisabledBitIdenticalBaseline`: `enable_mmu=false` baseline 字节级保持（21 个 [cache] + 25 个 [cpu-integration] 测试零回归）
- **`tests/cpu/integration/test_7stage_riscv.cpp`**（扩展）：+1 case
  - `EnableMMU7StageCommitRetire`: 7-stage + MMU exception routing
- **`tests/cpu/integration/test_10stage_riscv.cpp`**（扩展）：+1 case
  - `EnableMMU10StageDeepPipeline`: 10-stage + MMU substage 在 commit/retire 前
- **`tests/cpu/integration/test_3stage_riscv.cpp`**（扩展）：+1 case
  - `EnableMMU3StageBuilds`: 3-stage minimum pipeline + MMU
- **`tests/cpu/test_cpu_riscv_mmu_hooks.cpp`**（新增独立文件）：+3 case 验证 RiscV hook 接到 CPU pipeline
  - `CSRWriteSatpRoutesToRiscVMMUPlugin`: 模拟 CPU 写 satp CSR，verify MMUPlugin.csr_write_satp 被调 + satp_value 正确
  - `SFENCEVMARoutesToRiscVMMUPlugin`: 模拟 SFENCE.VMA 指令，verify sfence_vma 4-way dispatch
  - `MMUExceptionPropagatesToCPU`: 模拟 page fault，verify exception code 12/13/15 传到 CPU

### Docs sync（commit 4/6）

- **`ip/cpu/README.md`**（修改）：新增"RiscV MMU Integration" 段，说明 `enable_mmu=true` 时 MMU substage 注册位置 + RiscV hook 路由
- **`ip/cpu/configs/cpu_params_schema.json`**（修改 description）：`enable_mmu` 字段 description 加 "详见 ip/cpu/plugins/mmu.h RiscVMMUPlugin"
- **`ip/mmu/STATUS.md`**（修改）：`INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链)` → `INTEGRATED + CPU PIPELINE`
- **`CHANGELOG.md`**（修改）：v0.2.0 条目 — "cpu-mmu-integration: RiscVMMUPlugin 注册到 cpu_factory + at_stage substages + 5/3/7/10-stage integration tests + 3 个 RiscV hook 集成测试"

### 最终验证（commit 5/6）

- 4 architecture gates all PASS
- baseline 维持 306/306 PASS + +8 新增 cpu-mmu-integration 测试 = **314/314 PASS**

### 不修改

- `L1CachePlugin` / `L1CacheTLMBridge` / `cf_plugin` 框架层 / CppTLM / CppHDL
- `MMUPlugin` / `RiscvMMUPlugin` 核心算法（mmu-cache-integration commit 6 已实装 + 测试）
- ADR-044（VIPT 锁定 + 数据流契约已稳定）
- `bundles/mem_bundles.h` 现有 Bundle 签名
- `ip/cpu/plugins/` 中除 `mmu.h` 外的其他 Plugin
- `plugin-framework-stall`（独立 change，Tier 1 #2，不在本 change scope）
- `cache-phase1.5-4way`（独立 change，Tier 2 #4）
- MMU `tlb_entry.h` 等 lib/ 层文件
- `ip/mmu/rtl/`（Phase 5+ 沿用）
- Sv32/Sv48 PTW 解码（推迟到 `mmu-sv32-sv48-ext` change）
- DSE 配置扫描（推迟到 `cache-dse-sweep` change）
- `L1Cache` 4-way VIPT 升级（推迟到 `cache-phase1.5-4way` change）
- `ip/memory/` subsystem（Phase 2+，cpptlm MemoryTLM 已服务 SoC JSON）

## Capabilities

### New Capabilities

- **`cpu-mmu-pipeline-registration`**: `cpu_factory.h::build_cpu()` 条件注册 `RiscvMMUPlugin` 在 `PluginOrder` 数组（仅 `config.enable_mmu=true`）；MMU hook 通过 `at_stage("csr_write_satp")` / `at_stage("sfence_vma")` / `at_stage("mmu_exit")` 三个 substage 接到 CPU pipeline。`enable_mmu=false` baseline 必须 bit-identical 通过 25 个 [cpu-integration] 测试
- **`cpu-mmu-ipc-keys`**: CPU → MMU IPC Payload Key 集合（`SAT` / `SFENCE_VADDR` / `SFENCE_ASID` / `MMU_EXCEPTION`）；Key 放在 `ip/cpu/tlm/cpu_keys.h` 避免 mmu 反向依赖 CPU；CPU write `SAT` 后 MMU 闭包读取并调 `RiscvMMUPlugin::csr_write_satp()`
- **`cpu-mmu-exception-routing`**: exception 12/13/15 路由从 MMUPlugin → CPU exception path；MMU exception code 通过 `pl::MMU_EXCEPTION` Payload Key 传递，CPU `at_stage("mmu_exit")` 闭包读取并写到 CPU exception 路径
- **`cpu-mmu-pipeline-integration-tests`**: 5 个 integration tests（5/7/10/3-stage + RiscV hook tests）验证 `enable_mmu=true/false` 双路径；`enable_mmu=false` 必须 bit-identical baseline；`enable_mmu=true` 验证 5/7/10/3-stage + MMU substage 全部注册 + RiscV hook 路由

### Modified Capabilities

- **`mmu-riscv-isa-adapter`**（mmu-tlb-ptw-impl archived）：原 spec 列出 "RiscvMMUPlugin 继承 MMUPlugin, 单源真相位于 ip/cpu/plugins/mmu.h, 自持 satp CSR 状态, SFENCE.VMA hook 路由到 invalidate_vaddr/asid/all, exception 12/13/15 写 pl::EXCEPTION_CODE"。本 change **modify** 该 spec：增加 "RiscvMMUPlugin 必须通过 cpu_factory.h::build_cpu() 在 config.enable_mmu=true 时被注册到 PluginOrder 数组；通过 at_stage(\"csr_write_satp\")/at_stage(\"sfence_vma\")/at_stage(\"mmu_exit\") 三个 substage 接到 CPU pipeline"
- **`cpu-pipeline-config-schema`**（新增）：`ip/cpu/configs/cpu_params_schema.json` 已有 `enable_mmu` + `mmu_mode` 字段。本 change **modify** 该 spec：增加 "enable_mmu=true 必须触发 RiscVMMUPlugin 注册；mmu_mode 字段值映射到 MMUPlugin SvMode 构造"

## Impact

- **新增文件**:
  - `ip/cpu/tlm/cpu_keys.h`（~40 LOC）—— CPU → MMU IPC Payload Key 集合
  - `tests/cpu/test_cpu_riscv_mmu_hooks.cpp`（~150 LOC）—— 3 个 RiscV hook 集成测试
- **修改文件**:
  - `ip/cpu/cpu_factory.h`：`PluginOrder` 条件注册 + `build_cpu()` 新增 MMU 注册调用 + 3 substage 声明（~+50 LOC）
  - `ip/cpu/plugins/mmu.cpp`：`at_stage()` 三个闭包实装（~+40 LOC）
  - `tests/cpu/integration/test_5stage_riscv.cpp`：+2 case（~+30 LOC）
  - `tests/cpu/integration/test_3stage_riscv.cpp`：+1 case（~+15 LOC）
  - `tests/cpu/integration/test_7stage_riscv.cpp`：+1 case（~+15 LOC）**+ Oracle B2 修复：现有 `node_count()==7` / `stage_count()==14` assertions 改为 `enable_mmu=true` 路径下 +5/+5（3 MMUPlugin::setup substages + 3 RiscV hook substages = 6，stage_count 计 at_stage 注册数；待 commit 1 精确计算）**
  - `tests/cpu/integration/test_10stage_riscv.cpp`：+1 case（~+15 LOC）**+ Oracle B2 修复：审计 `10stage_topology_from_config`（cpu_deep_pipeline.json 含 `enable_mmu:true`）—— 仅更新 assert 计 enable_mmu=true 时的预期数字**
  - `tests/cpu/test_cpu_factory.cpp`：**Oracle B2 修复**—— `build_cpu_registers_11_real_plugins` case 的 `plugins.size()==11` 改为 `plugins.size()==12`（enable_mmu=true 默认路径）
  - `ip/cpu/configs/cpu_params_schema.json`：enable_mmu description 微调（~+10 chars）
  - `ip/cpu/README.md`：新增"RiscV MMU Integration" 段（~+30 LOC）
  - `ip/mmu/STATUS.md`：INTEGRATED + CPU PIPELINE 状态更新
  - `CHANGELOG.md`：v0.2.0 条目
- **不修改**: 见 "What Changes / 不修改" 节
- **依赖与时序**:
  - 本 change 必须在 `mmu-cache-integration`（archived 2026-09-13）**之后**实施——MMU hook + VIPT 数据流已就位 ✓
  - 本 change 必须在 ADR-044（`cb85fea`/`baa3757`）**之后**实施——VIPT 数据流契约已锁定 ✓
  - 本 change 是后续 `soc-cpu-l1-mmu-demo` change 的强制前置
- **基线影响**:
  - 当前 ctest 基线：306/306 PASS（291 + 15 mmu-cache-integration）
  - 增量：5 stage tests (3-stage/5-stage/5-stage-disabled/7-stage/10-stage) + 3 RiscV hook tests = **+8 cases** → **314/314 PASS**；[cpu-integration] 25+8=**33/33**
  - 关键约束：`enable_mmu=false` baseline 必须 **bit-identical** 通过 25 个 [cpu-integration] 测试
- **breaking 变更**:
  - `cpu_factory.h::build_cpu()` 现在可能注册 MMU Plugin（仅 `enable_mmu=true` 时）——若下游 test 用 PipeBuilder 手动指定 PluginOrder 而非 cpu_factory，可能受影响。缓解：所有现有 cpu test 走 `cpu_factory`，迁移无破坏
  - `RiscvMMUPlugin::at_stage` 增加 3 个新 substage（csr_write_satp/sfence_vma/mmu_exit）声明——若下游 test 用 `pb.has_stage("csr_write_satp")` 检测会失败。缓解：所有现有 mmu test 用 `MMUPlugin::setup()` 自有 substage，无冲突
  - **Oracle B2 真实 breaking**：现有 `tests/cpu/integration/test_7stage_riscv.cpp` 与 `tests/cpu/test_cpu_factory.cpp` 中 `node_count()` / `stage_count()` / `plugins.size()` assertions **硬编码**旧数字（7/14/11），启用 `enable_mmu=true` 默认路径后这些数字会跳变。**修复**（已列在 proposal Impact "修改文件"）：commit 1 同步更新这 4 个 assertions；commit 1 落定后 `enable_mmu=false` baseline 测试**字节级保持**，`enable_mmu=true` 路径测试用更新后数字。**这是真正的契约变更**，需要 reviewer 关注。

## Alternatives Considered

### Alternative A: cpu_factory 全局启用 MMU（无条件注册）

**放弃理由**: 强制所有 config 启用 MMU 会破坏 bit-identical baseline 25 个 [cpu-integration] 测试（其中部分可能跑无 MMU 路径）。**已采纳**: 条件注册 `if (config.enable_mmu)`，保持 baseline 兼容。

### Alternative B: 推迟 RiscV hook 路由到独立 change

**放弃理由**: RiscV hook 路由与 cpu_factory 注册是同一工作流的两半，拆分会引入 2 个 review round-trip。**已采纳**: 一并实施，5 commits 总工作量可控。

### Alternative C: 不动 RiscV hook，复用 mmu-cache-integration commit 6 直调接口

**放弃理由**: MMU hook 设计意图是 at_stage 闭包驱动（mmu-cache-integration commit 6 comment "此函数主要供 CPU 侧 at_stage 触发"）。直调会破坏 D4 范式 + 引入 CPU PluginOrder 依赖 mmu 子模块。**已采纳**: at_stage 三个 substage。

### Alternative D: CPU exception 路由通过 mmu_keys.h 新 Key

**放弃理由**: mmu 反向依赖问题——Key 应放在生产者侧（CPU），不放在消费者侧（mmu）。**已采纳**: Key 放 `ip/cpu/tlm/cpu_keys.h`，mmu 读 CPU 写的 Key（自然依赖方向）。

### Alternative E: 工具链修复 5 个 pre-existing failures 也包含在本 change

**放弃理由**: 工具链修复是 RISC-V 测试框架问题（tohost 字符串 + riscv64 assembler path），与 CPU-MMU 集成无强耦合，独立 S-sized change 更合适。**已采纳**: 工具链修复留给 `soc-cpu-l1-mmu-demo` change 的 commit 0（demo 跑起来前置条件）。

### Alternative F: plugin-framework-stall 也包含在本 change

**放弃理由**: plugin-framework-stall 改 `PipeBuilder::run()` 契约，影响范围最大（影响每个 Plugin），应作为独立 Tier 1 #2 change 单独评审。**已采纳**: stall 推迟到独立 change，本 change 用 `PTW_ACTIVE + RETRY` 数据依赖方案（mmu-cache-integration Decision 7 已知 limitation）。

### Alternative G: cache-phase1.5-4way 也包含在本 change

**放弃理由**: 4-way 升级 scope 大（10+ commits），与 cpu-mmu-integration 正交。**已采纳**: 4-way 推迟到独立 Tier 2 #4 change。

### Alternative H: L1Cache 4-way VIPT 升级同步

**放弃理由**: 与 G 同——L1Cache 4-way 与 cpu-mmu-integration 无直接耦合。**已采纳**: L1Cache 4-way 推迟。