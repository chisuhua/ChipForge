# Phase 6c plugin-elaboration-substrate 教训与模式 (CH_MEM 模式)

> **沉淀自 Phase 6c W0-M6 实施期间 (10 commits: `25e2672` → `5fe72fe`, 2026-09-16 → 2026-09-20, v0.3.0 + v0.3.1 发布)**
> **目的**: 收集 `cf::plugin` 从"每周期仿真器"翻转"elaboration DSL"过程中的 15 类行级陷阱 + 7 个可复用模式, 供 Phase 6d+ 新 Plugin 作者参考。
> **关联文档**:
> - **方法学评估**: [`docs/methodology/plugin-style-design-methodology-v1.md`](../methodology/plugin-style-design-methodology-v1.md) (Phase 1.4 v1, 6 维度 × 3 边界, **superseded by v2.0 (Phase 6c W9)** + Phase 6c elaboration chapter)
> - **审计报告**: [`docs/audit/cppHDL-maturity-audit.md`](../audit/cppHDL-maturity-audit.md) (W0 报告)
> - **研究文档**: [`docs/research/phase6c-elaboration-pattern-study.md`](../research/phase6c-elaboration-pattern-study.md) (Phase 6c 立项前研究)
> - **OpenSpec archive**: `openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/` + `archive/2026-09-20-fix-5stage-mux-segv-elaboration/`

---

## 一、`CF_PLUGIN_USE_CH_MEM` 编译开关陷阱 (3 类)

### 1.1 业务代码必须**双文件分离** (`<name>.h` TLM + `<name>_chmem.h` CH_MEM)

```cpp
// ❌ 错误: 同一文件混合 TLM + CH_MEM 模式
// ip/cpu/plugins/reg_file.h
#ifdef CF_PLUGIN_USE_CH_MEM
  std::array<ch_reg<ch_uint<32>>, 32> regs_;  // CH_MEM: ch 类型
#else
  std::array<uint32_t, 32> regs_;              // TLM: POD
#endif

// ✅ 正确: 双文件分离, 通过 #ifdef 在 CMakeLists.txt 选择
// ip/cpu/plugins/reg_file.h (TLM 模式, 总是编译)
class RegFilePlugin : public PluginBase { /* TLM 业务 */ };

// ip/cpu/plugins/reg_file_chmem.h (CH_MEM 模式, 仅 #ifdef CF_PLUGIN_USE_CH_MEM)
#ifdef CF_PLUGIN_USE_CH_MEM
class RegFilePlugin : public PluginBase { /* CH_MEM 业务 */ };
#endif
```

**`check_plugin_portability.sh` v2.0 Check 2 强制**: `_chmem.h` 文件必须含 `ch_*`; TLM 文件不应含 `ch_*`。

**理由**: 单一文件混合会导致:
- TLM 模式编译时 ch_mem/ch_reg 触发 ch::core::context 找不到 → 段错误
- CH_MEM 模式编译时 POD 操作符重载缺失 → 编译错误
- CI 静态检查无法区分模式, 必须物理分离

### 1.2 `uint_t<N>` 在两种模式下是不同类型

```cpp
// TLM 模式: uint_t<32> = uint32_t (POD)
// CH_MEM 模式: uint_t<32> = ch::core::ch_uint<32> (硬件类)

// ❌ 错误: 业务代码依赖 POD 行为 (如 sizeof, printf 格式)
printf("rs1 = %u\n", rs1);   // TLM 模式 OK, CH_MEM 模式编译失败 (ch_uint 无 %u)

// ✅ 正确: 用 ch_uint<N> 的 API (兼容两种模式)
auto rs1 = dec.rs1;           // 直接赋值
n->operator()(KeyType::RS1) = rs1;  // PayloadStore 装代理 (TLM: POD, CH_MEM: ch 句柄)
```

**`uint_t.h` 静态断言限制**: `static_assert(is_unsigned<uint_t<8>>::value)` 在 TLM 模式有效, CH_MEM 模式删去 (ch_uint 不是 POD)。

### 1.3 `array_store<T, N>` 的 `commit()` 在 TLM 模式是 no-op

