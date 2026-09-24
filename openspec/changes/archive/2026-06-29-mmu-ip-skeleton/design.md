## Context

`ip/cpu/plugins/mmu.h` 当前是占位类（41 行，setup/build 空函数），注释明示"未来扩展 TLB + 页表遍历"。其他 IP（`ip/cache/`, `ip/memory/`, `ip/interconnect/`, `ip/peripheral/`, `ip/tilecore/`, `ip/tilecopy/`）都有独立目录骨架。MMU 作为微架构密集单元（CAM + 多级 + 替换 + PTW），放在 `ip/cpu/plugins/` 内部会与 `branch_predictor`/`reg_file`/`hazard` 等执行流 Plugin 性质混淆。

**当前状态 (2026-06-24)**:
- `ip/cpu/plugins/mmu.h` 占位 41 行，零实装
- `ip/cpu/configs/cpu_params_schema.json` 已有 `enable_mmu: bool` + `mmu_mode: enum(sv32/sv39/sv48)` 配置字段，但无对应实现
- `L1CachePlugin` 已有完整 TLM 骨架 + Bridge + Adapter + params_schema + 4/4 单元测试 + 13 个跨层测试，**可作为本 change 的同构参照模板**
- D4 Plugin 范式强制：业务代码无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`、阶段用 `at_stage()`、跨阶段通信用 `Payload<T>` Key
- CppHDL 迁移路径（Phase 5/6）要求 std::array / uint_t<N>，禁止 std::optional / virtual / dynamic alloc

**约束**:
- 与 `ip/cache/` 完全同构（tlm/rtl/test/configs/docs 标准目录 + policies/ 子目录）
- D4 Plugin 范式合规
- HDL 友好（Phase 5 CppHDL 转换零阻力）
- 现有 21/21 ctest PASS 不破
- Bundle 字段全部 `cf::plugin::uint_t<N>`
- `bundles/mem_bundles.h` 现有 6 个 Bundle 不改签名，仅追加 `TlbReq`/`TlbResp`

**利益相关者**:
- `ip/cpu/plugins/mmu.h` 重构影响后续 `RiscvMMUPlugin` 实施者
- L1Cache 集成者：未来 MMU↔L1Cache 一致性协议设计需要协调（推迟到 `mmu-tlb-ptw-impl`）
- DSE 配置者：MMU 维度加入 DSE 扫描
- Phase 5 CppHDL 迁移者：TLB 模板需可直接生成 ch::Component

## Goals / Non-Goals

**Goals:**
- 建立 `ip/mmu/` IP 目录骨架（与 `ip/cache/` 同构）
- 落地模板化 `TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` 单级 TLB
- 落地 `MultiLevelTLB` 多级 TLB 编排器（N=1..4 可配置，coherence 协议：shadow fill + 反向失效）
- 落地 4 种 `TLBReplacementPolicy`（None/FIFO/LRU/RRIP），接口类比 `cf::ip::cache::policies::ReplacementPolicy`
- 落地 `PageTableWalker` 状态机接口（具体算法推迟）
- 落地 `MMUPlugin`（Plugin 入口，持 MultiLevelTLB + PageTableWalker）
- 落地 `TLBFactory`（按 JSON 配置选模板特化）
- 落地 HDL 友好约束（编译期 `static_assert` 禁 optional/virtual/dynamic alloc；模板化定长）
- 落地 GPU/CPU 双兼容配置（`asid_bits`/`topology`/`num_lookup_ports`/`supported_page_sizes` 全部 JSON 驱动）
- 落地 `bundles/mem_bundles.h` 扩展（TlbReq/TlbResp）
- 重构 `ip/cpu/plugins/mmu.h` 为 `RiscvMMUPlugin` 继承 `cf::ip::mmu::MMUPlugin`
- 12-15 个新单元测试（骨架 smoke + 多级 + config + factory）

**Non-Goals:**
- **不**实现 TLB lookup 算法（命中比较、tag match、permission check 推迟到 `mmu-tlb-ptw-impl`）
- **不**实现 PTW 状态机具体转换逻辑（接口 + stub 即可）
- **不**实现 satp CSR 解码（归 `RiscvMMUPlugin` 后续 change）
- **不**实现 PTE 格式解析（归 `PageTableWalker` 后续 change）
- **不**实现 L1Cache↔MMU 一致性协议（VIVT/VIPT/PIPT 推迟）
- **不**实现 `ip/mmu/rtl/`（Phase 5+ 沿用目录占位）
- **不**实现 cpptlm `MMUTLMBridge` 适配（与 `L1CacheTLMBridge` 同构，但推迟到 TLB/PTW 算法稳定后）
- **不**实现 SoC JSON 拓扑样例（推迟到 `mmu-tlb-ptw-impl`）
- **不**改 `ip/README.md` STATUS 段以外的章节
- **不**改 `L1CachePlugin` 行为 / 现有 13 个测试
- **不**实现 `split_id` 拓扑（骨架阶段仅 `unified`，split_id 是配置 schema 预留项）

## Decisions

### Decision 1: TLB 模板化 + 抽象基类 + 工厂 (三件套)

**选择**: 三个抽象层次各司其职
```cpp
// 1. 抽象基类: MultiLevelTLB 持多态
class TLBBase {
 public:
  virtual TLBLookup lookup(uint_t<64> vaddr, uint_t<16> asid) const = 0;
  virtual void insert(...) = 0;
  virtual void invalidate_vaddr(...) = 0;
  virtual void invalidate_asid(...) = 0;
  virtual void invalidate_all() = 0;
  virtual uint64_t hit_count() const = 0;
  virtual uint64_t miss_count() const = 0;
  virtual const char* name() const = 0;  // 返回 "L0"/"L1"/...
  virtual ~TLBBase() = default;
};

