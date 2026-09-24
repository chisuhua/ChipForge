# 路线图执行状态跟踪

> **最后更新**: 2026-09-24 (本次会话: **A+C Hybrid 战略落地 + 3 个 OpenSpec initiative 创立 + 7 个 openspec change proposals 落地 + sync_strategy_status.sh 实装 (替代 stub) + strategy §7 自动派生 + Phase 1.5 Wave 3-4 债务清理路径锁定**)
> **战略入口**: [`docs/roadmap/strategy/a-plus-c-hybrid.md`](strategy/a-plus-c-hybrid.md) — A+C Hybrid 战略 (Phase 1.5 Wave 3-4 清债 → Phase 2 产品化前置)
> **当前可启动**: ① **P0#1 cpu-pipeline-canonical-ordering-assert** (P0 即时启动, 1-2 周) ② **P1#3 mmu-paddr-consume-and-real-memory** (P1 一月内, 依赖 P0#1) ③ **P1#4 plugin-framework-cycle-precision** (独立, 可并行)
> **更新时机**: 每周一 / 阶段切换时 / 重大决策落地后
> **权威源**: `docs/roadmap/phases/*.md` (框架级 Phase 0/6/6d) + `soc/cpu/docs/roadmap/*.md` (CPU SoC Phase 1-5) + `.omo/plans/*.md` + `.omo/drafts/*.md` + `openspec/changes/*/proposal.md`
> **本文件目的**: 不重复阶段文档的任务清单,只跟踪执行状态、阻塞和下一步

> **A+C Hybrid 战略落地摘要 (2026-09-24)**:
> - **战略选择**: A+C 混合 (Wave 3-4 清债 → Phase 2 产品化前置), 否决 B 工具化 (过早) / 否决纯 A (不清债会重复踩坑)
> - **3 个 OpenSpec initiative**: `wave3-cpu-pipeline-debt` (v0.4.0) + `wave3-mmu-real-memory-and-cycle` (v0.5.0) + `wave4-csr-cache-dse` (v0.6.0, 占位)
> - **7 个 openspec change proposals**: P0#1 canonical-ordering-assert + P0#2 fix-rv32ui-load-width (回顾性: v0.6.0 commit 8909165 已完成) + P1#3 mmu-paddr-consume + P1#4 cycle-precision + P1#5 multi-cycle + P2#6 phase-1.5-wave-4 + P2#7 cache-phase1.5-4way
> - **`tools/sync_strategy_status.sh` 实装**: 从 openspec changes + initiative YAML 派生 strategy §7 表格 (替代 stub), `--dry-run` / `--json` 双模式
> - **3 个版本节点**: v0.4.0 (P0 清债) → v0.5.0 (P1 MMU 真端到端) → v0.6.0 (P2 Phase 1.5 毕业 → Phase 2 启动前置)
> - **Phase 6c/v0.6.0 验收报告引用**: Phase 6d 完整收官 (v0.5.0) + v0.6.0 静态配置 Result 范式 (ADR-047) 均已落地, Wave 3 启动门槛已就位

> **Phase 6c 完整收官摘要 (2026-09-16 → 2026-09-20)**:
> - **Phase 6c 完整落地**: `plugin-elaboration-substrate` OpenSpec change 已归档 (2026-09-17), `fix-5stage-mux-segv-elaboration` OpenSpec change 已归档 (2026-09-20)。CHANGELOG v0.3.0 (2026-09-17) + v0.3.1 (2026-09-20) 已发布。
> - **9 个 M1-M5 commit + 2 个 M6 修复 commit (共 11 个)**: W0+M1 (`25e2672`) + M2 Spike/M3/M4 (`9a03bb2`/`918e577`/`baa504b`/`3e8ada2`/`a38a1e4`/`19d4f5d`/`edad878`/`b68996a`) + M6 修复 (`61a642d`) + 测试修复 (`5fe72fe`)。总交付 ~2815 行代码。CHANGELOG v0.3.0 commit 清单为权威源。
> - **框架层 5 个 P0 组件完成双模化**: `uint_t.h` + `payload.h` + `pipe_builder.h` + `storage.h` + `ctrl_link.h`。CH_MEM 编译开关 `CF_PLUGIN_USE_CH_MEM` 实现零回归。
> - **端到端 PoC 通过**: `pipeline2_stall_matrix` 16/16 PASS (Verilog `always_ff @(posedge)` 真实生成) + `m3_poc_regfile`/`m3_poc_alu` 单元级 byte-equal + `cpphdl_poc_full_chain` 完整链路 + `m4_poc_5stage_simulator_tick` 12/12 PASS (M6 SEGV 修复后)。
> - **CI 门禁全绿**: `verify_adr.sh` 31 PASS / 0 FAILED / 0 STALE; `verify_plugin_decision.sh` 3+4/3 PASS; `check_plugin_portability.sh` v2.0 8/8 PASS。
> - **ADR 收口**: ADR-040 v2.0 (`CH_MEM 是新正道` 翻转 v1.0 `ch 渗透禁令`) + ADR-046 (`多周期协议引擎豁免 D4`, `CF_PLUGIN_USE_FSM_EXEMPT` 机制)。
> - **方法学验证**: cf::plugin 从"每周期仿真器"→ "elaboration DSL" 翻转成功 (7 大借鉴点全部落地)。SpinalHDL/VexRiscv plugin 设计方法学的 C++ 等价路径被 PoC 实证。

> **3 天进展摘要 (2026-06-21 → 2026-06-24)**:
> - **M4-DSE CpuFactory Real** 完整收官 (`m4-dse-cpufactory-real-M4.12` 分支, 17 commits ahead of main): 11 plugins 在 CpuFactory 中真实注册 (EARLY×2 + NORMAL×7 + LATE×2, **RetirePlugin 推迟 Phase 5+**), BranchPredictor 编译时 3 模板参数 → 运行时 1 模板参数, cpu_sim PicolibcHostMemory + 最小 RV32I 解释器 + `--elf` flag, add.elf 在 3/5/7/10-stage 全部 tohost=1 (M5.11 byte-identical 保留), 576-config DSE sweep 全部 tohost=1 (Pareto 空集 degenerate-by-design 详见 baseline doc)。**M4-DSE 7/8 PASS** (M4.13 reg_file writeback→retire refactor 未实施, 推迟 Phase 5+ plugin stub → real)。
> - **Honest performance baseline** 创建 (`docs/performance/m4-cpufactory-real-baseline.md`, 317 行, 5 章节): 完整记录 Phase 1.5 能力边界 — `ipc=0.0` 是 Phase 1.5 known limitation (ADR-042 Plugin 推迟 + retire counting 推迟 Phase 5+), 非 regression。Phase 5+ 升级路径明确 (retire counting / plugin stub → real / sail-riscv differential / RTL 综合)。
> - **OpenSpec change archived** 2026-06-23: `openspec/changes/archive/2026-06-23-m4-dse-cpufactory-real/` 4/4 artifacts + 8 requirements spec。`openspec validate --specs` 13/13 PASS。
> - **ADR renumber**: ADR-040 (Plugin Deferral) → ADR-042 (为 ADR-041 Bridge Tick 留位); 新增 ADR-037 (Plugin 作为设计范式) + ADR-043 (CI 强制架构门禁); 4 cross-reference 文件更新。`adr.md` 实现决策 31→34 (78% → 79%)。
> - **验证门**: ctest 43/43 PASS, openspec validate 13/13 PASS, 4/4 add.elf 集成测试 PASS, 576/576 sweep `tohost=1`, doc_link_check 新增 0 broken。

