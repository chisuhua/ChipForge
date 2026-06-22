# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## v0.0.6 (2026-06-21) - m4g-extend-tid-and-hooks

> **OpenSpec change**: `m4g-extend-tid-and-hooks` (详见 `openspec/changes/archive/2026-06-21-m4g-extend-tid-and-hooks/`)
> **目的**: M4G 二阶前向兼容锁 (硬前置解锁 M5-DSE 2-wide superscalar, 节省 ~200 LOC Phase 5+ 重构)
> **战略依据**: `ip/cpu/docs/research/post-m4g-strategic-decision-2026-06-20.md` (Option B 战略决策)

### Added (Gap A: tid plumbing via `set_tid`)
- `include/cf/plugin/plugin_base.h` — `PluginBase::set_tid(std::uint8_t)` 虚函数 (默认 no-op, 避免 3rd-party plugin break)
- `ip/cpu/plugins/reg_file.h` — `RegFilePlugin::set_tid` override + `tid_` 成员 + `current_tid()` 访问器
- `ip/cpu/plugins/hazard.h` — `HazardPlugin::set_tid` override + `tid_` 成员
- `ip/cpu/plugins/branch_predictor.h` — `BranchPredictorPlugin::set_tid` override + `tid_` 成员
- `include/cf/plugin/pipe_builder.h` — `n_threads_` 成员 + `set_n_threads()` setter + `run()` per-tid 循环
- `ip/cpu/cpu_factory.h` — `CPUConfig::n_threads` 字段 (默认 1) + `build_cpu()` 注入 `set_n_threads`

### Added (Gap B: OoO commit primitive documentation)
- `include/cf/plugin/pipe_builder.h:104-138` — 6 行注释块: `register_commit_hook` + `commit_storages` = OoO 提交原语; `CtrlLink::flush_when` = mispredict-squash 原语; 引用 `dse_architecture_v2_design_research.md §3 E.1` (ROB 设计) 作为 Phase 5+ consumer

### Added (Gap C: COMMIT stage naming)
- `ip/cpu/docs/multi_isa_architecture.md §2.4` — 5-stage 表新增 6th `commit` 行 (COMMIT 阶段名锁定, 避免 Phase 5+ 在 `at_stage("retire")` vs `at_stage("commit")` 碎片化)

### Added (Tests: 8 RED→GREEN cases)
- `tests/cpu/test_forward_compat.cpp` — `PluginBase::set_tid_default_noop` + `set_tid_overridable` + `RegFileSetTidStoresTid` + `HazardSetTidStoresTid` + `BranchPredictorSetTidStoresTid` + `PipeBuilderRunCallsSetTidDefaultOnce` + `PipeBuilderRunCallsSetTidPerThread` + `PipeBuilderRunDispatchesStagesPerTid`

### Impact
- **0 行为变化**: n_threads=1 默认 byte-identical, M4G baseline 不退化
- **基线**: 36/36 ctest PASS (10 已有 + 8 新增 = 18/18 in test_forward_compat)
- **变更规模**: 8 文件 +217/-27 LOC (生产 ~84 LOC + 测试 133 LOC)
- **breaking 变更**: 0 (set_tid 默认 no-op 兼容)
- **下一里程碑**: 启动 M5-DSE 2-wide superscalar (`openspec/changes/m5-dse-superscalar/` 4/4 artifacts ready)

## v0.0.4 (2026-06-18) - cache-policy-foundation (DRAFT, archive 等待重写后落地)

> **OpenSpec change**: `cache-policy-foundation`（详见 `openspec/changes/cache-policy-foundation/`）
> **状态**: 本条目为 v0.0.4 占位，描述 cache-policy-foundation v2 重写后的预期落地内容
> **目的**: 落地 A7 架构债务（`ip/cache/policies/` 子目录缺失 + L1CachePlugin 不可插拔替换策略）+ 修正 L1Cache 容量注释错误（32KB→16KB）

### Changed（L1Cache 容量注释修正）
- `ip/cache/tlm/L1CachePlugin.h` L18 注释：`256 sets × 64-byte = 32KB L1` → `256 × 1 way × 64B = 16KB L1 (direct-mapped)`（原 32KB 是 8-way 误算；实际 RAM = 16384 字节 = 16KB）

