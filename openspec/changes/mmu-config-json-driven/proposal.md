---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
depends_on:
  - mmu-paddr-consume-and-real-memory
---

# mmu-config-json-driven — MMU 配置 JSON 化（拆分自 P1#3）

> **Scope 决策 (2026-09-25)**: 本 change 由 P1#3 `mmu-paddr-consume-and-real-memory` 拆分而来, 详见 ADR-048 §后续项 + `docs/roadmap/strategy/execution-roadmap.md §2.5` (注意: §2.4 是 P1#5 `cpu-pipeline-multi-cycle`, 与本 change 无关)。**不**并入 P1#3, 避免 scope creep。
>
> **激活条件**: 本 change 在 P1#3 archive 后实装。tasks.md 待 P1#3 archive 后再写 (与 spec.md:7 一致)。

## Why

P1#3 `mmu-paddr-consume-and-real-memory` 的目标是让 MMU 在 CPU 流水线中真起作用 (IBus/DBus 真消费 `pl::PADDR` + PTW 真内存 walk + MemoryInterface 抽象)。P1#3 proposal 当前已隐含"TLB 几何从 `params_schema.json` 读"的 (用户 idea), 但这是 **配置层抽象**问题, 与 P1#3 的 **数据通路**问题正交:

1. **P1#3 铺好注入点是 JSON 化的天然落点**: P1#3 把 TLB 几何暴露为 `MMUPlugin` 构造参数 (预期字段 `{name, entries, associativity, ...}`, 与既有 `TLBConfig` 1:1) 后, 这些参数从 C++ 硬编码迁移到 JSON 配置就是顺水推舟, 零接口变动。
2. **避免 P1#3 scope creep**: 当前 P1#3 proposal 已 41 tasks, Oracle 估时 2-2.5 周; 加入 JSON 化会再增 ~5-7 tasks + 跨 SoC JSON/MMUPlugin/CpuFactory 三方 schema 协调, 估时 +0.5-1 周 (Metis 偏差上限)。
3. **时序合理**: P1#3 archive 后, 注入点稳定, 此时做 JSON 化只涉及"把硬编码搬到 JSON 字段"——重命名成本极低; 提前做需要等 P1#3 ABI 稳定才能 merge, 浪费时间盒。
4. **与 P1#5 并行可行**: JSON 化是 MMU 单 module 改动, 不依赖 P1#5 (MUL/DIV 多周期子流水)。P1#5 改 latency_table 框架时, 本 change 可同步在 MMU 侧做参数解析, 互不阻塞。

**承接关系**: ADR-048:135-140 "后续项 (P1#3 规划)" 原假设 JSON 化是 P1#3 子项, 本 change 把该假设改为"拆分独立 change", ADR-048 同步修订。

## What Changes

### 1. `MMUPlugin` 构造函数接 JSON 配置对象

- **位置**: `ip/mmu/tlm/MMUPlugin.h` 构造函数签名 (实际路径, 不是 `ip/mmu/tlb/mmu_plugin.h` —— 该目录不存在)
- **现状** (基于真实代码, 不是虚构 `MMUConfig` 结构):
  - `MMUPlugin(SvMode mode, std::vector<TLBConfig> levels_cfg, PTWConfig ptw_cfg)` (`ip/mmu/tlm/MMUPlugin.h:47`)
  - `TLBConfig{name, entries, associativity, num_lookup_ports=1, lookup_latency_cycles=1, replacement_policy="LRU"}` (6 字段, `ip/mmu/tlm/MMUPlugin.h:35-42`)
  - `PTWConfig{max_inflight=2}` (1 字段, `ip/mmu/tlm/MMUPlugin.h:43-45`)
  - `RiscvMMUPlugin(SvMode, vector<TLBConfig>, PTWConfig, uint64_t satp_value=0)` (`ip/cpu/plugins/mmu.h:36-37`) —— 比基类多 `satp_value`
  - 全部 4 个参数由 `CpuFactory::register_early_plugins()` (`ip/cpu/cpu_factory.h:369-396`, 内联模板静态方法, **不是单独 `.cpp`**——`ip/cpu/factories/` 目录不存在) 硬编码填充
  - 注: 代码库中**无 `MMUConfig` 聚合结构体, 无 `hart_id` 参数**, `SvMode` 映射表本身已由 `config.mmu_mode` (`ip/cpu/configs/cpu_default.json` → `CPUConfig`) 驱动
