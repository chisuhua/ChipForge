# 接口设计详解

## 共享 Bundle 定义

### 1.0 Bundle 形态演进 (Phase 1 → Phase 5 → Phase 6)

> **本小节目的**: 明确 Bundle 字段类型在不同 Phase 的形态,避免架构文档 (interface-design.md) 描述 `ch_uint<N>` + `bundle_base<Self>` 与 Phase 1 代码 (`cf::plugin::uint_t<N>` POD) 之间的"双轨制"漂移。

ChipForge 项目的 Bundle 字段类型分**三阶段**演进,与路线图对齐:

| 阶段 | 字段类型 | Bundle 形态 | 依赖 | 决策依据 |
|------|----------|------------|------|----------|
| **Phase 1 (TLM)** | `cf::plugin::uint_t<N>` | POD struct (无虚函数) | 仅 `cf::plugin::uint_t.h` (Phase 0 提供) | D4 Plugin-style 强制 (`bundles/README.md` §2) |
| **Phase 5 (RTL 协同)** | `ch_uint<N>` + `bundle_base<Self>` | 派生类,需 CppHDL bundle 体系 | `CppHDL/include/bundle/bundle_base.h` | ADR-024 (⚠️ Mapper 推迟) + 路线图 §6.3 |
| **Phase 6 (完整框架)** | 自动 codegen 派生两套 | codegen 工具生成两套 Bundle | Phase 6 调度算法 + JSON 解析 | `declarative-hybrid-framework.md` §5.5 BundleMapper 设计 |

#### 1.0.1 ★ 早期形态 POD vs 设计目标 bundle_base 对照表

> ★ ★ **目标态 = 下表右列「Phase 5+（设计目标）」**（★ `bundle_base` 派生 + `CH_BUNDLE_FIELDS` + `make_output` 自动 Direction）；★ ★ **左列「Phase 1」= 早期形态**（★ `cf::plugin::uint_t<N>` typedef 占位 + POD struct + 手动 Direction）⇒ ★ ★ **两列都是形态描述，★ 「当前状态」不在本表**（★ 治理修订 2026-11-08 · ADR-095 §2.3「就地净化」· 用户已签认）⇒ ★ 当前实现进度见 [`roadmap-evolution.md`](./roadmap-evolution.md)。

| 维度 | Phase 1（★ 早期形态） | Phase 5+（★ **设计目标**） | 升级路径 |
|------|------------------|--------------------|----------|
| **字段类型** | `cf::plugin::uint_t<N>` (typedef 占位) | `ch_uint<N>` (CppHDL 强类型) | BundleMapper 模板映射 (Phase 5) |
| **Bundle 形态** | POD struct (可 memcpy) | `: public bundle_base<Self>` 派生 | 同上 |
| **元信息宏** | 无 (POD 无虚函数) | `CH_BUNDLE_FIELDS_T(...)` | 编译期 codegen |
| **Direction 切换** | 手动 (master/slave 各自定义) | `make_output(...)` 自动 | CppHDL runtime |
| **典型 Bundle 位置** | `bundles/mem_bundles.h` (待实现, Phase 0 仅头文件声明) | `CppHDL/include/bundle/mem_bundles.h` | include 路径切换 |
| **业务代码影响** | 0 (Phase 1 Plugin 已用 POD) | 0 (Mapper 编译期映射) | 切换头文件即可 |
| **决策依据** | D4 Plugin-style 强制 | ADR-024 + 路线图 Phase 5 | `.omo/drafts/bundle-mapper-phase-5-6-decision.md` |

**★ 关键澄清（★ 治理修订 2026-11-08）**：★ 本文档其余段落（如 §「所有 Bundle 继承 bundle_base」）展示的是 **Phase 5+ 目标形态**；★ ★ **「当前实现」不是本文件承载的内容**（★ 属实施态 ⇒ 见 `roadmap-evolution.md`）⇒ ★ 本文件只承载**形态与升级路径**。Phase 1 业务代码以 POD 写就，无需也无法直接编译 §后续代码块中的 `bundle_base` 派生示例。

**★ 早期形态（Phase 1）的 Bundle 定义示例**（★ `bundles/mem_bundles.h` · ★ **形态示例、非当前状态断言**）：
```cpp
struct CacheReq {
  cf::plugin::uint_t<64> address{0};
  cf::plugin::uint_t<64> data{0};
  cf::plugin::bool_t     is_write{false};
  cf::plugin::uint_t<2>  op{0};
  cf::plugin::uint_t<8>  id{0};
};
```