```cpp
// TLM 模式: commit() 是 no-op (单缓冲, 即写即读)
// CH_MEM 模式: commit() 交换 first_/second_, 读返回上一周期 commit 值

// ❌ 错误: 假设 TLM 模式 commit 后读立即可见
pb.register_commit_hook([this] { tags_.commit(); });
// TLM 模式: no-op, 读仍可见
// CH_MEM 模式: 读返回**上一周期**值 (!!)

// ✅ 正确: TLM 兼容代码 (commit 在 TLM 是 no-op, 不影响行为)
pb.register_commit_hook([this] { tags_.commit(); });
// 业务代码透明切换, 不需要 if/else 分支
```

**`storage.h` 内部 #ifdef 隔离**: `commit()` 函数体在 CH_MEM 模式才执行 swap, TLM 模式是空函数。

---

## 二、at_stage 闭包内的纪律 (3 类)

### 2.1 ❌ 禁止运行期 `if (ch_bool_var)`

```cpp
// ❌ 致命错误: 编译期通过 + 静默求值错误
ch_bool stall;
pb.at_stage("decode", NORMAL, [this, &pb, stall] {
  if (stall) { return; }  // ch_bool::operator bool() 静默取 false!
});

// ✅ 正确: 用 select 显式表达条件
pb.at_stage("decode", NORMAL, [this, &pb, stall] {
  // 不 return, 而是用 select 把 stall 接到数据流
  auto dec_rs1 = select(stall, ch_uint<32>(0_d), real_rs1);
  n->operator()(KeyType::RS1) = dec_rs1;
});
```

**根因** (来自 `phase6c-elaboration-pattern-study.md` §3):
- `ch_bool` 有 `explicit operator bool()` (`core/bool.h:48`)
- C++17 contextual conversion 让 `if(ch_bool_var)` **编译期通过**
- 调用 `explicit operator bool()` 在 elaboration 期**静默返回 false**
- 业务代码一处 `if` 就静默截断 DAG, **比 SEGV 更糟**

**`check_plugin_portability.sh` v2.0 Check 5**: CI grep 检测 `if (.*ch_bool|halt|stall|flush|en|is_)` 模式, 警告不通过。

**Scala 等价对比**: SpinalHDL 的 `when(cond) { ... }` 是 Scala 控制流, **类型系统强制 `Bool.toBoolean()`**。C++ 没这个纪律, 必须 CI grep 兜底 (ADR-040 v2.0 §3 Tier-1 Check 5)。

### 2.2 ❌ 禁止 `if (cond) return;` 早返

```cpp
// ❌ 错误: 早返依赖 "return 后什么也不做" 隐式语义
pb.at_stage("refill", LATE, [this]() {
  auto n = refill_node_;
  if (!n) return;
  cf::plugin::bool_t hit = n->operator()(g_hit);
  if (hit) return;  // 命中无需 refill, 早返
  write_set(idx, tag, mem_data);
});

// ✅ 正确: 显式 if/else 全分支展开
pb.at_stage("refill", LATE, [this]() {
  auto n = refill_node_;
  if (!n) {
    // n 缺失: no-op, 显式注释
  } else {
    cf::plugin::bool_t hit = n->operator()(g_hit);
    if (hit) {
      // 命中: storage 保持不变 (no-op, 显式注释)
    } else {
      // miss: 执行 refill
      write_set(idx, tag, mem_data);
    }
  }
});
```

**`check_plugin_portability.sh` Check 1 强制**: at_stage 闭包内禁 `if (cond) return;`。

**Phase 6 形态** (推迟): 用 `chlib::when(cond) { ... }` 模板。

### 2.3 ❌ 禁止 `pb.run()` 在 Plugin::build() 内调用

```cpp
// ❌ 致命错误: Plugin::build() 是声明期, 不是执行期
void MyPlugin::build(PipeBuilder& pb) {
  // ... at_stage 注册 ...
  pb.run();  // 死循环 (在 build 内调 run, run 又触发 at_stage 闭包)
}

// ✅ 正确: Plugin::build() 只注册 at_stage, 不触发执行
void MyPlugin::build(PipeBuilder& pb) {
  pb.at_stage("foo", NORMAL, [this]() { /* 业务 */ });
  pb.at_stage("bar", LATE, [this]() { /* 业务 */ });
  // 不调用 pb.run(), 由顶层 PipeBuilder 调度
}
```

