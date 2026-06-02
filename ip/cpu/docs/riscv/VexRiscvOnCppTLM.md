这个思路非常清晰且务实：**CppTLM 做架构/行为级验证，CppHDL 做 RTL 生成**，两者通过共享的 Bundle 定义和 `ch_stream` 接口保持语义一致性。这比直接硬刚 RTL 插件系统要合理得多。

基于 CppTLM 的现有基础设施，我为你设计一套**"TLM 级插件化流水线 CPU"** 的架构方案，它能在 CppTLM 上验证指令行为与流水线配置，同时保留向 CppHDL RTL 生成的迁移路径。

---

## 一、架构蓝图：CppTLM 上的 VexRiscv 风格插件系统

### 1.1 核心设计原则

| 原则 | 说明 |
|------|------|
| **水平功能切片** | 一个插件 = 一个功能（如 `MulPlugin`），横跨 Fetch→Decode→Execute→Memory→WriteBack |
| **阶段解耦** | 流水线阶段本身不硬编码功能，只提供**数据通路**和**Hook 点** |
| **服务发现** | 插件通过 `core->get_plugin<<OtherPlugin>()` 动态发现依赖 |
| **Bundle 共享** | 指令 Bundle、寄存器 Bundle 等定义同时服务于 TLM 验证和后续 RTL 生成 |
| **周期精确** | 利用 CppTLM 的 `SimObject::tick()` + `EventQueue`，实现周期级行为模拟 |

### 1.2 与 CppTLM 现有设施的映射

```
CppTLM 现有能力                插件化 CPU 中的用途
─────────────────────────────────────────────────────────────
SimObject + tick()            →  PipelineCore 主控 + 各阶段调度
ch_stream<T> / Bundle         →  CPU↔Cache/内存 的接口 (IBus/DBus)
PortPair + ModuleRegistry     →  SoC 级模块连接 (CPU 作为 TLM 模块接入系统)
EventQueue                    →  全局周期推进 + 流水线阶段调度
```

**注意**：流水线**内部**阶段间通信不走 `ch_stream`（那是模块间协议），而是走内部 `Stageable<T>` 机制。

---

## 二、关键组件设计

### 2.1 插件基类：两阶段生命周期

仿造 SpinalHDL 的 `FiberPlugin`，但在 CppTLM 的 TLM 语境下简化为 `setup` + `build`：

```cpp
// include/cpu/plugin.hh
class PipelinePlugin {
public:
    virtual ~PipelinePlugin() = default;
    
    // 阶段1：注册服务、声明 Stageable 字段、检查依赖
    virtual void setup(class PipelineCore* core) {}
    
    // 阶段2：连接逻辑、绑定到流水线阶段
    virtual void build(class PipelineCore* core) {}
    
    // 阶段3：周期执行（由 PipelineCore 统一调度）
    virtual void tick() {}
    
    virtual const char* name() const = 0;
};

// 插件依赖声明（编译期类型安全）
template<typename T>
concept PluginConcept = std::is_base_of_v<<PipelinePlugin, T>;
```

### 2.2 流水线核心：阶段 + Stageable

```cpp
// include/cpu/pipeline_core.hh
enum class StageId { IF, ID, EX, MEM, WB, COUNT };

class PipelineCore : public SimObject {
public:
    PipelineCore(const std::string& name, EventQueue* eq);
    
    // 插件管理
    template<<PluginConcept T, typename... Args>
    T* add_plugin(Args&&... args);
    
    template<<PluginConcept T>
    T* get_plugin();  // 服务发现
    
    void setup();   // 调用所有 plugin->setup()
    void build();   // 调用所有 plugin->build()
    
    // 周期推进：按顺序 tick 各阶段 + 插件
    void tick() override;
    
    // Stageable 注册：插件在 setup() 中声明跨阶段数据
    template<typename T>
    Stageable<T>* create_stageable(const std::string& name);
    
    template<typename T>
    Stageable<T>* get_stageable(const std::string& name);

private:
    std::vector<std::unique_ptr<<PipelinePlugin>> plugins_;
    std::unordered_map<std::type_index, PipelinePlugin*> plugin_map_;
    std::unordered_map<std::string, std::unique_ptr<IStageable>> stageables_;
    
    // 流水线阶段状态
    struct StageState {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t inst = 0;
        // ... 基础通路
    };
    std::array<<StageState, static_cast<size_t>(StageId::COUNT)> stages_;
};
```

