## Why

`mmu-tlb-ptw-impl`（archived 2026-09-12）落地了 MMU 侧双 Key 输出（`pl::PADDR` + `pl::MMU_VADDR`），解锁 ADR-044 VIPT 数据流契约的**MMU 端**。但**消费端**未实装——`L1CachePlugin::lookup` 闭包当前只读 `g_addr`（`ip/cache/tlm/L1CachePlugin.cpp:148`），不做 VIPT 索引；`soc/mmu_minimal.json` 不存在；`src/cf_plugin/bridge/mmu_bridge_adapter.cpp` 不存在（mmu-tlb-ptw-impl 已 defer）。

更重要的是，Oracle Q1 review 在 `MMUPlugin.cpp:60-65` 发现**真实 bug**：PTW completion WalkCallback/FaultCallback 闭包**没捕获 vaddr** 也**没写 `pl::MMU_VADDR`**，导致 TLB cold miss → PTW walk → walk 完成后 `pl::MMU_VADDR` 保持上一笔事务的旧值（PayloadStore 跨 `pb.run()` 持久，旧值不会被自动清除）。一旦本 change 让 L1Cache 真的消费 `MMU_VADDR` 做 VIPT 索引，**TLB miss→refill 后的第一笔 cache lookup 会用错 vaddr → 间歇性 alias 数据损坏**。该 bug 已 commit 0（`78275db`）紧急修复。

本 change 是 `mmu-tlb-ptw-impl` 的强制下游消费者，落地 **L1Cache 消费 `MMU_VADDR` VIPT 索引 + PIPT fallback 兼容基线 + SoC 全链集成 + cpptlm Adapter + MMU 边界测试扩展**，**完成 ADR-044 §3.2 数据流方案 A 的两端契约**（MMU 输出端 mmu-tlb-ptw-impl + Cache 消费端 mmu-cache-integration）。

## What Changes

### Bug 修复（commit 0/9，**已落地** `78275db`）

- **`ip/mmu/tlm/MMUPlugin.cpp`**（修改）：PTW completion `WalkCallback`/`FaultCallback` 闭包加 `[node, vaddr]` capture，在 success 和 fault 两条路径都写 `pl::MMU_VADDR = vaddr`（防止 stale vaddr）
- **`tests/mmu/test_ptw_unit.cpp`**（新增，3 cases）：间接验证闭包 capture 模式 — `PTWWalkCallbackClosureCapturesVaddr` + `PTWInvalidPTETriggersFaultCallbackWithCode12` + `PTWReservedEncodingTriggersFaultCallbackWithCode15`

### Cache 改造（commit 2/9）

- **`ip/cache/tlm/L1CachePlugin.cpp`**（修改）：`#include "ip/mmu/tlm/mmu_keys.h"`；`at_stage("lookup")` 闭包改用 `n->has(MMU_VADDR)` 三元选择 idx_src/tag_src — 有 MMU 时 `idx = extract_idx(MMU_VADDR)` + `tag = extract_tag(PADDR)`（VIPT 路径），无 MMU 时回退 `g_addr`（PIPT 路径，行为与改造前完全一致，21 个 [cache] baseline 测试零回归）
- **D4 合规**：全分支 `if/else`（不早返，HDL 友好）
- **不修改** `L1CachePlugin.h`（helper 签名不变）、`extract_idx/extract_tag` 实现（`uint_t<64>` 接受 vaddr 和 paddr）、`issue_request` API（向后兼容）

### 新增 Cache Keys 配置（commit 1/9）

- **`ip/cache/tlm/cache_keys.h`**（新增）：集中 L1Cache 用 Payload Key，**新增** `g_vaddr` Key（mirror `mmu_keys::MMU_VADDR`，同一 Payload 类型 `Payload<uint64_t>`，但**独立对象**——IP 间不通过 Key 共享数据，通过 Key identity 匹配）；新增 `vipt_fallback` 配置 Knob（enum "auto"/"paddr_only"/"panic"，默认 "auto"）
- **不移动现有 file-scope `g_addr`/`g_idx`/`g_tag`/`g_hit`/`g_data`/`g_error`/`g_mem_data`/`g_mem_id`**（向后兼容，21 baseline 测试零回归）

