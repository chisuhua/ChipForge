# Phase 1.3d-extras: ch_stream 协议转换 + full JSON e2e — 决策草案 v1

> **决策ID**: DECISION-2026-06-13-01
> **决策日期**: 2026-06-13
> **决策状态**: **Proposed v1**（基于 v2 草案格式 + Phase 1.3d 实施经验, 待本次 session 实施 + 验证后改 Accepted v1）
> **提出方**: Sisyphus（基于 Phase 1.3d-extras 启动门槛 + 路线图 §3 PA-6 实施前提分析）
> **决策影响**: Phase 1.3 真正收官; cpptlm StreamAdapter 集成打通; 为 Phase 1.4 baseline 对比铺路
> **关联文档**: `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` (v2 父决策), `docs/architecture/adr.md` ADR-007 (StreamAdapter 跨 TLM↔RTL 通用桥接), `bundles/README.md` §2 (D4 Plugin-style 强制), `docs/roadmap/roadmap-status.md` §3 PA-6/PA-8

---

## 1. Why

Phase 1.3d (`c8d1dd1`) 实现了 `L1CacheTLMBridgeAdapter` 作为 cpptlm ModuleFactory 兼容层, 但**留下两个未解缺口**:

1. **ch_stream 协议转换未实现** — `L1CacheTLMBridge::set_stream_adapter()` 仅保存指针, Adapter 与 Bridge 之间无 ch_stream 协议握手 (R6 风险)
2. **full JSON `instantiateAll` e2e 未跑通** — `test_l1_cache_plugin_e2e.cpp` 明确标注 "Phase 1.3d 限制: ch_stream 协议转换未实现, 跳过 instantiateAll 的连接/adapter 管道, 测试通过 ModuleFactory::create() 直接实例化 (单模块, 无连接)"

`roadmap-status.md` §3 PA-6 (P1 优先级) 明确要求完成此两项 + ADR-007 不再 EXPECTED_MISSING。

### 1.1 关键技术挑战: Bundle 类型双轨制 (DRIFT-1)

| 类型 | 来源 | 字段类型 | 继承 |
|------|------|----------|------|
| `cf::bundles::CacheReq` | ChipForge Phase 1.1 (`bundles/mem_bundles.h`) | `cf::plugin::uint_t<N>` POD | 无 (struct) |
| `bundles::CacheReqBundle` | CppTLM (`include/bundles/cache_bundles_tlm.hh`) | `ch_uint<N>` (CppHDL 兼容) | `bundle_base` |

`ChStreamAdapterFactory::registerAdapter<ModuleT, ReqBundleT, RespBundleT>(type)` 期望 `ReqBundleT` 和 `RespBundleT` 是**CppTLM ch_stream Bundle 类型** (派生自 `bundle_base`), 因为 `StreamAdapter<>` 内部使用 `bundles::serialize_bundle/deserialize_bundle` 对其进行 Packet 转换。

直接传入 `cf::bundles::CacheReq` 会编译失败 (无 `bundle_base` 继承, 无 `serialize_bundle` 特化)。

### 1.2 4 字段窄桥的边界 (R6 风险)

v2 决策 D1=C 选了 4 字段窄桥 (addr/data/is_write/id), `op/burst_len/parent_id/fragment_*` 走 CppTLM `CacheReqBundle` 的 default 值 (0/false/1)。Phase 1.3 范围内 (单事务单拍) 足够, Phase 2+ 多拍 / RTOS 场景需重新评估。

---

## 2. What Changes (决议 F1-F5)

### F1: ch_stream 协议转换实现路径

**方案 (推荐 F1.A)**: Bridge 内部**直接接收** CppTLM `bundles::CacheReqBundle/CacheRespBundle` (持有 `ch_stream<>`), 在 tick() 时**就地**做 4 字段 ↔ POD 转换, 写入 `payload_node_`。

理由:
- v2 D1=C (POD 保持) 不变: `bundles/mem_bundles.h` 业务 Bundle 仍 POD
- Bridge 是薄适配层 (`src/cf_plugin/bridge/`), 不受 D4 检查约束 (D2=B 框架层)
- 4 字段 ↔ POD 转换 ~8-12 行, 无需新文件

**否决 F1.B**: 在 POD 上重载 `serialize_bundle/deserialize_bundle` (会污染业务 Bundle, 违反 D4)
**否决 F1.C**: 推迟到 Phase 2 (PA-6 仍 P1, 推迟会卡 Phase 1.4)

### F2: ChStreamAdapterFactory 注册

```cpp
// src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
#include "framework/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
namespace { struct L1CacheBridgeAutoRegister {
  L1CacheBridgeAutoRegister() {
    ChStreamAdapterFactory::get().registerAdapter<
        L1CacheTLMBridgeAdapter,
        bundles::CacheReqBundle,
        bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter");
  }
}; static L1CacheBridgeAutoRegister _auto_register; }
```

注册时机: 静态全局变量, Adapter `.cpp` 加载时自动执行, 与 `ModuleFactory::registerObject<>` 静态注册并行 (参考 v2 决策 D2=B 框架层定位)。

### F3: full JSON `instantiateAll` e2e 测试

**新增**: `src/cf_plugin/tests/test_l1_cache_json_instantiate.cpp`