> **8 天进展摘要 (2026-06-13 → 2026-06-21)**:
> - **M4G-extend-tid-and-hooks** 落地 (commit `ec6ee4f`, 2026-06-21): PluginBase::set_tid + 3 plugin override + PipeBuilder n_threads + OoO commit_hook 文档 + COMMIT 阶段名。8 文件 +217/-27 LOC, 18/18 ctest PASS, 0 行为变化。
> - **M5-DSE OpenSpec change** 启动 (`openspec/changes/m5-dse-superscalar/`): 4/4 artifacts ready (proposal + design + 3 specs + tasks 46 项)。覆盖 M5.10-M5.19 (3/5/7/10 拓扑 + 2-wide superscalar + MUL 多周期 + DSE sweep 工具链)。
> - **Phase 2 工具链 v1.1** Accepted: F1-F8 全 8 决议批准 (2026-06-21), `openspec/changes/m5-dse-superscalar` 解锁。
> - **验证门**: verify_adr 30→31 PASS / STALE 1→0 (ADR-033 fix), verify_no_ghost_refs PASS, openspec validate 1 passed.
> - **清理**: 2 个 git stash dropped (1 month old, redundant/obsolete), empty-directory-cleanup 归档异常 7/20 → 20/20 闭合。

> **⚠️ Phase 1.4 范围修正 (2026-06-13)**: roadmap §1.4 + §2.1 描述的 "cpptlm::CacheTLM baseline 对比" 经用户对话澄清 + Metis 验证为**错误解读**(`cpptlm::CacheTLM` 是 stub, `std::map` 全关联无淘汰, 无法配置为 256×64B direct-mapped)。Phase 1.4 真实目的 = **L1CachePlugin 设计方法学复盘** (`.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md`, DECISION-2026-06-13-02, F1.A + 6 维度 + 3 边界 + 6 B2 模式 + 3 B3 局限 + 5 Phase 6 任务)。性能基线对比推迟到 Phase 2+ (需 `HybridCacheWrapper` + BUILD_RTL=ON)。

---

## 1. 状态总览

| 阶段 | 里程碑 | 状态 | 进度 | 阻塞项 | 下一交付物 |
|------|--------|------|------|--------|----------|
| Phase 0 | M0 - 脚手架可运行 | ✅ Completed | 100% (5/5 P0) | 无 | Phase 1 启动 |
| Phase 1 | M1 - L1CachePlugin Hello World | ✅ 核心 | 100% (1.1+1.2+1.3全部+1.4方法学复盘+M4G-extend+M4-DSE 7/8 完成, MMU/VIPT/CPU Pipeline 4 change 归档) | 无 | Phase 1.5 启动 |
| Phase 1.5 | Wave 1+2 已完成 + Wave 3+4 待 | 🚧 In Progress | Wave 1 (riscv-tests 30/40 PASS, v0.2.2 修真至 40/40) + Wave 2 (soc-cpu-l1-mmu-demo 6/6 PASS + 4 hidden bug 修复) ✅; Wave 3+4 见 [strategy](strategy/a-plus-c-hybrid.md) | 无 | A+C Hybrid 战略: 3 initiative + 7 change proposals 2026-09-24 落地 |
| Phase 1* | M1 (legacy, 已取代) | Superseded | - | 无 | 已删除 (2026-06-09) |
| **A+C Hybrid Initiative** | **Wave 3-4 清债 → Phase 2 产品化前置** | **🚀 Launched** | **3/3 initiative 创立 + 7/7 change proposals 落地** (2026-09-24) | 无 | P0#1 canonical-ordering-assert 立即启动 |
| Phase 2 | M2 - ISA 全覆盖 | Not Started | 0% | 依赖 Phase 1.5 Wave 4 archive (CSR/exception 落地) + RV64 切换 | riscv-tests RV64GC + RISCOF |
| Phase 3 | M3/M4 - FreeRTOS/Zephyr | Not Started | 0% | 依赖 Phase 2 | CLINT/PLIC IP 完善 |
| Phase 4 | M5 - Linux 启动 | Not Started | 0% | 依赖 Phase 3 | Sv39 + VirtIO Block |
| Phase 5 | M6/M7/M8/M10 - RTL | Not Started | 0% | 依赖 Phase 4 | L1CacheRtl (CppHDL) |
| Phase 6a | M6a - 调度算法 + JSON 解析 | Not Started | 0% | 依赖 Phase 6d 部分验证 | `PipeBuilder::auto_schedule` |
| Phase 6b | M6b - CompareDriver + ScoreBoard | Not Started | 0% | 依赖 Phase 6d 端到端 | TLM↔RTL 对比基线 |
| **Phase 6c** | **M6c - 完整 RTL 生成 (VerilogCodeGen 集成)** | **✅ Completed** | **100% (M1-M5 + M6)** | **无** | **已发布 v0.3.0 + v0.3.1** |
| Phase 6d | M6d - 5-stage Pipeline CH_MEM + Verilator + MMU FSM | ✅ Completed (v0.5.0) | 100% (6d.1-6d.8 全完成) | 无 | Wave 3 P1#3 mmu-paddr-consume 验证 Phase 6d 端到端 |

> *Phase 1* = 旧 `phase-1-foundation.md` (已于 2026-06-09 PA-3 git rm, 仅在此处作历史记录), 现使用 `phase-1-tlm-foundation.md`

---

## 2. 详细状态(按依赖顺序)

### Phase 0 - Plugin 最小脚手架

