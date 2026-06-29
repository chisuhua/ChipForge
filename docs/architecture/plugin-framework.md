# Plugin 框架架构

| 字段 | 值 |
|------|-----|
| 版本号 | 1.0 |
| 日期 | 2026-06-09 |
| 状态 | **Active (Phase 0 已完成, 5/5 P0 组件稳定)** |
| 关联决策 | [decision-plugin-framework-2026-06-08](../../.omo/drafts/decision-plugin-framework-2026-06-08.md) |
| 适用范围 | ChipForge 插件架构（独立于 CppTLM/CppHDL 框架层） |

---

## 1. 设计动机

ChipForge 的电路设计有两种主要风格：命令式的 `tick()` 风格（每个模块自己实现 `tick()` 推进仿真）和声明式的 Plugin 风格（每个模块通过 `at_stage` 注册阶段回调，调度由 `PipeBuilder` 决定）。本节列出选择 Plugin 风格的五个关键动机。这些论证直接引用决策文档 [`decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md) §2.1-2.3 的三段式论证。

### 1.1 替换 `tick()`：Plugin 是范式不是工具

**问题**：`tick()` 风格中，每个电路模块自己实现 `tick()`，由模块自行决定何时读、写、组合信号。在 `cpptlm::ChStreamModuleBase` 或 `ch::Component` 里，这通常表现为：模块持有 `state_` 字段，构造函数注册 `tick` 回调，仿真器按固定顺序调用。这种模式短期写起来直接，但当模块数量超过 20 个、阶段超过 10 个时，`tick()` 之间的隐式时序约束（"必须先 `tick()` A 才能 `tick()` B"）会散落在调用顺序的细节里，几乎不可能静态验证。

**Plugin 风格的解法**：调度由 `PipeBuilder` 拥有，模块只声明"我在某个阶段做什么"。`PipeBuilder::run()` 按注册顺序执行所有 `at_stage` 回调，阶段间数据通过 `Payload<T>` 显式传递。这意味着时序约束不再是 `tick()` 调用顺序的副作用，而是 `at_stage` 的第一个参数（阶段名）——一旦阶段名重复、阶段顺序错乱，编译期或第一次 `run()` 就能发现。

**关键区分**（决策文档 §2.1）：
- **工具**（tool）：可加可减，不影响业务结构。例如 `printf`、断言库。
- **范式**（paradigm）：决定业务结构，加减会破坏一致性。例如面向对象、函数式、声明式。

Plugin 是范式。决策文档明确指出：如果 Phase1-5 用 `tick()` 风格，Phase6 想升级到 Plugin-style 需要**重写所有 IP 业务逻辑**，不是简单重构。这意味着范式选择必须在 Day 1 落地，Phase 0 必须先打基础。

**佐证**：SpinalHDL/VexRiscv 在工业界验证了 Plugin 范式。VexRiscv 40+ 复杂 Plugin（Cache、MMU、CSR、Debug、Interrupt）共享同一流水线骨架，所有 Plugin 都用 `setup` + `build` 两步注册——这套范式支撑了 RISC-V 生态里最成功的开源 CPU 之一。

### 1.2 类型安全：`Payload<T>` 与编译期检查

`tick()` 风格传递数据时，往往用裸指针（`void*` + 类型强转）或 std::variant（运行时类型检查）。两者都不友好：裸指针错类型时崩在运行时，std::variant 写起来冗长且无法编译期发现类型不匹配。

`cf::plugin::Payload<T>` 用模板把"Key"和"类型"绑定到全局静态对象上：

```cpp
// 阶段 A 的 Plugin
static cf::plugin::Payload<uint64_t> addr_key{"addr"};

// 阶段 B 的 Plugin
auto addr = node(addr_key);  // 编译期就知道是 uint64_t
```

`Payload<T>` 继承 `PayloadKeyBase`，但 `operator()(Payload<T>)` 用模板参数 `T` 锁定取值类型。`PayloadStore::get<T>()` 在运行时还会做 `typeid` 二次校验：即使有人意外地用 `Payload<uint32_t>` 和 `Payload<uint64_t>` 指向同一个全局对象（理论上不会发生，因为是单例），`typeid` 也会在 `get` 时抛 `runtime_error`。

这套设计的三个好处：
1. **编译期捕获**：写错类型时编译失败（`Payload<uint32_t>` 不会接受 `uint64_t` 值）。
2. **Key 自描述**：每个 `Payload` 有名字（`"addr"`），调试时一眼看出是干什么的。
3. **跨阶段解耦**：阶段 A 只声明"我产生 addr"，阶段 B 只声明"我消费 addr"，不互相依赖具体 Plugin 类。

### 1.3 声明式：`at_stage` 而非命令式 `tick`

声明式编程的核心是把"做什么"和"什么时候做"分开。在 Plugin 风格里：
- **做什么**：注册到 `PipeBuilder` 的回调（`std::function<void()>`）
- **什么时候做**：阶段名 + Phase（`EARLY` / `NORMAL` / `LATE`）

对比命令式：

```cpp
// tick() 风格：模块自己安排
void MyModule::tick() {
  if (phase == PHASE_A) do_a();
  if (phase == PHASE_B) do_b();
}

// Plugin 风格：调度由 PipeBuilder 拥有
void MyPlugin::build(PipeBuilder& pb) {
  pb.at_stage("do_a", Phase::NORMAL, [this] { do_a(); });
  pb.at_stage("do_b", Phase::LATE,   [this] { do_b(); });
}
```

声明式写法有三个直接收益：
1. **业务代码无 `if (phase == X)` 分支**：每个回调只关心单一职责。
2. **阶段顺序由 `at_stage` 调用顺序自然形成**：`PipeBuilder` 按注册顺序跑回调，谁先注册谁先执行。
3. **可视化容易**：把 `pb.stage_names()` 打印出来，整个流水线的阶段顺序一目了然。

### 1.4 生命周期：`ctor → setup → build → run` 四阶段

Plugin 风格强制一个明确的生命周期，每个阶段职责清晰：

| 阶段 | 调用方 | 时机 | 派生类任务 |
|------|--------|------|------------|
| `ctor()` | 用户代码 | 构造时 | 声明 `Payload` 静态 Key、初始化成员 |
| `setup(pb)` | `PipeBuilder::build()` | 所有 `build()` 之前 | 跨 Plugin 引用声明（如查找其他 Plugin 注册的节点） |
| `build(pb)` | `PipeBuilder::build()` | 所有 `setup()` 之后 | 注册 `at_stage` 回调（实际生成逻辑） |
| `pb.run()` | 用户代码 | 构建完成后 | 触发所有 `at_stage` 回调按序执行 |

`setup` 和 `build` 分离是一个微妙但重要的设计。如果一个 Plugin 在 `build` 里想引用另一个 Plugin 注册的 `PipeNode`，那个 `PipeNode` 必须已经在 `at_stage` 时被建立——但 `at_stage` 本身只能在 `build` 里调用。这就有循环依赖。

解决方法是 `setup`：在所有 `build` 调用之前，统一让每个 Plugin 声明"我要什么"。`setup` 默认空实现，派生类可以（可选）override。一旦 `setup` 阶段完成，框架已经知道所有跨 Plugin 引用，`build` 阶段再生成阶段回调就无环了。

### 1.5 Phase 调度：`EARLY` / `NORMAL` / `LATE` 三档

`at_stage` 接受一个 `Phase` 参数：`EARLY=0`、`NORMAL=1`、`LATE=2`。Phase 给调度增加一个粗粒度的优先级维度：

- **`EARLY`（早）**：流水线入口准备（取指、解码前导、reset 同步）。
- **`NORMAL`（中）**：核心计算（ALU、访存、CSR 读写）——默认档。
- **`LATE`（晚）**：流水线出口收尾（写回、提交、异常处理）。

注意：`PipeBuilder` 当前的 `run()` 实现是**按注册顺序**调用所有回调，不显式按 Phase 分桶。Phase 字段主要服务于：
1. **文档自描述**：`pb.stage_names()` 配合每个阶段的 Phase 字段，调用方一眼能看出哪个阶段是入口准备、哪个是出口收尾。
2. **未来调度扩展**（Phase 6）：如果将来引入"先跑完所有 EARLY 再跑所有 NORMAL"的分桶调度，Phase 字段已经准备好了。
3. **可读性**：用户在 `at_stage` 时显式选 Phase，强迫思考"这个回调属于流水线的早/中/晚哪一段"。

### 1.6 一句话总结

> Plugin 框架的目的不是"提供调度"，而是"用声明式风格替代 `tick()` 风格"。调度本身仍然由业务代码决定；框架只提供类型安全 Key、阶段回调、状态机、声明式控制这四类基础设施。

详细决策依据见 [decision-plugin-framework-2026-06-08.md §2.1-2.3](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)。

---

## 2. 5 个 P0 组件

Phase 0 锁定 5 个最小可行组件（`PluginBase` / `Payload<T>` / `PipeNode` / `PipeBuilder` / `CtrlLink`）加 1 个编译期类型切换工具（`uint_t<N>` / `bool_t`）。这 5 个组件支撑 Plugin-style 业务逻辑跑起来，但不构成完整框架（完整框架推迟到 Phase 6）。

完整性与最小性边界：
- **包含**：`setup` + `build` 生命周期、类型安全 Key、节点状态机、阶段注册、声明式控制、位宽语义。
- **不包含**：依赖分析、JSON 配置、ScoreBoard、CompareDriver、RTL AST 生成、BundleMapper（全部推迟到 Phase 6）。

**完整 API 参考**：详见 [docs/api/cf_plugin.md](../api/cf_plugin.md)

> 上一节（§1）回答了"为什么需要 Plugin 风格"。本节回答"Plugin 风格的 5 个 P0 组件分别是什么、长什么样"。每个组件给出：设计意图、公共 API 列表（高层视图）、头文件位置、测试文件位置、关键约束。完整方法签名请翻阅 [cf_plugin.md](../api/cf_plugin.md)。

### 2.1 PluginBase

**角色**：所有 Plugin-style IP 的抽象基类。

**设计意图**：极简接口。`PluginBase` 借鉴 VexRiscv 的 `Plugin.scala`（仅 25 行，2 个方法 `setup` + `build`）。ChipForge 走同样的路线：基类只声明生命周期钩子，不提供任何业务能力。业务能力（`at_stage` 注册、跨 Plugin 引用）由 `PipeBuilder` 在 `setup`/`build` 回调时按需提供。

为什么这么极简：
1. **基类稳定**：基类越简单，将来加新能力（Phase 6 的延迟构建、JSON 配置）时不会破坏派生类。
2. **派生类自由**：派生类只 override 必需的方法，没有"必须填的回调"。
3. **禁止 `tick()` 业务重写**（D4 决策）：`tick()` 设为 `private = delete`，派生类无法意外定义同名函数。这把"Plugin-style vs tick()-style"从编码约定升级为编译期约束。

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

| 成员 | 类别 | 用途 |
|------|------|------|
| `PluginBase()` | 构造函数 | 默认构造（`= default`） |
| `virtual ~PluginBase()` | 虚析构 | 默认实现（`= default`），保证派生类正确析构 |
| 拷贝构造 / 拷贝赋值 | 禁用（`= delete`） | 每个 Plugin 独立注册，不应被容器拷贝 |
| `virtual void setup(PipeBuilder&)` | 虚函数 | 跨 Plugin 引用声明，**默认空实现**，派生类可选 override |
| `virtual void build(PipeBuilder&) = 0` | 纯虚函数 | 实际生成逻辑（`at_stage` 注册），**派生类必须实现** |
| `private: void tick() = delete` | 编译期禁用 | 阻止派生类定义 `tick()`（D4 决策强制） |

**关键约束**：
- 派生类**必须** override `build()`，否则仍是抽象类，无法实例化（编译期保证）。
- 派生类**不应** override `tick()`（编译期直接禁用）。
- 派生类**不应**持有时序状态（调度由 `PipeBuilder` 决定，Plugin 应当是无状态的）。

**头文件**：`include/cf/plugin/plugin_base.h`

**测试**：`src/cf_plugin/tests/test_plugin_lifecycle.cpp`（7 个测试用例）

**典型用法**：

```cpp
struct MyPlugin : cf::plugin::PluginBase {
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("greet", cf::plugin::Phase::NORMAL, [] {
      printf("Hello\n");
    });
  }
};
```

### 2.2 Payload<T> + PayloadStore

**角色**：类型安全 Key 模板 + 跨 `PipeNode` 隔离的存储。

**设计意图**：借鉴 VexRiscv `Stageable[T]` 的"全局静态对象作为 Key"模式。每个 `Payload<T>` 实例就是一个全局静态对象，它的地址（`&payload`）就是 Key；模板参数 `T` 决定取值类型。`PayloadStore` 是 Key-Value 容器，存的是 `std::any`，取值时按 `T` 还原。

**为什么不直接用 `std::map<std::string, std::any>`**：
- 字符串 Key 写错不会编译（"adrr" 和 "addr" 编译器看不出区别）。
- 取值时如果类型不对，`std::any_cast` 抛 `bad_any_cast` 异常，错误信息不友好。
- `Payload<T>` 用模板强制把 Key 写成有类型的全局对象，三个问题全部解决。

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

#### `PayloadKeyBase`（类型擦除基类）

| 成员 | 用途 |
|------|------|
| 构造函数 `PayloadKeyBase(name)` | 接受一个名字（用于调试） |
| `name()` | 返回名字 |
| `virtual type()` | 返回 `std::type_info`（运行时类型擦除） |
| `operator<` | 按指针身份比较（全局静态对象地址唯一） |
| 拷贝 / 赋值 | 禁用（单例语义） |

#### `Payload<T>`（类型化 Key）

| 成员 | 用途 |
|------|------|
| 构造函数 `Payload<T>(name)` | 接受名字 |
| `type()` override | 返回 `typeid(T)`（编译期就锁定的类型） |
| `static_type()` 静态 | 编译期常量，返回 `typeid(T)` |
| 拷贝 / 赋值 | 禁用（单例语义） |

#### `PayloadStore`（Key-Value 存储）

| 成员 | 用途 |
|------|------|
| `put<T>(key, value)` | 写入，模板参数 `T` 锁定类型 |
| `get<T>(key) const` / `get<T>(key)` | 读取，返回 `const T&` / `T&`；缺失时默认构造 |
| `has<T>(key)` | 检查 Key 是否存在且类型匹配 |
| `clear()` | 清空所有 Payload |
| `size()` | 返回当前 Payload 数量（调试用） |
| 拷贝 / 赋值 | 禁用 |

**关键约束**：
- **跨 `PipeNode` 隔离**：每个 `PipeNode` 持有一个 `PayloadStore`，不同 `PipeNode` 的同 Key Payload 互不干扰。这意味着两个阶段可以各自用同名 Key（如 `addr`），互不污染。
- **错类型 `get()` 抛 `runtime_error`**：运行时 `typeid` 二次校验，发现类型不匹配立即抛错（不会崩在 `any_cast` 处）。
- **Key 必须全局静态**：`Payload<T>` 的拷贝被禁用，意图是保证 Key 单例（地址作为标识）。

**头文件**：`include/cf/plugin/payload.h`

**测试**：`src/cf_plugin/tests/test_payload.cpp`（8 个测试用例）

**典型用法**：

```cpp
// 全局静态 Key（必须 static / 命名空间作用域 / 静态成员）
static cf::plugin::Payload<uint64_t> addr_key{"addr"};

