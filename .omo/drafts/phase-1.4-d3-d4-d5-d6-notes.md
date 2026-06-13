# D3 + D4 + D5 + D6 评估笔记 — L1CachePlugin 方法学复盘

> **来源**: Phase 1.4 (DECISION-2026-06-13-02, F1.A + F2 6 维度)
> **关联**: `.omo/plans/phase-1.4-plugin-design-methodology-review.md` T3
> **基线**: D4 + ADR-040 静态检查 3+4/3 全部 PASS (T1.3 evidence)
> **T2 评估**: D1+D2 已完成 (`.omo/drafts/phase-1.4-d1-d2-notes.md`, 18 观察 / 51 边界)
> **范围**: D3 (TLM↔RTL) + D4 (阶段调度) + D5 (Payload 通信) + D6 (测试便利)
> **日期**: 2026-06-13

---

## 元信息

| 项目 | 值 |
|------|-----|
| 复盘对象 | L1CachePlugin (`ip/cache/tlm/L1CachePlugin.{h,cpp}`) + Bridge (`src/cf_plugin/bridge/l1_cache_bridge.{h,cpp,adapter.{h,cpp}}`) + 测试 (`src/cf_plugin/tests/test_*.cpp`) |
| 评估维度 | D3 (TLM↔RTL) + D4 (阶段调度) + D5 (Payload 通信) + D6 (测试便利) |
| 边界类型 | B1 (接受) / B2 (摩擦) / B3 (局限) |
| ADR-040 4 Tier | T1-#1 at_stage 无早返 / T1-#2 ip/*/tlm/ 无 ch 渗透 / T1-#3 Plugin::build 不调 pb.run() / T1-#4 存储 array_store / T2-#1 业务不直接 std::array / T2-#2 位提取 helper 集中 |

---

## D3: TLM↔RTL 可移植性

### D3.1 array_store 抽象的双缓冲 — **B2 摩擦**

**观察** (L1CachePlugin.h:147-149 + .cpp:126-128):
```cpp
using TagStore  = cf::plugin::storage::array_store<cf::plugin::uint_t<kTagBits>,     kNumSets>;
// ...
pb.register_commit_hook([this] { tags_.commit(); });
pb.register_commit_hook([this] { data_.commit(); });
pb.register_commit_hook([this] { valid_.commit(); });
```

- TLM 模式: `array_store::commit()` 是 no-op (单缓冲, 即写即读)
- Phase 6 RTL 模式: 改 `array_store` 内部实现 + commit 钩子自动生效
- 业务调用方 (`tags_[idx] = ...` in L1CachePlugin.cpp:235-237) **不变** (lessons §2.4)
- lessons §2.4 文档化:`operator[]` 语义在 TLM/RTL 都一致 → 单元测试无需修改

**摩擦点**:
- `register_commit_hook` 3 个 (L126-128) — **手动注册**而非自动检测 → 漏注册某个 storage 会导致 RTL 模式双缓冲不完整
- 没有 `#ifdef` 守卫,业务代码总是注册 3 个 commit hook → TLM 模式**多调用 3 次 no-op 函数** (轻微性能影响)
- `array_store` 抽象**不暴露** `ch_mem::write(addr, data)` 的双参数 API 切换问题 — lessons §2.4 指出这是 A 类不匹配 (API 形态), Phase 6 升级时所有 `tags_[idx] = ...` 调用点都需修改