- **状态**: ✅ Completed (5/5 P0 组件已交付, 51/51 单元测试 PASS, 7/7 ctest PASS, 退出标准 v2 全部达成)
- **预估工时**: 14-16 工作日(2.5-3 周)
- **已用**: 1 个 session 集中实施(2026-06-08)
- **依赖**: 无
- **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`

**P0 任务**(详见 `phase-0-plugin-scaffolding.md`):

| # | 任务 | 工时 | 状态 | 单元测试 |
|---|------|------|------|---------|
| 1.1 | PluginBase 接口 | 2d | ✅ 完成 (2026-06-08) | 7/7 PASS |
| 1.2 | Payload&lt;T&gt; 类型安全 Key | 2d | ✅ 完成 (2026-06-08) | 8/8 PASS |
| 1.3 | PipeNode 节点 | 3d | ✅ 完成 (2026-06-08) | 14/14 PASS |
| 1.4 | PipeBuilder 编排器 | 4d | ✅ 完成 (2026-06-08) | 11/11 PASS |
| 1.5 | CtrlLink 控制 API | 3d | ✅ 完成 (2026-06-08) | 11/11 PASS |
| M1.5 | PipeArbitration | — | ✅ 完成 (Phase 1.5) | 7/7 PASS |
| M1.6 | Storage (array_store) | — | ✅ 完成 (ADR-040) | 5/5 PASS |
| M1.7 | Coexistence + HelloPlugin | — | ✅ 完成 | 5+3=8/8 PASS |

**进度**: 5+3/5+3 P0+M1 组件完成; **71 个框架测试用例通过** (100% ctest pass rate, 44/44)

### Phase 1 - TLM Foundation (L1CachePlugin)

- **状态**: In Progress (~95%, 1.1 + 1.2 + 1.3a + 1.3b + 1.3c + 1.3d + 1.3d-extras + 1.3e + 1.3f 完成; Phase 1.3 全部子任务落地, 8/8 ✅; Phase 1.4 方法学复盘完成; M4G-extend + M4-DSE 7/8 完成)
- **依赖**: Phase 0
- **预估工时**: 7-9 工作日(~1.5 周); Phase 1.3 单项重估 **1 天 → 4.5 天** (v2 决策草案 §8)
- **已完成** (2026-06-10):
  - 1.1 Bundle 定义 (`bundles/mem_bundles.h` 6 个 Bundle, D4 合规) + 9 个单元测试 PASS (`073402c`)
  - `bundles/README.md` 设计原则文档
  - 1.2 L1CachePlugin 实现 (lookup + refill 两阶段, Plugin-style, D4 合规) (`e8deacc`)
    - `ip/cache/tlm/L1CachePlugin.{h,cpp}` (256 sets × 64B line, direct-mapped)
    - 4 个单元测试 (miss / refill / hit-after-refill / D4 runtime) PASS
  - 1.2 配套: `docs/lessons/phase-1.2-l1cacheplugin.md` 7 类 15+ 模式教训 (`2a81938`)
  - 1.3a L1CacheTLMBridge 框架层桥接 (D1=C + D1'=末尾) (`26fe7d2`)
    - `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (Bridge 持有 PipeBuilder + Plugin)
    - `src/cf_plugin/tests/test_l1_cache_bridge.cpp` (2 tests: tick invokes pb.run / 4-field forwarding) PASS
    - 11/11 ChipForge ctest PASS in 3.90s
  - 1.3e BundleMapper drift 防护 (`verify_adr.sh` ADR-024 增强)
    - 拒绝 `bundles/bundle_mapper.h` 提前实现 (canonical 设计推迟到 Phase 5/6)
    - 正/负向测试均通过
  - 1.3b `soc/l1_cache_minimal.json` 最小 SoC 拓扑 spec
    - `traffic_gen → l1 (L1CacheTLMBridge) → mem` 三模块管道
    - 4 个结构验证测试 (top-level fields / modules / connections / l1 params) PASS
    - 12/12 ChipForge ctest PASS
  - 1.3c `ip/cache/configs/params_schema.json` (L1CachePlugin IP 配置 JSON Schema)
    - 4 核心 param required (num_sets/tag_bits/idx_bits/line_data_bits) + strict 模式
    - Defaults 匹配 L1CachePlugin geometry (256/20/8/512)
    - 6 个结构验证测试 PASS; 13/13 ChipForge ctest PASS in 4.11s
  - 1.3f `ip/cache/README.md` §9 Phase 1.3 使用指南
    - 5 个子章节 (Plugin / Bridge / JSON / Schema / 测试汇总 + 决策)
    - 9 个相对链接全部验证 OK
    - 文档完整支持 Phase 1.3 用户 (单元测试作者 / SoC 集成者 / 配置维护者)
  - 1.3d `L1CacheTLMBridgeAdapter` (cpptlm ModuleFactory 兼容适配层)
    - 解决 Bridge 构造签名与 ModuleFactory::registerObject 不兼容问题
    - 5 个 e2e 测试 (ModuleFactory 发现 / Adapter 构造 / Bridge 持有 / Adapter::tick / 1000+ tx)
    - 14/14 ChipForge ctest PASS in 5.28s
    - Phase 1.3d-extras 推迟: ch_stream adapter 注册 + full JSON instantiateAll
  - 1.3d-extras (2026-06-13, 本次 commit):
    - `ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, ::bundles::CacheReqBundle, ::bundles::CacheRespBundle>` 静态注册
    - Adapter 内部 4 字段窄桥 (F1.A, D1=C 不变): `addr/data/is_write/id` ↔ `cf::bundles::CacheReq` POD
    - `soc/l1_cache_adapter_e2e.json` (full JSON instantiateAll spec)
    - `test_l1_cache_json_instantiate.cpp` (5 子测试): instantiateAll / 3 模块 getInstance / startAllTicks / 100 cycle / Bridge pb_run
    - 16/16 ChipForge ctest PASS in 4.91s
    - ADR-007 §2.3 L131 + §3 末尾 "实施更新 (2026-06-13)" 增补
- **Phase 1.3 v2 决策** (`8d80fd3` DECISION-2026-06-10-02 v2):
  - D1=C: Phase 1.3 保持 `cf::bundles::*` POD 不动, Bridge 做 4 字段窄桥 (addr/data/is_write/id)
  - D1'=末尾: Bridge `tick()` 末尾调用 `plugin_->pb.run()` (回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1)
  - D1''=不实现: BundleMapper 推迟 Phase 5/6, 加 `verify_adr.sh` drift 防护
  - D2=B: Bridge 在 `src/cf_plugin/bridge/`, 不在 `ip/`
  - D3=A: 仅 1.3 最小 e2e; 1.4 baseline 留到下次 session