// 在某个 PipeNode 写入
node.put(addr_key, 0x1000ULL);

// 在另一个 PipeNode 读取
uint64_t addr = node(addr_key);  // 类型安全
```

### 2.3 PipeNode

**角色**：节点（封装 `PayloadStore`） + 简单 valid/ready 状态机。

**设计意图**：`PipeNode` 是阶段间数据 + 状态的容器。每个阶段（`at_stage` 注册的）对应一个 `PipeNode` 实例，由 `PipeBuilder` 自动管理（`unordered_map<std::string, std::shared_ptr<PipeNode>>`）。节点内部：
- **数据**：`PayloadStore`，按 Key 存阶段输出（`Payload<T>` 类型安全）。
- **状态**：5 状态状态机（`IDLE` / `FIRING` / `MOVING` / `BLOCKED` / `CANCELING`），用 valid/ready 握手模拟流水线行为。

为什么需要状态机：即使 Plugin 是声明式的，底层仿真仍然要推进 valid/ready 握手。`PipeNode` 把这层机制封装起来，Plugin 作者只在 `at_stage` 回调里"读"和"写" Payload + 触发状态转换，不必关心握手协议的细节。

**状态机转换图**：

```
        assert_valid
IDLE  ────────────────►  FIRING
                           │
              assert_ready │  ◄──┐
                           ▼      │ assert_ready
                         MOVING   │ (从 BLOCKED 回升)
                           │      │
              deassert_ready│      │
                           ▼      │
                         BLOCKED ─┘

        cancel
