## Context

`mmu-ip-skeleton`（archived 2026-06-29）落地了 `ip/mmu/` 目录骨架（913 LOC lib + 258 LOC tlm），包含：
- `ip/mmu/lib/tlb.h`（236 行）：模板化 `TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` —— **方法全部已实装（header-only）**，但 `hits_`/`misses_`/`evicts_` 三个统计字段被 `const` 的 `lookup()` 修改（编译错误）
- `ip/mmu/lib/multi_level_tlb.{h,cpp}`：N 级 TLB 编排器（coherence 接口已声明，stub）
- `ip/mmu/lib/ptw.{h,cpp}`：PageTableWalker 接口 + Sv32/Sv39/Sv48 解码器签名（stub）
- `ip/mmu/lib/tlb_factory.{h,cpp}`：工厂特化（stub）
- `ip/mmu/tlm/MMUPlugin.{h,cpp}`：PluginBase 派生，`setup()`/`build()` 空函数；**已有 5 个 substage 声明**：`tlb_lookup_ifetch` / `tlb_lookup_loadstore` / `ptw_l2` / `ptw_l1` / `ptw_l0`
- `ip/mmu/tlm/mmu_keys.h`：**已有 10 个 Key**：`VADDR` / `PADDR` / `PTW_ACTIVE` / `PTW_VADDR` / `PTW_ASID` / `PTW_L0_RAW` / `PTW_L1_RAW` / `PTW_L2_RAW` / `PTW_FAULT` / `PERMS`（**缺** `MMU_VADDR` / `EXCEPTION_CODE` / `SATP_PPN` / `SATP_MODE`）
- `ip/mmu/policies/tlb_replacement_policy.{h,cpp}`：4 策略（None/FIFO/LRU/RRIP）**已完整实装** + 8 组显式实例化 `<8,1>/<16,2>/<32,4>/<64,4>/<128,4>/<256,8>/<8,8>/<16,16>`
- `bundles/mem_bundles.h` 已有 `TlbReq` / `TlbResp` Bundle

ADR-044（`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`，cb85fea）锁定 **VIPT 数据流契约**：
- L1 ICache/DCache 统一 VIPT（`idx_bits + offset_bits ≤ 12`）
- 编译期 `static_assert` 已在 `L1CachePlugin.h` 落地（baa3757）
- **MMUPlugin 必须输出 `pl::PADDR` + `pl::MMU_VADDR`** 供 L1CachePlugin VIPT 索引

**当前状态 (2026-09-11)**:
- `ip/mmu/lib/*.cpp` 仅 81 + 97 + 60 = 238 LOC stub 实现
- `tests/mmu/test_*.cpp` 5 个文件被 `tests/CMakeLists.txt` 的 `list(REMOVE_ITEM)` 排除
- `ip/mmu/STATUS.md` = `PARTIAL (mmu-ip-skeleton)`，"下一里程碑: mmu-tlb-ptw-impl"
- **构建接线缺失**：根 `CMakeLists.txt` 不含 `add_subdirectory(ip/mmu)`；`tests/CMakeLists.txt` 无 `MMU_IMPL_SOURCES`；**无 mmu 源码被编进 build**
- 259/259 ctest PASS（基线，5 个 mmu test 已排除）

**约束**:
- 与 `ip/cache/` 完全同构（lib/tlm 双层切分 + HDL 友好约束）
- D4 Plugin 范式强制（业务代码无 `tick()`/无状态机/at_stage 闭包/Bundle 字段 `cf::plugin::uint_t<N>`）
- HDL 友好（Phase 5 CppHDL 转换零阻力：`static_assert` 禁 `std::optional`/`virtual`/动态分配）
- 现有 259/259 ctest PASS 不破
- `bundles/mem_bundles.h` 现有 Bundle 不改签名
- CpuFactory 已注册 `RiscvMMUPlugin` 类型别名（`ip/cpu/plugins/mmu.h` 占位类）
- ADR-044 VIPT 数据流契约不修改（MMU 侧双 Key 输出已满足；L1Cache 侧消费推迟到 `mmu-cache-integration`）
- **不新建 `tools/verify_hdl_friendly.sh`**；HDL 友好性由现有 `tools/check_plugin_portability.sh`（已 CI gate）覆盖