### 集成测试（commit 3/9）

- **`tests/cache/test_mmu_cache_integration.cpp`**（新增，~150 LOC，3 cases）：
  1. `MMUPluginOutputDrivesVIPTIndex`：mock `MMU_VADDR` + `PADDR` Key → 验证 idx 来自 vaddr, tag 来自 paddr
  2. `NoMMUOutputFallsBackToPIPT`：不写 MMU_VADDR → 走 issue_request → g_addr 路径（等价现有 baseline）
  3. `VIPTMissWhenPADDRAbsentButVAddrMatches`：验证 has() fallback 对称性

### Bridge Adapter（commit 4/9）

- **`src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`**（新增，~150 LOC）：`MMUTLMBridgeAdapter` 继承 `cpptlm::ChStreamModuleBase`，同构 `L1CacheTLMBridgeAdapter`（`src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp`）— cpptlm `ChStreamAdapterFactory` 注册 + `ch_stream` 4 字段窄桥（`TlbReqBundle` / `TlbRespBundle` 已在 commit 9a/12 定义）
- **不实现统一 `MMUCacheBridgeAdapter`**（隔离性优先 — Oracle Q3 推荐）

### SoC 拓扑（commit 5/9）

- **`soc/mmu_minimal.json`**（新增，~50 行）：**全链** `tg → mmu_bridge → l1_cache_bridge → memory`，镜像 `soc/l1_cache_minimal.json` 结构 + MMUPlugin levels/sv39 配置
- **路径放 `soc/`**（不是 `soc/cpu/configs/`——该目录不存在；Oracle Q4 验证）
- **`tests/soc/test_mmu_minimal_json.cpp`**（新增，4 cases）：结构验证（模块数/连接数/类型字段/required params），mirror `tests/soc/test_soc_l1_cache_minimal_json.cpp`

### MMU 边界测试扩展（commit 6/9）

- **mmu 测试 29 → 39**（**SPLIT from original 29→65**）：本 change 只加**与集成直接相关**的边界测试 ~10 cases：
  1. PTW miss→complete 后 `MMU_VADDR` 与输入 vaddr 一致性（commit 0 修补的回归网，4 cases: success + fault + reserved + invalid vaddr）
  2. megapage (2MB) 下 VIPT idx 安全断言（1 case）
  3. SFENCE.VMA 后 TLB invalidate + cache tag 自然 miss（1 case）
  4. Sv39 reserved encoding 完整覆盖（2 cases: R=1,W=1,X=1 + V=0）
  5. ASID 切换 cross-level invalidate（2 cases: invalidate_vaddr + invalidate_asid）
- **48-case DSE sweep 推迟**到独立 `cache-dse-sweep` change（Oracle Q6 DEFER）

### 文档同步（commit 7/9）

- **`ip/mmu/STATUS.md`**：`IMPLEMENTED` → `INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链)`
- **`CHANGELOG.md`**：v0.1.0 (2026-09-XX) — "L1Cache 消费 MMU_VADDR VIPT 索引 + PIPT fallback + mmu_bridge_adapter + soc/mmu_minimal.json 全链 + 29→39 mmu 边界测试扩展"
- **`ip/mmu/docs/integration.md`**：新增 "L1Cache 集成契约" 段（ADR-044 §3.2 双向验证）
- **`docs/architecture/overview.md`**：快照日期 → 2026-09-XX；mmu-cache-integration 完成标记；下一里程碑 → `cache-dse-sweep` + `cache-phase1.5-4way`

### 最终验证（commit 8/9）

- **`tests/CMakeLists.txt`**：确认 MMU_IMPL_SOURCES + mmu_bridge_adapter.cpp 已链接
- **`tools/verify_adr.sh` / `tools/verify_plugin_decision.sh` / `tools/check_plugin_portability.sh` / `tools/doc_link_check.sh`**：4 architecture gates 全 PASS
- **`./build/bin/chipforge_tests`**：**322/322 PASS**（291 baseline + 3 cache integration + ~10 mmu boundary + 其他微调；**不**含 48-case DSE）