**`check_plugin_portability.sh` Check 3 强制**: Plugin::build() 内不调 `pb.run()`。

**`pb.run()` vs `pb.elaborate()`**:
- TLM 模式 (deprecated): `pb.run()` 每周期执行所有 at_stage 闭包
- CH_MEM 模式 (新正道): `pb.elaborate(ctx)` 一次性发射 lnode DAG, 业务代码**不感知**模式

---

## 三、`PayloadStore` cell 管理 (3 类)

### 3.1 CH_MEM 模式下 cell miss 抛异常 (v0.3.1 M6 fix)

```cpp
// ❌ 错误: CH_MEM 模式下读未填充 cell 默认构造 null impl → 下游 SEGV
ch_uint<32> val = n->operator()(KeyType::DECODE);  // cell miss → T{} → null impl
auto sum = val + ch_uint<32>(1_d);  // SEGV in muximpl::create_instruction

// ✅ 正确 (v0.3.1 后): const 读路径抛 fail-fast 异常
const T& PayloadStore::get(key) const {
  if (cell missing) throw std::runtime_error("PayloadStore cell missing: " + key.name() + "...");
  return std::any_cast<const T&>(cell);
}
```

**v0.3.1 M6 修复**: `payload.h::PayloadStore` 拆分读/写路径:
- `const T& get(key)` 读 miss **抛** `std::runtime_error`
- `T& get(key)` 非 const 写访问器**保留** `emplace-on-miss`

**配套工厂预填充** (`cpu_factory_chmem.h::build_cpu()`):
- 在 `pb->build()` 之前注册 EARLY at_stage 闭包
- 为 6 个 stage (fetch/decode/execute/memory/writeback/branch) 显式预填充
- 使用真实 `ch_literal<0, W>{}` 字面值 (非 null 句柄)

**`test_payload_store_miss_throws.cpp`** 验证:
- TEST_CASE 1: CH_MEM 模式 `get(key)` 未填 → 抛异常含 key.name()
- TEST_CASE 2: CH_MEM 模式写后读 → 正常
- TEST_CASE 3: TLM 兼容模式行为不变

### 3.2 业务代码写路径仍用 emplace-on-miss

```cpp
// ✅ 正确: 写路径用 emplace, 业务代码 `n(key) = val` 透明
T& PayloadStore::get(key) {
  if (cell missing) emplace-on-miss;  // T{} 默认构造
  return std::any_cast<T&>(cell);
}
```

**`n->operator()(KeyType::X) = expr;` 模式** (CH_MEM):
- `operator()` 非 const 调用 `T& get(key)` → 写访问器 → emplace-on-miss
- `= expr` 调用 `T::operator=(const T&)` 发射 lnode DAG assign 节点

### 3.3 跨 stage 共享 cell 必须用同一 PipeNode

```cpp
// ❌ 错误: 不同 stage 名称 → 不同 PipeNode → 不同 PayloadStore
pb.at_stage("decode", NORMAL, [this, &pb] {
  auto n = pb.node_of_logic_stage("decode");
  n->put(g_idx, idx);  // 写入 decode 节点
});
pb.at_stage("refill", LATE, [this, &pb] {
  auto n = pb.node_of_logic_stage("refill");  // 不同节点!
  auto idx = n->operator()(g_idx);  // 读到默认值 0!
});

// ✅ 正确: 共享同一 PipeNode
auto n = pb.node_of_logic_stage("decode");  // shared_ptr<PipeNode>
pb.at_stage("decode", NORMAL, [this, &pb, n] {
  n->put(g_idx, idx);
});
pb.at_stage("refill", LATE, [this, &pb, n] {
  auto idx = n->operator()(g_idx);  // 共享 cell
});
```

**根因**: `PipeBuilder::at_stage()` 在 `nodes_` map 中以 stage_name 为 key 创建 PipeNode。Payload Key 按指针身份匹配, 但**不同 PayloadStore 的同名 Key 互不干扰** (来自 `docs/lessons/phase-1.2-l1cacheplugin.md` §1.1, Phase 1.4 沿用)。

---

## 四、RegFile / Storage 的特殊陷阱 (2 类)

### 4.1 ❌ 禁止 `array_store<ch_reg<ch_uint<32>>, 32>` 错误组合