any    ────────────────►  CANCELING
state                       │
                            │ complete_cancel
                            ▼
                          IDLE
```

转换规则：
- `IDLE` + `assert_valid()` → `FIRING`
- `FIRING` + `assert_ready()` → `MOVING`
- `BLOCKED` + `assert_ready()` → `FIRING`（回升）
- `MOVING` + `deassert_ready()` → `BLOCKED`
- 任何非 `IDLE` 状态 + `cancel()` → `CANCELING`
- `CANCELING` + `complete_cancel()` → `IDLE`
- `reset()` → 直接回到 `IDLE`

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

#### 构造与查询

| 成员 | 用途 |
|------|------|
| `PipeNode(name)` | 构造（接受名字） |
| 移动构造 / 移动赋值 | 支持（默认实现） |
| 拷贝构造 / 拷贝赋值 | 禁用 |
| `name()` | 返回节点名 |
| `state()` | 返回当前状态（`State` 枚举） |
| `is_idle()` / `is_firing()` / `is_moving()` / `is_blocked()` / `is_canceling()` | 派生状态查询 |

#### 状态机驱动

| 成员 | 用途 |
|------|------|
| `assert_valid()` | `IDLE → FIRING`（数据准备好） |
| `assert_ready()` | `FIRING → MOVING`（下游接收）或 `BLOCKED → FIRING`（回升） |
| `deassert_ready()` | `MOVING → BLOCKED`（下游未就绪） |
| `cancel()` | 任何非 `IDLE → CANCELING` |
| `complete_cancel()` | `CANCELING → IDLE` |
| `reset()` | 直接回到 `IDLE` |

#### Payload 访问（类型安全）

| 成员 | 用途 |
|------|------|
| `operator()(Payload<T>&)` | 模板取值，返回 `T&` / `const T&` |
| `put<T>(key, value)` | 写入 |
| `has<T>(key)` | 检查存在 |
| `payloads()` | 直接访问 `PayloadStore`（供 `CtrlLink` 等需要细粒度控制的场景） |

#### 工厂与辅助

| 成员 | 用途 |
|------|------|
| `static create(name)` | 工厂函数，返回 `unique_ptr<PipeNode>` |
| `static state_name(state)` | 状态名转字符串（调试用） |

**头文件**：`include/cf/plugin/pipe_node.h`

**测试**：`src/cf_plugin/tests/test_pipe_node.cpp`（14 个测试用例）

### 2.4 PipeBuilder + Phase

**角色**：编排器。负责 Plugin 注册、阶段回调、阶段间节点管理、构建流程控制。

**设计意图**：`PipeBuilder` 是 Plugin 框架的"调度中枢"。它接受一组 `PluginBase`，按顺序触发它们的 `setup` / `build`，并把生成的 `at_stage` 回调收归自己。`run()` 时按注册顺序执行所有回调——这就是调度。

为什么集中调度而不是分散到 Plugin：
- **顺序确定**：`PipeBuilder` 的 `std::vector<StageEntry>` 天然按 push_back 顺序排列，调度可预测。
- **可观测**：`stage_names()` 暴露当前所有阶段，调试时一眼看清。
- **可重置**：`reset_all()` 一次性把全部 `PipeNode` 回到 `IDLE`，不必用户手动遍历。

借鉴来源：
- **`chlib/stream_builder.h`**（156 行）：链式 API 形态（虽然 `PipeBuilder` 实际不是链式，而是方法调用式，但调用风格类似）。
- **`chlib/pipeline.h`**：阶段注册 + 调度模式。

**Phase 枚举**：

```cpp
enum class Phase {
  EARLY  = 0,  // 早（流水线入口准备）
  NORMAL = 1,  // 中（核心计算，默认档）
  LATE   = 2   // 晚（流水线出口收尾）
};
```

`phase_name(Phase)` 自由函数把枚举转字符串（`"EARLY"` / `"NORMAL"` / `"LATE"`）。

当前 `PipeBuilder::run()` 按注册顺序调用所有回调，**不显式按 Phase 分桶**。Phase 字段主要服务：
1. **自描述**：每个 `StageEntry` 携带 `Phase` 字段，`stage_names()` 返回时附带 Phase 信息。
2. **未来扩展**（Phase 6）：如果引入"先跑完所有 EARLY 再跑 NORMAL 再跑 LATE"的分桶调度，字段已就位。
3. **可读性**：用户在 `at_stage` 时显式选 Phase，强迫思考"这个回调属于早/中/晚哪一段"。

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

#### Plugin 管理

| 成员 | 用途 |
|------|------|
| `register_plugin(unique_ptr<PluginBase>)` | 接管 Plugin 所有权；空指针抛 `invalid_argument` |
| `plugin_count()` | 返回已注册 Plugin 数 |

#### 阶段注册

| 成员 | 用途 |
|------|------|
| `at_stage(stage_name, phase, callback)` | 注册阶段回调；空名字或空回调抛 `invalid_argument`；首次注册时自动建立对应 `PipeNode` |
| `declare_substage(parent, sub, depth=0)` | 声明子阶段（Phase 0 仅声明父子关系，无深度调度） |
| `stage_count()` | 返回已注册阶段数 |
| `has_stage(name)` | 检查阶段是否存在 |
| `stage_names()` | 返回所有阶段名（按注册顺序） |

#### 节点查询

| 成员 | 用途 |
|------|------|
| `node_of_logic_stage(name)` | 按阶段名查 `PipeNode`（返回 `shared_ptr`）；不存在时返回 `nullptr` |
| `node_count()` | 返回已建立 `PipeNode` 数 |

#### 构建与执行

| 成员 | 用途 |
|------|------|
| `build()` | 编译入口：按顺序调所有 Plugin 的 `setup` 和 `build` |
| `run()` | 执行入口：按注册顺序调所有 `at_stage` 回调 |
| `reset_all()` | 重置所有 `PipeNode` 到 `IDLE` |

**头文件**：`include/cf/plugin/pipe_builder.h`

**测试**：`src/cf_plugin/tests/test_pipe_builder.cpp`（11 个测试用例）

**典型用法**：

```cpp
cf::plugin::PipeBuilder pb;
pb.register_plugin(std::make_unique<MyPlugin>());
pb.register_plugin(std::make_unique<OtherPlugin>());
pb.build();  // 触发所有 setup + build
pb.run();    // 触发所有 at_stage 回调
```

### 2.5 CtrlLink

**角色**：声明式控制（halt / throw / flush / bypass）。

**设计意图**：把流水线里常见的四种"条件控制"封装成一个对象。`CtrlLink` 不持有 `PipeNode`，它只是一个条件收集器 + 查询器，业务代码（通常在 `at_stage` 回调里）调用 `should_halt()` 之类的查询函数，框架据此决定是否 stall / throw / flush / 旁路某个 Payload。

**Condition 类型**：

```cpp
using Condition = std::function<bool()>;
```

`Condition` 接受一个返回 `bool` 的可调用对象（lambda / 函数指针 / `std::function`）。`CtrlLink` 内部按"控制类别"分四个容器（`halt_conds_` / `throw_conds_` / `flush_conds_` / `bypass_map_`）独立保存。

**OR 合并语义**（关键约束）：

`should_halt()` / `should_throw()` / `should_flush()` 都是**逻辑 OR**——只要任意一个条件返回 `true`，总查询就返回 `true`。这意味着多个 Plugin 可以各自注册自己的"暂停条件"，框架按"任意条件触发"处理，符合硬件语义（`stall = src1_stall | src2_stall`）。

借鉴来源：`chlib/pipeline.h::pipeline_stall_ctrl`（393 行），OR 合并逻辑直接来自该组件的同名方法。

**链式 API**：

`halt_when` / `throw_when` / `flush_when` / `bypass` 全部返回 `CtrlLink&`，可以链式调用：

```cpp
ctrl.halt_when([&] { return node.is_blocked(); })
     .flush_when([&] { return reset_signal; })
     .bypass(flush_key, [&] { return flush_active; });
