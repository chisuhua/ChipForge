# ADR-049 §后续项 — JSON 配置驱动 (草案)

> **状态**: 📋 Draft (2026-09-25)
> **来源**: `openspec/changes/mmu-config-json-driven/proposal.md` (已 commit `46e00a1`)
> **目的**: P1#3 owner 在创建 `ADR-049-mmu-paddr-consumption-contract.md` 时, 将本文件 §1-§4 内容**完整复制**到 ADR-049 末尾 §后续项
> **激活条件**: 本草案在 P1#3 实装 ADR-049 时合入; 本 change (`mmu-config-json-driven`) 实装前必须验证合入成功

---

## 1. 决策概述 (Decision Summary)

将 `MMUPlugin` / `RiscvMMUPlugin` 构造参数从 C++ 硬编码迁移到 JSON 配置驱动, 消除 `CpuFactory::register_early_plugins()` 中的硬编码 TLB 几何 + PTWConfig + satp_value。配置层在 Plugin build 期一次加载, 不引入运行期开销。

**动机**:
1. D4 范式"配置驱动装配" (ADR-040 v2.0 §3) 实现样本
2. DSE 启用 — JSON 配置是 roadmap MS#9 参数扫描的前提
3. scope 控制 — 从 P1#3 (MemoryInterface 抽象) 拆出, 避免 scope creep

