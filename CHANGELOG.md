# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## v0.0.7 (2026-06-29) - mmu-ip-skeleton

> **OpenSpec change**: `mmu-ip-skeleton` (详见 `openspec/changes/mmu-ip-skeleton/`)
> **目的**: 建立 `ip/mmu/` IP 骨架（目录/STATUS/Plugin/Bundle/Config schema），落地 lib/tlm 双层切分，TLB/PTW 算法推迟到 `mmu-tlb-ptw-impl`

### Added (IP 骨架)
- `ip/mmu/README.md` + `STATUS.md` (PARTIAL 骨架阶段标记)
- `ip/mmu/docs/{README,architecture,configuration,integration}.md` (4 文档)
- `ip/mmu/rtl/.gitkeep` + `ip/mmu/test/.gitkeep` (Phase 5+ 沿用)
- `tests/mmu/` 目录 + 5 个测试文件 + `CMakeLists.txt`

### Added (lib/ 纯 C++ 算法层)
- `ip/mmu/lib/tlb_entry.h` — 模板化 `TLBEntry<TAG_BITS, ASID_BITS>` (valid/tag/pfn/asid/perms/global)
- `ip/mmu/lib/tlb_lookup.h` — `TLBLookup` 5 字段 (hit/paddr/perms/fault/fault_code)
- `ip/mmu/lib/tlb_base.h` — `TLBBase` 抽象基类 (10 纯虚方法)
- `ip/mmu/lib/tlb.h` — 模板化 `TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` (std::array 存储)
- `ip/mmu/lib/tlb_factory.h/.cpp` — `TLBFactory::create()` 按 JSON 选模板特化 (8 种支持组合)
- `ip/mmu/lib/multi_level_tlb.h/.cpp` — `MultiLevelTLB` 编排器 (shadow fill + 反向失效)
- `ip/mmu/lib/ptw.h/.cpp` — `PTW` Page Table Walker 接口 (回调驱动, 0 tick)

### Added (policies/ 替换策略)
- `ip/mmu/policies/tlb_replacement_policy.h` — 模板化抽象基类
- `ip/mmu/policies/{no_replacement,fifo,lru,rrip}_policy.h` — 4 策略实装
- `ip/mmu/policies/tlb_replacement_policy.cpp` — `create(name)` 工厂

### Added (tlm/ 声明式 Plugin 层)
- `ip/mmu/tlm/mmu_keys.h` — 10 个 `Payload<T>` Key 集合 (VADDR/PADDR/PERMS/PTW_ACTIVE/PTW_VADDR/PTW_ASID/PTW_L0/L1/L2_RAW/PTW_FAULT)
- `ip/mmu/tlm/MMUPlugin.h/.cpp` — `MMUPlugin : public cf::plugin::PluginBase`, 用 `at_stage()` 注册 5 个 logical stage 闭包

### Added (Config + RISC-V 适配)
- `ip/mmu/configs/params_schema.json` — JSON Schema draft-07 (含 topology/asid_bits/sv_mode/supported_page_sizes/ptw_max_inflight/shadow_fill_from_next/levels)
- `bundles/tlb_bundles_extension.h` — TlbReq / TlbResp Bundle (12 字节 POD)
- `ip/cpu/plugins/mmu.h` 重构为 `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin` + `using MMUPlugin = RiscvMMUPlugin` 向后兼容别名

### Added (IP Catalog)
- `ip/README.md` — STATUS 表 + IP 模块列表加 `mmu` 行

### Design Decisions (9 Decisions in design.md)
1. TLB 模板化 + 抽象基类 + 工厂三件套
2. MultiLevelTLB 编排器 (coherence 协议集中)
3. PTW 用 substage 不用 tick() (D4 强制)
4. HDL 友好性通过 static_assert 编译期检查
5. 配置 schema 用 topology 字段预留 split_id
6. ASID bits 0-16 全支持
7. **lib/ vs tlm/ 职责切分** (强制 Plugin 范式合规, lib/ 0 依赖 Plugin 框架)
8. PTW 用 substage 不用 tick() 详细示例
9. HDL 友好约束的边界 (lib/ 严格, tlm/ 宽松)