```cpp
// ❌ 致命错误: array_store 要求 T 是 trivially_copyable, ch_reg 不是
array_store<ch_reg<ch_uint<32>>, 32> regs_;
// static_assert(is_trivially_copyable<ch_reg<...>>) 编译失败

// ✅ 正确: 用 32 个独立 ch_reg (不用 array_store)
std::unique_ptr<std::array<ch_reg<ch_uint<32>>, 32>> regs_;
// 业务代码直接 regs_[i] 访问 (返回 ch_reg 引用)
```

**根因**: `array_store` 设计意图是**纯数据容器** (如 tags/data/valid), 不是**硬件节点容器**。ch_reg 是 lnode 包装, 需要 ch::core::context 绑定 → 不可位拷贝。

**Phase 6c M3 Prereq-5 修复**: 删除原 spec 错误方案, 改用"32 个独立 ch_reg"。

### 4.2 ❌ 禁止 `static thread_local` 跨 at_stage 闭包共享状态

```cpp
// ❌ 致命错误: dual static thread_local → decode 闭包和 writeback 闭包各自独立
static thread_local std::array<ch_reg<ch_uint<32>>, 32> regs{};  // decode 闭包持有
static thread_local std::array<ch_reg<ch_uint<32>>, 32> regs{};  // writeback 闭包持有 (不同对象!)
// 写回的 regs 和读出的 regs 是两个不同对象 → 寄存器堆功能失效

// ✅ 正确: 提升 regs 为 plugin 实例成员
class RegFilePlugin : public PluginBase {
 private:
  std::unique_ptr<std::array<ch_reg<ch_uint<32>>, 32>> regs_;
  // 单一数组供读/写闭包共享
};
```

**Phase 6c M3 Prereq-3 修复** (Oracle A3 关键缺陷): dual static thread_local 架构缺陷, 改为 plugin 实例成员。

**`get_regs()` 延迟初始化** (`reg_file_chmem.h:148-158`):
- ch_reg<T> 构造需活跃 ch::core::context (发射 regimpl 节点)
- Plugin 对象构造时 context 可能未激活
- `get_regs()` 首次调用发生在 elaborate() 期间 (at_stage 闭包), 此时 ctx_swap 已生效

**Oracle M3/W8 #95 警告**: 不要用 `unordered_map<void*, array>` 或 `thread_local + cached_ctx` (地址复用场景 use-after-free)。**实例成员**是唯一安全选项。

---

## 五、Simulator 与 Verilog 生成 (2 类)

### 5.1 `ch::Simulator::tick()` 调用必须 reset() 后

```cpp
// ❌ 错误: 不 reset 直接 tick, 寄存器初值未定义
ch::Simulator sim(ctx);
sim.tick();  // 寄存器初值是 random / null impl

// ✅ 正确: sim.reset() 后再 tick
ch::Simulator sim(ctx);
sim.reset();
for (int cycle = 0; cycle < N; ++cycle) {
  sim.set_input_value(io.a, val_a);
  sim.set_input_value(io.b, val_b);
  sim.tick();
}
```

### 5.2 `ch::toVerilog()` 输出文件路径必须可写

```cpp
// ❌ 错误: /tmp 无写权限 / 目录不存在
ch::toVerilog("/nonexistent/dir/foo.v", ctx);  // silent failure

// ✅ 正确: 先创建目录 + 验证可写
const std::string out_file = "/tmp/cpphdl_poc_hello.v";
ch::toVerilog(out_file, ctx);
std::ifstream f(out_file);
REQUIRE(f.is_open());  // 验证文件存在
REQUIRE(!verilog.empty());  // 验证非空
```

**Phase 6c M6 SEGV 教训**: `muximpl::create_instruction` 解引用 null → SEGV → 必须 fail-fast 验证。

---

## 六、测试隔离与上下文管理 (2 类)

### 6.1 ch::core::context 的 thread_local 状态污染

```cpp
// ❌ 错误: 多个 TEST_CASE 顺序跑时, 前一个 ch_device 析构还原 ctx_curr_=nullptr
TEST_CASE("a") {
  ch::ch_device<Hello> dev;  // 构造时 ctx_curr_ = dev's ctx
  // ...
}  // 析构时 ctx_swap 还原 ctx_curr_ = nullptr

TEST_CASE("b") {
  ch_bool b(true);  // 调用 node_builder::build_literal → ctx_curr_=nullptr → fail
}
```

