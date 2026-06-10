# Cache IP 设计文档

> 🚧 **Status: TLM model + Bridge + JSON spec (Phase 1.2 + 1.3a + 1.3b + 1.3c + 1.3e, 2026-06-10)** — `ip/cache/tlm/L1CachePlugin.{h,cpp}` (Plugin-style, 4/4 unit tests) + `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (L1CacheTLMBridge, 2/2 unit tests) + `soc/l1_cache_minimal.json` (最小 SoC 拓扑 spec) + `ip/cache/configs/params_schema.json` (JSON Schema)。L1I/L2/RTL/ModuleFactory JSON 集成 (1.3d) 推迟到下个 session。详见 [Phase 1.3 v2 决策草案](../../.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md) §3-4。

## 1. 功能概述

### 1.1 模块定位
Cache IP 位于 CPU 与主存之间，提供低延迟的数据/指令缓存服务。支持多级缓存（L1I/L1D/L2）配置。

### 1.2 核心功能
- 可配置容量、关联度、行大小
- 支持多种替换策略（LRU、PLRU、Random、FIFO）
- 支持多种写策略（Write-Back、Write-Through）
- 支持预取策略（Stride、Next-Line、Stream）
- MESI/MOESI 一致性协议支持（多核场景）
- 性能统计（命中率、Miss 延迟、带宽利用率）

### 1.3 性能目标
| 指标 | 目标值 | 说明 |
|------|--------|------|
| Hit Latency | 1-3 cycles | 取决于缓存级别 |
| Miss Penalty | 10-100 cycles | 取决于下级存储延迟 |
| Hit Rate | > 90% (L1) | 典型工作负载 |

## 2. 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 缓存模型（周期精确） |
| `rtl/` | CppHDL RTL 实现 |
| `test/` | 缓存验证套件 |
| `configs/` | 容量/策略配置 |

## 3. 接口设计

### 3.1 端口定义
| 端口名 | 方向 | 类型 | Bundle | 说明 |
|--------|------|------|--------|------|
| cpu_req | in | ch_stream | MemReqBundle | CPU 侧请求 |
| cpu_resp | out | ch_stream | MemRespBundle | CPU 侧响应 |
| mem_req | out | ch_stream | MemReqBundle | 下级存储请求 |
| mem_resp | in | ch_stream | MemRespBundle | 下级存储响应 |
| snoop | in/out | ch_stream | SnoopBundle | 一致性侦听（多核） |

### 3.2 握手协议
- valid/ready 背压握手
- 请求-响应配对（通过 tag/id 关联）

## 4. 可插拔策略

| 策略类型 | 可选实现 | 默认值 | 说明 |
|---------|---------|--------|------|
| 替换策略 | LRU, PLRU, Random, FIFO | LRU | 缓存行淘汰算法 |
| 写策略 | WriteBack, WriteThrough | WriteBack | 写命中处理方式 |
| 分配策略 | WriteAllocate, NoWriteAllocate | WriteAllocate | 写缺失处理方式 |
| 预取策略 | None, NextLine, Stride, Stream | None | 硬件预取算法 |

## 5. 配置参数

| 参数 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| capacity_kb | int | 32 | 1-4096 | 容量 (KB) |
| associativity | int | 4 | 1-16 | 关联度（路数） |
| line_size_bytes | int | 64 | 16-256 | 缓存行大小 |
| num_mshr | int | 4 | 1-16 | MSHR 条目数 |
| hit_latency | int | 1 | 1-10 | 命中延迟（周期） |
| replacement_policy | string | "lru" | 见策略表 | 替换算法 |
| write_policy | string | "write_back" | 见策略表 | 写策略 |
| prefetch_policy | string | "none" | 见策略表 | 预取策略 |

## 6. DSE 参数化

### 可探索维度
| 参数 | 扫描范围 | 影响指标 |
|------|---------|---------|
| capacity_kb | [4, 8, 16, 32, 64] | 命中率 vs 面积 |
| associativity | [1, 2, 4, 8] | 命中率 vs 延迟 |
| replacement_policy | [LRU, PLRU, Random] | 命中率 vs 复杂度 |
| prefetch_policy | [None, NextLine, Stride] | 带宽 vs 命中率 |

## 7. 性能统计

| 统计项 | 类型 | 说明 |
|--------|------|------|
| hit_count | counter | 命中次数 |
| miss_count | counter | 缺失次数 |
| hit_rate | rate | 命中率 |
| avg_miss_latency | histogram | 平均缺失延迟 |
| eviction_count | counter | 淘汰次数 |
| prefetch_hit_count | counter | 预取命中次数 |

## 8. 相关文档
- [项目架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../../docs/architecture/testing-and-dse.md)
- [Phase 1.3 v2 决策草案](../../.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md)
- [Phase 1.2 Lessons 文档](../../docs/lessons/phase-1.2-l1cacheplugin.md)

---

## 9. Phase 1.3 使用指南

> 本节面向 Phase 1.3 用户 (单元测试作者 / SoC 集成者 / 配置维护者)。Phase 1.2/1.3a/1.3b/1.3c/1.3e 已落地；1.3d (ModuleFactory JSON 集成) 推迟。

### 9.1 L1CachePlugin 直接使用 (Plugin-style 单元测试)

`L1CachePlugin` 派生自 `cf::plugin::PluginBase` (D4 强制：无 `tick()`,无状态机)。直接在 PipeBuilder 中注册使用:

```cpp
#include "cf/plugin/pipe_builder.h"
#include "ip/cache/tlm/L1CachePlugin.h"

