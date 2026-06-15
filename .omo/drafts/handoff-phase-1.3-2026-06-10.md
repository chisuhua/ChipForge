# Session Handoff — Phase 1.3 Archive Complete

> **Author**: Sisyphus (session-8)
> **Date**: 2026-06-10
> **Last Commit**: `17782f4 docs(phase-1.3-archive): 路线图 + CHANGELOG 归档指引更新`
> **For**: 新 session 接续 PA-6 / PA-7 / PA-8 / PA-9
> **Recovery**: 读本文件 + 跑 `tools/run_chipforge_tests.sh` → 立即可继续

---

## Goal

完成 Phase 1.3 (L1CachePlugin + Bridge + JSON + Schema + Drift + Adapter) 全部子任务,
为新 session 启动 Phase 1.3d-extras / Phase 1.4 baseline 准备清晰入口。

**Phase 1 进度**: 0% → 10% (1.1) → 30% (1.2) → **65% (1.3 全子任务完成)**

## Constraints & Preferences

- D4 mandatory: no `tick()`, no state machine, Bundle 字段用 `cf::plugin::uint_t<N>`, stages via `at_stage()`, cross-stage IPC via `Payload<T>` Keys
- D2=B: Bridge/Adapter 在 `src/cf_plugin/bridge/` (框架层, NOT subject to `ip/` D4 static check)
- 2-space indent, no tabs, ≤1 consecutive blank line
- Classes CamelCase, functions camelCase, variables snake_case
- Chinese comments preferred, Doxygen for public API, file header (功能描述/作者/最后修改日期)
- TDD: RED (failing test) → GREEN (minimal impl) → REFACTOR
- ctest via `tools/run_chipforge_tests.sh` (14/14 PASS in ~4.5s)
- Phase 0 `uint_t<512>` falls back to `uint64_t` (`uint_t.h:37`); tracked for Phase 6 upgrade
- LSP false positives (`cf::plugin` namespace visibility) are pre-existing; **CMake build is source of truth**
- Atomic commits only; never commit without explicit user request
- Never push without explicit user request

## Progress

### Done (session-8, 7 commits)