**`test_cppHDL_hello_poc.cpp:168-175` v0.3.1 fix** (`5fe72fe`):
- 在每个 SECTION 入口前 `ch::core::context chbool_ctx("ctx"); chbool_ctx.set_as_current_context();` 显式设置 active ctx
- **不要依赖** `ch::ch_device<T>` 的 ctx 持久性

### 6.2 PoC 测试必须显式管理 context lifecycle

```cpp
TEST_CASE("cpphdl_poc_chbool_contextual_conversion") {
  // 每个 SECTION 入口前显式 set_as_current_context
  ch::core::context ctx("chbool_poc_ctx");
  ctx.set_as_current_context();
  
  SECTION("ch_bool to bool in if") {
    ch_bool a(false, "a_bool");  // 现在 ctx 活跃
    // ...
  }
  SECTION("ch_bool 显式转换") {
    ch_bool x(true, "x_bool");
    // ...
  }
}
```

**Phase 6c v0.3.1 M6 已知剩余 issue**: `chipforge_tests_chmem` 全量跑时 `cpphdl_poc_chbool_contextual_conversion` 失败 (单独跑 PASS), 根因是 context pollution。Phase 6d PoC follow-up OpenSpec change 跟踪修复。

---

## 七、可复用模式 (7 个)

### 模式 #1: RegFilePlugin 实例成员 + 延迟初始化

```cpp
template <typename T, std::size_t N_REGS = 32>
class RegFilePlugin : public PluginBase {
 private:
  using regs_array_t = std::array<ch_reg<ch_uint<32>>, N_REGS>;
  std::unique_ptr<regs_array_t> regs_;

  regs_array_t& get_regs() {
    if (!regs_) {
      regs_ = std::make_unique<regs_array_t>();
      for (size_t i = 0; i < N_REGS; ++i) {
        (*regs_)[i] = ch_reg<ch_uint<32>>(
            ch_uint<32>(ch::core::ch_literal<0, 1>{}),  // 真实字面值 (非 null impl!)
            ("reg_" + std::to_string(i)).c_str());
      }
    }
    return *regs_;
  }
};
```

**应用**: `reg_file_chmem.h:131-158`。Phase 6d+ 任何含状态寄存器的 Plugin (Cache refill FSM / MMU PTW) 沿用。

### 模式 #2: select 树分发表 (替代 ROM)

```cpp
// RISC-V ALU 11-op select 树 (借鉴 CppHDL/examples/riscv-mini/src/rv32i_alu.h:172)
ch_uint<32> IntAluPlugin::compute(ch_uint<32> a, ch_uint<32> b, ch_uint<3> funct3, ch_uint<7> funct7) {
  ch_uint<32> result(0_d);
  result = select(funct3 == 0x0_d && funct7 == 0x00_d, a + b,        result);  // ADD
  result = select(funct3 == 0x0_d && funct7 == 0x20_d, a - b,        result);  // SUB
  result = select(funct3 == 0x1_d,                    a << b,        result);  // SLL
  result = select(funct3 == 0x2_d,                    a < b,         result);  // SLT
  result = select(funct3 == 0x3_d,                    a < b,         result);  // SLTU
  result = select(funct3 == 0x4_d,                    a ^ b,         result);  // XOR
  result = select(funct3 == 0x5_d && funct7 == 0x00_d, a >> b,        result);  // SRL
  result = select(funct3 == 0x5_d && funct7 == 0x20_d, signed_sra(a, b), result);  // SRA
  result = select(funct3 == 0x6_d,                    a | b,         result);  // OR
  result = select(funct3 == 0x7_d,                    a & b,         result);  // AND
  return result;
}
```

**应用**: `int_alu_chmem.h:172`。Phase 6d DecoderPlugin 完整 RV32I 主 opcode 译码沿用 11 层 select 嵌套。

### 模式 #3: BranchPlugin 6-op select 树