### 不修改

- `L1CacheTLMBridge` 本身（`CacheReq.address` 语义不变——MMUPlugin 在前置阶段翻译）
- `cf_plugin` 框架层 / CppTLM / CppHDL
- `bundles/tlb_bundles_tlm.hh`（mmu-tlb-ptw-impl commit 9a/12 已落地）
- ADR-044 本身（VIPT 锁定 + 数据流契约已稳定）
- `ip/cpu/plugins/` 中除 mmu.h 外的 Plugin
- CtrlLink stall 机制（推迟到独立 `plugin-framework-stall` change——Oracle Q7 DEFER，因为 `PipeBuilder` run 循环未消费 `CtrlLink::should_halt()`）
- DSE 48-case sweep（推迟到 `cache-dse-sweep` change）
- L1Cache 4-way VIPT 升级（推迟到 `cache-phase1.5-4way`）
- Sv32/Sv48 PTW 解码（推迟到 `mmu-sv32-sv48-ext`）
- `ip/mmu/rtl/`（Phase 5+ 沿用）

## Capabilities

### New Capabilities

- `mmu-cache-idx-vipt`: L1CachePlugin VIPT 索引 + PIPT fallback 兼容（commit 1-2）— `L1CachePlugin::lookup` 闭包用 `n->has(MMU_VADDR)` 三元选 idx_src/tag_src；21 baseline [cache] 测试零回归
- `mmu-ptw-completion-dual-write`: PTW completion `WalkCallback`/`FaultCallback` 闭包必须同时写 `pl::PADDR` + `pl::MMU_VADDR`（commit 0 bug 修复契约）— `[node, vaddr]` capture 模式，闭包外部传入 vaddr
- `mmu-cache-bridge-adapter`: `MMUTLMBridgeAdapter` cpptlm `ChStreamModuleBase` 注册 + ch_stream 4 字段窄桥（commit 4）— mirror `L1CacheTLMBridgeAdapter` 结构
- `mmu-soc-minimal-topology`: `soc/mmu_minimal.json` 全链 `tg → mmu_bridge → l1_cache_bridge → memory`（commit 5）— 镜像 `soc/l1_cache_minimal.json` 结构 + MMUPlugin levels/sv39 配置
- `mmu-cache-integration-test`: `tests/cache/test_mmu_cache_integration.cpp`（commit 3）— mock `MMU_VADDR` + `PADDR` Key 验证 VIPT 路径 + PIPT fallback 路径 + miss 对称性
- `mmu-sfence-vma-cache-coherence`: SFENCE.VMA 后 TLB invalidate → cache tag 自然 miss（commit 6 boundary test）— 复用 `mmu-riscv-isa-adapter`（mmu-tlb-ptw-impl archived spec）的 SFENCE.VMA hook 路由

### Modified Capabilities

- **`mmu-vipt-data-flow`**（mmu-tlb-ptw-impl archived）：原 spec 明确**收缩到 MMU 侧**（"L1Cache-side VIPT consumption is deferred to mmu-cache-integration"）。本 change **expand** 该 spec 的 REQUIREMENTS，新增"L1CachePlugin MUST consume `pl::MMU_VADDR` for VIPT index extraction"和"L1CachePlugin MUST fall back to `pl::g_addr` when `pl::MMU_VADDR` is absent (PIPT mode, no-op behavior)"
- **`mmu-cpptlm-bridge`**（mmu-tlb-ptw-impl archived）：原 spec 列出 `MMUTLMBridge` + `MMUTLMBridgeAdapter` 都实装，实际 `MMUTLMBridgeAdapter` 推迟。本 change **修改** spec，移除 adapter 实现要求（adapter 推到本 change 完成 + 明确 narrow 范围——独立 `MMUTLMBridgeAdapter`，不做统一 `MMUCacheBridgeAdapter`）

## Impact

