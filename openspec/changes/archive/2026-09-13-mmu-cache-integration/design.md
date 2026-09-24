## Context

`mmu-tlb-ptw-impl`（archived 2026-09-12）落地了 MMU 侧 VIPT 数据流输出（`pl::PADDR` + `pl::MMU_VADDR` 双 Key）并解锁了 29/29 mmu 测试 PASS。本 change 是其强制下游消费者，落地 ADR-044 §3.2 数据流方案 A 的**消费端**（L1Cache 消费 `MMU_VADDR`），并补全 SoC 全链集成测试。

### 当前状态（mmu-tlb-ptw-impl 后）

- ✅ `MMUPlugin::at_stage("tlb_lookup_*")` 命中时写 `pl::PADDR` + `pl::MMU_VADDR`（`MMUPlugin.cpp:54-55`）
- ❌ `MMUPlugin::at_stage` PTW completion WalkCallback 漏写 `pl::MMU_VADDR`（**Oracle Q1 真实 bug**，commit 0/9 已紧急修复 `78275db`）
- ❌ `L1CachePlugin::at_stage("lookup")` 仅读 `g_addr`（PIPT 路径），不消费 `pl::MMU_VADDR`
- ❌ `soc/mmu_minimal.json` 不存在（mmu-tlb-ptw-impl 已 defer）
- ❌ `src/cf_plugin/bridge/mmu_bridge_adapter.cpp` 不存在（mmu-tlb-ptw-impl 已 defer）
- ✅ `bundles/tlb_bundles_tlm.hh`（TlbReqBundle/TlbRespBundle）已就位（mmu-tlb-ptw-impl commit 9a）
- ✅ `src/cf_plugin/bridge/mmu_bridge.{h,cpp}` 核心已就位（mmu-tlb-ptw-impl commit 9b）

### 利益相关方

- **L1Cache IP**（`ip/cache/tlm/`）：消费 MMU 输出
- **MMU IP**（`ip/mmu/tlm/`）：bug fix（commit 0）+ PTW completion dual-write 契约（commit 0）
- **SoC 集成**（`soc/` + `tests/soc/`）：最小 JSON + 实例化测试
- **cpptlm Bridge Adapter**（`src/cf_plugin/bridge/`）：从骨架 → 完整实装
- **ADR-044**（`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`）：VIPT 数据流契约权威

## Goals / Non-Goals

### Goals

1. **完成 ADR-044 §3.2 数据流方案 A 的两端契约**：MMU 输出端（mmu-tlb-ptw-impl）+ Cache 消费端（本 change）
2. **修复 Oracle Q1 发现的 PTW completion stale vaddr bug**：commit 0 已紧急 hotfix
3. **L1CachePlugin 消费 `pl::MMU_VADDR` 进行 VIPT 索引**：保留 PIPT fallback（21 baseline [cache] 测试零回归）
4. **cpptlm `MMUTLMBridgeAdapter` 实装**：独立 adapter，不做统一 `MMUCacheBridgeAdapter`
5. **`soc/mmu_minimal.json` 全链集成测试**：`tg → mmu_bridge → l1_cache_bridge → memory`
6. **MMU 边界测试扩展** 29→39（~10 cases）：SPLIT from original 65-case target

### Non-Goals

1. **DSE 48-case sweep** — 推迟到 `cache-dse-sweep` change（编译期 `static_assert` 已覆盖大部分参数合法性）
2. **CtrlLink stall 机制** — 推迟到 `plugin-framework-stall` change（`PipeBuilder` run 循环未消费 `should_halt()`，需框架级 change）
3. **L1Cache 4-way VIPT 升级（Phase 1.5）** — 推迟到 `cache-phase1.5-4way` change
4. **Sv32/Sv48 PTW 解码** — 推迟到 `mmu-sv32-sv48-ext` change
5. **mmu 测试 29 → 65 完整扩展** — 拆为 29→39（边界 ~10 cases）+ cache-dse-sweep（48 DSE cases）
6. **`L1CachePlugin.h` 修改** — helper 签名不变（`extract_idx/extract_tag` 接受 64-bit，vaddr/paddr 都适用）
7. **L1CacheTLMBridge / cf_plugin 框架层修改** — `CacheReq.address` 语义不变（MMU 在前置阶段翻译）
8. **`ip/mmu/rtl/`** — Phase 5+ 沿用