**利益相关者**:
- `ip/cpu/plugins/mmu.h` 重构影响 CpuFactory（11-Plugin 套件注册路径）
- L1Cache 集成者（后续 `mmu-cache-integration`）：MMU↔Cache VIPT 数据流消费者
- Phase 5 CppHDL 迁移者：TLB 模板需可直接生成 `ch::Component`

## Goals / Non-Goals

**Goals:**
- 修复 `ip/mmu/lib/tlb.h` 编译错误（`mutable` 关键字）
- 实装 `ip/mmu/lib/multi_level_tlb.cpp` —— N 级 TLB 编排器（**并行查全部 level，返回最深 hit** + shadow fill + 反向 invalidate + ASID/process switch）
- 实装 `ip/mmu/lib/ptw.cpp` —— PTW Sv39 三级 walk 状态机（**PTW 持成员状态**，lib 纯 C++）
- 实装 `ip/mmu/lib/tlb_factory.cpp` —— 7 组特化
- 扩展 `ip/mmu/policies/tlb_replacement_policy.cpp` 显式实例化表 8→12
- 实装 `ip/mmu/tlm/MMUPlugin.cpp` —— at_stage 闭包（**沿用已有 5 个 substage 声明**：`tlb_lookup_ifetch` / `tlb_lookup_loadstore` / `ptw_l2` / `ptw_l1` / `ptw_l0`）
- 在 `ip/cpu/plugins/mmu.h` 实装 `RiscvMMUPlugin`（**继承 MMUPlugin**，单源真相）—— satp CSR + SFENCE.VMA + exception 12/13/15
- 在 CpuFactory PluginOrder 加 `RiscvMMUPlugin`（条件：enable_mmu=true；schema 字段已存在）
- 实装 `src/cf_plugin/bridge/mmu_bridge.{h,cpp}` + `mmu_bridge_adapter.{h,cpp}`（同构 `L1CacheTLMBridge`）
- **构建接线**：新建 `ip/mmu/CMakeLists.txt` + 根 `add_subdirectory(ip/mmu)` + `tests/CMakeLists.txt` 加 `MMU_IMPL_SOURCES`
- 解除 `tests/CMakeLists.txt` 的 5 行 `list(REMOVE_ITEM)`
- 扩展 5 个 `tests/mmu/test_*.cpp` → 完整测试（**实测当前 29 → 目标 65 cases**）
- 新增 `tests/cache/test_mmu_cache_integration.cpp`（**仅 MMU 双 Key 输出验证**，不验证 L1Cache 消费）
- 新增 `tests/soc/test_mmu_minimal_json.cpp` + `soc/mmu_minimal.json`
- 同步 `ip/mmu/STATUS.md` / `CHANGELOG.md` / `docs/architecture/overview.md`

**Non-Goals:**
- **不**实装 Sv32/Sv48 PTW walk（推迟到 `mmu-sv32-sv48-ext`）
- **不**实装 L1Cache 消费 `pl::MMU_VADDR`（推迟到 `mmu-cache-integration`，与 ADR-044 §6 Phase 1.5 对齐）
- **不**实装 CtrlLink stall 机制（实测 `CtrlLink::halt_when` 接受 `std::function<bool()>` 谓词，无 should_halt 轮询；推迟到 `mmu-cache-integration`）
- **不**实装 DSE 48-case 扫描（推迟到 `mmu-cache-integration`）
- **不**实装 `ip/mmu/rtl/`（Phase 5+ 沿用）
- **不**实装 SoC 多核 TLB shootdown 协议
- **不**实现 `split_id` topology（保留 schema enum 但实装仍 `unified`）
- **不**实现 ASID=0 Bare 完整 path（仅 TLB bypass，无 PTW）
- **不**实现性能计数器（推迟到 DSE 阶段）
- **不**改 `L1CachePlugin` 行为（**核心约束**，ADR-044 §6 Phase 1.5 工作）
- **不**新建 `tools/verify_hdl_friendly.sh`（用现有 `check_plugin_portability.sh` 替代）

## Decisions

### Decision 1: tlb.h 编译错误修复 —— mutable 关键字

**选择**: 把 `hits_`/`misses_`/`evicts_` 三个统计字段加 `mutable` 关键字：

