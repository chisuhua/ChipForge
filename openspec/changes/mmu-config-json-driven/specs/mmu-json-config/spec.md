# Capability: mmu-json-config

> **Status**: 📋 Draft Skeleton (2026-09-25, 修订 2026-09-25)
> **来源**: `openspec/changes/mmu-config-json-driven/proposal.md` — 由 P1#3 `mmu-paddr-consume-and-real-memory` 拆分
> **激活条件**: P1#3 archive 后 (当前 P1#3 0/41 tasks TODO), 详见 `docs/roadmap/strategy/execution-roadmap.md §2.5` (注意: §2.4 是 P1#5 `cpu-pipeline-multi-cycle`, 与本 capability 无关) + ADR-048 §后续项
> **本文档定位**: 骨架规范 — 满足 `openspec validate` 通过 + 冻结 capability 边界, 完整 Scenarios/字段定义待 P1#3 archive 后细化
> **tasks.md 状态**: **未创建** (按用户诉求, P1#3 archive 后再写)

## Purpose

MMU 配置 JSON 化 —— `MMUPlugin` 构造参数从 `ip/mmu/configs/params_schema.json` 反序列化, 消除 `CpuFactory::register_early_plugins()` 中 TLB 几何硬编码。该 capability 体现"配置驱动装配"的 D4 范式 (ADR-040 v2.0 §3)。

> **字段命名约束**: 字段 MUST 与既有 `params_schema.json` 词汇表 1:1 对齐 (本 capability **不发明** `mmu.tlb_geometry` / `sets` / `ways` / `block_size` / `parallel_factor` 等新词)。

## ADDED Requirements

### Requirement: JSON 字段反序列化

`MMUPlugin` 构造函数 MUST 支持从 `params_schema.json` 字段反序列化构造参数 (`mmu.levels` / `mmu.ptw_max_inflight`; sv_mode 由既有 `cpu_default.json::mmu_mode` 单源驱动, 不重复声明)。当 JSON 字段完整时, 反序列化结果 SHALL 直接使用, 不输出 fallback 警告。

#### Scenario: JSON 完整配置 → MMUConfig 字段全填

- **GIVEN**: SoC JSON `soc/cpu_l1_mmu_demo.json` 含 `mmu.levels[]` (字段 `name`/`entries`/`associativity`/`num_lookup_ports`/`lookup_latency_cycles`/`replacement_policy`) + `mmu.ptw_max_inflight` 完整字段
- **WHEN**: `CpuFactory::build_cpu()` 调用 `load_mmu_config(soctype_json, fallback_config)` 然后构造 `RiscvMMUPlugin(sv_mode, levels, ptw_cfg, satp_value)`
- **THEN**: `MMUConfig` 字段从 JSON 反序列化, 无 fallback 警告, `[mmu-config-json]` 测试 PASS

### Requirement: JSON 字段缺失 fallback

JSON 字段缺失时, `MMUPlugin` MUST fallback 到当前硬编码默认值 (完整集合: `SvMode::Sv39` + 2 级 TLB `{{"L0",8,8,1,1,"LRU"}, {"L1",8,8,1,2,"LRU"}}` + `PTWConfig{max_inflight=2}` + `satp_value=0`), 保证 TLM baseline 兼容 (现有 5 ELF `[cpu-l1-mmu-demo]` 测试 byte-equal 不回归, P1#3 archive 后扩展为 8 用例), 并 SHALL 输出**单条聚合** warning log `"MMUConfig JSON fallback to hardcoded: missing=[field1,field2,...]"` (避免每字段一条刷屏 `[mmu]` 47 用例)。

#### Scenario: JSON 缺失字段 → fallback 硬编码 + warning log

- **GIVEN**: `soc/cpu_l1_mmu_demo.json` 缺失 `mmu.levels` 字段
- **WHEN**: `CpuFactory::build_cpu()` 走 JSON 加载路径
- **THEN**: `MMUConfig` 字段取硬编码默认值 (Sv39 + 2 级 TLB L0/L1 + PTW{2} + satp=0), 输出**单条** warning log `"MMUConfig JSON fallback to hardcoded: missing=[levels]"`, `[cpu-l1-mmu-demo]` 5 ELF byte-equal 不回归

### Requirement: 启动期 fail-fast 校验

JSON 字段值非法 MUST 启动期 fail-fast 抛异常, 不允许静默回退到错误状态或产出可执行 binary。复用现有错误范式:
- 校验逻辑抛 `std::invalid_argument` (项目惯例, `cpu_factory.h:421-422` 已有先例)
- `CpuFactory` 在 build 期 catch 后包装成 `std::unexpected(PluginError::BuildFailed)` (`cpu_factory.h:360-364` 已有先例)
- **不**新建 `ConfigError` 类型 (proposal 原"ConfigError"为虚构)

#### Scenario: JSON 非法值 → std::invalid_argument 抛异常

- **GIVEN**: SoC JSON `mmu.levels[]` 某项 `entries=0`
- **WHEN**: `load_mmu_config()` 校验 JSON schema
- **THEN**: 抛 `std::invalid_argument("mmu.levels[0].entries must be > 0")`, `CpuFactory` 包装为 `std::unexpected(PluginError::BuildFailed)`, 启动失败, 不输出 binary (符合 ADR-040 v2.0 §fail-fast)

非法值清单 (与既有 schema 校验一致):
- `entries < 1` 或 `> 4096`
- `associativity < 1` 或 `> 64` 或 `> entries`
- `levels` 数组长度 < 1 或 > 4
- `replacement_policy` 不在 `{"None","FIFO","LRU","RRIP"}` (PascalCase, schema:65 enum)
- `num_lookup_ports < 1` 或 `> 8`
- `lookup_latency_cycles < 0` 或 `> 16`
- `ptw_max_inflight < 1` 或 `> 8`

### Requirement: SoC JSON 集成

`soc/cpu_l1_mmu_demo.json` MUST 支持 `mmu.levels[]` / `mmu.ptw_max_inflight` 字段, 与 P1#3 引入的 `mmu.memory_interface` 同源配置驱动; 端到端 `[cpu-l1-mmu-demo]` 5 ELF (add/addi/auipc/jal/beq) tohost=1 SHALL PASS 且 byte-equal P1#3 基线。

> **sv_mode 不在 SoC JSON 重复声明** — 单源在 `ip/cpu/configs/cpu_default.json::mmu_mode`, 与本 capability 配置优先级链一致 (见 proposal.md §兼容性 配置优先级)。

#### Scenario: SoC JSON → 端到端 tohost=1

- **GIVEN**: `soc/cpu_l1_mmu_demo.json` 含 `mmu.levels[]` / `mmu.ptw_max_inflight` / `mmu.memory_interface` (P1#3 字段); `cpu_default.json::mmu_mode="sv32"`
- **WHEN**: `CpuFactory::build_cpu()` 加载 SoC JSON + cpu_default.json
- **THEN**: MMU 从 JSON 读取全部配置 (`levels` + `ptw_max_inflight` 走 SoC JSON, `sv_mode` 走 cpu_default.json), `[cpu-l1-mmu-demo]` 端到端 5 ELF (add/addi/auipc/jal/beq) tohost=1 PASS, 与 P1#3 既有用例 byte-equal

---

## Out of Scope (不在本 capability 范围)

- **CSR/exception 路由**: P2#6 `phase-1.5-wave-4` scope
- **TLB 算法/替换策略实现**: 本 capability 只搬配置, 不改 TLB lookup/eviction 逻辑
- **P1#5 multi-cycle 交互**: latency_table 框架层, 不依赖 MMU JSON 字段 (文件级零交集, 唯一共享文件 `cpu_factory.h` 是不同函数 `register_early_plugins` vs `register_normal_plugins`)
- **Cache 配置 JSON 化**: L1Cache/L2Cache 配置层是 P2#7 `cache-phase1.5-4way` 平行课题
- **sv_mode SoC JSON 声明**: 单源在 `cpu_default.json::mmu_mode`, 本 capability 不重复声明
- **`ConfigError` 新类型**: 复用 `std::invalid_argument` + `PluginError::BuildFailed` 现有错误范式

## 关联文档

- **Proposal**: `openspec/changes/mmu-config-json-driven/proposal.md`
- **决策记录**: ADR-048 §后续项 (修订后) + (P1#3 交付) ADR-049 §后续项 (必填合入 JSON 配置驱动段落)
- **战略同步**: `docs/roadmap/strategy/execution-roadmap.md §2.5` + `docs/roadmap/strategy/a-plus-c-hybrid.md §3/§4/§5/§6`
- **前置依赖**: `openspec/changes/mmu-paddr-consume-and-real-memory/proposal.md` (P1#3, 当前 0/41 tasks)
- **既有 schema**: `ip/mmu/configs/params_schema.json` (字段命名源)
- **既有硬编码源**: `ip/cpu/cpu_factory.h:369-396` (`register_early_plugins()`)
- **既有 SoC JSON**: `soc/cpu_l1_mmu_demo.json` (当前无 `mmu.*` 嵌套节点, 13 行)
- **既有 sv_mode 单源**: `ip/cpu/configs/cpu_default.json::mmu_mode`
