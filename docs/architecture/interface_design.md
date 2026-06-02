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
// cpu/tlm/RiscvIssTlm.h
class RiscvIssTlm : public sc_module {
public:
    // 指令总线（ch_stream 接口）
    ch_stream<MemReqBundle>*  ibus_req;
    ch_stream<MemRespBundle>* ibus_resp;
    // 数据总线
    ch_stream<MemReqBundle>*  dbus_req;
    ch_stream<MemRespBundle>* dbus_resp;
    // 中断输入
    sc_in<bool>     irq_timer;
    sc_in<bool>     irq_software;
    sc_in<uint32_t> irq_external;

    void execute_cycle();  // 驱动 ISS 执行一条指令

private:
    std::unique_ptr<spike_t> spike_;  // Spike ISS 实例

    uint32_t fetch(uint64_t pc) {
        MemReqBundle req;
        req.transaction_id = next_txn_id_++;
        req.address  = pc;
        req.is_write = false;
        req.size     = 4;
        ibus_req->push(req);
        auto resp = ibus_resp->pop();
        return static_cast<uint32_t>(resp.data);
    }
};
```

## RTL 组件接口（CppHDL）

```cpp
// cache/rtl/L1CacheRtl.h
class L1CacheRtl : public Component {
public:
    // 使用 __io 宏定义端口（与 TLM 完全相同的 Bundle 类型）
    __io(
        ch_in<CacheReqBundle>   cpu_req;
        ch_out<CacheRespBundle> cpu_resp;
        ch_out<MemReqBundle>    mem_req;
        ch_in<MemRespBundle>    mem_resp;
    )

    void describe() override {
        // 时序逻辑：通过 ch_reg 定义状态（寄存器）
        ch_mem<ch_uint<64>, 256> cache_data("cache_data");
        ch_mem<ch_uint<32>, 256> cache_tag("cache_tag");
        ch_reg<ch_bool> valid_bits[256];

        // 组合逻辑：直接表达（当前周期内完成）
        auto req = io().cpu_req;
        auto index = bits<7, 0>(req.address);
        auto tag   = bits<31, 8>(req.address);
        auto tag_hit = (cache_tag.aread(index) == tag)
                     & valid_bits[index];

        // 输出驱动
        CacheRespBundle resp;
        resp.transaction_id = req.transaction_id;
        resp.data    = cache_data.aread(index);
        resp.is_hit  = tag_hit;
        resp.error_code = 0_d;
        io().cpu_resp <<= resp;

        // 时序更新：写使能时更新缓存行
        auto write_en = req.is_write & tag_hit;
        cache_data.write(index, req.data, write_en);
        cache_tag.write(index, tag, write_en);
    }
};
```

## SoC 组合层

```cpp
// soc/RiscvVirtSoC.h
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

    void connect_all();  // Bundle 接口一致，自动连接
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

