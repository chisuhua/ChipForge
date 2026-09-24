## Context

mmu-cache-integration (archived 2026-09-13) + ptw-walk-bridge-fix (archived 2026-09-14) 完成后，317/317 tests PASS + 4 architecture gates 全绿。Roadmap §5 里程碑 1（`soc-cpu-l1-mmu-demo`）已规划但调研发现 **CPU Pipeline 核心 Plugin（IBusPlugin + DBusPlugin）是 STUB**——IBus 永远返回 NOP（0x00000013），DBus 永远返回 0/什么都不做；任何路径要"真跑 RISC-V"都绕不开这个 stub。Round-2 调研（Explore + Oracle）进一步发现两个隐藏前置：(1) `PipeBuilder::run()` 按闭包插入顺序而非阶段/相位调度（`pipe_builder.h:100-109`，Phase 枚举被忽略），(2) 阶段间无 Payload 传播（`PipeNode` 是独立 KV 存储），且没有任何插件写 PC。

soc-cpu-l1-mmu-demo change 已删除（依赖此修复）。当前 `cpu-pipeline-stubs-replace` change 优先级最高，**前置修复**才能让后续 SoC demo 真正执行 RISC-V ELF。

## Goals / Non-Goals

### Goals

1. **修复 `PipeBuilder::run()` 调度**：按"阶段首次出现序 × Phase(EARLY→NORMAL→LATE)"执行闭包
2. **新增 `StageLinkPlugin`**：4 个传播闭包复制 stage 间 Payload Keys
3. **替换 IBusPlugin stub**：构造注入 `PicolibcHostMemory*`，真实取指
4. **替换 DBusPlugin stub**：同一注入指针做真实访存
5. **LSU 职责归位**：删除 `lsu.h:66` 的 `MEM_DATA=0` stub（LSU 仅做 AGU，DBus 写 MEM_DATA）
6. **IBusPlugin PC 更新**：`at_stage("writeback",Phase::LATE)` 读 `DECODE.branch_taken` 写新 PC
7. **CpuFactory 加可选 `PicolibcHostMemory*` 参数**：默认 null = 零回归
8. **`cpu_sim` 删除软件解释器**（`main.cpp:176-213`），注入 mem + pin isa=rv32i
9. **新增 2 test cases**：StageLinkPlugin propagation smoke + cpu_sim real ELF tohost
10. **验证 baseline 306 → 308 PASS**（零回归）

### Non-Goals

1. CacheTLM/MemoryTLM 桥接到 CPU（SoC 级 ELF→MemoryTLM 是后续 change）
2. BranchPredictor 真实 redirect（保持 stub 无害——单 pass 语义下无 mispredict）
3. HazardPlugin 真实 stall（同上）
4. RV32I 完整指令回归（仅验证基础 add/addi/lw/sw/beq；MEM_SIZE/JALR/SRA 等推迟）
5. 真实 ISS（Spike/riscvOVPSim）外部 oracle
6. 多拍流水线（pipeline register + 真实 stall）；单 pass 语义足够 demo

## Decisions

### Decision 1: `PipeBuilder::run()` 调度改造

**选择**: 改 `pipe_builder.h::run()`，按 (stage_first_occurrence_order) × (Phase order) 执行闭包。`TopologyBuilder::expand` 在 `pb->build()` 前调用（`cpu_factory.h:245-261`），阶段首现序已正确（fetch→decode→execute→memory→writeback），**无需改 factory 顺序**。

**替代方案**:
- A. 保持线性 + 让每个插件按"被读取时"的阶段重新注册 → 破坏现有插件结构，11 个文件改动
- B. 改 PipelineBuilder API 让用户在 build_cpu 时显式给阶段顺序 → 偏离 VexRiscv 灵感模式
- C. 不改调度，依赖 StageLinkPlugin 自己调度 → StageLinkPlugin 跨 4 个阶段传播，需要按特定顺序注册才能工作，脆弱

**理由**: `TopologyBuilder::expand` 已经先注册 topology 阶段 markers（`pipe_builder.h:114-128`），`build()` 中所有 plugin build 调用前 stages_ 已经有 5 个 stage markers。按插入序遍历 stages_ 天然按阶段序（markers 先插入，plugin 后插入），改 run() 仅需把当前线性回调切换为按阶段+相位排序。**最小侵入**（~15 LOC），保持所有现有 plugin 接口不变。

### Decision 2: StageLinkPlugin 用单 plugin 4 闭包

**选择**: 单个 `StageLinkPlugin` Plugin 类，4 个 `at_stage(X, EARLY)` 闭包分别处理 fetch→decode、decode→execute、execute→memory、memory→writeback。

**替代方案**:
- A. 4 个独立 link plugin → 注册顺序与拓扑顺序耦合，脆弱
- B. 把传播逻辑分散到每个业务 plugin 的 EARLY phase → 11 个 plugin 都改，污染严重