```cpp
mutable uint64_t hits_ = 0;
mutable uint64_t misses_ = 0;
mutable uint64_t evicts_ = 0;
```

**WHY**: `lookup()` 是 `const`（接口要求不变性），但统计字段需要写入。`mutable` 是 C++ 标准方案，HDL 友好（不变内存布局）。备选：把统计写入改成 `static` 局部（线程不安全），或移除统计接口（损失可观测性）。

**Trade-off**: `mutable` 在单线程 Plugin 上下文无副作用（无 race），保留统计能力。

### Decision 2: PTW state machine —— lib 持成员状态，tlm 同步 Payload

**选择**: PTW 持**成员状态**（`current_pte_l2/l1/l0`、`pending_walk_`、`walk_stage_` 枚举），lib/ 不引入 Payload 依赖；MMUPlugin 在 tlm/ 层每个 cycle 同步到 `pl::PTW_L0_RAW`/`PTW_L1_RAW`/`PTW_L2_RAW`/`PTW_FAULT`（已有 Key）：

```cpp
// ip/mmu/lib/ptw.h —— lib/ 纯 C++
class PTW {
  enum class WalkStage { Idle, WalkL2, WalkL1, WalkL0, CheckPerms, WriteBack, Done };
  WalkStage stage_ = WalkStage::Idle;
  PTE current_pte_l2_, l1_, l0_;
  std::array<PTE, 4096> pte_stub_memory_;  // 4096 entry × sizeof(PTE) ≈ 192KB/实例 (Oracle 修正：原 "32KB" 误算); idx = (ppn >> 12) & 0xFFF
public:
  void start_walk(uint64_t vaddr, uint16_t asid, uint64_t satp_ppn);
  WalkResult advance();  // 每 cycle 推进一级
};
```

**WHY**: lib/ 唯一允许的依赖是 `cf/plugin/uint_t.h`（README + AGENTS.md 强制）。Payload 是 tlm/ 概念，PTW 必须保持纯 C++。**修正**: 原 plan.md spec 措辞"PTW MUST propagate walk state via Payload Keys"违反此约束，已修正为"MMUPlugin 在 tlm 层同步 PTW 状态到 Payload"。

**Trade-off**: PTW 是状态机（lib 层使用 `enum class WalkStage`），但每个 cycle 由 MMUPlugin::at_stage 推进——**业务层无 tick()**，状态机不触发 D4 检查器拦截。

### Decision 3: VIPT 数据流用 2 个独立 Payload Key（mmu 侧）

**选择**: MMUPlugin::at_stage 命中时同时写 `pl::PADDR` + `pl::MMU_VADDR`；PTW miss 期间**只写 `pl::PTW_ACTIVE=1`**，PADDR/MMU_VADDR 在 walk 完成（`ptw_l0` 末）才写。

**WHY**: ADR-044 §3.2 要求命中路径双写；miss 路径 VIPT 索引无意义（PTW stall 期间 L1Cache 不应该 speculatively index）。备选方案：miss cycle 0 写 MMU_VADDR "speculative VIPT"——但 L1Cache 在 stall 期间不动，多写一个 Key 无收益。

**收缩**: L1CachePlugin 消费 `pl::MMU_VADDR` 推迟到 `mmu-cache-integration`（ADR-044 §6 Phase 1.5）；本 change 的 integration test 仅验证 MMU 侧双写一致性。

### Decision 4: MultiLevelTLB 并行查全部 level，返回最深 hit

**选择**: `MultiLevelTLB::lookup()` 并行调用所有 level 的 `lookup()`，从结果中选 **level 编号最大的 hit**（spec mmu-multi-level-coherence 修订）：

```cpp
TLBLookupResult MultiLevelTLB::lookup(uint64_t vaddr, uint16_t asid) {
  TLBLookupResult best{};
  best.hit = false;
  size_t best_level = 0;
  for (size_t i = 0; i < levels_.size(); ++i) {
    auto r = levels_[i]->lookup(vaddr, asid);
    if (r.hit && i >= best_level) {  // 取最深 hit
      best = r;
      best_level = i;
    }
  }
  return best;
}
```

**WHY**: spec 要求"返回最深 hit"——深层 PTE 来自更权威的 walk 路径，更可靠。备选（最浅 hit 优先）会让 stale-shallow 条目覆盖 deep-new，但行为与现 `multi_level_tlb.cpp` 一致——本 change 改 spec 与代码对齐。

