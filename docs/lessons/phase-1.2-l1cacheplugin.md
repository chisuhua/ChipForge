# Phase 1.2 L1CachePlugin 教训与模式

> 沉淀自 `e8deacc` 实施过程，供 Phase 2+ Plugin-style IP 开发参考。

## 一、PipeBuilder 阶段注册陷阱

### 1.1 `at_stage("A", ...)` 与 `at_stage("B", ...)` 创建不同 PipeNode

```cpp
// ❌ 错误: 每个 at_stage 创建独立 PipeNode, PayloadStore 隔离
pb.at_stage("lookup", NORMAL, [this]() {
  auto n = pb.node_of_logic_stage("lookup");
  n->put(g_idx, idx);   // 写入 lookup 节点
});
pb.at_stage("refill", LATE, [this]() {
  auto n = pb.node_of_logic_stage("refill");  // 不同节点!
  auto idx = n->operator()(g_idx);            // 读到默认值 0!
});
```

**原因**: PipeBuilder::at_stage() 内部在 `nodes_` map 中以 stage_name 为 key 创建 PipeNode。不同 stage 名称 → 不同 PipeNode → 不同 PayloadStore。Payload Key 按 **指针身份** 匹配，但不同 PayloadStore 的同名 Key 互不干扰。

**修复**: lookup 与 refill 共享同一 PipeNode:

```cpp
// ✅ 正确: 闭包使用同一个 node
auto n = pb.node_of_logic_stage("lookup");
lookup_node_ = n;
refill_node_ = n;   // 共享同一节点

pb.at_stage("lookup", NORMAL, [this]() {
  lookup_node_->put(g_idx, idx);
});
pb.at_stage("refill", LATE, [this]() {
  refill_node_->put(g_mem_data, ...);  // 读到 lookup 写入的 idx
});
```

**配套规则**: 测试辅助 API (issue_request/refill_from_memory/read_response) 也必须使用同样的内部门店节 `payload_node_`，不要接受外部 node 参数:

```cpp
// ✅ helper 内部统一使用 payload_node_
void issue_request(const CacheReq& req) {
  payload_node_->put(g_addr, req.address);
}
```

### 1.2 `pb.run()` 执行所有 at_stage 回调（一次性）

```cpp
// ❌ 错误: 测试中分两次写 + 分两次 run
helper.issue_request(n, req);
pb.run();                     // lookup 跑了, 但 g_mem_data 还没写入!
helper.refill_from_memory(n, mem);
pb.run();                     // refill 跑, g_hit 已经被上一步覆盖
```

**原因**: `pb.run()` 遍历 `stages_` 向量依次执行所有注册回调（先 lookup 再 refill）。**所有输入必须在 run() 之前设置完毕**。

```cpp
// ✅ 正确: 所有输入在同一次 run() 前写入
helper.issue_request(n, req);         // 写 g_addr
helper.refill_from_memory(n, mem);    // 写 g_mem_data
pb.run();  // lookup → refill 在同 cycle 完成
```

### 1.3 `declare_substage` 声明不对应实际线程调度

`declare_substage("lookup", "refill", 1)` 在 Phase 0 PipeBuilder 中仅做**声明记录**，不产生深度调度。lookup 和 refill 仍然在 pb.run() 中顺序执行（注册顺序）。Phase 6 完整框架才会利用 substage 做并行调度。

## 二、TLM 模式的存储选择

### 2.1 `ch_mem` 不可在 TLM 模式使用

spec skeleton 显示 `ch_mem<uint_t<20>, 256> tags_`，但 **TLM 模式不存在 ch::core::context**，`ch_mem` 构造函数会尝试注册内部 AST 节点 → 段错误。

| 模式 | 存储类型 | 说明 |
|------|---------|------|
| TLM (Phase 1) | `std::array<T, N>` | 纯 C++ 容器, 无框架依赖 |
| RTL (Phase 5+) | `ch_mem<T, N>` | 需 ch::core::context + Component |

```cpp
// TLM 模式
std::array<uint_t<20>,   256> tags_;
std::array<uint64_t,     256> data_;
std::array<bool,         256> valid_;
```

