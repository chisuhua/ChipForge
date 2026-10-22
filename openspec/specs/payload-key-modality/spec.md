# payload-key-modality Specification

## Purpose

定义 CH_MEM 模式下 **payload key 的模态约束**（value type 必须 mode-aware）与**生产者/消费者顺序契约**，使跨阶段共享的信号能承载**运行期 DAG 节点**而非 elaboration 期常量。

**为什么需要**：`PayloadStore` 的 `put` / `get` / `has` 按**精确类型匹配**（`include/cf/plugin/payload.h:122-128`）⇒ 裸类型 key 在 CH_MEM 下 cell 是 POD，只能装常量；消费端读到的就是 elaboration 期快照 ⇒ **编译通过、测试全绿、语义为零**。

**引入 change**：`chmem-mmu-keys-modality`（2026-10-22，归档于 `openspec/changes/archive/2026-10-22-chmem-mmu-keys-modality/`），是 `mmu-chmem-phase-b-c-launch` Phase B 的硬前置。对应 ADR-040 v3.0 §10.5 / §10.6。

## Requirements

### Requirement: Payload key 的 value type MUST 是 mode-aware 类型

跨阶段共享的 `cf::plugin::Payload<T>` key，其 value type `T` MUST 使用 `cf::plugin::uint_t<N>`（或 `cf::plugin::bool_t`），MUST NOT 使用裸 `bool` / `uintN_t` / `std::uintN_t`。

**理由**（CH_MEM 模式）：
- `PayloadStore` 的 `put` / `get` / `has` 是**精确类型匹配**（`include/cf/plugin/payload.h:122-128`）。
- mode-aware 时 `T` = `ch::core::ch_uint<N>` ⇒ cell 里装的是 **ch 代理**，可承载**运行期 DAG 节点**。
- 裸类型时 `T` 是 POD ⇒ cell 里只能装 **POD 常量** ⇒ 消费端读取得到的是 elaboration 期求值的快照 ⇒ **信号退化为常量**。

**TLM 模式零影响**：`cf::plugin::uint_t<N>` 在 TLM 模式解析为裸整型（`include/cf/plugin/uint_t.h:73-87`），`uint_t<1>` ≡ `uint8_t`、`uint_t<8>` ≡ `uint8_t`、`uint_t<64>` ≡ `uint64_t` ⇒ 行为等价。唯一需注意的差异是 1-bit 标志位：TLM 下 `uint_t<1>` = `uint8_t`（非 `bool`）⇒ 值语义仍为 0/1，读点 MUST 用 `static_cast<bool>` 或等价的显式转换。

**模板参数与 value type 是两件事**：key 的**模板参数** MUST 保持 **raw**（`cpu_keys<std::uint64_t>` / `csr_keys<ch_uint<32>>`）—— `PayloadStore` 按 `&key` 指针身份索引，换模板参数会得到**不同静态实例** ⇒ `has()` 恒 false。只有 **value type** 走 `uint_t<N>`。

**Where**: `ip/mmu/tlm/mmu_keys.h`、`bundles/csr_bundles.h`、`ip/cpu/tlm/cpu_keys.h`、`ip/cache/plugins/cache_keys.h`。
（2026-10-22 `chmem-cpu-keys-modality` 扩展：`bundles/csr_bundles.h` 与 `ip/cpu/tlm/cpu_keys.h` 两个文件从 allowlist 收敛为已合规；`ip/cache/plugins/cache_keys.h` 与 `ip/mmu/tlm/mmu_keys.h` 的剩余裸 key 仍待其 CH_MEM 消费点落地后由对应 change 收敛。）

#### Scenario: CH_MEM 模式下 key 的 cell 可承载运行期信号

- **WHEN** 一个 key 声明为 `Payload<cf::plugin::uint_t<64>>`，生产者向其 cell 写入一个 `ch_uint<64>` DAG 节点
- **THEN** 写入 MUST 成功（不产生类型不匹配）
- **AND** 消费者从该 cell 读取时 MUST 得到**同一个 DAG 节点**（`impl()` 指针相等），而非 POD 快照

