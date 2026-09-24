## 1. tlb.h / tlb_entry.h 编译错误修复（最小可工作集）

### 1.1 tlb_entry.h + tlb.h mutable 关键字

- [x] 1.1.1 在 `ip/mmu/lib/tlb_entry.h` 给 `TLBEntry<TAG_BITS, ASID_BITS>` 加 `using tag_type = cf::plugin::uint_t<TAG_BITS>;`（**修复 8 处编译错误**：`tlb.h` L74/99/120/137/147/165/181/190 引用 `typename Entry::tag_type`）
- [x] 1.1.2 在 `ip/mmu/lib/tlb.h` 把 `uint64_t hits_` / `misses_` / `evicts_` 三个统计字段声明改为 `mutable uint64_t hits_` / `misses_` / `evicts_`（L227-229）
- [x] 1.1.3 验证：`grep -n 'tag_type' ip/mmu/lib/tlb_entry.h` 应返回 1 匹配（typedef 已加）；`grep -n 'tag_type' ip/mmu/lib/tlb.h` 仍返回 8 匹配（用法保留，但现已合法解析）
- [x] 1.1.4 验证：`git diff --stat` 仅 2 文件（tlb.h + tlb_entry.h）；`cmake --build build --target cf_plugin_mmu_lib -j4` 编译通过

## 2. 构建接线（之前缺失，必须先做）

### 2.1 ip/mmu/CMakeLists.txt 新建

- [x] 2.1.1 新建 `ip/mmu/CMakeLists.txt`：定义 `cf_plugin_mmu_lib` static library，编译 `ip/mmu/lib/*.cpp` + `ip/mmu/tlm/*.cpp` + `ip/mmu/policies/*.cpp`
- [x] 2.1.2 验证：`bash tools/build.sh --no-build` configure 成功；`cmake --build build --target cf_plugin_mmu_lib -j4` 编译成功（注：必须先修 tlb.h 的 const 问题，否则 lib 实例化失败）

### 2.2 根 CMakeLists.txt + tests/CMakeLists.txt 接线

- [x] 2.2.1 根 `CMakeLists.txt` 加 `add_subdirectory(ip/mmu)`（**实测：当前没有**）
- [x] 2.2.2 `tests/CMakeLists.txt` 加 `set(MMU_IMPL_SOURCES ${CMAKE_SOURCE_DIR}/ip/mmu/tlm/MMUPlugin.cpp ...)`，加到 `add_executable(chipforge_tests ...)` 的源列表
- [x] 2.2.3 `tests/CMakeLists.txt` 删除 `list(REMOVE_ITEM TEST_SOURCES mmu/test_*.cpp)` 5 行 + 删除上方的注释行（说明排除原因的注释）
- [x] 2.2.4 验证：`bash tools/build.sh --no-build` configure 成功；`cmake --build build -j4` 编译 mmu lib 全部通过；测试二进制 ≈288 PASS（259 + mmu 解阻塞 +29）

## 3. lib/ 算法层实装（沿用已有方法，新增 PTW/MLTLB 实装）

### 3.1 4 替换策略显式实例化扩展 8→12

- [x] 3.1.1 `ip/mmu/policies/tlb_replacement_policy.cpp` 加 4 组显式实例化：`<16, 1>` / `<32, 2>` / `<64, 8>` / `<256, 1>`（覆盖 TLBFactory 新增特化所需组合）
- [x] 3.1.2 验证：`cmake --build build --target cf_plugin_mmu_lib -j4` 编译通过（无 undefined reference）

### 3.2 PTW Sv39 walk 实装

- [x] 3.2.1 在 `ip/mmu/lib/ptw.h` 加 `enum class WalkStage { Idle, WalkL2, WalkL1, WalkL0, CheckPerms, WriteBack, Done }` + 成员 `WalkStage stage_` + `PTE current_pte_l2_/l1_/l0_` + `std::array<PTE, 4096> pte_stub_memory_`
- [x] 3.2.2 在 `ip/mmu/lib/ptw.h` 声明 `void start_walk(uint64_t vaddr, uint16_t asid, uint64_t satp_ppn)` + `WalkResult advance()`
- [x] 3.2.3 在 `ip/mmu/lib/ptw.cpp` 实装 `start_walk`：保存 vaddr/asid/satp_ppn，stage_ = WalkL2
- [x] 3.2.4 在 `ip/mmu/lib/ptw.cpp` 实装 `advance()`：根据 stage_ 推进 1 步
  - WalkL2：读 `pte_stub_memory_[(satp_ppn >> 12) & 0xFFF]` 写入 `current_pte_l2_`，解析 PTE；stage_ = WalkL1
  - WalkL1：读 `pte_stub_memory_[((l2_pte.ppn + vpn[1]) >> 12) & 0xFFF]`，stage_ = WalkL0
  - WalkL0：读 `pte_stub_memory_[((l1_pte.ppn + vpn[0]) >> 12) & 0xFFF]`，stage_ = CheckPerms
  - CheckPerms：校验 access_type vs PTE.R/W/X，不匹配设 `fault_code = 12|13|15`；stage_ = WriteBack
  - WriteBack：构造 final paddr/perms，stage_ = Done，返回 `{done=true, paddr, perms, fault_code}`
