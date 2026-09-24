## 1. PTW walk 端到端（commit A/2）

- [x] 1.1 `ip/mmu/lib/ptw.h` 新增 public `void advance_from_stub()` 声明
- [x] 1.2 `ip/mmu/lib/ptw.cpp` 实现 `advance_from_stub()`：用 `current_pte_paddr_` 算 stub idx，读 PTE，调 `advance(pte.raw, current_level_)`
- [x] 1.3 `ip/mmu/tlm/MMUPlugin.cpp:79-81` 把 3 个空 lambda 替换为 `[this]() { ptw_->advance_from_stub(); }`
- [x] 1.4 `ip/mmu/tlm/MMUPlugin.h` 加 public `cf::ip::mmu::PTW* ptw() const { return ptw_.get(); }` 访问器（mirror `multi_tlb()`，让测试可种 PTE chain）
- [x] 1.5 `tests/mmu/test_mmu_plugin.cpp` 加 `PTWWalkEndToEndViaAtStage` test case：
  - 构造 Sv39 MMUPlugin + 2-level TLB 8/8 + LRU
  - pb.register_plugin + pb.build()
  - **Setup**: `ptw()->stub_write_pte` 准备完整 L2→L1→L0 PTE chain（leaf ppn=0x80000），satp_ppn=0x1000
  - `pb.run()` ≥1 cycle（Sv39 3-level walk 当前 TLM 拓扑下在 1 cycle 内完成），验证 `pl::PADDR=0x80000000` + `pl::MMU_VADDR=0x40000000`
- [x] 1.6 验证：`./build/bin/chipforge_tests "[mmu]"` 41/41 PASS，baseline 307/307 PASS，4 architecture gates 全绿

## 2. MMUTLMBridge issue_request + read_response 实装（commit B/2）

- [x] 2.1 `ip/mmu/tlm/MMUPlugin.h` 加 public `void issue_request(const std::shared_ptr<cf::plugin::PipeNode>& n, const cf::bundles::TlbReq& req)` 声明
- [x] 2.2 `ip/mmu/tlm/MMUPlugin.cpp` 实现 `issue_request`：写 `mmu_keys<T>::VADDR` + 设 `last_vaddr_/current_asid_`
- [x] 2.3 `ip/mmu/tlm/MMUPlugin.h/.cpp` 加 public `cf::bundles::TlbResp read_response(const std::shared_ptr<cf::plugin::PipeNode>& n) const`：从 `tlb_lookup_ifetch` 节点读 `mmu_keys<T>::PADDR/PERMS/EXCEPTION_CODE` + `hit` 标志（mirror `L1CachePlugin::read_response`，L1CachePlugin.cpp:232-239）。
  - **hit 推导规则**: `hit = (exception_code == 0 && PADDR != 0)` —— PERMS 在 mmu-tlb-ptw-impl commit 8/12 没有写闭包（walk_cb 丢弃 perms 参数），暂以 PADDR!=0 间接证明 hit；后续 mmu-cache-integration 后续 change 可补 PERMS/HIT payload key。
- [x] 2.4 `src/cf_plugin/bridge/mmu_bridge.h` 加 `std::shared_ptr<cf::plugin::PipeNode> lookup_node_` 成员（mirror L1CacheTLMBridge）
- [x] 2.5 `src/cf_plugin/bridge/mmu_bridge.cpp` issue_request 改调 `plugin_->issue_request(lookup_node_, pod_req)`（替换 stub 注释）；read_response 改调 `plugin_->read_response(lookup_node_)` 替换 stub
- [x] 2.6 `tests/cache/test_mmu_cache_integration.cpp` 加 `EndToEndTranslationThroughBridge` test case：
  - 构造 MMUTLMBridge + MMUPlugin
  - **Setup**: `plugin->ptw()->stub_write_pte` 准备完整 L2→L1→L0 PTE chain（leaf ppn=0x80000，satp_ppn=0x1000），或 `plugin->multi_tlb()->level(0)->insert(0x40000000, 0x80000000, 0, 0xFF)` 预填 TLB（避免走 walk 路径，直接 hit）
  - `bridge.issue_request({vaddr=0x40000000, asid=0})` → `bridge.tick()` → `bridge.read_response()` 验证 paddr=0x80000000, exception=0, hit=true
- [x] 2.7 验证：`./build/bin/chipforge_tests "[cache][MMUCacheIntegration]"` 4/4 PASS，baseline 308/308 PASS，4 architecture gates 全绿

## 3. 最终验证（commit end）

- [x] 3.1 `bash tools/verify_adr.sh` PASS
- [x] 3.2 `bash tools/verify_plugin_decision.sh` PASS（PTW advance + Bridge issue_request 都符合 D4）
- [x] 3.3 `bash tools/check_plugin_portability.sh` PASS（lib/ 0 依赖 + tlm/ HDL 友好）
- [x] 3.4 `bash tools/doc_link_check.sh --quiet` PASS（exit 0）
- [x] 3.5 5 次连跑稳定性：`for i in 1..5; do ./build/bin/chipforge_tests 2>&1 | tail -1; done` 全部 308/308 PASS

## 4. 归档

- [x] 4.1 标记 tasks 完成：`sed -i 's|^- \[ \]|- [x]|g' openspec/changes/ptw-walk-bridge-fix/tasks.md`
- [x] 4.2 `openspec archive ptw-walk-bridge-fix` 移动到 `openspec/changes/archive/2026-09-13-ptw-walk-bridge-fix/`
- [x] 4.3 验证 archive 成功：`openspec list` 显示无 active ptw-walk-bridge-fix
- [x] 4.4 永久化 specs: `openspec/specs/{ptw-at-stage-wiring,mmu-cpptlm-bridge}/spec.md`

## 5. 文档同步（commit end 内）

- [x] 5.1 `CHANGELOG.md` 加 v0.1.1 (2026-09-13) 条目 — "PTW at_stage wiring bug fix + MMUTLMBridge issue_request 实装"
- [x] 5.2 `ip/mmu/STATUS.md` 段落：ptw-walk-bridge-fix change 完成，下一里程碑 cache-dse-sweep
- [x] 5.3 `ip/mmu/docs/integration.md` 加 "PTW walk end-to-end" 段（解释 stub memory + advance_from_stub 模式）

## 6. 下一里程碑

- [x] 6.1 下一 change 候选: `cache-dse-sweep` (DSE 12-case Pareto, 现在 PTW 实际工作可以扫)
- [x] 6.2 下一 change 候选: `cpu-mmu-integration` (RISC-V CPU 集成 MMU, 现在 Bridge issue_request 真实工作)
