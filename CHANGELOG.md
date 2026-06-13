# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Phase 1.3d-extras: ch_stream adapter 注册 + full JSON instantiateAll e2e
  - `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (Phase 1.3d-extras 增补)
    - 静态注册 `ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, ::bundles::CacheReqBundle, ::bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter")`
    - 暴露 `req_in()` / `resp_out()` ch_stream 访问器 (cpptlm::StreamAdapter<ModuleT,...> 期望接口)
    - 4 字段窄桥 (DECISION-2026-06-13-01 F1.A, D1=C 不变): `addr/data/is_write/id` ↔ `cf::bundles::CacheReq` POD;
      `op/burst_len/parent_id/fragment_*` 走 CppTLM default 值 (0/false/1, R6 风险 Phase 2+ 评估)
  - `soc/l1_cache_adapter_e2e.json` (新建, full JSON instantiateAll spec, `L1CacheTLMBridgeAdapter` 类型)
  - `src/cf_plugin/tests/test_l1_cache_json_instantiate.cpp` (新建, 5 子测试):
    instantiateAll / 3 模块 getInstance / startAllTicks / 100 cycle 推进 / Bridge pb_run 验证
  - 14/14 → 16/16 ChipForge ctest PASS in 4.91s
  - **PA-6 闭环**: Phase 1.3 全部子任务完成 (1.3a + 1.3b + 1.3c + 1.3d + 1.3d-extras + 1.3e + 1.3f)
  - 下一里程碑: PA-7 cpptlm::CacheTLM baseline 对比 (2-3 天)
- Phase 1.3d: `L1CacheTLMBridgeAdapter` (cpptlm ModuleFactory 兼容适配层)
  - `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (继承 ChStreamModuleBase)
  - 解决 v2 §4 决策: Bridge 构造签名 (unique_ptr<L1CachePlugin>) 与
    ModuleFactory::registerObject 期望的 (string, EventQueue*) 不兼容
  - Adapter 是薄包装: 内部创建默认 Plugin + Bridge, tick() 委托给 Bridge
  - `src/cf_plugin/tests/test_l1_cache_plugin_e2e.cpp` (5 tests: ModuleFactory
    发现 / Adapter 构造 / Bridge 持有 / Adapter::tick 触发 pb.run / 1000+ tx)
  - 14/14 ChipForge ctest PASS in 5.28s
  - Phase 1.3d-extras 范围 (推迟): ch_stream adapter 注册 + full JSON
    instantiateAll e2e (需要 ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>)
// Phase 1.3 全部完成: 1.3a + 1.3b + 1.3c + 1.3d + 1.3e + 1.3f (commit 待)
- Phase 1.3f: `ip/cache/README.md` §9 Phase 1.3 使用指南 (L1CachePlugin + Bridge + JSON)
  - Status banner 更新: Phase 1.2 + 1.3a + 1.3b + 1.3c + 1.3e 已落地 (1.3d 推迟)
  - §9.1 L1CachePlugin 直接使用 (Plugin-style 单元测试 pattern)
  - §9.2 L1CacheTLMBridge 使用 (cpptlm 适配层 + D1' 末尾挂载契约)
  - §9.3 SoC JSON 拓扑 (`soc/l1_cache_minimal.json`)
  - §9.4 参数 Schema (`ip/cache/configs/params_schema.json`)
  - §9.5 测试套件汇总表 (13 tests PASS in 4.11s)
  - §9.6 相关决策与 ADR (v2 决策草案 + ADR-024/037 + D4)
  - 9 个相对链接全部验证 OK
- Phase 1.3c: `ip/cache/configs/params_schema.json` (L1CachePlugin IP 配置 JSON Schema)
  - JSON Schema draft-07 格式, 严格模式 (additionalProperties=false)
  - 4 核心 param 字段 required: `num_sets`, `tag_bits`, `idx_bits`, `line_data_bits`
  - Defaults 匹配 L1CachePlugin geometry: 256/20/8/512 (Phase 1.2 验证值)
  - `replacement_policy` + `write_policy` 预留 forward-compat (Phase 1 仅 direct-mapped/WriteBack)
  - `src/cf_plugin/tests/test_cache_params_schema_json.cpp` (6 tests: top-level / type const / impl_mode enum / 4-required / strict / defaults)
  - 13/13 ChipForge ctest PASS in 4.11s
- Phase 1.3b: `soc/l1_cache_minimal.json` (Phase 1.3 最小 SoC 拓扑 spec)
  - `traffic_gen` (TrafficGenTLM) → `l1` (L1CacheTLMBridge) → `mem` (MemoryTLM) 拓扑
  - 依据: v2 决策草案 §4 (D1=C + D1' 契约), D2=B (Bridge 在 src/cf_plugin/bridge/)
  - `src/cf_plugin/tests/test_soc_l1_cache_minimal_json.cpp` (4 tests: top-level fields / modules / connections / l1 params)
  - 12/12 ChipForge ctest PASS in 4.00s
  - Phase 1.3d 范围预留: Bridge 注册到 cpptlm::ModuleFactory 后可被此 JSON 实例化
- Phase 1.3e: BundleMapper drift 防护 (verify_adr.sh ADR-024 增强)
  - `tools/verify_adr.sh` 新增 drift 防护检查: 拒绝 `bundles/bundle_mapper.h` 提前实现
  - 依据: v2 决策草案 D1'' + `bundles/README.md:102` (Phase 5 才转换) + `plugin-framework.md:129` (Phase 6 才实现)
  - 负向测试通过: 创建 stub → `verify_adr.sh --only=ADR-024` 报告 FAILED (Critical drift)
  - 正向测试通过: 删除 stub → 报告 PASS (符合 Phase 5 推迟约定)
- Phase 1.3a: `L1CacheTLMBridge` (Plugin-style first IP 的 cpptlm 适配桥接)
  - `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (框架层, 不受 D4 检查约束)
  - 构造: 接管 `unique_ptr<L1CachePlugin>`, 在内部 `PipeBuilder` 注册 + build
  - D1' 契约: `tick()` 末尾调用 `pb_.run()` (回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1)
  - D1=C 实现: 4 字段 test API 转发 (addr/data/is_write/id)
  - `src/cf_plugin/tests/test_l1_cache_bridge.cpp` (2 tests: tick invokes pb.run / 4-field forwarding)
  - 11/11 ChipForge ctest PASS in 3.56s; D4 verify_plugin_decision 3+4/3 PASS
  - Phase 1.3d 范围预留: `set_stream_adapter()` + ch_stream<CacheReqBundle> 协议转换 (cpptlm::StreamAdapterBase 已前向声明)
