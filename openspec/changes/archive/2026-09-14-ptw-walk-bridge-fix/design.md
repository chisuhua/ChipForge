## Context

`mmu-cache-integration`（archived 2026-09-13）落地后 306/306 tests PASS + 4 architecture gates 全绿。但探索发现两处**真实功能缺陷**，下游 `cache-dse-sweep` / `cpu-mmu-integration` change 启动前必须修掉：

1. **PTW walk 永远 busy**：`MMUPlugin::at_stage("ptw_l0/l1/l2")` 闭包当前是空 lambda（commit 7 `78275db` 漏接线）。PTW walk 启动后停在 `busy_=true` 状态，`WalkCallback` 永远不被调。
2. **`MMUTLMBridge::issue_request` 是 stub**：`mmu_bridge.cpp:42-44` 写"完整 issue_request 实装推迟" 注释。即使 PTW 修好，Bridge 也不调用 MMUPlugin → SoC 端到端仿真失败。

### 利益相关方

- `cache-dse-sweep` change（阻塞）：DSE sweep 需 PTW 实际工作
- `cpu-mmu-integration` change（阻塞）：CPU 端发起请求需要 MMUTLMBridge issue_request 真实工作
- `plugin-framework-stall` change（不直接依赖）：PTW advance 落地后，RETRY 替代方案可被替换

## Goals / Non-Goals

### Goals

1. 修复 PTW at_stage 接线 bug（commit A）：Sv39 3-level walk 在 3 cycles 内完成
2. 修复 MMUTLMBridge::issue_request stub（commit B）：Bridge 真实调用 MMUPlugin issue_request
3. baseline 306 → 308 PASS（+2 test cases），既有 306 zero regression
4. 4 architecture gates 继续全绿

### Non-Goals

1. 真实内存模型（stub memory 仍是 stub）—— `cpu-mmu-integration` change 范围
2. CtrlLink stall 框架改造 —— `plugin-framework-stall` change 范围
3. Sv32/Sv48 PTW 解码完整化 —— `mmu-sv32-sv48-ext` change 范围
4. DSE sweep 实施 —— `cache-dse-sweep` change 范围
5. CPU 集成 MMU —— `cpu-mmu-integration` change 范围

## Decisions

### Decision 1: PTW::advance_from_stub() 新 public API

**选择**: 在 PTW 加 public method `advance_from_stub()`，MMUPlugin at_stage 闭包调它 1 行。

**替代方案**:
- A. PTW 加 accessor (current_pte_paddr_ + stub_read_pte + current_level_) 让 MMUPlugin 内部组合
- B. 把 stub memory 搬到 MMUPlugin 成员，PTW::advance 接收 raw PTE 不变

**理由**: A 暴露多个 internal accessor 增耦合；B 违反 lib/ 与 tlm/ 分层（stub memory 应在 lib 层 PTW）。advance_from_stub() 是单一封装点，lib/ 自封装，tlm/ 一行调用。最小侵入，HDL 1:1 友好（stub memory 在 PTW 内部，未来 RTL 化时 stub 替换为真实 memory fetch）。

### Decision 2: MMUPlugin::issue_request API mirror L1CachePlugin

**选择**: MMUPlugin 加 public `issue_request(node, req)` method，签名 mirror `L1CachePlugin::issue_request`。

**替代方案**:
- A. MMUTLMBridge 直接调 `pb_.run()` 后读 `pl::PADDR`（绕过 issue_request API）
- B. RiscVMMUPlugin::issue_request（推迟到 cpu-mmu-integration change）

**理由**: A 跳过 issue_request 抽象层，test API 不一致；B 推迟后 MMUTLMBridge 仍要 stub。mirror L1CachePlugin 模式让 Bridge API 与 Cache Bridge API 平行（同一调用约定）。

### Decision 3: mmu_bridge.h 加 lookup_node_ 成员

**选择**: `MMUTLMBridge` 持 `std::shared_ptr<PipeNode> lookup_node_`（mirror L1CacheTLMBridge），issue_request 通过它把 vaddr 写到 MMUPlugin 的 lookup 节点。

**替代方案**:
- A. issue_request 直接调 `plugin_->setup(pb_)` 然后 `pb_.run()` —— 复杂且违反 D4
- B. MMUPlugin issue_request 接受全局 Payload 而不是 PipeNode —— 破坏 lib/ 0 依赖

**理由**: A 复杂且 D4 不允许 plugin::setup 重入；B 破坏分层。mirror L1CacheTLMBridge 的 `payload_node_` 模式最干净。

### Decision 4: 测试覆盖用 stub memory 直接驱动

**选择**: commit A 的测试用 `stub_write_pte` 准备完整 L2→L1→L0 PTE chain，verify 3 cycles `pb.run()` 后 `WalkCallback` fired with correct paddr。

**替代方案**:
- A. 集成测试（end-to-end MMUTLMBridge → MMUPlugin → PTW）—— commit B 范围
- B. 单元测试（PTW::advance 直接 API）—— 已存在

**理由**: A 是 commit B 范围；B 已存在但**不测 at_stage 路径**。commit A 关键是补 at_stage 路径回归网，stub memory 是测试友好接口（commit 4 公共 API）。

## Risks / Trade-offs

### Risk 1: stub memory PTE 计算不精确