### Added（落地可插拔替换策略接口）
- `ip/cache/policies/replacement_policy.h` — `cf::ip::cache::policies::ReplacementPolicy` 抽象基类（4 虚方法 + 1 工厂方法）
- `ip/cache/policies/no_replacement_policy.h` — 默认 no-op 实现，保持 Phase 1.3 行为零变化
- `ip/cache/policies/lru_policy.h` — LRU reference implementation（1-way 简化，Phase 1.5 L2CachePlugin 整体替换）
- `tests/cache/test_replacement_policy.cpp` — 5 单元测试（factory-create-LRU / factory-create-None / factory-unknown-throws / LRU-on-access-increments / NoReplacement-victim-returns-zero）

### Changed（L1CachePlugin 集成注入点）
- `ip/cache/tlm/L1CachePlugin.h/.cpp` — 构造函数签名扩展：`explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr)`；`lookup` 阶段 `at_stage` 回调内调用 `policy_->on_access(set, way)`

### Impact
- **向后兼容**: 默认 `nullptr` policy → `NoReplacementPolicy` 行为等价于 hard-coded
- **基线**: 16/16 ctest PASS（v0.0.5 后）+ 5 新增 = 21/21 PASS
- **与 v0.0.5 协作**: 测试放 `tests/cache/`（v0.0.5 约定）；不重建 `ip/cache/test/`
- **修复 v1 已知问题**: 详见 `openspec/changes/archive/2026-06-18-cache-policy-foundation-v1-original/` 的 9 项问题

## v0.0.3 (2026-06-18) - ip-catalog-status-correct (DRAFT, archive 等待重写后落地)

> **OpenSpec change**: `ip-catalog-status-correct`（详见 `openspec/changes/ip-catalog-status-correct/`）
> **状态**: 本条目为 v0.0.3 占位，描述 ip-catalog-status-correct v2 重写后的预期落地内容
> **目的**: 修正 `docs/architecture/ip-catalog.md` IP 状态表与实际代码对齐；L1Cache 状态从 `Phase 1.2 L1D` 修正为 `Phase 1.3 unified 16KB`

### Changed（L1Cache 状态修正）
- `docs/architecture/ip-catalog.md` L1Cache 行：状态从 `🟡 TLM 实现中 (Phase 1.2 L1D)` → `🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)`
- `ip/cache/README.md §4` "可插拔策略"表格：`256 sets × 64-byte cache line = 32KB L1` → `256 × 1 way × 64B = 16KB L1 (direct-mapped)`
- `ip/cache/README.md §5` "配置参数"表格：`capacity_kb` 默认值 32 → 16

### Added（IP 状态表补全 2 列）
- `docs/architecture/ip-catalog.md` IP 索引表新增"实现范围"列（8 个 IP 全部填写）
- `docs/architecture/ip-catalog.md` IP 索引表新增"实施预计"列（5 个零代码 IP 指向 v0.0.5 STATUS.md + roadmap 路径）

### Impact
- **无运行时影响**：仅文档同步
- **与 v0.0.5 协作**: 零代码 IP 实施预计引用 v0.0.5 STATUS.md，不重复声明；不修改 `ip/README.md`（v0.0.5 STATUS 约定段已含 7 IP 状态表）
- **不新建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 `IP_STATUS_TEMPLATE.md` 已覆盖零代码 IP）
- **修复 v1 已知问题**: 详见 `openspec/changes/archive/2026-06-18-ip-catalog-status-correct-v1-original/` 的 4 项问题

## v0.0.5 (2026-06-17) - empty-directory-cleanup

> **OpenSpec change**: `empty-directory-cleanup`（详见 `openspec/changes/empty-directory-cleanup/`）
> **目的**: 清理项目结构噪音（22 个仅含 .gitkeep 的空目录），建立 IP 状态目录约定（`STATUS.md` 模板），明确"测试在 `tests/<ip>/` 而非 `ip/<ip>/test/`"的不变式。

### Removed（清理空目录噪音）
- `ip/memory/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/interconnect/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/peripheral/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/cpu/test/` — 1 个 README-only 目录（README 移到 `ip/cpu/docs/verification.md`）