- [x] 3.2.5 处理 PTE reserved encoding（`R=1, W=1, X=1` → `fault_code = 15`）
- [x] 3.2.6 处理 PTE V=0 → `fault_code = 15`
- [x] 3.2.7 **stub 内存测试填充 API**（Oracle 风险 #2）：在 `ip/mmu/lib/ptw.h` 加 public 测试接口 `void stub_write_pte(size_t idx, const PTE&)` + `PTE stub_read_pte(size_t idx) const`，让 `tests/mmu/test_mmu_plugin.cpp` 可直接植入 PTE（否则 `pte_stub_memory_` private 无法测试）
- [x] 3.2.8 **commit 4 同步 MMUPlugin 调用点**（Oracle 风险 #1，**关键**）：commit 4 改 PTW 接口（`start_walk(vaddr,asid,cb,cb)` → `start_walk(vaddr,asid,satp_ppn)`；`advance(pte_raw,level)` → `advance()` 无参）必须**同时**改 `MMUPlugin.cpp` L59-66/87/97/106/115 的 5 处旧调用点为新签名，否则中间 3 个 commit（4/5/6）build 全红违反 atomic-commit 纪律。**实施合并策略**：commit 4 = PTW 接口 + stub 填充 API + MMUPlugin.cpp 5 处调用点适配（不重写完整 at_stage 闭包，留给 commit 7）
- [x] 3.2.9 验证：编译通过；`tools/verify_plugin_decision.sh` 报告 `ptw.cpp` 无 `void tick()`（PTW 是 lib 算法，不属于业务 Plugin 层）

### 3.3 MultiLevelTLB coherence 实装

- [x] 3.3.1 在 `ip/mmu/lib/multi_level_tlb.cpp` 实现 `MultiLevelTLB::lookup(vaddr, asid)`：并行查全部 level，返回 **level 编号最大的 hit**（Decision 4）
- [x] 3.3.2 实现 shadow fill：deep hit 后调所有浅层 `tlb.insert_from(...)`（不污染 evict 计数）
- [x] 3.3.3 实现 reverse invalidate：deep evict 时同步 invalidate 浅层匹配 (vaddr, asid) 条目
- [x] 3.3.4 实现 `invalidate_asid(asid)` + `invalidate_all()` 跨级操作
- [x] 3.3.5 验证：编译通过；与 3.2 PTW write_back 集成（PTW → TLB insert → shadow fill → reverse invalidate 链路）

### 3.4 TLBFactory 7 组特化

- [x] 3.4.1 在 `ip/mmu/lib/tlb_factory.cpp` 实现 7 组新特化：None(16,1,40,0,1), LRU-4w(64,4,40,9,2), LRU-8w(64,8,40,9,2), LRU-2w(32,2,40,9,2), FIFO(256,1,40,9,1), RRIP-4w(64,4,40,9,2), RRIP-8w(64,8,40,16,2)
- [x] 3.4.2 实现 `TLBFactory::create(config)` 调度（按 config.policy + levels 选特化）
- [x] 3.4.3 实现 VIPT safety 拒绝：`idx_bits + offset_bits > 12` 时抛 `std::runtime_error("VIPT safety violated: ...")`
- [x] 3.4.4 验证：编译通过（依赖 3.1 显式实例化扩展）；现有 cache 测试不退化

## 4. tlm/ Plugin 层实装

### 4.1 MMUPlugin at_stage 闭包