PTW 用 `current_pte_paddr_ >> 12 & 0xFFF` 算 stub idx，但生产 RISC-V 系统 PTE 物理地址从 satp 链式计算。stub 测试只验证 walk 推进，不验证 paddr 计算正确性。**Mitigation**: 在 commit B 的 end-to-end test 里也覆盖 paddr 正确性；stub idx 计算是 PTW::advance 内部细节，本测试只验 walk 跑通。

### Risk 2: at_stage ptw_l0/l1/l2 重复闭包导致 walk 推进过快

若单 cycle 触发 3 个 ptw_l* 闭包，Sv39 3-level walk 在 1 cycle 内完成（违反 ADR-040 "1 cycle per logical stage" 假设）。**Mitigation**: 当前 `pb.run()` 单 cycle 遍历所有闭包——若 Sv39 3-level 走完会 1 cycle 触发 3 闭包，导致 1 cycle 完成。这与 ADR-040 logical stage 模型冲突，但与现有 mmu-tlb-ptw-impl 设计意图一致（commit 7 文档：PTW 3 级 sub-pipe 是逻辑拆分，为 Phase 6 cycle-scheduling 预留）。Phase 6 RTL 化时用 stage scheduling 解决。当前 TLM 阶段可接受。

### Risk 3: MMUPlugin::issue_request 改 last_vaddr_ 状态

`last_vaddr_` 是 MMUPlugin 私有成员（at_stage 闭包读），issue_request 改它可能被并发起 walk 冲突。**Mitigation**: `ptw_max_inflight=2`（默认）允许 2 并发 walk，但当前 TLB lookup 串行（`at_stage("tlb_lookup_*")` 闭包），issue_request 串行喂 vaddr 不会冲突。Phase 5 RTL 化时再加并发保护。

### Trade-off: commit A vs commit B 顺序

A 修复 PTW at_stage 接线 → 内部测试通过 → commit B 用真实 PTW 路径做 end-to-end。如果先做 B，issue_request 真实调用但 PTW 不工作，bridge test 会失败 → 难以隔离 root cause。**A 先 B 后**。

## Migration Plan

### Phase 1: PTW walk 端到端（commit A）

1. `ip/mmu/lib/ptw.h` 加 `void advance_from_stub();` public method
2. `ip/mmu/lib/ptw.cpp` 实现：用 `current_pte_paddr_` 算 stub idx，读 PTE，调 `advance(pte.raw, current_level_)`
3. `ip/mmu/tlm/MMUPlugin.cpp:77-79` 把空 lambda 改为 `[this]() { ptw_->advance_from_stub(); }`
4. `tests/mmu/test_mmu_plugin.cpp` 加 `PTWWalkEndToEndViaAtStage` test case
5. 验证: `[mmu]` 40 → 41 PASS, baseline 306 → 307 PASS

### Phase 2: Bridge 端到端（commit B）

1. `ip/mmu/tlm/MMUPlugin.h/.cpp` 加 public `issue_request(node, req)` method（mirror L1CachePlugin）
2. `src/cf_plugin/bridge/mmu_bridge.h` 加 `std::shared_ptr<PipeNode> lookup_node_` 成员
3. `src/cf_plugin/bridge/mmu_bridge.cpp` issue_request 改调 `plugin_->issue_request(lookup_node_, pod_req)`
4. `tests/cache/test_mmu_cache_integration.cpp` 加 `EndToEndTranslationThroughBridge` test case
5. 验证: `[cache][MMUCacheIntegration]` 3 → 4 PASS, baseline 307 → 308 PASS

### Phase 3: 最终验证 + archive

1. 4 architecture gates 验证
2. 5 次连跑稳定性
3. `openspec archive ptw-walk-bridge-fix`
4. CHANGELOG v0.1.1 + ip/mmu/STATUS.md 更新

### 回滚策略

- commit A 单独可回滚（移除 advance_from_stub() + 恢复空 lambda）
- commit B 单独可回滚（issue_request 改回 stub 注释）

## Open Questions

### Q1: 单 cycle 推进 3 level 的语义冲突？

Sv39 3-level walk 在 1 cycle 内完成（因为 3 个 at_stage 闭包都在单 `pb.run()` 里跑）vs ADR-040 logical stage 模型。**已知妥协**：当前 TLM 阶段可接受，Phase 6 RTL 化时加 stage scheduling。

### Q2: stub memory 测试 vs 真实内存模型

commit A 测试用 stub memory（commit 4 公共 API），但生产 RISC-V 系统 PTW 从 memory fetch PTE。**缓解**: stub memory 是测试友好接口；生产系统需要真实内存模型（cpu-mmu-integration change 范围）。

### Q3: commit B 真实 issue_request 是否需要访问权限检查？

L1CachePlugin::issue_request 不做权限检查（仅写 lookup node），MMUPlugin::issue_request 同样不做（权限检查在 at_stage 闭包内做）。**OK mirror 模式**。

## References

- `ip/mmu/tlm/MMUPlugin.cpp:77-79` 空 lambda — bug 1
- `src/cf_plugin/bridge/mmu_bridge.cpp:42-44` stub 注释 — bug 2
- `ip/mmu/lib/ptw.cpp:42-84` `advance(pte_raw, level)` 状态机
- `ip/mmu/lib/ptw.cpp:86-93` `stub_write_pte/stub_read_pte` 测试 API
- `ip/cache/tlm/L1CachePlugin.cpp:202-210` issue_request mirror 模式
- `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` payload_node_ mirror 模式
