## Why

`mmu-ip-skeleton`（archived 2026-06-29）落地了 `ip/mmu/` 骨架（913 LOC lib + 258 LOC tlm），但 TLB/PTW 算法实装尚未完成：`ip/mmu/lib/tlb.h` 的 `lookup()` 标记为 `const` 却修改 `hits_`/`misses_` 统计字段（编译错误），`TLBEntry` 不含 `tag_type` 类型别名（引用 `typename Entry::tag_type` 全部编译失败）。这导致 `tests/CMakeLists.txt` 的 5 行 `list(REMOVE_ITEM mmu/test_*.cpp)` 必须存在——任何尝试解阻塞都会立即编译失败。`ip/mmu/lib/` 也**无任何 .cpp 被编进 build**（根 CMakeLists.txt 没有 add_subdirectory(ip/mmu/)，测试 CMakeLists.txt 缺少 `MMU_IMPL_SOURCES`）。

ADR-044（`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`，cb85fea）锁定 **MMUPlugin 必须输出 `pl::PADDR` + `pl::MMU_VADDR`** 供 L1CachePlugin VIPT 索引，但 `MMUPlugin::at_stage` 是空 stub，VIPT 数据流尚无消费者。

本 change 落地 **tlb.h 编译错误修复 + 真实 mmu build wiring + PTW Sv39 walk 实装 + MMUPlugin at_stage 闭包 + RISC-V 适配 + cpptlm Bridge + 测试集解阻塞**，**解锁 ADR-044 VIPT 数据流的真实链路**（之前 L1CachePlugin 的 VIPT 索引只能消费全 paddr）。`mmu-tlb-ptw-impl` 是项目下一里程碑（`ip/mmu/STATUS.md` + AGENTS.md "当前可启动" 段均明确指向），是 `mmu-ip-skeleton` 的下游强制消费者。

## What Changes

### lib/ 算法层（修复 + 实装，约 700 LOC 增量）

- **`ip/mmu/lib/tlb.h`**（修改，**编译错误修复**）：把 `hits_`/`misses_`/`evicts_` 三个统计字段声明为 `mutable`，使 `const` 的 `lookup()` 可合法修改；保留所有现有方法签名
- **`ip/mmu/lib/tlb.cpp`**（**不新增**——tlb.h 是 header-only template，方法已内联实现）：tasks 不再要求新建 .cpp
- **`ip/mmu/policies/tlb_replacement_policy.cpp`**（扩展显式实例化）：现有 8 组 `<8,1>/<16,2>/<32,4>/<64,4>/<128,4>/<256,8>/<8,8>/<16,16>` 扩展至 12 组（含新增 `<16,1>/<32,2>/<64,8>/<256,1>`，覆盖 TLBFactory 新增的 7 组特化）
- **`ip/mmu/lib/multi_level_tlb.cpp`**（实装）：现有 stub 替换为完整多级查找+shadow fill+反向 invalidate（并行查全部 level，返回**最深**命中）；Coherence 协议实装
- **`ip/mmu/lib/ptw.cpp`**（实装）：PTW Sv39 三级 walk 状态机（`started` → `walk_l2` → `walk_l1` → `walk_l0` → `check_perms` → `write_back` → `done`），PTW 持**成员状态**（`current_pte_l2/l1/l0` + `pending_walk_`）而非 Payload Key（**保持 lib/ 纯 C++**，Payload 仅在 tlm/ 层使用）
- **`ip/mmu/lib/ptw.h`**（修改）：加 `std::array<PTE, 4096> pte_stub_memory_`（**4096 entry = 32KB**，索引 `pte_stub_memory_[(ppn >> 12) & 0xFFF]`）；命名统一为 `PTE`（**不是** `Pte`，与代码现状一致）

### tlm/ Plugin 层实装（约 500 LOC 增量）