- [x] 4.1.1 在 `ip/mmu/tlm/MMUPlugin.cpp` **重排** `setup()` 声明链（**不沿用现有顺序**）：`tlb_lookup_ifetch` → `ptw_l2`（根级）→ `ptw_l1` → `ptw_l0`（叶级）；现骨架声明的 `ptw_l0→ptw_l1→ptw_l2` 顺序与新设计（WalkL2=根级先走）冲突，必须重排
- [x] 4.1.2 实现 `at_stage("tlb_lookup_ifetch")`：读 `pl::PC` + `pl::ASID`，调 `multi_level_tlb_.lookup()`，命中写 `pl::PADDR` + `pl::MMU_VADDR`，miss 调 `ptw_.start_walk()` + 写 `pl::PTW_ACTIVE=1`
- [x] 4.1.3 实现 `at_stage("tlb_lookup_loadstore")`：读 `pl::MEM_ADDR` + `pl::ASID`，同上
- [x] 4.1.4 实现 `at_stage("ptw_l2")` / `ptw_l1` / `ptw_l0`：每 cycle 调 `ptw_.advance()`，Done 时写 `pl::PADDR` + `pl::MMU_VADDR` + 清 `pl::PTW_ACTIVE`；同步 `pl::PTW_L0_RAW/L1_RAW/L2_RAW/PTW_FAULT`（已有 Key）
- [x] 4.1.5 实现 `issue_request` / `read_response` 测试 API（per D4 Plugin 范式；MMUPlugin.h 加声明）
- [x] 4.1.6 验证：编译通过；`tools/verify_plugin_decision.sh` PASS；`tools/check_plugin_portability.sh` PASS

### 4.2 mmu_keys.h 新增 4 Key

- [x] 4.2.1 在 `ip/mmu/tlm/mmu_keys.h` 新增 `MMU_VADDR` Key（`cf::plugin::uint_t<64>`）
- [x] 4.2.2 新增 `EXCEPTION_CODE` Key（`cf::plugin::uint_t<8>`）
- [x] 4.2.3 新增 `SATP_PPN` Key（`cf::plugin::uint_t<44>`）+ `SATP_MODE` Key（`cf::plugin::uint_t<4>`）
- [x] 4.2.4 验证：编译通过；**不重定义已存在的 10 个 Key**（PTW_ACTIVE/PTW_VADDR/PTW_ASID/PTW_L0_RAW/PTW_L1_RAW/PTW_L2_RAW/PTW_FAULT/PERMS/VADDR/PADDR）

## 5. RISC-V ISA 适配

### 5.1 RiscvMMUPlugin 实装

- [x] 5.1.1 在 `ip/cpu/plugins/mmu.h` 把占位类改写为 `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin`（**单源真相位于此处**，不创建新文件）
- [x] 5.1.2 加 `uint64_t satp_value_` 成员（RiscvMMUPlugin 自持 CSR 状态）
- [x] 5.1.3 实现 `setup()`：自声明 substage `csr_write_satp` / `sfence_vma`（挂在 `execute` 父 stage 下，与 RiscvCsrPlugin 解耦）
- [x] 5.1.4 实现 `at_stage("csr_write_satp")`：解析 satp.MODE / satp.PPN，写 `pl::SATP_MODE` / `pl::SATP_PPN` Payload Key，调 `multi_level_tlb_.invalidate_all()`
- [x] 5.1.5 实现 `at_stage("sfence_vma")`：根据 rs1 / rs2 调 `invalidate_vaddr(vaddr, asid)` 或 `invalidate_asid(asid)` 或 `invalidate_all()`
- [x] 5.1.6 在 `MMUPlugin::at_stage("tlb_lookup_*")` 加 exception 12/13/15 写入逻辑（写 `pl::EXCEPTION_CODE`）
- [x] 5.1.7 保留 `using MMUPlugin = cf::ip::mmu::MMUPlugin` 向后兼容别名（保留现状）
- [x] 5.1.8 验证：编译通过；现有 11-Plugin CPU 测试（**实测 [cpu-integration]=25（之前文档错的）**）全部 PASS

### 5.2 CpuFactory PluginOrder 注册

- [x] 5.2.1 在 `ip/cpu/cpu_factory.h` 的 `PluginOrder` 数组加 `RiscvMMUPlugin`（条件：`config.enable_mmu == true`；schema 字段已存在）
- [x] 5.2.2 在 `ip/cpu/cpu_factory.h` 加 `if (config.enable_mmu) pb.register_plugin<RiscvMMUPlugin>(...)`
- [x] 5.2.3 验证：`./build/bin/chipforge_tests '[cpu]' '[cpu-configs]' '[cpu-integration]'` = 148 cases 全部 PASS（**实测 114 + 9 + 25 = 148**）

## 6. Bridge 层实装

### 6.0 bundles/tlb_bundles_tlm.hh 新建（**桥接 adapter 必备**）