**Phase 5/6 (待实施) Bundle 升级路径** (草案, 见 `.omo/drafts/bundle-mapper-phase-5-6-decision.md`):
- `BundleMapper<cf::bundles::CacheReq, bundles::CacheReqBundle>` 模板将 POD 字段映射为 `ch_uint<N>` + `CH_BUNDLE_FIELDS_T(...)` 派生
- ★ 早期形态（Phase 1）的业务代码 **无需修改**, 只需切换 Bundle 头文件包含
- 推迟依据: `plugin-framework.md` §1 + `adr.md` ADR-024 ⚠️ 状态 + `bundles/README.md` §8 表格

**为什么 Phase 1 用 POD 而非 bundle_base?**
- D4 Plugin-style 强制: 业务代码无 tick(), 无状态机, Bundle 字段用 `uint_t<N>` (D4.3)
- POD struct 可直接 memcpy 给 ch_stream 事务层, 零开销
- Phase 5 升级时, `BundleMapper` 是纯编译期模板, 不影响业务代码

**禁止模式** (D4 静态检查, `bundles/README.md` §5):
- ❌ `ch_uint<64> address;` (在 `bundles/*.h` 中)
- ❌ `uint64_t address;` (在 `bundles/*.h` 中)
- ❌ `class CacheReq : public bundle_base<CacheReq> { ... }` (在 Phase 1)
- ✅ `cf::plugin::uint_t<64> address{0};` (Phase 1 唯一允许)

**相关决策**: `.omo/drafts/bundle-mapper-phase-5-6-decision.md` (Phase 5/6 BundleMapper 实施路径, 待本次新建)

所有 Bundle **在 Phase 5+ 设计目标中**继承 `bundle_base`，使用 `ch_uint<N>` / `ch_bool` 强类型，保证 TLM 和 RTL 共用同一份定义。**★ 早期形态（Phase 1）使用 POD struct（见 §1.0.1 对照表）**。Phase 5+ 目标代码示例（待实施）：

```cpp
// bundles/mem_bundles.h
struct MemReqBundle : public bundle_base<MemReqBundle> {
    ch_uint<64> transaction_id;  // 端到端追踪 ID
    ch_uint<64> address;
    ch_uint<8>  burst_len;
    ch_bool     is_write;
    ch_uint<512> data;           // 64B 缓存行
    ch_uint<2>  privilege;       // M/S/U mode

    CH_BUNDLE_FIELDS_T(transaction_id, address, burst_len, is_write, data, privilege)

    void as_master_direction() {
        this->make_output(transaction_id, address, burst_len, is_write, data, privilege);
    }
};

struct MemRespBundle : public bundle_base<MemRespBundle> {
    ch_uint<64> transaction_id;  // 与请求匹配
    ch_uint<512> data;
    ch_bool     error;

    CH_BUNDLE_FIELDS_T(transaction_id, data, error)

    void as_slave_direction() {
        this->make_output(transaction_id, data, error);
    }
};
```

```cpp
// bundles/cache_bundles.h
struct CacheReqBundle : public bundle_base<CacheReqBundle> {
    ch_uint<64> transaction_id;
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;

    CH_BUNDLE_FIELDS_T(transaction_id, address, size, is_write, data)

    void as_master_direction() {
        this->make_output(transaction_id, address, size, is_write, data);
    }
};

struct CacheRespBundle : public bundle_base<CacheRespBundle> {
    ch_uint<64> transaction_id;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;

    CH_BUNDLE_FIELDS_T(transaction_id, data, is_hit, error_code)

    void as_slave_direction() {
        this->make_output(transaction_id, data, is_hit, error_code);
    }
};
```

```cpp
// bundles/impl_mode.h
enum class ImplMode {
    TLM_ONLY = 0,   // 高速 TLM 仿真
    RTL_ONLY = 1,   // 周期精确 RTL
    COMPARE  = 2,   // TLM + RTL 并行对比
    SHADOW   = 3    // RTL 跟踪 TLM（调试）
};
```

## TLM 组件接口（CppTLM）