### 2.2 `uint_t<512>` 退化为 `uint64_t`

```cpp
uint_t<512> line_data{0xCAFEBABEDEADBEEFULL};
// 实际类型: uint64_t (高位 448-bit 被截断)
```

仅限于 Phase 0/1 的 64-bit 单次访问测试，line_data 高位被丢弃。Phase 6 将升级为 `__int128` 或 `boost::multiprecision::uint512_t`。

### 2.3 `if (cond) return;` 在 at_stage 回调中不可移植

**现象**（`L1CachePlugin.cpp:131-136`）：
```cpp
pb.at_stage("refill", cf::plugin::Phase::LATE, [this]() {
  auto n = refill_node_;
  if (!n) return;

  cf::plugin::bool_t hit = n->operator()(g_hit);
  if (hit) return;  // 命中无需 refill
  // ... refill 逻辑 ...
});
```

**问题**：
- **B 类不匹配**（时序语义）：TLM 模式单缓冲，`return;` 等价于"不做写"；RTL 模式双缓冲（`array_store::commit()` 切换后），早返会跳过 commit 边界
- **C 类不匹配**（抽象层级）：RTL 中无"早返"概念，需用 `when(cond) { ... }` 表达条件；`return` 在硬件描述中无对应
- Phase 1 单元测试可能 PASS（`if (hit) return;` 看似正确），但 Phase 5/6 升级后行为偏差

```cpp
// ❌ 错误: 早返依赖"return 后什么也不做"的隐式语义
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) return;  // RTL 无 return 概念
  write_set(idx, tag, mem_data);
});
```

```cpp
// ✅ 推荐: 显式 if/else 或 when(cond) 条件块
pb.at_stage("refill", Phase::LATE, [this]() {
  if (hit) {
    // 命中: 保持 storage 不变 (no-op, 显式注释)
  } else {
    // miss: 执行 refill
    write_set(idx, tag, mem_data);
  }
});
// Phase 6 形态: when(!hit) { write_set(...); }
```

**配套检查**：`tools/check_plugin_portability.sh` Check 1 — 在 `ip/*/tlm/**/*.cpp` 中扫描 `at_stage` 回调内的 `if (cond) return;` 模式。

**配套决策**：[ADR-040](../architecture/adr/ADR-040-tlm-hdl-portability-constraints.md) §2.1 Tier-1 #4

### 2.4 数组直接索引 `tags_[idx] = ...` 在 RTL 升级时需要 adapter

**现象**（`L1CachePlugin.cpp:205-207`）：
```cpp
void L1CachePlugin::write_set(
    std::size_t set,
    cf::plugin::uint_t<kTagBits> tag,
    cf::plugin::uint_t<kLineDataBits> line_data) {
  if (set >= kNumSets) return;
  tags_[set] = tag;        // 直接 std::array 索引赋值
  data_[set] = line_data;  // Phase 6 切换 ch_mem 后需改为 mem_.write(addr, data)
  valid_[set] = true;
}
```

**问题**：
- **A 类不匹配**（API 形态）：TLM 模式 `std::array::operator[]` 单参数；RTL 模式 `ch_mem::write(addr, data)` 双参数
- Phase 6 升级时所有 `tags_[idx] = ...` 调用点都需修改

```cpp
// ❌ 错误: 直接持有 std::array, Phase 6 切换 ch_mem 时所有调用点都要改
std::array<cf::plugin::uint_t<kTagBits>, kNumSets> tags_{};
// ...
tags_[idx] = tag;  // ch_mem 改为 ch_mem.write(idx, tag) 后所有赋值语句都要改
```

```cpp
// ✅ 推荐: 用 array_store 包装, 对外 API 与 std::array 一致
cf::plugin::storage::array_store<cf::plugin::uint_t<kTagBits>, kNumSets> tags_{};
// ...
tags_[idx] = tag;  // Phase 1 模式: 直接转发到 std::array
                     // Phase 6 模式: 写入 shadow buffer, pb.run() 末尾 commit()
// 业务调用方无需修改！
```