测试覆盖 (5 子测试):
1. JSON 加载 + `ModuleFactory::instantiateAll` 不抛异常
2. `factory.getInstance("l1")` 返回非空指针, dynamic_cast 到 `L1CacheTLMBridgeAdapter*` 成功
3. `factory.getInstance("tg")` + `"mem"` 也返回非空 (3 模块拓扑)
4. `factory.startAllTicks()` 不崩溃, EventQueue 推进 100 cycle (`eq.run(100)` 后 cycle 计数 +100)
5. Adapter 内部 ch_stream 协议转换跑通: 5 事务后 Bridge 的 `pb_run_count()` ≥ 5

**范围限制** (与 v2 D3=A 一致):
- 仅 1.3 范围, traffic_gen → l1 → mem 三模块
- 不测 mem 内部行为, 只测拓扑连通
- 不引入 ch_stream 数据流验证 (Phase 1.4 baseline 对比范围, R7)

### F4: ADR-007 状态更新

`docs/architecture/adr.md` §3 (ADR-007 详细记录) 末尾追加:

> ##### 实施更新 (2026-06-13)
> Phase 1.3d-extras 落地 `L1CacheTLMBridgeAdapter` ch_stream 注册 (DECISION-2026-06-13-01 F1.A+F2):
> - `ChStreamAdapterFactory::get().registerAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>("L1CacheTLMBridgeAdapter")`
> - Bridge 内部 4 字段 ↔ POD 转换, 不修改业务 Bundle
> - full JSON `instantiateAll` e2e 5 子测试通过 (test_l1_cache_json_instantiate)
> **范围限制**: 仅 L1Cache 拓扑, 跨 TLM↔RTL 通用 StreamAdapter 模板仍 Phase 5 实施 (R7 范围)

§2.3 L131 ADR-007 状态: `🚧（仅 \`HybridCacheWrapper\` 局部）` → `🚧（Phase 1.3d-extras L1Cache Adapter 已注册; 通用模板 Phase 5 实施）`

### F5: 退出标准

| 编号 | 标准 | 验证命令 |
|------|------|---------|
| E1 | `registerAdapter<>` 注册编译通过 | `cmake --build build` 退出码 0 |
| E2 | ctest 14/14 → 18/18+ (4 个新测试) | `ctest --test-dir build --output-on-failure` |
| E3 | `verify_adr.sh --only=ADR-007` 不再 EXPECTED_MISSING | `bash tools/verify_adr.sh --only=ADR-007 2>&1 \| tail -5` |
| E4 | full JSON `instantiateAll` 不抛异常 | `test_l1_cache_json_instantiate` 5/5 PASS |
| E5 | `eq.run(100)` 后 cycle 推进 100 | `test_l1_cache_json_instantiate::test_cycle_advance` PASS |

---

## 3. Out of Scope

- 通用 StreamAdapter 模板 (Phase 5 实施, R7 范围)
- 多拍 / 分片支持 (R6 风险, Phase 2+ 评估)
- MemReq ↔ MemResp 协议转换 (Cache 范围, Memory 范围 Phase 2+ 单独 change)
- Phase 1.4 baseline 对比 (PA-7/PA-9, 独立 decision)
- 真正的业务数据流验证 (需 PA-7 baseline, 本次仅拓扑连通)

---

## 4. Verification Commands (commit 前必跑)

```bash
cd /workspace/project/ChipForge
cmake --build build 2>&1 | tail -5                    # 期望: 退出 0, 0 错误
ctest --test-dir build --output-on-failure 2>&1 | tail -8   # 期望: 100% tests passed, 0 tests failed out of 18
bash tools/verify_adr.sh --only=ADR-007 2>&1 | tail -3  # 期望: ✓ PASS 或 ✓ ALL PASS (不再 EXPECTED_MISSING)
```

---

## 5. Commit Message 模板

```
feat(phase-1.3d-extras): ch_stream adapter 注册 + full JSON instantiateAll e2e

- src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp:
  + 静态注册 ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>
  + 4 字段窄桥 (addr/data/is_write/id) 协议转换 (D1=C 不变)
- src/cf_plugin/tests/test_l1_cache_json_instantiate.cpp (新建):
  + 5 子测试: instantiateAll / 3 模块 getInstance / startAllTicks / 5 事务 pb_run / 100 cycle 推进
  + 14/14 → 18/18 ctest PASS
- src/cf_plugin/CMakeLists.txt:
  + test_l1_cache_json_instantiate target 注册 + cpptlm_link
- docs/architecture/adr.md ADR-007:
  + §2.3 L131 状态行更新
  + §3 详细记录末尾追加 "实施更新 (2026-06-13)" 小节

决策依据: DECISION-2026-06-13-01 (F1.A + F2 + F3)
无业务代码变更 (bundles/mem_bundles.h POD 保持, D4 合规)
退出标准 E1-E5 全部通过
```

---

## 6. Rollback

```bash
cd /workspace/project/ChipForge
git revert <commit-hash>  # 单原子 commit, 一键回滚
```

---

## 7. 决议表 (F1-F5)

| # | 决议 | 状态 |
|---|------|------|
| F1 | ch_stream 转换走 F1.A (Bridge 内部 4 字段↔POD) | **Proposed** |
| F2 | ChStreamAdapterFactory 静态注册 (Adapter 加载时) | **Proposed** |
| F3 | test_l1_cache_json_instantiate 5 子测试 | **Proposed** |
| F4 | ADR-007 状态 §2.3 L131 更新 + §3 末尾 "实施更新" | **Proposed** |
| F5 | E1-E5 退出标准 | **Proposed** |