```

空 `Condition`（`!cond`）被静默忽略，不会崩。

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

| 成员 | 用途 |
|------|------|
| `CtrlLink()` | 默认构造 |
| 拷贝 / 赋值 | 禁用 |
| `halt_when(Condition)` | 注册 halt 条件，返回 `*this`（链式） |
| `throw_when(Condition)` | 注册 throw 条件，返回 `*this` |
| `flush_when(Condition)` | 注册 flush 条件，返回 `*this` |
| `bypass<T>(Payload<T>, Condition)` | 注册 Payload 旁路条件；按 Key 存储 |
| `should_halt()` | 返回所有 halt 条件的 OR |
| `should_throw()` | 返回所有 throw 条件的 OR |
| `should_flush()` | 返回所有 flush 条件的 OR |
| `bypass_active(PayloadKeyBase&)` | 查询指定 Key 的旁路条件（空 Key 返回 false） |
| `halt_count()` / `throw_count()` / `flush_count()` / `bypass_count()` | 返回各类条件数（调试） |
| `clear()` | 清空所有条件 |

**关键约束**：
- **OR 合并**：`should_*()` 是所有条件的逻辑 OR（不是 AND）。
- **空 Condition 容错**：传入 `nullptr` / 空 `std::function` 静默忽略。
- **bypass 按 Key 存储**：不同 Payload Key 各自的旁路条件互不干扰。

**头文件**：`include/cf/plugin/ctrl_link.h`

**测试**：`src/cf_plugin/tests/test_ctrl_link.cpp`（11 个测试用例）

### 2.6 uint_t<N> / bool_t

**角色**：编译期位宽切换 + 1 位布尔 typedef。

**设计意图**：为 Bundle 字段提供"位宽"概念。在 TLM 模式下，行为与标准整数类型完全一致；在未来的 RTL 模式（Phase 6）下，可由 `BundleMapper` 替换为 `ch_uint<N>`，但调用方代码不变。

借鉴：
- **VexRiscv `Stageable[T]`** + 泛型位宽模式。
- **CppHDL `ch_state_machine`** 内部位宽切换。

为什么不在 Phase 0 直接用 `ch_uint<N>`：Phase 0 是 TLM 模式验证阶段，引入 RTL 抽象会增加复杂度但没有可见收益。`uint_t<N>` 是"位宽语义"的占位 typedef，行为与标准类型一致；Phase 6 再决定是否替换为 `ch_uint<N>`（届时会有 BundleMapper 等基础设施配合）。

**公共 API 列表**（仅高层视图，完整签名见 [cf_plugin.md](../api/cf_plugin.md)）：

| 成员 | 用途 |
|------|------|
| `template<unsigned N> using uint_t = ...` | 编译期 typedef 到 `uint8_t` / `uint16_t` / `uint32_t` / `uint64_t`（选最接近且 >= N 的标准类型） |
| `using bool_t = bool` | 1 位布尔（Phase 0 typedef 到 `bool`，Phase 6 可替换为 `ch_bool`） |

**位宽映射表**：

| N 范围 | `uint_t<N>` 实际类型 |
|--------|----------------------|
| `N <= 8` | `uint8_t` |
| `N <= 16` | `uint16_t` |
| `N <= 32` | `uint32_t` |
| `N <= 64` | `uint64_t` |
| `N > 64` | **暂不支持**（兜底为 `uint64_t`，编译期无警告；如需更大位宽需扩展 `uint_t_impl`） |

**编译期断言**：

```cpp
static_assert(std::is_unsigned<uint_t<8>>::value,  "uint_t<8> must be unsigned");
static_assert(std::is_unsigned<uint_t<32>>::value, "uint_t<32> must be unsigned");
static_assert(std::is_unsigned<uint_t<64>>::value, "uint_t<64> must be unsigned");
static_assert(std::is_same<bool_t, bool>::value,   "bool_t must be bool (Phase 0)");
```

这些断言保证：
1. `uint_t<N>` 始终是无符号整数（即使将来有人改实现也不会破坏调用方假设）。
2. `bool_t` 始终等于 `bool`（Phase 0 行为锁定）。

**头文件**：`include/cf/plugin/uint_t.h`

**测试**：相关验证包含在 `test_plugin_lifecycle.cpp`（Test 6：uint_t<N> 类型切换正确）。

---

**完整 API 参考**：详见 [docs/api/cf_plugin.md](../api/cf_plugin.md)

---

## 3. 生命周期

Plugin 体系采用**两段式编译 + 一段执行**的生命周期模型：先把所有 Plugin 注册到 `PipeBuilder`，再由 `PipeBuilder` 统一调度 `setup → build → run`。这种分离保证了**跨 Plugin 引用声明**在主体构建前全部可见，**主体构建**在引用声明后确定，**执行**在编译完成后再启动。

### 3.1 时序总览

```
┌─────────────────────────────────────────────────────────────────────┐
│  Plugin  ctor                                                        │
│    │                                                                 │
│    ▼                                                                 │
│  pb.register_plugin(unique_ptr<PluginBase>)   ← 注册 Plugin 实例     │
│    │                                                                 │
│    ▼                                                                 │
│  pb.build()                                                          │
│    │  ┌──────────────────────────────────────────────────────┐      │
│    ├─▶│  Phase 1: 遍历 plugins_, 调用 p->setup(pb)            │      │
│    │  │   语义: 跨 Plugin 引用声明（声明需要的 Payload / Node）│      │
│    │  │   顺序: 按 plugins_ 容器顺序                          │      │
│    │  │   允许: 调用 pb.declare_substage / node_of_logic_stage│      │
│    │  │   禁止: 调用 pb.at_stage                              │      │
│    │  └──────────────────────────────────────────────────────┘      │
│    │  ┌──────────────────────────────────────────────────────┐      │
│    ├─▶│  Phase 2: 遍历 plugins_, 调用 p->build(pb)             │      │
│    │  │   语义: 实际生成逻辑（at_stage 注册执行回调）         │      │
│    │  │   顺序: 按 plugins_ 容器顺序                          │      │
│    │  │   允许: 调用 pb.at_stage 注册回调                     │      │
│    │  │   禁止: 修改其他 Plugin 私有状态                      │      │
│    │  └──────────────────────────────────────────────────────┘      │
│    │                                                                 │
│    ▼                                                                 │
│  pb.run()                                                            │
│    │  ┌──────────────────────────────────────────────────────┐      │
│    └─▶│  Phase 3: 遍历 stages_, 调用 callback()              │      │
│       │   语义: 按注册顺序执行各阶段回调                      │      │
│       │   顺序: 按 stages_ 容器顺序（与 at_stage 调用顺序一致）│      │
│       │   可重复: 可调用 pb.run() 多次（确定性保证见 §3.4）  │      │
│       └──────────────────────────────────────────────────────┘      │
│                                                                       │
│  （可选）pb.reset_all() → pb.run()    ← 重置所有 Node 状态后重跑     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 四个生命周期钩子