// 2. 模板化实现: 编译期定长, HDL 友好
template <std::size_t ENTRIES, std::size_t WAYS,
          std::size_t TAG_BITS, std::size_t ASID_BITS,
          std::size_t PORTS = 1>
class TLB : public TLBBase {
  std::array<TLBEntry, ENTRIES> entries_{};
  // ... 模板实现
};

// 3. 工厂: JSON 配置 → 模板特化
class TLBFactory {
 public:
  static std::unique_ptr<TLBBase> create(const TLBLevelConfig& cfg);
};
```

**理由**:
- `TLB<>` 模板保证编译期定长（std::array、log2_ceil 编译期算），Phase 5 CppHDL 1:1 映射
- `TLBBase` 让 `MultiLevelTLB` 持 `std::vector<std::unique_ptr<TLBBase>>` 多态容器
- `TLBFactory` 桥接"JSON 运行时"与"模板编译期"，DSE 扫描只编译支持的几组特化（典型 4-7 组）

**替代方案**:
- ❌ 单一非模板 `TLB` + `std::vector<Entry>`: 运行时大小，HDL 转换需重写
- ❌ `TLBL0`/`TLBL1` 继承 `TLBBase`: 每加一级要新类，coherence 协议分散
- ❌ 仅模板无工厂: 难以从 JSON 启动时实例化

### Decision 2: MultiLevelTLB 编排器，coherence 协议在编排器层

**选择**: MultiLevelTLB 负责所有 coherence（shadow fill + 反向失效），单级 TLB 不知道其他级存在

```cpp
class MultiLevelTLB {
  std::vector<std::unique_ptr<TLBBase>> levels_;  // [L0, L1, ..., Ln-1]
  bool shadow_fill_enabled_;
  
  TLBLookup lookup(uint_t<64> vaddr, uint_t<ASID_BITS> asid) {
    for (size_t i = 0; i < levels_.size(); ++i) {
      auto r = levels_[i]->lookup(vaddr, asid);
      if (r.hit) {
        if (shadow_fill_enabled_) {
          for (size_t j = 0; j < i; ++j) {
            levels_[j]->insert_from(r);  // 浅层回填
          }
        }
        return r;
      }
    }
    return {.hit = 0};
  }
  
  void refill_from_ptw(uint_t<64> vaddr, uint_t<ASID_BITS> asid,
                       uint_t<64> paddr, uint_t<8> perms) {
    levels_.back()->insert(vaddr, paddr, asid, perms);  // 写入最深层
    if (shadow_fill_enabled_) {
      for (size_t i = 0; i < levels_.size() - 1; ++i) {
        levels_[i]->insert(vaddr, paddr, asid, perms);
      }
    }
    // 注: 此时不需反向失效, 因为 insert 覆盖相同 vaddr
  }
  