## Decisions

### Decision 1: Q1 PTW completion dual-write bug fix 算 mmu-cache-integration commit 0

**选择**: `78275db` commit 0 已在 main 分支落地，作为 mmu-cache-integration 的紧急前置 hotfix。

**替代方案**:
- A. 拆为独立 `hotfix-mmu-ptw-dual-write` change（用户决策明确不采纳）
- B. 等 L1Cache 真的消费时再修（风险：间歇性数据损坏难复现）

**理由**: 用户决策（mmu-cache-integration explore 阶段确认）；bug 与集成紧耦合，同一 change review 更自然；commit message 明确标注 "Part of mmu-cache-integration (planned)"。

### Decision 2: L1Cache 直接 include mmu_keys.h 读 `pl::MMU_VADDR`（弱耦合）

**选择**: `L1CachePlugin.cpp` 加 `#include "ip/mmu/tlm/mmu_keys.h"`，直接用 `mmu_keys<T>::MMU_VADDR` Payload Key 读 MMU 输出。

**替代方案**:
- A. L1Cache 自定义 `cache_keys::CACHE_VADDR` Key + Bridge Adapter 做 key-to-key 转换（更强解耦）
- B. L1Cache 自定义 `cache_keys::CACHE_VADDR` Key + `cache_keys::CACHE_VADDR` 同名 Payload 但不同对象（Key identity 不匹配，bug）

**理由**: ADR-044 §3.2 推荐方案 A：MMUPlugin 写 `pl::MMU_VADDR`，L1Cache 直接读同一 Key（Key identity 匹配——全局静态对象指针身份）。独立 Key 需要 Bridge Adapter 做 key-to-key 转换，增加 adapter 复杂度。`include "ip/mmu/tlm/mmu_keys.h"` 是 IP 间 tlm/ 层弱耦合（IP 不依赖对方 lib/ 或 runtime 实现，仅依赖 tlm/ 接口定义）。

### Decision 3: PIPT fallback 静默回退（不回退就报错）

**选择**: `L1CachePlugin::lookup` 用 `n->has(MMU_VADDR)` 三元选择 idx_src：无 MMU 输出时用 `g_addr` 做 idx 和 tag（PIPT 路径，行为与改造前完全一致）。

**替代方案**:
- A. 硬报错：MMU 启用但 lookup 节点没 MMU_VADDR → 异常
- B. 配置开关：`L1CachePlugin::vipt_required_ = false`（默认 false）

**理由**: 21 个 baseline [cache] 测试（`tests/cache/` 5 文件）全部在无 MMU 的 PipeBuilder 里跑，硬报错会全部回归。ADR-044 §3.3 明确 "Bridge 不需要修改（PIPT 兼容 VIPT）"，回退正是该契约的 Plugin 内镜像。D4 禁止早返，所以用 `has()` 三元选择而非 `if (missing) throw`。

### Decision 4: 独立 `MMUTLMBridgeAdapter`（不做统一 `MMUCacheBridgeAdapter`）

**选择**: `MMUTLMBridgeAdapter` 单独继承 `cpptlm::ChStreamModuleBase`，持一个 `MMUTLMBridge` 成员。

**替代方案**:
- A. 统一 `MMUCacheBridgeAdapter`（同时持 `L1CacheTLMBridge` + `MMUTLMBridge`）
- B. 复用 `L1CacheTLMBridgeAdapter`（改名 `L1CacheMMUBridgeAdapter`）

**理由**: `l1_cache_bridge_adapter.h:71` 确立的模式是 "一个 Bridge 一个 Adapter"（构造签名 `(string, EventQueue*)` 受 ModuleFactory `registerObject` 约束）。统一适配器会让 cache 侧测试必须实例化 MMU，破坏 `tests/cache` 隔离性，且违反 `l1_cache_bridge_adapter.h:66-69` 显式收窄范围先例。链式组合应发生在 SoC JSON connections 层，而非 C++ 类层。

### Decision 5: SoC 最小 JSON 全链 `tg → mmu → l1 → mem`（不省 l1）