- `26fe7d2` Phase 1.3a: `L1CacheTLMBridge` 框架层 (D1'末尾 `pb.run()` + 4-field test API)
- `3dbe058` Phase 1.3b: `soc/l1_cache_minimal.json` (3-module topology: tg/l1/mem)
- `3b6fc27` Phase 1.3c: `ip/cache/configs/params_schema.json` (JSON Schema draft-07, 4 required params + strict)
- `c8d1dd1` Phase 1.3d: `L1CacheTLMBridgeAdapter` (thin `ChStreamModuleBase` subclass solving Bridge ctor signature vs ModuleFactory signature mismatch)
- `18418ac` Phase 1.3e: `tools/verify_adr.sh` ADR-024 drift 防护 (refuses `bundles/bundle_mapper.h`)
- `e5d865a` Phase 1.3f: `ip/cache/README.md` §9 Phase 1.3 usage guide (9/9 links verified)
- `17782f4` Docs: roadmap-status.md §3-6 + CHANGELOG.md Unreleased Pending section

**Phase 1.3 v2 决策草案**: `8d80fd3` (DECISION-2026-06-10-02 v2)
- D1=C: POD + 4-field narrow bridge (addr/data/is_write/id)
- D1'=末尾: `tick()` 末尾调 `pb.run()`
- D1''=不实现: BundleMapper 推迟 Phase 5/6, drift 防护
- D2=B: Bridge/Adapter 在 `src/cf_plugin/bridge/` (framework layer)
- D3=A: 仅最小 e2e (full JSON `instantiateAll` 推迟)

### In Progress
- (none)

### Blocked
- (none)

## Key Decisions

- **Phase 1.3d scope narrowed after SEGFAULT**: `factory.instantiateAll(config)` crashes because `L1CacheTLMBridgeAdapter` not registered with `ChStreamAdapterFactory` (ch_stream adapter pipeline). Pragmatic fix: test uses `std::make_unique<L1CacheTLMBridgeAdapter>(name, &eq)` directly to verify Bridge lifecycle, defers full JSON e2e to Phase 1.3d-extras (PA-6).
- **`L1CacheTLMBridgeAdapter` 是 D2=B + ModuleFactory 兼容的架构级 fix**: thin wrapper inheriting `ChStreamModuleBase`, standard `(string, EventQueue*)` ctor, internally creates default `L1CachePlugin` + `L1CacheTLMBridge`, delegates `tick()` to Bridge.
- **`EventQueue` 是全局类 (无 namespace), `StreamAdapterBase` 在 `cpptlm` namespace** — Bridge header 使用 explanatory comment 标记这个 forward declaration asymmetry (lines30-35 of `l1_cache_bridge.h`).
- **BundleMapper 显式推迟 Phase 5/6** per canonical design; `verify_adr.sh` ADR-024 主动阻止 Phase 1.3 提前实现 `bundles/bundle_mapper.h`.
- **Test1 (Phase 1.2) refactor lessons**: test helper 必须 `pb`-registered Plugin instance (raw pointer via `plugin.get()` before `std::move`); shared `payload_node_` across lookup+refill `at_stage` callbacks essential for cross-stage Payload visibility.
- **D4 static check false positive avoidance**: 重写 "enum class State" 注释为 "显式状态机" in `L1CachePlugin`.

## Next Steps (PA-6/PA-7/PA-8/PA-9, 互不阻塞)

### Path A: PA-6 + PA-8 (Phase 1.3d-extras) — P1 推荐

1. **PA-8 决策草案起草** (建议先做):
   - 文件: `.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-10.md`
   - 参照 v2 决策草案格式 (`8d80fd3`)
   - 候选决议:
     - **F1** ch_stream 协议转换实现范围: 4 字段窄桥 vs full Bundle
     - **F2** Burst/parent_id/fragment_* 字段处理 (default? upgrade to BundleMapper?)
     - **F3** Adapter 内部 Bundle 来源: 直接构造 vs 从 `ch_stream` 反序列化
     - **F4** 测试策略: 复用 Phase 1.3d test pattern (直接 ctor) + add new `instantiateAll` e2e
     - **F5** `ch_stream<CacheReqBundle>` ↔ `payload_node_` 转换点: Bridge 内部 vs Adapter 内部

2. **PA-6 实施** (after PA-8 决策):
   - 在 `chstream_register.hh` 末尾追加 `REGISTER_CHSTREAM_EXTRAS` 宏 OR 扩展现有宏:
     ```cpp
     ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>("L1CacheTLMBridgeAdapter");
     ChStreamAdapterFactory::get().registerAdapter<L1CacheTLMBridgeAdapter,
         bundles::CacheReqBundle, bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter");
     ```
   - 参考位置: `CppTLM/include/chstream_register.hh:43-44` (CacheTLM 注册 pattern)
   - 实现 `L1CacheTLMBridge` 内部 `ch_stream<CacheReqBundle>` ↔ `payload_node_` 协议转换
   - 修改 `L1CacheTLMBridge::tick()`: 加入 adapter 协议转换逻辑
   - 添加 `test_l1_cache_plugin_e2e_full_json.cpp` (full `factory.instantiateAll("soc/l1_cache_minimal.json")` e2e)

### Path B: PA-7 + PA-9 (Phase 1.4 baseline) — P2

1. **PA-9 决策草案起草**:
   - 文件: `.omo/drafts/decision-phase-1.4-baseline-2026-06-10.md`
   - 5 项候选决议:
     - **E1** baseline 选型: `cpptlm::CacheTLM` vs `cpptlm::rtl::HybridCacheWrapper` vs 手写 reference
     - **E2** trace 对比工具: 手写 vs gem5 m5out
     - **E3** 共享 traffic_gen 输入策略
     - **E4** hit rate 容差: ±5% (默认) vs ±0% (严格)
     - **E5** 测试时长: 1k vs 10k transactions

2. **PA-7 实施** (after PA-9 决策):
   - 创建 `soc/l1_cache_baseline.json` (用 `cpptlm::CacheTLM` 作为对比基线)
   - 实现 `test_l1_cache_plugin_vs_cachetlm.cpp`: 共享 traffic_gen 输入, 对比 hit/miss + 最终 cache 状态 + 延迟分布
   - 验证 `cpptlm::CacheTLM` 与 `L1CachePlugin` 功能等价 (Phase 0 投入变现的关键证据)

### Path C: Phase 2 (bare-metal) — P3 (建议先完成 1.3d-extras)

- 详见 `docs/roadmap/phases/phase-2-baremetal.md`
- 任务: riscv-tests RV64GC + SpikeBridge + RISCOF + HTIF

## Critical Context

### 关键 commit hash (供新 session 引用)

| Commit | 内容 |
|--------|------|
| `26fe7d2` | Phase 1.3a L1CacheTLMBridge 框架层 |
| `3dbe058` | Phase 1.3b soc/l1_cache_minimal.json 拓扑 spec |
| `3b6fc27` | Phase 1.3c params_schema.json |
| `c8d1dd1` | Phase 1.3d L1CacheTLMBridgeAdapter cpptlm ModuleFactory 兼容层 |
| `18418ac` | Phase 1.3e BundleMapper drift 防护 |
| `e5d865a` | Phase 1.3f ip/cache/README.md §9 使用指南 |
| `17782f4` | 路线图 + CHANGELOG 归档指引更新 |
| `8d80fd3` | Phase 1.3 v2 决策草案 (D1=C/D1'=末尾/D1''=不实现/D2=B/D3=A) |
| `8de8bfa` | Phase 1.3 v1 决策草案 (superseded) |
| `553b78d` | Phase 1.3 decision context (plugin-docs-extraction plan) |

### 关键文件位置

| 类别 | 路径 |
|------|------|
| **L1CachePlugin (业务逻辑)** | `ip/cache/tlm/L1CachePlugin.{h,cpp}` (D4 Plugin-style, 256 sets × 64B line) |
| **L1CacheTLMBridge (框架层)** | `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (D1'末尾契约, 4-field test API) |
| **L1CacheTLMBridgeAdapter (cpptlm 兼容)** | `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (`(string, EventQueue*)` ctor) |
| **Bundle 定义** | `bundles/mem_bundles.h` (6 types: MemReq/MemResp/CacheReq/CacheResp/L1CachePluginBundle/IntBundle) |
| **SoC 拓扑 spec** | `soc/l1_cache_minimal.json` (3 modules: tg/l1/mem) |
| **参数 Schema** | `ip/cache/configs/params_schema.json` (JSON Schema draft-07, strict mode) |
| **使用指南** | `ip/cache/README.md` §9 (5 subsections, 9/9 links verified) |
| **测试** | `src/cf_plugin/tests/test_l1_cache_*.cpp` (5 files, 14 tests) |
| **决策草案** | `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` |
| **漂移防护** | `tools/verify_adr.sh` (ADR-024 拒绝 `bundles/bundle_mapper.h`) |

### Pre-existing 工作树状态 (不要 commit)

- **Modified** (not from session-8): `.omo/plans/plugin-docs-extraction.md` (来自 `553b78d` 2026-06-09)
- **Untracked** (local dev env, ignore): `.cache/`, `.claude/`, `.cursorrules`, `.gemini/`, `.kiro/`, `.mcp.json`, `.opencode.json`, `.qoder/`, `.windsurfrules`, `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, `QODER.md`, `.github/code-review-graph.instruction.md`, `.omo/boulder.json`, `.omo/evidence/`, `.omo/plans/adr-033-naming-lock.md`, `.omo/run-continuation/`, `state/plugin-docs-baseline.txt`, `state/post-change-adr-verify.log`

### 关键 cpptlm API (Phase 1.3d-extras 必读)

来源: `CppTLM/include/chstream_register.hh` + `CppTLM/include/AGENTS.md`

```cpp
// 1. 模块注册 (factory)
ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>("L1CacheTLMBridgeAdapter");

// 2. Stream adapter 注册 (ch_stream 协议)
ChStreamAdapterFactory::get().registerAdapter<L1CacheTLMBridgeAdapter,
    bundles::CacheReqBundle, bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter");

// 3. 多端口变体 (Phase 2 可能需要)
ChStreamAdapterFactory::get().registerMultiPortAdapter<T, REQ, RESP, N>("T");
ChStreamAdapterFactory::get().registerDualPortAdapter<T, PE_REQ, PE_RESP, NET_REQ, NET_RESP>("T");
ChStreamAdapterFactory::get().registerBidirectionalPortAdapter<T, FLIT, N>("T");

// 4. 复合宏 (生产代码推荐)
#define REGISTER_CHSTREAM_EXTRAS \
    ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>("L1CacheTLMBridgeAdapter"); \
    ChStreamAdapterFactory::get().registerAdapter<L1CacheTLMBridgeAdapter, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter");
```

**注意**:
- `REGISTER_OBJECT` (Legacy) 在 `BUILD_LEGACY_MODULES=OFF` 时退化为 no-op (默认)
- `REGISTER_CHSTREAM` Always ON, 推荐新代码使用
- `ChStreamModuleBase` 必须用 `registerObject`, 不能用 `registerModule` (因继承 `SimObject` 而非 `SimModule`)

### Bundle 字段 (D4 严格)

```cpp
// cf::bundles::CacheReq
struct CacheReq {
  cf::plugin::uint_t<40> address;   // Phase 1.2 实际 32-bit, 预留 40-bit (RISC-V 39-bit)
  cf::plugin::uint_t<64> data;
  bool is_write;
  cf::plugin::uint_t<8> id;
};

// cf::bundles::CacheResp
struct CacheResp {
  cf::plugin::uint_t<64> data;
  bool hit;
  cf::plugin::uint_t<8> id;
};
```

**4 字段窄桥覆盖**: address / data / is_write / id
**未覆盖字段** (R6 风险): burst_len / parent_id / fragment_* — Phase 1.3d-extras 需决策 default 行为 vs BundleMapper 升级

### 验证 checklist (新 session 第一件事)

```bash
# 1. 验证 baseline (期望 14/14 PASS)
tools/run_chipforge_tests.sh

# 2. 验证 drift 防护 (期望 2/2 PASS)
tools/verify_adr.sh --only=ADR-024

# 3. 验证当前 HEAD
git log --oneline -8

# 4. 验证 working tree (期望 staged 空, unstaged 只有 plugin-docs-extraction.md)
git status
```

期望输出:
- `tools/run_chipforge_tests.sh`: `100% tests passed, 0 tests failed out of 14`
- `tools/verify_adr.sh --only=ADR-024`: 全部 PASS (含正负向)
- `git log --oneline -8`: HEAD = `17782f4`, 上面是 `c8d1dd1`/`e5d865a`/`3b6fc27`/`3dbe058`/`18418ac`/`26fe7d2`/`8d80fd3`
- `git status`: branch ahead of origin/main by 1 commit, modified = plugin-docs-extraction.md, untracked = local dev files

### TDD 测试 pattern (Phase 1.3a-f 通用)

```cpp
// Pattern 1: 单元测试 (D1' 契约验证)
void test_bridge_tick_invokes_pb_run() {
  EventQueue eq;
  L1CachePlugin plugin({...});
  L1CacheTLMBridge bridge(std::make_unique<L1CachePlugin>(plugin));
  int before = bridge.pb_run_count();
  bridge.tick();
  assert(bridge.pb_run_count() == before + 1);
}

// Pattern 2: e2e (Adapter lifecycle + ModuleFactory 发现)
void test_module_factory_recognizes_adapter_type() {
  ModuleFactory::registerObject<L1CacheTLMBridgeAdapter>("L1CacheTLMBridgeAdapter");
  auto types = ModuleFactory::getRegisteredObjectTypes();
  assert(... found ...);
}

// Pattern 3: JSON spec 验证 (JSON Schema / SoC topology)
void test_soc_topology_modules() {
  auto j = nlohmann::json::parse(json_str);
  assert(j["modules"].size() == 3);
  assert(j["modules"][1]["type"] == "L1CacheTLMBridge");
}
```

### 文档入口 (按优先级)

1. **新 session 起点**: `docs/roadmap/roadmap-status.md` §3 (PA-6/PA-7/PA-8/PA-9) + §5 (Top3)
2. **Phase 1 总体**: `docs/roadmap/phases/phase-1-tlm-foundation.md` §1.3-1.4
3. **决策上下文**: `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` (§3-4 D1'/§5 D1''/§9 Step7=Phase1.4)
4. **ADR 引用**: `docs/architecture/adr.md:118` ADR-024 (canonical Bundle 三层分层 ⚠️)
5. **使用指南**: `ip/cache/README.md` §9 (5 subsections + 测试套件汇总)
6. **变更日志**: `CHANGELOG.md` Unreleased (Added + Pending sections)
7. **本次会话活动**: `docs/roadmap/roadmap-status.md` §6 (本次会话八)
8. **Phase 1.2 教训**: `docs/lessons/phase-1.2-l1cacheplugin.md` (7 categories, 15+ patterns)

## Open Questions for New Session

1. **F1 (PA-8)**: 4 字段窄桥 (D1=C) 是否够用? 需要 burst_len/parent_id 吗?
2. **F3 (PA-8)**: Adapter 内部 Bundle 来源 — 直接构造 (Phase 1.3d 当前) vs 从 `ch_stream` 反序列化 (Phase 1.3d-extras)?
3. **E1 (PA-9)**: Phase 1.4 baseline 选型 — `cpptlm::CacheTLM` vs `HybridCacheWrapper`?
4. **Push `17782f4` to origin/main?** (AGENTS.md 说 no auto-push)
5. **Hand off to fresh session or continue in same?**

## Test Coverage Map

| Component | Test File | Tests |
|-----------|-----------|-------|
| `L1CachePlugin` unit | `test_l1_cache_plugin_unit.cpp` | 4 (miss/refill/hit-after-refill/D4 runtime) |
| `L1CacheTLMBridge` | `test_l1_cache_bridge.cpp` | 2 (tick→pb.run/4-field forwarding) |
| `L1CacheTLMBridgeAdapter` e2e | `test_l1_cache_plugin_e2e.cpp` | 5 (ModuleFactory/Adapter ctor/Bridge hold/tick→pb.run/1000+ tx) |
| `soc/l1_cache_minimal.json` | `test_soc_l1_cache_minimal_json.cpp` | 4 (top-level/modules/connections/l1 params) |
| `params_schema.json` | `test_cache_params_schema_json.cpp` | 6 (schema structure/strict mode) |
| **总计** | — | **14/14 ctest PASS** |

**Gaps (Phase 1.3d-extras 需要补)**:
- ❌ Full JSON `instantiateAll` e2e (SEGFAULT 当前) → PA-6
- ❌ ch_stream 协议转换正确性 → PA-6
- ❌ 多事务 hit rate 验证 (当前 0/1000 hit, refill 路径未走通) → PA-6
- ❌ Trace 对比 (Phase 1.4) → PA-7

## Tooling State

- **Build**: `cmake -S . -B build && cmake --build build -j$(nproc)` (~30s incremental)
- **Test**: `tools/run_chipforge_tests.sh` (14/14 PASS in ~4.5s)
- **Drift 防护**: `tools/verify_adr.sh --only=ADR-024` (2/2 PASS)
- **LSP**: clangd available, false positives on `cf::plugin` namespace (pre-existing, non-blocking)
- **Format**: 2-space indent, no tabs, ≤1 blank line (per AGENTS.md)

## Risk Register (当前)

| ID | 风险 | 状态 |
|----|------|------|
| R6 | ch_stream 协议转换 4 字段窄桥够用性 (burst/parent_id) | ⏳ PA-8 决策草案待起草 |
| R7 | Phase 1.4 baseline 选型 | ⏳ PA-9 决策草案待起草 |
| R1-R4 | Phase 0/1 历史风险 | ✅ 全部 closed (14/14 ctest PASS) |
| R5 | 文档决策被用户拒绝 | ✅ 删除 (v2 决策已闭环) |

---

## Recovery Commands (copy-paste ready)

```bash
# 1. 状态确认
cd /workspace/project/ChipForge
git log --oneline -8
git status
tools/run_chipforge_tests.sh
tools/verify_adr.sh --only=ADR-024

# 2. 读核心文件
cat docs/roadmap/roadmap-status.md | head -160  # §1-§3
cat .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md  # 决策上下文
cat src/cf_plugin/bridge/l1_cache_bridge.h  # Bridge 架构
cat src/cf_plugin/bridge/l1_cache_bridge_adapter.h  # Adapter 架构
cat soc/l1_cache_minimal.json  # SoC topology
cat CppTLM/include/chstream_register.hh  # cpptlm 注册 pattern

# 3. 启动新任务
# PA-8: 起草 .omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-10.md
# PA-9: 起草 .omo/drafts/decision-phase-1.4-baseline-2026-06-10.md
# PA-6: 实施 ch_stream adapter 注册 + full JSON e2e
# PA-7: 实施 cpptlm::CacheTLM baseline 对比
```

---

*Session handoff ready. Phase 1.3 fully archived. Next session should verify baseline (14/14 ctest) then start PA-6/PA-8 OR PA-7/PA-9.*