### Notes
- **D4 Plugin 范式合规**: 所有业务逻辑用 `at_stage()` 声明, 0 业务 tick() (编译期 `PluginBase::tick() = delete`)
- **lib/ 0 Plugin 依赖**: `grep -rn "cf/plugin/plugin_base.h\|cf/plugin/pipe_builder.h\|cf/plugin/payload.h" ip/mmu/lib/` MUST 0 匹配
- **Phase 0 logical stage 模型**: `pb.run()` 单 cycle 遍历所有闭包, PTW 3 级 sub-pipe 是逻辑拆分, 为 Phase 6 cycle-scheduling 预留
- **测试位置**: 遵守 `test-location-discipline` spec, 全部在 `tests/mmu/`, 不在 `ip/mmu/test/`

### Pending (下一阶段入口)

> `mmu-tlb-ptw-impl` change —— TLB lookup/insert 算法实装 + PageTableWalker Sv32/Sv39/Sv48 解码 + CtrlLink halt_when PTW stall + cpptlm MMUTLMBridge + RISC-V 特定 hook (satp 拦截 / SFENCE.VMA / exception 12/13/15)

## v0.0.9 (2026-09-13) - mmu-tlb-ptw-impl


> **目的**: 实现 TLB lookup/insert 算法 + PTW Sv39 三级 walk + MultiLevelTLB coherence + MMUPlugin at_stage 闭包 + RISC-V satp/SFENCE.VMA/exception 12/13/15 hook + cpptlm MMUTLMBridge. 解锁 ADR-044 VIPT 数据流真实链路 (MMUPlugin 同时输出 pl::PADDR + pl::MMU_VADDR).

## v0.2.0 (2026-09-13) - cpu-mmu-integration

> **目的**: 把 mmu-cache-integration v0.1.0 实装的 RiscVMMUPlugin 真正接到 CPU pipeline. 条件注册 + 3 个 RiscV hook substage (csr_write_satp/sfence_vma/mmu_exit) + exception 12/13/15 传播到 CPU. 落地 Oracle Tier 1 #1 推荐的 M5 critical path 起点.

### Added

- **`ip/cpu/cpu_factory.h`**（修改）：`build_cpu()` 在 `config.enable_mmu=true` 时条件注册 `cf::cpu::plugins::RiscvMMUPlugin`; 映射 `config.mmu_mode` ("sv32"/"sv39"/"sv48") 到 `cf::ip::mmu::SvMode` 枚举; 默认 TLB 几何 2-level 8/8 + LRU + ptw_max_inflight=2（与 SoC JSON 一致）; 加 `pb->build()` 调用触发所有 plugin 的 setup()/build() 闭包（之前 baseline 不需要 plugin 闭包，加 MMU 后必须 build()）
- **`ip/cpu/tlm/cpu_keys.h`**（NEW）：4 个 Payload Key `SAT`/`SFENCE_VADDR`/`SFENCE_ASID`/`CPU_EXCEPTION_CODE`（CPU→MMU IPC Key 集合）；放 `ip/cpu/` 而非 `ip/mmu/` 避免反向依赖；Key identity 是全局 static 指针身份
- **`ip/cpu/plugins/mmu.h`**（修改）：override `setup()` + `build()` 声明 3 个 RiscV hook substage
- **`ip/cpu/plugins/mmu.cpp`**（修改）：`setup()` 声明 3 substage（csr_write_satp/sfence_vma 挂 execute，mmu_exit 挂 memory）；`build()` 注册 3 个 at_stage 闭包，路由到 `csr_write_satp()`/`sfence_vma()` hook（4-way RISC-V Spec §6.2 dispatch）/exception 传播；D4 合规：用 if/else 全分支（无早返）
- **`tests/cpu/integration/test_{3,5,7,10}stage_riscv.cpp`**（修改）：各加 `EnableMMU{3,5,7,10}Stage*` 测试 + `EnableMMUDisabledBitIdenticalBaseline`（5-stage enable_mmu=false 字节级 baseline）
- **`tests/cpu/test_cpu_riscv_mmu_hooks.cpp`**（NEW）：3 个 RiscV hook 集成测试（`CSRWriteSatpRoutesToRiscVMMUPlugin` + `SFENCEVMARoutesToRiscVMMUPlugin` + `MMUExceptionPropagatesToCPU`），镜像 test_l1_cache_plugin_unit.cpp 隔离测试原则
- **`tests/cpu/test_cpu_factory.cpp`**（修改）：`plugins.size() == 11` → `12`（Oracle B2 修复：RiscVMMUPlugin 注册后 +1）
- **`ip/cpu/README.md`**（修改）：新增 "RiscV MMU Integration (mmu-cache-integration v0.2.0)" 段说明 3 个 hook substage 契约
- **`ip/cpu/configs/cpu_params_schema.json`**（修改）：`enable_mmu` description 指向 `cf::cpu::plugins::RiscvMMUPlugin`
- **`ip/mmu/STATUS.md`**（修改）：`INTEGRATED + CPU PIPELINE`

