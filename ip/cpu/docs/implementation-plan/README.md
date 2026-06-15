# ip/cpu 总体实施规划 (Implementation Plan Overview)

> **本文件位置**: `ip/cpu/docs/implementation-plan/README.md`
> **作用**: CPU 实施 M1-M5 **总览**, 阶段间依赖、范围/不在范围、议题 1-8 的"为什么这样选"论证。
> **状态**: 🟢 Accepted (从 `cpu_implementation_guide_v2.0.md` §4, 9-15, 18-22 拆分而来, 内容未改)
> **版本**: v2.0 (2026-06-15 决策)

> **本文件不包含**静态架构、Plugin 套件、cf_plugin 扩展点的技术细节。 这些见 [`../blueprint.md`](../blueprint.md)。
> **本文件不包含**任务级 PASS/FAIL 状态。 见 [`../status.md`](../status.md)。
> **决策快照** 见 [`../cpu_implementation_guide_v2.0.md`](../cpu_implementation_guide_v2.0.md)。

---

## 1. 实施范围

| 项 | 范围 | 备注 |
|----|------|------|
| **ISA** | RISC-V RV32I/RV64I + M + Zicsr + Zifencei | 5 个扩展, 11 个 Plugin 套件 |
| **流水线** | 3 级 (embedded) / 5 级 (default) | 7 级超标量 Phase 5+ |
| **IP 形态** | TLM_ONLY (ImplMode::TLM) | RTL/COMPARE Phase 5+ |
| **多 ISA 集成** | 仅 RISC-V, 但目录接口预留 ARM | 不创建 arch/arm/ 目录 |
| **联调** | CPU + L1CachePlugin + picolibc 内存 | 绕过 MemoryTLM stub |
| **CPU 架构 DSE** | 改 JSON 配置切换 3/5 级流水 | 为以后 DSE 工具链打基础 |
| **验证** | 单元测试 + build_cpu 集成 + 手工编译 ELF | 不引入 riscv-tests 工具链 |

## 2. 不在范围 (明确推迟)

| 项 | 推迟到 | 理由 |
|----|--------|------|
| FPU (F/D 扩展) | Phase 5+ | 单精度/双精度浮点, 需要 FPU Plugin, 复杂度高 |
| Vector (V 扩展) | Phase 6+ | 向量寄存器 + lane 操作, 需重新设计 Payload |
| Multi-core | Phase 6+ | 多核一致性 + Cache coherence, 需大量新增 |
| 7 级超标量 | Phase 5+ | 7 级流水 + 重命名/发射/ROB 复杂, DSE 阶段 |
| RTL_ONLY / COMPARE | Phase 5+ | 本期末仅 TLM, Phase 5 实施 CppHDL RTL |
| riscv-tests 工具链 | Phase 5+ | 依赖完整 CPU + 工具链, 本期不引入 |
| Spike / sail_riscv 对比 | Phase 5+ | 行为验证, 本期不引入 |
| MemoryTLM | Phase 2.5+ | 当前 stub, 联调阶段 picolibc 绕过 |
| arch/arm/ 目录 | Phase 5+ | 本期仅 RISC-V, 目录结构预留 |
| BundleMapper (POD↔ch_uint) | Phase 5+ | RTL 阶段才需, 推迟 |

---

## 3. 议题 1-8 实施层决策 (为什么这样选)

> 议题的"决议状态 (F1-F6)" 见 [`../cpu_implementation_guide_v2.0.md`](../cpu_implementation_guide_v2.0.md) §3。
> 这里是**实施层细节**: 每个议题的"为什么选 A/B/C"。

### 3.1 Plugin 拆分粒度 (议题 2 选 B)

#### 3.1.1 5 个核心 P0 优先实施, 6 个 P1+ 推迟

| 优先级 | Plugin | 实施时机 |
|--------|--------|----------|
| **P0 (本期 M3 必做)** | RiscvDecodePlugin, RiscvIntAluPlugin | M3 |
| P0 (本期 M2 必做) | RegFilePlugin, HazardPlugin, IBusPlugin, DBusPlugin | M2 |
| P1 (本期 M3 必做) | RiscvBranchPlugin, RiscvLsuPlugin, RiscvMulPlugin, BranchPredictorPlugin | M3 |
| P2 (本期 M3 stub) | RiscvCsrPlugin | M3 (目录 + .h 占位, .cpp 写 `// TODO: M3+`) |
| P3+ (Phase 5+ 推迟) | RiscvFpuPlugin, MmuPlugin, ExceptionPlugin | 暂不创建 |

#### 3.1.2 推迟的 Plugin 占位策略

