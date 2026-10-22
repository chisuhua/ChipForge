---
id: chmem-mmu-keys-modality
status: done
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: []
review_by: 2026-11-15
---

# chmem-mmu-keys-modality — MMU payload key 模式化 + CH_MEM 生产者/消费者顺序契约

> **定位**：`mmu-chmem-phase-b-c-launch` **Phase B 的硬前置**。本 change archive 之前，Phase B 的 B.3（`MMUPlugin_chmem` 真翻译）**不可开工**。
> **施工依据**：`.rddf/plans/chmem-mmu-keys-modality.md`（TDD 5 步，AC-MK-1..16）。
> **工期**：1.5-2.0 人日 ｜ **窗口**：Sprint 2 早期（10-23 ~ 10-30）。
> **已审**：oracle（架构决策校验）+ metis（实施前缺口分析）双审，2026-10-22；采纳项见「决策日志」。

**主题**: CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)
**优先级**: P1

## 架构依据

### 缺陷 A — MMU payload key 非 mode-aware ⇒ CH_MEM 通道只能装常量

`ip/mmu/tlm/mmu_keys.h:32-56` 的 5 个目标 key 用**裸类型**（`Payload<std::uint64_t>` / `Payload<bool>`），而框架的 mode-aware 类型是 `cf::plugin::uint_t<N>` / `bool_t`（`include/cf/plugin/uint_t.h:42-44`）。对照 `ip/cpu/core/payload_common.h:129,141`（`INSTRUCTION` / `RD_IDX`）用的是 `Payload<cf::plugin::uint_t<N>>` ✅。

**编译器实证**（CH_MEM 模式）：

```
PayloadStore::get(mmu_keys<uint64_t>::PADDR_VALID) → bool&          (POD)
PayloadStore::get(mmu_keys<uint64_t>::PADDR)       → std::uint64_t&  (POD)
put(Payload<bool>&, ch_bool(true))   → error: no matching function for call to 'put(Payload<bool>&, ch_bool)'
put(Payload<uint64_t>&, ch_uint<64>) → error: no matching function for call to 'put(Payload<unsigned long>&, ch_uint<64>)'
```

⇒ 消费端 `ch_bool(tlb_n->operator()(MmuKeys::PADDR_VALID))`（`ip/cpu/plugins/ibus_chmem.h:145` / `ip/cpu/plugins/dmem_chmem.h:145`）把节点**折成 elaboration 期常量**。

### 「全绿但零语义」在本仓是既有模式，不是理论风险

`ip/cpu/plugins/csr_chmem.h:191-194` 用 `static_cast<bool>(select(...))` 把运行期表达式折成常量 bool 再写进 `Payload<bool>`；消费端 `csr_chmem.h:318` / `ip/cpu/arch/riscv/int_alu_chmem.h:374` 再 `ch_bool(bool)` 读回 ⇒ **CSR 写使能经 payload 通道实际是常量化**。测试全绿是因为它改走 `last_csr_write_en_` 观察口（`csr_chmem.h:280-281`），绕开了 payload 通道。

### 「key 要模式化」是仓库内既存意图，只是从未执行

`ip/cpu/plugins/mmu_exception_handler_chmem.h:80` 自陈：*"未验证,Phase B 同步做 key 类型 ch_uint<8> 化后这里 OR-merge 启用。"*

### 缺陷 B（更硬）— elaboration 顺序：改完 key 仍会静默 Bare

| 事实 | 出处 |
|---|---|
| `canonical_stage_order()` = stage 名字在 `stages_` 的**首现序** | `include/cf/plugin/pipe_builder.h:215-224` |
| 插件的 `at_stage` 全部在 `pb->build()` 内注册 | `pipe_builder.h:156-157` |
| factory 在 `pb->build()` **之前**注册 `at_stage("fetch", EARLY, populate)` ⇒ `"fetch"` 被钉在最前 | `ip/cpu/cpu_factory_chmem.h:204` vs `:437` |
| 同 stage 内 **EARLY(0)→NORMAL(1)→LATE(2)**，同 phase 桶内按插入序稳定排序 | `pipe_builder.h:57-61` + `:414-424` |
| `declare_substage()` 只建 node，**不进 `stages_`** ⇒ 不扰动排序 | `pipe_builder.h:122-132` |
| CH_MEM 消费点 stage 名：ibus = `("fetch", NORMAL)`；dmem = `("memory", NORMAL)` | `ibus_chmem.h:70` / `dmem_chmem.h:63` |

