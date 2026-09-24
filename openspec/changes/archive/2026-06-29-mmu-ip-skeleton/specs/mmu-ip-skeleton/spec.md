# mmu-ip-skeleton

## Purpose

建立 `ip/mmu/` IP 目录骨架（与 `ip/cache/` 同构），落地 TLB 模板、MultiLevelTLB 编排器、4 种替换策略、PageTableWalker 接口、配置 schema、HDL 友好约束、GPU/CPU 双兼容配置。骨架阶段不实现 TLB/PTW 算法（推迟到 `mmu-tlb-ptw-impl`），只锁定接口与结构。

## ADDED Requirements

### Requirement: ip/mmu/ 目录结构对齐 ip/cache/

`ip/mmu/` MUST 包含与 `ip/cache/` 同构的标准目录：

- `ip/mmu/README.md` (IP 设计文档)
- `ip/mmu/STATUS.md` (状态: 骨架/规划中)
- `ip/mmu/tlm/` (CppTLM 模型目录)
- `ip/mmu/rtl/` (CppHDL RTL 目录，Phase 5+ 沿用，可保留 .gitkeep)
- `ip/mmu/configs/params_schema.json` (JSON Schema draft-07)
- `ip/mmu/docs/{README.md, architecture.md, configuration.md, integration.md}` (最小文档子结构)
- `ip/mmu/policies/` (替换策略子目录)
- `ip/mmu/test/` (预留，与 `ip/cache/test/` 同形，骨架阶段可保留 .gitkeep 或删除)

`ip/mmu/STATUS.md` MUST 标注 "🟡 骨架阶段 (mmu-ip-skeleton, 2026-06-24)，算法实装推迟到 mmu-tlb-ptw-impl"。

#### Scenario: ip/mmu/ 目录存在且符合标准结构
- **WHEN** `ls -la ip/mmu/` 执行
- **THEN** MUST 存在 `README.md`, `STATUS.md`, `tlm/`, `rtl/`, `configs/`, `docs/`, `policies/` 七个条目
- **AND** `ip/mmu/STATUS.md` MUST 含 "骨架" 状态标记
- **AND** `ip/mmu/configs/params_schema.json` MUST 是合法 JSON Schema draft-07

### Requirement: TLB 模板化与 HDL 友好约束

`cf::ip::mmu::TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` MUST 是模板类，所有存储 MUST 用 `std::array`，所有位宽字段 MUST 用 `cf::plugin::uint_t<N>`，禁止使用 `std::optional`/`std::variant`/`virtual`/动态分配。

#### Scenario: TLB 模板编译期静态检查
- **WHEN** 用 `static_assert((ENTRIES % WAYS) == 0)` 实例化 TLB
- **THEN** 当 ENTRIES 不是 WAYS 的整数倍时 MUST 编译失败
- **AND** 当 ENTRIES == 0 或 WAYS == 0 时 MUST 编译失败
- **AND** `TLB<64, 4, 27, 9, 1>` MUST 编译通过

#### Scenario: TLB 模板存储为 std::array
- **WHEN** 编译 `TLB<64, 4, 27, 9, 1>` 类
- **THEN** `sizeof(TLB<64,4,27,9,1>)` MUST 是编译期常量
- **AND** 不得出现 `std::vector` 成员
- **AND** 不得出现 `new`/`malloc` 在 lookup/insert 路径

### Requirement: lib/ 与 tlm/ 严格职责切分（Plugin 范式合规）

`ip/mmu/` MUST 含两个并列子目录，**职责严格分离**（参考 `ip/cpu/core/` vs `ip/cpu/plugins/` 的三层切分）：

- `ip/mmu/lib/` —— 纯 C++ 算法层，**不依赖** `cf::plugin::PluginBase`/`PipeBuilder`/`Payload`
  - 唯一允许 include 的 `cf::plugin/*` 头：`cf/plugin/uint_t.h`（位宽 typedef，仅 POD 类型定义）
  - 内部 `TLB` / `MultiLevelTLB` / `PTW` / `TLBFactory` / `TLBBase` / `TLBEntry` / `TLBLookup` 全部放这里
  - 可独立单元测试（无需 PipeBuilder 启动）
  - Phase 5 CppHDL 转换对象**仅限 lib/ 内**的模板类
