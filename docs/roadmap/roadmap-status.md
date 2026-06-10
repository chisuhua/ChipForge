# 路线图执行状态跟踪

> **最后更新**: 2026-06-10 (本次会话八: Phase 1.3 v2 决策草案批准 + 实施启动)
> **更新时机**: 每周一 / 阶段切换时 / 重大决策落地后
> **权威源**: `docs/roadmap/phases/*.md` + `.omo/plans/*.md` + `.omo/drafts/*.md`
> **本文件目的**: 不重复阶段文档的任务清单,只跟踪执行状态、阻塞和下一步

---

## 1. 状态总览

| 阶段 | 里程碑 | 状态 | 进度 | 阻塞项 | 下一交付物 |
|------|--------|------|------|--------|----------|
| Phase 0 | M0 - 脚手架可运行 | ✅ Completed | 100% (5/5 P0) | 无 | Phase 1 启动 |
| Phase 1 | M1 - L1CachePlugin Hello World | In Progress | ~30% (1.1+1.2 完成) | 依赖 Phase 0(已解除) | 1.3 最小 SoC JSON |
| Phase 1* | M1 (legacy, 已取代) | Superseded | - | 无 | 已删除 (2026-06-09) |
| Phase 2 | M2 - ISA 全覆盖 | Not Started | 0% | 依赖 Phase 1 | riscv-tests 集成 |
| Phase 3 | M3/M4 - FreeRTOS/Zephyr | Not Started | 0% | 依赖 Phase 2 | ClintTlm/PlicTlm 完善 |
| Phase 4 | M5 - Linux 启动 | Not Started | 0% | 依赖 Phase 3 | Sv39 + VirtIO Block |
| Phase 5 | M6/M7/M8/M10 - RTL | Not Started | 0% | 依赖 Phase 4 | L1CacheRtl (CppHDL) |
| Phase 6 | M6 - 完整 PipeBuilder | Not Started | 0% | 依赖 2-3 Plugin 稳定 | 调度算法 |

> *Phase 1* = 已被 `phase-1-tlm-foundation.md` 取代的旧 `phase-1-foundation.md`,待决定保留/废弃/删除

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

**进度**: 5/5 P0 组件完成;51/51 单元测试通过(100% ctest pass rate)
**剩余**: 退出标准 §2.2(质量标准)与 §2.3(集成标准)待验证 — 调度确定性证明、零 TODO、覆盖率≥80%、Doxygen、与 ChStreamModuleBase/Component 共存

### Phase 1 - TLM Foundation (L1CachePlugin)

- **状态**: In Progress (~65%, 1.1 + 1.2 + 1.3a + 1.3b + 1.3c + 1.3d + 1.3e + 1.3f 完成; Phase 1.3 全部子任务落地)
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
- **Phase 1.3 v2 决策** (`8d80fd3` DECISION-2026-06-10-02 v2):
  - D1=C: Phase 1.3 保持 `cf::bundles::*` POD 不动, Bridge 做 4 字段窄桥 (addr/data/is_write/id)
  - D1'=末尾: Bridge `tick()` 末尾调用 `plugin_->pb.run()` (回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1)
  - D1''=不实现: BundleMapper 推迟 Phase 5/6, 加 `verify_adr.sh` drift 防护
  - D2=B: Bridge 在 `src/cf_plugin/bridge/`, 不在 `ip/`
  - D3=A: 仅 1.3 最小 e2e; 1.4 baseline 留到下次 session
- **下一步**: Phase 1.3 全部子任务完成 (1.3a/1.3b/1.3c/1.3d/1.3e/1.3f). Phase 1.3d-extras (ch_stream adapter + full JSON instantiateAll) 推迟到 Phase 2+; Phase 1.4 (cpptlm::CacheTLM baseline 对比) 启动

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

### Phase 6 - 完整 PipeBuilder 框架

- **状态**: Not Started (0%)
- **依赖**: 2-3 个 Plugin-style IP 稳定运行
- **关键交付**: 调度算法 + JSON 解析 + CompareDriver + RTL 生成

---

## 3. 当前未决项(Pending Actions)