⇒ 若 producer 注册新 stage 名 `tlb_lookup_ifetch`，它必然排在 `"fetch"` 之后 ⇒ **消费端先跑**，`has()` 恒 false ⇒ **永久 Bare**；loadstore 侧则取决于 MMU 与 DMem 的注册顺序（碰巧成立才成立）。一腿死、一腿活，且**完全静默**。

### 下游被卡住的 4 个节点

| 下游 | 卡点 |
|---|---|
| Phase B 的 B.3（`MMUPlugin_chmem` 真翻译） | 缺陷 A + 缺陷 B |
| Phase B 的 B.4（`test_mmu_chmem_basic` 判据） | 缺陷 A ⇒ AC-S3-3 无鉴别力 |
| Phase 3.5 BLOCKER-B5（ch_bool PTW_ACTIVE，`fetch_stall_requirements_v0.11.0.md` §1.5） | 缺陷 A ⇒ 运行期 PTW_ACTIVE 永远交不出来 |
| Phase C.1（`mmu_exception_handler_chmem` 接线） | 同根因（`CPU_EXCEPTION_CODE`）⇒ **归姊妹 change** |

## 范围

### In Scope

1. `ip/mmu/tlm/mmu_keys.h` —— 5 个目标 key 改 mode-aware（`MMU_VADDR` / `PADDR` / `EXCEPTION_CODE` / `PTW_ACTIVE` / `PADDR_VALID`）
2. `ip/cpu/plugins/ibus_chmem.h:141-149` + `ip/cpu/plugins/dmem_chmem.h:142-148` —— 2 个 CH_MEM 消费点改为「读 DAG 节点」
3. **顺序契约探针** —— 测试内最小探针插件，实证 producer `("fetch", EARLY)` → consumer `("fetch", NORMAL)` ⇒ 同一节点且随拍更新
4. TLM 侧 4 个消费点 + 6 个 TLM 测试的**兼容性复核**（预期零改动）
5. 新增 1 个 CH_MEM 单测（鉴别力 + 变异对照 ≥2）
6. `tools/check_plugin_portability.sh` —— WARN-only 的 key 模式化检查 + known-violations allowlist
7. 文档：ADR-040 新增「payload key 的 mode-aware 约束」节 + `openspec/specs/payload-key-modality/spec.md` + CHANGELOG + AGENTS.md 纪律段
8. `ip/cpu/plugins/mmu_exception_handler_chmem.h:86` 注释代码处置

### Out of Scope (严禁触碰)

- `MMUPlugin_chmem.h` / `ip/cpu/plugins/mmu_chmem.h`（Phase B 的产物）
- `ip/cpu/cpu_factory_chmem.h` 的 plugin 注册（Phase C.4；且 **Phase B 期间不得注册**）
- `ip/cpu/tlm/cpu_keys.h` 4 key + `bundles/csr_bundles.h` 4 key + `cpu_keys.h:34-36` static_assert ⇒ **姊妹 change `chmem-cpu-keys-modality`**，挂 **Phase C.1 入口**
- `mmu_keys` 其余 10 个裸类型 key（`SATP_MODE` / `SATP_PPN` / `PTW_VADDR` …）⇒ 同姊妹 change（CH_MEM 侧 satp 写入路径未落地 ⇒ 现在改无消费点可验证）
- `check_plugin_portability.sh` Check 2/7 的扫描范围扩展（follow-up `portability-check-mmu-coverage`）
- `ip/mmu/tlm/` → `ip/mmu/plugins/` 整体迁移（独立治理 change）
- 修 `test_7stage_riscv.cpp:124` 的 known fail 与 4 个 pre-existing spec fail（**failing-set 相等是唯一判据**）