**额外收益**：
- `array_store::commit()` 是 `PipeBuilder::run()` 末尾自动调用的钩子（`pipe_builder.h:101`）
- Phase 6 切换后 `commit()` 提交 shadow → current，实现双缓冲
- 单元测试无需修改（`operator[]` 语义不变）

**配套检查**：`tools/check_plugin_portability.sh` Check 4（[WARN]）— 鼓励业务代码声明 `array_store` 而非 `std::array`。

**配套决策**：[ADR-040](../architecture/adr/ADR-040-tlm-hdl-portability-constraints.md) §2.1 Tier-2 #1 + §2.2 `array_store` 抽象

### 2.5 位提取 `>>` + `&` 模式需要 helper 包装

**现象**（`L1CachePlugin.cpp:108-113`）：
```cpp
cf::plugin::uint_t<L1CachePlugin::kIdxBits> idx =
    static_cast<cf::plugin::uint_t<L1CachePlugin::kIdxBits>>(
        (static_cast<uint64_t>(addr) >> kIdxShift) & kIdxMask);
cf::plugin::uint_t<L1CachePlugin::kTagBits> tag =
    static_cast<cf::plugin::uint_t<L1CachePlugin::kTagBits>>(
        (static_cast<uint64_t>(addr) >> kTagShift) & kTagMask);
```

**问题**：
- **A 类不匹配**（表达式）：TLM 模式 shift + mask；RTL 模式位选 `addr[HI:LO]`
- 位宽常量分散在调用点（`kIdxShift` / `kIdxMask` / `kTagShift` / `kTagMask`），修改地址位宽时易遗漏
- `static_cast<uint64_t>(addr)` 中间转换暴露 `uint_t<64>` 的 POD 内部表示，破坏封装

```cpp
// ❌ 错误: 位提取逻辑散布在 at_stage 闭包内
constexpr uint64_t kIdxMask = (1ULL << kIdxBits) - 1;
constexpr unsigned kIdxShift = kOffsetBits;
auto idx = (static_cast<uint64_t>(addr) >> kIdxShift) & kIdxMask;
auto tag = (static_cast<uint64_t>(addr) >> kTagShift) & kTagMask;
```

```cpp
// ✅ 推荐: 集中到 helper, Phase 1 shift+mask, Phase 6 addr[HI:LO] 位选
// (在 L1CachePlugin.h 类内)
static constexpr cf::plugin::uint_t<kIdxBits> extract_idx(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (static_cast<uint64_t>(addr) >> kOffsetBits) &
      ((1ULL << kIdxBits) - 1));
}
static constexpr cf::plugin::uint_t<kTagBits> extract_tag(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  return static_cast<cf::plugin::uint_t<kTagBits>>(
      (static_cast<uint64_t>(addr) >> (kOffsetBits + kIdxBits)) &
      ((1ULL << kTagBits) - 1));
}
// 调用点:
auto idx = extract_idx(addr);
auto tag = extract_tag(addr);
```

**额外收益**：
- 修改地址位宽（`kOffsetBits` / `kIdxBits` / `kTagBits`）时只改一处
- Phase 6 RTL 升级时，helper 内部实现从 `shift+mask` 切换为 `addr[HI:LO]` 位选，调用点零修改
- 编译期 constexpr，零运行时开销

**配套检查**：编码规范 + code review（`tools/check_plugin_portability.sh` 暂不自动检测 —— Tier-2 约束，code review 阶段把关）。

**配套决策**：[ADR-040](../architecture/adr/ADR-040-tlm-hdl-portability-constraints.md) §2.1 Tier-2 #2 + §4 步骤 3

## 三、D4 静态检查的隐藏规则

### 3.1 grep 匹配**注释内容**，不仅是代码

`verify_plugin_decision.sh` 使用 `grep -rlnE "enum class.*State" ip/` 检查状态机。**注释中的 "enum class State" 也会触发失败**:

```cpp
// ❌ 文件头注释写 "遵循 D4: 无 enum class State" → 触发 FAIL
// ✅ 改写为 "无显式状态机" → PASS
```

### 3.2 检查范围是 `ip/`，不是 `src/`