- **目标** (P1#3 archive 后细化, 当前为设计意图):
  - 引入 `MMUConfig{sv_mode, levels, ptw_cfg, satp_value}` 聚合结构体 (P1#3 决定最终字段集, 本 change 复用)
  - `MMUPlugin` / `RiscvMMUPlugin` 构造函数接 JSON config 对象 (或保持多参数 + 单独 `load_mmu_config()` 工厂, P1#3 archive 时定)
  - JSON 字段缺失时 fallback 硬编码 (保证现有 `[cpu-l1-mmu-demo]` 测试不破坏, 见 §兼容性)

### 2. `params_schema.json` 字段扩充

- **位置**: `ip/mmu/configs/params_schema.json` (已存在, 当前未被反化)
- **现状** (基于真实 schema, 不是虚构字段):
  - schema 已定义 `params.topology` (enum: unified/split_id) + `params.asid_bits` (0-16) + `params.sv_mode` (enum: ["Bare","Sv32","Sv39","Sv48"], **默认 `"sv39"` 小写**——**既有 bug**, default 不在 enum 内) + `params.supported_page_sizes` + `params.ptw_max_inflight` (1-8, 默认 2) + `params.shadow_fill_from_next` + `params.levels[]` (1-4 项, 含 `name`/`entries`/`associativity`/`num_lookup_ports`/`lookup_latency_cycles`/`replacement_policy`)
  - `MMUPlugin` / `register_early_plugins` **不读 schema** —— 全部硬编码
- **目标** (字段命名 MUST 与既有 `params_schema.json` 词汇表 1:1 对齐, **禁止发明第三套配置词汇**):
  - SoC JSON `mmu.levels` 数组 (每项字段 `name`/`entries`/`associativity`/`num_lookup_ports`/`lookup_latency_cycles`/`replacement_policy`, 与 `MMUPlugin::TLBConfig` 字段 1:1)
  - SoC JSON `mmu.sv_mode` 字段 (字符串白名单 `"sv32"`/`"sv39"`/`"sv48"`, **小写**, 与 `cpu_default.json::mmu_mode` 一致; **顺手修复 schema 既有的大小写不一致 bug**——接受小写输入, 内部转 `MMUPlugin` PascalCase 枚举)
  - SoC JSON `mmu.ptw_max_inflight` 字段 (uint, 对应 `PTWConfig::max_inflight`, 默认 2)
  - **删除**虚构的 `mmu.tlb_geometry` / `mmu.sets` / `mmu.ways` / `mmu.block_size` / `mmu.parallel_factor`——这些与既有 schema 不兼容, 强行实装会产生第三套配置词汇 (wave4 Cache JSON 化 P2#7 会效仿错误范式)

### 3. `register_early_plugins()` 硬编码消除

- **位置**: `ip/cpu/cpu_factory.h::CpuFactory::register_early_plugins()` (line 369-396, 内联模板静态方法——**不是单独 `.cpp` 文件**; `ip/cpu/factories/` 目录不存在)
- **现状** (基于真实代码):
  - 硬编码 TLB 几何: `{{"L0", 8, 8, 1, 1, "LRU"}, {"L1", 8, 8, 1, 2, "LRU"}}` (`cpu_factory.h:379-382`, **两级**而非单级)
  - 硬编码 `PTWConfig{2}` (`cpu_factory.h:385`)
  - 硬编码 `satp_value=0` (`cpu_factory.h:386`)
  - `SvMode` 映射表本身**已由 `config.mmu_mode` 驱动** (`cpu_factory.h:374-377`, 从 `ip/cpu/configs/cpu_default.json` → `CPUConfig::mmu_mode`), 不是纯硬编码
- **目标**:
  - 调用新模块 `ip/mmu/lib/mmu_config_loader.{h,cpp}` 的 `load_mmu_config(json_path, fallback_config)` 读 SoC JSON `mmu.*` 节点
  - fallback 路径 (JSON 缺失) 仍保留当前硬编码值 (按 ADR-040 v2.0 §3 "TLM baseline 兼容")
  - **新增文件**: `ip/mmu/lib/mmu_config_loader.h` + `ip/mmu/lib/mmu_config_loader.cpp` (~150 LOC, lib/ 层允许 nlohmann/json 依赖, 禁 `cf::plugin::*` 依赖; nlohmann 已在 `tests/mmu/test_mmu_config_schema.cpp:4` 使用)

### 4. SoC JSON 集成

- **位置**: `soc/cpu_l1_mmu_demo.json` + 未来 SoC 配置
- **现状** (基于真实文件 13 行):
  - 当前 SoC JSON 通过 `components` 数组扁平条目 `{"type": "cf::ip::mmu::MMUPlugin", "sv": "sv32"}` 注册 MMU (`soc/cpu_l1_mmu_demo.json:9`)
  - **`mmu.*` 嵌套节点不存在** (proposal 原 "已有 `mmu.memory_interface`" 描述错误, P1#3 0/41 tasks 未交付该字段)
  - sv_mode 已由 `ip/cpu/configs/cpu_default.json::mmu_mode` 驱动, **不走 SoC JSON**
- **目标**:
  - P1#3 archive 后, 在 `soc/cpu_l1_mmu_demo.json` 增加 `mmu.levels` / `mmu.ptw_max_inflight` 嵌套节点 (与 P1#3 的 `mmu.memory_interface` 同源)
  - sv_mode **不在 SoC JSON 重复声明**——单源在 `cpu_default.json::mmu_mode`, 与本 change 配置优先级链一致 (见 §兼容性 配置优先级)

### 5. 新增 `[mmu-config-json]` 测试 family

- **范围**: 3-5 个 unit test (Catch2), 验证:
  - JSON 完整配置 → `MMUConfig` 字段全填, 无 fallback warning
  - JSON 缺失字段 → fallback 硬编码 + warning log (单条聚合, 含缺失字段列表; **不是每字段一条**, 否则 `[mmu]` 47 用例刷屏)
  - JSON 非法值 (e.g. `entries=0` / `associativity>entries` / `levels` 数组越界 1-4 / `sv_mode` 非白名单 / `replacement_policy` 非 `None`/`FIFO`/`LRU`/`RRIP`) → 启动期 fail-fast (抛 `std::invalid_argument`, 由 `CpuFactory` 现有 `std::unexpected(PluginError::BuildFailed)` 包装, 见 §3.3)
  - 与 P1#3 `soc/cpu_l1_mmu_demo.json` 兼容 (端到端 5 ELF `add`/`addi`/`auipc`/`jal`/`beq` tohost=1 不回归; P1#3 archive 后基线扩展为 8 用例, 本 change 以扩展后基线为准)
  - 既有 `[cpu-l1-mmu-demo]` 5 ELF (add/addi/auipc/jal/beq) byte-equal 不回归
  - **新增测试文件**: `tests/mmu/test_mmu_json_config.cpp` (待 P1#3 archive 后创建)
  - 既有 `[mmu]` 47 用例 + `[cpu-l1-mmu-demo]` 6 用例应保持 PASS (除非 P1#3 主动扩展基线)

### 6. ADR 决策 (必填, **不"可选"**)

- **ADR-049 不存在**: 当前最新 ADR 是 ADR-048 (`docs/architecture/adr/ADR-048-plugin-registration-canonical-order.md`), ADR-049 是 **P1#3 交付物**, archive 前不存在
- **决策**: 本 change 不立新 ADR (≤1 周估时不配独立 ADR); 配置驱动决策 MUST 合入 **P1#3 交付的 ADR-049 §后续项**, 覆盖:
  - `MMUConfig` 聚合结构体字段定义 (待 P1#3 实装后细化)
  - 配置优先级链 (见 §兼容性 配置优先级)
  - 字段命名对齐 `params_schema.json` 词汇表的约束
  - `JsonConfigLoader` 归属 `ip/mmu/lib/` (lib/ 层允许 nlohmann/json 依赖, 禁 `cf::plugin::*` 依赖)
- **archive 验收**: "ADR-049 §后续项 已含 JSON 配置驱动段落" 作为强制 checkbox

### 不在本 change 范围 (Out of Scope)

- **CSR/exception 路由**: 那是 P2#6 `phase-1.5-wave-4` scope, 与 JSON 化正交
- **TLB 算法/替换策略实现**: 本 change 只搬配置, 不改 TLB lookup/eviction 逻辑 (那是 mmu 自身的 stub → real 演进, 已在 mmu/STATUS.md 跟踪)
- **P1#5 multi-cycle 与 JSON 化的交互**: latency_table 框架 (P1#5) 是 Plugin 框架层, 不依赖 MMU JSON 字段, 本 change 不涉及
- **Cache 配置 JSON 化**: L1Cache/L2Cache 配置层是 wave4 P2#7 `cache-phase1.5-4way` 平行课题, 不并入

## Capabilities

### New Capabilities
- `mmu-json-config`: MMU 配置 JSON 化能力 —— `MMUPlugin` 构造参数从 `params_schema.json` 反序列化, 支持 fallback 硬编码 + 启动期 fail-fast 校验。该 capability 体现"配置驱动装配"的 D4 范式 (ADR-040 v2.0 §3)。

### Modified Capabilities
<!-- 检查 openspec/specs/, 是否有现存 capability 描述 MMU 行为会因本 change 而变化 -->
- 无现有 spec 描述 MMU 配置层 (现有 specs 如 `mmu-riscv-isa-adapter` / `mmu-tlb-lookup-insert` / `mmu-ptw-sv39-walk` 描述 ISA/TLB/PTW 算法层, 不涉及配置层)。本 change 是**新 capability**而非**修改**。

## Impact

### 改动文件

| 文件 | 类型 | 影响 |
|------|------|------|
| `ip/mmu/tlm/MMUPlugin.h` | 接口扩展 | `MMUPlugin` 构造函数接 JSON config (P1#3 archive 后定的 `MMUConfig` 聚合结构体) |
| `ip/mmu/lib/mmu_config_loader.h` (新增) | 新接口 | `load_mmu_config(json_path, fallback_config)` 函数, 纯 C++ (lib/ 层); ~80 LOC |
| `ip/mmu/lib/mmu_config_loader.cpp` (新增) | 新实现 | nlohmann/json 反序列化 + schema 校验; ~150 LOC |
| `ip/mmu/configs/params_schema.json` | schema 修订 | 修正既有 `sv_mode` default 大小写不一致 bug (PascalCase enum + 小写 default → 接受小写, 内部转 PascalCase); 既有 `params.*` 字段保留 (本 change 不发明新字段) |
| `ip/cpu/cpu_factory.h` | 重构 | `CpuFactory::register_early_plugins()` (line 369-396) 改调 `load_mmu_config()`, 保留 fallback 硬编码 (L0/L1 + PTW{2} + satp=0) |
| `ip/cpu/plugins/mmu.h` | 接口同步 | `RiscvMMUPlugin` 构造函数签名同步扩展 (含 `satp_value` JSON 来源, 由 `load_mmu_config` 注入) |
| `soc/cpu_l1_mmu_demo.json` | schema 扩充 | P1#3 archive 后加 `mmu.levels` / `mmu.ptw_max_inflight` 嵌套节点 (与 P1#3 的 `mmu.memory_interface` 同源); sv_mode **不**在 SoC JSON 重复声明 |
| `tests/mmu/test_mmu_json_config.cpp` (新增) | 测试 | `[mmu-config-json]` family, 3-5 用例 |
| `docs/architecture/adr/ADR-048-plugin-registration-canonical-order.md` | 文档修订 | §后续项 "P1#3 时改为 JSON 配置驱动" → "拆分独立 change `mmu-config-json-driven`, P1#3 archive 后启动" |
| `docs/roadmap/strategy/execution-roadmap.md` | 文档修订 | §2.5 已落地, 校对一致即可 (注: 该文件 line 126 也用了错误命名 `mmu.tlb_geometry`/`mmu.sv_mode`, **不在本 change scope**, 待用户决定是否连带修订) |
| `docs/roadmap/strategy/a-plus-c-hybrid.md` | 文档修订 | §3 Initiative 清单 / §4 依赖图 / §5 柱映射 / §6 版本节点同步新增 `mmu-config-json-driven` |
| (P1#3 交付) `docs/architecture/adr/ADR-049-*.md` | 必填合入 | §后续项 MUST 含"JSON 配置驱动"段落 (本 change 范围) |

### 兼容性

- **TLM baseline 兼容**: 既有 `[cpu-l1-mmu-demo]` 5 ELF (P1#3 archive 后扩展为 8 用例) byte-equal 不回归 (JSON 缺失字段 fallback 硬编码路径与当前行为等价)
- **CH_MEM 模式**: `mmu-config-json-driven` 走 TLM 模式 (配置层在 Plugin build 期), CH_MEM elaboration 不感知, 自动兼容; 配置结构 MUST 保持可被 CH_MEM 模式构造期消费 (POD, 不含运行期状态, 不含 ch 类型) (`ip/cpu/plugins/mmu_ptw_chmem.h` 已存在, Phase 6d MMU CH_MEM 化已起步, 注意 ABI 共享)
- **ADR-040 v2.0**: 本 change 是 §3 "TLM baseline 兼容" 的实现样本, 不引入新违规

### 配置优先级链 (P1#3 archive 后定稿, 现为初稿)

```
SoC JSON `soc/*.json::mmu.levels/ptw_max_inflight`  (最高优先级, 实例级)
  ↓ 缺失该字段
`ip/mmu/configs/params_schema.json::params.*` 默认值 (IP 级 schema defaults, 不发明新字段)
  ↓ 缺失
硬编码 fallback (cpu_factory.h:379-386 当前值: Sv39/L0/L1/PTW{2}/satp=0)
```

**与 `cpu_default.json::mmu_mode` 关系**:
- sv_mode 单源在 `ip/cpu/configs/cpu_default.json::mmu_mode`, 走 `CPUConfig.mmu_mode` → `cpu_factory.h:376-377` 映射表, **不**经 `load_mmu_config()` (避免双写冲突)
- `load_mmu_config()` 只管 `mmu.levels` + `mmu.ptw_max_inflight` 2 个字段 (TLB 几何 + PTW + 隐式 `satp_value=0`)
- 这样本 change 实际 scope 缩为"TLB 几何 + PTW JSON 化", 与"≤1 周"估时匹配

### 不依赖

- **D4 (无 `tick()` / 无状态机)**: 配置加载是 build 期一次, 无运行期逻辑, 不涉及 D4
- **CH_MEM**: 不涉及 elaboration 语义 (配置结构 POD, 模式无关)
- **CSP/死锁**: 配置加载是同步构造期, 无时序耦合

### 风险 (待 P1#3 落地后细化)

- R1 (低): JSON schema 字段命名与 `soc/cpu_l1_mmu_demo.json` 既有 `mmu.memory_interface` 不一致, 需统一 (P1#3 实装后对齐)
- R2 (低): fallback 硬编码路径若与 P1#3 改后的 `MMUConfig` 默认值不一致, 需同步更新
- R3 (中): 多 SoC JSON 配置 (`soc/cpu_l1_mmu_l2_demo.json` 等) 字段缺失处理策略需在 tests 显式覆盖
- R4 (低): ADR-049 后续项扩展若涉及 JSON 化, 需协调 ADR-049 owner 避免重复决策