- Phase 1.2: `L1CachePlugin` (Plugin-style first IP, lookup + refill two-stage pipeline)
  - `ip/cache/tlm/L1CachePlugin.h/.cpp` (256 sets, 64B line, direct-mapped)
  - `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp` (4 tests: miss / refill / hit-after-refill / D4 runtime)
  - 10/10 ChipForge ctest PASS in 2.30s; D4 verify_plugin_decision 3/3 PASS
  - All Bundle fields `cf::plugin::uint_t<N>`; no `tick()`, no state machine; at_stage-driven
  - Phase 0 limitation: `uint_t<512>` falls back to `uint64_t` (tracked; Phase 6 upgrade planned)

### Notes
- Phase 1.3 v2 决策草案 (`8d80fd3`): D1=C (POD + 4 字段窄桥) / D1'=末尾 (tick末尾调pb.run) / D1''=不实现 (BundleMapper推迟Phase 5/6)
- Phase 1.1 Bundle definitions shipped in `073402c` (Bundles 6 types + 9/9 unit tests)
- Phase 0 LSP false positives remain (cf/plugin namespace visibility); tracked, not blocking

### Pending (下一阶段入口)

> Phase 1.3 全部 6 子任务完成 (`26fe7d2`..`c8d1dd1`, 14/14 ctest PASS).
> 以下三项可任意顺序启动, 详见 `docs/roadmap/roadmap-status.md` §3 (PA-6~PA-9).

| 阶段 | 任务 | 入口 | 前置 |
|------|------|------|------|
| **Phase 1.3d-extras** | ch_stream 协议转换 + full JSON `instantiateAll` e2e | PA-6 | `c8d1dd1` (Adapter 已注册) + 起草 PA-8 决策草案 |
| **Phase 1.4** | `cpptlm::CacheTLM` baseline 对比 (`soc/l1_cache_baseline.json`) | PA-7 | Phase 1.3 + 起草 PA-9 决策草案 (5 项候选决议) |
| **Phase 2** | bare-metal 测试套件 (riscv-tests RV64GC + SpikeBridge) | (未立项 PA) | 建议 Phase 1.3d-extras 先完成 |

**当前可立即工作的入口**:
- 起草 `.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-10.md` (PA-8, 参考 v2 格式 `8d80fd3`)
- 起草 `.omo/drafts/decision-phase-1.4-baseline-2026-06-10.md` (PA-9, 5 项候选决议: E1 baseline 选型 / E2 trace 工具 / E3 共享 traffic_gen / E4 hit rate 容差 / E5 测试时长)

## [0.0.1] - 2026-06-10

### Added
- Phase 0 Plugin scaffolding framework (`cf::plugin` namespace, 6 headers in `include/cf/plugin/`)
  - `PluginBase` (lifecycle interface)
  - `Payload<T>` (type-safe key for cross-stage IPC)
  - `PipeNode` (dataflow node)
  - `PipeBuilder` (orchestrator)
  - `CtrlLink` (4-control API: halt_when / throw_when / flush_when / bypass)
  - `uint_t<N>` (compile-time TLM/RTL switch)
- `cf_plugin` INTERFACE library (CMake target)
- 7 cf_plugin unit tests in `src/cf_plugin/tests/` (8/8 ctest PASS in 2.34s)
- CppTLM v2.1.0 integration (TLM framework, `cpptlm_core` target)
- CppHDL v1.0.0 integration (HDL framework, `cpphdl` target, JIT disabled)
- `tools/verify_plugin_decision.sh` (D4 static check, 3/3 PASS)
- `tools/run_chipforge_tests.sh` (wrapper for ChipForge-only ctest run)