| 钩子 | 位置 | 语义 | 默认行为 |
|------|------|------|----------|
| **构造函数 (ctor)** | `PluginBase` 派生类 | 创建 Plugin 实例，初始化派生类私有状态 | `PluginBase()` 默认构造，派生类可选自定义 |
| **`setup(PipeBuilder&)`** | `plugin_base.h:60` | **跨 Plugin 引用声明**——声明本 Plugin 需要的 Payload Key、Node、Substage | **空实现**（`virtual void setup(PipeBuilder&) {}`）；简单 Plugin 可不重写 |
| **`build(PipeBuilder&)`** | `plugin_base.h:65` | **实际生成逻辑**——通过 `pb.at_stage(name, phase, callback)` 注册阶段回调 | **纯虚函数**（派生类**必须**实现，否则无法实例化）|
| **`tick()`** | `plugin_base.h:71` | **显式禁用**（`private deleted`）—— D4 决策禁止业务 `tick()` 模式 | 编译期阻止任何派生类定义同名函数 |

> **关键不变量**：`setup()` 一定先于所有 `build()` 执行；同一 Plugin 集合的 `build()` 顺序与 `plugins_` 容器顺序一致。这一不变性是 §3.4 调度确定性的基础。

### 3.3 最小示例：HelloPlugin

