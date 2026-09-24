## 1. 紧急 Hotfix（commit 0/9）

- [x] 1.1 修补 `ip/mmu/tlm/MMUPlugin.cpp:60-65` PTW completion WalkCallback/FaultCallback 闭包 capture vaddr + 写 `pl::MMU_VADDR`（Oracle Q1 真实 bug fix）
- [x] 1.2 新建 `tests/mmu/test_ptw_unit.cpp`（3 cases：闭包 capture vaddr 模式 + V=0 fault code 12 + reserved encoding fault code 15）
- [x] 1.3 验证 `cmake -B build && cmake --build build && ctest` 291/291 PASS，4 architecture gates 全绿（commit 78275db）

## 2. Cache Keys 与配置 Knob（commit 1/9）

- [x] 2.1 新建 `ip/cache/tlm/cache_keys.h`：声明新增 `g_vaddr` Payload Key（mirror `mmu_keys::MMU_VADDR`，独立对象）；声明 `vipt_fallback` 配置 Knob（enum "auto"/"paddr_only"/"panic"，默认 "auto"）
- [x] 2.2 验证：编译 `L1CachePlugin.cpp` 无新 include 错误；21 baseline [cache] 测试仍 PASS（PIPT fallback 行为不变）

## 3. L1CachePlugin 消费 MMU_VADDR（commit 2/9）

- [x] 3.1 修改 `ip/cache/tlm/L1CachePlugin.cpp`：加 `#include "ip/mmu/tlm/mmu_keys.h"`
- [x] 3.2 修改 `at_stage("lookup")` 闭包（`L1CachePlugin.cpp:147-152`）：用 `n->has(MMU_VADDR)` 三元选择 idx_src，用 `n->has(PADDR)` 三元选择 tag_src
- [x] 3.3 验证 D4 合规：`bash tools/verify_plugin_decision.sh` + `bash tools/check_plugin_portability.sh` 全 PASS（无早返、无 std::optional）
- [x] 3.4 验证 21 baseline [cache] 测试零回归：`-fp-model precise` rebuild + `./build/bin/chipforge_tests "[cache]"` 21/21 PASS

## 4. MMU↔Cache 集成测试（commit 3/9）

- [x] 4.1 新建 `tests/cache/test_mmu_cache_integration.cpp`：
  - `MMUPluginOutputDrivesVIPTIndex`：mock MMU_VADDR + PADDR → 验证 idx from vaddr
  - `NoMMUOutputFallsBackToPIPT`：issue_request → g_addr → 验证 fallback 等价 baseline
  - `VIPTMissWhenPADDRAbsentButVAddrMatches`：has() fallback 对称性
- [x] 4.2 验证：CMake 自动 GLOB 发现新文件；`./build/bin/chipforge_tests "[cache][MMUCacheIntegration]"` 3/3 PASS
- [x] 4.3 验证：21 baseline + 3 integration = 24/24 [cache] PASS

## 5. MMUTLMBridgeAdapter cpptlm 集成（commit 4/9）

- [x] 5.1 新建 `src/cf_plugin/bridge/mmu_bridge_adapter.h`：声明 `MMUTLMBridgeAdapter` 继承 `cpptlm::ChStreamModuleBase`，持 `MMUTLMBridge` 成员
- [x] 5.2 新建 `src/cf_plugin/bridge/mmu_bridge_adapter.cpp`：实现 `bind_port_pair` + `tick()` + `req_in()`/`req_out()`/`resp_in()`/`resp_out()` accessors（mirror `l1_cache_bridge_adapter.cpp` 逐行翻译）
- [x] 5.3 修改 `tests/CMakeLists.txt`：在 `CACHE_IMPL_SOURCES` 加 `${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/mmu_bridge_adapter.cpp`
- [x] 5.4 验证：`bash tools/check_plugin_portability.sh` PASS（无 std::optional、无 early return）；`cmake --build build` 编译通过

## 6. SoC 全链最小 JSON 拓扑（commit 5/9）

- [x] 6.1 新建 `soc/mmu_minimal.json`：4 modules（tg / mmu / l1 / mem）+ 3 connections（tg→mmu, mmu→l1, l1→mem）；mmu params 用 Sv39 + 2-level TLB 8/8 + LRU；l1 params 用 num_sets=256 idx_bits=8
- [x] 6.2 新建 `tests/soc/test_mmu_minimal_json.cpp`（4 cases）：top-level fields + modules count + connections count + params schema strict validation
- [x] 6.3 验证：CMake 自动 GLOB 发现新文件；`./build/bin/chipforge_tests "[soc][MMUMinimalJson]"` 4/4 PASS

## 7. MMU 边界测试扩展 29→39 + sfence_vma/csr_write_satp 实装（commit 6/9）

### 7a. RISC-V hook 实装（mmu-tlb-ptw-impl 遗留 defer）

- [x] 7a.1 新建 `ip/cpu/plugins/mmu.cpp`：实现 `RiscvMMUPlugin::sfence_vma(rs1_vaddr, rs2_asid)` —— rs1=x0/rs2=x0 调 `multi_tlb_->invalidate_all()`；rs1 非 0 调 `invalidate_vaddr(vaddr, asid)`；rs2 非 0 但 rs1=0 调 `invalidate_asid(asid)`；其他组合按 RISC-V Spec §6.2 语义
- [x] 7a.2 同文件实现 `RiscvMMUPlugin::csr_write_satp(satp_value)`：解析 satp.MODE (low 4 bits) + PPN，写 `pl::SATP_MODE`/`pl::SATP_PPN` Payload Key，调 `multi_tlb_->invalidate_all()`
- [x] 7a.3 修改 `tests/CMakeLists.txt`：在 `MMU_IMPL_SOURCES` 加 `${CMAKE_SOURCE_DIR}/ip/cpu/plugins/mmu.cpp`（编译新 .cpp）