- **目录占位**: 所有 Plugin 都有 `ip/cpu/plugins/*.h` (P0/P1/P2/P3 都创建)
- **实现占位**: P2/P3 仅有 `.h` 头文件声明, `.cpp` 写 `// TODO: M3+` 或 `throw std::runtime_error("Plugin not implemented");`
- **不在 CpuFactory 注册**: P2/P3 Plugin 不在 `build_cpu()` 中注册, 用户配置 `ext_zicsr=true` 也不会生效 (本期)

#### 3.1.3 推迟 Plugin 的 ADR

需要在 `.omo/drafts/` 起草 ADR-XXX (Plugin 推迟决策):

- ADR 内容: 为什么 RiscvFpuPlugin / MmuPlugin / ExceptionPlugin 推迟, 推迟到 Phase 5+ 的具体条件
- 引用: multi_isa v2.0 §1.1 项目目标 (本期仅 RV32I/RV64I + M + Zicsr + Zifencei)
- 范围: 推迟的 Plugin 在 `ip/cpu/plugins/fpu.h` 中声明, 但 `.cpp` 写明推迟原因

### 3.2 RegFilePlugin array_store 抽象 (议题 3 选 B+C)

#### 3.2.1 议题 3 注释: "B 和 C 是一个东西"

用户指出: **选项 B (引用 L1CachePlugin 的 array_store) 和 选项 C (复用 cf_plugin storage.h)** 是同一物。 这是 Phase 1.4 1.4 §2.4 决策: cf_plugin Phase 0 已有 `include/cf/plugin/storage.h` 提供 `array_store<T,N>`, L1CachePlugin `ip/cache/tlm/L1CachePlugin.h:147-149` 引用 cf_plugin storage.h, 这是 L1CachePlugin 6 维度方法学 D1 + D2 共同支持的"复用现有抽象"。

#### 3.2.2 RegFilePlugin 物理实现 (摘要)

具体代码见 `ip/cpu/plugins/reg_file.h` (M2 实施时落地)。要点:

- 模板参数 `<typename T, size_t N>`: T = xlen 类型 (uint32_t / uint64_t), N = 32 (x0-x31)
- 用 `cf::plugin::array_store<T, N>` (来自 `cf_plugin/storage.h`)
- x0 写屏蔽: 写时 `if (idx != 0) store[idx] = value;`
- 读透明: 读 x0 返回 0

### 3.3 JSON 字段名 (议题 4 选 B)

采用 multi_isa v2.0 §6.1 标准字段 (而非沿用旧字段)。**理由**: 与权威设计 1:1 对齐, 避免字段混乱。

具体字段定义见 multi_isa_architecture.md v2.0 §6.1, 实施时在 M4 修订 `configs/cpu_default.json` / `cpu_embedded.json` / `cpu_params_schema.json`。

### 3.4 Plugin 注册顺序 (议题 5 选 B)

**CpuFactory 是 PluginOrder 单一真相源**:
- 同 Phase 内顺序 = CpuFactory 注册顺序
- 跨 Phase 顺序 = EARLY → NORMAL → LATE (由 PipeBuilder 强制)
- JSON `plugins[]` 数组**不控制顺序**, 仅决定"哪些 Plugin 被实例化"

具体实施见 [`../blueprint.md` §5](../blueprint.md)。

### 3.5 联调路径: picolibc 绕过 MemoryTLM (议题 6 选 C)

**为什么选 C**:
- 选项 A (实施 MemoryTLM): Phase 2.5+ 工作, 本期范围外
- 选项 B (L1Cache 2KB 限制): 限制太死, 无法做完整测试
- **选项 C (picolibc 内存区域绕过)**: 用 64KB 静态 RAM + picolibc 库直接编进 ELF, 不需 MemoryTLM

**实施方式**:
- M4 阶段: 新建 `PicolibcHostMemory` 模块 (64KB 静态 RAM)
- picolibc 提供 `tohost` 机制: 测试程序把 1 写到 `tohost` 地址即 PASS
- 手工编译小 ELF (add.S / sub.S / etc), 远 < 64KB

### 3.6 ISA 切换机制: 仅 riscv (议题 7 选 A)

- **不创建 arch/arm/ 目录**: 本期不实施 ARM, 避免空目录噪音
- **CpuFactory 接口预留**: `if (isa == "riscv") { ... } else if (isa == "arm") { ... }` 留 else 分支
- **目录接口预留**: `arch/` 目录可放 `arch/riscv/` 或未来 `arch/arm/`, 接口一致

### 3.7 验证范围: build_cpu + 手工 ELF (议题 8 选 B)

