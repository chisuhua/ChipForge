# CPU 架构 DSE 实现方案 (Design Space Exploration)

| 字段 | 值 |
|------|-----|
| 版本号 | 1.0 |
| 日期 | 2026-06-17 |
| 状态 | 🟡 Accepted (待实施, 立项 M4-DSE / M5-DSE 子阶段) |
| 适用范围 | ChipForge IP/CPU 子系统 |
| 父文档 | [`multi_isa_architecture.md`](multi_isa_architecture.md) v2.0 |

> **本文档定位**: 在 `multi_isa_architecture.md` v2.0 §5 (多 ISA) / §6 (可配置流水线) 的**设计意图**基础上,
> 给出**经过事实校核的可执行实现方案**。本文记录:
>
> 1. **当前真实状态** — 哪些已经实现,哪些是空 stub,哪些是死字段
> 2. **可探索的设计维度** — 今天真正可调的旋钮 (✅) vs 文档承诺但未实现 (⚠️) vs 完全不支持 (❌)
> 3. **完整实现路径** — 把 `CpuFactory` 从空壳变成真正 DSE 引擎的代码改动
> 4. **风险与边界** — 哪些维度必须推迟到 Phase 5+
>
> **本文件不修改** `multi_isa_architecture.md` 的设计意图,只在 §11 附录中标注"已落实 / 待落实"状态。

---

## 目录

