# cf_plugin API 参考 (Phase 0)

> **版本**: 0.1.0
> **状态**: Phase 0 完成, 5/5 P0 组件稳定
> **生成方式**: 手动维护 (Doxygen 暂未集成; 等待环境安装)
> **上次更新**: 2026-06-08

> **架构权威**:详见 [docs/architecture/plugin-framework.md](../../architecture/plugin-framework.md) (设计意图、生命周期、ADR摘要)。本文档仅描述 API签名。

`cf::plugin` 命名空间的所有公共 API 索引。

## 目录

| 组件 | 文件 | 角色 |
|------|------|------|
| [`PluginBase`](#pluginbase) | `plugin_base.h` | 抽象基类 |
| [`Payload<T>` / `PayloadStore`](#payloadt--payloadstore) | `payload.h` | 类型安全 Key + 存储 |
| [`PipeNode`](#pipenode) | `pipe_node.h` | 节点 + 状态机 |
| [`PipeBuilder` / `Phase`](#pipebuilder--phase) | `pipe_builder.h` | 编排器 |
| [`CtrlLink`](#ctrllink) | `ctrl_link.h` | 控制 API |
| [`uint_t<N>` / `bool_t`](#uinttn--boolt) | `uint_t.h` | 编译期类型切换 |

---

## `PluginBase`

```cpp
namespace cf::plugin;

class PluginBase {
 public:
  PluginBase() = default;
  virtual ~PluginBase() = default;
  PluginBase(const PluginBase&) = delete;
  PluginBase& operator=(const PluginBase&) = delete;

  virtual void setup(PipeBuilder&) {}           // 默认空, 可选 override
  virtual void build(PipeBuilder&) = 0;         // 派生类必须实现
};
```

**生命周期**:
1. `ctor()` — 构造 (派生类声明 Payload 静态 Key)
2. `setup(pb)` — 跨 Plugin 引用声明 (默认空)
3. `build(pb)` — 实际生成 (at_stage 注册)
4. `pb.build()` — 编译入口
5. `pb.run()` — 执行入口

**禁止**: 派生类不应定义 `tick()` (D4 决策)

**示例**:
```cpp
struct MyPlugin : cf::plugin::PluginBase {
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("greet", cf::plugin::Phase::NORMAL, [] {
      printf("Hello\n");
    });
  }
};
```

---

## `Payload<T>` / `PayloadStore`

### `Payload<T>`

```cpp
template <typename T>
class Payload : public PayloadKeyBase {
 public:
  explicit Payload(std::string name);
  Payload(const Payload&) = delete;

  const std::type_info& type() const noexcept override;
  static constexpr const std::type_info& static_type() noexcept;
};
```

类型安全 Key。用法: 全局静态对象 + 跨阶段通信。

### `PayloadStore`

```cpp
class PayloadStore {
 public:
  template <typename T> void put(const Payload<T>& key, T value);
  template <typename T> const T& get(const Payload<T>& key) const;
  template <typename T> T& get(const Payload<T>& key);
  template <typename T> bool has(const Payload<T>& key) const;
  void clear() noexcept;
  std::size_t size() const noexcept;
};
```

- `get()` 在缺失时默认构造 T (便于 `n(key) = value` 直接写)
- 跨 PipeNode 隔离 (每个 PipeNode 持有独立 PayloadStore)

---

## `PipeNode`

```cpp
class PipeNode {
 public:
  enum class State { IDLE, FIRING, MOVING, BLOCKED, CANCELING };

  explicit PipeNode(std::string name);

  const std::string& name() const noexcept;
  State state() const noexcept;

  bool is_idle() const noexcept;
  bool is_firing() const noexcept;
  bool is_moving() const noexcept;
  bool is_blocked() const noexcept;
  bool is_canceling() const noexcept;

  void assert_valid();
  void assert_ready();
  void deassert_ready();
  void cancel();
  void complete_cancel();
  void reset() noexcept;

  template <typename T> T& operator()(const Payload<T>& key);
  template <typename T> const T& operator()(const Payload<T>& key) const;
  template <typename T> void put(const Payload<T>& key, T value);
  template <typename T> bool has(const Payload<T>& key) const;

  PayloadStore& payloads() noexcept;
  static std::unique_ptr<PipeNode> create(std::string name);
  static const char* state_name(State s) noexcept;
};
```

**状态机**:
```
IDLE ──assert_valid──> FIRING ──assert_ready──> MOVING
                          │                       │
                          │                  deassert_ready
                          │                       │
                          │                       v
                          └──<──assert_ready── BLOCKED
                                                 │
                                          assert_valid
                                                 │
cancel (any non-IDLE) ──> CANCELING ──complete_cancel──> IDLE
```

---

## `PipeBuilder` / `Phase`

### `Phase` 枚举

```cpp
enum class Phase { EARLY, NORMAL, LATE };
const char* phase_name(Phase p) noexcept;
```

### `PipeBuilder`

```cpp
class PipeBuilder {
 public:
  using StageCallback = std::function<void()>;

  void register_plugin(std::unique_ptr<PluginBase> plugin);
  void at_stage(const std::string& stage_name, Phase phase, StageCallback cb);
  void declare_substage(const std::string& parent, const std::string& sub, int depth = 0);

  std::shared_ptr<PipeNode> node_of_logic_stage(const std::string& stage_name) const;

  void build();   // 调用所有 Plugin 的 setup() + build()
  void run();     // 按注册顺序调用所有 at_stage 回调
  void reset_all();

  std::size_t plugin_count() const noexcept;
  std::size_t stage_count() const noexcept;
  std::size_t node_count() const noexcept;

  bool has_stage(const std::string& name) const;
  std::vector<std::string> stage_names() const;
};
```

**典型用法**:
```cpp
cf::plugin::PipeBuilder pb;
pb.register_plugin(std::make_unique<MyPlugin>());
pb.build();   // 触发所有 Plugin 的 setup() 和 build() (at_stage 注册)
pb.run();     // 按顺序执行所有已注册回调
```

---

## `CtrlLink`

```cpp
class CtrlLink {
 public:
  using Condition = std::function<bool()>;

  CtrlLink& halt_when(Condition cond);   // 阻塞下游 ready
  CtrlLink& throw_when(Condition cond);  // 注入 cancel
  CtrlLink& flush_when(Condition cond);  // 清空寄存器
  template <typename T>
  CtrlLink& bypass(const Payload<T>& key, Condition src_active);

  bool should_halt() const;
  bool should_throw() const;
  bool should_flush() const;
  bool bypass_active(const PayloadKeyBase& key) const;

  std::size_t halt_count() const noexcept;
  std::size_t throw_count() const noexcept;
  std::size_t flush_count() const noexcept;
  std::size_t bypass_count() const noexcept;

  void clear() noexcept;
};
```

**OR 合并**: `should_halt()` 是所有 halt_when 条件的逻辑 OR。
**链式 API**: `halt_when(...).throw_when(...).flush_when(...)`。

---

## `uint_t<N>` / `bool_t`

```cpp
template <unsigned N> using uint_t = ...;  // 标准 C++ 整数
using bool_t = bool;
```

`uint_t<8>` → `uint8_t`
`uint_t<16>` → `uint16_t`
`uint_t<32>` → `uint32_t`
`uint_t<64>` → `uint64_t`
`uint_t<N>` (N > 64) → `uint64_t` (暂不支持, Phase 6 升级)

**编译期约束**: `static_assert(std::is_unsigned<uint_t<N>>::value, ...)`

---

## 头文件依赖图

```
uint_t.h
   │
   ▼
payload.h ───► (独立)
   │
   ▼
pipe_node.h
   │
   ▼
plugin_base.h (前向声明 PipeBuilder)
   │
   ▼
pipe_builder.h ───► payload.h, pipe_node.h, plugin_base.h
   │
   ▼
ctrl_link.h ───► payload.h
```

所有头文件独立可 include; cf_plugin 头文件 + CppTLM 头文件 + CppHDL 头文件可同时使用 (见 `test_coexistence.cpp`)。

---

## 编译期保证

1. `PluginBase` 是抽象类 (`is_default_constructible == false`)
2. 未 override `build()` 的派生类仍是抽象
3. `Payload<T>::static_type()` 返回 `typeid(T)`
4. 错类型 `get<wrong_T>(key)` 编译失败 (模板参数不匹配)
5. `static_assert` 验证 `uint_t<N>` 是无符号

## 运行时保证

1. `PipeNode` 状态机确定 (相同输入序列 → 相同状态转换)
2. `PayloadStore` 跨 PipeNode 隔离
3. `CtrlLink` 多条件 OR 合并
4. `PipeBuilder::run()` 按注册顺序执行

## 已知限制 (Phase 0 范围)

- 模板深度受 C++ 编译器限制 (通常 900+)
- `uint_t<N>` 仅 typedef 标准整数, 无 ch_uint 替换
- `CtrlLink::bypass` 仅记录条件, 不真正转发数据 (Phase 6)
- `PipeBuilder` 顺序执行, 无依赖分析调度 (Phase 6)