```cpp
// ═══════════════════════════════════════════════════════════════
// TLM 组件设计视角（以 CPU 为例）
// ═══════════════════════════════════════════════════════════════

// 模块继承 ChStreamModuleBase，不是 sc_module
// （示例：cpptlm::CPUTLM 已被注册到 CppTLM ModuleFactory；项目自有 RISC-V ISS 处于设计中）
//
// ★★ v2 标注（2026-10-28，ADR-092 §D7 / ADR-083 v1.2）：
//   ① 本例是 **legacy CppTLM 设想** ⇒ 其 `ch_stream<Bundle>` 成员与 `register_input` 登记
//      **均不存在**（CppTLM 无 `ch_stream` 类型；`declarative-hybrid-framework.md:186`
//      已就同一问题记录「原文档描述的 API 均不存在」）⇒ 本例仅作 **legacy 知识留存**，MUST NOT 当作可编译 API
//   ② ★ **bundle 名更正**：`MemReqBundle` / `MemRespBundle` 在代码中不存在 ⇒ 实际是
//      **`cf::bundles::MemReq` / `MemResp`**（`bundles/mem_bundles.h`）
//   ③ ★ **目标态的接口层**见 ADR-091 §D3a：★ **CppHDL 的 `ch_stream<CpuBusReq>`**
//      （`ip/cpu/core/bus_iface_chmem.h`，真名 `CpuBusReq`/`CpuBusResp`）★ **不是** CppTLM 组件范式
//      ★ ★ **v1.16 补注（2026-11-07 · ADR-094）**：★ `ch_stream` 是**契约形状**，★ **物理形态 =
//      逐字段标量端口**（★ `ibus_port_chmem.h:21-30` 有论证：上游 `as_master_direction()` 本身
//      就是逐字段 `make_output(payload,valid)` + `make_input(ready)`）⇒ ★ ★ **层 2 单元的接口
//      MUST 是逐字段标量端口**（★ ADR-094 r.11 / r.16）⇒ ★ 层次化 Plugin 的结构事实见
//      [`hierarchical-plugin-architecture.md`](./hierarchical-plugin-architecture.md)
class CpuTlmExample : public ChStreamModuleBase {
public:
    CpuTlmExample(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    // ── stream 是模块内部接口（设计视角）──
    // 模块设计者从 stream 角度思考数据流
    // StreamAdapter 自动将其映射到 Port（框架视角）
private:
    // 内部 stream 通道（★ bundle 名见下方更正）
    cpptlm_stream<MemReq>   ibus_req_;    // 指令请求
    cpptlm_stream<MemResp>  ibus_resp_;   // 指令响应
    cpptlm_stream<MemReq>   dbus_req_;    // 数据请求
    cpptlm_stream<MemResp>  dbus_resp_;   // 数据响应

public:
    void on_config_loaded() override {
        // JSON 配置加载后回调
        auto isa = get_param<std::string>("isa");   // "rv64gc"
        auto stages = get_param<int>("pipeline_stages"); // 5
    }

    void tick() override {
        // 每周期执行：取指 → 译码 → 执行 → 访存 → 写回
        fetch_stage();
        decode_stage();
        execute_stage();
        memory_stage();
        writeback_stage();
    }
};

// ── 框架组装视角（用户无需关心）──
// ModuleFactory 自动完成以下操作：
// 1. 解析 JSON 配置，实例化 CPU TLM 类
// 2. 为每个 ch_stream 创建 StreamAdapter
// 3. StreamAdapter 将 ch_stream 暴露为 MasterPort/SlavePort
// 4. 通过 JSON 连接描述完成端口互连
//
// JSON 配置示例（soc 级别）：
// {
//   "modules": [
//     {"name": "cpu_0", "type": "CPUTLM", "params": {"cores": 1}}
//   ],
//   "connections": [
//     {"from": "cpu_0.ibus_req", "to": "icache_0.req_in"}
//   ]
// }
```

## RTL 组件接口（CppHDL）

> ★ ★ **v1.16 订正注（2026-11-08 · ADR-091 §D3a.7 / ADR-094 r.11 / r.16）**：★ ★ **本节下方示范是 Phase 1 ~ 5 时代的 `__input(Bundle)` / `__output(Bundle)` 形态 ⇒ ★ 与现行目标态冲突** ⇒ ★ ★ **目标态 = 层 2 单元的接口 MUST 是「逐字段标量端口」**（★ `__io` 宏 + `ch_in` / `ch_out`；★ **MUST NOT** 用 bundle 成员跨层：★ bundle 成员是 `type_proxy`、**不是端口** ⇒ 被上游接口白名单排除；★ 且 `ch_in<ch_stream<…>>` 会退化成**整宽 blob 的单个 `type_input`** ⇒ 语义空壳）⇒ ★ ★ **正确示范见下方「层 2 单元接口（目标形态）」**（★ 取自已落地的 `ip/cpu/core/regfile_chmem.h`）⇒ ★ **决策权威** = ADR-091 v1.16 §D3a.7 + ADR-094 r.11 / r.16；★ **层次化 Plugin 的结构事实**见 [`hierarchical-plugin-architecture.md`](./hierarchical-plugin-architecture.md)。