**Trade-off**: 当浅深两层 paddr 不一致时，深层赢（避免 stale shallow hit）。

### Decision 5: TLBFactory 7 组新特化 + 显式实例化扩展

**选择**: `TLBFactory` 加 7 组新特化（None(16,1) + LRU-4w(64,4) + LRU-8w(64,8) + LRU-2w(32,2) + FIFO(256,1) + RRIP-4w(64,4) + RRIP-8w(64,8)），同步扩展 `tlb_replacement_policy.cpp` 的显式实例化表 8→12（含新增 `<16,1>/<32,2>/<64,8>/<256,1>`）。

**WHY**: 新工厂组合若不在显式实例化表，链接时 `TLBReplacementPolicy<...>::create` undefined reference。骨架阶段 8 组不含本 change 需要的几组。

**Trade-off**: 12 组显式实例化增加编译时间（每次新增 ~30s 编译开销），但 LRU/FIFO/RRIP 共享代码（仅 create() 工厂不同）。

### Decision 6: RiscvMMUPlugin 单源真相位于 ip/cpu/plugins/mmu.h

**选择**: `RiscvMMUPlugin` 实现位于 `ip/cpu/plugins/mmu.h`（已有占位类），继承 `cf::ip::mmu::MMUPlugin`，**不**新建 `ip/mmu/tlm/RiscvMMUPlugin.{h,cpp}`：

```cpp
// ip/cpu/plugins/mmu.h
namespace cf::cpu::plugins {
class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin {
  uint64_t satp_value_ = 0;
public:
  void setup() override;
  void build() override;
  void at_stage(const char* stage) override;
};
}
```

**WHY**: CpuFactory 已经 include `ip/cpu/plugins/mmu.h`；新建 mmu/tlm 文件需修改 factory.cpp include 路径，违反"不修改 RISC-V 其他 Plugin"约束。**修正**: 原 plan.md 提议"单源真相位于 ip/cpu/plugins/mmu.h"——本 design 强化此决策。

**Trade-off**: mmu IP 物理边界模糊（cpu 命名空间下有 MMU 类），但符合现状 factory 设计。

### Decision 7: PTW PTE stub memory —— 4096 entry 32KB + 索引映射

**选择**: `std::array<PTE, 4096> pte_stub_memory_`（32KB），索引约定 `pte_stub_memory_[(ppn >> 12) & 0xFFF]`（取 ppn 低 12 位作为 4KB 对齐 entry 索引）：

```cpp
// ip/mmu/lib/ptw.h
std::array<PTE, 4096> pte_stub_memory_{};
// walk_l2: pte_stub_memory_[(satp_ppn >> 12) & 0xFFF]
// walk_l1: pte_stub_memory_[((l2_pte.ppn + vpn[1]) >> 12) & 0xFFF]
// walk_l0: pte_stub_memory_[((l1_pte.ppn + vpn[0]) >> 12) & 0xFFF]
```

**WHY**: 4096 entry（**Oracle 修正**：原 "32KB" 误算，`sizeof(PTE) ≈ 48B` → 实际 ~192KB/实例；足够测试覆盖，HDL 1:1 推迟 Phase 5 时再考虑压缩）；索引约定让测试填充可预测（直接 `[ppn & 0xFFF]`）。备选（65536 / 256KB）会浪费编译期内存（模板实例化膨胀）。

**修正**: 原 plan.md 写 `std::array<Pte, 65536>` + 256KB，类型名 `Pte` 是错的（实际是 `PTE`）——已修正。

**Trade-off**: 测试填充 API 简单（`pte_stub_memory_[idx] = test_pte`），但 Sv39 walk 索引与真实 MMU 不一致（真实硬件从内存读 PTE）——这是 stub 局限，HDL 1:1 推迟到 Phase 5。

### Decision 8: 不实装 CtrlLink stall

**选择**: PTW walk 期间**不**调 `CtrlLink::halt_when`。MMUPlugin 在 walk cycle 持续推进 substage（`ptw_l2`/`ptw_l1`/`ptw_l0`），UPstream pipeline 自然从 cycle 0 到 cycle 3 stall（**D4 强制**：at_stage 闭包完整执行，无早返）。