**选择**: `soc/mmu_minimal.json` 包含 4 modules + 3 connections。

**替代方案**:
- A. 极简：`traffic_gen → mmu_bridge → memory`（3 modules + 2 connections）
- B. SoC 完整拓扑（5+ modules + 5+ connections，含 RISC-V CPU）

**理由**: 本 change 的存在意义是验证 MMU→Cache 的 VIPT 数据流（ADR-044 §3.2）。缺了 L1 cache 节点的 "minimal" JSON 测的是 MMU 单点——那是 `mmu-tlb-ptw-impl` 已覆盖的范围（29/29 PASS），放进本 change 是纯重复。完整 CPU 拓扑推迟到 `cpu-mmu-integration` change。`soc/cpu/configs/` 目录不存在——现有惯例是 `soc/*.json` 平铺。

### Decision 6: MMU 测试 29 → 39 SPLIT（不打包 65）

**选择**: 本 change 加 ~10 个边界测试（与集成直接相关：PTW completion dual-write 回归 + SFENCE.VMA + ASID switch + 2MB megapage + Sv39 reserved encoding）。

**替代方案**:
- A. 65 打包：边界 + DSE 扫描全在本 change
- B. 29→39 本 change + 48 DSE 独立 `cache-dse-sweep` change（Oracle Q6 推荐）

**理由**: 65 例里 DSE 配置扫描与 MMU 正确性测试是两种价值的测试——前者验证参数合法性，后者抓回归。混在一起失败定位模糊。ADR-044 §6 检查清单 1-2 项要求的 `static_assert` 已是编译期配置护栏，运行时重复扫 48 个配置边际价值低。当前 associativity 固定 1-way（`L1CachePlugin.h:72`），扫 16 cache-sizes × 3 policies 边际收益低。

### Decision 7: PTW_ACTIVE + RETRY 数据依赖替代 CtrlLink stall

**选择**: 本 change 不实装 CtrlLink stall；`MMUTLMBridgeAdapter::tick()` 用 `PTW_ACTIVE` Payload 做声明式数据依赖——PTW busy 时输出 `hit=false + error=RETRY`，由上游（traffic_gen / CPU 侧）重发。

**替代方案**:
- A. 加 `PipeBuilder::run()` 消费 `CtrlLink::should_halt()` 轮询（框架级 change）
- B. 用 `CtrlLink::halt_when(std::function<bool()>)` 注册 PTW_ACTIVE 谓词（无 run 循环消费是死代码）
- C. RETRY 语义（当前选择）

**理由**: `CtrlLink::should_halt()` API 已存在（`include/cf/plugin/ctrl_link.h:58-61`），但 `PipeBuilder::run()` 循环未消费——grep 不到任何 halt/stall 引用。新增 `should_halt` 轮询改的是框架契约（影响全部 Plugin），远超本 change 范围。`halt_when` 需要状态机（D4 禁止）或 run 循环消费（缺失）。RETRY 语义是最小可用方案：cache lookup 闭包读 `PTW_ACTIVE`，为 1 时输出 `error=true`，由上游重发。PTW ~30 cycle latency 跨周期表达，RETRY 语义自然适配。

**已知限制**: RETRY 要求上游（traffic_gen / CPU 侧）支持重发——若上游不支持，本 change 内将 PTW miss 场景标记为 known-limitation，仅测 TLB hit 路径。

### Decision 8: 修改 `mmu-vipt-data-flow` 和 `mmu-cpptlm-bridge` specs（MODIFIED delta）

**选择**: 用 OpenSpec `## MODIFIED Requirements` 模式更新 archived specs 中的"L1Cache SHALL NOT be modified"和"MMUTLMBridgeAdapter MUST register" requirements。

**替代方案**:
- A. 用 `## REMOVED Requirements` 删旧 + `## ADDED Requirements` 加新（破坏 archived spec 引用链）
- B. 新建独立 capability specs 而不修改 archived（spec 重复）

**理由**: OpenSpec delta 操作明确支持 `MODIFIED Requirements`——保留原 requirement header 但更新 scenarios 反映新行为。这保持 archived spec 与 active change 的契约连续性，避免 spec 重复。

## Risks / Trade-offs

### Risk 1: L1Cache 改造影响范围