## Why

### 1. 不做这步，Phase B 的 15d 会交付「全绿但零翻译」

Phase B 的 B.3 要求 `MMUPlugin_chmem` 用「单级 ch_mem TLB（8 项直接映射）+ 调 PtWalkFsmPlugin walk」发射 `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE`。ch_mem TLB 读与 PTW FSM 都是**运行期量**，而当前通道装不下它们（B.3 按原样**不可实现**）。若不先修，Phase B 只有三条出路：交付常量折叠的假翻译、跳过 B.3、或在实现期临时改契约 —— 三者都会把 v0.11.0 launch gate 判据 #3（`[cpu-l1-mmu-demo] 7/7` + `[mmu-verilator] 4/4`）建立在空语义上。

### 2. 计划的 AC-S3-3 目前**没有鉴别力**

AC-S3-3 要求「非恒等页表向量」+「消费端断言 `has(PADDR_VALID)==true` 且 sel 真选 PADDR」。但通道只能装常量 ⇒ 这两条判据都能被**常量**满足 ⇒ AC 全绿而 TLB/PTW 实质不存在。这与仓库里 `csr_chmem.h:191-194` 已经发生过的失败是同一类。

### 3. 不做这步，Phase 3.5 的依赖方向会原地复发

计划刚把 `fetch-stall-protocol-v0.2` 从 Phase B 的 pre_flight 移到 Phase 3.5 slot，理由是 BLOCKER-B5（"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"）**反向依赖 Phase B**。但通道装不下运行期 ch_bool ⇒ Phase B 交不出 PTW_ACTIVE ⇒ 计划刚解开的逻辑倒挂会在 Phase 3.5 原地复发（变成 Phase 3.5 → Phase B 反向依赖）。这是计划自己最警惕的 G3「惰性骨架」同款陷阱，只是换了个位置。

### 4. 不做这步，这颗雷会在 Phase C.1 再炸一次

`CPU_EXCEPTION_CODE`（`ip/cpu/tlm/cpu_keys.h:48`）是同一颗雷，而 C.1 要用它做 `mmufault_clear = (CPU_EXCEPTION_CODE != 0)`。姊妹 change 会在 Phase C.1 入口处理它 —— 但**本 change 必须先把"key 要模式化"这条规则成文**，否则姊妹 change 会重复做一遍同样的发现。

### 5. 仓库里没有任何成文或 CI 规则约束 payload key 声明类型

ADR-040 Tier-1 #3 与 AGENTS.md 架构约束 #3 只覆盖 **Bundle 字段**；`check_plugin_portability.sh` Check 7 只 grep 字面 `ch_reg<ch_uint|ch_mem<`。⇒ 不加规则，本次修完必然回退。

## What Changes

### 1. `ip/mmu/tlm/mmu_keys.h`（key 声明）

```cpp
template <typename T = std::uint64_t>
struct mmu_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");
  static constexpr unsigned kBits = sizeof(T) * 8;   // 32 / 64 —— 两模式同值

  // 目标 5 key: mode-aware
  // TLM:    uint_t<64> ≡ uint64_t, uint_t<8> ≡ uint8_t, uint_t<1> ≡ uint8_t
  // CH_MEM: ch_uint<64> / ch_uint<8> / ch_uint<1>  ⇒ 可承载运行期 DAG 节点
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> MMU_VADDR{"mmu.mmu_vaddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> PADDR{"mmu.paddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>>       EXCEPTION_CODE{"mmu.exception_code"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PTW_ACTIVE{"mmu.ptw_active"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PADDR_VALID{"mmu.paddr_valid"};
  // 其余 10 个 key 不动；key 字符串名全部保留
};
```