#### Scenario: 裸类型 key 无法承载运行期信号

- **WHEN** 一个 key 声明为 `Payload<bool>`，生产者尝试写入 `ch_bool` 表达式
- **THEN** 编译 MUST 失败（`PayloadStore::put` 无匹配重载）
- **AND** 该失败 MUST 在**编译期**暴露（而非运行期静默降级）

### Requirement: Payload cell 的生产者 MUST 先于消费者写入

生产者 MUST 在消费者读取之前写入 payload cell，且 MUST NOT 依赖 C++ 侧的求值顺序或 plugin 注册顺序。

**机制**：CH_MEM 模式下 payload cell 内的 ch 代理是「C++ 侧重绑的指针」—— `logic_buffer<T>::operator=` 仅重绑 `node_impl_`（上游 `lnode/logic_buffer.tpp:21-26`），不发射 assign 节点。消费者通过 `PayloadStore::get` 拿到的是**读取瞬间的快照**。

**唯一不依赖 plugin 注册顺序的做法**：生产者与消费者 MUST 挂在**同一个 stage 名**下，生产者用 **EARLY** phase、消费者用 **NORMAL** phase（`Phase` enum: `EARLY=0 < NORMAL=1 < LATE=2`；`include/cf/plugin/pipe_builder.h` 的 `elaborate()` 在同一 stage 内按 phase 顺序执行，同 phase 桶内按插入序稳定排序）。

生产者 MUST NOT 注册新的 stage 名 —— `canonical_stage_order()` 是 stage 名字在 `stages_` 的**首现序**，与 plugin 注册顺序无关；新增 stage 名会改变 canonical order 并可能把生产者排到消费者之后。生产者需要额外 node 时 SHOULD 用 `declare_substage(parent, sub)`（只建 node，不进 `stages_`）。

**Where**: 任何跨 stage 的 payload 生产/消费对。

#### Scenario: 同 stage 内 EARLY 生产者对 NORMAL 消费者可见

- **WHEN** 生产者注册在 `("stage", EARLY)`、消费者注册在 `("stage", NORMAL)`
- **THEN** 消费者 MUST 读到生产者写入的值
- **AND** 该结果 MUST NOT 依赖 producer / consumer 插件的注册顺序

#### Scenario: 消费者早于生产者时读到空 cell

- **WHEN** 消费者注册在 `("stage", EARLY)`、生产者注册在 `("stage", LATE)`
- **THEN** 消费者 MUST 观察到 cell 不存在（`has()` 为 false），MUST NOT 读到陈旧值或默认值

### Requirement: 动态行为的验证 MUST 经 Simulator 读值

payload cell 中的 ch 节点是**运行期**信号。验证其动态性 MUST 经 Simulator 求值（tick ≥ 2 拍后读值），MUST NOT 用 C++ 侧的直接转换或缓冲读取判定 —— 后者对非常量节点会失败并返回默认值（上游 `ch_uint<N>::operator uint64_t` 对非常量节点报错）。

MUST NOT 用非 const `operator()` 探测 cell 是否存在：非 const `get` 在 miss 时会 emplace `T{}`（CH_MEM 下即 `ch_uint<N>()` **null impl**），污染 `has()` 结果与 DAG（`include/cf/plugin/payload.h:151-161`）。探测 MUST 用 `has()` + const-get。

#### Scenario: 运行期信号被证明逐拍更新

- **WHEN** 生产者把一个自翻转寄存器（`ch_reg` 的 next 值设为取反）的节点写入 cell
- **THEN** 消费者读到的节点在 Simulator **不少于 2 拍**的 tick 中 MUST 观测到值翻转
- **AND** 测试 MUST NOT 仅以"读到的值等于某常量"作为通过条件

#### Scenario: 冻结常量与运行期节点可区分