### 2.3 Stageable：跨阶段数据通路

这是 VexRiscv 最核心的机制之一。插件在 Decode 阶段 `insert` 数据，在 Execute 阶段 `read`：

```cpp
// include/cpu/stageable.hh
template<typename T>
class Stageable : public IStageable {
public:
    explicit Stageable(const std::string& name) : name_(name) {
        // 每个阶段一个流水线寄存器
        regs_.fill(std::nullopt);
    }
    
    // 在 stage 阶段写入（通常是组合逻辑输出）
    void set(StageId stage, const T& value) {
        comb_[static_cast<size_t>(stage)] = value;
    }
    
    // 在 stage 阶段读取（读取上一周期锁存的值）
    std::optional<T> get(StageId stage) const {
        return regs_[static_cast<size_t>(stage)];
    }
    
    // 由 PipelineCore 在 tick 末尾统一推进流水线
    void advance() {
        // WB → 丢弃, MEM → WB, EX → MEM, ID → EX, IF → ID
        for (int i = static_cast<int>(StageId::COUNT) - 1; i > 0; --i) {
            regs_[i] = regs_[i-1];
        }
        regs_[0] = comb_[0];  // IF 阶段取组合逻辑值
        comb_.fill(std::nullopt);
    }
    
private:
    std::string name_;
    std::array<std::optional<T>, static_cast<size_t>(StageId::COUNT)> regs_;
    std::array<std::optional<T>, static_cast<size_t>(StageId::COUNT)> comb_;
};
```

### 2.4 指令 Bundle（TLM/RTL 共享）

利用 CppTLM 的 Bundle 层，定义一份指令相关的数据结构，后续 CppHDL 直接复用：

```cpp
// include/bundles/inst_bundles.hh
struct InstBundle : ch::bundle_base<<InstBundle> {
    ch_uint<<32> raw;           // 原始指令
    ch_uint<<32> pc;            // PC
    ch_bool     valid;         // 是否有效
    ch_bool     is_rvc;        // 压缩指令
    
    CH_BUNDLE_FIELDS_T(raw, pc, valid, is_rvc)
};

struct DecodeBundle : ch::bundle_base<<DecodeBundle> {
    ch_uint<<7>  opcode;
    ch_uint<<5>  rd, rs1, rs2;
    ch_uint<<3>  funct3;
    ch_uint<<7>  funct7;
    ch_uint<<32> imm;
    ch_bool     use_imm;       // 是否使用立即数
    
    CH_BUNDLE_FIELDS_T(opcode, rd, rs1, rs2, funct3, funct7, imm, use_imm)
};

struct RegFilePort : ch::bundle_base<<RegFilePort> {
    ch_uint<<5>  addr;
    ch_uint<<32> rdata;
    ch_uint<<32> wdata;
    ch_bool     wen;
    
    CH_BUNDLE_FIELDS_T(addr, rdata, wdata, wen)
};
```

---

## 三、插件示例：乘法单元 MulPlugin

展示如何在 CppTLM 上实现"水平功能切片"：

