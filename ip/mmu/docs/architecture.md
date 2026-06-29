# MMU 微架构

## 1. 总体数据流

```
vPC ───► [IBusPlugin] ──fetch stage──► 写 pl::PC ──► MMUPlugin::tlb_lookup_ifetch
                                                          │
                                                          ▼
                                              ┌────────────────────┐
                                              │ MultiLevelTLB      │
                                              │ ┌──────────────┐   │
                                              │ │ L0 TLB (8/8) │   │ hit 1-cycle
                                              │ │ ~1 cycle     │◄──┤
                                              │ └──────────────┘   │
                                              │ ┌──────────────┐   │ miss → trigger PTW
                                              │ │ L1 TLB (64/4)│   │
                                              │ │ ~2 cycles    │   │
                                              │ └──────────────┘   │
                                              │ ┌──────────────┐   │
                                              │ │ PTW (3-cycle)│   │ Sv39 3-level walk
                                              │ │ Sv39: L0/L1/L2│   │
                                              │ └──────────────┘   │
                                              └────────────────────┘
                                                          │
                                                          ▼
                                              写 pl::PADDR (hit) / PTW_ACTIVE=0
                                                          │
                                                          ▼
                                                     IBusPlugin 读 pl::PADDR
                                                     ┌──────────────────┐
                                                     │  L1I Cache       │
                                                     └──────────────────┘
```

类似地，LSU 数据通路走 `tlb_lookup_loadstore` 阶段。

## 2. 多级 TLB 编排（MultiLevelTLB 协议）

详见 [../docs/architecture §3] + 配套 spec `mmu-tlb-coherence-protocol/spec.md`。

### 2.1 Lookup 路径
```
MMUPlugin::at_stage("tlb_lookup_ifetch"):
  result = multi_tlb_->lookup(vpc, asid)
  if result.hit:
    写 pl::PADDR = result.paddr
    清 pl::PTW_ACTIVE
  else:
    写 pl::PTW_ACTIVE = 1
    写 pl::PTW_VADDR = vpc
    写 pl::PTW_ASID = asid
    (下游 CtrlLink halt_when 触发 stall)
```

### 2.2 PTW 路径（logical stage 接力）
```
at_stage("ptw_l0"): 读 L0 PTE → 写 pl::PTW_L0_RAW
at_stage("ptw_l1"): 读 L1 PTE → 写 pl::PTW_L1_RAW
at_stage("ptw_l2"): 读 L2 PTE → 写 pl::PADDR + 清 PTW_ACTIVE + multi_tlb_->refill_from_ptw()
```

**重要**：当前 `pb.run()` 是**单次遍历所有 `at_stage` 回调**（无 cycle 精度，Phase 0 framework 限制）。3 个 ptw_l0/l1/l2 是**逻辑阶段拆分**，为 Phase 6 cycle-scheduling 框架升级预留结构。详见 [docs/methodology/plugin-style-design-methodology-v1.md §B3-D4.2]。

## 3. HDL 友好性约束（Phase 5 CppHDL 转换准备）

### 3.1 硬约束（编译期 / 静态检查）

| 约束 | 验证手段 |
|------|---------|
| ENTRIES % WAYS == 0 | `static_assert` |
| ENTRIES > 0 && WAYS > 0 | `static_assert` |
| `TLB<>` 模板自身不定义新 virtual | 基类 `TLBBase` 是唯一 virtual 入口 |
| 全 `std::array`，无 `std::vector`（在 TLB 模板内） | 代码审查 + `tools/verify_hdl_friendly.sh` |
| 无 `std::optional` / `std::variant` | grep 0 匹配 |
| 无动态分配（new/malloc） | grep 0 匹配 |

### 3.2 软约束（lib/ 严格，tlm/ 宽松）

| 层 | `virtual` 允许 | `std::optional` 允许 | 动态分配 | std::vector |
|----|--------------|---------------------|---------|-----------|
| `lib/TLBBase` | ✓ (多态基类) | ✗ | ✗ | ✗ |
| `lib/TLB<>` 模板 | ✗ (HDL 1:1) | ✗ | ✗ | ✗ (用 std::array) |
| `lib/MultiLevelTLB` | ✗ (直接持 TLB 实例) | ✗ | ✗ (构造时一次性 resize, 运行期不增删) | ✓ (构造时一次性 resize 持 N 个 TLB unique_ptr, 运行期容量不变) |
| `lib/PTW` 接口 | ✓ (Strategy 模式) | ✗ | ✗ | ✗ |
| `tlm/MMUPlugin` | ✓ (PluginBase) | ✓ (lib 内部) | ✓ (Plugin 持成员) | ✓ |

### 3.3 Phase 5 1:1 映射路径

```
lib/TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>
            ↓ (Phase 5 CppHDL codegen)
        ch::Component
            ↓
        Verilog (Verilator 仿真 / 综合)
```