- **WHEN** 对照源的值在多拍内恒定（等价于旧实现里 `ch_bool(POD cell)` 得到的冻结常量）
- **THEN** 测试 MUST 能区分「读到运行期节点」与「读到冻结常量」⇒ 任一退化为常量的变异 MUST 使测试失败

### Requirement: mode-aware key 的生产者 MUST 写入 ch 节点，MUST NOT 折叠成 POD

生产者 MUST 直接向 mode-aware payload key 写入 ch 节点（`ch_uint<N>` / `ch_bool`），MUST NOT 使用 `static_cast<bool>` / `static_cast<uint64_t>` 等把非常量节点折成 POD 的转换。

**理由**：key 类型对了但表达式被折成 POD 时，cell 里装的是 **elaboration 期求值一次的常量** ⇒ 消费端拿到的条件恒定 ⇒ **信号语义为零，且不产生任何编译/运行期错误**。

**Where**: 任何向 mode-aware payload key 写入的 CH_MEM 生产者。

#### Scenario: 折叠后的常量与真实节点可区分

- **WHEN** 生产者把一个自翻转寄存器（`ch_reg` 的 next 值设为取反）的节点写入 cell
- **THEN** 消费者读到的节点在 Simulator **不少于 2 拍**的 tick 中 MUST 观测到值翻转
- **AND** 若生产者把同一表达式折成 POD 写入，消费者读到的值 MUST 恒定（翻转次数为 0）⇒ 二者 MUST 可区分

#### Scenario: 已知折叠反例（chmem-cpu-keys-modality 修复前的 3 处）

- **WHEN** 生产者用 `static_cast<bool>(<ch_bool 表达式>)` 写入 cell —— `ip/cpu/plugins/csr_chmem.h` 的 `CSR_READ_VALID` / `CSR_WRITE_EN` / `TRAP_PENDING` 三处
- **THEN** 写入的 MUST 是 **literal 节点**（`impl()->is_const() == true`）⇒ 值恒定
- **AND** 判据 MUST 能识别该状态（`impl()->is_const()`）⇒ 折叠 MUST 使测试失败

#### Scenario: 消费者读到的是节点而非 POD 快照

- **WHEN** 消费者从 mode-aware cell 读取
- **THEN** 读到的值 MUST 是 **ch 代理**（`impl()` 非空且指向 DAG 节点）
- **AND** 消费者 MUST NOT 用 `ch_bool(cell)` 之类的包装把节点再折成常量（`ch_uint<1>` → `ch_bool` 的转换 MUST 走 `impl()` 保持同一节点）

### Requirement: 已知裸类型 key 的收敛 MUST 与其 CH_MEM 消费点落地绑定

裸类型 key 的收敛 MUST 与"引入其 CH_MEM 消费者"的那个 change 同批落地。理由：改了 key 类型但没有消费者 ⇒ 无法用任何测试证明新类型真的承载了节点（只能靠编译期类型断言）⇒ 属于**无验证的改动**。

**Where**:
- `ip/cache/plugins/cache_keys.h` —— 等 cache CH_MEM 消费点落地
- `ip/mmu/tlm/mmu_keys.h` 的 `PERMS` / `PTW_ASID` / `PTW_L0_RAW` / `PTW_L1_RAW` / `PTW_L2_RAW` / `PTW_FAULT` / `SATP_MODE` / `SATP_PPN` —— 等 `RiscvMMUPluginChmem` 适配器（`chmem-ptw-walk`）落地

#### Scenario: 收敛顺序被记录而非静默跳过

- **WHEN** 一个裸类型 key 暂时保留
- **THEN** 它的归属 MUST 记录在 ADR-040 v3.0 §10.8 违规表 + `tools/check_plugin_portability.sh` Check 13 的 allowlist 中
- **AND** 记录 MUST 指向"引入其 CH_MEM 消费者的那个 change"，MUST NOT 是无归属的遗留