- **Phase 1.3d-extras 决策** (`decision-phase-1.3d-extras-bridge-2026-06-13.md` DECISION-2026-06-13-01, F1-F5, 2026-06-13):
  - F1.A: ch_stream 转换走 Bridge 内部 4 字段↔POD 路径 (不在 Bundle 上重载)
  - F2: ChStreamAdapterFactory 静态注册 (Adapter .cpp 加载时自动执行)
  - F3: test_l1_cache_json_instantiate 5 子测试 (拓扑连通 + 协议转换路径打通)
  - F4: ADR-007 §2.3 + §3 末尾 "实施更新" 增补 (R6 风险保留, 通用桥接 Phase 5 实施)
  - F5: E1-E5 退出标准 (16/16 ctest + 100 cycle 推进 + Bridge pb_run 验证)
- **下一步**: **Phase 1.4 全部完成 (PA-7 方法学复盘 v1 文档 + PA-9 决策草案 DECISION-2026-06-13-02)**. Phase 1 退出标准全部达成 (PA-1 ~ PA-9 全部 ✅), Phase 2 启动门槛就绪.

**关键约束**(D4 强制): 业务代码无 `tick()`、Bundle 字段用 `uint_t<N>`、所有阶段用 `at_stage()`

**Phase 1.3 关键参考文档**:
- v2 决策草案: `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` (`8d80fd3`)
- canonical Bundle 设计: `docs/architecture/overview.md:125`, `docs/architecture/interface-design.md`
- ADR-024 Bundle 三层分层 (⚠️ Mapper 未实现): `docs/architecture/adr.md:118`
- 1.2 lessons: `docs/lessons/phase-1.2-l1cacheplugin.md` (TDD 教训可复用)

### Phase 2 - Bare-metal 测试套件

- **状态**: Not Started (0%)
- **依赖**: Phase 1
- **关键交付**: riscv-tests RV64GC 全部 PASS + RISCOF 合规认证

### Phase 3 - RTOS

- **状态**: Not Started (0%)
- **依赖**: Phase 2
- **关键交付**: FreeRTOS + Zephyr 稳定运行

### Phase 4 - Linux 启动

- **状态**: Not Started (0%)
- **依赖**: Phase 3
- **关键交付**: OpenSBI → Linux Kernel → Shell 交互

### Phase 5 - RTL 协同验证

- **状态**: Not Started (0%)
- **依赖**: Phase 4
- **关键交付**: L1CacheRtl + COMPARE 模式 + Verilator 仿真

### Phase 6 - 完整 PipeBuilder 框架 (拆分 6a/6b/6c/6d)

#### Phase 6c - 完整 RTL 生成 (VerilogCodeGen 集成)

- **状态**: ✅ **Completed** (M1-M5 + M6 全部落地, 2026-09-20)
- **依赖**: Phase 0/1 Plugin-style IP 稳定 (L1CachePlugin + 11 个 CPU Plugin + 5-stage Demo)
- **关键交付**: cf::plugin 框架双模化 + 7 大借鉴点 + 端到端 PoC + Verilog 真实生成

**Phase 6c 任务** (详见 `docs/roadmap/phases/phase-6-declarative.md` §1 + `openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/tasks.md`):

| # | 任务 | 工时 | 状态 | 关键 commit |
|---|------|------|------|------|
| M0 | CppHDL 成熟度审计 + W0 PoC | 3d | ✅ 完成 (2026-09-16) | `25e2672` |
| M1 | 框架双模化 (uint_t/payload/pipe_builder) | 7d | ✅ 完成 (2026-09-16) | `25e2672` |
| M2 | Stage plumbing (Spike 1-7 + W3/W4) | 10d | ✅ 完成 (2026-09-17) | `9a03bb2`/`918e577`/`baa504b` |
| M3 | RegFile + IntAlu PoC + 6 Prereq | 14d | ✅ 完成 (2026-09-17) | `3e8ada2`/`a38a1e4`/`19d4f5d` |
| M4 | Decoder/Branch/Hazard + 5-stage 集成 | 14d | ✅ 完成 (2026-09-17) | `edad878`/`b68996a` |
| M5 | ADR 收口 + check_plugin_portability.sh v2.0 + ADR-046 | 7d | ✅ 完成 (2026-09-17) | (随 M4 commit) |
| M6 | 5-stage Simulator SEGV 修复 (PayloadStore fail-fast + factory pre-populate) | 1d | ✅ 完成 (2026-09-20) | `61a642d` |
| M6.1 | chbool context pollution 测试修复 | 0.5d | ✅ 完成 (2026-09-20) | `5fe72fe` |
| **总 Phase 6c 交付代码** | — | ~2815 行 | **100% 完成** | 11 commits (9 M1-M5 + 2 M6) |

**Phase 6c 验证门 (2026-09-20 实测)**:
- ✅ `verify_adr.sh`: 31 PASS / 10 EXPECTED_MISSING (Phase 1 提案) / 0 FAILED / 0 STALE
- ✅ `verify_plugin_decision.sh`: D4 + ADR-040 检查 3+4/3 全部 PASS
- ✅ `check_plugin_portability.sh` v2.0: **8/8 PASS** (含新增 Check 8 PayloadStore fail-fast)
- ✅ TLM baseline `[framework]`: 89/89 PASS, 275 assertions 零回归
- ✅ CH_MEM `[cpphdl]`: 6/6 PASS (15 assertions, W0 PoC 完整链路)
- ✅ CH_MEM `[chmem]`: 9/9 PASS (69 assertions, PayloadStore + 4 elaboration PoC)
- ✅ `pipeline2_stall_matrix`: 16/16 PASS (含 Verilog `always_ff @(posedge)` 真实生成)
- ✅ `m3_poc_regfile_elaborate`/`m3_poc_alu_elaborate`: 13/13 PASS (CH_MEM 单元级 + toVerilog)
- ✅ `m4_poc_5stage_simulator_tick`: 12/12 PASS (M6 SEGV 修复后)