- `ip/mmu/tlm/` —— 声明式 Plugin 层，**依赖** `cf::plugin::PluginBase` + `PipeBuilder`
  - `MMUPlugin.{h,cpp}` 派生自 `cf::plugin::PluginBase`
  - `mmu_keys.h` 含 `Payload<T>` Key 集合（与 `ip/cpu/arch/riscv/payload_riscv.h` 同构）
  - 所有业务逻辑（MUST）通过 `at_stage()` 声明；**禁止业务 `tick()`**（D4 决策，编译期强制）
  - 持 `lib/` 内算法对象为成员变量（参考 `BranchPredictorPlugin` 的 `btb_`/`bimodal_` 模式）
- `ip/mmu/policies/` —— 替换策略（与 `lib/` 无依赖，与 `tlm/` 无依赖）

#### Scenario: lib/ 头文件 0 引用 Plugin 框架
- **WHEN** `grep -rn "cf/plugin/plugin_base.h\|cf/plugin/pipe_builder.h\|cf/plugin/payload.h\|cf/plugin/pipe_node.h\|cf/plugin/ctrl_link.h" ip/mmu/lib/` 执行
- **THEN** MUST 0 匹配（lib/ 不引用 Plugin 框架）
- **AND** 唯一允许的 `cf::plugin` include 是 `cf::plugin::uint_t<N>` 位宽 typedef

#### Scenario: tlm/ MMUPlugin 派生自 PluginBase
- **WHEN** `class MMUPlugin : public cf::plugin::PluginBase` 编译
- **THEN** MUST 编译通过
- **AND** `MMUPlugin` MUST override `setup()` + `build()`
- **AND** 不得定义 `tick()`（编译期被 `PluginBase::tick() = delete` 阻止）

#### Scenario: lib/ 单元测试不启动 PipeBuilder
- **WHEN** 编译 `tests/mmu/test_tlb_unit.cpp`（仅测 `lib/tlb.h`）
- **THEN** MUST 编译通过，**不** link `PipeBuilder`/`PayloadStore` 任何符号
- **AND** MUST 不引用 `cf::plugin::PipeBuilder` / `Payload<T>` 类

### Requirement: MMUPlugin 用 at_stage 声明所有业务逻辑（D4 强制）

`MMUPlugin::build()` MUST 用 `at_stage()` 注册所有翻译逻辑，**禁止**用普通成员函数 + tick() 推进。

TLB lookup 路径：1 个 `at_stage("tlb_lookup_<ifetch|loadstore>", ...)` 闭包（1-cycle TLB hit）

PTW 路径：3 个 `at_stage("ptw_l0/l1/l2", ...)` 闭包（Sv39 3 级 walk），在 `setup()` 用 `declare_substage()` 声明子阶段

Miss 处理：PTW 期间 stall 下游用 `CtrlLink::halt_when()`（推迟到 `mmu-tlb-ptw-impl` 实施）

#### Scenario: TLB hit 逻辑阶段同步
- **WHEN** `MMUPlugin::build()` 注册 `at_stage("tlb_lookup_ifetch", Phase::NORMAL, [...])` 闭包
- **AND** 闭包内调用 `multi_tlb_->lookup(vpc, asid)` 返回 hit
- **THEN** 闭包 MUST 写 `pl::PADDR` (含 paddr) + 清 `pl::PTW_ACTIVE`
- **AND** 不写 `pl::PTW_L0_RAW` / `pl::PTW_L1_RAW` / `pl::PTW_L2_RAW`（PTW 未启动）
- **AND** `pl::PTW_FAULT` 保持 0 (无 page fault)

