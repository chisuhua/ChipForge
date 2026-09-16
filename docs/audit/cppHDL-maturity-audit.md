# CppHDL 成熟度审计 (Phase 6c W0 门槛报告)

| 字段 | 值 |
|------|-----|
| 日期 | 2026-09-16 |
| 审计目标 | 验证 CppHDL 现有基础设施能否支撑 cf::plugin 的 elaboration 重解释（Phase 6c M1-M5） |
| 审计范围 | `ch::Component` / `ch_reg` / `ch_mem` / `ch_uint` / `node_builder` / `Simulator` / `toVerilog` / `ch_state_machine` |
| 审计方法 | 完整源码阅读（`/workspace/project/CppHDL/`），关键 API 逐个确认 |
| 结论 | **通过** — CppHDL 完整可支撑 Phase 6c，仅 `ch_state_machine` 有简化（不影响单周期组件） |

---

## 1. 审计结论（先给）

| 维度 | 状态 | 证据 |
|------|------|------|
| `ch::Component` + `create_ports()` + `describe()` + `build()` | ✅ 完整可用 | `/workspace/project/CppHDL/include/component.h:29-31` + `src/component.cpp:124-139` |
| `ch_uint<N>` + `ch_bool` + 操作符（+ - << >> == != & \| ^ select bits）| ✅ 完整可用 | `include/core/operators.h` 聚合 5 个 per-category 头 |
| `ch_reg<T>` + 非阻塞赋值 `operator->next` + `operator<<=` | ✅ 完整可用 | `include/core/reg.h:46-56` |
| `ch_mem<T, N>` + `sread/write/aread` 完整接口 | ✅ 完整可用 | `include/core/mem.h`（306 行）|
| `node_builder::instance()` 单例 + `build_literal/input/output/register/mux/operation/bits/bit_select` | ✅ 完整可用 | `include/core/node_builder.h:23-216` |
| `ch_op` 枚举覆盖所有硬件操作 | ✅ 完整可用 | `include/core/lnodeimpl.h:41-75`（add/sub/mul/div/mod/and/or/xor/not/eq/ne/lt/le/gt/ge/shl/shr/sshr/neg/bit_sel/bits_extract/bits_update/concat/sext/zext/mux/and_reduce/or_reduce/xor_reduce/rotate_l/rotate_r/popcount/assign）|
| `Simulator::tick()` + `eval_sequential/combinational/reset` | ✅ 完整可用 | `include/simulator.h:79-84` + `set_input_value/get_value for ch_uint<N>/ch_bool/Bundle` (`simulator.h:386-440`) |
| `toVerilog(filename, ctx)` 直接生成 Verilog | ✅ 完整可用 | `include/codegen_verilog.h:33` + `src/codegen_verilog.cpp`（866 行）|
| `ch_state_machine<S, N>` DSL | ⚠️ 简化实现 | `include/chlib/state_machine.h:183-185` `current_state()` 返回 entry_state（line 226 "simplified implementation"）|
| `chlib/stream` 流式 API + `stream_halt_when/takeWhen/throwWhen` | ✅ 完整可用 | `include/chlib/stream.h:92-110` |
| `__io` / `__in` / `__out` 端口宏 | ✅ 完整可用 | `include/core/io.h:300-301` |

---

## 2. 关键 API 详细审计

### 2.1 ch_reg 非阻塞赋值（Phase 6c 关键）

`include/core/reg.h:46-56`：

```cpp
// Conversion operators
operator lnode<T>() const;
lnode<T> as_ln() const;

const next_type *operator->() const { return __next__.get(); }

// 非阻塞赋值操作符（关键！）
template <typename U> void operator<<=(const U &value) const {
    if (__next__) {
        __next__->next = value;
    }
}
```

**审计判定**：✅ **`reg->next = expr` 或 `reg <<= expr` 都发射下一个周期值**（类似 SpinalHDL 的 `reg := value` 和 `RegNext`）。这正是 Phase 6c 需要的"elaboration 时赋值 → 硬件时序寄存器"。

### 2.2 ch_mem 读写接口（Phase 6c M2 关键）

`include/core/mem.h:153-234`：

```cpp
// 异步读（不依赖时钟）
read_port aread(const ch_uint<addr_width> &addr, ...) const;

// 同步读（带 enable）
read_port sread(const ch_uint<addr_width> &addr,
                const ch_bool &enable = ch_bool(true), ...) const;

// 写（带 enable）
write_port write(const ch_uint<addr_width> &addr, const U &data,
                const E &enable = ch_bool(true), ...);
```

**审计判定**：✅ **`sread(addr, ch_bool(true))` 默认读 + `write(addr, data, enable)` 默认带 enable** —— 这正是 `array_store` 后端切换 `ch_mem` 的目标 API。