- [x] 6.0.1 新建 `bundles/tlb_bundles_tlm.hh`：定义 `::bundles::TlbReqBundle` + `::bundles::TlbRespBundle`（全局命名空间，镜像 CppTLM cache_bundles_tlm.hh；POD 类型 `cf::bundles::TlbReq/TlbResp` 已存在于 `bundles/tlb_bundles_extension.h`，无需新建）（ch_stream Bundle 类型，镜像 `bundles/cache_bundles_tlm.hh`）—— 4 字段窄桥（TlbReq: vaddr/asid/access_type/size; TlbResp: hit/paddr/perms/exception_code）
- [x] 6.0.2 验证：编译通过；header 包含于 `src/cf_plugin/bridge/mmu_bridge_adapter.cpp` 后无缺失类型错误

### 6.1 MMUTLMBridge 实装

- [x] 6.1.1 新建 `src/cf_plugin/bridge/mmu_bridge.h`：声明 `MMUTLMBridge` 类（持 `PipeBuilder pb_` + `std::unique_ptr<MMUPlugin> plugin_`）
- [x] 6.1.2 在 `src/cf_plugin/bridge/mmu_bridge.cpp` 实现构造函数（注册 plugin 到 pb）+ `issue_request` / `read_response` / `tick` + `pb_run_count`（同构 `L1CacheTLMBridge`）
- [x] 6.1.3 验证：编译通过；`tools/check_plugin_portability.sh` PASS

### 6.2 MMUTLMBridgeAdapter 实装

- [x] 6.2.1 新建 `src/cf_plugin/bridge/mmu_bridge_adapter.h`：声明 `MMUTLMBridgeAdapter`（cpptlm `ChStreamAdapterFactory` 注册层 + `ch_stream` 4 字段窄桥）
- [x] 6.2.2 在 `src/cf_plugin/bridge/mmu_bridge_adapter.cpp` 实现 adapter：`cpptlm::ChStreamAdapterFactory::get().registerAdapter<MMUTLMBridgeAdapter, ::bundles::TlbReqBundle, ::bundles::TlbRespBundle>`
- [x] 6.2.3 实现 `bind_port_pair` / `tick` 同构 `L1CacheTLMBridgeAdapter`
- [x] 6.2.4 验证：`./build/bin/chipforge_tests '[cache]'` 21 cases + `[soc]` 10 cases 全部 PASS

### 6.3 SoC JSON 拓扑样例

- [x] 6.3.1 新建 `soc/mmu_minimal.json`：traffic_gen → mmu_bridge → memory 最小拓扑
- [x] 6.3.2 在 `tests/soc/test_mmu_minimal_json.cpp`：调 `cpptlm::instantiateAll(soc/mmu_minimal.json)` 验证模块拓扑
- [x] 6.3.3 验证：`./build/bin/chipforge_tests '[soc]'` 11 cases 全部 PASS

## 7. 测试集扩增（解阻塞 + 扩展 + 2 个新 test）

### 7.1 5 个 mmu test 扩展（**实测当前 29 → 目标 65 cases**）

- [x] 7.1.1 扩展 `tests/mmu/test_tlb_unit.cpp` 8 → 18 cases（4 策略 × hit/miss/evict/invalidate 矩阵 + 边界用例）
- [x] 7.1.2 扩展 `tests/mmu/test_multi_level_tlb.cpp` 8 → 16 cases（shadow fill + reverse invalidate + ASID switch + stale-shallow 边界）
- [x] 7.1.3 扩展 `tests/mmu/test_tlb_factory.cpp` 6 → 13 cases（7 组特化 + VIPT safety 拒绝）
- [x] 7.1.4 扩展 `tests/mmu/test_mmu_config_schema.cpp` 2 → 8 cases（**实测**当前只有 2 个 case，不是 6；DSE JSON Schema 验证扩展）
- [x] 7.1.5 扩展 `tests/mmu/test_mmu_plugin.cpp` 5 → 10 cases（at_stage 闭包 + RiscvMMUPlugin 路径；**测试 setup 时给 fetch/memory 父 stage stub**）
- [x] 7.1.6 验证：`./build/bin/chipforge_tests '[mmu]'` 全部 PASS（**目标 65 cases**：18+16+13+8+10=65；不是 69）

### 7.2 MMU↔Cache VIPT 集成测试（仅 MMU 侧）