- **不引入 riscv-tests**: 工具链复杂度高, 本期不引入
- **build_cpu() 跑通**: CpuFactory.build_cpu() 接受 JSON 配置返回可用 PipeBuilder
- **手工编译最小 ELF**: `add.S` + `link.ld` + 编译脚本, 远 < 64KB
- **tohost 机制**: 测试程序写 1 到 tohost, 仿真器读出即 PASS

---

## 4. 三级测试金字塔 (复用 multi_isa §8)

### 4.1 Level A — Plugin 单元测试 (本期必做)

**目标**: 验证单个 Plugin 在最小流水线 (1~2 个 Node) 内的行为正确性。

```cpp
// arch/riscv/tests/test_int_alu.cpp

TEST(RiscvIntAlu, AddBasic) {
  cf::plugin::PipeBuilder pb(cf::plugin::ImplMode::TLM);
  auto* ex = pb.create_node("EX");
  pb.bind_logic_stage("execute", ex);

  pb.register_plugin(std::make_unique<RiscvIntAluPlugin>());
  pb.build();

  DecodePayload d{};
  d.op_class = OpClass::ALU;
  d.writes_rd = true;
  RiscvDecodeDetail rd{};
  rd.funct3 = 0;  // ADD
  rd.funct7 = 0;
  (*ex)(pl::DECODE) = d;
  (*ex)(pl::RISCV_DETAIL) = rd;
  (*ex)(pl::RS1) = 3;
  (*ex)(pl::RS2) = 4;
  ex->arb().valid = true;
  ex->arb().ready = true;

  pb.tick();
  EXPECT_EQ((*ex)(pl::RESULT), 7u);
}
```

**覆盖率目标**: 11 个 Plugin × 单指令验证 (4-6 用例/Plugin) = 50-66 单元测试。

### 4.2 Level B — 集成测试 (CPU 模块完成时)

```cpp
// tests/integration/test_5stage_riscv.cpp
// 议题 8 选 B: 手工编译最小 ELF

TEST(Integration, RV32I_5Stage_Add) {
  auto config = load_json("ip/cpu/configs/cpu_default.json");
  auto pb = cf::cpu::CpuFactory::build_cpu(config);
  load_elf_to_picolibc(*pb, "tests/manual_elf/add.elf");
  run_until_tohost(*pb);
  EXPECT_EQ(read_tohost(*pb), 1u);  // PASS
}

TEST(Integration, RV32I_3Stage_Add) {
  auto config = load_json("ip/cpu/configs/cpu_embedded.json");
  auto pb = cf::cpu::CpuFactory::build_cpu(config);
  load_elf_to_picolibc(*pb, "tests/manual_elf/add.elf");
  run_until_tohost(*pb);
  EXPECT_EQ(read_tohost(*pb), 1u);  // PASS
}
```

### 4.3 Level C — COMPARE 模式 (Phase 5+ 推迟)

- TLM_ONLY (本期) → 推迟 RTL_ONLY/COMPARE 到 Phase 5
- multi_isa v2.0 §8.4 描述完整, 但 Phase 5 才实施

---

## 5. 风险与边界

### 5.1 复用 L1CachePlugin 6 维度教训 (从 Phase 1.4 复盘)

| B2 摩擦 (来自 L1Cache) | CPU 端预防策略 |
|----------------------|---------------|
| helper API 内部泄漏 (7 个 public helper) | **friend class 隔离** (L1Cache 1.4 §2 模式) |
| array_store 抽象不完整 | 议题 3 选 B+C: 复用 cf_plugin storage.h 抽象 |
| 早返陷阱 (at_stage lambda 内 if 早返) | 统一约定 + lessons 文档化 |
| Payload Key 数量限制 | 双 Payload (通用 + ISA 特有) |

### 5.2 CPU 特有风险

| 风险 | 缓解 |
|------|------|
| Plugin 数量多 (11 vs L1Cache 1) → 调度顺序确定性 | 议题 5 选 B: CpuFactory 集中管理 |
| 流水线深度可配置 (3/5/7) → Plugin 跨深度复用 | 用 logical_stage 名 (multi_isa v2.0 §3) |
| RegFilePlugin 多 xlen (32/64) | 模板参数化, 编译期选 |
| MulPlugin 3 级子流水 vs 5 级合并 | declare_substage (议题 1 选 C 扩展 cf_plugin) |
| CsrPlugin 270+ CSR 字段 | 表格驱动, 不在 at_stage 闭包内 if/else |
| picolibc 内存 64KB 限制 | 手工编译小 ELF (议题 8 选 B) |
| cf_plugin 缺 at_stage / declare_substage | 议题 1 选 C: 复用 + 扩展 (见 blueprint §6.2.1) |
| cf_plugin 缺 PipeLink (StageLink / DirectLink) | 议题 1 选 C: 复用 + 扩展 (见 blueprint §6.2.2) |