> **代码锚点**：`src/cf_plugin/tests/test_hello_plugin.cpp:32-39`

下面这段来自 Phase 0 单元测试的 `HelloPlugin` 是完整的最小 Plugin 实现（约 8 行）：

```cpp
// src/cf_plugin/tests/test_hello_plugin.cpp:30-39
static Payload<uint32_t> g_greet_count{"greet_count"};

struct HelloPlugin : PluginBase {
  void build(PipeBuilder& pb) override {
    pb.at_stage("greet", Phase::NORMAL, [&pb] {
      auto node = pb.node_of_logic_stage("greet");
      node->operator()(g_greet_count) = node->operator()(g_greet_count) + 1;
    });
  }
};
```

调用方代码（同一文件 41-47 行）：

```cpp
static int run_hello_pipeline() {
  PipeBuilder pb;
  pb.register_plugin(std::make_unique<HelloPlugin>());  // 1. ctor + register
  pb.build();                                            // 2. setup + build
  pb.run();                                              // 3. 执行阶段回调
  return pb.node_of_logic_stage("greet")->operator()(g_greet_count);
}
```

**这段代码演示了 Plugin 生命周期的全部四个阶段**：

1. **ctor**：`HelloPlugin` 由 `std::make_unique` 构造，无自定义 ctor
2. **setup**：未重写，使用 `PluginBase` 默认空实现（Plugin 不需要引用其他 Plugin 的 Node）
3. **build**：注册 `"greet"` 阶段 + `Phase::NORMAL` 回调，回调内通过 `Payload<uint32_t> g_greet_count` 这个**编译期类型安全 Key** 访问 Node 内的计数字段
4. **run**：`pb.run()` 按注册顺序调用所有 `at_stage` 回调；本例仅一个回调，调用一次后 `g_greet_count` 从 0 变 1

### 3.4 调度确定性保证

`PipeBuilder::build()` 和 `PipeBuilder::run()` 都按**容器插入顺序**遍历（见 `pipe_builder.h:93-100`），因此：

- 同一组 Plugin 在同一注册顺序下，**多次执行结果完全一致**（已由 `test_determinism_multiple_runs` 和 `test_determinism_independent_instances` 验证，4/4 PASS）
- 不同 `PipeBuilder` 实例的调度相互独立（无全局状态）
- `pb.reset_all()` 可重置所有 Node 状态而不改变 Plugin 注册顺序

> **Phase 6 扩展点**：调度算法（依赖分析、最优调度）将在 Phase 6 引入。当前 Phase 0 的"按注册顺序"是最朴素的确定性策略，但已足够验证 Plugin 风格可行性。


---

## 4. 与 CppTLM/CppHDL 关系

本节阐明 Plugin 体系与现有 CppTLM（TLM 仿真层）、CppHDL（RTL 描述层）的边界关系。核心结论：**Plugin 与 CppTLM `ChStreamModuleBase` / CppHDL `ch::Component` 都是正交关系**——三者并行存在，按层组合而非互替。

### 4.1 ChStreamModuleBase

`ChStreamModuleBase`（位于 CppTLM `include/core/`，是 TLM 模块基类）与 `Plugin`（Phase 0 提案）是**两种正交的抽象层级**——它们面向不同的设计问题，不互相替代也不互相依赖。

#### 4.1.1 五维对比表

| 维度 | `ChStreamModuleBase`（已实现） | `Plugin`（Phase 0 提案） |
|------|--------------------------------|--------------------------|
| **基类实例** | TLM 模块实例（`CacheTLM`、`MemoryTLM`、`NIC` 等） | 横切关注点单元（`CacheTagLookupPlugin`、`DMABurstPlugin`、`HelloPlugin` 等） |
| **调度单元** | `tick()` 由 `EventQueue` 周期性调用 | **无 `tick()`**（编译期 `private deleted`）；通过 `at_stage(stage, phase, fn)` 注册回调 |
| **通信机制** | `StreamAdapter` 注入（`Input<>` / `Output<>` / `MultiPort<>` 等） + `ch_stream<Bundle>` 内部传递 | `Payload<T>` 类型安全 Key（编译期类型检查） + `PipeNode::operator()` 跨阶段共享 |
| **时间模型** | 离散事件 / 周期（`EventQueue::run` 推进） | 逻辑阶段 + Phase 顺序（`EARLY → NORMAL → LATE`）——**与物理时间解耦** |
| **组合方式** | `ModuleFactory` JSON 注册（`modules: [...]` 数组） + `REGISTER_CHSTREAM` 宏 | `PipeBuilder::register_plugin(unique_ptr<PluginBase>)` C++ 注册 |

#### 4.1.2 关系约束（v1.0 草案）

1. **Plugin 不替代 `ChStreamModuleBase` 子类**：Plugin 是**横切关注点**（cross-cutting concern），必须挂载到具体模块上才能生效。Phase 0 阶段两者各自独立运行；Phase 1 起将定义挂载接口
2. **Plugin 实例的生命周期短于模块**：单次 `PipeBuilder::build()` 内创建并固化；模块在 `EventQueue` 中持续运行
3. **Plugin 间的通信通过 `PipeBuilder` 共享的 `PipeNode`**：不直接调用彼此方法，避免形成隐式依赖图
4. **Phase 0 Plugin 仅在 TLM 模式生效**：RTL 模式继续使用现有 `ch::Component::describe()` DAG；Plugin 与 RTL 生成的集成推迟到 Phase 6

#### 4.1.3 关键结论

**两套正交，不互相替代**：

- 想实现"一个 Cache 控制器"——继承 `ChStreamModuleBase`，用 `StreamAdapter` 通信，由 `EventQueue` 调度
- 想给上述 Cache 注入"tag lookup 逻辑"——实现 `Plugin`，用 `Payload<T>` 共享状态，由 `at_stage` 阶段调度
- 二者通过**配置层**（JSON 模块列表 + C++ Plugin 注册）组合，而非通过**代码继承**组合

> **v1.0 阶段标记**：§4.1 的"挂载接口"在 Phase 0 尚未实现——目前两套机制各自独立运行。集成路径将在 Phase 1 L1CachePlugin 实施时定义（参见 [`docs/roadmap/phases/phase-1-tlm-foundation.md`](../../docs/roadmap/phases/phase-1-tlm-foundation.md)）。

### 4.2 Component

`ch::Component`（位于 CppHDL `include/core/component.h`）是 RTL 描述的根类，用于通过 `describe()` 方法构建 `lnode` DAG，最终由 `VerilogCodeGen` 转换为可综合 Verilog。Plugin 与 Component 同样是**正交关系**——两者面向不同的设计层级，并行存在。

