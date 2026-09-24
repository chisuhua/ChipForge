## Why

`mmu-cache-integration`（archived 2026-09-13）落地后，306/306 tests PASS + 4 architecture gates 全绿。但后续探索发现两处**隐藏 bug**：(1) `MMUPlugin::at_stage("ptw_l0/l1/l2")` 闭包当前是空 lambda（commit 7 `78275db` 漏接线），导致 PTW walk 永远停在 `busy_=true` 状态，`WalkCallback` 永远不被调；(2) `MMUTLMBridge::issue_request` 是 stub 注释（`mmu_bridge.cpp:42-44`），不调 MMUPlugin at_stage 链。这两个 bug 是**真实功能缺陷**——任何 TLB miss 走 PTW walk 的真实场景都会卡死，必须在下游 `cache-dse-sweep` / `cpu-mmu-integration` change 启动前修掉。

## What Changes

### 修复 1: PTW at_stage 闭包接线（commit A）

- **`ip/mmu/lib/ptw.h`** 新增 public method `advance_from_stub()`（5 LOC）：MMUPlugin 调它 1 行即可让 PTW 自动从内部 `pte_stub_memory_` 读 PTE 并推进一步
- **`ip/mmu/lib/ptw.cpp`** 实现 `advance_from_stub()`：用 `current_pte_paddr_ >> 12` 算 stub memory idx，调 `advance(pte.raw, current_level_)` 复用状态机
- **`ip/mmu/tlm/MMUPlugin.cpp:77-79`** 把 3 个空 lambda 替换为 `[this]() { ptw_->advance_from_stub(); }`
- **`tests/mmu/test_mmu_plugin.cpp`** 加 1 case `PTWWalkEndToEndViaAtStage`：构造 MMUPlugin + PipeBuilder + stub_write_pte 完整 L2→L1→L0 walk，验证 `pl::PADDR` 和 `pl::MMU_VADDR` 在 `pb.run()` 3 次后正确

### 修复 2: MMUTLMBridge::issue_request + read_response 实装（commit B）

- **`ip/mmu/tlm/MMUPlugin.h/.cpp`** 新增 public `issue_request(node, req)` API（mirror `L1CachePlugin::issue_request` 模式）：把 `TlbReq.vaddr/asid` 写到 `mmu_keys<T>::VADDR` + 内部 `last_vaddr_/current_asid_`，喂给 at_stage 闭包
- **`ip/mmu/tlm/MMUPlugin.h/.cpp`** 新增 public `read_response(node)` API（mirror `L1CachePlugin::read_response`）：从 `tlb_lookup_ifetch` 节点读 `mmu_keys<T>::PADDR/PERMS/EXCEPTION_CODE` + hit 标志；之前 stub 注释导致 spec 场景"read_response 返回 MMUPlugin 响应"无法满足
- **`src/cf_plugin/bridge/mmu_bridge.cpp`** issue_request 改调 `MMUPlugin::issue_request()`（替换原 stub 注释）；mmu_bridge.h 加 `lookup_node_` 成员（mirror L1CacheTLMBridge）；read_response 改调 `MMUPlugin::read_response()`（之前也是 stub 注释，commit 9b 漏实装）
- **`tests/cache/test_mmu_cache_integration.cpp`** 加 1 case `EndToEndTranslationThroughBridge`：调 MMUTLMBridge issue_request → tick → read_response，验证 paddr/hit 正确

### 不修改

- mmu_keys.h（不增 Key）
- L1CachePlugin（不改动）
- MMUPlugin at_stage("tlb_lookup_*") 命中路径（已工作）
- 既有 306 tests 行为（zero regression）
- `MMUTLMBridgeAdapter`（commit 4 实装已 OK，只调 issue_request）
- PipeBuilder / cf_plugin 框架层

## Capabilities

### New Capabilities

- `ptw-at-stage-wiring`: PTW walk 通过 MMUPlugin at_stage 闭包端到端推进 — PTW::advance_from_stub() 提供 stub memory 自动读取，MMUPlugin::at_stage("ptw_l0/l1/l2") 每周期调 1 次，Sv39 3-level walk 在 3 cycles 内完成。修复"PTW 永远 busy"隐藏 bug。

### Modified Capabilities

- `mmu-cpptlm-bridge`: MMUTLMBridge::issue_request 从 stub 改为真实调 MMUPlugin::issue_request —— 修复"bridge issue_request 是空注释"隐藏 bug。spec contract 不变（issue_request 行为是 implementation detail，不影响外部契约）。**delta 只覆盖 requirement body 修改，不改 header text**。

## Impact

- **修改文件**:
  - `ip/mmu/lib/ptw.{h,cpp}`: +1 method ~10 LOC
  - `ip/mmu/tlm/MMUPlugin.{h,cpp}`: at_stage 3 行替换 + `ptw()` 访问器 + `issue_request` + `read_response` ~50 LOC
  - `src/cf_plugin/bridge/mmu_bridge.{h,cpp}`: +lookup_node_ 成员 + issue_request/read_response 改调 ~20 LOC
  - `tests/mmu/test_mmu_plugin.cpp`: +1 case `PTWWalkEndToEndViaAtStage`
  - `tests/cache/test_mmu_cache_integration.cpp`: +1 case `EndToEndTranslationThroughBridge`
- **基线影响**: 306 → **308** PASS (+2 test cases)；既有 306 零回归
- **阻塞的下游**:
  - `cache-dse-sweep` change 需要 PTW 实际工作（不同 satp_ppn 配置扫描依赖 walk 完成）
  - `cpu-mmu-integration` change 已于 2026-09-14 归档（走 `MMUPlugin::multi_tlb()` 路径，不依赖 bridge issue_request）；本修复仍作为 Bridge 契约补全
- **breaking 变更**: 无（修复既有 bug，不改 API contract）

## Alternatives Considered

### Alternative A: 推迟 PTW walk 到独立 change（如 `ptw-walk-completer`）
**放弃理由**: 修复范围小（~80 LOC, 2 commits），独立 change 开销 > 收益；MMUTLMBridge issue_request 强依赖 PTW 实际工作能力，拆开反而要协调两个 change 的依赖时序。

### Alternative B: 用 mock PTE 内存替换 stub memory（生产内存模型）
**放弃理由**: 当前 IP 是 mmu-cache-integration 阶段，真实内存模型属于 `cpu-mmu-integration` change；stub memory 是测试友好接口（`stub_write_pte/stub_read_pte` public accessor），不需要现在换。

### Alternative C: 重写 MMUPlugin at_stage 闭包逻辑，把 PTW 状态机搬到 tlm 层
**放弃理由**: 违反 D4 Plugin 范式（lib/ 与 tlm/ 严格分层，lib/ 0 依赖 Plugin 框架）；当前 PTW 已经是 lib 层状态机，MMUPlugin 只调 `advance_from_stub()` 1 行推进即可，最小侵入。

### Alternative D: 用 CtrlLink halt_when 做 PTW busy 框架级 stall（commit A 先实现 PTW advance 不依赖 stall）
**放弃理由**: CtrlLink stall 已在 mmu-cache-integration Decision Q7 推迟到 `plugin-framework-stall` change（PipeBuilder run 循环未消费 `should_halt()`）。PTW advance 在 framework stall 落地前可以先做（依赖 stub memory 单 cycle 推进，不需跨 cycle stall）。
