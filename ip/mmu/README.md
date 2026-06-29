# MMU IP 设计文档

> ✅ **Status: PARTIAL (mmu-ip-skeleton 骨架, 2026-06-29)** — 目录骨架、Plugin 入口、Bundle、Config schema 落地，TLB/PTW 算法推迟到 `mmu-tlb-ptw-impl`。详见 [STATUS.md](STATUS.md) + [docs/architecture.md](docs/architecture.md)。

## 1. 功能概述

### 1.1 模块定位

MMU IP 位于 CPU 与 L1 Cache 之间，提供**虚实地址翻译**与**访问权限检查**。支持：
- 多级 TLB（L0 最小最快 → Ln-1 最大最慢）
- 多 ISA (RISC-V Bare/Sv32/Sv39/Sv48)
- 多 profile (CPU 1-2 ports vs GPU 4-8 ports)
- 可插拔替换策略 (None/FIFO/LRU/RRIP)
- 声明式 Page Table Walker (Sv32/Sv39/Sv48 三级 walk)

### 1.2 核心功能

| 功能 | 描述 |
|------|------|
| 虚实地址翻译 | 接收 vaddr + asid → 返回 paddr + perms |
| 访问权限检查 | perms 含 R/W/X/U 位，与指令访问类型比对 |
| 多级 TLB 缓存 | L0-Ln-1 流水线并行查找，shadow fill 浅层 |
| Page Table Walker | TLB miss 时启动 Sv32/Sv39/Sv48 walk，refill TLB |
| ASID 支持 | 0-16 bits 可配置（0=无 ASID, 9=RISC-V sv39, 12-16=GPU VMID） |
| 多种 page size | 4KB/2MB/1GB/64KB 等可配置 array |
| 失效协议 | SFENCE.VMA 语义 (invalidate vaddr/asid/all) |
| 性能统计 | 每级 hit/miss/evict 计数器 |

### 1.3 性能目标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| L0 Hit Latency | 1 cycle | 单端口 L0 TLB |
| L1 Hit Latency | 2-3 cycles | 多端口 L1 TLB |
| TLB Miss → PTW 启动 | 1 cycle | 同步启动 |
| PTW Walk (Sv39) | ~30 cycles | 3 级 walk + 3 次内存访问 |
| Hit Rate (L0+L1) | > 99.9% (typical workload) | 取决于工作集大小 |

## 2. 快速开始

```bash
# 编译
mkdir build && cd build
cmake .. -DMMU_IMPL_MODE=TLM_ONLY
make

# 运行测试
ctest -L mmu
```

## 3. 目录结构

| 目录 | 说明 |
|------|------|
| `lib/` | **纯 C++ 算法层**（不依赖 Plugin 框架，HDL 1:1 友好） |
| `tlm/` | **声明式 Plugin 层**（`cf::plugin::PluginBase` 派生） |
| `rtl/` | CppHDL RTL 模型（Phase 5+ 沿用） |
| `test/` | 预留（测试在 `tests/mmu/`，遵守 `test-location-discipline`） |
| `configs/` | JSON 配置和参数 Schema |
| `docs/` | 详细设计文档（architecture / configuration / integration） |
| `policies/` | TLB 替换策略（None/FIFO/LRU/RRIP） |

### 3.1 lib/ 与 tlm/ 严格职责切分（Plugin 范式强制）

```
ip/mmu/lib/    # 纯 C++, 0 依赖 cf::plugin::{PluginBase,PipeBuilder,Payload}
                # 唯一允许 include: cf::plugin/uint_t.h (位宽 typedef)
                # 内容: TLB / MultiLevelTLB / PTW / TLBFactory / TLBLookup
                # 可独立单元测试, Phase 5 CppHDL 转换对象

ip/mmu/tlm/    # Plugin 框架集成, 依赖 cf::plugin::*
                # 内容: MMUPlugin (PluginBase 派生) + mmu_keys.h (Payload Key 集合)
                # 持 lib/ 算法为成员, at_stage 闭包内调用
```

> **为什么这样切分**：`cf::plugin::PluginBase` 包含 `tick() = delete`、`at_stage()` 调度、`Payload<T>` 类型擦除等，这些都不是 HDL 1:1 映射对象。`lib/` 与 Plugin 框架解耦，保证 TLB/PTW 算法可独立测试、Phase 5 CppHDL 转换零阻力。详见 [docs/architecture.md §7](docs/architecture.md)。

