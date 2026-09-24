## Why

Round-2 调研发现 `IBusPlugin`（`ibus.h:54` 写 NOP 0x00000013）和 `DBusPlugin`（`dbus.h:52-62` LOAD 返 0、STORE 空操作）是 stub，阻塞任何真 RISC-V 程序执行；即使替换这 2 个 stub 还不够——`PipeBuilder::run()` 按闭包插入顺序而非阶段/相位调度（`pipe_builder.h:100-109`），阶段间无传播（per-stage PipeNode 独立 KV，无 PC/INSTRUCTION 跨节点传递），且**没有任何插件写 PC**（隐藏第三 stub）。soc-cpu-l1-mmu-demo change 已删除（依赖此修复），需要先修 stub + 调度 + 传播 + PC 写入。

## What Changes

### Layer 0（前置，必做）：`PipeBuilder::run()` 阶段+相位有序调度 + StageLinkPlugin

- **`include/cf/plugin/pipe_builder.h`**（修改，`run()` 实现）：改线性闭包顺序为按"阶段首次出现序 × Phase(EARLY→NORMAL→LATE)"调度。`TopologyBuilder::expand` 已在 `pb->build()` 前调用（`cpu_factory.h:245-261`），阶段首现序已正确（fetch→decode→execute→memory→writeback），**无需改 factory 顺序**。~15 LOC。

- **`ip/cpu/plugins/stage_link.h/.cpp`**（新建）：`StageLinkPlugin` 注册 4 个 `at_stage("X",Phase::EARLY)` 闭包，**在 `CpuFactory::build_cpu` 中 register_early_plugins 之前注册**（保证 StageLink 的 EARLY 闭包在所有业务 plugin 的 EARLY 之前跑），阶段有序后保证在本阶段所有业务闭包前跑——fetch→decode(PC, INSTRUCTION)、decode→execute(PC, DECODE, RISCV_DETAIL, RS1, RS2)、execute→memory(PC, DECODE, MEM_ADDR, MEM_DATA, RD_DATA)、memory→writeback(PC, DECODE, RD_DATA, MEM_DATA)。**D4: PASS**（纯 at_stage 声明式，无 tick/状态机/早返）。~80 LOC。

### Layer 1+3: IBusPlugin stub 替换 + PC 更新归位

- **`ip/cpu/plugins/ibus.h/.cpp`**（修改）：
  - 构造注入 `PicolibcHostMemory* mem_`（nullable;null = 保持现有 NOP stub 行为，`tests/cpu/test_ibus.cpp` 的 `set_instruction()` 继续可用）
  - `at_stage("fetch",NORMAL)`: 读 `PC` 写 INSTRUCTION = `mem_->read_word(pc)`
  - `at_stage("writeback",Phase::LATE)`: 读传播来的 `DECODE.branch_taken`/`branch_target`（`payload_common.h:94-95`，由 `branch.h:96-97` 真实写入），写 `PC = taken ? target : pc+4` 到 fetch 节点
  - **D4: PASS**（无 ch_* 类型；不用早返；纯声明式）。~40 LOC
- **`ip/cpu/cpu_factory.h`**（修改，`build_cpu` 签名）：加可选参数 `PicolibcHostMemory* mem = nullptr`（默认 null = 现有 NOP stub 行为，零回归）；向后兼容所有既有单元测试。~5 LOC

### Layer 2: DBusPlugin stub 替换 + LSU 职责归位

- **`ip/cpu/plugins/dbus.h/.cpp`**（修改）：
  - `at_stage("memory",NORMAL)`:LOAD → `mem_->read_word(MEM_ADDR)` 写 MEM_DATA；STORE → `mem_->write_word(MEM_ADDR, MEM_DATA)`
  - 删除 `dbus.h:53,58` 的重复地址生成——**职责归位**：LSU = AGU, DBus = 总线接口
  - **D4: PASS**（同 Layer 1）。~35 LOC
- **`ip/cpu/arch/riscv/lsu.h`**（修改，注意是 `arch/riscv/` 而非 `plugins/`）：删除 `lsu.h:66` 的 `MEM_DATA=0` stub 行（LSU 不再写 MEM_DATA, 仅做地址生成与 STORE data）~1 LOC

### Layer 4: `cpu_sim` 接线 + 删除软件解释器

- **`tools/cpu_sim/main.cpp`**（修改）：
  - 构造 `PicolibcHostMemory mem;`（已存在）
  - 调 `CpuFactory::build_cpu<T>(cfg, &mem)` 注入 mem
  - **删除** `main.cpp:176-213` 软件解释器（不再需要——CPU pipeline 真实执行）
  - 主循环：`pb.run()` 直到 `mem.exited()`（`picolibc_host_memory.h:check_tohost`）
  - `cfg.isa = "rv32i"` 显式 pin（修既有 `CpuFactory<uint32_t>` vs `isa="rv64gc"` config 不一致）。~50 LOC 调整（净减 ~30 LOC 删除解释器）