- 模板参数 `T` 保持 raw（若改用 `uint_t<64>` 会让 `static_assert` 在 CH_MEM 模式失败）。
- flag key 用 `uint_t<1>`（`ch_uint<1>`）**而非** `bool_t`（`ch_bool`）：规避 `csr_chmem.h:188-190` 自陈的 cpphdl 5.052 bool↔ch_bool 歧义；且 `ch_uint<1>` 满足 `select()` 的 `HardwareType` 条件（`core/operators.h:40-42,147`），可**直接**作 mux 条件。
- TLM 侧唯一类型变化是 2 个 flag key `bool→uint8_t` ⇒ **行为等价**（全部读点都是 `static_cast<bool>`）。

### 2. `ip/cpu/plugins/ibus_chmem.h` + `ip/cpu/plugins/dmem_chmem.h`（消费点）

```cpp
// 改后: 直取节点, 不再 ch_bool() 折常量
ch::core::ch_uint<1>  paddr_valid = tlb_n->operator()(MmuKeys::PADDR_VALID);
ch::core::ch_uint<32> paddr       = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
```

- `ch_uint<32>(ch_uint<64>)` 的截断语义**先探针后落**（`payload.h:109` 只是注释）；退路 `bits<31,0>`。
- `has()` 守卫保留 ⇒ 未写 cell 时仍走 vaddr（Bare byte-identical）。

### 3. 顺序契约（写入 spec + ADR-040，供 Phase B 遵守）

> CH_MEM 下 payload cell 的 ch 代理是「C++ 侧重绑的指针」（`build/_deps/install/include/cpp-hdl/lnode/logic_buffer.tpp:21-26` 的 `logic_buffer<T>::operator=` 只重绑 `node_impl_`），消费者**读取瞬间快照** ⇒ **生产者必须先写**。
> 唯一不依赖 plugin 注册顺序的做法：**producer 与 consumer 挂同一 stage 名，producer 用 EARLY、consumer 用 NORMAL**。
> producer **不得**注册新 stage 名；需要 node 用 `declare_substage(parent, sub)`（只建 node，不进 `stages_`）。
> 输入侧（vaddr→MMU）走插件句柄（仿 `ip/cpu/plugins/mmu_ptw_chmem.h:176-191`）；PTW_ACTIVE→fetch stall 走 **CtrlLink 聚合路径**（`elaborate()` 的聚合循环在全部闭包之后跑，`pipe_builder.h:435-452`）。

### 4. 探针 + 单测

- `tests/mmu/test_mmu_keys_modality.cpp`（**手动加入 `CHMEM_TEST_SOURCES`**）：编译期类型断言 / 鉴别力（节点指针相等 + 随拍更新）/ 顺序 / 变异对照
- 顺序契约探针（**仅测试内定义的最小插件**，自建 PipeBuilder —— `cpu_factory_chmem.h:119` 当前不注册任何 MMU plugin，**不要改 factory**）

### 5. CI 规则 + 已知违规登记

- `check_plugin_portability.sh` 加 **WARN-only** 检查：非 `*_chmem.h` 文件出现 `Payload<bool>` / `Payload<std::uint` ⇒ WARN；allowlist 预置 `bundles/csr_bundles.h` + `ip/cpu/tlm/cpu_keys.h`
- ADR-040 新节登记 5 类已知违规（csr_bundles 4 bool key + csr_chmem 常量化 + `csr_bundles.h:17` 声称 ibus_chmem 消费 `TRAP_PENDING` 但实测 0 次 + cpu_keys 4 key + mmu_keys 其余 10 key）

## Acceptance