[L1Cache 改造影响 lookup 闭包] → Mitigation: 用 `has()` 三元选择，21 baseline [cache] 测试零回归验证（必须 288/288 → 291/291 PASS，由 commit 0/78275db 验证 + commit 2 重跑）。

### Risk 2: mmu_keys.h::MMU_VADDR/PADDR Key identity 跨翻译单元

[L1Cache 和 MMUPlugin 必须共享同一 Key 对象地址] → Mitigation: `Payload<T>` 是全局静态对象（`cf::plugin::Payload<...> MMU_VADDR{"mmu.mmu_vaddr"}`），Key identity 是指针身份。L1Cache 通过 `#include "ip/mmu/tlm/mmu_keys.h"` 引用同一全局对象。

### Risk 3: PTW_ACTIVE RETRY 语义不被上游支持

[traffic_gen 可能不支持重发] → Mitigation: 本 change 在 `soc/mmu_minimal.json` 用 traffic_gen 单事务场景，仅测 TLB hit 路径。PTW miss 路径标记为 known-limitation，推迟到后续 traffic_gen 升级 change。

### Risk 4: mmu_bridge_adapter.cpp cpptlm API 兼容性

[cpptlm::ChStreamModuleBase / ChStreamAdapterFactory 可能在不同 cpptlm 版本有 API 变化] → Mitigation: mirror `l1_cache_bridge_adapter.cpp` 逐行翻译（同 cpptlm 版本），保持结构一致；CI gate `check_plugin_portability.sh` 在编译期捕获 API drift。

### Risk 5: 4 architecture gates 中的 doc_link_check 可能拦截新文件

[新文件 `cache_keys.h`、`mmu_bridge_adapter.{h,cpp}`、`mmu_minimal.json`、`test_mmu_cache_integration.cpp`、`test_mmu_minimal_json.cpp` 需要正确的文档交叉引用] → Mitigation: commit 7 docs sync 阶段统一更新 `docs/architecture/overview.md` + `ip/mmu/docs/integration.md` + `ip/cache/README.md`，新增文件链接到现有 docs。

### Risk 6: 9 commits 顺序风险

[commit 1-8 任何一步编译失败 → 后续 commit 不可测试] → Mitigation: 每个 commit 后 `cmake --build build` + `ctest` 验证；commit 0 已落地（`78275db`），commit 1 cache_keys.h 独立可验证，commit 2 L1Cache 改造后跑完整 [cache] baseline。

### Trade-off: PIPT fallback vs 硬报错

选择 PIPT fallback（Decision 3）保留 21 baseline 测试兼容性，但增加了"MMU 启用但未正确接线"的隐蔽故障模式。缓解：commit 7 docs sync 阶段明确文档化 fallback 语义；D4 Plugin build() 时打一次性 INFO 日志声明当前模式（可选增强）。

### Trade-off: 独立 adapter vs 统一 adapter

选择独立 `MMUTLMBridgeAdapter`（Decision 4）保留测试隔离性，但 SoC JSON 必须显式连接两个 adapters。缓解：JSON 文件结构清晰（4 modules + 3 connections）；traffic_gen → mmu_bridge → l1_cache_bridge → memory 拓扑与设计意图一致。

## Migration Plan

### Phase 1: 紧急 Hotfix（commit 0/9，已完成）

- ✅ `78275db` 修补 `MMUPlugin.cpp:60-65` PTW completion dual-write bug
- ✅ 加 `tests/mmu/test_ptw_unit.cpp` (3 cases) 回归网
- ✅ baseline 291/291 PASS，4 architecture gates 全绿

### Phase 2: Cache 改造 + 集成测试（commits 1-3/9）

- commit 1: 新增 `ip/cache/tlm/cache_keys.h`（集中 L1Cache Payload Key，含新增 `g_vaddr`）
- commit 2: `L1CachePlugin.cpp` include mmu_keys.h + lookup 闭包 has() 三元
- commit 3: `tests/cache/test_mmu_cache_integration.cpp` (3 cases)

**回滚策略**: commit 2 的 lookup 闭包改动可单独 revert；commit 1 是新增文件可删除；commit 3 是新增测试可禁用。

### Phase 3: Bridge Adapter + SoC 拓扑（commits 4-5/9）