### 5.3 VexRiscv 解决思路风险 (议题 1 备注)

- 借鉴思路但**不**直接移植 → 自主实现 C++ 17 抽象
- 避免 Scala 宏生成 → 用 JSON 表驱动 (更简单)
- 避免 SpinalHDL 依赖 → 纯 CppTLM + C++ 17

---

## 6. 实施里程碑 (M1-M5 总览)

> **详细任务清单** 见各 M 文件: [`M1-cpu-skeleton.md`](M1-cpu-skeleton.md), [`M2-core-plugins.md`](M2-core-plugins.md), [`M3-riscv-plugins.md`](M3-riscv-plugins.md), [`M4-integration.md`](M4-integration.md), [`M5-verification.md`](M5-verification.md)。
> **任务状态** (PASS/FAIL/进度) 见 [`../status.md`](../status.md)。

| 阶段 | 内容 | 验收 | 估时 |
|------|------|------|------|
| **M1** | **核心框架层** (议题 1 选 C: 复用 cf_plugin + 扩展)<br>• 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` / `declare_substage`<br>• 新增 `cf::plugin::PipeLink` (StageLink / DirectLink)<br>• 新增 `cf::plugin::PipeArbitration`<br>• 新增 `ip/cpu/core/payload_common.h` (DecodePayload + 通用 Key) | 4/4 框架级单元测试 PASS<br>• test_pipe_node<br>• test_pipe_builder<br>• test_payload<br>• test_ctrl_link | **3-4 d** |
| **M2** | **ISA 无关 Plugin** (议题 2 选 B: 5 个核心 P0)<br>• `ip/cpu/plugins/reg_file.h` (议题 3 选 B+C: array_store)<br>• `ip/cpu/plugins/hazard.h`<br>• `ip/cpu/plugins/branch_predictor.h` (P1)<br>• `ip/cpu/plugins/ibus.h`<br>• `ip/cpu/plugins/dbus.h`<br>• `ip/cpu/plugins/fpu.h` (P3+ 占位)<br>• `ip/cpu/plugins/mmu.h` (P3+ 占位)<br>• `ip/cpu/plugins/exception.h` (P3+ 占位) | 5/5 P0 单元测试 PASS<br>• P3+ 占位 (.h 声明, .cpp TODO) | **2-3 d** |
| **M3** | **RISC-V ISA 特有 Plugin** (议题 2 选 B: 6 个 P0/P1/P2)<br>• `arch/riscv/decoder_table.h` (P0)<br>• `arch/riscv/payload_riscv.h` (P0)<br>• `arch/riscv/decode.h` (P0)<br>• `arch/riscv/int_alu.h` (P0)<br>• `arch/riscv/mul.h` (P1)<br>• `arch/riscv/branch.h` (P1)<br>• `arch/riscv/lsu.h` (P1)<br>• `arch/riscv/csr.h` (P2 stub)<br>• `arch/riscv/fpu.h` (P3+ 占位) | 6 × 单元测试 PASS<br>• RV32I 译码正确性<br>• RV32I 整数运算 | **4-5 d** |
| **M4** | **CpuFactory + JSON 配置 + 集成测试** (议题 4 选 B + 议题 5 选 B + 议题 8 选 B)<br>• `ip/cpu/cpu_factory.h` (集中 PluginOrder)<br>• 修订 `configs/cpu_default.json` (multi_isa v2.0 §6.1 字段)<br>• 修订 `configs/cpu_embedded.json`<br>• 新增 `configs/cpu_params_schema.json` (修订版)<br>• `tests/integration/test_5stage_riscv.cpp`<br>• `tests/integration/test_3stage_riscv.cpp`<br>• `tests/manual_elf/add.S` + `link.ld` + 编译脚本<br>• 议题 6 选 C: `PicolibcHostMemory` 静态 RAM | build_cpu() 跑通<br>• 5 级 + 3 级 end-to-end 跑通 `add.elf`<br>• tohost = 1 (PASS) | **2-3 d** |
| **M5** | **联调 + 文档收尾** (议题 6 选 C)<br>• CPU + L1CachePlugin + PicolibcHostMemory 联调<br>• 跑通 4-6 个基础 RISC-V ELF (add/sub/and/or/sll/srli)<br>• 修订 `ip/cpu/README.md` + `ip/cpu/docs/README.md`<br>• ADR-XXX: Plugin 推迟决策<br>• `git commit` + tag | 4-6 ELF 全 PASS<br>• 16/16 ctest 不退化<br>• D4 + ADR-040 3+4/3 PASS | **1-2 d** |
| **总计** | | | **12-17 d** |

### 6.1 阶段间依赖

```
M1 (框架层) ──► M2 (ISA 无关 Plugin) ──► M3 (RISC-V Plugin) ──► M4 (集成) ──► M5 (联调)
                                       │
                                       └──► M4 (CpuFactory 集成 M2+M3)