- [ ] **AC-MK-1**：CH_MEM 下 `PADDR_VALID`/`PTW_ACTIVE` ≡ `ch_uint<1>&`；`PADDR`/`MMU_VADDR` ≡ `ch_uint<64>&`；`EXCEPTION_CODE` ≡ `ch_uint<8>&`（编译期断言）
- [ ] **AC-MK-2**：**鉴别力** —— producer 写入的 ch 表达式被 consumer 读到时**节点指针相同**且**随拍更新**（`sim.get_value` 相邻两拍不同）；禁止 C++ 侧直读 cell 判动态性
- [ ] **AC-MK-3**：`ch_uint<32>(ch_uint<64>)` 与旧 `ch_uint<32>(uint64_t)` 截断语义等价（实测；退路 `bits<31,0>`）
- [ ] **AC-MK-4**：TLM 侧**行为等价**（flag key 0/1 值语义 + `static_cast<bool>` 读点全兼容）；地址 key **类型逐字节等价**；6 个 TLM 测试文件**零改动**通过
- [ ] **AC-MK-5**：**failing-set 逐条相等** —— baseline 钉死：TLM **481 case / 1 known fail**（`test_7stage_riscv.cpp:124 node_count==17`）/ CH_MEM **114 case** / `openspec validate --specs --strict` **71 passed + 4 failed**（4 个 change 名逐条列出）
- [ ] **AC-MK-6**：**变异对照 ≥2 全红**（退回 `ch_bool(cell)` / 读错 key / producer 顺序改 LATE）
- [ ] **AC-MK-7**：Check 2 可见性自证通过（`ip/mmu/plugins/` 故意写坏 ⇒ FAIL；正常 ⇒ PASS）
- [ ] **AC-MK-8**：5 架构 gate 0 **失败** + `doc_link_check.sh` 0 失败 + 新增 WARN 检查的 **WARN 数不增加** + openspec validate 与 baseline 相等
- [ ] **AC-MK-9**：`openspec/specs/payload-key-modality/spec.md` 落地，**含顺序契约**（C.4 注册 MMU 时必须遵守）
- [ ] **AC-MK-10**：**无惰性代码**；Bare 路径 byte-identical（`[chmem]` 逐 case cycle 比对）
- [ ] **AC-MK-11**：反回归 grep —— `grep -rn "mmu_keys<std::uint" ip/ --include='*_chmem.h'` **零命中**
- [ ] **AC-MK-12**：顺序契约探针通过（producer `("fetch", EARLY)` → consumer `("fetch", NORMAL)` ⇒ 同一节点；**不依赖** plugin 注册顺序）
- [ ] **AC-MK-13**：反例探针 —— producer 改挂 `("fetch", LATE)` ⇒ consumer 拿不到 producer 的值
- [ ] **AC-MK-14**：2 个 CH_MEM 消费点改后**编译通过**
- [ ] **AC-MK-15**：新测试已手动加入 `CHMEM_TEST_SOURCES`，且 `--list-tests | grep <case>` 能列出
- [ ] **AC-MK-16**：**RED 证据** —— HEAD 状态下新增测试的编译错误输出被捕获并贴进 `tasks.md`
- [ ] **AC-MK-17**：下游文档同批修订（37KB plan 的 7 处残留 `:61 :68 :245 :714 :49 :99 :51` + `mmu-chmem-phase-b-c-launch.md:99` B.0 依赖 + `fetch_stall_requirements_v0.11.0.md` §1.5 注明 CtrlLink 聚合路径）

## Capabilities

| capability | 状态 | 说明 |
|---|---|---|
| `payload-key-modality` | **新增** | payload key 的 value type 必须用 `cf::plugin::uint_t<N>` / `bool_t`；CH_MEM 下可承载运行期 DAG 节点；含生产者/消费者顺序契约 |
| `mmu-chmem-phase-b-c-launch` | 依赖本 change | Phase B 的 B.3/B.4 前提 |

## Impact

### 受影响文件

| 类别 | 文件 |
|---|---|
| 改 | `ip/mmu/tlm/mmu_keys.h`、`ip/cpu/plugins/ibus_chmem.h`、`ip/cpu/plugins/dmem_chmem.h`、`tools/check_plugin_portability.sh`、`ip/cpu/plugins/mmu_exception_handler_chmem.h`（注释） |
| 新增 | `tests/mmu/test_mmu_keys_modality.cpp`、`openspec/specs/payload-key-modality/spec.md`、ADR-040 新节 |
| 只复核 | `ip/cpu/plugins/ibus.h`、`ip/cpu/plugins/dbus.h`、`ip/cache/plugins/L1CachePlugin.cpp`、`ip/cpu/plugins/mmu.cpp`、`ip/mmu/tlm/MMUPlugin.cpp` + 6 个 TLM 测试 |
| 同步 | `tests/CMakeLists.txt`（`CHMEM_TEST_SOURCES`）、`.rddf/plans/mmu-chmem-pipeline-integration.md`、`.rddf/improvements/mmu-chmem-phase-b-c-launch.md`、`.rddf/protocols/fetch_stall_requirements_v0.11.0.md`、`CHANGELOG.md`、`AGENTS.md`、`.rddf/state/.planner-handoff.json`、`.rddf/state/.planner-state.json` |