- commit 4: `src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}` + `tests/CMakeLists.txt` 加入 mmu_bridge_adapter.cpp 到 CACHE_IMPL_SOURCES
- commit 5: `soc/mmu_minimal.json` + `tests/soc/test_mmu_minimal_json.cpp` (4 cases)

**回滚策略**: commit 4 adapter 是新增 .cpp，可禁用；commit 5 JSON 是新增文件，删除即可。

### Phase 4: MMU 边界测试扩展 + 文档同步（commits 6-7/9）

- commit 6: mmu 测试 29→39（~10 cases：PTW completion dual-write 回归 + SFENCE.VMA + ASID switch + 2MB megapage + Sv39 reserved encoding）
- commit 7: `ip/mmu/STATUS.md` INTEGRATED + `CHANGELOG.md` v0.1.0 + `ip/mmu/docs/integration.md` + `docs/architecture/overview.md` 快照

**回滚策略**: commit 6 测试改动可单独 disable；commit 7 docs 改动可 revert 文档。

### Phase 5: 最终验证（commit 8/9）

- commit 8: `tools/verify_adr.sh` + `tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` + `tools/doc_link_check.sh` 全 PASS
- baseline 322/322 PASS（291 + 3 cache integration + 4 soc MMU minimal + ~10 mmu boundary + 其他微调）

### Archive 阶段（commit 9/9）

- `openspec archive mmu-cache-integration` 移动 change 到 `openspec/changes/archive/2026-09-13-mmu-cache-integration/`
- 6 个新 capability specs + 2 个 modified specs 永久化到 `openspec/specs/`

## Open Questions

### Q1: traffic_gen 是否支持 PTW RETRY 重发？

**已知**: `MMUTLMBridgeAdapter::tick()` 用 PTW_ACTIVE RETRY 语义，但 traffic_gen（cpptlm 模块）是否支持重发需进一步验证。**缓解**: 本 change 标记为 known-limitation；commit 5 的 `test_mmu_minimal_json.cpp` 仅做结构验证（不跑仿真）。

### Q2: L1Cache `extract_idx/extract_tag` 是否需要分 vaddr/paddr 双 helper？

**决策**: 不需要——helper 签名 `uint_t<kAddrBitsRaw>` (64-bit) 接受 vaddr 和 paddr（同 64-bit）。ADR-044 §3.2 提到"Phase 1.5 引入 MMU 后需分两个 helper"是过时的注释——内部实现不变（shift+mask），调用点已区分。

### Q3: `cache_keys.h` 是否要 reuse 现有 file-scope `g_addr`/`g_idx`/`g_tag`？

**决策**: 不——保留 file-scope Keys 避免破坏 21 baseline 测试。新增 `cache_keys.h` 只放**新增**的 `g_vaddr` Key + 配置 Knob 类型（`vipt_fallback` enum）。这是向后兼容的渐进改造。

### Q4: `mmu_bridge_adapter.cpp` 是否需要补 cpptlm::run() 循环的 retry handling？

**决策**: 不——adapter 只是 stub 4 字段窄桥。retry 处理在 traffic_gen / SoC 集成层，本 change 范围之外。

### Q5: 后续 `cache-dse-sweep` change 是否会与本 change 的 4-case Pareto 子集冲突？

**决策**: 不会——本 change 只测 4 个最小集成 case（VIPT 命中 + PIPT fallback + VIPT miss + 非法几何拒绝），`cache-dse-sweep` 是独立 change 测完整 12-case Pareto（4 几何 × 3 策略）。两者互补。

## References

- `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` — VIPT 数据流契约权威
- `openspec/changes/archive/2026-09-12-mmu-tlb-ptw-impl/` — 上游 change artifacts
- `openspec/changes/archive/2026-09-12-mmu-tlb-ptw-impl/proposal.md` Decision 8 — 明确 mmu-cache-integration scope
- `src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp` — adapter 模板（mirror）
- `soc/l1_cache_minimal.json` — JSON 拓扑模板（mirror）
- `docs/architecture/adr.md` — ADR 注册表（ADR-040 / ADR-044 已注册）
- `tools/verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` — 4 architecture gates（CI PR-blocking）