- **`ip/mmu/tlm/MMUPlugin.cpp`**（实装）：
  - `setup()`：用 `declare_substage()` 声明 `tlb_lookup_ifetch` + `tlb_lookup_loadstore` + `ptw_l2` + `ptw_l1` + `ptw_l0`（**沿用骨架已有的 5 个 substage 声明**，不重新发明 `ptw_complete`）
  - `build()`：`at_stage("tlb_lookup_ifetch")` 注册 vPC→paddr；`at_stage("tlb_lookup_loadstore")` 注册 vaddr→paddr；PTW walk 通过 `at_stage("ptw_l0")`/`ptw_l1`/`ptw_l2` 推进（**链式 substage**，每 cycle 走一级）
  - 命中时同时写 `pl::PADDR` + `pl::MMU_VADDR`（**ADR-044 §3.2 数据流契约**）；PTW miss 期间**只写 `pl::PTW_ACTIVE=1`**，PADDR/MMU_VADDR 在 walk 完成（`ptw_l0` 末）才写
- **`ip/mmu/tlm/MMUPlugin.h`**（修改）：加 `issue_request` / `read_response` 测试 API（per D4 Plugin 范式；现状没有）
- **`ip/mmu/tlm/mmu_keys.h`**（修改）：**当前已有 10 个 Key**，新增 4 个 Key（`MMU_VADDR` / `EXCEPTION_CODE` / `SATP_PPN` / `SATP_MODE`），不重定义已存在的 Key
- **`ip/mmu/tlm/RiscvMMUPlugin.{h,cpp}`**（**单源真相位于 `ip/cpu/plugins/mmu.h`** —— proposal 此处有偏差）：
  - `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin`（继承基类，**不创建新文件**）
  - `at_stage("csr_write_satp")`：RiscvMMUPlugin 自持 CSR 状态（持有 `satp_value_` 成员），解析 satp.MODE / satp.PPN，写 `pl::SATP_MODE` / `pl::SATP_PPN` Payload Key，调 `multi_level_tlb_.invalidate_all()`
  - `at_stage("sfence_vma")`：根据 rs1 / rs2 调 `invalidate_vaddr(vaddr, asid)` 或 `invalidate_asid(asid)` 或 `invalidate_all()`
  - exception 12/13/15 在 MMUPlugin 命中失败路径写 `pl::EXCEPTION_CODE`
  - **Stage 挂载**：CpuFactory 注册时 RiscvMMUPlugin 的 setup 把自己声明的 substage（如 `csr_write_satp`）挂在 `execute` 父 stage 下；与现有 11-Plugin 套件互不干扰（standalone 测试需要给父 stage stub）
- **`CpuFactory` 注册**：`ip/cpu/cpu_factory.h` 的 `PluginOrder` 数组加 `RiscvMMUPlugin`（条件：`config.enable_mmu == true`；**`cpu_factory.cpp` 不存在**，工厂是 header-only）；`enable_mmu` 字段已在 `cpu_params_schema.json` + `cpu_default.json` 存在（默认 `true`），不需要新增 schema 字段，只需加 schema 注释说明 enable_mmu 控制 MMU 启用
- **CtrlLink stall**：**本 change 不实装** CtrlLink::halt_when——实测 API 是 `halt_when(std::function<bool()>)`，全仓库无任何代码轮询 `should_halt()`。PTW stall 推迟到后续 `mmu-cache-integration` 阶段，那时 CtrlLink 框架已具备轮询机制

### Bridge 层实装（约 300 LOC 增量）

- **`src/cf_plugin/bridge/mmu_bridge.{h,cpp}`**（新增）：`MMUTLMBridge` 同构 `L1CacheTLMBridge`——持 `PipeBuilder pb_` + `std::unique_ptr<MMUPlugin> plugin_`，`tick()` 末尾调 `pb_.run()`（D1' 契约）
- **`src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`**（新增）：`MMUTLMBridgeAdapter` —— cpptlm `ModuleFactory` 注册 + `ch_stream` 4 字段窄桥（**注**：用 `cpptlm::ChStreamAdapterFactory` 而非 `StreamAdapterFactory`，后者是错的命名）

### 构建接线（约 30 LOC）