#### 4.2.1 核心差异

| 维度 | `ch::Component`（已实现） | `Plugin`（Phase 0 提案） |
|------|---------------------------|--------------------------|
| **设计层级** | RTL 描述（生成 Verilog） | 声明式逻辑单元（C++ 函数式组合） |
| **组合入口** | `Component::describe()` 返回 `lnode` DAG | `PluginBase::build(PipeBuilder&)` 注册阶段回调 |
| **状态描述** | `ch_reg<>` / `ch_uint<>` 等 RTL 原语 | `Payload<T>` 包装的 `T` 值（编译期类型安全） |
| **执行目标** | Verilator / 商业仿真器 / FPGA 综合 | TLM 仿真（SystemC 兼容事件队列） |
| **调度单位** | 时钟周期 + lnode 拓扑序 | 逻辑阶段 + Phase 顺序（无时钟概念） |

#### 4.2.2 关系约束

1. **Plugin 不替代 Component**：Plugin 生成的逻辑**最终需要落到 Component 上**才能在 RTL 仿真/综合流程中运行
2. **Phase 0 不做集成**：Plugin 仅在 TLM 模式生效；Plugin 描述的逻辑如何由 VerilogCodeGen 转换到 Component 推迟到 Phase 6
3. **并行存在**：同一个 IP 可以同时有 TLM 版（用 Plugin 描述）和 RTL 版（用 Component 描述），但 TLM 版优先落地

#### 4.2.3 集成路径（推迟到 Phase 6）

```
Plugin 业务代码 (Phase 1+)
        │
        │   Phase 6 引入
        ▼
Plugin → lnode DAG 转换器
        │
        ▼
ch::Component 描述
        │
        ▼
VerilogCodeGen
        │
        ▼
Verilog → Verilator / 综合
```

> **为什么 Phase 0 不做集成**：转换器的设计取决于 Phase 1 L1CachePlugin 的真实业务结构。提前设计会导致转换器接口无法应对实际约束（如哪些 `Payload<T>` 能映射到 `ch_reg<>`，哪些需要保持 TLM-only）。
>
> **关键承诺**（D4 决策）：Plugin 业务代码到 Phase 6 时**不重写**——转换器在 Plugin 一侧吃接口，Component 一侧吐 Verilog。

### 4.3 chlib 命名共存

CppHDL 的 `chlib` 子模块（位于 `CppHDL/include/chlib/`）已存在一套**自由函数**形式的流控制 API，与 Plugin 提案中的 `CtrlLink` **对象方法**在命名空间上存在重叠。本节明确两套机制**共存不互替**的边界。

#### 4.3.1 两套 API 的形式差异

| 维度 | chlib 自由函数（已实现） | `CtrlLink` 对象方法（Phase 0 提案） |
|------|---------------------------|-------------------------------------|
| **调用形式** | `chlib::stream_halt_when(stream, cond)` | `ctrl_link.halt_when(cond)` |
| **所属头文件** | `CppHDL/include/chlib/stream.h:58,92` | `include/cf/plugin/ctrl_link.h`（Phase 0 P0 #5） |
| **所属层** | CppHDL 流构建层（`stream_builder.h` 链式 API） | Plugin 控制流层（`PipeBuilder` 阶段调度） |
| **设计意图** | 单条 `ch_stream` 的运行时条件控制 | 跨 Plugin OR 合并的多条件控制 |

**具体命名对照**（D6 决策表）：

| CtrlLink 对象方法 | chlib 对应自由函数 | D6 方案 |
|------------------|-------------------|---------|
| `CtrlLink::halt_when` | `chlib::stream_halt_when`（`stream.h:92`） | **方案 C — 两者共存** |
| `CtrlLink::throw_when` | `chlib::stream_throw_when`（`stream.h:58`） | **方案 C — 两者共存** |
| `CtrlLink::flush_when` | （无直接对应） | **方案 C — 两者共存** |
| `CtrlLink::bypass` | （无直接对应） | **方案 C — 两者共存** |

#### 4.3.2 D6 决策依据

> **决策引用**：`.omo/drafts/decision-plugin-framework-2026-06-08.md` §3.5（D6 CtrlLink 控制 API 命名）+ ADR-033 §"决策依据"。

D6 决策**保留 CppHDL chlib 现有 28 个测试零破坏**，新 Plugin 业务代码统一使用 `CtrlLink::*` 对象方法，`chlib` 自由函数保持向后兼容。

**当时评估的三个选项**：

- **选项 A**：CtrlLink 复用 chlib 自由函数 —— 会引入 chlib 到 Plugin 路径，违反 Plugin 独立性
- **选项 B**：CtrlLink 改名为 `link_halt_when` 等 —— 破坏 Plugin 命名一致性
- **选项 C（采纳）**：chlib 自由函数迁移到 chstream 流（保留 chlib 现有 API）—— 两套机制按层隔离，零破坏

#### 4.3.3 使用边界

| 场景 | 推荐使用 |
|------|----------|
| 编写新 Plugin 业务代码 | `ctrl_link.halt_when()` 对象方法（Plugin 控制流） |
| 修改 CppHDL chlib 现有代码 | 保持 `chlib::stream_halt_when()` 自由函数（向后兼容） |
| Plugin 调用 chlib 内部实现 | 仅在 RTL 后端（Phase 6+），Plugin 一侧不直接调用 chlib |
| 同一作用域内同时使用两者 | 允许——命名空间不同（`chlib::` vs `cf::plugin::CtrlLink::`）不冲突 |

#### 4.3.4 验证

- `chlib` 现有 28 个单元测试**零修改**通过（chlib 自由函数路径不变）
- Plugin 单元测试（51/51 PASS）**不依赖 chlib 头文件**（独立性验证）
- ADR-033 验证命令（`adr.md:952-958`）已记录两套 API 的区分 grep 规则

> **v1.0 状态**：§4.3 描述的共存关系是 Phase 0 验收时确立的边界。Phase 1+ 业务代码**统一遵循** §4.3.3 的使用规则，避免混用。

---

## 5. 路线图锚点

本节锁定 Plugin 体系在 ChipForge 主路线图中的**三个关键阶段位置**：Phase 0（已完成）、Phase 1（下一个里程碑）、Phase 6（最终完整框架）。其他 Phase（2-5）与 Plugin 体系不直接耦合，按需消费 Plugin 提供的接口。

### 5.1 三阶段状态表