- **新增文件**:
  - `ip/cache/tlm/cache_keys.h`（~30 LOC，commit 1）
  - `src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`（~150 LOC，commit 4）
  - `soc/mmu_minimal.json`（~50 行，commit 5）
  - `tests/cache/test_mmu_cache_integration.cpp`（~150 LOC，commit 3）
  - `tests/soc/test_mmu_minimal_json.cpp`（~100 LOC，commit 5）
  - `tests/mmu/test_ptw_unit.cpp`（~130 LOC，commit 0 — 已落地）
- **修改文件**:
  - `ip/mmu/tlm/MMUPlugin.cpp`：闭包 capture vaddr（~+8/-2 LOC，commit 0 — 已落地）
  - `ip/cache/tlm/L1CachePlugin.cpp`：include mmu_keys.h + lookup 闭包 has() 三元（~+10/-2 LOC，commit 2）
  - `tests/CMakeLists.txt`：确认 MMU_IMPL_SOURCES 含 mmu_bridge_adapter.cpp（commit 1/4 同步）
  - `tests/mmu/test_*.cpp`：**2 个**现有测试文件边界扩展（`test_ptw_unit.cpp` + `test_mmu_plugin.cpp`，commit 6，~+200 LOC）
  - `ip/cpu/plugins/mmu.cpp`（**新增**）：`RiscvMMUPlugin::sfence_vma` + `csr_write_satp` 实装（mmu-tlb-ptw-impl 遗留的第二个 defer，本 change commit 6 落地，~+40 LOC）
  - `ip/mmu/STATUS.md`：IMPLEMENTED → INTEGRATED（commit 7）
  - `CHANGELOG.md`：v0.1.0 条目（commit 7）
  - `ip/mmu/docs/integration.md`：L1Cache 集成契约段（commit 7）
  - `docs/architecture/overview.md`：快照更新（commit 7）
- **不修改**: 见 "What Changes / 不修改" 节
- **依赖与时序**:
  - 本 change 必须在 `mmu-tlb-ptw-impl`（archived 2026-09-12）**之后**实施——MMU 侧双 Key 输出已就位 ✓
  - 本 change 必须在 ADR-044（`cb85fea`/`baa3757`）**之后**实施——VIPT 数据流契约已锁定 ✓
  - commit 0 已紧急 hotfix 落地（`78275db`），main branch 上
  - commit 1-8 在 OpenSpec proposal 归档后实施
- **基线影响**:
  - 当前 ctest 基线：291/291 PASS（288 + commit 0 +3 PTW tests）
  - 增量：3 个 [cache][MMUCacheIntegration] + 4 个 [soc][MMUMinimalJson] + 7 mmu boundary ≈ **+14 cases** → **305 cases**
  - **不**含 48-case DSE sweep（推迟到 `cache-dse-sweep`）
  - **不**含 L1Cache 4-way VIPT 升级（推迟到 `cache-phase1.5-4way`）
- **breaking 变更**:
  - `L1CachePlugin::lookup` 闭包读 `pl::MMU_VADDR` + `pl::PADDR`（新增依赖），但**保持** PIPT fallback 兼容——21 baseline 测试零回归
  - `MMUPlugin` PTW completion 回调语义：success/fault 路径都写 `pl::MMU_VADDR`（commit 0 修补，是**bug fix**而非 breaking）
- **HDL 友好性验证**: 通过现有 `tools/check_plugin_portability.sh` + `tools/verify_plugin_decision.sh` 覆盖；不引入新脚本
- **架构 gate 影响**: 4 architecture gates（verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check）已含本 change 所有改动（`cb85fea`/`baa3757` 已注册 ADR-044），无需新增 gate

## Alternatives Considered

### Alternative A: commit 0 bug fix 拆出独立 hotfix change

**放弃理由**: 用户决策（mmu-cache-integration explore 阶段确认）—— bug fix 与集成紧耦合，同一 change review 更自然；commit message 明确标注 "Part of mmu-cache-integration"。**已采纳**: bug fix 算 mmu-cache-integration commit 0。