- **`ip/mmu/CMakeLists.txt`**（**新增**，缺失）：定义 `cf_plugin_mmu_lib` static library，编译 `ip/mmu/lib/*.cpp` + `ip/mmu/tlm/*.cpp` + `ip/mmu/policies/*.cpp`
- **根 `CMakeLists.txt`**（修改）：加 `add_subdirectory(ip/mmu)`，让 mmu lib 被 build 系统发现
- **`tests/CMakeLists.txt`**（修改）：加 `MMU_IMPL_SOURCES` 变量；删除 `list(REMOVE_ITEM mmu/test_*.cpp)` 5 行 + 上方注释行

### 测试集解阻塞（约 600 LOC 增量）

- **`tests/CMakeLists.txt`**（修改）：删除 5 行 `list(REMOVE_ITEM)` + 加 `MMU_IMPL_SOURCES`
- **5 个 `tests/mmu/test_*.cpp`**（扩展）：从 stub → 完整测试（**实测当前 case 数：test_tlb_unit 8 + test_multi_level_tlb 8 + test_tlb_factory 6 + test_mmu_config_schema 6 + test_mmu_plugin 5 = 33 cases**）
  - `test_tlb_unit.cpp`：8 → 18 cases（4 策略 × hit/miss/evict/invalidate 矩阵）
  - `test_multi_level_tlb.cpp`：8 → 16 cases（shadow fill + reverse invalidate + ASID switch）
  - `test_tlb_factory.cpp`：6 → 13 cases（7 组特化 + VIPT safety 拒绝）
  - `test_mmu_config_schema.cpp`：6 → 12 cases（DSE JSON Schema 验证）
  - `test_mmu_plugin.cpp`：5 → 10 cases（**先给父 stage stub** —— setup 时 declare_substage 需要父 stage 存在；要么改用 `pb.declare_substage` 无父参数版本，要么 standalone 测试自己 register fetch/memory topology）
- **`tests/cache/test_mmu_cache_integration.cpp`**（新增，~150 LOC）：**收缩范围**为"MMU 双 Key 输出与 ADR-044 §3.2 契约一致性验证"，**不**验证 L1Cache 消费 MMU_VADDR（L1Cache 修改属于 `mmu-cache-integration` 后续 change，与 ADR-044 §6 Phase 1.5 路线图对齐）
- **`tests/soc/test_mmu_minimal_json.cpp`**（新增）：验证 `soc/mmu_minimal.json` 实例化
- **DSE 48-case sweep 推迟**：本 change 不包含 DSE 笛卡尔积扫描（spec 已移除 `mmu-dse-config-sweep` capability）；DSE 推迟到 `mmu-cache-integration` change

### 文档/ADR 同步

- **`ip/mmu/STATUS.md`**（修改）：`PARTIAL` → `IMPLEMENTED`；Implementation Roadmap 段更新；删除已知限制段中已实装项
- **`CHANGELOG.md`**（修改）：加 v0.0.9 (2026-09-XX) 条目（"MMU TLB 编译修复 + lib build wiring + PTW Sv39 walk + MMUPlugin at_stage 实装 + RISC-V satp/SFENCE.VMA/exception 12/13/15 hook + cpptlm MMUTLMBridge + 5 个测试解阻塞 + ADR-044 VIPT 数据流双 Key 输出"）
- **`docs/architecture/overview.md`**（修改）：快照日期 → 2026-09-XX；mmu-tlb-ptw-impl 完成标记；下一里程碑 → `mmu-cache-integration`（L1Cache 消费 MMU_VADDR + SoC 集成 + DSE）

### 不修改