auto plugin = std::make_unique<cf::ip::cache::tlm::L1CachePlugin>();
L1CachePlugin* helper = plugin.get();

cf::plugin::PipeBuilder pb;
pb.register_plugin(std::move(plugin));
pb.build();

auto lookup = pb.node_of_logic_stage("lookup");

// Issue read (miss path)
cf::bundles::CacheReq req{};
req.address = 0xDEADBEEFULL;
req.id = 1;
helper->issue_request(lookup, req);

pb.run();

// Read response
cf::bundles::CacheResp resp = helper->read_response(lookup);
assert(resp.hit == false);  // first access → miss
```

详见 [`src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp`](../../src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp) (4 tests: miss / refill / hit-after-refill / D4 runtime)。

### 9.2 L1CacheTLMBridge 使用 (cpptlm 适配层)

`L1CacheTLMBridge` (位于 `src/cf_plugin/bridge/`) 持有 `PipeBuilder` + `L1CachePlugin`，在 `tick()` 末尾调用 `pb_.run()` (D1' 契约)。测试 API 直接转发到 Plugin:

```cpp
#include "cf_plugin/bridge/l1_cache_bridge.h"

auto plugin = std::make_unique<L1CachePlugin>();
cf::plugin::bridge::L1CacheTLMBridge bridge(std::move(plugin));

bridge.issue_request(req);  // 写入 payload_node_
bridge.tick();              // 末尾调 pb_.run() → lookup + refill
auto resp = bridge.read_response();
assert(bridge.pb_run_count() >= 1);  // 验证 tick() 真的跑了
```

`set_stream_adapter()` 接口已预留，Phase 1.3d 注入 cpptlm::StreamAdapterBase。详见 [`src/cf_plugin/tests/test_l1_cache_bridge.cpp`](../../src/cf_plugin/tests/test_l1_cache_bridge.cpp)。

### 9.3 SoC JSON 拓扑 (`soc/l1_cache_minimal.json`)

最小验证拓扑: `traffic_gen → l1 (L1CacheTLMBridge) → mem`。结构:

```json
{
  "modules": [
    {"name": "tg",  "type": "TrafficGenTLM", "params": {...}},
    {"name": "l1",  "type": "L1CacheTLMBridge", "params": {num_sets:256, tag_bits:20, idx_bits:8, line_data_bits:512}},
    {"name": "mem", "type": "MemoryTLM", "params": {...}}
  ],
  "connections": [
    {"src": "tg",  "dst": "l1",  "latency": 1},
    {"src": "l1",  "dst": "mem", "latency": 100}
  ]
}
```

**当前状态**: JSON spec 已落地，结构验证测试 4/4 PASS (`test_soc_l1_cache_minimal_json`)。Phase 1.3d 将完成 `L1CacheTLMBridge` → `cpptlm::ModuleFactory` 注册，使此 JSON 可被 `instantiateAll()` 实例化。

### 9.4 参数 Schema (`ip/cache/configs/params_schema.json`)

JSON Schema draft-07，4 核心 param 字段 required:

| 字段 | 默认值 | 含义 |
|------|--------|------|
| `num_sets` | 256 | cache set 数 |
| `tag_bits` | 20 | tag 位宽 |
| `idx_bits` | 8 | idx 位宽 (= log2(num_sets)) |
| `line_data_bits` | 512 | cache line 数据位宽 (典型 64B) |

forward-compat: `replacement_policy` (LRU/PLRU/Random/FIFO) + `write_policy` (WriteBack/WriteThrough)。详见 [`src/cf_plugin/tests/test_cache_params_schema_json.cpp`](../../src/cf_plugin/tests/test_cache_params_schema_json.cpp) (6 tests: top-level / type const / impl_mode enum / 4-required / strict / defaults)。

### 9.5 测试套件汇总 (13 tests PASS in 4.11s)

| Phase | 测试 | 文件 |
|-------|------|------|
| 1.1 | `test_mem_bundles` | `src/cf_plugin/tests/test_mem_bundles.cpp` |
| 1.2 | `test_l1_cache_plugin_unit` | `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp` |
| 1.3a | `test_l1_cache_bridge` | `src/cf_plugin/tests/test_l1_cache_bridge.cpp` |
| 1.3b | `test_soc_l1_cache_minimal_json` | `src/cf_plugin/tests/test_soc_l1_cache_minimal_json.cpp` |
| 1.3c | `test_cache_params_schema_json` | `src/cf_plugin/tests/test_cache_params_schema_json.cpp` |

运行: `tools/run_chipforge_tests.sh`

### 9.6 相关决策与 ADR

- **v2 决策草案** ([`.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md`](../../.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md)): D1=C / D1'=末尾 / D1''=不实现 / D2=B / D3=A
- **ADR-024** (`docs/architecture/adr.md`): Bundle 三层分层 ⚠️ Mapper 未实现，verify_adr.sh 已加 drift 防护
- **ADR-037** (`docs/architecture/adr.md`): Plugin 作为设计范式 (不可逆)
- **D4 决策** ([`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)): 业务代码无 `tick()` / 无状态机 / Bundle 字段用 `uint_t<N>` / 阶段用 `at_stage()`