#### Scenario: TLB miss 触发 PTW 3 级 logical stage walk
- **WHEN** `MMUPlugin::build()` 注册 `at_stage("tlb_lookup_ifetch", ...)` + `at_stage("ptw_l0", ...)` + `at_stage("ptw_l1", ...)` + `at_stage("ptw_l2", ...)` 4 个闭包
- **AND** 第一个闭包 `lookup()` 返回 miss
- **THEN** 第一个闭包 MUST 写 `pl::PTW_ACTIVE=1` + `pl::PTW_VADDR=vpc` + `pl::PTW_ASID=asid`
- **AND** ptw_l0 闭包 MUST 写 `pl::PTW_L0_RAW` (Sv39 L0 PTE)
- **AND** ptw_l1 闭包 MUST 写 `pl::PTW_L1_RAW` (Sv39 L1 PTE)
- **AND** ptw_l2 闭包 MUST 写 `pl::PTW_L2_RAW` (Sv39 L2 PTE) + 写 `pl::PADDR=paddr` + 清 `pl::PTW_ACTIVE=0`
- **AND** ptw_l2 闭包 MUST 调用 `multi_tlb_->refill_from_ptw(...)` 回填多级 TLB
- **AND** 4 个闭包的 logical stage MUST 按 `declare_substage` 父化链顺序排列: `tlb_lookup_ifetch → ptw_l0 → ptw_l1 → ptw_l2`

> **Phase 0 framework 备注**：`pb.run()` 单次遍历所有 `at_stage` 回调，无 cycle 精度（见 `include/cf/plugin/pipe_builder.h` + `docs/methodology/plugin-style-design-methodology-v1.md` §B3-D4.2/D4.3）。PTW 的 3 级子阶段在 Phase 0 是**逻辑拆分**，为 Phase 6 cycle-scheduling 框架升级预留结构。当前 verification 通过 `pb.stage_names()` 断言 logical stage 拓扑序，不通过 cycle 计数。

#### Scenario: pb.stage_names() 含 5 个 logical stage
- **WHEN** `pb.build()` 完成
- **THEN** `pb.stage_names()` MUST 包含至少以下 5 个 entry（顺序可断言，名称必须断言）:
  - `tlb_lookup_ifetch`
  - `tlb_lookup_loadstore`
  - `ptw_l0`
  - `ptw_l1`
  - `ptw_l2`
- **AND** declare_substage 父化链 MUST 是 `fetch → tlb_lookup_ifetch → ptw_l0 → ptw_l1 → ptw_l2`（与 spec Scenario "TLB miss" 的 logical stage 顺序一致）

#### Scenario: MMUPlugin 0 业务 tick()
- **WHEN** 编译 `MMUPlugin.cpp`
- **THEN** 不得有 `void MMUPlugin::tick()` 成员函数（编译期被 `PluginBase::tick() = delete` 阻止）
- **AND** 不得有 `while/for` 循环在 `build()` 闭包内推进 PTW 状态
- **AND** 静态检查 `grep -rn "::tick()" ip/mmu/tlm/MMUPlugin.cpp` MUST 0 匹配（D4 编译期 + grep 双保险）

### Requirement: mmu_keys.h 提供 Payload Key 集合

`ip/mmu/tlm/mmu_keys.h` MUST 提供 `Payload<T>` Key 集合，命名空间 `cf::ip::mmu::payload`，至少含以下 Key:

| Key | 类型 | 用途 |
|-----|------|------|
| `VADDR<T>` | `uint_t<64>` | 虚地址 |
| `PADDR<T>` | `uint_t<64>` | 物理地址 |
| `PERMS` | `uint_t<8>` | 权限位 (R/W/X/U) |
| `PTW_ACTIVE` | `bool_t` | PTW 状态机是否活跃 |
| `PTW_VADDR<T>` | `uint_t<64>` | PTW 起始虚地址 |
| `PTW_ASID` | `uint_t<16>` | PTW 起始 ASID |
| `PTW_L0_RAW` | `uint_t<64>` | L0 PTE 原始值 |
| `PTW_L1_RAW` | `uint_t<64>` | L1 PTE 原始值 |
| `PTW_L2_RAW` | `uint_t<64>` | L2 PTE 原始值 |
| `PTW_FAULT` | `uint_t<8>` | PTW 异常码 (0=无, 1/2/3=page fault, 5/7=access fault) |

#### Scenario: mmu_keys.h 提供完整 Key 集合
- **WHEN** `#include "ip/mmu/tlm/mmu_keys.h"` 编译
- **THEN** `cf::ip::mmu::payload::mmu_keys<uint32_t>::PADDR` MUST 存在
- **AND** 全部 10 个 Key MUST 编译通过
- **AND** Key 是 `inline` 全局静态对象（零链接冲突）