- `L1CachePlugin` / `L1CacheTLMBridge` / `cf_plugin` 框架层 / CppTLM / CppHDL
- ADR-044 本身（VIPT 锁定 + 数据流契约已稳定）
- `bundles/mem_bundles.h` 现有 Bundle 签名（仅追加 `cf::bundles::TlbReq` / `TlbResp`）
- `ip/cpu/plugins/mmu.h` 之外的其他 CPU Plugin（hazard/branch_predictor/reg_file 等保持零干扰）
- CtrlLink stall 机制（推迟到 mmu-cache-integration）
- Sv32/Sv48 解码（推迟到 mmu-sv32-sv48-ext）
- `ip/mmu/rtl/`（Phase 5+ 沿用）
- L1Cache 消费 MMU_VADDR（推迟到 mmu-cache-integration）
- DSE 配置扫描（推迟到 mmu-cache-integration）
- `tools/verify_hdl_friendly.sh` —— **本 change 不新建此脚本**；改用现有的 `tools/check_plugin_portability.sh`（已在 CI gate）替代 HDL 友好性验证

## Capabilities

### New Capabilities

- `mmu-tlb-lookup-insert`: TLB 单级 lookup/insert/invalidate 算法（**已在 `tlb.h` 实现**，本 change 修复 const-correctness 编译错误），支持 4 种替换策略（None/FIFO/LRU/RRIP）
- `mmu-ptw-sv39-walk`: PageTableWalker Sv39 三级 walk 状态机实装（**PTW 持成员状态**，lib 层不依赖 Payload；tlm 层 MMUPlugin 同步到 pl::PTW_* Key）
- `mmu-multi-level-coherence`: MultiLevelTLB 多级 coherence 协议——并行查全部 level、返回**最深**命中、shadow fill、反向 invalidate、ASID 全级 invalidate
- `mmu-riscv-isa-adapter`: RISC-V 适配器（`RiscvMMUPlugin` 继承 `MMUPlugin`，单源真相位于 `ip/cpu/plugins/mmu.h`），自持 satp CSR 状态，SFENCE.VMA hook 路由到 `invalidate_vaddr/asid/all`，exception 12/13/15 写 `pl::EXCEPTION_CODE`
- `mmu-cpptlm-bridge`: `MMUTLMBridge` + `MMUTLMBridgeAdapter`（同构 `L1CacheTLMBridge`），cpptlm `ChStreamAdapterFactory` 注册
- `mmu-vipt-data-flow`: MMU→L1Cache VIPT 数据流契约（**收缩**到 MMU 侧）—— `MMUPlugin` 命中时同时写 `pl::PADDR` + `pl::MMU_VADDR`；L1Cache 侧消费移后续 change

### Modified Capabilities

（无现有 spec 的 REQUIREMENTS 被本 change 修改；`mmu-ip-skeleton` spec 仅约束骨架结构。）

## Impact

- **新增文件**:
  - `ip/mmu/CMakeLists.txt`（~20 LOC）
  - `src/cf_plugin/bridge/mmu_bridge.{h,cpp}`（~150 LOC）
  - `src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`（~150 LOC）
  - `soc/mmu_minimal.json`（~30 行）
  - `tests/cache/test_mmu_cache_integration.cpp`（~150 LOC）
  - `tests/soc/test_mmu_minimal_json.cpp`（~100 LOC）
- **修改文件**:
  - `ip/mmu/lib/tlb.h`：mutable 关键字（~3 行）
  - `ip/mmu/lib/ptw.{h,cpp}`：stub → 实装（~+200 LOC）
  - `ip/mmu/lib/multi_level_tlb.cpp`：stub → 实装（~+150 LOC）
  - `ip/mmu/lib/tlb_factory.cpp`：7 组特化（~+150 LOC）
  - `ip/mmu/policies/tlb_replacement_policy.cpp`：显式实例化列表 8→12（~+20 LOC）
  - `ip/mmu/tlm/MMUPlugin.{h,cpp}`：stub → at_stage 闭包实装（~+300 LOC）
  - `ip/mmu/tlm/mmu_keys.h`：+4 Key（~+15 LOC）
  - `ip/cpu/plugins/mmu.h`：`RiscvMMUPlugin` 继承实装（~+200 LOC）
  - `ip/cpu/cpu_factory.{h,cpp}`：PluginOrder 加 RiscvMMUPlugin（~+10 LOC）
  - `根 CMakeLists.txt`：加 `add_subdirectory(ip/mmu)`（+1 行）
  - `tests/CMakeLists.txt`：加 `MMU_IMPL_SOURCES` + 删 5 行 `list(REMOVE_ITEM)`（~+15 LOC）
  - 5 个 `tests/mmu/test_*.cpp`：扩展 stub → 完整（~+400 LOC）
  - `ip/mmu/STATUS.md`：PARTIAL → IMPLEMENTED
  - `CHANGELOG.md`：v0.0.9
  - `docs/architecture/overview.md`：快照更新