**Phase 6c 推迟到 Phase 6d+ 的剩余项**:
- ⏸ riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` 端到端 CppHDL sim 验证 (缺 riscv64-unknown-elf-gcc 工具链在 build env)
- ⏸ Verilator / Yosys / iverilog 综合验证集成
- ⏸ M3/W6 byte-equal 完整版 (#95 cell put/get CH_MEM 不稳定) → 已通过 PoC 单元级验证
- ⏸ 5-stage DecoderPlugin + BranchPlugin + HazardPlugin 完整 CH_MEM (M4 仅骨架 + PoC)
- ⏸ MMU/PTW 多周期 FSM (依赖 `ch_state_machine` 简化实现 + Verilator, ADR-046 已锁定豁免机制)
- ⏸ L1Cache refill FSM (同 MMU/PTW)
- ⏸ Harness 迁移 (pb.run() → CppHDL sim runner / Verilator)

#### Phase 6d - 5-stage Pipeline CH_MEM + Verilator + MMU FSM (待启动)

- **状态**: Not Started (0%)
- **依赖**: Phase 6c 完成 ✅; Phase 6d prereqs (toolchain + ADR-037 v2.0 修订)
- **关键交付**: riscv-tests RV32I 5 指令 RTL sim tohost=1 + Verilator 集成 + MMU/PTW 多周期 FSM
- **OpenSpec change**: `phase-6d-rtl-verification` (待创建, 见 `openspec/changes/`)
- **Phase doc**: [`docs/roadmap/phases/phase-6d-rtl-verification.md`](phases/phase-6d-rtl-verification.md) (新创建)

#### Phase 6a/6b (长期, Phase 6d 之后)

- **Phase 6a**: PipeBuilder::auto_schedule 调度算法 + JSON `pipeline_stages` 解析 (Phase 6d 端到端验证后)
- **Phase 6b**: CompareDriver + ScoreBoard + TLM↔RTL 对比基线 (依赖 Phase 6d + Phase 6a 调度稳定)

---

## 3. 当前未决项(Pending Actions)

> **PA-1 ~ PA-5 (Phase 0 准入) 全部 ✅ 完成, 归档保留**。
> **PA-6 ~ PA-9 为新 session 入口**, 详见各项目前置条件 + 引用文档。

### 3.1 历史归档(Phase 0-1.3 准入完成)

| ID | 类型 | 项目 | 责任 | 状态 | 优先级 |
|----|------|------|------|------|-------|
| PA-1 | 文档 | M4: `adr.md` 新增 ADR-037 + 更新 ADR-025~036 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, v1.1) | P1 |
| PA-2 | 文档 | M5: `declarative-hybrid-framework.md` §12.0.3 责任归属表 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, v2.0.3) | P1 |
| PA-3 | 文档 | 处置 `phase-1-foundation.md`(旧 vs 新 phase-1-tlm-foundation.md) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-09, 文件已 git rm) | P2 |
| PA-4a | 构建 | CppTLM 集成 + 根 CMakeLists.txt 升级 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08) | P1 |
| PA-4b | 构建 | CppHDL 集成(阻塞: CppHDL 上游 tests 路径 bug) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, 上游修复后集成) | P2 |
| PA-5 | 验证 | V1: `tools/verify_plugin_decision.sh`(可选) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, 3/3 PASS) | P3 |

### 3.2 活跃未决项(新 session 入口)

| ID | 类型 | 项目 | 前置条件 | 状态 | 优先级 |
|----|------|------|---------|------|-------|
| **PA-6** | 实施 | **Phase 1.3d-extras**: ch_stream 协议转换 + full JSON `instantiateAll` e2e | ✅ **Completed (2026-06-13)**: 静态注册 `ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, ::bundles::CacheReqBundle, ::bundles::CacheRespBundle>` + Adapter 内部 4 字段窄桥 (F1.A) + `test_l1_cache_json_instantiate` 5/5 子测试 PASS + 16/16 ctest | ✅ Done | ~~P1~~ |
| **PA-7** | 实施 | **Phase 1.4**: L1CachePlugin 设计方法学复盘 v1 文档 | ✅ **Completed (2026-06-13, 本次会话十)**: `docs/methodology/plugin-style-design-methodology-v1.md` (352 行) — 6 维度 × 3 边界 + 6 B2 模式 + 3 B3 局限 + 5 Phase 6 任务链接。详见 `soc/cpu/docs/roadmap/phase-1-tlm-foundation.md §1.4` (E1-E5 重解读) | ✅ Done | ~~P1~~ |
| **PA-8** | 文档 | **Phase 1.3d-extras 决策草案** (`decision-phase-1.3d-extras-bridge-2026-06-13.md` 草案) | ✅ **Completed (2026-06-13)**: `.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md` (PA-6+PA-8 合并), F1-F5 决议, 状态改 Proposed v1 | ✅ Done | ~~P2~~ |
| **PA-9** | 文档 | **Phase 1.4 决策草案** (`.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md`) | ✅ **Completed (2026-06-13, 本次会话十)**: DECISION-2026-06-13-02, F1-F5 决议 (E1=A 复盘对象 / E2=6 维度 / E3=5 类输入 / E4=3 类边界 / E5=单例子深复盘) | ✅ Done | ~~P2~~ |

---

## 4. 风险与阻塞

| ID | 描述 | 影响阶段 | 缓解措施 | 状态 |
|----|------|---------|---------|------|
| R1 | D4 决策(Plugin-style 强制)被 C++17 静态类型系统拒绝 | Phase 0/1 | Phase 0 退出标准强制"端到端跑通最小 Plugin" | ✅ Phase 0+1 验证通过 (14/14 ctest PASS) |
| R2 | 脚手架工时低估(可能 4 周 vs 计划 2-3 周) | Phase 0 | 每周评估;必要时拆为 Phase 0a/0b | ✅ Phase 0 完成, 实际 1 session |
| R3 | CppHDL 集成阻塞(tests/CMakeLists.txt:38 路径 bug) | Phase 5+ | Phase 0-2 暂不需 CppHDL;Phase 5+ 再评估或用独立构建 | 已识别 + 缓解 |
| R4 | 命名冲突(halt_when vs stream_halt_when 等 4 项) | Phase 0 | 决策文档 §3.5 已识别 D6-D9 方案 | ✅ ADR-033 Accepted (2026-06-09) |
| **R6** | **Phase 1.3d-extras ch_stream 协议转换设计不确定**: 4 字段窄桥 (D1=C) 仅覆盖 addr/data/is_write/id;burst_len/parent_id/fragment_* 走 default | Phase 1.3d-extras / Phase 2 | ✅ **已闭环 (2026-06-13)**: `decision-phase-1.3d-extras-bridge-2026-06-13.md` F1.A (4 字段窄桥路径), PA-6 实施 + PA-8 草案同步完成. R6 仍跟踪: Phase 2 多拍/分片场景需重新评估, 升级路径明确为 BundleMapper (Phase 5/6) | ⏳ 监控中 (Phase 2+ 触发) |
| **R7** | **Phase 1.4 范围定位不确定**: 早期 roadmap 解读为"cpptlm::CacheTLM baseline 性能对比", 但 Metis 验证 cpptlm::CacheTLM 是 stub (std::map 全关联无淘汰, 无法配置为 256×64B), 性能对比无意义 | Phase 1.4 | ✅ **已闭环 (2026-06-13)**: 用户对话澄清 Phase 1.4 真实目的 = L1CachePlugin 设计方法学复盘 (DECISION-2026-06-13-02 F1.A). cpptlm::CacheTLM 性能基线对比方案明确推迟到 Phase 2+ (需 HybridCacheWrapper + BUILD_RTL=ON), 不阻塞 Phase 1 退出 | ✅ 闭环 |

---

## 5. 下一步建议(Top 2)

> Phase 1.3 全部子任务完成 (1.3a + 1.3b + 1.3c + 1.3d + 1.3d-extras + 1.3e + 1.3f, commits `26fe7d2`..`387b8ca`)。
> PA-6 (1.3d-extras) 与 PA-8 (决策草案) 已于 2026-06-13 完成。当前仅剩 PA-7 与 PA-9 待启动。

### 建议 1:A+C Hybrid 战略 P0#1 — cpu-pipeline-canonical-ordering-assert (1-2 周) — 🚀 Ready to Launch (2026-09-24)

**战略入口**: [`strategy/a-plus-c-hybrid.md`](strategy/a-plus-c-hybrid.md) — A+C Hybrid (Wave 3-4 清债 → Phase 2 产品化前置)

**前置条件** (全部已就绪):
- ✅ Phase 6c + 6d + v0.6.0 release 完成 (技术框架就位)
- ✅ A+C Hybrid 战略 3 initiative + 7 change proposals 落地 (战略层面就位)
- ✅ `tools/sync_strategy_status.sh` 实装 (状态自动派生)
- ✅ 当前 v0.6.0 测试基线: TLM 386/17 (pre-existing fail) + CH_MEM 43/43

**任务** (详见 [change proposal](../../openspec/changes/cpu-pipeline-canonical-ordering-assert/proposal.md)):
- 1.1 在 `ip/cpu/cpu_factory.h::register_early_plugins` 加 canonical-ordering static_assert
- 1.2 新建 `tests/cpu/integration/test_canonical_ordering.cpp` (故意错序注册触发 compile error)
- 1.3 新建 ADR-048 plugin-registration-canonical-order
- 1.4 4 个 `[cpu-integration]` test_*stage_riscv.cpp 加 expected_canonical_order 钩子
- 1.5 3 门禁全 PASS + openspec archive

**价值**:
- **解锁 MMU stall 真生效**: 当前 stall 机制依赖隐式调用顺序, 升级为显式契约
- **解锁 Wave 3 P1#3 mmu-paddr-consume-and-real-memory**: IBus/DBus 真消费 PADDR 有 stall 链保护前提
- **解锁 Wave 4 P2#6 phase-1.5-wave-4 (CSR/exception/mispredict)**: 路径无 canonical ordering 风险

### 建议 2:A+C Hybrid 战略 P1#3 — mmu-paddr-consume-and-real-memory (4-6 周, 依赖 P0#1)

**前置条件**:
- ⏸ P0#1 cpu-pipeline-canonical-ordering-assert archive (依赖)

**任务** (详见 [change proposal](../../openspec/changes/mmu-paddr-consume-and-real-memory/proposal.md)):
- 1.1 IBusPlugin/DBusPlugin 真消费 `pl::PADDR` (PADDR-first pattern)
- 1.2 PTW `advance_from_real_memory()` 替换 stub (新 `MemoryInterface` 抽象)
- 1.3 `[cpu-l1-mmu-demo]` 升级 8 用例 (含 PADDR 翻译验证)
- 1.4 新建 ADR-049 mmu-paddr-consumption-contract

**价值**:
- **SoC demo "真跑" 名副实**: MMU 翻译 → PADDR → 真实物理内存 (非 stub)
- **解锁 Wave 4 P2#6**: mmu_exit hook 真生效, exception 12/13/15 路由完整

### 建议 3:Phase 2 — bare-metal RV64GC 测试套件 (5-7 周, 依赖 Wave 4 archive)

**前置条件**:
- ⏸ Wave 4 (CSR/exception/mispredict) archive (预计 v0.6.0 ~2027-01)
- ⏸ RV64 切换工作量新评估 (XLEN=64)

**任务** (详见 `soc/cpu/docs/roadmap/phase-2-baremetal.md`):
- 2.1 riscv-tests RV64GC 集成 (`git submodule add riscv-tests`)
- 2.2 Spike co-simulation 接口 (`SpikeBridge`)
- 2.3 RISCOF 合规认证框架
- 2.4 最小 HTIF 接口 (tohost/fromhost)
- 2.5 Phase 1.5 SoC JSON 在真实 RV64 binary 上端到端运行

**价值**: 启动 ChipForge 从 "TLM 验证" → "RV64 软件栈验证" 的转折点

---

## 6. 活动日志

| 日期 | 事件 |
|------|------|
| 2026-09-24 | **本次会话 (A+C Hybrid 战略落地)**: 战略选择 A+C Hybrid (Wave 3-4 清债 → Phase 2 产品化前置), 否决 B 工具化 / 否决纯 A (不清债会重复踩坑)。3 个 OpenSpec initiative 创立 (wave3-cpu-pipeline-debt v0.4.0 + wave3-mmu-real-memory-and-cycle v0.5.0 + wave4-csr-cache-dse v0.6.0)。7 个 openspec change proposals 落地 (P0#1 canonical-ordering-assert + P0#2 fix-rv32ui-load-width 回顾性: v0.6.0 commit 8909165 已完成 + P1#3 mmu-paddr-consume-and-real-memory + P1#4 plugin-framework-cycle-precision + P1#5 cpu-pipeline-multi-cycle + P2#6 phase-1.5-wave-4 + P2#7 cache-phase1.5-4way)。`tools/sync_strategy_status.sh` 实装: 从 openspec changes + initiative YAML 派生 strategy §7 表格, `--dry-run` / `--json` 双模式, 替代原 stub。`docs/roadmap/strategy/a-plus-c-hybrid.md` §7 已自动派生 (7 行真实状态)。`docs/roadmap/roadmap-status.md` §1/§5 同步反映 A+C Hybrid 战略入口。3 个版本节点锁定: v0.7.0 (P0 清债, 2026-10) → v0.8.0 (P1 MMU 真端到端, 2026-12) → v0.9.0 (P2 Phase 1.5 毕业, 2027-02)。 |
| 2026-09-20 | **本次会话**: Phase 6c plugin-elaboration-substrate 完整收官。11 commits (`25e2672`/`9a03bb2`/`918e577`/`baa504b`/`3e8ada2`/`a38a1e4`/`19d4f5d`/`edad878`/`b68996a`/`61a642d`/`5fe72fe`) + v0.3.0 + v0.3.1 发布 + 验证报告。`roadmap-status.md` 头部摘要 + §1 状态总览 + §2 Phase 6 详细状态 + 推迟到 Phase 6d 项全部更新。新增 `docs/roadmap/phases/phase-6d-rtl-verification.md` (Phase 6d 独立 phase doc)。3 个 openspec change 创建 (PoC follow-up + Phase 6d prereqs + Phase 6d main, 全部 4/4 artifacts complete + `openspec validate` PASS)。`docs/methodology/plugin-style-design-methodology-v1.md` 添加 Phase 6c elaboration chapter。`docs/lessons/phase-6c-elaboration-substrate.md` 创建 (15 类踩坑)。AGENTS.md 添加 Phase 6c 纪律 + 速查表。<br>**本次会话 (Oracle/Metis 审查修订, 2026-09-20)**: commit 计数统一为 11 (CHANGELOG 权威源); 修 §2.6 失效引用 → §2 Phase 6 节; 修 E12 基线表述 (391 PASS / 12 known FAIL); PoC follow-up-fixes Fix #2 改用方案 B (TEST_CASE_METHOD fixture); phase-6d-rtl-verification 6d.1 估时 1w → 1.5w (decode_chmem.h 从零建, 修正"重启用 disabled"措辞); 6d.6 改 sv32 (对齐现有 MMUPlugin, 估时保留 1w); Prereq #2 按发行版分支 (jammy 22.04 = verilator 4.038 不足 5.020, 需源码 build; noble 24.04 OK); prerequisites 增加测试 ELF 获取项 (为 addi/auipc/jal/beq 编写 `tests/cpu/manual_elf/*.S`)。 |
| 2026-09-20 | **本次会话**: Phase 6c plugin-elaboration-substrate 完整收官。11 commits (`25e2672`/`9a03bb2`/`918e577`/`baa504b`/`3e8ada2`/`a38a1e4`/`19d4f5d`/`edad878`/`b68996a`/`61a642d`/`5fe72fe`) + v0.3.0 + v0.3.1 发布 + 验证报告。`roadmap-status.md` 头部摘要 + §1 状态总览 + §2 Phase 6 详细状态 + 推迟到 Phase 6d 项全部更新。新增 `docs/roadmap/phases/phase-6d-rtl-verification.md` (Phase 6d 独立 phase doc)。3 个 openspec change 创建 (PoC follow-up + Phase 6d prereqs + Phase 6d main, 全部 4/4 artifacts complete + `openspec validate` PASS)。`docs/methodology/plugin-style-design-methodology-v1.md` 添加 Phase 6c elaboration chapter。`docs/lessons/phase-6c-elaboration-substrate.md` 创建 (15 类踩坑)。AGENTS.md 添加 Phase 6c 纪律 + 速查表。<br>**本次会话 (Oracle/Metis 审查修订, 2026-09-20)**: commit 计数统一为 11 (CHANGELOG 权威源); 修 §2.6 失效引用 → §2 Phase 6 节; 修 E12 基线表述 (391 PASS / 12 known FAIL); PoC follow-up-fixes Fix #2 改用方案 B (TEST_CASE_METHOD fixture); phase-6d-rtl-verification 6d.1 估时 1w → 1.5w (decode_chmem.h 从零建, 修正"重启用 disabled"措辞); 6d.6 改 sv32 (对齐现有 MMUPlugin, 估时保留 1w); Prereq #2 按发行版分支 (jammy 22.04 = verilator 4.038 不足 5.020, 需源码 build; noble 24.04 OK); prerequisites 增加测试 ELF 获取项 (为 addi/auipc/jal/beq 编写 `tests/cpu/manual_elf/*.S`)。 |
| 2026-06-13 | **本次会话十**: Phase 1.4 L1CachePlugin 设计方法学复盘 v1 落地, PA-7 + PA-9 + R7 闭环, 18/18 ctest PASS。 |PA-9 决策草案 (`.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md`, DECISION-2026-06-13-02) F1-F5 决议, E1-E5 重解读 (复盘对象 → 6 维度 → 3 边界 → 单例子深复盘)。T1 输入材料消化 (12 文件, 3560 行) + D4+ADR-040 静态检查 3+4/3 PASS 基线。T2/T3 6 维度评估笔记 (`.omo/drafts/phase-1.4-d1-d2-notes.md` 324 行 + `.omo/drafts/phase-1.4-d3-d4-d5-d6-notes.md` 555 行) — 41 评估点, 78% B1 接受 (32) / 15% B2 摩擦 (6) / 7% B3 局限 (3)。T4 整合 v1 文档 (`docs/methodology/plugin-style-design-methodology-v1.md`, 352 行) — 6 维度 × 3 边界 + 6 B2 模式 + 3 B3 局限 + 5 Phase 6 任务链接 (B3-L1/L2/L3 + B3-P1/P2)。T4.3 lessons 文档 supersede 注释。T5 roadmap 同步 (§1/§3/§4/§5 全部更新 + 范围修正 banner)。子代理事故恢复: 19 openspec mirror 目录 + 2 root config 已清理, 2 tracked workflow (.github/workflows/architecture-gates.yml + doc_check.yml) 已 `git restore`。`docs/roadmap/roadmap-status.md` 头部 "最后更新" 改 2026-06-13 本次会话十, §1 状态总览更新, §3 PA-7/PA-9 状态 ⏳ → ✅, §4 R7 ⏳ → ✅ 闭环 (E1 重解读: 不做性能基线, 改做方法学复盘), §5 建议 1 状态 ⏳ → ✅ Completed (本次会话十)。Phase 1 进度 75% → 85% (方法学复盘 +10%)。1 个原子 commit (T6)。Phase 1 退出标准全部达成, Phase 2 启动门槛就绪 (PA-1 ~ PA-9 全部 ✅)。 |
| 2026-06-10 | **本次会话八**: Phase 1.3 全部 6 子任务落地 + 归档指引更新。v2 决策草案 (`8d80fd3` DECISION-2026-06-10-02 v2) 5 项决议 (D1=C POD+窄桥 / D1'=末尾调 pb.run / D1''=不实现+drift 防护 / D2=B 框架层 / D3=A 仅最小 e2e) 全部落地。1.3a L1CacheTLMBridge 框架层 (`26fe7d2`, D1' 末尾挂载契约) + 1.3e BundleMapper drift 防护 (`18418ac`, D1'' `verify_adr.sh` ADR-024 拒绝 `bundles/bundle_mapper.h` 提前实现) + 1.3b `soc/l1_cache_minimal.json` 拓扑 spec (`3dbe058`) + 1.3c `ip/cache/configs/params_schema.json` (`3b6fc27`) + 1.3f `ip/cache/README.md` §9 使用指南 (`e5d865a`) + 1.3d `L1CacheTLMBridgeAdapter` cpptlm ModuleFactory 兼容层 (`c8d1dd1`, 解决 Bridge 构造签名 `(unique_ptr<L1CachePlugin>)` 与 `registerObject<T>(string, EventQueue*)` 不兼容)。14/14 ChipForge ctest PASS in 4.41s; `verify_adr.sh --only=ADR-024` 2/2 PASS (含 drift 防护)。roadmap-status.md §3 PA-6/PA-7/PA-8/PA-9 新增 (Phase 1.3d-extras + Phase 1.4 baseline 启动入口); §4 R6/R7 新增 (ch_stream 协议转换不确定性 + baseline 选型不确定性); §5 Top3 重写 (1.3d-extras → 1.4 → Phase 2); §6 活动日志追加本次会话八条目。6 个原子 commit (Phase 1.3a/1.3b/1.3c/1.3d/1.3e/1.3f)。Phase 1 进度 30% → 65%。**Phase 1.3 全部子任务完成, 新 session 可启动 PA-6/PA-7/PA-8/PA-9 任意顺序**。 |
| 2026-06-10 | **本次会话七**: Phase 1.2 落地 + Phase 1.2 教训文档化 + 文档债务清零。`ip/cache/tlm/L1CachePlugin.{h,cpp}` 实现 lookup + refill 两阶段 (Plugin-style, D4 合规, 256 sets × 64B line);`test_l1_cache_plugin_unit.cpp` 4/4 PASS (miss / refill / hit-after-refill / D4 runtime);D4 静态检查 3/3 PASS;`docs/lessons/phase-1.2-l1cacheplugin.md` 沉淀 7 类 15+ 模式;`docs/roadmap/README.md` Phase 1 status 同步为 In Progress;`roadmap-status.md` Phase 1 进度 10% → ~30%;`ip/README.md` + `ip/cache/README.md` cache 状态从 🔴 规划中 → 🟡 TLM 实现中;10/10 ctest PASS, 0 warnings;3 个原子 commit (Phase 1.2 实施 + Lessons 文档 + 文档同步)。Phase 1 进度 10% → 30%。 |
| 2026-06-10 | **本次会话六**: Phase 1.1 Bundle 定义完成。`bundles/mem_bundles.h` 实现 6 个 Bundle (MemReq/MemResp/CacheReq/CacheResp/L1CachePluginBundle/IntBundle), 全部字段用 `cf::plugin::uint_t<N>` (D4 合规);`test_mem_bundles.cpp` 9/9 PASS (含 5 个 static_assert 编译期检查);`bundles/README.md` 设计原则文档;`tools/run_chipforge_tests.sh` 更新包含新测试 (9/9 PASS);捕获 Phase 0 限制 `uint_t<512>` 退化为 `uint64_t` (uint_t.h:37 兜底), Phase 6 升级方案已记录;2 个原子 commit (Phase 0 收尾 + Phase 1.1 提交)。Phase 1 进度 0% → 10%。 |
| 2026-06-09 | **本次会话五** (Quick 准备): ADR-033 (CtrlLink 4-control-API) 🚧 → ✅ Accepted, D6 共存方案绑定 (halt_when / throw_when / flush_when / bypass);git rm `docs/roadmap/phases/phase-1-foundation.md`;修正 4 处 ip/{cache,memory,interconnect,peripheral}/README.md 链接指向 `phase-1-tlm-foundation.md`;roadmap-status.md 同步 (PA-3 状态推进 + 建议 1 删除 + Phase 1* 行 + 活动日志);1 个原子 commit 提交。Phase 1 (L1CachePlugin) Large plan 启动前的前置解锁。 |
| 2026-06-09 | **本次会话四**: 路线图文档同步——`docs/roadmap/README.md` Phase 0 状态修正为 ✅ Completed(2026-06-08);`roadmap-status.md` §1 状态总览 + §2 Phase 0 详情同步;`docs/architecture/overview.md` 顶部新增"实现状态快照"banner,标明应用层(`bundles/`/`ip/*/`/`soc/riscv_virt.json`)待建设;消除文档-状态背离 |
| 2026-06-08 | **本次会话续三**: CppHDL 集成已恢复(上游 commit 7fe4a5d 修复);test_coexistence 5/5 PASS(CppTLM+CppHDL 共存);PA-5 verify_plugin_decision.sh 3/3 PASS;coverage 测量设施就位;`docs/api/cf_plugin.md` API 文档(Doxygen 替代);Phase 0 退出标准 v2 全部达成(Status: ✅ Completed);7/7 ctest 100% PASS |
| 2026-06-08 | **本次会话续二**: Phase 0 P0 #1-5 全部实施完成 (51/51 单元测试 PASS);PluginBase + Payload<T> + PipeNode + PipeBuilder + CtrlLink;ctest 聚合 5/5 PASS in 0.61s |
| 2026-06-08 | **本次会话终**: 创建 `src/cf_plugin/` 工作区 (CMakeLists.txt + README.md);`cf_plugin` INTERFACE 库已注册;cmake configure 0 错误 (1.3s + 2.9s);Phase 0 实施基础设施就绪 |
| 2026-06-08 | **本次会话续**: 根 `CMakeLists.txt` STUB → 真实集成 (v0.0.1);CppTLM 子模块已集成 (configure 0 错误, cpptlm_core 目标可达, 5+ .o 编译成功);CppHDL 因上游 tests 路径 bug 暂缓 (PA-4b 跟踪);`compile_commands.json` 已生成 |
| 2026-06-08 | **本次会话**: 创建 `roadmap-status.md` 跟踪文件;M4 (ADR-037) 验证已完成;M5 (§12.0.3 责任归属表) 实施完成 (v2.0.3) |
| 2026-06-08 | 决策记录 D1-D11 完成;Phase 0/1/6 文档创建;roadmap/README.md 调整;declarative-hybrid-framework.md 升级 v2.0.2;plugin-framework-revision-plan.md 创建 |
| 2026-06-05 | T1-T7 任务:CI 修复、.gitignore 扩展、IP stub 目录、planning banners |
| 2026-06-03 | 项目骨架初始化(roadmap, architecture, ip 目录) |

---

## 7. 更新指南

- **何时更新**:
  - 每周一上午(例行)
  - 阶段状态变更时(Not Started → In Progress → Completed)
  - 阻塞项出现/解除时
  - 重大决策落地后
- **如何更新**:
  1. 修改"最后更新"日期
  2. 更新 §1 状态总览对应行
  3. 在 §2 详细状态中标记任务进度
  4. 如有新待办,加入 §3
  5. 在 §6 活动日志追加一行
- **不要**:
  - 复制阶段文档的任务清单(本文件只跟踪状态)
  - 在本文件中详细描述任务(那是阶段文档的职责)