#### Scenario: mmu_keys 与 payload_riscv_keys 独立
- **WHEN** 同时使用 `mmu_keys` + `payload_keys_riscv`
- **THEN** Key 名称 MUST 不冲突（如 `MMU::PADDR` vs `RISCV::BRANCH_TARGET`）
- **AND** 编译时 MUST 0 冲突

### Requirement: TLBLookup 字段集合契约（lib/ 接口稳定）

`cf::ip::mmu::TLBLookup` (在 `ip/mmu/lib/tlb_lookup.h`) MUST 含以下字段（接口稳定性优先于算法完整性）:

```cpp
struct TLBLookup {
  cf::plugin::bool_t    hit{false};        // valid bit 替代 std::optional
  cf::plugin::uint_t<64> paddr{0};         // 物理地址 (hit 时有效)
  cf::plugin::uint_t<8>  perms{0};         // 权限位 R/W/X/U (RISC-V 4 位 + reserved 4 位)
  cf::plugin::bool_t    fault{false};      // 访问是否触发 fault
  cf::plugin::uint_t<4>  fault_code{0};    // 0=无, 1=page fault, 5=access fault 等
};
```

字段 MUST 用 `cf::plugin::uint_t<N>`，无 `std::optional`。`hit` 是 valid bit，`paddr`/`perms` 仅在 `hit==true` 时有意义。

#### Scenario: TLBLookup 字段集合 + 编译通过
- **WHEN** `#include "ip/mmu/lib/tlb_lookup.h"` 编译
- **THEN** 5 个字段 MUST 全部存在（`hit`/`paddr`/`perms`/`fault`/`fault_code`）
- **AND** 全部使用 `cf::plugin::uint_t<N>` 或 `cf::plugin::bool_t`
- **AND** `sizeof(TLBLookup) % alignof(uint64_t) == 0` (8 字节对齐)

#### Scenario: TLBLookup 默认构造为 miss
- **WHEN** `TLBLookup lookup;` (默认构造)
- **THEN** `lookup.hit == false`
- **AND** `lookup.fault == false`
- **AND** `lookup.fault_code == 0`
- **AND** `lookup.paddr == 0`
- **AND** `lookup.perms == 0`

### Requirement: PTW 接口方法签名契约（lib/ 接口稳定）

`cf::ip::mmu::PTW` (在 `ip/mmu/lib/ptw.h`) MUST 暴露以下接口（骨架阶段实现可 stub，**接口签名必须稳定**）:

```cpp
class PTW {
 public:
  using WalkCallback = std::function<void(uint64_t paddr, uint8_t perms)>;
  using FaultCallback = std::function<void(uint8_t fault_code)>;
  
  void start_walk(uint64_t vaddr, uint16_t asid,
                  WalkCallback on_success, FaultCallback on_fault);
  void advance(uint64_t pte_raw, std::size_t level);  // 推进 1 步 (由 MMUPlugin 在 at_stage 闭包内调用)
  bool is_busy() const;
  bool is_done() const;
  uint64_t result_paddr() const;       // 完成后返回 paddr
  uint8_t  result_perms() const;       // 完成后返回 perms
  uint8_t  result_fault() const;       // 完成后返回 fault_code, 0=无
};
```

PTW 类 MUST NOT 派生 `cf::plugin::PluginBase`、MUST NOT 调 `at_stage`、MUST NOT 持 `PipeNode`。仅纯 C++ 接口。

#### Scenario: PTW 接口方法签名编译通过
- **WHEN** `#include "ip/mmu/lib/ptw.h"` 编译
- **THEN** `class PTW` MUST 暴露 6 个公共方法（`start_walk`/`advance`/`is_busy`/`is_done`/`result_paddr`/`result_perms`/`result_fault`）
- **AND** MUST NOT include `cf/plugin/plugin_base.h` 或 `cf/plugin/pipe_builder.h`

#### Scenario: PTW 类不继承 PluginBase
- **WHEN** 编译 `ip/mmu/lib/ptw.h`
- **THEN** `class PTW` MUST NOT 派生 `cf::plugin::PluginBase`
- **AND** `static_assert(std::is_base_of_v<cf::plugin::PluginBase, cf::ip::mmu::PTW> == false)` MUST 编译通过