```cpp
// include/cpu/plugins/mul_plugin.hh
class MulPlugin : public PipelinePlugin {
public:
    const char* name() const override { return "MulPlugin"; }
    
    void setup(PipelineCore* core) override {
        // 1. 声明本插件跨阶段使用的数据
        core->create_stageable<<MulReq>("mul_req");
        core->create_stageable<<MulResult>("mul_result");
        
        // 2. 依赖检查：需要 RegFilePlugin 提供寄存器读取服务
        auto* regfile = core->get_plugin<<RegFilePlugin>();
        if (!regfile) {
            throw std::runtime_error("MulPlugin requires RegFilePlugin");
        }
    }
    
    void build(PipelineCore* core) override {
        // 获取 Stageable 句柄
        mul_req_ = core->get_stageable<<MulReq>("mul_req");
        mul_result_ = core->get_stageable<<MulResult>("mul_result");
    }
    
    void tick() override {
        // ── Decode 阶段 ──
        // 检查当前指令是否是 MUL/MULH/MULHSU/MULHU
        auto decode = core_->get_stageable<<DecodeBundle>("decode")->get(StageId::ID);
        if (decode && is_mul_inst(decode->opcode, decode->funct3, decode->funct7)) {
            MulReq req;
            req.rs1_val = read_reg(decode->rs1);  // 通过 RegFilePlugin 服务读取
            req.rs2_val = read_reg(decode->rs2);
            req.signed_mode = decode->funct3;     // MUL=0, MULH=1, etc.
            mul_req_->set(StageId::ID, req);        // 插入流水线
        }
        
        // ── Execute 阶段 ──
        auto req = mul_req_->get(StageId::EX);     // 读取 ID 阶段插入的数据
        if (req) {
            MulResult res;
            // TLM 级可以是行为模型（快速），后续 RTL 级替换为乘法器逻辑
            res.result = behavioral_mul(req->rs1_val, req->rs2_val, req->signed_mode);
            mul_result_->set(StageId::EX, res);
        }
        
        // ── WriteBack 阶段 ──
        auto result = mul_result_->get(StageId::WB);
        if (result) {
            writeback(result->result);  // 写回寄存器
        }
    }
    
private:
    Stageable<<MulReq>* mul_req_ = nullptr;
    Stageable<<MulResult>* mul_result_ = nullptr;
    
    bool is_mul_inst(uint32_t opcode, uint32_t funct3, uint32_t funct7) {
        return opcode == 0b0110011 && funct7 == 0b0000001;
    }
    
    uint32_t behavioral_mul(uint32_t a, uint32_t b, uint32_t mode) {
        // TLM 级：直接 C++ 乘法，验证功能正确性
        return a * b;
    }
};
```

---

## 四、SoC 级集成：CPU 作为 CppTLM 模块

PipelineCore 本身是一个 `SimObject`，通过 `ch_stream` 与 Cache/Memory 交互，完美融入 CppTLM 的 `ModuleRegistry`：

```cpp
// src/soc_main.cpp
int main() {
    ModuleRegistry registry;
    EventQueue eq;
    
    // 创建插件化 CPU
    auto* cpu = registry.create_tlm_module<<PipelineCore, IBusBundle, DBusBundle>("cpu");
    
    // 添加插件（顺序 = 优先级，类似 VexRiscv 的 plugin 列表）
    cpu->add_plugin<<IBusSimplePlugin>();
    cpu->add_plugin<<RegFilePlugin>();
    cpu->add_plugin<IntAluPlugin>();
    cpu->add_plugin<<MulPlugin>();      // ← 可插拔
    cpu->add_plugin<<BranchPlugin>();
    cpu->add_plugin<<CSRPlugin>();
    cpu->add_plugin<<DBusSimplePlugin>();
    
    // 两阶段初始化
    cpu->setup();
    cpu->build();
    
    // 连接 SoC（PortPair 解耦）
    auto* cache = registry.create_tlm_module<<CacheTLM, CacheReqBundle, CacheRespBundle>("l1_cache");
    new PortPair(registry.get_port("cpu_ibus"), registry.get_port("cache_ibus"));
    
    // 运行
    eq.add_module(cpu);
    eq.add_module(cache);
    eq.run(1000000);
}
```

---

## 五、向 CppHDL RTL 的迁移路径

这是你最关心的部分。CppTLM 验证通过后，如何生成 RTL？

### 5.1 共享资产：Bundle 定义

`include/bundles/` 下的 Bundle 定义是**最大公约数**。CppHDL 的 `bundle_base` 和 CppTLM 的 Bundle 层如果共享同一套头文件，则 TLM 验证的指令格式、接口协议可直接用于 RTL 生成。

### 5.2 插件代码的分层

建议每个插件拆分为两层：

```cpp
// mul_plugin.hh          // 公共接口 + TLM 行为模型
// mul_plugin_rtl.hh      // CppHDL RTL 实现（可选）
```