```cpp
// ═══════════════════════════════════════════════════════════════
// RTL 组件设计视角（以 L1 Cache 为例）
// ═══════════════════════════════════════════════════════════════

// CppHDL 组件继承 Component
class L1CacheRtl : public Component {
public:
    // 端口声明（Bundle 类型）
    __input(CacheReqBundle)   cpu_req;     // CPU 侧请求
    __output(CacheRespBundle) cpu_resp;    // CPU 侧响应
    __output(MemReqBundle)    mem_req;     // 下级存储请求
    __input(MemRespBundle)    mem_resp;    // 下级存储响应

    // 内部状态
    ch_reg<ch_uint<32>> cache_tag[256];
    ch_reg<ch_uint<64>> cache_data[256];
    ch_reg<ch_bool>     valid_bits[256];

    void describe() override {
        // 组合逻辑：缓存查询
        auto req = io(cpu_req);
        auto index = slice<7, 0>(req.address);
        auto tag = slice<31, 8>(req.address);

        auto tag_match = (cache_tag[index] == tag);
        auto is_hit = tag_match & valid_bits[index];

        // 驱动响应
        CacheRespBundle resp;
        resp.data = ch_sel(is_hit, cache_data[index], 0);
        resp.hit = is_hit;
        io(cpu_resp) <<= resp;

        // 时序逻辑：写命中时更新
        auto write_en = req.is_write & is_hit;
        cache_data[index] <<= ch_sel(write_en, req.data, cache_data[index]);
        cache_tag[index] <<= ch_sel(write_en & !valid_bits[index], tag, cache_tag[index]);
        valid_bits[index] <<= valid_bits[index] | write_en;
    }
};
```

### ★ 层 2 单元接口（★ **目标形态** · 2026-11-08 补）

> ★ ★ **目标态的层 2 单元接口 = 逐字段标量端口**（★ 权威 = ADR-091 v1.16 §D3a.7 + ADR-094 r.11 / r.16）⇒ ★ **示例取自已落地的唯一层 2 实例** `ip/cpu/core/regfile_chmem.h`（★ 7 个标量端口）⇒ ★ **对照上方旧示范**：★ 旧 = 4 个 **bundle** 端口；★ 新 = 7 个**标量**端口（★ 4 in + 2 out + 1 in 使能）⇒ ★ **代价 = 端口数变多**（★ 收益 = 端口真为端口 ⇒ 层次化 emitter 可正确建 module 边界 + 观测可按端口寻址）。

```cpp
// ★ 层 2 单元（`ch::Component`）—— ★ 逐字段标量端口形态
class RegFileComponent : public ch::Component {
 public:
  using Addr = ch::core::ch_uint<5>;
  using Data = ch::core::ch_uint<32>;

  // ★ `__io` 宏（上游 `include/core/io.h`）⇒ 生成 `io_type` struct + `io_storage_` + `io()` accessor
  // ★ ★ 成员顺序 = aggregate 初始化顺序 ⇒ `create_ports()` MUST 按同序传
  __io(
      // ── 读端口 A ──
      ch::core::ch_in<Addr> rs1_addr;
      ch::core::ch_out<Data> rs1_data;
      // ── 读端口 B ──
      ch::core::ch_in<Addr> rs2_addr;
      ch::core::ch_out<Data> rs2_data;
      // ── 写端口 ──
      ch::core::ch_in<Addr> rd_addr;
      ch::core::ch_in<Data> rd_data;
      // ★ 写使能（★ 1-bit `ch_uint<1>` ⇒ 与桥接的 `Ports::rd_we` 同款 ⇒ 类型必须一致）
      ch::core::ch_in<ch::core::ch_uint<1>> rd_we;
  )

  explicit RegFileComponent(ch::Component* parent = nullptr,
                             const std::string& name = "regfile")
      : ch::Component(parent, name) {}
  // ★ ★ 本单元**不感知** `Payload`（★ 层 2 不依赖层 1）
  //   ⇒ ★ 边界转换由**桥接 plugin**（`regfile_bridge_chmem.h`）承担
  // ★ ★ MUST NOT 用 `ch_stream`（★ 越层：ADR-092 §D4 定 `ch_stream` = IP 间 / 核↔从设备接口）

  // ★ ★ 完整实现另含 `create_ports()` override（★ 显式命名各端口 ⇒ ADR-091 G-1o：
  //   顶层端口名 MUST NOT 与内部 ch_reg / ch_node 同名）⇒ ★ 照抄时勿漏
};
```