## 4. 接口设计

### 4.1 lib/ 算法层接口（纯 C++，无 Plugin 依赖）

| 类型 | 说明 |
|------|------|
| `cf::ip::mmu::TLB<ENTRIES,WAYS,TAG_BITS,ASID_BITS,PORTS>` | 模板化单级 TLB |
| `cf::ip::mmu::MultiLevelTLB` | N 级 TLB 编排器，coherence 协议 + shadow fill |
| `cf::ip::mmu::PTW` | Page Table Walker，状态机接口（无 tick） |
| `cf::ip::mmu::TLBFactory` | 模板特化工厂（5-7 组白名单组合） |
| `cf::ip::mmu::TLBLookup` | 查询结果 (hit/paddr/perms/fault_code) |
| `cf::ip::mmu::SvMode` | Bare/Sv32/Sv39/Sv48 枚举 |

### 4.2 tlm/ Plugin 层接口（声明式集成）

| 类型 | 说明 |
|------|------|
| `cf::ip::mmu::MMUPlugin` | `PluginBase` 派生，持 lib/ 算法为成员 |
| `cf::ip::mmu::payload::mmu_keys<T>` | 10 个 `Payload<T>` Key 集合 |
| `cf::ip::mmu::RiscvMMUPlugin` | RISC-V 适配器（`ip/cpu/plugins/mmu.h`），继承 `MMUPlugin` |

### 4.3 CPU/SoC 集成接口

MMU 不直接暴露 Bundle，而是**通过 CPU 的 fetch/memory 阶段 Payload Key**（`pl::PC` / `pl::MEM_ADDR`）读写：

```
vPC → MMUPlugin::at_stage("tlb_lookup_ifetch") → 写 pl::PADDR → IBusPlugin 读取
vaddr → MMUPlugin::at_stage("tlb_lookup_loadstore") → 写 pl::PADDR → DBusPlugin 读取
```

详细集成示例见 [docs/integration.md](docs/integration.md)。

## 5. 可插拔策略

| 策略类型 | 可选实现 | 默认值 | 说明 |
|---------|---------|--------|------|
| 替换策略 | None, FIFO, LRU, RRIP | LRU | Phase 1 落地：None + LRU reference；其他推迟 |

详见 [ip/mmu/policies/](policies/)。

## 6. 配置参数

详见 [configs/params_schema.json](configs/params_schema.json) + [docs/configuration.md](docs/configuration.md)。

| 参数 | 类型 | 默认 | 范围/枚举 |
|------|------|------|----------|
| `topology` | string | "unified" | ["unified", "split_id"] (skeleton 仅 unified) |
| `asid_bits` | int | 9 | 0-16 |
| `sv_mode` | string | "sv39" | ["Bare", "Sv32", "Sv39", "Sv48"] |
| `supported_page_sizes` | array[int] | [4096, 2097152, 1073741824] | 4096-1073741824 |
| `ptw_max_inflight` | int | 2 | 1-8 |
| `shadow_fill_from_next` | bool | true | - |
| `levels` | array | required | 1-4 项, 含 entries/associativity/ports/policy |

## 7. 测试方法

### Level A - lib/ 单元测试（无 Plugin 框架）
- `tests/mmu/test_tlb_unit.cpp` —— 单级 TLB lookup/insert/invalidate 4 组场景 (8 tests)
- `tests/mmu/test_multi_level_tlb.cpp` —— 多级 coherence (8 tests)
- `tests/mmu/test_tlb_factory.cpp` —— 工厂特化 (6 tests)

### Level B - 配置验证
- `tests/mmu/test_mmu_config_schema.cpp` —— JSON Schema 验证 (6 tests)

### Level C - Plugin 集成
- `tests/mmu/test_mmu_plugin.cpp` —— 启动 PipeBuilder 验证 at_stage 集成 (5 tests)

## 8. 相关文档
- [docs/architecture.md](docs/architecture.md) — 详细微架构 / lib/tlm 分层 / Plugin 范式映射
- [docs/configuration.md](docs/configuration.md) — Knobs 详表
- [docs/integration.md](docs/integration.md) — CPU/SoC 集成契约
- [docs/methodology/plugin-style-design-methodology-v1.md](../../docs/methodology/plugin-style-design-methodology-v1.md) — D4 范式方法学
- [openspec/changes/mmu-ip-skeleton/](../../openspec/changes/mmu-ip-skeleton/) — 本 change 完整 artifacts