### Fixed
- Oracle B2 breaking（commit 1 引入）：`cpu_factory` 默认 `enable_mmu=true` 导致现有 5+ 个 cpu 测试的 `node_count`/`stage_count`/`plugins.size` assertions 硬错误。**修复**：commit 1.5-1.7 tasks 同步更新 4 个测试文件（3-stage/5-stage/7-stage/test_cpu_factory）到新数字（详细注释说明来源）
- D4 早返违规（commit 2 引入）：3 个 RiscV hook 闭包用 `if (!node) return;` 早返。**修复**：改为 `if (node && has(key)) { ... } else { (void)0; }` 全分支（D4 HDL 1:1 友好）

### Verified
- `./build/bin/chipforge_tests` → **314/314 PASS**（was 291 baseline; +23 cases = 15 mmu-cache-integration + 8 cpu-mmu-integration）
- `[cpu-integration]` 25 → **33/33 PASS**（+8: 5 stage + 3 RiscV hook baseline）
- `[RiscV]` 4 → **7/7 PASS**（+3 new hook integration tests）
- `[cpu]` 114/114 PASS（含 B2 修复）
- 4 architecture gates all PASS（verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check）
- 5 次连跑稳定性 PASS（无 flaky test）

### Pending (下一阶段入口)

> `plugin-framework-stall` change —— `PipeBuilder::run()` 消费 `CtrlLink::should_halt()` 替代 RETRY 数据依赖（Oracle Tier 1 #2）

## v0.1.0 (2026-09-13) - mmu-cache-integration

> **目的**: 完成 ADR-044 §3.2 VIPT 数据流方案 A 双端契约 — L1CachePlugin 消费 pl::MMU_VADDR 做 VIPT 索引 + PIPT fallback 兼容 + MMUTLMBridgeAdapter cpptlm 集成 + soc/mmu_minimal.json 全链 + PTW completion dual-write bug 修补.

### Added
- **`ip/mmu/tlm/MMUPlugin.cpp`** (commit 0): PTW completion `WalkCallback`/`FaultCallback` 闭包加 `[node, vaddr]` capture, 同时写 `pl::PADDR` + `pl::MMU_VADDR` (修复 stale vaddr 风险, mmu-tlb-ptw-impl 遗留 defer)
- **`tests/mmu/test_ptw_unit.cpp`** (commit 0): 3 PTW 单测 — 闭包 capture vaddr 模式 + V=0 fault 12 + reserved encoding fault 15
- **`ip/cpu/plugins/mmu.cpp`** (commit 6): `RiscvMMUPlugin::csr_write_satp` + `sfence_vma` 实装 (mmu-tlb-ptw-impl 遗留 defer)
- **`ip/mmu/lib/tlb_base.h` / `tlb.h` / `multi_level_tlb.{h,cpp}`** (commit 6): `invalidate_vaddr_any_asid` 新 API (RISC-V SFENCE.VMA rs1!=x0, rs2=x0 跨 ASID 失效语义)
- **`ip/mmu/tlm/MMUPlugin.h`** (commit 6): `multi_tlb()` public getter + `invalidate_vaddr_any_asid` facade
- **`ip/cache/tlm/cache_keys.h`** (commit 1): 新增 `g_vaddr` VIPT Key + `vipt_fallback` Knob (Auto/PaddrOnly/Panic)
- **`ip/cache/tlm/L1CachePlugin.cpp`** (commit 2): lookup 闭包用 `n->has(MMU_VADDR)` 三元选择 idx_src/tag_src (VIPT 索引 + PIPT fallback 兼容 baseline)
- **`src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`** (commit 4): `MMUTLMBridgeAdapter` 独立 cpptlm `ChStreamModuleBase` 子类 + `ChStreamAdapterFactory` 注册
- **`soc/mmu_minimal.json`** (commit 5): 4 modules + 3 connections 全链 (`tg → mmu → l1 → mem`)
- **`tests/mmu/test_ptw_unit.cpp`** (commit 6): 3 边界 case — MMU_VADDR 一致性 (success + 2 fault paths)
- **`tests/mmu/test_mmu_plugin.cpp`** (commit 6): 4 RISC-V test — SFENCE.VMA (单 vaddr + 全清) + csr_write_satp + ASID switch + 多 ASID 失效
- **`tests/cache/test_mmu_cache_integration.cpp`** (commit 3): 3 integration test — VIPT 命中 + PIPT fallback + has() 对称性
- **`tests/soc/test_mmu_minimal_json.cpp`** (commit 5): 4 结构验证 — top-level + modules + connections + params
- **`ip/mmu/STATUS.md`**: `IMPLEMENTED` → `INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链)`
- **`openspec/changes/mmu-cache-integration/`**: 完整 OpenSpec lifecycle artifacts (proposal + design + tasks + 6 ADDED + 2 MODIFIED specs)