| ID | 类型 | 项目 | 责任 | 状态 | 优先级 |
|----|------|------|------|------|-------|
| PA-1 | 文档 | M4: `adr.md` 新增 ADR-037 + 更新 ADR-025~036 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, v1.1) | P1 |
| PA-2 | 文档 | M5: `declarative-hybrid-framework.md` §12.0.3 责任归属表 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, v2.0.3) | P1 |
| PA-3 | 文档 | 处置 `phase-1-foundation.md`(旧 vs 新 phase-1-tlm-foundation.md) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-09, 文件已 git rm) | P2 |
| PA-4a | 构建 | CppTLM 集成 + 根 CMakeLists.txt 升级 | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08) | P1 |
| PA-4b | 构建 | CppHDL 集成(阻塞: CppHDL 上游 tests 路径 bug) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, 上游修复后集成) | P2 |
| PA-5 | 验证 | V1: `tools/verify_plugin_decision.sh`(可选) | Prometheus+Sisyphus | ✅ 已完成 (2026-06-08, 3/3 PASS) | P3 |

---

## 4. 风险与阻塞

| ID | 描述 | 影响阶段 | 缓解措施 | 状态 |
|----|------|---------|---------|------|
| R1 | D4 决策(Plugin-style 强制)被 C++17 静态类型系统拒绝 | Phase 0/1 | Phase 0 退出标准强制"端到端跑通最小 Plugin" | 监控中 |
| R2 | 脚手架工时低估(可能 4 周 vs 计划 2-3 周) | Phase 0 | 每周评估;必要时拆为 Phase 0a/0b | 待启动 |
| R3 | CppHDL 集成阻塞(tests/CMakeLists.txt:38 路径 bug) | Phase 5+ | Phase 0-2 暂不需 CppHDL;Phase 5+ 再评估或用独立构建 | 已识别 + 缓解 |
| R4 | 命名冲突(halt_when vs stream_halt_when 等 4 项) | Phase 0 | 决策文档 §3.5 已识别 D6-D9 方案 | 已识别 |
| R5 | 文档决策被用户拒绝 | 全部 | 修订计划提供渐进回滚方案 | 待确认 |

---

## 5. 下一步建议(Top 3)

### 建议 1:启动 Phase 1 — L1CachePlugin (1.5 周) — 🔄 剩余

**前置条件已就绪** (2026-06-09 Quick 准备已解锁):
- ✅ ADR-033 (CtrlLink 4-control-API) 已 ✅ Accepted,D6 共存方案绑定 (halt_when / throw_when / flush_when / bypass)
- ✅ Phase 0 脚手架 5/5 P0 组件完成 (51/51 单元测试 PASS, 7/7 ctest PASS)
- ✅ CppTLM + CppHDL 集成完成 (`cpptlm_core` + `cpphdl` 目标可达)
- ✅ `bundles/mem_bundles.h` 计划在 Phase 1.1 落地 (1 天)
- ⚠️ 风险: 应用层代码空 (`bundles/`、`ip/*/tlm/`、`soc/riscv_virt.json` 6/8 引用悬挂) — Phase 1 启动时一并建设

**任务** (详见 `phase-1-tlm-foundation.md`):
- 1.1 Bundle 定义 (`MemReq` / `MemResp` 用 `uint_t<N>`,D4 合规) — 1 天
- 1.2 L1CachePlugin 实现 (lookup + refill 两阶段,无 `tick()`) — 3-4 天
- 1.3 最小 SoC JSON (`l1_cache_minimal.json` 用 CPUTLM/CacheTLM/MemoryTLM/CrossbarTLM) — 1 天
- 1.4 对比基线 (与 `cpptlm::CacheTLM` 执行迹对比) — 1-2 天

**价值**: 验证 D4 Plugin-style 端到端可行性,是 Phase 0 投入变现的关键节点

### 建议 2:解决 ADR-033 衍生命名冲突 (CtrlLink::halt_when vs CppHDL chlib stream_halt_when) — 🟡 待确认

**背景**: 2026-06-09 ADR-033 锁定 D6 共存方案 (4 conflicts),但 chlib 自由函数与 `cf::plugin::CtrlLink` 对象方法在 Phase 1 编码期可能产生实际调用点冲突

**选项**:
- A. 维持共存 (D6 原方案) — chlib 自由函数保持向后兼容,新 Plugin 业务代码统一 `CtrlLink::*`
- B. chlib 自由函数加 namespace prefix (`chlib::stream_halt_when`) — 显式区分
- C. CtrlLink 改名 (`CtrlLinkHalt` 等) — 规避冲突但破坏已落地 API

**价值**: 在 Phase 1 编码前关闭最后的不确定性,避免 `CtrlLink::halt_when` 调用点大规模重命名

**建议**: 维持 D6 (选项 A),如果 Phase 1.2 实施时实测发现调用点混淆再升级到选项 B

---

## 6. 活动日志

| 日期 | 事件 |
|------|------|
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