| Phase | 状态 | 说明 |
|-------|------|------|
| Phase 0 | ✅ 已完成 (2026-06-08) | 5/5 P0 组件 + 51/51 单元测试 PASS（PluginBase 7/7、Payload 8/8、PipeNode 14/14、PipeBuilder 11/11、CtrlLink 11/11） |
| Phase 1 | 🚧 待开发 | L1CachePlugin Hello World（验证 Plugin 风格可行性）；首条端到端 Plugin 业务代码落地 |
| Phase 6 | 🚧 待开发 | 完整 PipeBuilder 框架 + RTL 生成（12-20 周）；推迟的 Phase 1a/1b/1c 内容合并到此阶段 |

> **详细阶段定义**：
> - Phase 0：[`docs/roadmap/phases/phase-0-plugin-scaffolding.md`](../../docs/roadmap/phases/phase-0-plugin-scaffolding.md)（实施记录 + 退出标准）
> - Phase 1：[`docs/roadmap/phases/phase-1-tlm-foundation.md`](../../docs/roadmap/phases/phase-1-tlm-foundation.md)（L1CachePlugin 业务实现）
> - Phase 6：[`docs/roadmap/phases/phase-6-declarative.md`](../../docs/roadmap/phases/phase-6-declarative.md)（v2.0.2 暂未创建，路线图 README 已预留位置）

### 5.2 Phase 0 接口稳定性承诺

> **引用**：[`phase-0-plugin-scaffolding.md` §6.1](../../docs/roadmap/phases/phase-0-plugin-scaffolding.md) + [`declarative-hybrid-framework.md` §12.2.3](declarative-hybrid-framework.md)

Phase 0 完成后，以下 **5 个接口**在 Phase 1-5 期间**保持稳定**（仅 Phase 6 才升级）：

- `cf::plugin::PluginBase::setup(PipeBuilder&)` / `build(PipeBuilder&)`
- `cf::plugin::Payload<T>` 模板与 `operator()` 访问
- `cf::plugin::PipeNode` 状态机 API（`is_firing` / `is_moving` / `is_blocked` / `is_canceling`）
- `cf::plugin::PipeBuilder` 的 `at_stage` / `register_plugin` / `build` / `run` / `node_of_logic_stage` / `declare_substage`
- `cf::plugin::CtrlLink` 的 `halt_when` / `throw_when` / `flush_when` / `bypass`

**这意味着 Phase 1 业务逻辑（L1CachePlugin）使用上述接口后，**不需要重写**即可在 Phase 6 升级到完整框架。**

### 5.3 Phase 6 触发条件

满足以下**任一**条件时启动 Phase 6：

1. Phase 1 L1CachePlugin + 至少 2 个其他 Plugin-style IP 稳定运行
2. 出现"第三个需要 TLM↔RTL 协同的 IP"（焦点调试需求）
3. 用户主动决定启动

### 5.4 与 v2.0.1 路线图的对应关系

v2.0.1 §12.2 中"Phase 1a/1b/1c 拆分方案"在 v2.0.2 中重新映射：

- **Phase 1a**（Plugin/Pipe 核心机制）→ 范围缩小到 **Phase 0 脚手架**
- **Phase 1b**（JSON + 验证基础设施）→ 合并到 **Phase 6a**
- **Phase 1c**（端到端 IP）→ 合并到 **Phase 1**（L1CachePlugin 既是端到端 IP 又是验证用例）

> **拆分理由**：v2.0.1 中"Phase 1"工时估计过高（10-16 周），且 Plugin 核心机制不依赖 JSON 解析。Phase 0 脚手架让 Phase 1 业务代码立即可用 Plugin-style，Phase 6 框架升级不会重写业务代码。

---

## 6. 文档维护规则

本节定义本文档**自身**的维护契约——4 条规则确保 Plugin 框架的"文档-代码-决策"三者保持同步。任何对 Plugin 体系（代码、ADR、决策）的变更都必须触发对应规则的检查。

### 6.1 规则 1：版本号规则

**每次 Phase 状态变更升级次版本号**（v1.0 → v1.1 → v1.2 ...）。

- Phase 0 完成时：v1.0 → v1.1
- Phase 1 L1CachePlugin 落地时：v1.x → v1.(x+1)
- Phase 6 完整框架发布时：v1.x → v2.0（主版本号升级，因为引入新能力）

> 主版本号（v1 → v2）仅在**接口破坏性变更**或**新能力域**引入时升级；次版本号（v1.0 → v1.1）用于功能增量和状态推进。

### 6.2 规则 2：状态变更流程

任何 ADR 状态变更（🚧 → ✅ 或 ✅ → 🚧）需**同步**本文档对应章节：

- ADR-025 / ADR-027 / ADR-028（Plugin 核心机制）状态变更 → 同步 §2.1 / §2.3 / §2.4
- ADR-030 / ADR-031 / ADR-033（流水线抽象）状态变更 → 同步 §2.3 / §2.5
- ADR-029（ImplMode）状态变更 → 同步 §4.2.3
- ADR-032（PipeBuilder）状态变更 → 同步 §2.4

**同步操作清单**：

1. 更新本文档对应章节的 ✅ / 🚧 状态
2. 更新 §5.1 三阶段状态表
3. 更新顶部"状态"字段
4. 在 CHANGELOG.md 记录变更

### 6.3 规则 3：ADR 同步规则

任何 ADR **新增/废弃**需在本文档 §1 设计动机更新论证：

- 新增 ADR（编号 N+1）：在 §1 增加对应设计动机的引文段，注明"对应 ADR-N+1"
- 废弃 ADR（标记 Deprecated）：在 §1 移除对应引文段，在 CHANGELOG 记录废弃原因
- ADR 实质修改（决策反转）：在 §1 调整对应引文段，并触发本文档版本号升级（见规则 1）

> 此规则确保 §1 设计动机始终是**当前活跃** ADR 的精确反映，而非历史快照。

### 6.4 规则 4：接口承诺（Phase 0 → Phase 6）

**Phase 0 的 5 个接口在 Phase 6 之前不破坏**（参见 §5.2）：

- `PluginBase::setup` / `build` 签名冻结
- `Payload<T>` 模板接口冻结（仅允许新增特化）
- `PipeNode` 状态机 API 冻结
- `PipeBuilder` 的核心方法（`at_stage` / `register_plugin` / `build` / `run` / `node_of_logic_stage`）签名冻结
- `CtrlLink` 的四个方法（`halt_when` / `throw_when` / `flush_when` / `bypass`）签名冻结

**Phase 6 扩展**（**仅扩展，不破坏**）：

- `PipeBuilder` 可新增方法（如 `parse_json` / `dependency_analysis`）
- `PluginBase` 可新增虚函数（但需保持向后兼容——通过默认实现 + `final` 阻止链式重定义）
- `Payload<T>` 可新增辅助方法

> **违反此规则的变更**：必须在评审环节拒绝，除非同时升级主版本号（v1.x → v2.0）并提供迁移路径。