> ★ **三个层次的接口形态（★ 勿混）**：★ **层 1 stage 间** = `Payload<T>` + `PipeNode`（无握手）· ★ **层 2 单元接口** = 逐字段标量端口（★ 本节）· ★ **IP 之间 / 核 ↔ 从设备** = `ch_stream<Bundle>`（★ ADR-092 §D4）⇒ ★ 权威 = ADR-091 v1.16 §D3a.7。

## SoC 组合层

> **重要说明：** SoC 组装不直接操作 `ch_stream`。
> 模块内部使用 `ch_stream` 定义数据流，StreamAdapter 自动将其映射为 Port。
> SoC 层通过 JSON 配置 + ModuleFactory 引用端口名称完成连接。

```cpp
// soc/RiscvVirtSoC.h
// 推荐使用 JSON 配置驱动组装（见下方）
// 以下手工 C++ 组装仅供理解参考
// 实际 SoC 装配推迟到 Phase 2+ 实施；当前推荐使用 JSON + ModuleFactory
class ExampleVirtSoC {
public:
    explicit ExampleVirtSoC(ImplMode mode = ImplMode::TLM_ONLY);

    void build();    // 实例化并连接所有组件
    void run(uint64_t cycles);
    void load_elf(const std::string& path);

private:
    ImplMode mode_;

    // CPU：TLM（ISS 驱动，cpptlm::CPUTLM 业务扩展待 Phase 2 实施）
    std::unique_ptr<ComponentBase> cpu_;

    // Cache：按 mode_ 决定实现（CppTLM 标准 CacheTLM；Phase 5+ 才有 L1CacheRtl）
    std::unique_ptr<ComponentBase> l1_cache_;

    // 其余组件（TLM 通用模板，无 L1/DRAM/外设特化）
    std::unique_ptr<ComponentBase> dram_;    // cpptlm::MemoryTLM
    std::unique_ptr<ComponentBase> bus_;     // cpptlm::CrossbarTLM
    std::unique_ptr<ComponentBase> clint_;   // 需在 ip/peripheral/ 自行实现
    std::unique_ptr<ComponentBase> plic_;    // 需在 ip/peripheral/ 自行实现
    std::unique_ptr<ComponentBase> uart_;    // 需在 ip/peripheral/ 自行实现

    // 连接通过 Port 接口完成（由 StreamAdapter 从 ch_stream 自动映射而来）
    void connect_all();
};
```

### JSON 配置驱动组装（推荐）

除手工 C++ 组装外，推荐使用 CppTLM 的 ModuleFactory + JSON 配置系统实现 SoC 组装：

```cpp
// main.cpp -- JSON 配置驱动启动
#include "core/module_factory.hh"
#include "core/event_queue.hh"

int main(int argc, char* argv[]) {
    // 注册所有组件到工厂
    REGISTER_ALL;

    // 加载 JSON 配置
    auto config = json::parse(std::ifstream(argv[1]));

    // ModuleFactory 自动完成 8 步组装流程
    EventQueue eq;
    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    // 加载 ELF 并运行仿真
    auto cpu = factory.getInstance<CpuTlmExample>("cpu_0");
    cpu->load_elf(argv[2]);
    eq.run(10000000);  // 运行 10M 周期

    // 导出统计
    factory.dump_metrics("stats.json");
    return 0;
}
```

此方式无需重新编译即可切换配置，是设计空间探索的基础。

## 内存地图（参考 QEMU virt）

```
地址范围                       大小      用途
0x0000_0000_0000_1000          4 KB      Debug ROM / 复位向量
0x0000_0000_0000_2000          4 KB      HTIF（tohost/fromhost）
0x0000_0000_0200_0000          64 KB     CLINT（mtime, mtimecmp）
0x0000_0000_0C00_0000          64 MB     PLIC（中断控制器）
0x0000_0000_1000_0000          4 KB      UART 0（NS16550A）
0x0000_0000_1000_1000          4 KB      VirtIO Block
0x0000_0000_1000_2000          4 KB      VirtIO Net
0x0000_0000_2000_0000          256 MB    Flash / ROM（程序/固件）
0x0000_0000_8000_0000          512 MB    主 DRAM（可配置大小）
```