### Fixed
- mmu-tlb-ptw-impl 遗留 PTW completion stale vaddr bug (commit 0): WalkCallback 闭包不写 pl::MMU_VADDR, 多 ASID 场景导致 cache 索引错误
- Oracle round-2 发现 sfence_vma rs1!=0, rs2=0 分支不跨 ASID 失效, 加 invalidate_vaddr_any_asid API 修复
- header 注释 `rs1 (vaddr, -1=all)` 修正为 `0=all (x0)` 约定

### Verified
- `./build/bin/chipforge_tests` → 306/306 PASS (was 291 baseline; +15: 7 mmu + 3 cache + 4 soc + 1 集成微调)
- `[mmu]` 32 → **40** PASS
- `[RiscV]` 0 → **4** PASS (新 tag)
- `[cache][MMUCacheIntegration]` 3/3 PASS (新 tag)
- `[soc][MMUMinimalJson]` 4/4 PASS (新 tag)
- 4 architecture gates all PASS (verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check)
- 5 次连跑稳定性 PASS (无 flaky test)

### Pending (下一阶段入口)

> `cache-dse-sweep` change —— 完整 12-case Pareto DSE 配置扫描 (Phase 1.5 L1Cache 升 4-way 触发)

## v0.0.9 (2026-09-13) - mmu-tlb-ptw-impl
- **`ip/mmu/lib/tlb.h`**: `mutable` 关键字修复 `hits_`/`misses_` const-correctness (L227-229)
- **`ip/mmu/lib/tlb_entry.h`**: 新增 `using tag_type = cf::plugin::uint_t<TAG_BITS>;` (解决 8 处 typename Entry::tag_type 编译错误)
- **`ip/mmu/lib/ptw.{h,cpp}`**: Sv39 三级 walk state machine 实装 + `stub_write_pte`/`stub_read_pte` 测试 API + reserved encoding + V=0 fault handling
- **`ip/mmu/lib/multi_level_tlb.{h,cpp}`**: 并行查全部 level + 返回最深 hit (deepest = largest level index) + shadow fill + reverse invalidate
- **`ip/mmu/lib/tlb_factory.cpp`**: 11 组特化 (None 16,1 / LRU 64,4 / LRU 64,8 / LRU 32,2 / FIFO 256,1 / RRIP 64,4 / RRIP 64,8 / etc.) + VIPT safety check (idx_bits + offset_bits > 12 throw runtime_error)
- **`ip/mmu/policies/tlb_replacement_policy.cpp`**: 显式实例化 8→12 组 (覆盖新增 TLBFactory 特化所需组合)
- **`ip/mmu/tlm/MMUPlugin.{h,cpp}`**: at_stage 闭包实装 (tlb_lookup_ifetch + tlb_lookup_loadstore + ptw_l0/l1/l2) + 5 substage declare + set_name/declare_substage 修复
- **`ip/mmu/tlm/mmu_keys.h`**: 新增 4 Key (MMU_VADDR / EXCEPTION_CODE / SATP_PPN / SATP_MODE)
- **`ip/cpu/plugins/mmu.h`**: RiscvMMUPlugin 实装 (csr_write_satp + sfence_vma + exception 12/13/15 + satp_value accessors)
- **`bundles/tlb_bundles_tlm.hh`**: ch_stream Bundle 类型 (TlbReqBundle + TlbRespBundle, 4 字段窄桥, 全局 bundles:: 命名空间)
- **`src/cf_plugin/bridge/mmu_bridge.{h,cpp}`**: MMUTLMBridge 核心 (mirror L1CacheTLMBridge, issue_request/read_response API + tick() 末尾 pb.run())
- **`tests/CMakeLists.txt`**: mmu lib/policies/tlm 5 个 .cpp 加入 MMU_IMPL_SOURCES, 5 个 mmu tests 解阻塞