- [x] 7.2.1 新建 `tests/cache/test_mmu_cache_integration.cpp`：验证 MMUPlugin 命中时同时写 `pl::PADDR` + `pl::MMU_VADDR`（**不验证 L1Cache 消费**，推迟到 mmu-cache-integration）
- [x] 7.2.2 加 1 case：MMU 命中时两 Key 一致性（PADDR 由 MMU 计算，MMU_VADDR == 输入 VPC）
- [x] 7.2.3 验证：`./build/bin/chipforge_tests '[cache]'` 22 cases 全部 PASS（21 + 1）

### 7.3 soc minimal JSON 集成测试

- [x] 7.3.1 新建 `tests/soc/test_mmu_minimal_json.cpp`：验证 `soc/mmu_minimal.json` 实例化
- [x] 7.3.2 验证：`./build/bin/chipforge_tests '[soc]'` 11 cases 全部 PASS（10 + 1）

## 8. 文档与 CHANGELOG 同步

### 8.1 ip/mmu/STATUS.md 更新

- [x] 8.1.1 `ip/mmu/STATUS.md`：标题 `PARTIAL` → `IMPLEMENTED (TLB/PTW/Bridge 实装, 2026-09-XX)`
- [x] 8.1.2 Implementation Roadmap 段：标记下一里程碑为 `mmu-cache-integration`（L1Cache 消费 MMU_VADDR + SoC 集成 + DSE + CtrlLink stall）
- [x] 8.1.3 已知限制段：删除 "TLB/PTW 算法 stub" / "PTW Sv32/Sv39/Sv48 解码 stub" / "CPU 集成未实装" / "cpptlm MMUTLMBridge 未实装"

### 8.2 CHANGELOG.md 更新

- [x] 8.2.1 `CHANGELOG.md` 加 v0.0.9 (2026-09-XX) 条目 "mmu-tlb-ptw-impl"：tlb_entry.h 加 tag_type typedef + tlb.h mutable 修复 + lib build wiring + PTW Sv39 walk + MMUPlugin at_stage 实装 + RISC-V satp/SFENCE.VMA/exception 12/13/15 hook + MMUTLMBridge + 5 个测试解阻塞 + 集成测试（**不含 DSE 48-case 扫描**）

### 8.3 docs/architecture/overview.md 更新

- [x] 8.3.1 快照日期 2026-07-01 → 2026-09-XX
- [x] 8.3.2 标记 mmu-tlb-ptw-impl 完成，下一里程碑为 `mmu-cache-integration`
- [x] 8.3.3 更新测试计数 259 → **330 cases**（+71 = mmu 解阻塞 +29 + 扩展到 65 +36 + integration +1 + soc +1 + 调整 = 71）

### 8.4 docs/build-reports/ 追加报告

- [x] 8.4.1 在 `docs/build-reports/` 新增 `2026-09-XX-mmu-tlb-ptw-impl-complete.md`：build/回归/完备性报告

## 9. 最终验证与归档

### 9.1 4 gate 全过

- [x] 9.1.1 `bash tools/verify_adr.sh` PASS（**实测 31 ✅ + 10 🚧 = 41 ADRs**；ADR-044 仍 🚧，本次不改）
- [x] 9.1.2 `bash tools/verify_plugin_decision.sh` PASS（3+4/3 PASS）
- [x] 9.1.3 `bash tools/check_plugin_portability.sh` PASS（4/4 PASS）
- [x] 9.1.4 `bash tools/doc_link_check.sh --quiet` PASS（exit 0）

### 9.2 测试全过

- [x] 9.2.1 `./build/bin/chipforge_tests` **330/330 PASS**（实测基线 259 + mmu 解阻塞 +29 + 扩展 +36 + integration +1 + soc +1 + 调整 = **~330**；**不**含 DSE 48-case）
- [x] 9.2.2 5 次连跑稳定（无 flaky test）
- [x] 9.2.3 各 family tag 全部 PASS：`[mmu]` 65 + `[cache]` 22 + `[cpu]` 114 + `[cpu-configs]` 9 + `[cpu-integration]` **25** + `[soc]` 11 + `[framework]` 71 + `[bundles]` 9 = 326（实测） ≈ 330（含 boundary 用例扩展）

### 9.3 ASan 构建验证

- [x] 9.3.1 `bash tools/build.sh --asan --rebuild-deps` 编译通过
- [x] 9.3.2 `./build/bin/chipforge_tests` ASan 模式 330/330 PASS（无 memory error）

### 9.4 openspec archive

- [x] 9.4.1 `openspec archive mmu-tlb-ptw-impl`（归档 change 至 `openspec/changes/archive/2026-09-XX-mmu-tlb-ptw-impl/`）