### Requirement: MMUPlugin 构造签名契约（tlm/ 接口稳定）

`cf::ip::mmu::MMUPlugin` (在 `ip/mmu/tlm/MMUPlugin.h`) MUST 暴露以下构造签名（接口稳定性优先）:

```cpp
class MMUPlugin : public cf::plugin::PluginBase {
 public:
  struct TLBConfig {
    std::string name;
    std::size_t entries;
    std::size_t associativity;
    std::size_t num_lookup_ports = 1;
    std::size_t lookup_latency_cycles = 1;
    std::string replacement_policy = "LRU";  // None/FIFO/LRU/RRIP
  };
  struct PTWConfig {
    std::size_t max_inflight = 2;
  };
  
  MMUPlugin(SvMode mode,
            std::vector<TLBConfig> levels_cfg,
            PTWConfig ptw_cfg);
  
  void setup(cf::plugin::PipeBuilder& pb) override;
  void build(cf::plugin::PipeBuilder& pb) override;
};
```

`SvMode` 是 `cf::ip::mmu::SvMode` 枚举（`Bare`/`Sv32`/`Sv39`/`Sv48`），位于 `ip/mmu/lib/ptw.h`。

#### Scenario: MMUPlugin 构造签名编译通过
- **WHEN** `MMUPlugin mmu(SvMode::Sv39, {TLBConfig{"L0", 8, 8}, TLBConfig{"L1", 64, 4}}, {max_inflight: 2});` 编译
- **THEN** MUST 编译通过
- **AND** 构造后 `mmu` MUST 是有效 PluginBase 派生实例

#### Scenario: MMUPlugin 与 RiscvMMUPlugin 类型关系
- **WHEN** `class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin` 编译
- **THEN** RiscvMMUPlugin MUST 可用 `MMUPlugin` 构造（继承构造）
- **AND** 旧 `cf::cpu::plugins::MMUPlugin` MUST 仍可用 `using MMUPlugin = RiscvMMUPlugin;` 别名兼容

### Requirement: TLBFactory 选模板特化

`cf::ip::mmu::TLBFactory::create(const TLBLevelConfig& cfg)` MUST 按 `cfg.entries`/`cfg.ways`/`cfg.asid_bits` 选择预定义的模板特化，未支持的组合 MUST 抛 `std::invalid_argument` 异常。

支持组合（MUST 特化）:
- entries ∈ {8, 16, 32, 64, 128, 256}
- ways ∈ {1, 2, 4, 8}
- asid_bits ∈ {0, 9, 12, 16}

#### Scenario: 工厂创建典型配置
- **WHEN** 调用 `TLBFactory::create({entries=64, ways=4, asid_bits=9, policy="LRU"})`
- **THEN** MUST 返回非空 `std::unique_ptr<TLBBase>`
- **AND** `name()` MUST 返回 "L0"（由调用方指定 name）
- **AND** 实际类型 MUST 是 `TLB<64, 4, 27, 9, 1>` 或等价特化

#### Scenario: 工厂拒绝未支持配置
- **WHEN** 调用 `TLBFactory::create({entries=100, ...})` (100 不在支持列表)
- **THEN** MUST 抛 `std::invalid_argument` 异常
- **AND** 异常信息 MUST 含 "entries=100 not supported"

### Requirement: TLBReplacementPolicy 抽象基类与 4 种策略

`cf::ip::mmu::policies::TLBReplacementPolicy` MUST 是抽象基类（**模板化**），含 4 个方法：
- `void on_access(uint32_t set, uint32_t way) = 0`
- `uint32_t select_victim(uint32_t set) = 0`
- `void on_insert(uint32_t set, uint32_t way) = 0`
- `std::string name() const = 0`

4 个策略 MUST 落地（命名空间 `cf::ip::mmu::policies`）:
- `NoReplacementPolicy` (1-way / 全关联 no-op, name="None")
- `FIFOPolicy` (FIFO 队列, name="FIFO")
- `LRUPolicy` (近似 LRU 或精确 LRU, name="LRU")
- `RRIPPolicy` (RRIP, name="RRIP")