- **`tests/cpu/integration/test_5stage_riscv.cpp:155-163`**（改造既有 `5stage_add_elf_end_to_end`）：删除软件解释器后，原依赖 `cpu_sim` 的 tohost=1 验证由"解释器路径"转为"真实 CPU pipeline 路径"。无需新增 case；只需确认既有断言在真实 pipeline 下仍 PASS。

### 不修改

- `cf_plugin` 框架层（除 `pipe_builder.h::run()` 排序调整）
- L1CacheTLMBridge/Adapter、MMUTLMBridge/Adapter（既有 cpptlm 集成不变）
- Decode/ALU/Mul/Branch/Hazard/RegFile（已是真实实现）
- Phase 5 RTL 转换路径
- BranchPredictor 真实 redirect（保持 stub 无害——单 pass 语义下无 mispredict）
- CacheTLM/MemoryTLM 桥接（SoC 级 ELF 加载是后续 change）
- Sv32/Sv48 PTW（`mmu-sv32-sv48-ext` change）

## Capabilities

### New Capabilities

- **`cpu-stage-ordered-scheduling`**: `PipeBuilder::run()` MUST execute callbacks in canonical stage order (fetch→decode→execute→memory→writeback) × Phase order (EARLY→NORMAL→LATE), so plugin `at_stage("X",EARLY)` closure runs before any NORMAL phase of stage X. Verified by existing pipe_builder tests (some insertion-order assertions may need updating).

- **`cpu-stage-link-propagation`**: 4 propagation closures (`StageLinkPlugin`) copy keys fetch→decode→execute→memory→writeback at each stage boundary Phase::EARLY. Without propagation, plugins can't read upstream payloads since each PipeNode is an independent KV store.

- **`cpu-real-fetch-and-memory`**: `IBusPlugin` reads `PC` and writes `INSTRUCTION` = `mem_->read_word(pc)` (via injected `PicolibcHostMemory*`); `DBusPlugin` reads `MEM_ADDR` and writes `MEM_DATA` = `mem_->read_word(addr)` for LOAD or `mem_->write_word(addr, data)` for STORE. When `mem_` is null (legacy unit tests), IBus writes NOP and DBus writes 0, preserving existing behavior.

- **`cpu-pc-update-on-writeback`**: `IBusPlugin` writes `PC = branch_taken ? branch_target : pc+4` to fetch node at `at_stage("writeback",Phase::LATE)`. Read `DECODE.branch_taken`/`branch_target` from writeback node (propagated from execute). Single-pass semantics mean no misprediction / flush needed.

### Modified Capabilities

- **`cpu-pipeline-config-schema`**: `CpuFactory::build_cpu` signature gains optional second parameter `PicolibcHostMemory* mem = nullptr` (back-compat: null preserves all existing unit tests' stub behavior).
- **`cpu-real-fetch-and-memory`** (modified header): `IBusPlugin` constructor now takes optional `PicolibcHostMemory*` (defaults null for back-compat with `test_ibus.cpp`).
- **`mmu-tlb-coherence-protocol`** (mmu-tlb-ptw-impl archived, no modification needed): no change.
- **`mmu-riscv-isa-adapter`** (cpu-mmu-integration archived): no change.

## Impact

- **修改文件**:
  - `include/cf/plugin/pipe_builder.h`（run() 阶段+相位有序，~15 LOC）
  - `ip/cpu/cpu_factory.h`（build_cpu 加可选 PicolibcHostMemory* 参数，~5 LOC）
  - `ip/cpu/plugins/ibus.h/.cpp`（构造注入 + 真实 fetch + writeback LATE PC 更新，~40 LOC）
  - `ip/cpu/plugins/dbus.h/.cpp`（构造注入 + 真实访存，~35 LOC）
  - `ip/cpu/arch/riscv/lsu.h`（删 1 行 `MEM_DATA=0` stub）
  - `tools/cpu_sim/main.cpp`（构造 mem 注入 + 删除软件解释器 + pin isa=rv32i，~50 LOC 调整净减 ~30）
  - `tests/cpu/integration/test_5stage_riscv.cpp`（+1 case 验证 tohost 路径）
- **新增文件**:
  - `ip/cpu/plugins/stage_link.h/.cpp`（4 传播闭包，~80 LOC）
- **基线影响**: 306 → 307 PASS（+1 新 case：StageLinkPlugin propagation smoke test；既有 5-stage 的 tohost=1 用例在解释器删除后转为真实 pipeline 验证，不算新增）
- **阻塞的下游**:
  - `soc-cpu-l1-mmu-demo` change（已删除，恢复需此 change 完成 + cpu_factory 集成 PicolibcHostMemory 后再启）
- **breaking 变更**: `PipeBuilder::run()` 排序变化可能影响依赖插入序的既有单测（11 个 framework[pipe_builder] 测试 + 依赖插入序的 cpu 单测），需逐项审计后调整断言（语义升级，非回归）