### 2.3 node_builder 单例工厂（Phase 6c M1 关键）

`include/core/node_builder.h:23-216`：

```cpp
class node_builder {
public:
    static node_builder &instance() {  // 单例
        static node_builder instance;
        return instance;
    }
    
    template <typename T>
    lnodeimpl *build_literal(T value, ...);
    
    template <typename T>
    std::pair<regimpl *, proxyimpl *> build_register(  // 返回 {reg, proxy}
        lnodeimpl *init_val = nullptr, lnodeimpl *next_val = nullptr, ...);
    
    template <typename Cond, typename TrueVal, typename FalseVal>
    lnodeimpl *build_mux(const lnode<Cond> &cond, ...);  // mux
};
```

**审计判定**：✅ **所有 hardware 节点都通过 `node_builder::instance()` 创建** —— 这正是 cf::plugin `at_stage` 闭包要发射的目标 API。`build_register` 返回 `{reg, proxy}` 对，可直接接到 `ch_reg<T>` 构造。

### 2.4 Simulator 完整周期 API（Phase 6c M3 关键）

`include/simulator.h:79-84, 386-440`：

```cpp
void tick();                  // 单周期推进
void eval();                  // 单次求值
void eval_sequential();       // 求值时序逻辑
void eval_combinational();    // 求值组合逻辑
void reset();                 // 复位
void tick(size_t count);      // 多周期推进

// IO 接口
template <typename T, typename Dir>
void set_input_value(const ch::core::ch_in<T> &port, uint64_t value);
template <unsigned N>
void set_input_value(const ch::core::ch_uint<N> &signal, uint64_t value);
template <typename T>
const sdata_type get_value(const ch::core::ch_out<T> &port) const;
template <unsigned N>
const sdata_type get_value(const ch::core::ch_uint<N> &signal) const;
```

**审计判定**：✅ **`tick()` + `set_input_value/get_value` 完整覆盖单周期组合 + 时序逻辑仿真**。`eval_sequential/combinational` 三步求值模型（`simulator.cpp` 实现）。

### 2.5 toVerilog 完整 Verilog 输出（Phase 6c 关键）

`include/codegen_verilog.h:33` + `src/codegen_verilog.cpp:18-94`：

```cpp
void toVerilog(const std::string &filename, ch::core::context *ctx);
// verilogwriter::print(): print_header (port decl) + print_body (assign/always_ff) + print_footer (endmodule)
```

**审计判定**：✅ **直接从 `ctx` 的 lnode DAG 输出 Verilog**。支持 input/output/reg/op/mux/bits_update/literal 7 类节点打印（`codegen_verilog.h:64-71`）。

### 2.6 ch_state_machine 简化（Phase 6c MMU/PTW 推迟依据）

`include/chlib/state_machine.h:183-185, 226-227`：

```cpp
StateEnum current_state() const {
    // For now, return entry state - full implementation needs simulator integration
    return entry_state;  // ← 简化！
}
void build() {
    // This is a simplified implementation - full implementation would
    // need to generate proper combinational logic for state transitions
    // ...
    state_reg->next = next_state;  // ← 只生成 next-state reg 写入
}
```

**审计判定**：⚠️ **ch_state_machine 只能生成 Verilog 的 state-reg 写入，不能在 CppHDL simulator 内推进状态机**。意味着：

- ✅ **Phase 6c M1-M5（单周期组合 + 时序组件）**：完全可用 CppHDL simulator
- ⚠️ **MMU/PTW 多周期算法**：**必须依赖 Verilator 仿真** 或生成的 Verilog + 第三方 simulator
- **Phase 6c 范围纪律**：M1-M5 不涉及多周期 FSM（MMU 推迟到 Phase 6d）

---

## 3. ch_bool 转换纪律检查（Phase 6c M1 强制约束）

**审计目的**：验证 `ch_bool` 的 boolean 转换在 C++ 上下文（`if`/`while`/`!`/`?:`/`&&`/`||`）中的行为，确定"禁止运行期分支"纪律的实际保障机制。

**审计方法**：
- grep `operator bool` 在 `include/core/bool.h` 和 `include/chlib/...`
- 读 `src/lnode/bool.cpp` 中 `operator bool()` 的实现逻辑
- 验证 C++17 contextual conversion 对 `explicit operator bool()` 的使用规则

**审计结论**：⚠️ **`ch_bool` 的 boolean 转换纪律**不由类型系统保证，**靠 CI grep 兜底**