**理由**: 阶段+相位有序调度保证 4 个 link 闭包在对应阶段所有业务闭包之前运行。单 plugin 内部 closures 按 registration order 串行（plugin build 内顺序由开发者控制），与 4 个 link 阶段一一对应。**单文件 ~80 LOC**，新增 1 plugin。

### Decision 3: IBusPlugin 注入 PicolibcHostMemory*（具体指针非抽象接口）

**选择**: 构造 `IBusPlugin(PicolibcHostMemory* mem = nullptr)`，在 `at_stage("fetch", NORMAL)` 闭包内 `mem_->read_word(pc)` 写 INSTRUCTION。

**替代方案**:
- A. 抽象 `MemoryPort` 接口（read_word/write_word）→ 未来 CacheTLM 适配可实现；现 0 CacheTLM 用户 = 提前抽象
- B. 直接用 cpptlm CpuTLM（已存在，但只是合成流量发生器，非 ISS）→ 不真跑 ELF
- C. 新增 InstructionStreamPlugin 持有内存 → 职责劈裂，IBus 仍是 stub

**理由**: `PicolibcHostMemory` 已在 `ip/cpu/`（`picolibc_host_memory.h:34` tohost@0x0, kTohostAddr=0x0），零新耦合。具体指针简单（~5 LOC header change），默认 null 向后兼容 21 个 [cache] + 33 个 [cpu-integration] tests。未来若引入 CacheTLM 桥接，再抽 `MemoryPort` 接口（成本<10 LOC）。**避免 premature abstraction**。

### Decision 4: PicolibcHostMemory 选择（不引入真实 MemoryTLM）

**选择**: 复用现有 `ip/cpu/picolibc_host_memory.h`，在 `cpu_sim` 单进程内跑 ELF。

**替代方案**:
- A. 引入 cpptlm::MemoryTLM + 新 CPUTLMBridge → 跨域（PipeBuilder↔EventQueue），RETRY 语义缺失（`mmu-cache-integration` Decision Q7 已 defer）
- B. 新建 ip/memory/ 子系统 → 0 LOC 起手，>500 LOC 工作量

**理由**: `PicolibcHostMemory` 已有 ELF loader（`load_elf_text`）、tohost 检测（`check_tohost` 写字节时触发）、64KB RAM + `kTohostAddr=0x0`——**完整 ELF→tohost 链路已存在**。无需 CppTLM EventQueue Bridge，无需 RETRY。**最小路径**。

### Decision 5: IBusPlugin 持有 PC 更新（不新建 PCUpdatePlugin）

**选择**: IBusPlugin 增加第 2 个 `at_stage("writeback", LATE)` 闭包，读传播来的 `DECODE.branch_taken`/`branch_target`，写 fetch 节点 `PC`。

**替代方案**:
- A. 新 PCUpdatePlugin → 职责清晰但增加 1 个 plugin
- B. RiscvBranchPlugin 写 PC → BranchPlugin 是 RISC-V ISA 关注点，不该管 Pipeline
- C. CpuFactory::run() 末尾统一调度 PC 更新 → 框架层改动

**理由**: IBusPlugin 是 fetch 阶段 owner，自然也是 PC 寄存器 owner。VexRiscv paradigm：fetch plugin 同时管取指 + PC 更新。**单 plugin 加 1 个闭包**（~15 LOC），职责仍然聚焦。**D4: PASS**（闭包内纯声明式数据搬运）。

### Decision 6: `cpu_sim` 删除软件解释器

**选择**: 删 `main.cpp:176-213` 的 RV32I mini-interpreter，依赖真实 CPU pipeline 执行。

**替代方案**:
- A. 保留解释器作为 fallback → 代码冗余，真伪并存导致测试不清晰
- B. 解释器作为 CPU pipeline 的"参考 oracle"→ 复杂，且现在 CPU pipeline 真实执行后解释器多余

**理由**: 解释器存在前提是 CPU pipeline 是 stub，stub 修复后 CPU pipeline 真实执行——解释器是欺骗性 demo。**删 50 LOC，真实 demo**。原解释器只覆盖 ADDI/ADD/SW/JAL，CPU pipeline 真实执行后所有 RV32I 都能跑。

### Decision 7: Demo ISA pin 到 RV32I

**选择**: `cfg.isa = "rv32i"` 显式 pin。

**替代方案**:
- A. 保留默认 rv64gc → `CpuFactory<uint32_t>` 实例化与 config 类型不匹配，潜在位宽 bug
- B. 实例化 `CpuFactory<uint64_t>` → 模板改大，DataPath 重做

**理由**: 当前 `cpu_sim` 用 `CpuFactory<uint32_t>`（`main.cpp:131`）但默认 config `isa="rv64gc"`（`cpu_factory.h:57`）——既有不一致。Demo 阶段 RV32I 足够（add.S 是 RV32）。**修不一致风险，pin RV32I**。