1. [当前真实状态 (经过审计的事实)](#1-当前真实状态-经过审计的事实)
2. [设计空间维度清单](#2-设计空间维度清单)
3. [ISA 无关性 — 实际隔离机制](#3-isa-无关性--实际隔离机制)
4. [CPUConfig 扩展方案](#4-cpuconfig-扩展方案)
5. [拓扑声明阶段 — 流水线深度真实展开](#5-拓扑声明阶段--流水线深度真实展开)
6. [Plugin 改造策略](#6-plugin-改造策略)
7. [CpuFactory::build_cpu 完整实现](#7-cpufactorybuild_cpu-完整实现)
8. [DSE Sweep 工具 (脚本层)](#8-dse-sweep-工具-脚本层)
9. [实施路线图](#9-实施路线图)
10. [风险与限制](#10-风险与限制)
11. [附录: 与 multi_isa_architecture.md 的对应关系](#11-附录-与-multi_isa_architecturemd-的对应关系)
12. [附录: 关键文件改动清单](#12-附录-关键文件改动清单)

---

## 1. 当前真实状态 (经过审计的事实)

> 本节基于 2026-06-17 对仓库的实证审计,所有结论都附带 file:line 引用。

### 1.1 CpuFactory — **空 Stub, 但形状正确**

`ip/cpu/cpu_factory.h:73-127` 定义 `CpuFactory<T>`,有完整的类型签名,但**实际行为是 no-op**:

```cpp
// cpu_factory.h:96-126 — 实际行为
template <typename U>
static void register_early_plugins(cf::plugin::PipeBuilder& pb,
                                   const CPUConfig& /*config*/) {
  (void)pb; (void)sizeof(U);   // ← 什么都没做
}
// ...register_normal_plugins, register_late_plugins 同理
```

`cpu_factory.cpp` 全文 12 行,只有一句注释,无实现。

**实证**: 全仓库 `grep "make_unique<(Hazard|RegFile|BranchPredictor|IBus|DBus|RiscvDecode|RiscvIntAlu|RiscvMul|RiscvBranch|RiscvLsu|RiscvCsr)Plugin"` 在 `*.cpp` / `*.h` / `*.cc` 中 **零匹配**。所有 `make_unique<...Plugin>` 字符串都出现在 `ip/cpu/docs/*.md` 文档示例中,不在编译代码里。

### 1.2 11 个 Plugin 本身**已实现**,只是没人调

| Plugin | 文件 | 状态 | 备注 |
|---|---|---|---|
| `RegFilePlugin<T>` | `plugins/reg_file.h` + `.cpp` | 🟢 完整 | ⚠️ `.cpp:40` 重定义 `build()` 用 `pl::keys_rv32::` 硬编码,覆盖头文件版本,**真 Bug** |
| `HazardPlugin<T>` | `plugins/hazard.h` | 🟢 完整 | 32 项 scoreboard, RAW/WAW 检测 |
| `BranchPredictorPlugin<T>` | `plugins/branch_predictor.h` + `.cpp` | 🟢 完整 | **所有尺寸 `static constexpr` 硬编码** |
| `IBusPlugin<T>` | `plugins/ibus.h` | 🟡 stub | fetch 阶段返回 NOP `0x00000013` |
| `DBusPlugin<T>` | `plugins/dbus.h` | 🟡 stub | 返回 0 |
| `RiscvDecodePlugin<T>` | `arch/riscv/decode.h` | 🟢 完整 | 调 `decode_rv32()`,填通用 + ISA Payload |
| `RiscvIntAluPlugin<T>` | `arch/riscv/int_alu.h` | 🟢 完整 | 10 RV32I 算术指令 |
| `RiscvMulPlugin<T>` | `arch/riscv/mul.h` | 🟢 完整 (单周期) | 注释承诺 3 级子流水,未实现 |
| `RiscvBranchPlugin<T>` | `arch/riscv/branch.h` | 🟢 完整 | BEQ..BGEU + JAL/JALR |
| `RiscvLsuPlugin<T>` | `arch/riscv/lsu.h` | 🟢 地址生成完整 | 数据通道 stub |
| `RiscvCsrPlugin` | `arch/riscv/csr.h` | 🔴 占位 | 无业务逻辑 |

**关键观察**: 所有 plugin 都实现了 `build(PipeBuilder&)` 方法,内部用 `pb.at_stage("execute", Phase::NORMAL, [this]() { ... })` 注册回调。**这些 `build()` 调用从未被触发**,因为没有生产代码注册 plugin 实例。

### 1.3 单元测试只验证 plugin 内部,不验证流水线

所有 `tests/cpu/test_*.cpp` 用**栈对象**直接调 plugin 的静态方法或成员方法,**不通过 PipeBuilder**:

- `test_int_alu.cpp` 调 `RiscvIntAluPlugin<T>::compute(OpCode::ADD, 3, 4)` — 纯静态方法
- `test_branch.cpp` 调 `RiscvBranchPlugin<T>::evaluate_branch(funct3, rs1, rs2)` — 纯静态方法
- `test_branch_predictor.cpp` 用 `BranchPredictorPlugin<T> bp; bp.update(...)` — 栈对象
- `test_mul.cpp`, `test_lsu.cpp`, `test_reg_file.cpp` 等同样模式

**结论**: 现有 18 个测试**完全没验证** `CpuFactory::build_cpu()` 的正确性,因为 factory 返回的是空 PipeBuilder。

### 1.4 集成测试是**假的**

`tests/cpu/integration/test_demo_soc.cpp:108-121` 字面解析 ELF 二进制,搜 `0x23 0x20 0x50 0x00` (`sw x5, 0(x0)` 指令字节模式),然后**直接调** `mem.write_word(0x0, 1)` 模拟 tohost 写入。注释明确写 "M5 stub: 实际 CPU 跑通推迟 Phase 5+"。

`test_3stage_riscv.cpp` / `test_5stage_riscv.cpp` 全部断言 `pb != nullptr`,不验证流水线行为。

### 1.5 框架层 (`cf_plugin`) 真实可用

`include/cf/plugin/pipe_builder.h` 提供完整 API:

| API | 行号 | 用途 |
|---|---|---|
| `register_plugin(unique_ptr<PluginBase>)` | 64-67 | 接管 plugin 所有权 |
| `at_stage(string, Phase, lambda)` | 69-76 | 注册阶段回调 (单一重载, 只接受字符串) |
| `declare_substage(parent, sub, depth)` | 78-86 | 记录父子映射,**当前 `depth` 参数被注释掉不生效** |
| `node_of_logic_stage(name)` | 88-92 | 查找 stage 对应 PipeNode |
| `build()` | 94-97 | 按顺序调所有 plugin 的 `setup()` → `build()` |
| `run()` | 99-102 | 执行所有 `at_stage` 回调 |

**生产级调用者**: L1CachePlugin (`ip/cache/tlm/L1CachePlugin.cpp:107`) — 是目前唯一调用 `declare_substage` 的生产代码。

### 1.6 一句话现状

> **`CpuFactory` 是一座空城: 框架到位, Plugin 写好, 但城门没打开 — 没有任何人把 Plugin 注册进 PipeBuilder。**

---

## 2. 设计空间维度清单

按"今天可探索"分级:

### 2.1 ✅ 真正可调 (5 个维度)

| # | 维度 | 调谐点 | 现状 |
|---|------|--------|------|
| 1 | **ISA 子集** | `CPUConfig::isa ∈ {rv32i, rv32im, rv32imac, rv64i, rv64gc}` | 字符串字段,需要 CpuFactory 真实 dispatch |
| 2 | **xlen (32/64)** | `CpuFactory<uint32_t>::build_cpu` vs `<uint64_t>` | 编译期模板 |
| 3 | **流水线深度** (3/5/7/10) | `CPUConfig::pipeline_stages ∈ {3, 5, 7, 10}` | 字段存在但未生效 — 本方案修复; 范围由 `cpu_params_schema.json` enum 与 `TopologyBuilder::expand()` switch 联合约束 (Q5 决策) |
| 4 | **分支预测器类型** | `CPUConfig::branch_predictor ∈ {static, bimodal, gshare, tournament}` | 字段存在但未生效 — 本方案修复 |
| 5 | **ISA 扩展开关** | `CPUConfig::ext_m / ext_a / ext_f / ext_d / ext_zicsr` | **新增字段**, 本方案引入 |

### 2.2 ⚠️ 配置存在但代码忽略 (4 个死字段 — 本方案激活)

| # | 字段 | 当前实际行为 | 修复方式 |
|---|------|---------------|---------|
| 6 | `CPUConfig::btb_entries` (16/32/64/128/256) | `BranchPredictorPlugin::kBtbSize=16` 硬编码 constexpr | 改为模板参数 + 5 个显式实例化 |
| 7 | `CPUConfig::mul_latency` (1/3/5) | `RiscvMulPlugin` 单 lambda 单周期 | 模板参数 + `declare_substage` 展开 |
| 8 | `CPUConfig::icache_latency` / `dcache_latency` | `IBusPlugin` / `DBusPlugin` 是 stub | 接受字段并用于延迟模拟 |
| 9 | `CPUConfig::enable_mmu` / `mmu_mode` (sv32/sv39/sv48) | `MMUPlugin` 是空类 | 留接口,本期 stub |

### 2.3 ❌ 当前不支持 (明确推迟到 Phase 5+)

| # | 维度 | 推迟理由 |
|---|------|---------|
| 10 | 乱序执行 (OoO) | 无 ROB / Reservation Station / Register Rename / Wakeup |
| 11 | 超标量 / 多发射 | 无 Issue Queue,无 Dispatch Slot |
| 12 | Load-Store Queue (LSQ) | 无 store buffer,无 store-forwarding |
| 13 | 跨 ISA (RISC-V ↔ ARM/MIPS/x86) | 无 `arch/arm/` 等;Payload Key 静态 assert 卡死 XLEN=32/64 |
| 14 | L2/L3 cache | `ip/memory/` 仅 README,无代码 |
| 15 | 真实 MMU/PMP | 字段保留,行为 stub |
| 16 | FPU (F/D 扩展) | plugin 占位,无业务逻辑 |
| 17 | 多核 / SMT | Phase 6+ |
| 18 | RTL_ONLY / COMPARE | `cf::plugin::ImplMode` 枚举尚未在 PipeBuilder 中实现 |

---

## 3. ISA 无关性 — 实际隔离机制

> **multi_isa_architecture.md §5.2 (ADR-6) 描述的"双 Payload 共存"是真实有效的隔离机制**。本节给出当前已落实的边界。

### 3.1 双重 Payload 架构 (已落实)

每个 PipeNode 同时存两份 Payload:

| Payload | 类型 | 字段 | 谁写 | 谁读 |
|---|---|---|---|---|
| **通用 `DecodePayload`** | `core/payload_common.h:72` | `op_class`, `rs1_idx`, `rd_idx`, `writes_rd`, `reads_rs1/2`, `branch_taken`, `branch_target` | `RiscvDecodePlugin` (未来任何 ISA Decode Plugin) | 所有 ISA 无关 plugin |
| **RISC-V 特有 `RiscvDecodeDetail`** | `arch/riscv/payload_riscv.h:50` | `funct3`, `funct7`, `funct12`, `imm`, `csr_addr` | `RiscvDecodePlugin` | RISC-V 特有 plugin (`RiscvIntAlu`, `RiscvMul`, `RiscvBranch`, `RiscvLsu`, `RiscvCsr`) |

**RISC-V 特有 plugin 与 ISA 无关 plugin 的清晰边界**:

| 类别 | 只读通用 Payload | 也读 RISC-V 特有 Payload |
|---|---|---|
| `RegFilePlugin` | ✅ | ❌ |
| `HazardPlugin` | ✅ | ❌ |
| `BranchPredictorPlugin` | ✅ (只看 PC + branch_taken) | ❌ |
| `IBusPlugin` / `DBusPlugin` | ✅ | ❌ |
| `RiscvDecodePlugin` | 写 | 写 |
| `RiscvIntAluPlugin` | ✅ | ✅ |
| `RiscvMulPlugin` | ✅ | ✅ |
| `RiscvBranchPlugin` | ✅ | ✅ |
| `RiscvLsuPlugin` | ✅ | ✅ |

### 3.2 切换 ISA 的工作量 (新增一条 ISA 需做)

按 `multi_isa_architecture.md §5.4` Checklist 落地:

| # | 任务 | 工作量 | 难度 |
|---|---|---|---|
| 1 | 创建 `ip/cpu/arch/<isa>/` 目录 | 5 min | 极低 |
| 2 | 写 `<isa>_decoder_table.h` (等价于 `arch/riscv/decoder_table.h`) | 1-2 天 | 中 (需理解目标 ISA 编码) |
| 3 | 写 `payload_<isa>.h` (等价于 `arch/riscv/payload_riscv.h`) | 0.5 天 | 低 |
| 4 | 写 `<Isa>DecodePlugin` (调 decoder_table 填双 Payload) | 1 天 | 中 |
| 5 | 写 `<Isa>IntAluPlugin` / `<Isa>BranchPlugin` / `<Isa>LsuPlugin` | 3-5 天 | 高 (需实现 ISA 语义) |
| 6 | (可选) `<Isa>MulPlugin` / `<Isa>FpuPlugin` / `<Isa>CsrPlugin` | 2-5 天 | 中-高 |
| 7 | 在 `CpuFactory::build_cpu` 加 `else if (isa == "<isa>")` 分支 | 0.1 天 | 极低 |
| 8 | 在 `cpu_params_schema.json` 加 ISA 字符串到 enum | 0.05 天 | 极低 |
| 9 | 写 `arch/<isa>/tests/` 单元测试 | 1-2 天 | 中 |

**总计**: 一条新 ISA ≈ **8-15 天** (不含 FPU/CSR/MMU),假定目标 ISA 是定长指令 (类似 RISC-V / MIPS32)。

**本方案不实现**跨 ISA — 仅给出迁移路径。

### 3.3 移除 RISC-V-specific 静态断言 (未来跨 ISA 必需)

`ip/cpu/core/payload_common.h:115,117` 当前静态断言:

```cpp
static_assert(XLEN == 32 || XLEN == 64,
              "XLEN must be 32 or 64 (RISC-V only supports these)");
static_assert(std::is_same<T, std::uint32_t>::value ||
              std::is_same<T, std::uint64_t>::value,
              "T must be uint32_t (RV32) or uint64_t (RV64)");
```

跨 ISA 时需要放宽:**XLEN 由 ISA 字符串决定**,例如 ARMv8 SVE 支持 128/256/512/1024/2048 位。

**本方案保留这两个静态断言**(本期不跨 ISA),但在 DSE 文档里标记为未来工作项。

---

## 4. CPUConfig 扩展方案

### 4.1 现状

`ip/cpu/cpu_factory.h:39-58` 现有 12 字段:

```cpp
struct CPUConfig {
  std::string  name               = "RiscvCpu";
  std::string  isa                = "rv64gc";
  std::uint8_t pipeline_stages    = 5;
  std::uint32_t clock_freq_mhz    = 100;
  bool   enable_pmp = true;
  bool   enable_mmu = true;
  std::string mmu_mode            = "sv39";
  std::string branch_predictor    = "gshare";
  std::uint16_t btb_entries       = 64;
  std::uint8_t  icache_latency    = 1;
  std::uint8_t  dcache_latency    = 1;
};
```

### 4.2 扩展后的 CPUConfig

```cpp
// ip/cpu/cpu_factory.h — 扩展 (向后兼容)
struct CPUConfig {
  // ===== 现有字段 (保持不变, 仅文档化默认值) =====
  std::string  name               = "RiscvCpu";
  std::string  isa                = "rv64gc";      // rv32i/rv32im/rv32imac/rv64i/rv64gc
  std::uint8_t pipeline_stages    = 5;             // 3 / 5 / 7 / 10
  std::uint32_t clock_freq_mhz    = 100;
  bool   enable_pmp = true;
  bool   enable_mmu = true;
  std::string mmu_mode            = "sv39";        // sv32/sv39/sv48
  std::string branch_predictor    = "gshare";      // static/bimodal/gshare/tournament
  std::uint16_t btb_entries       = 64;            // 16/32/64/128/256
  std::uint8_t  icache_latency    = 1;             // 0..32
  std::uint8_t  dcache_latency    = 1;             // 0..32

  // ===== 新增字段 (M4-DSE 子阶段引入) =====
  // 拓扑
  bool   enable_branch_predictor = true;           // 3 级嵌入式可关
  bool   split_if_id = false;                       // 7 级把 fetch 拆 IF1/IF2
  bool   merge_ex_mem = false;                      // 3 级合并 EX+MEM

  // 分支预测
  std::uint8_t ghr_bits = 8;                       // 8 / 16

  // ISA 扩展
  bool   ext_m  = false;                            // M 扩展 (MUL/DIV)
  bool   ext_a  = false;                            // A 扩展 (atomic)
  bool   ext_f  = false;                            // F 扩展 (单精度 FPU)
  bool   ext_d  = false;                            // D 扩展 (双精度 FPU)
  bool   ext_zicsr = false;                         // Zicsr (CSR)
  bool   ext_zifencei = false;                      // Zifencei (fence.i)

  // 功能单元延迟
  std::uint8_t mul_latency = 1;                    // 1 / 3 / 5 cycle(s)

  // Hazard 模式
  bool   use_strict_scoreboard = true;             // true=严格顺序, false=预留

  // 调试 / 统计
  bool   collect_stats = false;
};
```

### 4.3 配套 JSON Schema 更新

`ip/cpu/configs/cpu_params_schema.json` 需要加 12 个新字段的 schema 定义,详见 §12.4。

### 4.3.1 ISA 扩展字段与 `isa` 字符串的关系 (Q1 决策)

> **三种 ISA 扩展开关的等价关系** (2026-06-17 校核):
>
> | 表示法 | 字段位置 | 形式 | 用途 |
> |--------|----------|------|------|
> | 字符串前缀 | `cpu_params_schema.json::isa` | `"rv32imac"` | **唯一权威**,所有 `configs/cpu_*.json` 实例文件使用 |
> | 数组 | `multi_isa_architecture.md §6.1` | `["i", "m", "a", "c"]` | 设计意图视图,运行时归并到 `isa` 字符串 |
> | Boolean flat | `cpu_params_schema.json` (本节新增) | `ext_m=true, ext_a=true, ext_c=true` | DSE 工具笛卡尔积扫描,运行时与 `isa` 字符串互验 |
>
> **一致性规则** (M4.16 实施):
> 1. JSON 解析时,`ext_* = true` 扩展集合必须等于从 `isa` 字符串解析出的扩展集合 (否则 ajv 校验失败,报 "isa/ext_* 不一致")
> 2. 输出 (写回 JSON) 时,以 `isa` 字符串为唯一来源,`ext_*` 字段从 `isa` 字符串派生
> 3. C++ CPUConfig 在内存中只保留 `isa` 字符串 + `ext_*` boolean (两者冗余存储,但提供 `validate_config()` 在 build_cpu 前一致性校验)

### 4.4 派生结构 — DSE Sweep 输入

```cpp
// ip/cpu/cpu_factory.h — 新增 DSE 工具结构
struct DseSpace {
  std::vector<std::string>   pipeline_stages_sweep;   // {"3", "5", "7"}
  std::vector<std::string>   branch_predictor_sweep;  // {"static","gshare"}
  std::vector<std::uint16_t> btb_entries_sweep;      // {16, 64, 256}
  std::vector<bool>          ext_m_sweep;             // {false, true}
  std::vector<std::uint8_t>  mul_latency_sweep;       // {1, 3}
  std::vector<std::string>   isa_sweep;               // {"rv32i","rv64i"}

  // 笛卡尔积展开成 CPUConfig 列表
  std::vector<CPUConfig> expand() const;
};
```

### 4.5 配套 JSON 解析

```cpp
// ip/cpu/cpu_factory.cpp — 新增
CPUConfig parse_config(const std::string& json_text);
bool validate_config(const CPUConfig& cfg);  // 校验数值在 enum 内
```

JSON 子集解析不引入第三方库,手写 ~150 行即可,覆盖 cpu_default.json / cpu_embedded.json / cpu_superscalar.json / cpu_deep_pipeline.json。

---

## 5. 拓扑声明阶段 — 流水线深度真实展开

### 5.1 设计意图 (来自 multi_isa_architecture.md §6.2)

逻辑阶段名 → 物理 Node 映射:

| 配置 | 物理 Node 数 | 映射 |
|------|--------------|------|
| 3 级 (embedded) | 3 | `IF`={fetch,decode}, `EXMEM`={execute,memory}, `WB`={writeback} |
| 5 级 (default) | 5 | `IF`={fetch}, `ID`={decode}, `EX`={execute}, `MEM`={memory}, `WB`={writeback} |
| 7 级 (superscalar) | 7 | `IF1`, `IF2`, `ID`, `RENAME`(无), `ISSUE`(无), `EX`, `MEM`, `RETIRE`(无) |
| 10+ 级 (深流水) | ≥10 | 自由声明, Plugin 通过 `declare_substage()` 进一步分裂 EX |

### 5.2 当前 `declare_substage` 的缺陷

`include/cf/plugin/pipe_builder.h:78-86`:

```cpp
void declare_substage(const std::string& parent, const std::string& sub, int /*depth*/ = 0) {
  substage_parent_[sub] = parent;
  if (nodes_.find(parent) == nodes_.end()) {
    nodes_.emplace(parent, std::make_shared<PipeNode>(parent));
  }
  if (nodes_.find(sub) == nodes_.end()) {
    nodes_.emplace(sub, std::make_shared<PipeNode>(sub));   // ← 总创建新 Node!
  }
}
```

**问题**: 即使 parent 和 sub 应该"共享"一个 Node (3 级模式的 IF={fetch,decode}),`declare_substage("fetch", "decode", 0)` 仍然创建两个独立 Node。`at_stage("fetch", ...)` 和 `at_stage("decode", ...)` 落在不同 Node 上,无法模拟"合并"。

### 5.3 解决方案 — 新增 `merge_stage` API

**框架层最小改动** (3 行):

```cpp
// include/cf/plugin/pipe_builder.h — 新增方法
void merge_stage(const std::string& name, const std::string& parent) {
  if (nodes_.find(parent) == nodes_.end()) {
    nodes_.emplace(parent, std::make_shared<PipeNode>(parent));
  }
  // 关键: name 共享 parent 的 shared_ptr, 而不是创建新 Node
  nodes_[name] = nodes_[parent];
  substage_parent_[name] = parent;
}
```

**用法 (3 级模式)**:

```cpp
// 合并 fetch + decode 到同一 Node
pb.merge_stage("decode", "fetch");
// 合并 execute + memory 到同一 Node
pb.merge_stage("memory", "execute");
```

此时 `node_of_logic_stage("memory") == node_of_logic_stage("execute")` (同一 `shared_ptr`)。

### 5.4 TopologyBuilder — CPU 端实现

```cpp
// ip/cpu/cpu_factory.h — 新增内部类
class TopologyBuilder {
 public:
  explicit TopologyBuilder(cf::plugin::PipeBuilder& pb) : pb_(pb) {}

  // 在 register_plugin 之前调用,根据 config 展开物理 Node
  void expand(const CPUConfig& cfg) {
    switch (cfg.pipeline_stages) {
      case 3:
        // 合并 fetch+decode 到 IF,合并 execute+memory 到 EXMEM
        pb_.merge_stage("decode", "fetch");
        pb_.merge_stage("memory", "execute");
        break;
      case 5:
        // 5 个独立 stage (默认)
        break;
      case 7:
        if (cfg.split_if_id) {
          pb_.declare_substage("fetch", "fetch_p1", 1);
          pb_.declare_substage("fetch_p1", "fetch_p2", 1);
          // fetch_p2 = 最终 fetch 输出
        }
        break;
      case 10:
        // 拆分 EX 为多拍 + 加 MUL 子流水
        pb_.declare_substage("execute", "ex1", 1);
        pb_.declare_substage("ex1", "ex2", 1);
        pb_.declare_substage("ex2", "ex3", 1);
        break;
      default:
        throw std::invalid_argument("pipeline_stages must be 3/5/7/10");
    }
  }

 private:
  cf::plugin::PipeBuilder& pb_;
};
```

### 5.5 测试断言

```cpp
// tests/cpu/integration/test_3stage_riscv.cpp (M4-DSE 升级版)
static void test_3stage_topology() {
  CPUConfig cfg; cfg.pipeline_stages = 3;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  // 3 级模式: memory 和 execute 必须指向同一 Node
  assert(pb->node_of_logic_stage("memory") ==
         pb->node_of_logic_stage("execute"));
  // 3 级模式: decode 和 fetch 必须指向同一 Node
  assert(pb->node_of_logic_stage("decode") ==
         pb->node_of_logic_stage("fetch"));
  printf("  [PASS] test_3stage_topology\n");
}

static void test_5stage_topology() {
  CPUConfig cfg; cfg.pipeline_stages = 5;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  // 5 级模式: 所有 stage 各自独立
  assert(pb->node_of_logic_stage("memory") !=
         pb->node_of_logic_stage("execute"));
  assert(pb->node_of_logic_stage("decode") !=
         pb->node_of_logic_stage("fetch"));
  // 5 个独立 Node
  assert(pb->node_count() == 5);
  printf("  [PASS] test_5stage_topology\n");
}
```

---

## 6. Plugin 改造策略

### 6.1 BranchPredictorPlugin — 模板参数化 (影响最大)

**当前** (硬编码):

```cpp
// branch_predictor.h:61-64
static constexpr std::size_t kBtbSize = 16;
static constexpr std::size_t kBimodalSize = 16;
static constexpr std::size_t kGshareSize = 16;
static constexpr std::uint8_t kHistoryBits = 8;
```

**改造后** (5 个编译期参数):

```cpp
template <typename T,
          std::size_t BTB_SIZE   = 16,
          std::size_t BIMODAL_SZ = 16,
          std::size_t GSHARE_SZ  = 16,
          std::uint8_t GHR_BITS  = 8>
class BranchPredictorPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  static_assert((BTB_SIZE   & (BTB_SIZE   - 1)) == 0, "BTB_SIZE must be power of 2");
  static_assert((BIMODAL_SZ & (BIMODAL_SZ - 1)) == 0, "BIMODAL_SZ must be power of 2");
  static_assert((GSHARE_SZ  & (GSHARE_SZ  - 1)) == 0, "GSHARE_SZ must be power of 2");
  static_assert(GHR_BITS >= 1 && GHR_BITS <= 16, "GHR_BITS in [1,16]");

 public:
  using BtbArray    = std::array<BtbEntry, BTB_SIZE>;
  using CounterArr  = std::array<Counter, BIMODAL_SZ>;

  // 接收 CPUConfig 注入 BP 类型 (static/bimodal/gshare/tournament)
  explicit BranchPredictorPlugin(const CPUConfig& cfg) : cfg_(cfg) {
    bp_type_ = parse_bp_type(cfg.branch_predictor);
    reset();
  }

  static std::unique_ptr<cf::plugin::PluginBase> create(const CPUConfig& cfg) {
    return std::make_unique<BranchPredictorPlugin>(cfg);
  }

  // ...build() 不变, 但内部使用 BTB_SIZE/BIMODAL_SZ/GSHARE_SZ/GHR_BITS 模板参数

 private:
  const CPUConfig& cfg_;  // 由 factory 保证生命周期
  enum class BpType { STATIC, BIMODAL, GSHARE, TOURNAMENT };
  BpType bp_type_ = BpType::GSHARE;
};
```

**5 种显式实例化** (`branch_predictor.cpp`):

```cpp
// 16/32/64/128/256 entry × GHR 8/16 bit × RV32/RV64
template class BranchPredictorPlugin<uint32_t,  16, 16, 16,  8>;
template class BranchPredictorPlugin<uint32_t,  32, 32, 32,  8>;
template class BranchPredictorPlugin<uint32_t,  64, 64, 64,  8>;
template class BranchPredictorPlugin<uint32_t, 128,128,128, 16>;
template class BranchPredictorPlugin<uint32_t, 256,256,256, 16>;
// + uint64_t 版本 (10 个总实例)
```

**CpuFactory 中 switch 选择**:

```cpp
// 在 build_cpu 内构造 BP plugin
std::unique_ptr<PluginBase> bp;
switch (cfg.btb_entries) {
  case  16: bp = BranchPredictorPlugin<uint32_t,  16, 16, 16,  8>::create(cfg); break;
  case  32: bp = BranchPredictorPlugin<uint32_t,  32, 32, 32,  8>::create(cfg); break;
  case  64: bp = BranchPredictorPlugin<uint32_t,  64, 64, 64,  8>::create(cfg); break;
  case 128: bp = BranchPredictorPlugin<uint32_t, 128,128,128, 16>::create(cfg); break;
  case 256: bp = BranchPredictorPlugin<uint32_t, 256,256,256, 16>::create(cfg); break;
  default: throw std::invalid_argument("btb_entries must be 16/32/64/128/256");
}
```

### 6.2 其他 Plugin 改造

| Plugin | 改动策略 | 工作量 |
|---|---|---|
| **RegFilePlugin** | 不改模板 (`kNumRegs=32` 保留 RISC-V 假设);**修复** `.cpp:40` 双重定义 bug — 删除 `.cpp` 中 `build()` 重定义,只留头文件版本 | 0.05d |
| **HazardPlugin** | 不改;`use_strict_scoreboard` 字段预留接口 | 0d |
| **IBusPlugin** | 加 `latency_cycles_` 字段 + `setup_with_config` 接受 cfg;本期 stub (返回 NOP 但记录延迟到内部计数器) | 0.2d |
| **DBusPlugin** | 同 IBus | 0.2d |
| **RiscvDecodePlugin** | `setup_with_config` 根据 `ext_m/ext_a/ext_f/ext_d/ext_zicsr` 决定 `decode_rv32()` 是否识别这些指令(本期只是 flag 切换,真实多指令解码推迟到 M5) | 0.5d |
| **RiscvIntAluPlugin** | 不改(永远 1 个) | 0d |
| **RiscvMulPlugin** | **模板参数化** `mul_latency ∈ {1, 3, 5}`;`mul_latency ≥ 3` 时 `setup()` 内 `declare_substage("execute", "mul_s1", 1)` 等展开子流水 | 1d |
| **RiscvBranchPlugin** | 不改 | 0d |
| **RiscvLsuPlugin** | 加 latency 字段,接受 `dcache_latency`;本期 stub | 0.2d |
| **RiscvCsrPlugin** | 仅当 `ext_zicsr=true` 才注册;真实 CSR 寄存器读写真实实现推迟到 M5 | 0.5d (基础读 mstatus/mcycle) |

### 6.3 PluginBase 扩展 — `setup_with_config` (1 个虚函数)

```cpp
// include/cf/plugin/plugin_base.h — 唯一改动
class PluginBase {
 public:
  virtual ~PluginBase() = default;
  virtual void setup(PipeBuilder& /*pb*/) {}
  virtual void build(PipeBuilder& /*pb*/) = 0;
  // ↓ 新增: 接收 CPUConfig 的扩展 setup, 默认 fallback 到 setup()
  virtual void setup_with_config(PipeBuilder& pb, const void* /*cfg*/) {
    setup(pb);
  }
 private:
  void tick() = delete;
};
```

**为何用 `const void*`**: 保持 `cf_plugin` 不依赖 `ip/cpu`,避免循环依赖。CpuFactory 调用时 `static_cast<const CPUConfig*>(cfg_ptr)` 转换。

### 6.4 注入 CPUConfig 到 Plugin 的实际模式

**两条等价路径**,工厂代码优先使用 ctor 注入:

```cpp
// 路径 A (推荐): plugin ctor 接受 CPUConfig
explicit BranchPredictorPlugin(const CPUConfig& cfg) : cfg_(cfg) { ... }

// 路径 B (备选): 默认 ctor + 显式 setter
BranchPredictorPlugin() = default;
void configure(const CPUConfig& cfg);  // 在 register_plugin 后, build() 之前调用
```

**为何优先 ctor**: 与现有 `PipeBuilder::register_plugin(unique_ptr<PluginBase>)` API 无缝集成 — ctor 一步完成配置,无需外部调用次序约束。

---

## 7. CpuFactory::build_cpu 完整实现

### 7.1 改造前 vs 改造后

| 行数 | 状态 |
|---|---|
| `cpu_factory.h:73-127` 改造前 | **55 行**: 类型 + 3 个空 stub |
| `cpu_factory.h` 改造后 | **~140 行**: 类型 + TopologyBuilder + CPUConfig 扩展 + DseSpace + parse_config + 真实 build_cpu |

### 7.2 build_cpu 真实实现 (M4-DSE 实施后)

```cpp
// ip/cpu/cpu_factory.h — build_cpu 真实实现
template <typename T = std::uint32_t>
class CpuFactory {
 public:
  static std::unique_ptr<cf::plugin::PipeBuilder> build_cpu(const CPUConfig& cfg) {
    auto pb = std::make_unique<cf::plugin::PipeBuilder>();

    // ===== 阶段 1: 拓扑展开 (在 register_plugin 之前) =====
    TopologyBuilder(*pb).expand(cfg);

    // ===== 阶段 2: Plugin 构造 (按 EARLY → NORMAL → LATE 顺序) =====

    // 2.1 ISA 通用 Plugin (无 ISA 依赖)
    auto reg_file = std::make_unique<plugins::RegFilePlugin<T>>();
    auto hazard   = std::make_unique<plugins::HazardPlugin<T>>();
    auto ibus     = std::make_unique<plugins::IBusPlugin<T>>(cfg);
    auto dbus     = std::make_unique<plugins::DBusPlugin<T>>(cfg);

    // 2.2 分支预测器 (编译期 switch on btb_entries)
    std::unique_ptr<cf::plugin::PluginBase> bp;
    if (cfg.enable_branch_predictor) {
      switch (cfg.btb_entries) {
        case  16: bp = plugins::BranchPredictorPlugin<T,  16, 16, 16,  8>::create(cfg); break;
        case  32: bp = plugins::BranchPredictorPlugin<T,  32, 32, 32,  8>::create(cfg); break;
        case  64: bp = plugins::BranchPredictorPlugin<T,  64, 64, 64,  8>::create(cfg); break;
        case 128: bp = plugins::BranchPredictorPlugin<T, 128,128,128, 16>::create(cfg); break;
        case 256: bp = plugins::BranchPredictorPlugin<T, 256,256,256, 16>::create(cfg); break;
        default:  throw std::invalid_argument("btb_entries must be 16/32/64/128/256");
      }
    }

    // 2.3 RISC-V 特有 Plugin
    auto decode  = std::make_unique<arch::riscv::RiscvDecodePlugin<T>>(cfg);
    auto int_alu = std::make_unique<arch::riscv::RiscvIntAluPlugin<T>>();
    auto branch  = std::make_unique<arch::riscv::RiscvBranchPlugin<T>>();
    auto lsu     = std::make_unique<arch::riscv::RiscvLsuPlugin<T>>(cfg);

    // 2.4 可选 ISA 扩展
    std::unique_ptr<cf::plugin::PluginBase> mul, csr, fpu;
    if (cfg.ext_m) {
      switch (cfg.mul_latency) {
        case 1: mul = arch::riscv::RiscvMulPlugin<T, 1>::create(cfg); break;
        case 3: mul = arch::riscv::RiscvMulPlugin<T, 3>::create(cfg); break;
        case 5: mul = arch::riscv::RiscvMulPlugin<T, 5>::create(cfg); break;
        default: throw std::invalid_argument("mul_latency must be 1/3/5");
      }
    }
    if (cfg.ext_zicsr)  csr = arch::riscv::RiscvCsrPlugin<T>::create(cfg);
    if (cfg.ext_f || cfg.ext_d) fpu = arch::riscv::RiscvFpuPlugin<T>::create(cfg);

    // ===== 阶段 3: 注册 (顺序 = EARLY → NORMAL → LATE) =====

    // EARLY
    if (bp) pb->register_plugin(std::move(bp));
    pb->register_plugin(std::move(ibus));

    // NORMAL
    pb->register_plugin(std::move(decode));
    pb->register_plugin(std::move(hazard));
    pb->register_plugin(std::move(int_alu));
    pb->register_plugin(std::move(branch));
    pb->register_plugin(std::move(lsu));
    if (mul) pb->register_plugin(std::move(mul));
    if (csr) pb->register_plugin(std::move(csr));
    if (fpu) pb->register_plugin(std::move(fpu));

    // LATE
    pb->register_plugin(std::move(reg_file));
    pb->register_plugin(std::move(dbus));

    // ===== 阶段 4: 编译 (setup → build 顺序) =====
    pb->build();

    return pb;
  }
};
```

### 7.3 关键技术约束 (MUST FOLLOW)

1. **`pb.build()` 必须在所有 `register_plugin()` 之后调用** — `pipe_builder.h:94-97` 实现
2. **`pb.declare_substage()` / `pb.merge_stage()` 必须在 `register_plugin` 之前调用** — 否则 plugin 的 `setup()` 内 `declare_substage` 会失效
3. **CPUConfig 注入 plugin 的生命周期**: 用 ctor 接受 `const CPUConfig&` 时,工厂内 cfg 是栈对象 — plugin 必须 **copy 一份** cfg 到成员 (`BranchPredictorPlugin::cfg_(cfg)`) 而非持引用,否则 `build_cpu` 返回后 plugin 持有悬垂引用
4. **CPUConfig 的 `CPUConfig cfg_;` 大小** ≈ 100 字节,每 plugin copy 一份开销可忽略

---

## 8. DSE Sweep 工具 (脚本层)

### 8.1 现状

`tools/` 目录现有脚本 (`verify_adr.sh`, `check_plugin_portability.sh`, `doc_link_check.sh`, 等) **全部是验证 / lint 脚本, 零 DSE sweep**。

**状态更新 (2026-06-17)**: 本方案新增的 `tools/dse/sweep_driver.py` / `pareto_analyzer.py` / `sweep_config.example.json` / `README.md` 已创建 (合计 350 LoC Python + 106 行 README)。**但** `cpu_sim` 二进制 (sweep_driver.py 的调用目标) **尚未实施**,由 M5.16 任务负责 — 在 M5.16 完成前, `tools/dse/*` 可被语法/导入校验, 但不可端到端运行。详细阻塞说明见 [`tools/dse/README.md`](../../../tools/dse/README.md) 顶部 "🔵 阻塞中" 横幅。

### 8.2 本方案新增工具

```
tools/dse/
├── README.md                  — 工具说明 + 用法
├── sweep_driver.py            — 主入口, 笛卡尔积展开 + 调用 cpu_sim
├── parse_results.py           — 解析 cpu_sim stdout, 提取 IPC / cycle / 面积代理
├── pareto_analyzer.py         — Pareto 前沿计算 (IPC vs area_proxy)
└── sweep_config.example.json  — 示例 sweep 配置
```

### 8.3 sweep_driver.py 骨架

```python
#!/usr/bin/env python3
"""
DSE Sweep Driver — 笛卡尔积展开 CPUConfig, 批量调用 cpu_sim, 收集指标
用法: ./sweep_driver.py --sweep-config sweep_config.json --output results.json
"""
import json
import subprocess
import itertools
import argparse
from pathlib import Path

DEFAULT_DSE_SPACE = {
    "pipeline_stages":     [3, 5, 7],
    "btb_entries":         [16, 64, 256],
    "branch_predictor":    ["static", "gshare"],
    "ext_m":               [False, True],
    "mul_latency":         [1, 3],
    "isa":                 ["rv32i", "rv64i"],
}

def gen_configs(space):
    keys = list(space.keys())
    for combo in itertools.product(*[space[k] for k in keys]):
        cfg = dict(zip(keys, combo))
        cfg["name"] = "dse_" + "_".join(f"{k}={v}" for k, v in cfg.items())
        yield cfg

def run_simulation(cfg, cpu_sim_bin, cycles=1_000_000):
    cfg_json = json.dumps(cfg)
    result = subprocess.run(
        [cpu_sim_bin, "--config", cfg_json, "--cycles", str(cycles)],
        capture_output=True, text=True, check=True, timeout=300,
    )
    return parse_metrics(result.stdout, cfg)

def parse_metrics(stdout, cfg):
    """解析 cpu_sim stdout: 'IPC=1.23 cycles=1000000 branch_miss=2.1%'"""
    metrics = {"config": cfg}
    for line in stdout.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            try:
                metrics[k.strip()] = float(v.strip())
            except ValueError:
                metrics[k.strip()] = v.strip()
    return metrics

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--space",     help="JSON dict of param → list (else use DEFAULT_DSE_SPACE)")
    ap.add_argument("--cpu-sim",   default="./build/sim/cpu_sim", help="cpu_sim binary path")
    ap.add_argument("--cycles",    type=int, default=1_000_000)
    ap.add_argument("--output",    default="results/sweep.json")
    args = ap.parse_args()

    space = json.loads(args.space) if args.space else DEFAULT_DSE_SPACE
    results = []
    for cfg in gen_configs(space):
        print(f"[SWEEP] {cfg['name']}")
        metrics = run_simulation(cfg, args.cpu_sim, args.cycles)
        results.append(metrics)
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(results, indent=2))
    print(f"[SWEEP] {len(results)} configs done → {args.output}")

if __name__ == "__main__":
    main()
```

### 8.4 cpu_sim 二进制 (M5-DSE 实施后才有)

接受 `--config <json>`,内部调 `CpuFactory<T>::build_cpu(parse_config(json))`,跑 driver 循环,输出指标到 stdout。

### 8.5 Pareto 分析

```python
# tools/dse/pareto_analyzer.py
def compute_pareto(results, maximize, minimize):
    """maximize: ['ipc'], minimize: ['area_proxy', 'cycles']"""
    pareto = []
    for r in results:
        dominated = False
        for other in results:
            if other is r: continue
            better_max = all(other.get(m, 0) >= r.get(m, 0) for m in maximize)
            better_min = all(other.get(m, float('inf')) <= r.get(m, float('inf')) for m in minimize)
            strictly_better = better_max and better_min and \
                              (any(other.get(m, 0) > r.get(m, 0) for m in maximize) or
                               any(other.get(m, float('inf')) < r.get(m, float('inf')) for m in minimize))
            if strictly_better:
                dominated = True
                break
        if not dominated:
            pareto.append(r)
    return pareto
```

---

## 9. 实施路线图

### Phase A — 骨架 (M4-DSE, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| A.1 | `PluginBase::setup_with_config(pb, const void*)` 加 3 行 | `include/cf/plugin/plugin_base.h` | 编译通过 |
| A.2 | `PipeBuilder::merge_stage(name, parent)` 新增方法 | `include/cf/plugin/pipe_builder.h` | 单元测试 PASS |
| A.3 | `CpuFactory::build_cpu` 替换 stub,真实 register 11 个 plugin | `ip/cpu/cpu_factory.h` | test_cpu_factory 升级 PASS |
| A.4 | 修复 `reg_file.cpp:40` 双重定义 bug | `ip/cpu/plugins/reg_file.cpp` | 删除 .cpp 的 build() 重定义 |
| A.5 | `parse_config(json_text)` + `validate_config(cfg)` | `ip/cpu/cpu_factory.cpp` | JSON 4 个示例文件解析通过 |
| A.6 | test_cpu_factory 升级 (断言 plugin_count / stage_names / 拓扑) | `tests/cpu/test_cpu_factory.cpp` | 8 个用例 PASS |

### Phase B — 配置真正生效 (M4-DSE, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| B.1 | `BranchPredictorPlugin` 模板参数化 + 10 个显式实例化 | `ip/cpu/plugins/branch_predictor.h` + `.cpp` | 5 种 BTB 大小编译通过 |
| B.2 | `BranchPredictorPlugin::create(cfg)` 工厂方法 | 同上 | 接受 CPUConfig |
| B.3 | `HazardPlugin` `setup_with_config` 接受 `use_strict_scoreboard` | `ip/cpu/plugins/hazard.h` | 默认行为不变 |
| B.4 | `IBusPlugin` / `DBusPlugin` 接受 `icache/dcache_latency` | `ip/cpu/plugins/ibus.h` + `dbus.h` | 字段接受,stub 行为 |
| B.5 | 测试: `btb_entries = 16/64/256` 跑出不同 BTB 命中分布 | `tests/cpu/test_branch_predictor_dse.cpp` 新建 | 3 个 BTB 大小对比 PASS |

### Phase C — 流水线深度真实展开 (M5-DSE, 1 周)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| C.1 | `TopologyBuilder::expand(cfg)` 实现 §5.4 表格 | `ip/cpu/cpu_factory.h` | 4 种深度编译通过 |
| C.2 | 3 级模式断言: `node_of_logic_stage("memory") == node_of_logic_stage("execute")` | `tests/cpu/integration/test_3stage_riscv.cpp` 升级 | 拓扑断言 PASS |
| C.3 | 7 级模式 `declare_substage("fetch", "fetch_p1", 1)` + `fetch_p2` | `tests/cpu/integration/test_7stage_riscv.cpp` 新建 | 7 个 Node + 拓扑 PASS |
| C.4 | 10 级模式加 EX 子流水 | `tests/cpu/integration/test_10stage_riscv.cpp` 新建 | ≥10 个 Node + 拓扑 PASS |
| C.5 | 端到端执行 (3/5/7/10 级各跑 add.elf) | `tests/cpu/integration/test_*_stage_riscv.cpp` | tohost=1 PASS |

### Phase D — MUL 子流水 (M5-DSE, 可选, 3-5 天)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| D.1 | `RiscvMulPlugin<T, 1/3/5>` 三种实例化 | `ip/cpu/arch/riscv/mul.h` | 编译通过 |
| D.2 | `mul_latency=3/5` 时 `declare_substage("execute", "mul_s1", 1)` 等 | 同上 | 子 stage 注册 |
| D.3 | 测试对比 mul_latency=1 vs 3 的 wall-clock 差异 | `tests/cpu/test_mul_dse.cpp` 新建 | 3-cycle mul 比 1-cycle mul 慢 |

### Phase E — DSE Sweep 脚本 (M5-DSE, 可选, 3-5 天)

| # | 任务 | 文件 | 验收 |
|---|------|------|------|
| E.1 | `tools/dse/sweep_driver.py` | 新建 | 跑通 100 个 config |
| E.2 | `tools/dse/parse_results.py` | 新建 | 解析 stdout 指标 |
| E.3 | `tools/dse/pareto_analyzer.py` | 新建 | Pareto 前沿输出 |
| E.4 | `cpu_sim` 二进制支持 `--config --cycles` | 新建 | 命令行接口 |
| E.5 | 完整 sweep 一次 (预计 4×3×3×2×2×2×2 = 576 config) | — | 输出 results/sweep.json |

### Phase F — 文档同步 (与代码同步进行)

| # | 任务 | 文件 |
|---|------|------|
| F.1 | `multi_isa_architecture.md` §11 附录标注已落实 / 待落实 | `ip/cpu/docs/multi_isa_architecture.md` |
| F.2 | `blueprint.md` §5 CpuFactory 章节加注 "M4-DSE 之前是 stub" | `ip/cpu/docs/blueprint.md` |
| F.3 | `status.md` M4/M5 行加 DSE 子任务 | `ip/cpu/docs/status.md` |
| F.4 | `cpu_params_schema.json` 加 12 个新字段 | `ip/cpu/configs/cpu_params_schema.json` |
| F.5 | `cpu_superscalar.json` + `cpu_deep_pipeline.json` 新建 | `ip/cpu/configs/` |
| F.6 | `testing-and-dse.md` 引用 `tools/dse/sweep_driver.py` | `docs/architecture/testing-and-dse.md` |

---

## 10. 风险与限制

### 10.1 明确不在本期范围 (Phase 5+ 推迟)

| 维度 | 推迟理由 | 替代方案 |
|------|----------|---------|
| 乱序执行 (OoO) | 无 ROB / RS / Rename / Wakeup 抽象 | HazardPlugin 当前 strict scoreboard 即可支撑单发射按序 |
| 超标量 / 多发射 | 无 Issue Queue | RiscvIntAluPlugin 单 lambda 单周期 |
| 跨 ISA (非 RISC-V) | `payload_common.h:115,117` 静态断言卡 XLEN=32/64 | ARM/MIPS 需新 plugin + 放宽断言 |
| L2/L3 cache | `ip/memory/` 仅 README | IBusPlugin/DBusPlugin 接受 latency 但无真实 cache |
| 真实 MMU/PMP | MMUPlugin 是空 stub | 字段保留 |
| FPU (F/D) | RiscvFpuPlugin 是空 stub | 字段保留 |
| 多核 / SMT | Phase 6+ | 单核 CPU 即可 DSE |
| RTL_ONLY / COMPARE | `cf::plugin::ImplMode` 枚举未在 PipeBuilder 实现 | TLM 模式即可 DSE |

### 10.2 已知的真 Bug 需修复

1. **`ip/cpu/plugins/reg_file.cpp:40`**: `build()` 在 `.cpp` 里**重新定义**覆盖 `reg_file.h:119` 头文件版本,且 `.cpp` 版本用 `pl::keys_rv32::DECODE` 硬编码 — 即使 `RegFilePlugin<uint64_t>` 也会用 RV32 的 key。**修复**: 删除 `.cpp:40-77` 整个 build() 重定义,只保留头文件版本。

### 10.3 Plugin `kNumRegs=32` 硬编码风险

`RegFilePlugin::kNumRegs=32` 和 `HazardPlugin::kNumRegs=32` 都是 `static constexpr`,绑定 RISC-V 32 寄存器架构约定。**本期不修**(不破坏 RISC-V),**跨 ISA 时**(ARM/MIPS 16/32/64 寄存器)需要改造为 `<typename T, std::size_t N>` 模板。

### 10.4 `CpuFactory<T>` 模板参数 `T` 当前在 stub 中被丢弃

`cpu_factory.h:104,115,125` 中 `(void)sizeof(U);` 显式丢掉模板参数。本方案真实实现后,`T` 必须贯穿到所有 plugin 实例化 (`make_unique<RiscvDecodePlugin<T>>(cfg)`)。

### 10.5 真实 CPU 执行 driver 未写

当前 `pb->run()` 是空跑(IBus 返回 NOP 指令,Decode 看到 NOP 不做事)。真实 driver 需要 IBus 从 PicolibcHostMemory 读指令,需要 MMU/TLB/异常处理 (本期可绕过,直接用 0 基址)。**这部分工作量超出 DSE 文档范围**,属于 M5 联调任务。

---

## 11. 附录: 与 multi_isa_architecture.md 的对应关系

| multi_isa § | 设计意图 | 本方案落地状态 |
|-------------|---------|--------------|
| §3.2 Plugin 调度顺序 (EARLY/NORMAL/LATE) | 设计意图 | ✅ CpuFactory 真实实现按此顺序 |
| §4.2 Pipeline 内部 Payload Bundle | 设计意图 | ✅ 已落实 (双 Payload 共存) |
| §5.1 取消 ExecContext | 设计意图 | ✅ 已落实 (无 ExecContext 类) |
| §5.2 双 Payload 共存 | 设计意图 | ✅ 已落实 (通用 + RISC-V 特有 Payload) |
| §5.3 ISA 切换 = 更换 Plugin Registry | 设计意图 | ⚠️ 框架就绪, factory dispatch 在 M4-DSE 实施后才生效 |
| §5.4 新增 ISA Checklist | 9 步清单 | ✅ 路径已给出 (本期不实施跨 ISA) |
| §6.1 JSON 配置 Schema | 设计意图 | ✅ `cpu_params_schema.json` 已存在,本方案扩展 12 字段 |
| §6.2 3/5/7/10 级映射 | 设计意图 | ⚠️ 框架就绪,本方案加 `merge_stage` API + `TopologyBuilder` 真实展开 |
| §6.3 Plugin declare_substage 示例 | 设计意图 | ⚠️ `mul` 子流水在本方案 Phase D 实施 |
| §6.4 配置校验 | 设计意图 | ✅ `pb->build()` 内隐式校验 |
| §6.5 三种配置实例 (default/embedded/superscalar) | 设计意图 | ✅ default + embedded 已存在,本方案新建 superscalar/deep_pipeline |

**总评**: multi_isa_architecture.md 描述的**架构骨架完全正确**,但需要 (a) CpuFactory stub 真实实现 + (b) PipeBuilder 加 `merge_stage` 方法 + (c) BranchPredictor/MUL 模板参数化 + (d) 测试升级,才能从"设计意图"变成"可执行 DSE 引擎"。

---

## 12. 附录: 关键文件改动清单

### 12.1 框架层 (2 处小改)

| 文件 | 改动 |
|------|------|
| `include/cf/plugin/plugin_base.h` | +3 行:`setup_with_config` 虚函数 |
| `include/cf/plugin/pipe_builder.h` | +10 行:`merge_stage` 方法 |

### 12.2 CPU IP 层 (核心改动)

| 文件 | 改动 |
|------|------|
| `ip/cpu/cpu_factory.h` | **完整重写**: CPUConfig +12 字段 + TopologyBuilder + DseSpace + parse_config + 真实 build_cpu (从 30 行 stub → ~140 行) |
| `ip/cpu/cpu_factory.cpp` | 加 `parse_config` + `validate_config` 实现 (~150 行 JSON 子集解析) |
| `ip/cpu/plugins/branch_predictor.h` | 模板参数化 BTB/BIMODAL/GSHARE/GHR + create 工厂方法 + setup_with_config |
| `ip/cpu/plugins/branch_predictor.cpp` | 10 种实例化的 `template class` 显式特例化 |
| `ip/cpu/plugins/reg_file.cpp` | **删除** build() 重定义 (40-77 行),留头文件版本 |
| `ip/cpu/plugins/hazard.h` | 加 `use_strict_scoreboard` 字段 + setup_with_config |
| `ip/cpu/plugins/ibus.h` | 加 `latency_cycles_` 字段 + setup_with_config 接受 cfg |
| `ip/cpu/plugins/dbus.h` | 同 ibus |
| `ip/cpu/arch/riscv/mul.h` | 模板参数化 `mul_latency` + 三种实例化 + 子流水 |
| `ip/cpu/arch/riscv/lsu.h` | 加 latency 字段 |
| `ip/cpu/arch/riscv/decode.h` | setup_with_config 根据 ext_* 决定识别哪些指令 |
| `ip/cpu/arch/riscv/csr.h` | 最小 CSR 实现 (mstatus/mcycle read),仅 ext_zicsr=true 时注册 |

### 12.3 测试升级 (新增 + 改造)

| 文件 | 改动 |
|------|------|
| `tests/cpu/test_cpu_factory.cpp` | 升级断言: `plugin_count()` / `stage_names()` / 拓扑断言 |
| `tests/cpu/integration/test_3stage_riscv.cpp` | 加 3 级拓扑断言 |
| `tests/cpu/integration/test_5stage_riscv.cpp` | 加 5 级拓扑断言 |
| `tests/cpu/integration/test_7stage_riscv.cpp` | **新建** |
| `tests/cpu/integration/test_10stage_riscv.cpp` | **新建** |
| `tests/cpu/test_branch_predictor_dse.cpp` | **新建** — 3 种 BTB 大小对比 |
| `tests/cpu/test_mul_dse.cpp` | **新建** — 1/3/5 cycle mul 对比 |

### 12.4 配置 & 文档 (新增)

| 文件 | 改动 |
|------|------|
| `tools/dse/sweep_driver.py` | **新建** |
| `tools/dse/parse_results.py` | **新建** |
| `tools/dse/pareto_analyzer.py` | **新建** |
| `tools/dse/README.md` | **新建** |
| `tools/dse/sweep_config.example.json` | **新建** |
| `ip/cpu/configs/cpu_params_schema.json` | +12 个新字段 schema 定义 |
| `ip/cpu/configs/cpu_superscalar.json` | **新建** — 7 级 + ext_m=true 示例 |
| `ip/cpu/configs/cpu_deep_pipeline.json` | **新建** — 10 级 + mul_latency=3 示例 |
| `ip/cpu/docs/dse_architecture.md` | **新建** — 本文档 |

### 12.5 现有文档同步更新

| 文件 | 同步内容 |
|------|---------|
| `ip/cpu/docs/README.md` | 索引加 dse_architecture.md |
| `ip/cpu/docs/multi_isa_architecture.md` | §11 附录加 "DSE 实施状态" 标注 |
| `ip/cpu/docs/blueprint.md` | §5 CpuFactory 加 "M4-DSE 之前是 stub" |
| `ip/cpu/docs/status.md` | M4/M5 加 DSE 子任务 (M4.12-M4.16 / M5.10-M5.14) |
| `ip/cpu/docs/implementation-plan/M4-integration.md` | 加 M4.12-M4.16 DSE 任务 |
| `ip/cpu/docs/implementation-plan/M5-verification.md` | 加 M5.10-M5.14 DSE 任务 |
| `ip/cpu/docs/cpu_implementation_guide_v2.0.md` | §3.4 文件清单加 dse_architecture.md |
| `docs/architecture/testing-and-dse.md` | 引用 `tools/dse/sweep_driver.py` (替换文档中"不存在"的描述) |

---

## 13. 与现有 v2.0 决策的关系

| v2.0 决议 | 是否冲突 | 备注 |
|----------|---------|------|
| **F1**: 范围 = RV32I/RV64I + M + Zicsr + Zifencei, 5/3 级, TLM_ONLY | ✅ 无冲突 | 本方案在此范围内扩展 (加 MUL/D/F 扩展开关, 7/10 级) |
| **F2**: 完全复用 multi_isa 目录与 Plugin 分类 | ✅ 无冲突 | 本方案不改目录结构 |
| **F3**: 复用 L1CachePlugin 6 维度方法学 | ✅ 无冲突 | 本方案沿用 D4 (Plugin-style 无 tick) |
| **F4**: riscv-tests/Spike/Python 推迟 | ✅ 无冲突 | 本方案仅 C++ + Python sweep |
| **F5**: Phase 1 仅 TLM, RTL/COMPARE 推迟 | ✅ 无冲突 | DSE 仅在 TLM 模式 |
| **F6**: ISA 隔离, 仅 riscv | ✅ 无冲突 | 本方案不实施跨 ISA, 仅提供迁移路径 |
| **议题 1 选 C**: 复用 cf_plugin + 扩展 | ✅ 无冲突 | 本方案扩展 2 个 cf_plugin API |
| **议题 2 选 B**: 5 核心优先, 6 推迟 | ✅ 无冲突 | 本方案激活已实现的 6 个, P2 stub 仍保留 |
| **议题 3 选 B+C**: array_store 复用 cf_plugin storage.h | ✅ 无冲突 | 本方案不改变 storage 抽象 |
| **议题 4 选 B**: JSON 字段名按 multi_isa v2.0 §6.1 | ✅ 无冲突 | 本方案新增字段遵循同一风格 |
| **议题 5 选 B**: CpuFactory 内置 PluginOrder | ✅ 无冲突 | 本方案是议题 5 的真实落实 |
| **议题 6 选 C**: picolibc 绕过 MemoryTLM | ✅ 无冲突 | 不涉及 memory |
| **议题 7 选 A**: 不创建 arch/arm/ | ✅ 无冲突 | 本方案不创建 |
| **议题 8 选 B**: build_cpu + 手工 ELF | ✅ 无冲突 | 本方案扩展 build_cpu 但不改变 ELF 流程 |

**结论**: 本方案是 v2.0 决议 + 议题 1-8 的**落实与扩展**,**不修改任何决策**,仅在 M4-DSE / M5-DSE 子阶段引入新工作量。

---

*文档结束。如需修改, 走 v2.0 拆分维护约定 (见 [cpu_implementation_guide_v2.0.md §3.5](cpu_implementation_guide_v2.0.md#35-后续维护约定))。*