命名空间 MUST 独立于 `cf::ip::cache::policies::ReplacementPolicy`，不复用 cache 的基类。

#### Scenario: 4 个策略类存在
- **WHEN** `ls ip/mmu/policies/` 执行
- **THEN** MUST 存在 5 个头文件: `tlb_replacement_policy.h`, `no_replacement_policy.h`, `fifo_policy.h`, `lru_policy.h`, `rrip_policy.h`
- **AND** 每个头文件 MUST 含 `cf::ip::mmu::policies::` 命名空间

### Requirement: 配置 schema 含 GPU/CPU 友好字段

`ip/mmu/configs/params_schema.json` MUST 含以下字段（类型与范围如下）:

| 字段 | 类型 | 默认 | 范围/枚举 |
|------|------|------|----------|
| `topology` | string | "unified" | ["unified", "split_id"] |
| `asid_bits` | int | 9 | 0-16 |
| `sv_mode` | string | "sv39" | ["Bare", "Sv32", "Sv39", "Sv48"] |
| `supported_page_sizes` | array[int] | [4096, 2097152, 1073741824] | 4096-1073741824 |
| `ptw_max_inflight` | int | 2 | 1-8 |
| `shadow_fill_from_next` | bool | true | - |
| `levels` | array[object] | required | 1-4 项, 每项含 name/entries/associativity/num_lookup_ports(1-8)/lookup_latency_cycles(0-16)/replacement_policy(None/FIFO/LRU/RRIP) |

`topology="split_id"` 骨架阶段 MUST 在 schema `description` 标注 "Phase 1 skeleton only `unified`; split_id postponed to mmu-tlb-ptw-impl"。

#### Scenario: 典型 CPU 配置合法
- **WHEN** 提交 `{"topology":"unified", "asid_bits":9, "sv_mode":"sv39", "supported_page_sizes":[4096,2097152], "levels":[{"name":"L0","entries":8,"associativity":8,"num_lookup_ports":1,"replacement_policy":"FIFO"}, {"name":"L1","entries":64,"associativity":4,"num_lookup_ports":1,"replacement_policy":"LRU"}]}`
- **THEN** schema 验证 MUST 通过

#### Scenario: 典型 GPU 配置合法
- **WHEN** 提交 `{"topology":"unified", "asid_bits":16, "sv_mode":"sv39", "supported_page_sizes":[4096,65536,2097152,1073741824], "ptw_max_inflight":4, "levels":[{"name":"L0","entries":16,"associativity":16,"num_lookup_ports":4,"replacement_policy":"FIFO"}, {"name":"L1","entries":256,"associativity":8,"num_lookup_ports":4,"replacement_policy":"LRU"}]}`
- **THEN** schema 验证 MUST 通过
- **AND** 体现 GPU 高端口数 (num_lookup_ports=4) + 多种 page size (含 64KB)

#### Scenario: 非法配置被拒
- **WHEN** 提交 `{"topology":"unified", "asid_bits":32, ...}` (asid_bits 超出 0-16)
- **THEN** schema 验证 MUST 失败
- **AND** 错误信息 MUST 指向 asid_bits 字段

### Requirement: bundles/mem_bundles.h 扩展 TlbReq/TlbResp

`bundles/mem_bundles.h` MUST 追加 2 个 Bundle（既有 6 个 Bundle 不动）:

```cpp
struct TlbReq {
  cf::plugin::uint_t<64> vaddr{0};
  cf::plugin::uint_t<16> asid{0};
  cf::plugin::bool_t    is_fetch{false};  // IBus vs LSU
  cf::plugin::uint_t<8> id{0};
};

struct TlbResp {
  cf::plugin::uint_t<64> paddr{0};
  cf::plugin::uint_t<8>  perms{0};
  cf::plugin::bool_t     hit{false};
  cf::plugin::bool_t     fault{false};
  cf::plugin::uint_t<4>  fault_code{0};
  cf::plugin::uint_t<8>  id{0};
};
```

所有字段 MUST 用 `cf::plugin::uint_t<N>`，无 IO 方向语义（与既有 Bundle 一致）。