### 风险

| 风险 | 缓解 |
|---|---|
| `ch_uint<32>(ch_uint<64>)` 无构造路径 | 先探针后落；退路 `bits<31,0>` |
| TLM 侧 bool→uint8_t 触发隐式转换告警 | 6 个读点已逐一复核；显式 `static_cast<bool>` 补齐 |
| CH_MEM const-get 缺失时抛异常 | 保留 `has()` 守卫；探测用 const-get |
| 顺序契约与 factory populate 顺序冲突 | 同 stage 同 phase 桶内插入序稳定；探针 case 捕获 |
| scope creep（顺手修 known fail / 扩到 csr_bundles） | Out of Scope 表 + failing-set 相等判据 |
| 改完仍"全绿但零翻译" | AC-MK-2（节点指针级）+ AC-MK-12/13（顺序级）双重鉴别 |

### 下游依赖

- **阻塞**：`mmu-chmem-phase-b-c-launch` Phase B（B.3/B.4/B.5）
- **解锁**：Phase 3.5 BLOCKER-B5（PTW_ACTIVE 经 CtrlLink 聚合路径兑现）⇒ fetch-stall v0.2 起草前置成立
- **兄弟**：`chmem-cpu-keys-modality`（cpu_keys 4 key + csr_bundles 4 key + static_assert）⇒ 挂 Phase C.1 入口

### 决策日志

| 决策 | 理由 | 出处 |
|---|---|---|
| 改 key 类型而非新建 `mmu_keys_chmem` 双套 | 指针身份即语义（`payload.h:124,136,153` 全是 `cells_.find(&key)`）⇒ 双套 key = 把"静默 Bare"风险翻倍；且两套翻译逻辑会漂移 | oracle |
| 改 key 类型而非纯插件级 accessor | accessor 对**输入侧**正确且顺序无关（采纳为补充），但对**输出侧**不解决顺序约束；且 IBus 反向持 MMU 指针会成环 | oracle |
| 顺序契约用「同 stage + EARLY/NORMAL」而非调整 plugin 注册顺序 | 注册顺序**根本不在** elaboration 排序的输入里（`canonical_stage_order` 只看 `stages_` 首现序）；`declare_substage` 只建 node 不扰动排序 | oracle + 复核 |
| 走**独立 openspec change** 完整 rdd 流程（不并入 Phase B、不走 rdd-quick） | 跨 IP 契约 + ADR-040 新节 + 新 CI 检查，超 rdd-quick 的"≤2 文件、无公开 API 变更"边界；且 key 类型变更使既有 plan 的 key 裁决失效，需独立归档留追溯 | oracle + metis |
| 标志位用 `uint_t<1>` 而非 `bool_t` | 规避 cpphdl 5.052 bool↔ch_bool 歧义（`csr_chmem.h:188-190`）；`ch_uint<1>` 可直接作 `select()` 条件 | oracle + metis |
| 子项 D（`cpu_keys`）**排除**，另立姊妹 change 挂 Phase C.1 | Phase B 不需要；部分改会留 3 个雷；且 static_assert 与 `mmu_exception_handler_chmem.h:86` 注释代码需连带处理 | 用户裁决（2026-10-22） |
| 零回归判据 = **failing-set 相等**（不是"全绿"） | baseline 有 1 known fail + 4 pre-existing spec fail ⇒ "全绿"不可达，且会诱发 scope creep | metis |
| 工期报 **1.5-2.0d**（非 1.1d） | 含顺序契约探针 + 16 条 AC 加固 + CI WARN 检查 + 文档；oracle 的 2.5d 含 csr 实装（已排除） | 用户裁决（2026-10-22） |