**关联变更**: `openspec/changes/mmu-config-json-driven/` (依赖 P1#3 archive, 与 P1#5 `cpu-pipeline-multi-cycle` 并行, 估时 ≤ 1 周)

---

## 2. 配置优先级链 (Configuration Precedence)

```
[1] SoC JSON `soc/*.json::mmu.levels/ptw_max_inflight`      ← 最高优先级 (实例级)
         ↓ 缺失该字段
[2] `ip/mmu/configs/params_schema.json::params.*` 默认值    ← IP 级 schema defaults
         ↓ 缺失
[3] 硬编码 fallback (`cpu_factory.h:379-386` 当前值)        ← 兜底
    - SvMode::Sv39 (默认)
    - TLB 2 级: {{"L0", 8, 8, 1, 1, "LRU"}, {"L1", 8, 8, 1, 2, "LRU"}}
    - PTWConfig{max_inflight=2}
    - satp_value=0
```

**sv_mode 单源约束**:
- sv_mode 走 `ip/cpu/configs/cpu_default.json::mmu_mode` → `CPUConfig.mmu_mode` → `cpu_factory.h:376-377` 映射表
- **不**在 SoC JSON 重复声明 (避免双写冲突)
- `load_mmu_config()` 只管 `mmu.levels` + `mmu.ptw_max_inflight` 2 个字段

---

## 3. 字段命名约束 (Field Naming Discipline)

**禁止发明第三套配置词汇**。所有字段 MUST 与既有 `ip/mmu/configs/params_schema.json` 词汇表 1:1 对齐:

| JSON 字段 | C++ 类型 | 取值范围 | 默认值 | 对应 `TLBConfig`/`PTWConfig` 字段 |
|-----------|---------|----------|--------|-----------------------------------|
| `mmu.levels[].name` | string | non-empty | - | `TLBConfig::name` |
| `mmu.levels[].entries` | uint | 1-4096 | - | `TLBConfig::entries` |
| `mmu.levels[].associativity` | uint | 1-64, ≤ entries | - | `TLBConfig::associativity` |
| `mmu.levels[].num_lookup_ports` | uint | 1-8 | 1 | `TLBConfig::num_lookup_ports` |
| `mmu.levels[].lookup_latency_cycles` | uint | 0-16 | 1 | `TLBConfig::lookup_latency_cycles` |
| `mmu.levels[].replacement_policy` | string | {"None","FIFO","LRU","RRIP"} | "LRU" | `TLBConfig::replacement_policy` |
| `mmu.ptw_max_inflight` | uint | 1-8 | 2 | `PTWConfig::max_inflight` |

**禁止命名** (历史 proposal 已弃用, 严禁复活):
- `mmu.tlb_geometry` / `mmu.sets` / `mmu.ways` / `mmu.block_size` / `mmu.parallel_factor`
- 任何与 `TLBConfig`/`PTWConfig` 字段名不对应的命名

**wave4 引用**: P2#7 `cache-phase1.5-4way` 必须同样遵守此约束 (Cache JSON 化时禁止发明 `cache.set/way/block_size` 等新词, 复用 L1CachePlugin 既有字段)。

---

## 4. 实施位置 (Implementation Layout)

**新增文件**:
- `ip/mmu/lib/mmu_config_loader.h` (~80 LOC): `load_mmu_config(json_path, fallback_config)` 函数签名
- `ip/mmu/lib/mmu_config_loader.cpp` (~150 LOC): nlohmann/json 反序列化 + schema 校验

**修改文件**:
- `ip/mmu/tlm/MMUPlugin.h`: 构造函数接 JSON config (具体签名 P1#3 实装后定)
- `ip/cpu/plugins/mmu.h`: `RiscvMMUPlugin` 同步扩展 (含 `satp_value` JSON 来源)
- `ip/cpu/cpu_factory.h::register_early_plugins()` (line 369-396): 改调 `load_mmu_config()`, 保留 fallback 硬编码
- `ip/mmu/configs/params_schema.json`: 修复既有 `sv_mode` default 大小写不一致 bug (enum PascalCase + default 小写 `"sv39"` → 接受小写输入, 内部转 PascalCase 枚举); 既有 `params.*` 字段保留, 不发明新字段

**新增测试**: `tests/mmu/test_mmu_json_config.cpp` (3-5 用例, `[mmu-config-json]` family)

**lib/ 层 purity 约束**:
- `ip/mmu/lib/mmu_config_loader.{h,cpp}` 允许依赖 `nlohmann/json.hpp` (已在 `tests/mmu/test_mmu_config_schema.cpp` 使用)
- 严禁依赖 `cf::plugin::*` (维持 lib/ 层 purity, 保证 Phase 5 CppHDL 转换零阻力)

---

## 5. 错误范式 (Error Handling)

复用项目现有错误范式, **不**新建 `ConfigError` 类型:

- **校验逻辑抛** `std::invalid_argument` (项目惯例, `cpu_factory.h:421-422` 已有先例)
- **`CpuFactory` 包装** `std::unexpected(cf::plugin::PluginError::BuildFailed)` (`cpu_factory.h:360-364` 已有先例)
- **禁止**新加 `ConfigError` 类 / 枚举值 (避免引入第 N+1 个错误类型)

**fail-fast 校验清单** (启动期):
- `entries < 1` 或 `> 4096`
- `associativity < 1` 或 `> 64` 或 `> entries`
- `levels` 数组长度 < 1 或 > 4
- `replacement_policy` 不在 `{"None","FIFO","LRU","RRIP"}`
- `num_lookup_ports < 1` 或 `> 8`
- `lookup_latency_cycles < 0` 或 `> 16`
- `ptw_max_inflight < 1` 或 `> 8`

**Warning log 约定**:
- JSON 字段缺失时输出**单条聚合** warning: `"MMUConfig JSON fallback to hardcoded: missing=[field1,field2,...]"`
- 严禁每字段一条 (避免 `[mmu]` 47 用例刷屏)

---

## 6. 兼容性边界 (Compatibility Boundaries)

**TLM baseline 兼容**:
- 既有 `[cpu-l1-mmu-demo]` 5 ELF (add/addi/auipc/jal/beq) byte-equal 不回归
- P1#3 archive 后基线扩展为 8 用例, 本 change 以扩展后基线为准
- 既有 `[mmu]` 47 用例保持 PASS (除非 P1#3 主动扩展基线)

**CH_MEM 模式兼容**:
- `mmu-config-json-driven` 走 TLM 模式 (配置层在 Plugin build 期), CH_MEM elaboration 不感知, 自动兼容
- 配置结构 MUST 保持 POD (不含运行期状态, 不含 ch 类型), 保证 CH_MEM 模式构造期可消费
- `ip/cpu/plugins/mmu_ptw_chmem.h` 已存在 (Phase 6d MMU CH_MEM 化已起步), 配置层 ABI 共享

**ADR-040 v2.0**: 本 change 是 §3 "TLM baseline 兼容" 的实现样本, 不引入新违规

---

## 7. 关联文档 (Related Documents)

- **拆分来源**: `openspec/changes/mmu-config-json-driven/proposal.md` (commit `46e00a1`)
- **激活条件**: 本 change `mmu-config-json-driven` 在 P1#3 archive 后启动
- **并行轨道**: 与 P1#5 `cpu-pipeline-multi-cycle` 并行 (文件级零交集, 双方均依赖 P1#3 完成)
- **估时**: ≤ 1 周 (Oracle 估时: 配置层单 module 改动, 无 ABI 跨 5 文件)
- **战略同步**: `docs/roadmap/strategy/execution-roadmap.md §2.5` + `docs/roadmap/strategy/a-plus-c-hybrid.md §3/§4/§5/§6`
- **ADR 后续**: ADR-048 §后续项 (已拆分独立 change 修订) + ADR-049 §后续项 (本草案)

---

## 8. P1#3 owner 集成步骤 (Integration Steps)

在 P1#3 实装 ADR-049 (`docs/architecture/adr/ADR-049-mmu-paddr-consumption-contract.md`) 时:

1. 创建 ADR-049 文件时, 在文件末尾追加 `## 后续项 — JSON 配置驱动` 章节
2. 将本文件 §1-§7 内容**完整复制**到该章节
3. 调整 §7 关联文档引用 (从 commit hash 改为 ADR-049 自己的引用)
4. 在 P1#3 tasks.md §2.1 增加 checkbox: "ADR-049 §后续项已含 JSON 配置驱动段落 (引用 openspec/changes/mmu-config-json-driven/adr-049-followup-section.md)"
5. P1#3 archive 验证: `grep -l "JSON 配置驱动" docs/architecture/adr/ADR-049-*.md` 必须 1 命中

完成后通知 P1#6 owner (`mmu-config-json-driven` 实装 agent) 合入验证。