### Decision 8: Scope 限定 Minimal

**选择**: 不做 Standard（MEM_SIZE/JALR/RV32I 全指令回归）/ Full（BranchPredictor 真实化 + CacheTLM 桥接）。

**替代方案**:
- A. Standard → +200 LOC 风险收益不匹配
- B. Full → 引入 cpptlm 跨域桥接，超出本 change 能力

**理由**: Minimal（5-6 commits ~450 LOC）已能 demo 真 RISC-V 程序跑到 tohost。Standard/Full 是 demo 成功后下一个 change 的范围。**避免 scope 膨胀**。

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **`run()` 排序变化破坏既有 11 个 framework[pipe_builder] 测试 + cpu 集成测试** | 高 | 先单独提交 Layer 0 + 跑全量 ctest；按需调整断言（语义升级，非回归） |
| **`stage_count()==18` byte-identical 断言（`test_5stage_riscv.cpp:48`）被 StageLinkPlugin +4 闭包打破** | 中 | StageLinkPlugin 注册 4 个闭包 → stages_.size() 从 18 变 22，同步更新断言 |
| **tohost@0x0 与 .text@0x0 冲突（`picolibc_host_memory.h:34`）** | 中 | 现有 add.S 测试 ELF loader 用 base_addr 偏移（`main.cpp:151`），.text 不会占 0x0；保留 SW 到 0x0 写 tohost 能力 |
| **`RiscvLsuPlugin` 删 `MEM_DATA=0` 行破坏既有 lsu 单元测试** | 低 | grep 现有 lsu 单测：若都依赖 LSU 写 MEM_DATA=0，更新为 DBus 写 MEM_DATA 断言 |
| **`PC = branch_taken ? branch_target : pc+4` 对 JAL/JALR 不准确** | 低 | `RiscvBranchPlugin::branch.h:52-100` 写 RD = pc+4 (link)，跳转指令 target 由 branch_target 给定；writeback 阶段统一处理足够 |
| **`PicolibcHostMemory::check_tohost` 是 `val & 0xFF`** | 低 | 现有 add.S 测试通过，说明该 tohost 协议工作 |

## Migration Plan

### Phase 1: Layer 0 — 调度与传播（前置，必须）

1. **commit A**: `pipe_builder.h::run()` 阶段+相位有序 + 跑全量 ctest + 调整 stage_count 断言（~30 LOC）
2. **commit B**: 新 `ip/cpu/plugins/stage_link.{h,cpp}` + 测试 propagation smoke（~120 LOC）

### Phase 2: Layer 1+3 — IBusPlugin 修复 + PC 更新

3. **commit C**: IBusPlugin 注入 `PicolibcHostMemory*` + fetch + writeback LATE PC 更新（~50 LOC）

### Phase 3: Layer 2 — DBusPlugin 修复 + LSU 职责归位

4. **commit D**: DBusPlugin 注入 + 真实访存 + LSU 删 `MEM_DATA=0` stub（~40 LOC）

### Phase 4: Layer 7 — cpu_sim 改造 + delete 解释器

5. **commit E**: CpuFactory 加可选 mem 参数 + cpu_sim 注入 mem + 删解释器 + pin isa=rv32i（~50 LOC 调整净减 ~30）

### Phase 5: 测试 + 最终验证

6. **commit F**: `test_5stage_riscv.cpp` 新增 real ELF tohost case + 4 gates 验证 + archive

### 回滚策略

- commit A 单独可回滚：恢复线性插入序（~10 LOC 改动）
- commit B 单独可回滚：删除 StageLinkPlugin（pipeline 无传播但仍不崩）
- commit C-F 链式回滚：去掉 mem 参数 + 恢复 NOP stub 行为

## Open Questions

1. **`PicolibcHostMemory::check_tohost` 边界**: 当前 `val & 0xFF`（`ip/cpu/picolibc_host_memory.h:65` 在 `write_word` 内），要求写入值低字节 = 1。若 add.S 写 `tohost=1`（含 `& 0xFF = 1`），OK；若 `tohost=0x100000001`（高位非零），当前实现会漏判。**待 cpu_sim 验证**。
2. **`PipeBuilder::run()` 排序变化对既有测试影响**: framework[pipe_builder] 11 个测试可能断言 `stages_.size()` 或闭包执行顺序；`test_*stage_riscv.cpp` 断言 `stage_count()`。**需 commit A 落定后立即跑全量 ctest**。
3. **真实 ELF 跑通后下一 change**: `cache-dse-sweep` + `cpu-mmu-integration` 复检（应 PASS 更多 stage tests）；`soc-cpu-l1-mmu-demo` 重启探索（这次 CPU 真实执行，SoC JSON demo 可扩展）。