| 维度 | 实际情况 | 含义 |
|------|---------|------|
| `ch_bool` 提供 `operator bool()` | ✅ 是 | `core/bool.h:48` 声明 |
| `operator bool()` 限定符 | `explicit` | 显式转换，禁止隐式 |
| C++17 contextual conversion 使用 explicit 转换函数 | ✅ 是 | `if`/`while`/`!`/`?:` 等上下文**会**使用 `explicit operator bool()` |
| `if (ch_bool_var) {}` 编译 | ✅ **会编译通过**（与"W0 审计"原结论冲突，详见勘误） | contextual conversion 触发 explicit 转换 |
| `operator bool()` 对符号节点（非 const literal）的返回值 | `false`（`src/lnode/bool.cpp:51` 硬编码） | 静默走 false 分支 |
| 静默 false 分支后果 | DAG 静默截断（**比编译错误更危险**） | 业务代码无任何编译期保护 |

**勘误说明**（2026-09-17 by Oracle review）：
- 原 W0 审计"ch_bool 无 operator bool() 隐式转换，if(ch_bool) 编译失败"结论**不成立**——C++17 标准 [conv.general] 明确 contextual conversion to bool 使用 explicit 转换函数
- W0 PoC #6 `cpphdl_poc_chbool_contextual_conversion` 测试 line 159-167 实际**预期编译通过**（不是预期失败）；line 175 失败的真实原因：`static_cast<bool>(x)` 对非常量 ch_bool 静默返回 false，**不是**原 commit 16f942e 所说的"ch_bool(false, "name") 双参构造签名不匹配"

**Phase 6c M1+ 纪律条款（修正版）**：

> "at_stage 回调内禁止对 ch_bool/ch_uint 值使用 `if`/`?:` —— **类型系统不提供保护**（`if(ch_bool)` 编译通过且静默取 false），必须由 CI grep 静态检查强制（`tools/check_plugin_portability.sh` v2.0 Check 5）"

**M2 启动前必须落地**：
- [ ] `tools/check_plugin_portability.sh` Check 5 验证 grep 规则覆盖 `if (ch_bool_*)` / `if (.*_chbool)` / `if (halt_*)` 等模式
- [ ] 误报风险评估：`is_auipc` 含 `is_`、`en` 关键词太短需精确化
- [ ] 在 M2 PoC #2 写至少 1 个 case 故意写 `if (ch_bool_var)` 验证 grep 能抓到（防止 grep 规则本身失效）

---

## 4. Phase 6c PoC 验收清单（Day 2-3 实测）

W0 通过后立即执行的最小 PoC（验证链路完整性）：

| # | PoC | 输入 | 验收 |
|---|-----|------|------|
| 1 | `ch::ch_device<HelloComponent>` 顶层组装 | 单 `Component`，1 个 `ch_reg<ch_uint<8>>`，时钟驱动自增 1 | `ch::toVerilog("hello.v")` 输出可被 Verilator 编译 |
| 2 | 端口绑定测试 | `ch_in<ch_uint<8>>` + `ch_out<ch_uint<8>>` | `Simulator::set_input_value/get_value` 往返正确 |
| 3 | `ch_mem` 读写测试 | `ch_mem<ch_uint<8>, 16>` + `sread(addr)` + `write(addr, data, en)` | 写后读返回相同值 |
| 4 | Mux 树测试 | `select(cond, a, b)` 链 4 层 | Verilog 输出包含正确 mux 节点 |
| 5 | `ch_bool` 编译期错误测试 | 写 `if (some_ch_bool) {}` | 期望**编译失败**（纪律保证）|

**PoC 通过 → 进入 M1 启动**，PoC 失败 → 报告根因 + 回退方案（推迟 M1，补 CppHDL）。

---

## 5. 已知风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| `ch_state_machine` 简化 | MMU/PTW 多周期算法不能在 CppHDL sim 跑 cycle-accurate | Phase 6c 范围纪律：M1-M5 不涉及多周期；MMU 推迟到 Phase 6d 用 Verilator |
| `ch_bool` 无 `operator bool()` 纪律可能被未来 PR 破坏 | "编译期强制无运行期 if(ch_bool)" 失效 | M1 加入 grep 检查：`tools/check_plugin_portability.sh` 新规则 |
| `ch_in<ch_uint<N>>` Bundle 端口语义可能与 `ch_in<MyBundle>` 行为不一致 | 接口契约需在 M2 明确 | M2 第一个 2-stage PoC 验证 |

---

## 6. 审计签字

- 审计人：Sisyphus-Junior (cf_plugin 重构路线规划)
- 审计日期：2026-09-16
- 审计范围：Phase 6c M1-M5 必需 CppHDL 能力
- 结论：**通过**
- 下一步：进入 M1（uint_t.h 翻转）+ W0 Day2-3 PoC 实测