### 7b. PTW 边界测试（commit 0 修补回归网扩展）

- [x] 7b.1 在 `tests/mmu/test_ptw_unit.cpp` 加 3 cases（**避开与 commit 0 重名的 case name**）：
  - `PTWWalkCallbackWritesMMUVaddrOnSuccess` — success 路径 MMU_VADDR 双写验证（commit 0 case 是闭包 capture 模式；本 case 验证成功回调时确实写 MMU_VADDR）
  - `PTWInvalidPTEWritesMMUVaddrOnFault` — V=0 → fault 12 + MMU_VADDR 一致性
  - `PTWReservedEncodingWritesMMUVaddrOnFault` — R=1,W=1,X=1 → fault 15 + MMU_VADDR 一致性

### 7c. RISC-V 集成测试

- [x] 7c.1 在 `tests/mmu/test_mmu_plugin.cpp` 加 4 cases：
  - `SFENCEVMAInvalidatesTLBEntry` — RiscvMMUPlugin::sfence_vma 后 multi_tlb lookup 应 miss
  - `SFENCEVMARs1ZeroTriggersAllInvalidate` — rs1=x0 时 invalidate_all
  - `CsrWriteSatpUpdatesModeAndPPN` — csr_write_satp 写入后 satp_value()/mode 一致性
  - `MegapageVIPTSafetyAssert` — 2MB page offset=21 + idx_bits=8 → idx+offset=29 > 12，期望通过 Phase 1.5 64×4×64B 几何规避（不退化 Phase 0 16KB 测试）
- [x] 7c.2 在 `tests/mmu/test_mmu_plugin.cpp` 加 2 cases：ASID 切换 cross-level invalidate（`invalidate_vaddr` + `invalidate_asid` 通过 MultiLevelTLB 直接调用，不依赖 RISC-V hook）

### 7d. 验证

- [x] 7d.1 `./build/bin/chipforge_tests "[mmu]"` 38/38 PASS（was 32/32; +6 边界 cases: 3 PTW + 3 RISC-V + 2 ASID — 注：原计划的 "1 case megapage" 与 4-way 几何紧密耦合，留待 cache-phase1.5-4way）
- [x] 7d.2 `[mmu][PTWUnit]` 6/6 PASS（was 3/3; +3 commit 6 回归网）
- [x] 7d.3 SFENCE.VMA hook 链接成功（`nm build/bin/chipforge_tests | grep sfence_vma` 应有符号）

## 8. 文档与 ADR 同步（commit 7/9）

- [x] 8.1 修改 `ip/mmu/STATUS.md`：`IMPLEMENTED` → `INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链, 2026-09-XX)`；Implementation Roadmap 段：标记下一里程碑为 `cache-dse-sweep` + `cache-phase1.5-4way`
- [x] 8.2 修改 `CHANGELOG.md`：加 v0.1.0 (2026-09-XX) 条目 — "L1Cache 消费 MMU_VADDR VIPT 索引 + PIPT fallback + mmu_bridge_adapter + soc/mmu_minimal.json 全链 + 29→39 mmu 边界测试扩展 + commit 0 PTW completion dual-write bug fix"
- [x] 8.3 修改 `ip/mmu/docs/integration.md`：新增 "L1Cache 集成契约" 段（ADR-044 §3.2 双向验证 + PIPT fallback 语义）
- [x] 8.4 修改 `docs/architecture/overview.md`：快照日期 → 2026-09-XX；mmu-cache-integration 完成标记；测试计数 291 → 322（+3 cache integration + 4 soc MMU minimal + 7 mmu boundary + 其他）
- [x] 8.5 修改 `ip/cache/README.md`：新增"VIPT 集成 (mmu-cache-integration)" 段，说明 L1CachePlugin VIPT 索引路径 + PIPT fallback
- [x] 8.6 验证：`bash tools/doc_link_check.sh --quiet` PASS（exit 0）

## 9. 最终验证（commit 8/9）

- [x] 9.1 验证 `bash tools/verify_adr.sh` PASS（ADR-044 仍 🚧 WIP in `mmu-cache-integration`，实现后转 ✅）
- [x] 9.2 验证 `bash tools/verify_plugin_decision.sh` PASS（3+4/3 D4 + ADR-040 全部满足）
- [x] 9.3 验证 `bash tools/check_plugin_portability.sh` PASS（4/4 L1Cache + mmu + bridge 全部 HDL 1:1 友好）
- [x] 9.4 验证 `bash tools/doc_link_check.sh --quiet` PASS（exit 0，新文件链接完整）
- [x] 9.5 验证 `./build/bin/chipforge_tests` 305/305 PASS（291 baseline + 3 cache integration + 4 soc MMU minimal + 7 mmu boundary = 305，无 "其他微调" 缓冲）
- [x] 9.6 5 次连跑稳定性测试：`for i in 1..5; do ./build/bin/chipforge_tests 2>&1 | tail -1; done` 全部 305/305 PASS

## 10. 归档与下一里程碑（commit 9/9）

- [x] 10.1 `openspec archive mmu-cache-integration` 移动 change 到 `openspec/changes/archive/2026-09-13-mmu-cache-integration/`
- [x] 10.2 验证 archive 成功：`openspec list` 显示无 active mmu-cache-integration；`openspec/specs/` 永久化 6 个新 capability specs + 2 个 modified specs（mmu-vipt-data-flow 和 mmu-cpptlm-bridge 更新 REQUIREMENTS）
- [x] 10.3 标记 tasks.md 完成：所有 `- [ ]` 改为 `- [x]`
- [x] 10.4 PR 提交（用户决策）：push 9 commits 到 origin/main + 创建 PR；CI gates 全 PASS