#### Scenario: TlbReq/TlbResp 编译通过且字段类型合规
- **WHEN** `#include "bundles/mem_bundles.h"` 编译
- **THEN** 既有 6 个 Bundle MUST 不变
- **AND** `sizeof(cf::bundles::TlbReq) == sizeof(uint64_t) + sizeof(uint16_t) + sizeof(bool_t) + sizeof(uint8_t)` (12 字节, POD 自然布局, 无 internal padding)
- **AND** `sizeof(cf::bundles::TlbReq) % alignof(uint64_t) == 0` (8 字节对齐, 满足 memcopy 要求)
- **AND** 字段类型检查脚本 `tools/d4_check.sh` MUST 不报 D4 违规

### Requirement: RiscvMMUPlugin 继承 cf::ip::mmu::MMUPlugin

`ip/cpu/plugins/mmu.h` MUST 重构为:
```cpp
class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin {
  // RISC-V 特定 hook: satp 写入 / sfence.vma 拦截 / exception code 12/13/15
};
```

向后兼容 MUST 保留: `using MMUPlugin = RiscvMMUPlugin;`（旧名仍可用，避免破坏 M3 阶段 cpu 测试）。

骨架阶段 `RiscvMMUPlugin` 可只调基类接口，RISC-V 特定 hook 在注释中标"推迟到 mmu-tlb-ptw-impl"。

#### Scenario: 旧名 MMUPlugin 仍可用
- **WHEN** 旧代码引用 `cf::cpu::plugins::MMUPlugin`
- **THEN** MUST 编译通过（类型别名兼容）
- **AND** 实际类型 MUST 是 `RiscvMMUPlugin` 或其别名

#### Scenario: 既有 cpu 测试 0 破坏
- **WHEN** `tools/run_chipforge_tests.sh` 执行
- **THEN** 既有 21/21 测试 MUST 仍 PASS
- **AND** 既有 cpu Plugin 文件 MUST 零改动

### Requirement: MMUPlugin / TLB 类在 D4 框架下注册

`MMUPlugin` MUST 继承 `cf::plugin::PluginBase`，`setup()`/`build()` MUST 用 `at_stage()` 注册（无 `tick()`）。`TLB`/`TLBBase`/`MultiLevelTLB` 不直接继承 PluginBase（这些是 IP 内部组件类，不在 Plugin 框架）。

#### Scenario: MMUPlugin 派生与 D4 合规
- **WHEN** `class MMUPlugin : public cf::plugin::PluginBase` 编译
- **THEN** MUST 编译通过
- **AND** `setup()`/`build()` MUST 调 `pb.at_stage("...", Phase::NORMAL, callback)`
- **AND** 不得出现业务 `tick()` 方法（`PluginBase::tick()` 是 private deleted）

### Requirement: 骨架阶段单元测试覆盖

`tests/mmu/` MUST 含 4 个测试文件，覆盖:

1. `test_mmu_skeleton.cpp`: smoke test, 验证 MMUPlugin 构造 + TLBFactory.create() 返回有效对象
2. `test_multi_level_tlb.cpp`: 验证 MultiLevelTLB 编排 shadow fill / 反向失效 / ASID 切换 4 个场景
3. `test_mmu_config_schema.cpp`: 验证 params_schema.json 对典型 CPU + GPU 配置都通过验证，对非法配置失败
4. `test_tlb_factory.cpp`: 验证 TLBFactory 对支持/不支持 entries/ways/asid_bits 组合的行为

测试文件位置 MUST 遵守 `test-location-discipline` spec: `tests/mmu/`，不在 `ip/mmu/test/`。

#### Scenario: tests/mmu/ 存在且编译通过
- **WHEN** `ls tests/mmu/` 执行
- **THEN** MUST 存在 5 个 `test_*.cpp` 文件 (`test_tlb_unit` + `test_multi_level_tlb` + `test_mmu_config_schema` + `test_tlb_factory` + `test_mmu_plugin`)
- **AND** `cmake --build build` 编译 MUST 通过
- **AND** `ctest -L mmu` MUST 至少 33/33 PASS（lib/ 16 + Plugin 5 + config 6 + factory 6 = 33 个 test cases 跨 5 个文件）