### Fixed
- `ip/mmu/lib/tlb.h`: `mutable` 关键字修复 `lookup()` const 方法修改 `hits_`/`misses_` 编译错误
- `ip/mmu/lib/tlb.h`: `name_` 类型 const char* → std::string (避免 dangling pointer, test `L0`/`L1` 字符串显示乱码已修复)
- `ip/mmu/lib/tlb_entry.h`: 加 tag_type typedef (8 处编译错误)
- `tests/mmu/test_tlb_factory.cpp`: 3 个测试用 VIPT-unsafe 配置 (32/4, 64/4, 64/16) 改为 8/8 全关联 VIPT-safe
- `tests/mmu/test_mmu_plugin.cpp`: BareMode 4/1 → 8/8; L1 level 配置 64/4 → 8/8 (VIPT safe)
- `tests/mmu/test_multi_level_tlb.cpp`: deepest-hit 语义下 L0 hit_count 期望从 1 → 0 (commit 6 改了语义)

### Verified
- 288/288 chipforge_tests PASS (was 259 baseline; +29 mmu test cases)
- 5 consecutive stable runs
- 4 architecture gates all PASS (verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check)

### Pending (下一阶段入口)

> `mmu-cache-integration` change —— L1CachePlugin 消费 pl::MMU_VADDR (VIPT index) + SoC 集成测试 + DSE 配置扫描 + CtrlLink stall 机制


## v0.0.8 (2026-07-02) - l1-cache-vipt-coherence

> **目的**: ADR-044 落地 — L1 Cache↔MMU 耦合策略锁定 VIPT (虚地址索引 + 物理地址 tag), 反别名安全边界定义。准备 `mmu-tlb-ptw-impl` 实施前置条件。

### Added (L1Cache VIPT 设计)
- **ADR-044**: L1 Cache↔MMU VIPT 锁定 + 反别名安全边界 ([`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`](ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md))
  - L1 ICache/DCache 统一 VIPT, L2 PIPT
  - VIPT 安全条件: `idx_bits + offset_bits ≤ 12` (4KB page)
  - Phase 1.5 升级路径: 16KB 1-way → 16KB 4-way (64 sets × 4-way × 64B)
  - VIPT 数据流方案 A: MMUPlugin 同时输出 `pl::PADDR` + `pl::MMU_VADDR`
- `ip/cache/docs/architecture.md` (L1Cache 微架构: 地址映射 + VIPT 安全 + DSE 边界)
- `ip/cache/docs/integration.md` (L1Cache 集成契约: CPU/MMU 接口 + SoC JSON 样例)
- `ip/cache/configs/params_schema.json` 加 `associativity` 字段 (enum [1,2,4,8], default 4)
- `ip/cache/tlm/L1CachePlugin.h` 加 VIPT 编译期 `static_assert(kIdxBits + kOffsetBits <= 12)`

### Cross-document sync
- `ip/cache/README.md` §6 DSE 表加 VIPT 安全约束警告 + §8 引用新 docs
- `ip/mmu/docs/integration.md` §1.1 加 VIPT 数据流引用
- `ip/mmu/STATUS.md` Implementation Roadmap 加 ADR-044 依赖
- `docs/architecture/adr.md` 主表 ADR-044 注册 (🚧 Phase 1 提案)

### ADR status
- ADR-044: 🚧 → 🚧 (保持 Phase 1 提案; Phase 1.5 L1Cache 升级时升级到 ✅)

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