  void invalidate_vaddr(uint_t<64> vaddr, uint_t<ASID_BITS> asid) {
    for (auto& lvl : levels_) lvl->invalidate_vaddr(vaddr, asid);
  }
  
  void invalidate_asid(uint_t<ASID_BITS> asid) {
    for (auto& lvl : levels_) lvl->invalidate_asid(asid);
  }
};
```

**理由**:
- 单级 TLB 接口简洁（只管自己），便于 Phase 5 CppHDL 1:1 转换
- coherence 协议集中管理（shadow fill / invalidate 顺序），不分散到每级
- 测试容易：单级 TLB 独立单元测试 + MultiLevelTLB 协议集成测试

**替代方案**:
- ❌ TLB 知道兄弟级：每级重复 coherence 协议逻辑，N 级不一致风险
- ❌ 在 MMUPlugin 处管 coherence：MMUPlugin 变成"上帝对象"，违反单一职责

### Decision 3: TLBReplacementPolicy 与 cache 命名空间隔离

**选择**: 独立 `cf::ip::mmu::policies::TLBReplacementPolicy`，不复用 `cf::ip::cache::policies::ReplacementPolicy`

**理由**:
- TLB 与 cache 的 entry 生命周期不同（TLB evict 不需要 writeback；cache writeback dirty line）
- 接口签名相同（`on_access`/`select_victim`/`on_insert`/`name`），调用方零感知
- 命名空间隔离避免循环依赖与未来差异（TLB 可能加 `on_tlb_shootdown` 等 cache 不需要的方法）

**替代方案**:
- ❌ 复用 cache `ReplacementPolicy`: 短期省事，但 TLB 特有语义（ASID-aware shootdown）需扩展时需改基类，破坏 cache
- ❌ 模板化共用基类 `template<IPKind> ReplacementPolicy`: 复杂度高，调用方实例化繁琐

### Decision 4: HDL 友好性通过 `static_assert` 编译期检查

**选择**: TLB 模板顶层加 `static_assert` 锁死 HDL 友好性
```cpp
template <std::size_t ENTRIES, std::size_t WAYS, ...>
class TLB : public TLBBase {
  static_assert((ENTRIES % WAYS) == 0, "ENTRIES must be multiple of WAYS");
  static_assert(ENTRIES > 0 && WAYS > 0, "must be non-zero");
  // 禁 std::optional: 由代码审查 + grep 检查
  // 禁 TLB<> 模板自身定义新 virtual 函数（基类 TLBBase 的 virtual 是唯一入口）
  // 禁 dynamic alloc: 构造后零 new/malloc
};
```

**理由**:
- 编译期阻止 HDL 不友好配置（ENTRIES=0, 非对齐等）
- 命名/接口约束通过代码审查 + `tools/verify_hdl_friendly.sh` grep 检查
- Phase 5 转换时零阻力（`TLB<64,4,27,9,1>` 直接映射 `ch::component`）

**替代方案**:
- ❌ 仅靠文档约束：易遗漏，Phase 5 转换时才发现问题
- ❌ 用 `concept` (C++20)：项目锁 C++17，concept 不可用

### Decision 5: 配置 schema 用 `topology` 字段预留 split_id

**选择**: `params.topology: "unified" | "split_id"`，**Phase 1 skeleton 仅实现 `unified`**

**理由**:
- 用户明确要求"通过配置而不是硬编码"
- GPU 典型 `unified` + 高 `num_lookup_ports`，CPU 典型 `split_id` + 中端口
- split_id 完整实现推迟（需要 I-TLB + D-TLB 两个 MultiLevelTLB 实例 + routing），骨架阶段仅占位
- JSON schema 描述完整，调用方零感知

**替代方案**:
- ❌ 骨架阶段不预留 split_id: 后续 change 改 schema 破坏 ABI
- ❌ 骨架阶段就实现 split_id: 单 change 过大

### Decision 6: ASID bits 0-16 全支持，0 = 无 ASID

**选择**: `asid_bits: 0..16`，0 表示 ASID=0 only 或 disable ASID tagging

**理由**:
- RISC-V sv39: 9 bits
- ARM: 8/16 bits
- GPU VMID: 12-16 bits
- 0 = 极简 embedded (M-mode only, 无 ASID 切换)
- TLB 模板 `ASID_BITS=0` 编译期特化为 "asid 字段 0 宽, lookup 时 asid 永远 0"

**替代方案**:
- ❌ 硬编码 9 bits: RISC-V 锁定, GPU 切换需改代码
- ❌ 16 bits 固定: 浪费存储 + 增加 valid 字段

### Decision 7: lib/ vs tlm/ 职责切分（强制 Plugin 范式合规）

**选择**: 双层目录结构，**严格遵循 D4 Plugin 范式**（`docs/architecture/plugin-framework.md` §1.1 + `multi_isa_architecture.md` §1.2）

```
ip/mmu/
├── lib/    # 纯 C++ 算法层（无 Plugin 依赖，单元测试 + HDL 友好）
│   ├── tlb_base.h
│   ├── tlb.h              # template<ENTRIES, WAYS, ...>
│   ├── tlb_entry.h
│   ├── tlb_lookup.h
│   ├── multi_level_tlb.h
│   ├── ptw.h
│   └── tlb_factory.h
├── tlm/    # 声明式 Plugin 层（依赖 cf::plugin::*，生产路径）
│   ├── MMUPlugin.h/.cpp   # 持 lib/ 算法为成员，at_stage 集成
│   └── mmu_keys.h         # Payload<T> Key 集合
└── policies/  # 策略抽象（与 lib/ 无依赖）
```

**理由**:
- **Plugin 范式强制**（D4 决策，`decision-plugin-framework-2026-06-08.md` §2.1）：所有业务逻辑 MUST 用 `at_stage()` 声明，禁止 `tick()`。`MMUPlugin` 必须是 `PluginBase` 派生，所有 TLB lookup / PTW walk 逻辑在 `at_stage` 闭包内
- **lib/ 零 Plugin 依赖**：`lib/*.h` 只 include `cf/plugin/uint_t.h`（位宽 typedef），**不** include `cf/plugin/plugin_base.h` / `pipe_builder.h` / `payload.h`。这样 lib/ 可独立编译、独立单元测试、Phase 5 CppHDL 转换零依赖
- **tlm/ 集成 lib/**：MMUPlugin 持 `std::unique_ptr<MultiLevelTLB>` + `std::unique_ptr<PTW>` 为成员变量（参考 `BranchPredictorPlugin` 的 `btb_`/`bimodal_`/`gshare_` 模式），at_stage 闭包内调用 lib/ API
- **类比已有项目结构**：`ip/cpu/core/`（框架无关算法层）+ `ip/cpu/plugins/`（Plugin 层）+ `ip/cpu/arch/riscv/`（ISA 特定）— 同样三层切分

**示例代码**:
```cpp
// ip/mmu/lib/tlb.h —— 纯 C++
template <std::size_t ENTRIES, std::size_t WAYS,
          std::size_t TAG_BITS, std::size_t ASID_BITS, std::size_t PORTS = 1>
class TLB : public TLBBase {  // lib/ 内允许 virtual
  std::array<TLBEntry, ENTRIES> entries_{};  // HDL 1:1 映射
  // ... 纯 C++ 接口
};

// ip/mmu/tlm/MMUPlugin.h —— Plugin 派生
class MMUPlugin : public cf::plugin::PluginBase {
  void setup(cf::plugin::PipeBuilder& pb) override {
    pb.declare_substage("fetch",  "tlb_lookup_ifetch", 1);
    pb.declare_substage("memory", "tlb_lookup_loadstore", 1);
    pb.declare_substage("tlb_lookup_ifetch", "ptw_l0", 1);
    pb.declare_substage("ptw_l0", "ptw_l1", 1);
    pb.declare_substage("ptw_l1", "ptw_l2", 1);
  }
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("tlb_lookup_ifetch", Phase::NORMAL, [this] {
      // 调用 lib/ API, 写 Payload (不调 tlb_->lookup() 黑盒)
    });
  }
 private:
  std::unique_ptr<MultiLevelTLB> multi_tlb_;   // lib/ 编排器
  std::unique_ptr<PTW> ptw_;                   // lib/ 接口
};
```

**替代方案**:
- ❌ 单层 `tlm/` 目录（混 lib 算法 + Plugin 入口）：单元测试需启动 PipeBuilder，HDL 转换需先剥离 Plugin 依赖
- ❌ lib/ 派生 PluginBase：违反"lib 是纯算法"原则，HDL 转换困难
- ❌ Plugin 入口类 `tick()` 推进 PTW：违反 D4（业务代码无 tick），Phase 5 转换时需重写

### Decision 8: PTW 用 substage 不用 tick()（D4 强制）

**选择**: PTW 状态用 `Payload<T>` 跨周期传递，3 个 `at_stage` 接力，**禁止业务 `tick()`**

```cpp
// ip/mmu/tlm/mmu_keys.h
namespace cf::ip::mmu::payload {
  template <typename T>
  struct mmu_keys {
    static inline cf::plugin::Payload<T>       VADDR{"mmu.vaddr"};
    static inline cf::plugin::Payload<T>       PADDR{"mmu.paddr"};
    static inline cf::plugin::Payload<bool>    PTW_ACTIVE{"mmu.ptw_active"};
    // Sv39 3 级页表 walk 中间结果
    static inline cf::plugin::Payload<uint64_t> PTW_L0_RAW{"mmu.ptw_l0_raw"};
    static inline cf::plugin::Payload<uint64_t> PTW_L1_RAW{"mmu.ptw_l1_raw"};
    static inline cf::plugin::Payload<uint64_t> PTW_L2_RAW{"mmu.ptw_l2_raw"};
    static inline cf::plugin::Payload<uint8_t>  PTW_FAULT{"mmu.ptw_fault"};
  };
}

// ip/mmu/tlm/MMUPlugin.h
void setup(cf::plugin::PipeBuilder& pb) override {
  // PTW 3 级子流水 (parent 必须是已存在的 logic_stage 或已声明的 substage)
  // 与 RiscvMulPlugin 同构 (mul.h:63-67 declare_substage("execute", "mul_s1", 1))
  pb.declare_substage("fetch", "tlb_lookup_ifetch", 1);  // parent: 已有 logic_stage
  pb.declare_substage("memory", "tlb_lookup_loadstore", 1);  // parent: 已有 logic_stage
  pb.declare_substage("tlb_lookup_ifetch", "ptw_l0", 1);  // parent: 上一 substage
  pb.declare_substage("ptw_l0", "ptw_l1", 1);  // chain 父化
  pb.declare_substage("ptw_l1", "ptw_l2", 1);  // chain 父化
}

void build(cf::plugin::PipeBuilder& pb) override {
  // Logical stage 0: tlb_lookup_ifetch (TLB hit 同步, miss 触发 PTW)
  pb.at_stage("tlb_lookup_ifetch", Phase::NORMAL, [this] { ... });
  
  // Logical stage 1-3: PTW 3 级 walk (每级读 1 个 PTE)
  pb.at_stage("ptw_l0", Phase::NORMAL, [this] {
    // 读 L0 PTE, 写 PTW_L0_RAW
  });
  pb.at_stage("ptw_l1", Phase::NORMAL, [this] {
    // 读 L1 PTE, 写 PTW_L1_RAW
  });
  pb.at_stage("ptw_l2", Phase::NORMAL, [this] {
    // 读 L2 PTE, 写 PADDR, 清 PTW_ACTIVE
    // multi_tlb_->refill_from_ptw(...);  // 多级回填
  });
  
  // PTW 进行中 stall 下游 (CtrlLink 声明式)
  // 推迟到 mmu-tlb-ptw-impl 实施
}
```

**Phase 0 framework 备注**（重要 — 影响实施者理解）:
- 当前 `pb.run()` 是**单次遍历所有 `at_stage` 回调**（按注册顺序），**无 cycle 精度**（见 `include/cf/plugin/pipe_builder.h:88-100` + `docs/methodology/plugin-style-design-methodology-v1.md` §B3-D4.2/D4.3）
- PTW 的 3 级 logical stage 拆分**不是 cycle 精确调度**，是为 Phase 6 cycle-scheduling 框架升级预留的**逻辑结构**（B3-L1 推迟任务）
- 当前 verification 通过 `pb.stage_names()` 断言 logical stage 拓扑序，**不**通过 cycle 计数或 cycle 时序断言

**理由**:
- **D4 强制**（`plugin-framework.md` §2.1）: `PluginBase::tick() = delete`, 派生类**不可定义**同名函数
- **可观测**: `pb.stage_names()` 直接列出 `ptw_l0/l1/l2`, 调试/可视化清晰
- **可重放**: `pb.run()` 确定性, 同一组输入多次 run 结果一致
- **HDL 转换友好**: at_stage 3 级 sub-pipe 在 Phase 6 cycle 调度框架下直接映射 `ch_module` 3 级流水线, 1:1
- **与现有范式一致**: `RiscvMulPlugin::setup()` 用 `declare_substage("execute", "mul_s1", 1)` 同样模式, PTW 把 substage 数从 2 增到 3

**替代方案**:
- ❌ `PageTableWalker` 类有 `tick()` 推进状态机: 违反 D4 编译期禁止
- ❌ `PageTableWalker` 是 PluginBase 派生, `at_stage("ptw", ...)` 单 stage + 内部循环: 单 cycle 推进多步违反声明式意图
- ❌ 用 CtrlLink 跨越多 cycle: CtrlLink 是控制流机制, 不应承载数据流

### Decision 9: HDL 友好约束的边界（lib/ 严格，tlm/ 宽松）

**选择**: HDL 友好性约束**只对 `lib/` 强制**，`tlm/` 允许 Plugin 框架的 `virtual`/`any_cast`

**理由**:
- Phase 5 CppHDL 转换只针对 `lib/*.h` 模板类（`TLB<>` / `MultiLevelTLB` / `PTW` 实际是 TLM-only 接口）
- `tlm/MMUPlugin` 是 Plugin 框架集成层，**不**直接生成 HDL — Plugin → Component 转换在 Phase 6 才实施
- 因此 `lib/TLBBase` 用 `virtual`（多态容器需要）**OK**；`lib/TLB<>` 模板**禁止** `virtual`（HDL 转换对象）
- `tlm/MMUPlugin` 调 `cf::plugin::Payload<T>::operator()`（类型安全取值，HDL 不转换）

**约束分级**:

| 层 | `virtual` 允许 | `std::optional` 允许 | 动态分配 | std::vector |
|----|--------------|---------------------|---------|-----------|
| `lib/TLBBase` | ✓ (多态基类) | ✗ | ✗ | ✗ |
| `lib/TLB<>` 模板 | ✗ (HDL 1:1) | ✗ | ✗ | ✗ (用 std::array) |
| `lib/MultiLevelTLB` | ✗ (直接持 TLB 实例) | ✗ | ✗ (构造时一次性 resize, 运行期不增删) | ✓ (构造时一次性 resize 持 N 个 TLB unique_ptr, 运行期容量不变) |
| `lib/PTW` 接口 | ✓ (Strategy 模式) | ✗ | ✗ | ✗ |
| `tlm/MMUPlugin` | ✓ (PluginBase) | ✓ (lib 内部) | ✓ (Plugin 持成员) | ✓ |

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| 模板实例化组合爆炸（entries × ways × asid_bits × ports） | 工厂只特化 5-7 组典型组合，参数范围通过 JSON schema 限制 |
| MultiLevelTLB coherence 协议 bug 影响所有级 | 独立单元测试覆盖 shadow fill / 反向失效 / PTW refill / ASID 切换 4 个场景 |
| TLB 模板编译慢 | HEADER_ONLY 标记 + 前向声明 + 显式 instantiate 控制 |
| 现有 `ip/cpu/plugins/mmu.h` 占位类破坏 M3 阶段 cpu 测试 | 保留 `using MMUPlugin = cf::ip::mmu::MMUPlugin` 类型别名（向后兼容），`RiscvMMUPlugin` 仅为新名 |
| HDL 友好性约束被无意违反 | `static_assert` 锁死硬约束 + `tools/verify_hdl_friendly.sh` 软约束 grep 检查 |
| split_id 拓扑未实装但 schema 包含 | 在 schema `description` 明确标注 "Phase 1 skeleton only `unified`" |
| PageTableWalker 仅接口无实装 | 接口稳定性优先于算法完整性（与 cache-policy-foundation 同构） |
| RISC-V 适配器（`RiscvMMUPlugin`）占位会误导后续实施者 | 头注释明确写"骨架阶段占位，实装推迟到 mmu-tlb-ptw-impl" |
| 4 种策略 + 工厂增加 DSE 扫描时间 | 工厂 `create()` 用 `if-else` 链仅 4-7 分支，编译期完全展开 |
| lib/ 与 tlm/ 切分后类成员易跨层引用 | `static_assert` + `grep -L` 检查 `lib/*.h` 0 include `cf/plugin/plugin_base.h` / `pipe_builder.h` |

## Migration Plan

本 change 为新增 IP，**无迁移负担**。落地步骤：

1. **前置**: 当前 main 分支 ctest 21/21 PASS（基线）
2. **新增** `ip/mmu/` 目录及子文件（无任何删除/重命名）
3. **重构** `ip/cpu/plugins/mmu.h` 占位 → `RiscvMMUPlugin`（保留 `MMUPlugin` 类型别名）
4. **扩展** `bundles/mem_bundles.h` 追加 `TlbReq`/`TlbResp`（既有 6 个 Bundle 不动）
5. **新增** `tests/mmu/` 下 4 个测试文件
6. **修改** `ip/README.md` STATUS 表加 1 行
7. **验证**: `tools/run_chipforge_tests.sh` → 21/21 基线 + ~12/12 增量 = 33/33 PASS
8. **回滚**: 单一 commit revert 即可（新增 IP 无外部依赖）

## Open Questions

1. **PageTableWalker 接口的 `PTE` 类型归类**: 是放 `ip/mmu/pte.h` 还是 `ip/cpu/arch/riscv/pte.h`（ISA 特定）？
   - 当前倾向: `ip/mmu/pte.h` 内提供 `PTEBase`（ISA-无关），`ip/cpu/arch/riscv/pte.h` 提供 `RiscvPTE : PTEBase` 解码
   - 待 `mmu-tlb-ptw-impl` 决策

2. **ASID=0 是 "global" 还是 "no asid"**:
   - RISC-V: ASID=0 是有效值
   - 一些设计: ASID=0 是 global 标记
   - 骨架阶段不强制，待 `RiscvMMUPlugin` 决策

3. **MMUPlugin 持 PTW 还是 MultiLevelTLB 持 PTW**:
   - 当前倾向: MMUPlugin 持 PTW（编排器职责清晰）
   - 替代: MultiLevelTLB 持 PTW（coherence 协议完整）
   - 待 `mmu-tlb-ptw-impl` 决策

4. **`TlbReq.id` 字段是否保留**: 8-bit id 跟 `MemReq.id` 一致；skeleton 阶段保留，TLB 单级查询无 id 需求但 bundle 一致性需要
   - 当前保留，理由: bundle 三层分层（ADR-024）

5. **`bundles/mem_bundles.h` 还是新建 `ip/mmu/bundles/tlb_bundles.h`**:
   - 当前倾向: 追加到 `mem_bundles.h`（与 L1CachePluginBundle 同文件）
   - 替代: 独立文件（IP 自治）
   - 当前决定: 追加（同文件），理由是 bundle 数量少且共享

6. **lib/ 目录命名 vs mmulib/ 命名冲突**:
   - 候选: `ip/mmu/lib/` / `ip/mmulib/` / `ip/mmu/libmmu/`
   - 当前: `ip/mmu/lib/`（与 `ip/cpu/core/` 命名风格对齐，core 同样是"框架无关算法层"）
   - 待 review 反馈

7. **PTW 3 个 substage 在 `setup()` 声明还是在 `cpu_factory.h` 声明**:
   - 当前倾向: MMUPlugin::setup() 内 `declare_substage()`（Plugin 自描述，与 MulPlugin 同构）
   - 替代: cpu_factory.h 的 TopologyBuilder 集中声明（统一拓扑管理）
   - 当前: MMUPlugin 内（Plugin 自描述原则）

8. **`mmu_keys.h` 的 Payload Key 命名空间**:
   - 候选 A: `cf::ip::mmu::payload::mmu_keys<T>`（独立命名空间）
   - 候选 B: 追加到 `cf::cpu::arch::riscv::payload_keys_riscv<T>`（CPU ISA 命名空间下）
   - 当前: 候选 A（MMU 是独立 IP，key 应在 MMU 命名空间，与 IP 边界一致）