- **不修改**: 见 "What Changes / 不修改" 节
- **依赖与时序**:
  - 本 change 必须在 mmu-ip-skeleton（archived）**之后**实施——骨架提供 lib/tlm 接口 ✓
  - 本 change 必须在 ADR-044（cb85fea, baa3757）**之后**实施——VIPT 数据流契约已锁定 ✓
  - 本 change **不**触及 `ip/cpu/plugins/` 中除 mmu.h 外的 Plugin（保持零干扰）✓
  - 后续 change `mmu-cache-integration`（L1Cache 消费 MMU_VADDR + SoC 集成 + DSE + CtrlLink stall）依赖本 change 完成
  - 后续 change `mmu-sv32-sv48-ext` 独立启动
- **基线影响**:
  - 当前 ctest 基线：259/259 PASS（report `docs/build-reports/2026-07-02-build-test-regression.md`）
  - 增量：5 个 mmu 测试解阻塞（+33 → 扩展到 69）+ 2 个新 test（integration + soc） = **+40 cases** → **299 cases**
  - **不**含 DSE 48-case sweep（推迟到 mmu-cache-integration）
- **breaking 变更**:
  - `ip/mmu/lib/*.cpp` 的 stub 实现替换为完整实装（lib/ 接口签名不变，下游无破坏）
  - `tests/CMakeLists.txt` 删除 `list(REMOVE_ITEM)` 5 行 + 加 `MMU_IMPL_SOURCES`
  - `ip/cpu/plugins/mmu.h` 的占位类被实装（继承 MMUPlugin）
- **HDL 友好性验证**: 通过现有 `tools/check_plugin_portability.sh`（已 CI gate）+ `tools/verify_plugin_decision.sh`（已 CI gate）覆盖；不引入新脚本

## Alternatives Considered

### Alternative A: 推迟 PTW Sv39 walk 到独立 change（mmu-sv39-ptw）

**放弃理由**: PTW walk 与 TLB lookup 同步验证收益大；分拆意味着 MMUPlugin at_stage 闭包要分两次实施。**不放弃**: 一起实施。

### Alternative B: Sv32 + Sv39 一次实装

**放弃理由**: Sv32 PTE 4 字节 vs Sv39 PTE 8 字节编码差异易引入 bug。**不放弃**: 仅 Sv39。

### Alternative C: CtrlLink stall 一起实装

**放弃理由**: 实测 `CtrlLink::halt_when` 接受 `std::function<bool()>` 谓词，全仓库无 should_halt 轮询，框架未就绪。**不放弃**: stall 推迟到 mmu-cache-integration。

### Alternative D: L1Cache 消费 MMU_VADDR 一起实装

**放弃理由**: ADR-044 §6 把 L1Cache 侧消费明确列为 Phase 1.5，与 mmu-tlb-ptw-impl 是不同 change。MMU 侧双 Key 输出已满足契约，L1Cache 侧推迟避免 scope 膨胀。**不放弃**: L1Cache 消费推迟到 mmu-cache-integration。

### Alternative E: DSE 48-case 扫描一起实装

**放弃理由**: DSE 笛卡尔积扫描是 mmu-cache-integration 阶段的 L1Cache 集成验证内容；本 change 是 TLB/PTW 算法实装 + 解阻塞，不引入新覆盖。**不放弃**: DSE 推迟。

### Alternative F: 4 个替换策略从头实装

**放弃理由**: 4 策略已在 `ip/mmu/policies/tlb_replacement_policy.{h,cpp}` 完整实现（含 8 组显式实例化）。**不放弃**: 只扩展显式实例化表（8→12）。