脚本硬编码 `TARGET_DIRS="${ROOT_DIR}/ip"`，只扫描 `ip/` 目录下代码。`src/` 中的测试文件不在此范围。Plugin-style 业务代码必须放在 `ip/{module}/tlm/` 才能被静态检查覆盖。

## 四、测试驱动开发 (TDD) 实践经验

### 4.1 测试架构: Plugin 实例必须与 pb 注册的是同一个

```cpp
// ✅ 正确: 提取裸指针, move 进 pb, 通过裸指针访问
auto plugin = std::make_unique<L1CachePlugin>();
helper = plugin.get();              // 保留访问句柄
pb.register_plugin(std::move(plugin));

// 之后用 helper->issue_request(...) 驱动
// pb.run() 中的 at_stage 回调修改的是同一个 instance 的 storage
```

**不能**先构造栈变量再 copy/move，因为 `PluginBase` 禁止拷贝（`= delete`）。

### 4.2 测试顺序 = 真实使用顺序

```cpp
// 一个完整的 miss+refill+hit 周期:
// 1. 写请求 + 写 mem_data
helper.issue_request(node, req);
helper.refill_from_memory(node, mem);
pb.run();  // lookup(miss) + refill(写入 storage) 同 cycle

// 2. 再次请求同一地址
helper.issue_request(node, req);
pb.run();  // lookup(hit) → data 从 storage 读回
```

### 4.3 assert 前 printf 需 stderr

`assert()` 通过 `abort()` 终止，stdout 缓冲可能未 flush。调试 printf 应使用 `fprintf(stderr, ...)`。

## 五、Payload Key 声明惯例

### 5.1 文件作用域匿名 namespace（单个 .cpp 翻译单元）

```cpp
namespace {
  Payload<uint_t<64>> g_addr{"l1cache.addr"};
  Payload<bool_t>     g_hit{"l1cache.hit"};
  // ...
}
```

- **跨翻译单元问题**: 不同 .cpp 文件的匿名 namespace 是不同对象，指针身份不同。
- **多 Plugin 实例**: 所有实例共享同一组 Key（同一翻译单元），通过 `put/get` 操作不同 PipeNode 的 PayloadStore 隔离。

### 5.2 Key 命名规范

使用 `"prefix.key_name"` 格式，prefix = 模块名（`l1cache.`），便于 PayloadStore 调试和小数据打印。

## 六、CMake 集成模式

### 6.1 单文件 Plugin 直接编译进测试

Phase 1.2 的 L1CachePlugin 只有 1 个 .cpp，不需要独立的 library target:

```cmake
add_executable(test_l1_cache_plugin_unit
  tests/test_l1_cache_plugin_unit.cpp
  ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp  # 直接编译
)
target_include_directories(test_l1_cache_plugin_unit PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_l1_cache_plugin_unit PRIVATE cf_plugin)
```

### 6.2 测试注册不要加 header EXISTS 守卫

```cmake
# ❌ 错误: cmake configure 时 header 还不存在, 测试永远不会被注册
if(EXISTS ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.h)
  ...
endif()

# ✅ 正确: 无条件注册, TDD RED 阶段可以看到编译失败
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_l1_cache_plugin_unit.cpp)
  add_executable(...)
endif()
```

## 七、快速命中的常见编译错误

| 错误 | 原因 | 修复 |
|------|------|------|
| `use of deleted function L1CachePlugin(const L1CachePlugin&)` | `make_unique<Plugin>(plugin)` 传引用而非构造参数 | `make_unique<Plugin>()` 或用裸指针 |
| `request for member 'issue_request' in pointer type` | `helper` 是裸指针，用 `.` 而非 `->` | `helper->issue_request(...)` |
| Assertion `data == expected` failed (data=0) | `refill_from_memory` 写入的 node 与 `at_stage` 读的 node 不同 | 共享 `payload_node_` |
| `verify_plugin_decision` FAIL | 注释中包含 `enum class State` | 改写注释不触发 grep 模式 |
| `LSP: 'cf/plugin/pipe_builder.h' file not found` | LSP compile_commands.json 未包含 cf_plugin include 路径 | **false positive**, 不影响 cmake 构建 |