### Alternative B: L1Cache 读 `pl::MMU_VADDR` 改成读独立 `cache_keys::CACHE_VADDR` Key（IP 完全解耦）

**放弃理由**: ADR-044 §3.2 明确推荐方案 A：MMUPlugin 写 `pl::MMU_VADDR`，L1Cache 直接读同一 Key（Key identity 匹配）。独立 Key 需要 Bridge Adapter 做 key-to-key 转换，增加 adapter 复杂度。**已采纳**: L1Cache.cpp include `mmu_keys.h` 直接读 MMU_VADDR（IP 间弱耦合，tlm/ 层接口）。

### Alternative C: PIPT fallback 缺失时硬报错（Option B vs Option A）

**放弃理由**: 21 个 baseline [cache] 测试（`tests/cache/` 5 文件，~29 TEST_CASE）全部在无 MMU 的 PipeBuilder 里跑，硬报错会全部回归。ADR-044 §3.3 明确 "Bridge 不需要修改（PIPT 兼容 VIPT）"，回退正是该契约的 Plugin 内镜像。**已采纳**: PIPT 静默回退（Option A）。

### Alternative D: MMUCacheBridgeAdapter 统一 cache + mmu 链式组合

**放弃理由**: 会让 cache 侧测试必须实例化 MMU，破坏 `tests/cache` 隔离性，且违反 `l1_cache_bridge_adapter.h:66-69` 显式收窄范围先例。链式组合应在 SoC JSON connections 层而非 C++ 类层。**已采纳**: 独立 `MMUTLMBridgeAdapter`。

### Alternative E: SoC minimal JSON 用 `traffic_gen → mmu_bridge → memory`（省 l1）

**放弃理由**: 本 change 的存在意义是验证 MMU→Cache 的 VIPT 数据流（ADR-044 §3.2）。缺 L1 cache 节点的 "minimal" JSON 测的是 MMU 单点——那是 `mmu-tlb-ptw-impl` 已覆盖范围（29/29 PASS），放进本 change 是纯重复。**已采纳**: 全链 `tg → mmu_bridge → l1_cache_bridge → memory`。

### Alternative F: mmu 测试 29 → 65（含 48 DSE）打包

**放弃理由**: 65 例里 DSE 配置扫描与 MMU 正确性测试是两种价值的测试——前者验证参数合法性，后者抓回归。混在一起失败定位模糊。ADR-044 §6 检查清单 1-2 项要求的 `static_assert` 已是编译期配置护栏，运行时重复扫 48 个配置边际价值低。**已采纳**: SPLIT — 29→39 本 change（~10 例边界），48 DSE 推迟 `cache-dse-sweep`。

### Alternative G: CtrlLink stall 一起实装

**放弃理由**: 实测 `CtrlLink::halt_when` 接受 `std::function<bool()>` 谓词，`PipeBuilder` run 循环未消费 `should_halt()`。本 change 用 `PTW_ACTIVE` Payload 数据依赖 + RETRY 语义替代（D4 合规，无状态机）。**已采纳**: RETRY 替代方案（详细见 design.md Decision 7），CtrlLink stall 推迟 `plugin-framework-stall`。

### Alternative H: 4 个替换策略从头实装

**放弃理由**: 4 策略已在 `ip/cache/policies/` 完整实现。**不适用**: 本 change 不涉及替换策略新增。

### Alternative I: `soc/cpu/configs/mmu_minimal.json` 路径

**放弃理由**: `soc/cpu/configs/` 目录不存在（Oracle Q4 验证），现有惯例是 `soc/*.json` 平铺。**已采纳**: `soc/mmu_minimal.json`。

### Alternative J: 跳过 OpenSpec proposal 直接实装 commit 1-8

**放弃理由**: OpenSpec 工作流确保 scope 透明、影响可追溯、4 architecture gates 持续验证。直接实装违反仓库 PR-blocking gate（`.github/workflows/architecture-gates.yml`）。**已采纳**: 先 proposal → Momus review → Oracle review → 实装。