```cpp
ch_bool BranchPlugin::take_branch(ch_uint<32> rs1, ch_uint<32> rs2, ch_uint<3> funct3) {
  return select(funct3 == 0x0_d, rs1 == rs2,        // BEQ
         select(funct3 == 0x1_d, rs1 != rs2,        // BNE
         select(funct3 == 0x4_d, signed_lt(rs1, rs2),   // BLT
         select(funct3 == 0x5_d, signed_ge(rs1, rs2),   // BGE
         select(funct3 == 0x6_d, rs1 < rs2,         // BLTU
         select(funct3 == 0x7_d, rs1 >= rs2,        // BGEU
                ch_bool(false))))));  // default
}
```

**应用**: `branch_chmem.h:172`。Phase 6d 完整 B-type 沿用。

### 模式 #4: HazardPlugin RAW 检测 OR-merge

```cpp
ch_bool HazardPlugin::raw_hazard(ch_uint<5> id_rs1, ch_uint<5> id_rs2, 
                                  ch_uint<5> ex_rd, ch_uint<5> mem_rd, ch_uint<5> wb_rd) {
  ch_bool ex_match = (id_rs1 == ex_rd) || (id_rs2 == ex_rd);  // EX 阶段 RAW
  ch_bool mem_match = (id_rs1 == mem_rd) || (id_rs2 == mem_rd);  // MEM 阶段 RAW
  ch_bool wb_match = (id_rs1 == wb_rd) || (id_rs2 == wb_rd);  // WB 阶段 RAW
  return ex_match || mem_match || wb_match;  // OR-merge
}
```

**应用**: `hazard_chmem.h:172`。**CtrlLink wiring**:
```cpp
pb.register_ctrl_link("decode", std::make_shared<CtrlLink>(halt_when(hazard.raw_hazard(...))));
```

### 模式 #5: stage_payload_connector 自动插 ch_reg (M2)

```cpp
// 跨 stage Payload 自动插 ch_reg (借鉴 SpinalHDL M2S connection)
pb.register_stage_payload_connector<ch_uint<32>>(
    "decode",
    KeyType::RS1,
    [](ch::core::lnodeimpl* prev, ch::core::ch_bool stall, ch::core::ch_bool flush, 
       const std::string& name) -> ch::core::lnodeimpl* {
      // 用 chlib::pipeline_reg 包装
      return chlib::pipeline_reg<32>(prev, stall, flush, name).impl();
    });
```

**应用**: `pipe_builder.h:313-351`。Phase 6d 5-stage 集成时大量使用。

### 模式 #6: 自定义字面值用 `ch::core::ch_literal` (非 null impl)

```cpp
// ❌ 错误: ch_uint<32>(0) → null impl (int 0 → lnodeimpl* nullptr)
ch_reg<ch_uint<32>> reg(ch_uint<32>(0), "reg_name");

// ✅ 正确: ch_uint<32>(ch::core::ch_literal<0, 32>{}) → 真实字面值
ch_reg<ch_uint<32>> reg(ch_uint<32>(ch::core::ch_literal<0, 32>{}), "reg_name");
```

**v0.3.1 M6 修复的根因**: `ch_uint<N>(0)` 和 `ch_bool(0)` 由于 `int 0 → lnodeimpl* nullptr` 是标准转换序列 (优先级高于用户定义 `int → ch_literal_runtime` 链), 匹配到继承的 `logic_buffer(lnodeimpl *node)` ctor → `node_impl_ = nullptr`。上游 CppHDL commit `47af57f fix(core): prevent null pointer hijack` 已修复, 但旧代码仍可能踩坑。

### 模式 #7: TLM↔CH_MEM 字节对标 (Spike 6 协议)

```cpp
// 1. TLM 参考实现 (纯 C++, 不调 cf::plugin 或 ch::*)
static std::vector<T> tlm_reference_simulate(int n_cycles, const Schedule& sched);

// 2. CH_MEM 仿真 (用 ch::ch_device 或 PipeBuilder::elaborate)
static std::vector<T> chmem_simulate(int n_cycles, const Schedule& sched);

// 3. 字节对标 (byte-equal)
bool byte_equal = (tlm_trace == chmem_trace);
REQUIRE(byte_equal);
```

**应用**: `test_elaborate_pipeline2.cpp:181-373` (pipeline2_stall_matrix 16/16 PASS)。**Phase 6d 5-stage PoC 沿用**: TLM 跑 CpuFactory (pb.run()) vs CH_MEM 跑 CpuFactoryChmem (pb.elaborate() + ch::Simulator), byte-equal。