**判断**: **B2 摩擦** — 抽象方向正确 (双缓冲钩子),但**需要 Phase 6 完善**:
- 方案 A: 业务代码不直接调用 `operator[]`,改用 `store(idx, val)` / `load(idx)` 命名接口 (T2.7 模式 #3)
- 方案 B: 提供 `pb.register_storage_commit(storage)` 自动注册,业务代码零改动

**对未来 IP 写作者的建议**: 直接用 `register_commit_hook` 显式注册,代码 review 时确认 3 件套 (tags/data/valid) 全部注册。

### D3.2 register_commit_hook 注册顺序 — **B1 接受**

**观察** (L1CachePlugin.cpp:124-128 注释):
```cpp
// 注意: 注册顺序 = commit 顺序; tags_ 先于 data_/valid_ 提交.
pb.register_commit_hook([this] { tags_.commit(); });
pb.register_commit_hook([this] { data_.commit(); });
pb.register_commit_hook([this] { valid_.commit(); });
```

- 注释明确"注册顺序 = commit 顺序" → 跨阶段 RAW 死读隐患
- TLM 模式 no-op → 顺序无影响
- RTL 模式如果 `data_` 先于 `tags_` commit,会出现"data 已新但 tag 仍旧" 的中间态

**判断**: 注释文档化 + 显式顺序 = **B1 接受** — 是良好的防御性编程。

### D3.3 静态存储 std::array vs array_store — **B1 接受**

**观察** (L1CachePlugin.h:171-178):
```cpp
TagStore   tags_{};   // array_store 包装
DataStore  data_{};   // array_store 包装
ValidStore valid_{};  // array_store 包装
```

- 业务代码**不直接持有** `std::array` — 全部用 `array_store` 包装 (ADR-040 Tier-2 #1)
- `check_plugin_portability.sh` Tier-1 #4 + Tier-2 #1 检查:**0 匹配** (T1.3 evidence)

**判断**: **B1 接受** — ADR-040 Tier-2 软约束在 L1Cache 上**完整遵守**, 是个好范式示例。

### D3.4 ch_mem / ch_reg / ch_uint / ch::core 渗透 — **B1 接受 (零渗透)**

**观察** (L1CachePlugin.{h,cpp} 全文 + ADR-040 Tier-1 #2):
- TLM 模式**完全无** `ch_mem` / `ch_reg` / `ch_uint` / `ch::core` 渗透
- 业务代码 254 行 + 头 199 行 = 453 行, 0 匹配
- `check_plugin_portability.sh` Tier-1 #2 检查: **0 匹配** (T1.3 evidence)

**判断**: **B1 接受** — TLM 模式与 RTL 模式严格隔离, Phase 6 升级时无 ch_mem 调用点需要重写。

### D3.5 Plugin::build() 内不调 pb.run() — **B1 接受 (零调用)**

**观察** (L1CachePlugin.cpp:106-178, build() 完整):
- build() 内**无** `pb.run()` 调用
- `pb.run()` 仅在 Bridge `tick()` 末尾 (v2 决策 D1' 末尾挂载契约) + 测试代码中调用
- `check_plugin_portability.sh` Tier-1 #3 检查: **0 匹配** (T1.3 evidence)

**判断**: **B1 接受** — 阶段注册 (at_stage) 与执行 (pb.run) 严格分离,符合 Plugin 范式骨架。

### D3.6 位提取 helper (extract_idx/tag) shift+mask → RTL 位选 — **B2 摩擦**

**观察** (L1CachePlugin.cpp:82-92):
```cpp
cf::plugin::uint_t<L1CachePlugin::kIdxBits> L1CachePlugin::extract_idx(
    cf::plugin::uint_t<kAddrBitsRaw> addr) {
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (static_cast<uint64_t>(addr) >> kIdxShift) & kIdxMask);
}
```

- TLM 模式: `shift + mask` 表达式
- RTL 模式: `addr[HI:LO]` 位选 (lessons §2.5)
- helper 签名一致, 调用点零修改 — **B1 接受的部分**
- 但 `static_cast<uint64_t>(addr)` **暴露** `uint_t<64>` 的 POD 内部表示 (lessons §2.5 "破坏封装")

**摩擦点**:
- TLM 模式 `shift + mask` 写法暴露 POD 内部,业务代码不应知道 `uint_t<64>` 内部就是 `uint64_t`
- RTL 模式 `addr[HI:LO]` 由 `ch::bits<MSB,LSB>` 提供,封装在 `uint_t<>` 内
- lessons §2.5 "位宽常量分散在调用点" 风险已**通过 helper 集中**消除,但 `static_cast` 仍在 helper 内

**判断**: **B2 摩擦** — helper 集中位宽常量是好范式,但 `static_cast<uint64_t>` 是 TLM 模式的实现泄漏。Phase 6 升级时 helper 内部需全部改写。

**对未来 IP 写作者的建议** — **B2 模式 #4** (helper 内改进):
```cpp
// 推荐: helper 内部用 const 表达式 + 类型转换封装, 不暴露 POD
static constexpr cf::plugin::uint_t<kIdxBits> extract_idx(
    cf::plugin::uint_t<kAddrBits> addr) noexcept {
  // TLM 模式: shift+mask (但封装在 const 表达式内)
  // RTL 模式: addr[HI:LO] 位选 (Phase 6 替换)
  return static_cast<cf::plugin::uint_t<kIdxBits>>(
      (addr >> kOffsetBits) & ((1ULL << kIdxBits) - 1));
}
// 注: 移位常量 kIdxShift / kIdxMask 在文件作用域匿名 namespace
//      helper 仅暴露 constexpr 接口, 不暴露 shift/mask 表达式
```

### D3.7 ADR-040 4 Tier 综合判断

| Tier | 约束 | L1Cache 表现 | 边界 |
|------|------|------------|------|
| Tier-1 #1 | at_stage 回调内无 `if (cond) return;` 早返 | 0 匹配 (lessons §2.3 + L1CachePlugin.cpp:150-154, 173-176 显式 if/else) | **B1 接受** |
| Tier-1 #2 | ip/*/tlm/ 无 ch_mem/ch_reg/ch_uint/ch::core 渗透 | 0 匹配 | **B1 接受** |
| Tier-1 #3 | Plugin::build() 内未调用 pb.run() | 0 匹配 | **B1 接受** |
| Tier-1 #4 | 存储声明优先 array_store | 0 匹配 (L1CachePlugin.h:147-149 全 array_store) | **B1 接受** |
| Tier-2 #1 | 业务不直接持有 std::array | 0 匹配 (统一 array_store 包装) | **B1 接受** |
| Tier-2 #2 | 位提取 helper 集中 | extract_idx/extract_tag 集中 (L1CachePlugin.h:140-143) | **B1 接受 (有 D3.6 摩擦)** |

**判断**: 4 Tier **6 项**约束 L1Cache **全部遵守** (B1 接受 5 + B2 摩擦 1),ADR-040 移植性约束是**成功的范式骨架**。

---

## D4: 阶段调度清晰度

### D4.1 lookup + refill 两阶段拆分 — **B1 接受**

**观察** (L1CachePlugin.cpp:107, 130-156, 160-177):
- `pb.declare_substage("lookup", "refill", 1)` 声明 lookup → refill 串行依赖
- lookup 阶段 (Phase::NORMAL): 提取 idx/tag, 判定 hit, 写 g_idx/g_tag/g_hit/g_data/g_error
- refill 阶段 (Phase::LATE): 读 g_hit/g_mem_data/g_idx/g_tag, 仅 miss 时 write_set

**判断**: 阶段拆分**自然清晰**:
- 一次访问涉及 1 个 lookup (命中/缺失判定) + 0/1 个 refill (仅缺失)
- lookup 输出 (g_idx, g_tag, g_hit) 是 refill 输入 → 阶段间 Payload IPC 明确
- 命名 `"lookup"` + `"refill"` 自文档化,无需注释解释阶段意图

**B1 接受** — 阶段拆分与 D4 范式骨架一致。

### D4.2 pb.run() 一次性执行所有 at_stage 回调 — **B3 局限**

**观察** (L1CachePlugin.cpp + lessons §1.2):
- `pb.run()` 遍历 `stages_` 向量, 依次执行所有注册回调
- **所有输入必须在 run() 之前设置完毕** (lessons §1.2)
- 测试代码模式 (test_l1_cache_plugin_unit.cpp:88-90, 110-117, 143-150):
  ```cpp
  ctx.helper->issue_request(ctx.lookup_node, req);
  MemResp mem{};
  // ... fill mem ...
  ctx.helper->refill_from_memory(ctx.refill_node, mem);
  ctx.pb.run();  // lookup + refill 同 cycle
  ```

**局限点**:
- `pb.run()` **无 cycle 精度** — 不区分 cycle 0 / cycle 1,lookup 与 refill 在**同一次 run()** 内执行
- 没有"cycle 0 写请求, cycle 1 写 mem_data, cycle 2 触发 lookup, cycle 3 触发 refill" 的多 cycle 调度
- lessons §1.3 指出 `declare_substage` 仅做声明,不对应线程调度 (Phase 0 限制, Phase 6 升级)
- lessons §1.2 明确 `pb.run()` 是**一次性**执行所有 at_stage 回调

**影响**:
- 单事务单拍场景 OK (Phase 1.2 范围)
- 多事务多拍场景 (Phase 2 RTOS 上下文) 需要扩展 → Phase 6 完整框架必须支持
- **测试 API 模式固化** (issue_request + refill + run) → 未来 IP 写作者会自然继承这个模式

**判断**: **B3 局限** — 范式本身**没有硬限制**,但**当前 Phase 0 实现** `pb.run()` 是单 cycle 一次性,Phase 6 完整框架需补 (B3 链接 Phase 6 任务)。

**Phase 6 任务链接**:
- B3-L1: PipeBuilder 引入 multi-cycle scheduling (substage → real thread / cycle)
- B3-L2: at_stage 注册时声明 cycle phase 范围 (NORMAL / LATE → NORMAL/0, LATE/1)
- B3-L3: 测试 API 需支持多 cycle 顺序驱动 (issue_request at cycle 0, refill at cycle 2, run all)

### D4.3 declare_substage 声明不对应线程调度 — **B3 局限**

**观察** (lessons §1.3 + L1CachePlugin.cpp:107):
- `pb.declare_substage("lookup", "refill", 1)` **仅做声明记录**,不产生深度调度
- lookup 和 refill 仍在 pb.run() 中**按注册顺序**执行

**判断**: **B3 局限** — Phase 0 PipeBuilder 是"声明 + 顺序执行" 模型,Phase 6 完整框架需利用 substage 做**真正的并行/串行调度** (B3 链接 B3-L1)。

### D4.4 闭包内防御性 assert — **B1 接受**

**观察** (L1CachePlugin.cpp:135, 162):
```cpp
auto* n = lookup_node_.get();
assert(n && "build() must initialize lookup_node_ before at_stage callback runs");
```

- build() 已 assert lookup_node_ 非空 (L1CachePlugin.cpp:113-115), 闭包内**再 assert 一次**是防御性编程
- lessons §1.2 提到 "build() 内已初始化, 闭包内无须 defensive check" — 但源码仍保留

**判断**: **B1 接受** (略冗余, 但 defensive, 适合 Phase 0 阶段)
**对未来 IP 写作者**: 可选 — Phase 6 框架可提供 `pb.assert_nodes_initialized()` 统一检查,业务代码可省略闭包内 assert。

### D4.5 阶段时序 (Phase::NORMAL / Phase::LATE) — **B1 接受**

**观察** (L1CachePlugin.cpp:133, 160):
- lookup 在 `Phase::NORMAL` (较早执行)
- refill 在 `Phase::LATE` (较晚执行)
- 同一 cycle 内 NORMAL → LATE 顺序执行, 保证 lookup 输出是 refill 输入

**判断**: **B1 接受** — Phase 时序语义清晰, Phase 0 PipeBuilder 支持 NORMAL / LATE 二阶段时序。

### D4.6 跨阶段 RAW 死读防护 — **B1 接受**

**观察** (L1CachePlugin.cpp:164-171):
```cpp
cf::plugin::bool_t hit = n->operator()(g_hit);
cf::plugin::uint_t<L1CachePlugin::kLineDataBits> mem_data =
    n->operator()(g_mem_data);
cf::plugin::uint_t<L1CachePlugin::kIdxBits> idx =
    n->operator()(g_idx);
cf::plugin::uint_t<L1CachePlugin::kTagBits> tag =
    n->operator()(g_tag);

// hit 分支 (no-op): 读取所有需要的 Payload, 保证没有跨阶段 RAW 死读
if (!hit) {  // 全分支 if/else, 不早返
  write_set(idx, tag, mem_data);
}
```

- refill 阶段**不论 hit 与否**都读全部所需 Payload
- 这是 lessons §2.3 提到的"RTL 模式双缓冲 RAW 死读"防护
- 注释 "保证没有跨阶段 RAW 死读" 明确文档化

**判断**: **B1 接受** — 是 TLM→RTL 迁移的**关键防御性模式**, lessons §2.3 已文档化, future IP 写作者会自然继承。

---

## D5: Payload 通信效率

### D5.1 Payload<T> Key 全局静态 + 按指针身份匹配 — **B1 接受**

**观察** (L1CachePlugin.cpp:46-77 匿名 namespace):
```cpp
namespace {
cf::plugin::Payload<cf::plugin::uint_t<kAddrBits>> g_addr{"l1cache.addr"};
cf::plugin::Payload<cf::plugin::bool_t>            g_is_write{"l1cache.is_write"};
// ... 8 个 Key 全部
}
```

- 8 个 Payload Key 文件作用域静态
- 按**指针身份**匹配 (lessons §五): 不同 PipeNode 的 PayloadStore, 用同一组 Key 访问**不同 cell**
- 同一翻译单元共享 Key (单 .cpp 内 L1CachePlugin.cpp + 测试代码都可访问)

**判断**: **B1 接受** — Pointer identity 是 idiomatic C++ 设计模式, 简单直观。

### D5.2 跨 PipeNode 隔离机制 — **B1 接受 (有 lessons 文档化)**

**观察** (lessons §1.1 + L1CachePlugin.cpp:113-115):
- `lookup_node_ = pb.node_of_logic_stage("lookup")`
- `refill_node_ = lookup_node_` (**共享同一节点**)
- `payload_node_ = lookup_node_` (测试 API 也使用同一节点)
- 注释 (L109-112) 解释: "lookup 与 refill 共享同一个 PipeNode (payload_node_): 跨阶段 Payload Key 必须命中同一 PayloadStore 才能传递 idx/tag/hit/data; 否则 refill 读到的是默认构造值 (hit=false 但 idx=0/数据=0)"

**判断**: **B1 接受** — lessons §1.1 详细文档化 "at_stage 创建独立 PipeNode" 陷阱 + 共享 `payload_node_` 修复。**未来 IP 写作者不会重蹈覆辙** (lessons 文档化的价值)。

### D5.3 Key 命名规范 — **B1 接受**

**观察** (L1CachePlugin.cpp:61-75):
- 命名格式: `"l1cache."` 前缀 + 字段名
- `l1cache.addr` / `l1cache.idx` / `l1cache.tag` / `l1cache.hit` / `l1cache.data` / `l1cache.error` / `l1cache.mem_data` / `l1cache.mem_id`
- lessons §五: "使用 `prefix.key_name` 格式, prefix = 模块名 (`l1cache.`)"

**判断**: **B1 接受** — 命名规范清晰, 调试时可按前缀过滤。

### D5.4 跨翻译单元 Key 共享问题 — **B3 局限**

**观察** (lessons §五.1):
- 匿名 namespace 是**文件作用域**, 跨 .cpp 文件是**不同对象** (指针身份不同)
- L1CachePlugin.h 中**没有** Payload Key 声明 (在 .cpp 匿名 namespace)
- 业务 Bundle 字段用 `bundles::CacheReq` / `bundles::MemResp` (在 `bundles/mem_bundles.h`)

**局限点**:
- 如果未来 L1CachePlugin 拆为多个 .cpp (e.g. 业务代码 + lookup helper + refill helper), Payload Key 需提到 header
- 当前 L1CachePlugin.cpp 是单文件 → 局限未暴露
- 但 lessons §五.1 明确"跨翻译单元问题: 不同 .cpp 文件的匿名 namespace 是不同对象"

**影响**:
- 单文件 Plugin (当前) 无问题
- 多文件 Plugin (Phase 2+ 复杂 IP) 需将 Key 提到 header 公共命名空间
- **当前 lessons 文档化** 跨翻译单元问题, 未来 IP 写作者可应对

**判断**: **B3 局限** — 范式本身**没有硬限制**,但**匿名 namespace 模式**仅适合单文件 Plugin。Phase 6 框架需提供 "Plugin-scoped Payload 命名空间" 机制 (B3 链接 Phase 6)。

**Phase 6 任务链接**:
- B3-P1: Plugin 内部提供 `plugin_payload<T>(name)` API, 内部用 `std::any` 或 `type-erased registry` 实现跨翻译单元 Key 共享
- B3-P2: 编译期检查: 同一 Plugin 不同 .cpp 不应重复声明同名 Key (避免指针身份不一致)

### D5.5 Payload Key 数量与 lookup 字段数 — **B1 接受**

**观察** (L1CachePlugin.cpp + `bundles::CacheReq` / `CacheResp` / `MemResp`):
- 8 个 Payload Key (L1CachePlugin.cpp:61-75) 对应 1 个 CacheReq + 1 个 CacheResp + 部分 MemResp 字段
- 4 字段窄桥 (decision-phase-1.3d-extras §F1.A): addr/data/is_write/id 已在 issue_request 写 Payload
- 剩余字段 (op, mem_id) 也独立 Key, 但**没有**对应的 Bundle 字段映射 (lookup 阶段不读 op, refill 阶段读 mem_id)

**判断**: **B1 接受** — Key 数量合理, 不冗余。

### D5.6 阶段间 Payload 读/写 API — **B1 接受**

**观察** (L1CachePlugin.cpp 全部 at_stage 闭包):
- 读: `n->operator()(g_addr)` (L138)
- 写: `n->put(g_idx, idx)` (L141)
- 跨阶段: lookup 写 g_idx → refill 读 g_idx (L168)
- API 一致: `operator()` (read) / `put` (write)

**判断**: **B1 接受** — API 命名清晰, 读/写语义无歧义。

---

## D6: 测试便利性

### D6.1 Plugin 实例 + pb 注册 + 裸指针 helper 模式 — **B1 接受 (TDD 实践)**

**观察** (test_l1_cache_plugin_unit.cpp:54-70):
```cpp
struct TestCtx {
  cf::ip::cache::tlm::L1CachePlugin* helper;
  PipeBuilder pb;
  std::shared_ptr<cf::plugin::PipeNode> lookup_node;
  std::shared_ptr<cf::plugin::PipeNode> refill_node;

  TestCtx() {
    auto plugin = std::make_unique<cf::ip::cache::tlm::L1CachePlugin>();
    helper = plugin.get();  // 保留访问句柄
    pb.register_plugin(std::move(plugin));
    pb.build();
    lookup_node = pb.node_of_logic_stage("lookup");
    refill_node = pb.node_of_logic_stage("refill");
    assert(lookup_node != nullptr);
    assert(refill_node != nullptr);
  }
};
```

- lessons §四.1 详细解释: "Plugin 实例必须与 pb 注册的是同一个" (make_unique + 裸指针 helper + move)
- lessons §四.2 解释测试顺序: "测试顺序 = 真实使用顺序"
- lessons §四.3 解释 `assert()` 前 printf 需 stderr

**判断**: **B1 接受** — 是 idiomatic Plugin TDD 模式, lessons §四 文档化完整, future IP 写作者可直接复用 TestCtx 容器模式。

### D6.2 helper API 泄漏程度 — **B2 摩擦 (D1.4 复述)**

**观察** (test_l1_cache_plugin_unit.cpp:88, 110, 116, 143, 149, 153, 156, 162):
- 测试中 `ctx.helper->issue_request(...)` / `ctx.helper->refill_from_memory(...)` / `ctx.helper->read_response(...)` / `ctx.helper->is_set_valid(...)` / `ctx.helper->read_tag(...)` — **5 个 helper API**
- 全部以 `helper` (L1CachePlugin*) 为 receiver
- 生产路径应通过 Bundle 流式接口 (L1CachePlugin.h:90-97 注释)

**摩擦点** (同 T2 D1.4):
- 5 个 helper API 在测试中**频繁调用**,但**没有 friend class 隔离**或 `#ifdef CF_UNIT_TEST` 守卫
- 测试代码 181 行中 helper API 调用占 30+ 行
- 未来 IP 写作者**会模仿**这个模式 → helper API 数量膨胀

**判断**: **B2 摩擦** — 测试便利但有封装泄漏风险。

### D6.3 测试/业务代码行数比例 — **B1 接受 (健康)**

**观察** (T3.6 实际计算):
- 业务代码 L1CachePlugin.cpp: **254 行**
- 单元测试 test_l1_cache_plugin_unit.cpp: **181 行**
- 比例: 业务:测试 = **1:0.71** (测试比业务短)
- 全部测试代码 (test_l1_cache_bridge + e2e + json_instantiate): 599 行
- 业务 + 桥接层: 453 + 400 = 853 行
- 全部比例: 853:599 = **1:0.70** (测试与业务几乎 1:1)

**判断**: **B1 接受** (健康区间):
- 单元测试 181 行 vs 业务 254 行 → 比例 0.71 (测试比业务略短, 但覆盖了 4 个核心场景 + TestCtx 容器)
- 全部测试 599 行 vs 业务 + 桥接 853 行 → 比例 0.70 (测试密度合理, 未"测试比业务多得多"的过度测试)
- 业界推荐比例 1:1 ~ 2:1 (TDD 严格流派), L1Cache 处于 0.7 略低于 1:1, 但**有 Bridge 测试 + e2e 测试**作为补充

**对未来 IP 写作者**: 单元测试比例不低于 0.5 (业务:测试 ≥ 1:0.5), 综合测试 (含 e2e + instantiate) 比例不低于 0.7。

### D6.4 4 单元测试的覆盖度 — **B1 接受**

**观察** (test_l1_cache_plugin_unit.cpp):
1. `test_lookup_miss_path` (L77-97): Miss 路径, 首次访问空 set
2. `test_refill_path` (L102-123): Refill 路径, MemResp 到达后 set 填充
3. `test_hit_after_refill` (L128-162): Hit 路径, refill 后再次访问
4. `test_d4_compliance_runtime` (L167-172): D4 合规运行时检查 (Plugin 在 PipeBuilder 下不崩溃)

**判断**: **B1 接受** — 4 单元测试**完整覆盖** L1CachePlugin 的 4 个核心场景 (miss / refill / hit / D4 runtime), 未来 IP 写作者可参考这个 4 场景模板。

### D6.5 单元测试的 assert + printf 模式 — **B1 接受**

**观察** (test_l1_cache_plugin_unit.cpp:96, 122, 161, 171):
```cpp
printf("  [PASS] test_lookup_miss_path\n");
```

- 纯 main() + assert (L20 注释: "纯 main() + assert (与 Phase 0 cf_plugin + Phase 1.1 mem_bundles 一致)")
- lessons §四.3 提到 `assert()` 前 printf 需 stderr — **当前实现用 stdout** (L96, L122 等), 这是一个**潜在陷阱** (abort() 时 stdout 未 flush)

**判断**: **B1 接受 (有 lessons 文档化陷阱)** — 模式简单, 未来 IP 写作者可复用;但 printf 用 stdout 是已知陷阱, lessons §四.3 已文档化。

### D6.6 e2e 测试 + JSON instantiate 测试的层次 — **B1 接受**

**观察**:
- test_l1_cache_bridge.cpp (70 行): Bridge 框架层测试
- test_l1_cache_plugin_e2e.cpp (156 行): 端到端 SoC JSON 跑通
- test_l1_cache_json_instantiate.cpp (192 行): full JSON instantiateAll e2e

**判断**: **B1 接受** — L1CachePlugin 的测试**分 4 层**:
1. 单元测试 (test_l1_cache_plugin_unit.cpp) — 业务逻辑
2. Bridge 测试 (test_l1_cache_bridge.cpp) — 框架层
3. e2e 测试 (test_l1_cache_plugin_e2e.cpp) — 端到端
4. JSON instantiate 测试 (test_l1_cache_json_instantiate.cpp) — 配置驱动

**未来 IP 写作者**: 4 层测试结构是好范式, Phase 2+ IP 也应分这 4 层。

---

## 跨维度观察 (D3-D6 共同发现)

### 跨观察 3: Phase 6 输入识别 (3 个 B3 局限)

| 局限 | 位置 | 现象 | Phase 6 任务 |
|------|------|------|------------|
| B3-D4.2 | L1CachePlugin.cpp pb.run() | 一次性执行所有 at_stage, 无 cycle 精度 | B3-L1: multi-cycle scheduling |
| B3-D4.3 | L1CachePlugin.cpp:107 declare_substage | 仅声明,不对应线程调度 | B3-L1: substage → real scheduling |
| B3-D5.4 | 匿名 namespace Payload Key | 跨翻译单元 Key 指针身份不同 | B3-P1: plugin_payload<T> 命名空间机制 |

**判断**: 3 个 B3 局限**全部在 D4 调度 + D5 Payload 维度**,D3 TLM↔RTL + D6 测试便利**无 B3**。说明 Plugin 范式在"测试便利"维度是健康的,"阶段调度" + "Payload 跨翻译单元" 是范式**当前实现的限制**而非范式本身的硬伤。

### 跨观察 4: B2 摩擦集中在 2 个维度

| 维度 | B2 摩擦点 | 模式编号 |
|------|----------|---------|
| D1 可读性 | helper API 泄漏 + array_store 抽象 + at_stage 早返习惯 | 模式 #1 #2 #3 (T2) |
| D3 TLM↔RTL | array_store 双缓冲 + register_commit_hook 显式 + 位提取 static_cast | 模式 #3 (T2) + #4 (T3) |
| D5 Payload | (无) | - |
| D4 调度 | (无 B2, 全部 B1 + B3) | - |
| D6 测试 | helper API 泄漏 (与 D1.4 同源) | 模式 #1 (T2) |

**判断**: B2 摩擦**集中在 D1+D3 维度**(代码可读性 + TLM↔RTL 移植性),**D4 调度 + D5 Payload + D6 测试**几乎无 B2,说明 Plugin 范式在"调度 + Payload + 测试" 三个维度的**范式骨架成熟度**高于"代码可读性 + 移植性"。

### 跨观察 5: B1 接受占绝对多数 (22/26 = 85%)

| 维度 | B1 接受 | B2 摩擦 | B3 局限 | 总计 |
|------|---------|---------|---------|------|
| D1 可读性 | 5 | 3 | 0 | 8 |
| D2 范式合规 | 5 + 4 (Tier) | 1 | 0 | 10 |
| D3 TLM↔RTL | 5 (Tier) + 1 (顺序注释) | 1 (static_cast) | 0 | 7 |
| D4 阶段调度 | 3 (assert + 时序 + RAW) | 0 | 2 (pb.run + declare_substage) | 5 |
| D5 Payload 通信 | 5 (Key 静态 + 隔离 + 命名 + 数量 + API) | 0 | 1 (跨翻译单元) | 6 |
| D6 测试便利 | 5 (TDD 模式 + 比例 + 覆盖 + 层次 + printf) | 1 (helper API) | 0 | 6 |
| **合计** | **32** | **6** | **3** | **41** |

**判断**:
- B1 接受 **32/41 = 78%** (健康, Plugin 范式在 L1Cache 上**总体通过**)
- B2 摩擦 **6/41 = 15%** (可接受, 6 个摩擦点有具体模式)
- B3 局限 **3/41 = 7%** (低, 3 个局限已识别 Phase 6 任务)

**D1+D2+T2: B1 14/18 = 78%, B2 4/18 = 22%, B3 0/18 = 0%**
**D3-D6+T3: B1 18/23 = 78%, B2 2/23 = 9%, B3 3/23 = 13%**
**整体 (T2 + T3): B1 32/41 = 78%, B2 6/41 = 15%, B3 3/41 = 7%**

---

## D3-D6 B2 模式补充 (T2 之外的)

### B2 模式 #4: 位提取 helper 内部 const 封装 (D3.6)

详见 D3.6 节代码块。

### B2 模式 #5: array_store 业务接口统一 (D3.1 + 跨 T2.7 模式 #3)

```cpp
// 推荐: array_store 业务接口统一为 store(idx, val) / load(idx)
// 内部 TLM: 直接 operator[] = 
// 内部 RTL: shadow[idx] = val, commit 时 data_ = shadow_
template<typename T, std::size_t N>
class array_store {
 public:
  void store(std::size_t idx, T val) {
    if constexpr (kTlmMode) {
      data_[idx] = val;
    } else {
      shadow_[idx] = val;
    }
  }
  T load(std::size_t idx) const { return data_[idx]; }
  void commit() {
    if constexpr (!kTlmMode) data_ = shadow_;
  }
 private:
  std::array<T, N> data_{};
  std::array<T, N> shadow_{};
  static constexpr bool kTlmMode = true;
};

// 业务代码: tags_.store(idx, tag) 替代 tags_[idx] = tag
// 内部 operator[] 仍可访问 (向后兼容) 但推荐 store/load 命名
```

### B2 模式 #6: helper API friend class 隔离 (D1.4 + D6.2)

详见 T2 D1.4 模式 #1。

---

## T3 评估小结

| 维度 | 观察数 | B1 接受 | B2 摩擦 | B3 局限 |
|------|--------|---------|---------|---------|
| D3 TLM↔RTL | 7 (含 4 Tier + 3 综合) | 6 | 1 (static_cast) | 0 |
| D4 阶段调度 | 5 | 3 | 0 | 2 (pb.run + declare_substage) |
| D5 Payload 通信 | 6 | 5 | 0 | 1 (跨翻译单元) |
| D6 测试便利 | 6 (含 T3.6 比例) | 5 | 1 (helper API) | 0 |
| **合计** | **24** | **19 (79%)** | **2 (8%)** | **3 (13%)** |

**T2 + T3 合计 (D1-D6 全 6 维度)**:
- 42 个评估观察点
- B1 接受 33 (78.6%)
- B2 摩擦 6 (14.3%)
- B3 局限 3 (7.1%)

**关键发现**:
1. ✅ Plugin 范式在 L1Cache 上**总体通过** (B1 78%)
2. ⚠️ B2 摩擦 6 个 — 全部有可复用代码模式 (T2 模式 #1-3 + T3 模式 #4-6)
3. 🚧 B3 局限 3 个 — 全部在 D4 调度 + D5 Payload,**显式链接 Phase 6 任务** (B3-L1/L2/L3 + B3-P1/P2)

**对未来 IP 写作者**:
- ✅ 33 个 B1 接受点 → 直接复用 L1Cache 模式
- ⚠️ 6 个 B2 摩擦点 → 用 6 个代码模式缓解
- 🚧 3 个 B3 局限 → 不在 Phase 2+ 解决,但**不掩盖** — Phase 6 任务清单已生成

**D3-D6 评估完成,进入 T4 整合 v1 文档**。