> **Phase 5 HDL 1:1 映射验证推迟到 `mmu-tlb-ptw-impl` 之后**（forward-looking claim，骨架阶段通过 `tools/verify_hdl_friendly.sh` 静态检查 + `static_assert` 锁死硬约束）。

## 4. GPU/CPU 兼容性策略

| 维度 | 典型 CPU (RISC-V) | 典型 GPU | 配置化字段 |
|------|-------------------|---------|-----------|
| `topology` | split_id | unified | `params.topology` |
| `asid_bits` | 9 (sv39) | 12-16 (VMID) | `params.asid_bits` |
| `num_lookup_ports` | 1-2 (in-order/OoO) | 4-8 (multi-warp) | `levels[].num_lookup_ports` |
| `supported_page_sizes` | [4KB, 2MB, 1GB] | [4KB, 64KB, 2MB, 1GB] | `params.supported_page_sizes` |
| `ptw_max_inflight` | 1-2 | 4-8 (prefetch) | `params.ptw_max_inflight` |
| `ptw_strategy` | on-demand | prefetch-heavy | (推迟) |

**切换零代码改动** —— 只改 JSON config，lib/ 算法层完全不感知 CPU vs GPU。

## 5. 工厂模式（TLBFactory）

```
JSON config (entries × ways × asid_bits × ports)
            ↓
TLBFactory::create(TLBLevelConfig)
            ↓
模板特化选择 (if-else 链, 4-7 组典型组合)
            ↓
TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS> 实例
            ↓
std::unique_ptr<TLBBase> (多态容器, MultiLevelTLB 持有)
```

**注意**："配置化"是**编译期枚举式** —— `TLBFactory` 在 `tlb_factory.cpp` 的 `if-else` 链显式实例化典型组合，新增组合需修改工厂 + 重编译，**不**是运行时无限制配置。

## 6. 替换策略接口（policies/）

```cpp
template <std::size_t ENTRIES, std::size_t WAYS>
class TLBReplacementPolicy {
 public:
  virtual ~TLBReplacementPolicy() = default;
  virtual void on_access(uint32_t set, uint32_t way) = 0;
  virtual uint32_t select_victim(uint32_t set) = 0;
  virtual void on_insert(uint32_t set, uint32_t way) = 0;
  virtual std::string name() const = 0;
  static std::unique_ptr<TLBReplacementPolicy> create(const std::string& name);
};
```

4 种实现：None / FIFO / LRU / RRIP。namespace 隔离：`cf::ip::mmu::policies::` 与 `cf::ip::cache::policies::` 独立。

## 7. lib/ vs tlm/ 分层（核心架构决策）

### 7.1 为什么需要双层

| 关注点 | lib/ | tlm/ |
|--------|------|------|
| HDL 1:1 友好 | ✅ 直接映射 `ch::Component` | ❌ Plugin 框架不直接转 HDL |
| 单元测试 | ✅ 无 PipeBuilder 依赖 | ❌ 需启动 PipeBuilder |
| 编译期类型检查 | ✅ 模板特化 | ⚠️ 动态多态 |
| 业务集成 | ⚠️ 仅纯 C++ API | ✅ `at_stage()` 声明式集成 |

### 7.2 依赖方向

```
tlm/MMUPlugin ──include──> lib/{TLB, MultiLevelTLB, PTW, TLBFactory}
lib/*.h       ──include──> cf::plugin/uint_t.h (位宽 typedef)
lib/*.h       ──不 include──> cf::plugin/{plugin_base, pipe_builder, payload, ...}
```

**`lib/*.h` 0 引用 `cf::plugin::*` 框架符号**（除位宽 typedef），通过 `tools/verify_hdl_friendly.sh` 静态检查。

### 7.3 与项目其他 IP 的关系

| 参照 | 关系 |
|------|------|
| `ip/cpu/core/` | 框架无关算法层（与 `ip/mmu/lib/` 同层） |
| `ip/cpu/plugins/` | ISA 无关 Plugin 层（与 `ip/mmu/tlm/` 同层） |
| `ip/cpu/arch/riscv/` | ISA 特定 adapter（与 `ip/cpu/plugins/mmu.h::RiscvMMUPlugin` 同层） |
| `ip/cache/policies/` | 替换策略抽象（与 `ip/mmu/policies/` 同构） |
| `ip/cache/tlm/L1CachePlugin.h` | Plugin-style 业务 IP 先例（同构） |

## 8. 实施里程碑

| 阶段 | 内容 | change |
|------|------|--------|
| **mmu-ip-skeleton** (本 change) | 目录骨架 + lib/ 接口 + tlm/ Plugin 入口 + policies/ 策略 + Config schema + 5 测试文件 | 2026-06-29 |
| **mmu-tlb-ptw-impl** (下一 change) | TLB lookup/insert 算法 + PTW Sv32/Sv39/Sv48 解码 + MMUPlugin at_stage 闭包实装 + CtrlLink halt_when PTW stall | (待) |
| Phase 5+ | CppHDL 转换 + rtl/ 目录填充 | (Phase 5+) |