**WHY**: 实测 `CtrlLink::halt_when(std::function<bool()>)` 需要谓词 + should_halt 轮询，框架未就绪。PTW 多 cycle 推进由 substage 链天然提供（每 cycle 一个 substage）——无需 halt。

**修正**: 原 plan.md Decision 2 写 `ctrl_link_.halt_when(30)` 是错的 API（halt_when 不接受 int）。

**Trade-off**: 上游 pipeline 不感知 PTW stall（继续以每个 cycle 1 substage 速度推进）；语义上 3 cycle latency 与 halt 等价，但无显式 halt 信号。

### Decision 9: 5 个测试扩展粒度

**选择**: 29 → 65 cases，按"4 策略 × 4 操作矩阵"扩展（hit/miss/evict/invalidate × 4 policies = 16 cases 主体 + 边界用例）：

| 文件 | 现状（实测） | 目标 | 扩展方法 |
|------|------|------|----------|
| test_tlb_unit.cpp | 8 | 18 | +4 策略 × hit/miss 矩阵（10） |
| test_multi_level_tlb.cpp | 8 | 16 | +shadow fill + reverse invalidate + ASID switch（8） |
| test_tlb_factory.cpp | 6 | 13 | +7 组特化 + VIPT safety 拒绝（7） |
| test_mmu_config_schema.cpp | 2 | 8 | +DSE JSON Schema 验证（6）（**实测**当前只有 2 个 case，不是 6） |
| test_mmu_plugin.cpp | 5 | 10 | +at_stage 闭包 + CtrlLink 路径（5） |

**WHY**: 29 → 65 = +36 cases，每文件 +2~+7，按"主体覆盖 + 边界用例"分配。

**Trade-off**: 总测试数 259 → 330 = +71 cases（mmu 解阻塞 +29 + 扩展 +36 + integration +1 + soc +1 + 调整 = 71）。

## Risks / Trade-offs

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| TLBFactory 7 组新特化 + 12 组显式实例化编译时间增长 | Medium | Low | 接受 +30s 编译开销；CI 总时长仍在可接受范围 |
| RiscvMMUPlugin CpuFactory 注册导致 11-Plugin 套件回归 | Medium | High | task 5.2 验证 `[cpu]` 114 + `[cpu-configs]` 9 + `[cpu-integration]` 25 = 148 cases（**实测 25，之前文档错的**）全部 PASS |
| `RiscvMMUPlugin` Stage 挂载点（csr_write_satp / sfence_vma）在 TopologyBuilder 无对应父 stage | Medium | High | RiscvMMUPlugin::setup 自声明 substage 在 `execute` 父 stage 下；与 RiscvCsrPlugin 的耦合通过 Payload Key（不修改 RiscvCsrPlugin） |
| `enable_mmu` 已在 cpu_default.json 默认 true，CpuFactory 旧测试可能已隐式 enable | Medium | High | 实测验证（baseline 259/259 PASS 已含默认 enable_mmu=true 路径） |
| PTW stub memory 索引映射（`(ppn >> 12) & 0xFFF`）与真实 Sv39 硬件行为不同 | High | Low | 文档明确标注 stub 局限；HDL 1:1 推迟到 Phase 5 |
| `test_mmu_plugin.cpp` standalone 测试缺父 stage（fetch/memory）导致 substage 声明失败 | Medium | Medium | 测试 setup 时给 fetch/memory 父 stage stub；或声明时不带父参数（需 cf::plugin::PipeBuilder 支持） |
| mmu_keys.h 已有 10 个 Key，task 2.2 新增重复导致编译错误 | Medium | Low | 任务明确只新增 4 个（MMU_VADDR / EXCEPTION_CODE / SATP_PPN / SATP_MODE），不重定义已存在的 |
| PTW 状态机违反 D4（lib 层用 enum class） | Low | Medium | D4 强制"业务代码无状态机"——但 PTW 是 lib 算法层，状态机由 MMUPlugin 每 cycle 推进，符合 D4（业务层无 tick） |
| spec mmu-vipt-data-flow 中 L1Cache MUST 与"不修改 L1Cache"约束矛盾 | High | High | 已修订 spec：L1Cache MUST 改为"MUST emit MMU_VADDR consumer API readiness check in Phase 1.5 (deferred)"，本 change 仅 MMU 侧 MUST |
| tools/verify_hdl_friendly.sh 引用（spec scenario + design Risk） | Medium | Low | 删除所有引用；用现有 `check_plugin_portability.sh` 替代 |
| `mutable` 引入的 thread-safety 风险 | Low | Low | cf::plugin Plugin 单线程调度上下文，无 race |
| PTW `pte_stub_memory_` 跨测试实例的污染 | Low | Low | 测试 fixture 在每个 case 重新构造 PTW 实例 |