---

## 八、CH_MEM 编译期检查清单 (8 项)

每次新增 `_chmem.h` Plugin, 必须通过:

- [ ] **Check 1**: at_stage 闭包内无 `if (cond) return;` (CI grep)
- [ ] **Check 2**: `_chmem.h` 文件必须含 `ch_*` 类型; TLM 文件不含 `ch_*` (CI grep)
- [ ] **Check 3**: Plugin::build() 内不调 `pb.run()` (CI grep)
- [ ] **Check 4**: 存储声明优先 `array_store` 或 `ch_mem` (WARN, 不阻塞)
- [ ] **Check 5**: at_stage 回调内禁运行期 `if(ch_bool)` (CI grep, ADR-046 豁免)
- [ ] **Check 6**: 源/头文件内禁 `#define CF_PLUGIN_USE_CH_MEM` (仅 CMake 命令行)
- [ ] **Check 7**: TLM-only 文件不应含 ch 类型实例化
- [ ] **Check 8**: PayloadStore CH_MEM get-miss fail-fast (grep 验证 `const T& get(key)` 抛异常)

**`bash tools/check_plugin_portability.sh`** 应输出 `8/8 PASS`。

---

## 九、调试技巧 (3 类)

### 9.1 Verilog 输出快速验证

```bash
# 生成 Verilog 后立即 grep 关键内容
ch::toVerilog("/tmp/foo.v", ctx);
grep "always_ff @(posedge" /tmp/foo.v | wc -l    # 应 >0 (ch_reg 发射)
grep "module" /tmp/foo.v                          # 应有 module 声明
wc -l /tmp/foo.v                                  # 应 > 0 (非空)
```

### 9.2 Simulator node 计数验证

```cpp
ch::ch_device<Hello> dev;
auto& ctx = *dev.instance().context();
INFO("Nodes created: " << ctx.get_nodes().size());  // 应 > 0 (CH_MEM 模式发射 DAG)
```

### 9.3 `[INFO]` 日志定位 DAG 节点

CH_MEM 模式运行时输出大量 `[INFO] Created node ID X (top.<name>) of top.<name> (type, N bits) in context 0xADDR at /path/file.tpp:23` 日志, 每个 DAG 节点一条。**用 `grep "node ID" 配合 wc -l` 验证 DAG 节点数**。

---

## 十、Phase 6d 已知推迟项

| 推迟项 | 原因 | 影响 | 修复计划 |
|--------|------|------|----------|
| riscv-tests RV32I 5 指令 `tohost=1` | 工具链缺失 | 6c 仅 PoC | **Phase 6d.4** |
| Verilator 集成 | 工具链缺失 | 6c 仅 CppHDL sim | **Phase 6d.5** |
| M3/W6 byte-equal #95 | cell put/get CH_MEM 不稳定 | 6c 单元级 OK | **PoC follow-up OpenSpec** |
| 5-stage Decoder + Branch + Hazard 完整 CH_MEM | 6c 仅骨架 + PoC | 6d 完整 | **Phase 6d.1+6d.2+6d.3** |
| MMU/PTW 多周期 FSM | 依赖 ch_state_machine + Verilator | 6c ADR-046 锁定豁免 | **Phase 6d.6** |
| L1Cache refill FSM | 同上 | 6c 同上 | **Phase 6d.7** |
| Harness 迁移 | W9 仅文档 | 6d 实施 | **Phase 6d.8** |

---

## 十一、修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-09-20 | 初版: Phase 6c W0-M6 实施期间 15 类行级陷阱 + 7 个可复用模式 + 8 项编译期检查清单 |

---

*本文档是 Phase 6c CH_MEM 模式实施教训的系统沉淀。Phase 6d+ 新 Plugin 作者**先读本文档了解 15 类陷阱 + 7 个模式 + 8 项检查清单**, 再参考 [`docs/methodology/plugin-style-design-methodology-v1.md`](../methodology/plugin-style-design-methodology-v1.md) (Phase 1.4 v1 方法学评估 + Phase 6c elaboration chapter 待补充) + [`docs/research/phase6c-elaboration-pattern-study.md`](../research/phase6c-elaboration-pattern-study.md) (Phase 6c 立项前研究)。*