```cpp
// 公共接口（TLM/RTL 共用）
class MulPluginBase : public PipelinePlugin {
    virtual void setup(PipelineCore* core) = 0;
    virtual void build(PipelineCore* core) = 0;
};

// TLM 验证版：行为模型
class MulPluginTLM : public MulPluginBase {
    uint32_t behavioral_mul(...) { return a * b; }
};

// RTL 生成版：使用 CppHDL 原语
class MulPluginRTL : public MulPluginBase {
    void build(PipelineCore* core) override {
        // 使用 CppHDL 的 ch::Reg, ch::Mux 等生成硬件
        auto result = ch::Reg(ch::Mux(sel, prod_low, prod_high));
    }
};
```

### 5.3 关键衔接点

| CppTLM (验证) | CppHDL (RTL) | 衔接方式 |
|---------------|--------------|----------|
| `Stageable<T>` 的 tick() 推进 | 流水线寄存器生成 | Stageable 的 `advance()` 逻辑 → CppHDL 的 `Reg` + `ClockDomain` |
| 插件的 `setup()`/`build()` | Elaboration 阶段 | 直接复用两阶段生命周期 |
| `ch_stream` CPU 接口 | AXI/TileLink 接口 | CppTLM 的 `ch_stream` 通过 Mapper 层 → CppHDL 的 IO Bundle |
| Bundle 定义 | Verilog 端口 | 共享头文件，CppHDL CodeGen 直接输出 |

---

## 六、实施路线图建议

### Phase 1：基础设施（2-3 周）
1. 在 CppTLM 上实现 `PipelineCore` + `PipelinePlugin` + `Stageable<T>`
2. 实现 `RegFilePlugin` + `IBusSimplePlugin` + `DBusSimplePlugin`（最小可运行系统）
3. 验证一个 RV32I 基础核能跑通简单程序

### Phase 2：指令插件化（3-4 周）
1. 将 RV32I 指令集拆分为 `IntAluPlugin`、`BranchPlugin`、`LsuPlugin` 等
2. 实现 CSR、异常处理插件
3. 引入 RISC-V 测试集（riscv-tests）验证功能正确性

### Phase 3：架构探索（持续）
1. 快速尝试不同配置：
   - 有/无乘法器
   - 有/无分支预测
   - 单发射 / 双发射（通过 Stageable 的数组化）
   - 不同 Cache 接口宽度
2. 用 CppTLM 的 `EventQueue` 收集性能数据（IPC、各阶段阻塞周期）

### Phase 4：RTL 生成（当 CppHDL 成熟后）
1. 将 `MulPluginTLM` 替换为 `MulPluginRTL`
2. 用 CppHDL 的 `CodeGen` 输出 Verilog
3. 与 CppTLM 的 TLM 模型做 `COMPARE` 模式并行验证（CppTLM 架构中已预留此模式）

---

## 七、风险提示

1. **TLM 与 RTL 的语义鸿沟**：CppTLM 的 `tick()` 是 C++ 函数调用，而 RTL 是并行信号赋值。某些 TLM 验证通过的时序（如组合逻辑环）在 RTL 中可能不可综合。建议在 TLM 阶段就引入"可综合子集"检查。
2. **CppHDL 的成熟度**：调研显示 CppHDL 的 RTL 层和 CodeGen 仍在建设中，RTL 生成路径可能需要你参与共建。
3. **Stageable 的复杂度**：多发射、乱序执行会让 `Stageable<T>` 从简单数组变成重排序缓冲区，TLM 验证阶段建议先聚焦顺序单发射。

---

**总结**：CppTLM 的 `SimObject` + `EventQueue` + `ch_stream` 基础设施完全能支撑一个 TLM 级的插件化 RISC-V 核。关键在于**把 VexRiscv 的"水平功能切片"哲学映射到 C++ 的插件生命周期中**，并通过 `Stageable<T>` 实现跨阶段数据通路。这样验证出的最优配置，在 CppHDL RTL 生成时只需替换插件的底层实现（行为模型 → 硬件原语），架构拓扑本身保持不变。