### Added（建立 IP 状态目录约定）
- `ip/memory/STATUS.md` — PLANNED 变体（0 LOC, Phase 2+）
- `ip/interconnect/STATUS.md` — PLANNED 变体（0 LOC, Phase 2+）
- `ip/peripheral/STATUS.md` — PLANNED 变体（0 LOC, Phase 3+）
- `ip/tilecore/STATUS.md` — INITIAL DESIGN 变体（有 docs/architecture.md, Phase 5+）
- `ip/tilecopy/STATUS.md` — INITIAL DESIGN 变体（有 docs/architecture.md, Phase 5+）
- `docs/templates/IP_STATUS_TEMPLATE.md` — 3 个变体（PLANNED / INITIAL DESIGN / PARTIAL）的可复用模板
- `ip/README.md` 顶部加 "STATUS 约定" 段，列出 7 个 IP 的状态表

### Changed（同步文档 + 修正 CPU IP 目录结构）
- `src/cf_plugin/CMakeLists.txt` L30 后插入注释：明确 cf_plugin 单元测试在 `tests/framework/`
- `ip/cpu/README.md` 顶部加 "测试位置" 段：明确 CPU 测试在 `../tests/cpu/`
- `ip/cpu/test/README.md` → `ip/cpu/docs/verification.md`（移动而非删除，保留验证规范文档）

### Impact
- **0 运行时影响**：仅清理空目录 + 文档同步
- **变更规模**: 12 个 .gitkeep 目录删除 + 5 个新 STATUS.md + 1 个新 docs/templates/ + 3 个文档修改

## v0.0.2 (2026-06-17) - doc-code-realignment

> **OpenSpec change**: `doc-code-realignment`（详见 `openspec/changes/doc-code-realignment/`）
> **目的**: 修复 8 项文档/代码一致性漂移（CRITICAL 3 项 + HIGH 5 项），建立"0 幽灵引用"基线并以 CI 脚本固化。

### Removed（消除 3 项 CRITICAL 漂移）
- `soc/riscv_virt.json`（引用 7 个不存在类 + `impl_mode` 字段无消费者，不可运行）
- `ip/cpu/cpu_factory.cpp`（12 行 stub，保留 `cpu_factory.h` 声明供 Phase 2+ 实施）

### Changed（重写/更新 5 项 HIGH 漂移）
- `docs/architecture/overview.md` §"SoC 层是 IP 组合器" 改为指向 `soc/l1_cache_minimal.json` 真实工作示例（不再描述虚构的 `RiscvVirtSoC.h/cpp`）
- `docs/architecture/overview.md` §"ch_stream 接口即 ISA 无关层" 改为 "Phase 1.4+ Future Work" 占位段（当前 1 个 CPU IP，无法"验证" ISA 无关性）
- `docs/architecture/interface-design.md` §1.0 增加 "已实现 POD vs 设计目标 bundle_base" 对照表（明确 Phase 1 当前是 POD，与 §"所有 Bundle 继承 bundle_base" 段落对照）
- `soc/README.md` 顶部说明改为"目前 2 个 L1Cache 验证配置；RISC-V virt 推迟到 Phase 2+ 实施"
- 7 个文档中 8 个幽灵类名（`RiscvIssTlm` / `L1CacheTlm` / `BusMatrixTlm` / `DramTlm` / `UartTlm` / `ClintTlm` / `PlicTlm` / `RiscvCoreRtl`）全部替换为 Plugin 风格命名 / 已注册 CppTLM 类名 / "Phase X+ 实施" 占位

### Added
- `docs/architecture/adr/ADR-041-bridge-tick-pattern.md` — 明确 Bridge 适配层允许 `tick()` 模式的边界条件（业务 Plugin vs Bridge 适配责任划分）
- `docs/architecture/adr.md` 插入 ADR-041 摘要 + §3 G 详细记录 + 交叉引用 ADR-025/037/040
- `tools/verify_no_ghost_refs.sh` — CI 防漂移脚本（可执行权限 755 + bash 严格模式 + 8 个类名 grep + 排除 `openspec/changes/` / `CHANGELOG.md` / `.omo/drafts/`）

### Impact
- **0 运行时影响**：仅文档/JSON 同步，不改任何运行时行为
- **0 API 变更**：仅删除不可运行文件
- **CI 影响**：新增 grep 检查脚本，阻断任何带幽灵类名的 .md/.json/.h/.cpp
- **基线提升**：文档/代码一致性从约 50% 提升到 80%+（以 `tools/verify_no_ghost_refs.sh` exit 0 为准）

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