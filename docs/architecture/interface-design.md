# 接口设计详解

## 共享 Bundle 定义

所有 Bundle 继承 `bundle_base`，使用 `ch_uint<N>` / `ch_bool` 强类型，保证 TLM 和 RTL 共用同一份定义：

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
// TLM 组件设计视角（以 CPU ISS 为例）
// ═══════════════════════════════════════════════════════════════

// 模块继承 ChStreamModuleBase，不是 sc_module
class RiscvIssTlm : public ChStreamModuleBase {
public:
    RiscvIssTlm(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    // ── ch_stream 是模块内部接口（设计视角）──
    // 模块设计者从 ch_stream 角度思考数据流
    // StreamAdapter 自动将其映射到 Port（框架视角）
private:
    // 内部 ch_stream 通道
    ch_stream<MemReqBundle>  ibus_req_;    // 指令请求
    ch_stream<MemRespBundle> ibus_resp_;   // 指令响应
    ch_stream<MemReqBundle>  dbus_req_;    // 数据请求
    ch_stream<MemRespBundle> dbus_resp_;   // 数据响应

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
// 1. 解析 JSON 配置，实例化 RiscvIssTlm
// 2. 为每个 ch_stream 创建 StreamAdapter
// 3. StreamAdapter 将 ch_stream 暴露为 MasterPort/SlavePort
// 4. 通过 JSON 连接描述完成端口互连
//
// JSON 配置示例（soc 级别）：
// {
//   "modules": [
//     {"name": "cpu_0", "type": "RiscvIssTlm", "params": {"isa": "rv64gc"}}
//   ],
//   "connections": [
//     {"from": "cpu_0.ibus_req", "to": "icache_0.req_in"}
//   ]
// }
```

## RTL 组件接口（CppHDL）

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

## SoC 组合层

> **重要说明：** SoC 组装不直接操作 `ch_stream`。
> 模块内部使用 `ch_stream` 定义数据流，StreamAdapter 自动将其映射为 Port。
> SoC 层通过 JSON 配置 + ModuleFactory 引用端口名称完成连接。

```cpp
// soc/RiscvVirtSoC.h
// 推荐使用 JSON 配置驱动组装（见下方）
// 以下手工 C++ 组装仅供理解参考
class RiscvVirtSoC {
public:
    explicit RiscvVirtSoC(ImplMode mode = ImplMode::TLM_ONLY);

    void build();    // 实例化并连接所有组件
    void run(uint64_t cycles);
    void load_elf(const std::string& path);

private:
    ImplMode mode_;

    // CPU 始终用 TLM（ISS 驱动）
    std::unique_ptr<RiscvIssTlm>   cpu_;

    // Cache：按 mode_ 决定实现
    std::unique_ptr<ComponentBase> l1_cache_;  // L1CacheTlm 或 L1CacheRtl

    // 其余组件（TLM）
    std::unique_ptr<DramTlm>       dram_;
    std::unique_ptr<BusMatrixTlm>  bus_;
    std::unique_ptr<ClintTlm>      clint_;
    std::unique_ptr<PlicTlm>       plic_;
    std::unique_ptr<UartTlm>       uart_;

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
    auto cpu = factory.getInstance<RiscvIssTlm>("cpu_0");
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