## Migration Plan

**11 atomic commits** (按依赖顺序；**Oracle 风险 #1 修正**：commit 4 必须包含 MMUPlugin.cpp 调用点适配以避免中间 build 红):

1. **fix(mmu): repair tlb.h const-correctness via mutable** —— 修改 `ip/mmu/lib/tlb.h`，3 行 `mutable` 关键字（最小可工作集）
2. **build(mmu): wire ip/mmu sources into build** —— 新建 `ip/mmu/CMakeLists.txt`；根 `CMakeLists.txt` 加 `add_subdirectory(ip/mmu)`；`tests/CMakeLists.txt` 加 `MMU_IMPL_SOURCES` + 删 5 行 `list(REMOVE_ITEM)`
3. **feat(mmu): expand policy explicit instantiations 8→12** —— `ip/mmu/policies/tlb_replacement_policy.cpp` 加 4 组
4. **feat(mmu): PTW Sv39 walk state machine** —— `ip/mmu/lib/ptw.{h,cpp}` 实装
5. **feat(mmu): MultiLevelTLB coherence protocol** —— `ip/mmu/lib/multi_level_tlb.cpp` 实装
6. **feat(mmu): TLBFactory 7 组特化** —— `ip/mmu/lib/tlb_factory.cpp` 实装
7. **feat(mmu): MMUPlugin at_stage closures** —— `ip/mmu/tlm/MMUPlugin.{h,cpp}` + mmu_keys.h +4 Key
8. **feat(cpu): RiscvMMUPlugin RISC-V ISA adapter** —— `ip/cpu/plugins/mmu.h` + CpuFactory 注册
9. **feat(cf-plugin): MMUTLMBridge + Adapter** —— `src/cf_plugin/bridge/mmu_bridge.{h,cpp}` + `mmu_bridge_adapter.{h,cpp}`
10. **test(mmu): unblock + extend 5 mmu test + add 2 new test** —— 5 个 tests/mmu/test_*.cpp 扩展 + tests/cache/test_mmu_cache_integration.cpp + tests/soc/test_mmu_minimal_json.cpp + soc/mmu_minimal.json
11. **docs(mmu): STATUS IMPLEMENTED + CHANGELOG v0.0.9** —— `ip/mmu/STATUS.md` + `CHANGELOG.md` + `docs/architecture/overview.md`

**Phase 2 (后续 change, 独立)**:
- `mmu-cache-integration`：L1Cache 消费 MMU_VADDR + SoC JSON 集成测试 + DSE 48-case sweep + CtrlLink stall 机制
- `mmu-sv32-sv48-ext`：Sv32/Sv48 PTW walk 扩展

**Rollback 策略**:
- 本 change 是 additive（修 tlb.h + 加 .cpp + 扩 test），rollback = revert 11 commits
- 新增 `MMUPlugin::issue_request/read_response` API 是 additive，其他 Plugin 不依赖
- 已加的 4 个 Payload Key 不影响其他 Plugin（未注册读取）

## Open Questions

1. **PTW PTE 内存读路径**：选择 stub `std::array<PTE, 4096>` + 索引约定 `(ppn >> 12) & 0xFFF`（**决策完成**：stub 32KB，足够测试覆盖，HDL 1:1 推迟 Phase 5）
2. **DSE 扫描范围**：推迟到 `mmu-cache-integration`（**决策完成**：本 change 不含 DSE）
3. **satp.PPN 来源**：RiscvMMUPlugin 自持 CSR 状态 + 测试直接写 `pl::SATP_PPN`（**决策完成**：不依赖 RiscvCsrPlugin 写）
4. **L1Cache 消费 `pl::MMU_VADDR`**：推迟到 `mmu-cache-integration`（**决策完成**：与 ADR-044 §6 Phase 1.5 对齐）