```

---

## 7. 联调路径 (M5 实施)

### 7.1 联调前置 (M5 启动时)

- ✅ L1CachePlugin (已落地, 4/4 单元测试, Phase 1.2)
- ✅ CpuFactory (M4 实施)
- ✅ 11 个 RISC-V Plugin (M2 + M3 实施)
- ✅ PicolibcHostMemory (议题 6 选 C: 绕过 MemoryTLM stub)
- ❌ MemoryTLM (当前 stub, 推迟 Phase 2.5+)

### 7.2 联调架构

```
soc/cpu_l1_picolibc demo.json
├── modules:
│   ├── cpu      (build_cpu(cpus/cpu_default.json, TLM))
│   ├── l1       (L1CacheTLMBridge + L1CachePlugin, 256 sets × 8B/line)
│   └── host_mem (PicolibcHostMemory, 64KB 静态 RAM)
└── connections:
    ├── cpu.IBus.req → l1.req_in
    ├── l1.req_out → cpu.IBus.resp
    ├── cpu.DBus.req → l1.req_in
    ├── l1 → host_mem (on miss)
    └── host_mem → l1 (resp)
```

### 7.3 联调验证 (M5 验收)

| 测试 ELF | 验证指令 | 期望结果 |
|----------|----------|---------|
| add.elf | ADD (RV32I) | tohost=1 PASS |
| sub.elf | SUB (RV32I) | tohost=1 PASS |
| and.elf | AND (RV32I) | tohost=1 PASS |
| or.elf | OR (RV32I) | tohost=1 PASS |
| sll.elf | SLL (RV32I) | tohost=1 PASS |
| srli.elf | SRLI (RV32I) | tohost=1 PASS |
| mul.elf | MUL (RV32M) | tohost=1 PASS |

### 7.4 不在 M5 范围 (推迟)

- riscv-tests / Spike Diff / RISCOF (Phase 5+)
- MemoryTLM 实施 (Phase 2.5+)
- RTL 协同验证 (Phase 5+)
- Multi-core (Phase 6+)

---

## 8. M1 启动前置检查清单 (用户授权后)

> **当前状态 (2026-06-15)**: v2.0 文档已交付, 用户已接受 F1-F6 + 议题 1-8。 等待 git commit 授权 + 启动 M1。

| 项 | 状态 |
|----|------|
| **v2.0 文档** 用户授权 commit | 🟡 待用户授权 |
| **git tag** `phase-1.5-cpu-v2.0-baseline-2026-06-15` | 🟡 待用户授权 |
| **8 文件 staged** (v2.0 doc + 修订的 cf_plugin 扩展点) | 🟡 待 M1 启动前 |

### 8.1 M1 启动步骤 (用户授权后)

1. **git commit** v2.0 文档 + tag `phase-1.5-cpu-v2.0-baseline-2026-06-15`
2. **M1 启动**: 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` / `declare_substage` (议题 1 选 C)
3. **M1 验证**: 4/4 框架级单元测试 PASS
4. **进入 M2**: ISA 无关 5 个 Plugin
5. **进入 M3**: RISC-V ISA 6 个 Plugin
6. **进入 M4**: CpuFactory + JSON + 集成测试
7. **进入 M5**: 联调 + 文档收尾 + 最终 commit

---

## 相关文档

- **静态架构蓝图**: [`../blueprint.md`](../blueprint.md)
- **任务状态看板**: [`../status.md`](../status.md)
- **决策入口**: [`../cpu_implementation_guide_v2.0.md`](../cpu_implementation_guide_v2.0.md)
- **M1 详细任务**: [`M1-cpu-skeleton.md`](M1-cpu-skeleton.md)
- **M2 详细任务**: [`M2-core-plugins.md`](M2-core-plugins.md)
- **M3 详细任务**: [`M3-riscv-plugins.md`](M3-riscv-plugins.md)
- **M4 详细任务**: [`M4-integration.md`](M4-integration.md)
- **M5 详细任务**: [`M5-verification.md`](M5-verification.md)
- **权威设计**: [`../multi_isa_architecture.md`](../multi_isa_architecture.md)
