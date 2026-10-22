---
id: chmem-mmu-keys-modality
linked_change: openspec/changes/chmem-mmu-keys-modality/   # TODO(rdd-planner): 工件待建
linked_objective: .rddf/roadmap/objectives/objective-ip-mmu-mmu-chmem-phase-b-e.md
linked_feature: .rddf/roadmap/features/feat-mmu-chmem-integration.md
tier: standard
estimated_steps: 9
estimated_total_days: "1.5-2.0"
verifier: rdd-verifier
sprint_window:
  sprint-2-early: "10-23 ~ 10-30 (Phase 3 开工前的硬前置 — 必须先于 mmu-chmem-phase-b-c-launch Phase B)"
deps_status:
  blocked_by: []
  manual_deps: []
---

# Plan: chmem-mmu-keys-modality — MMU payload key 模式化 + CH_MEM 生产者/消费者顺序契约

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan phase-by-phase. Steps use checkbox (`- [ ]`) syntax for tracking.
> **本 plan 是 `mmu-chmem-phase-b-c-launch` Phase B 的硬前置**，不是可选优化。Phase B 的 B.3（MMUPlugin_chmem 真翻译）在本 change archive 之前**不可开工**。

**Goal:** 把 MMU payload 通道从「只能承载 elaboration 期常量」升级为「可承载运行期 DAG 节点」，并确立 + 实证 CH_MEM 模式下**生产者必先于消费者**的顺序契约 ⇒ 使 Phase B 的 B.3（ch_mem TLB + PtWalkFsmPlugin 真翻译）可实现，且 Phase 3.5 的 BLOCKER-B5（"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装）"）的依赖方向真正成立。

**Architecture:** 单一 source-of-truth plugin-style (ADR-083)。改 `ip/mmu/tlm/mmu_keys.h` 的 5 个目标 key 为 mode-aware 类型（`cf::plugin::uint_t<N>`，TLM ≡ 裸整型 / CH_MEM ≡ `ch_uint<N>`），改 2 个 CH_MEM 消费点为「读 DAG 节点」而非「读常量」，并用**测试内探针插件**实证顺序契约（producer 挂 `("fetch", EARLY)` → consumer 挂 `("fetch", NORMAL)` ⇒ 同一 stage 内 EARLY 必先于 NORMAL，**与 plugin 注册顺序无关**）。不写 `MMUPlugin_chmem.h`（那是 Phase B）。

---

## 一、为什么必须做（证据）

### 1.1 缺陷 A —— payload key 非 mode-aware（编译器实证）

`ip/mmu/tlm/mmu_keys.h` 的 key 用**裸类型**，而框架的 mode-aware 类型是 `cf::plugin::uint_t<N>` / `bool_t`（`include/cf/plugin/uint_t.h:42-44`）：

| key | 现状 | CH_MEM 下 cell 实际类型 |
|---|---|---|
| `MMU_VADDR` / `PADDR` | `Payload<std::uint64_t>` | `std::uint64_t&`（**POD**） |
| `PTW_ACTIVE` / `PADDR_VALID` | `Payload<bool>` | `bool&`（**POD**） |
| `EXCEPTION_CODE` | `Payload<std::uint8_t>` | `std::uint8_t&`（**POD**） |

对照：`ip/cpu/core/payload_common.h:129`（`INSTRUCTION`）/ `:141`（`RD_IDX`）用的是 `Payload<cf::plugin::uint_t<N>>` ✅ mode-aware。

**编译器实证**（复现命令见 §7）：

```
PayloadStore::get(mmu_keys<uint64_t>::PADDR_VALID) → bool&
PayloadStore::get(mmu_keys<uint64_t>::PADDR)       → std::uint64_t&
PayloadStore::get(mmu_keys<uint64_t>::PTW_ACTIVE)  → bool&
负例: put(Payload<bool>&, ch_bool(true))         → error: no matching function for call to 'put(Payload<bool>&, ch_bool)'
负例: put(Payload<uint64_t>&, ch_uint<64>(...))  → error: no matching function for call to 'put(Payload<unsigned long>&, ch_uint<64>)'
```

⇒ CH_MEM 模式下 MMU payload 通道**只能承载 elaboration 期常量**。消费端 `ch_bool(tlb_n->operator()(MmuKeys::PADDR_VALID))`（`ip/cpu/plugins/ibus_chmem.h:145` / `ip/cpu/plugins/dmem_chmem.h:145`）把节点折成常量。

### 1.2 缺陷 A 的仓库内活体实例（"全绿但零语义"是既有模式）

`ip/cpu/plugins/csr_chmem.h:191-194` 用 `static_cast<bool>(select(...))` 把**运行期表达式折成常量 bool** 再写进 `Payload<bool>`；消费端 `csr_chmem.h:318` / `ip/cpu/arch/riscv/int_alu_chmem.h:374` 再 `ch_bool(bool)` 读回 ⇒ **CSR 写使能经 payload 通道实际是常量化**。测试之所以全绿，是因为它改走 `last_csr_write_en_` 观察口（`csr_chmem.h:280-281`）而非 payload 通道验证。

⇒ "全绿但零翻译"在本仓不是理论风险，是已发生的模式。

### 1.3 支持"模式化"的仓库内既存意图

`ip/cpu/plugins/mmu_exception_handler_chmem.h:80` 自陈：

```
// 未验证,Phase B 同步做 key 类型 ch_uint<8> 化后这里 OR-merge 启用。
```

⇒ "key 类型要 ch_uint 化"在本仓**已被写下但从未执行**。本 change 是执行它，不是发明它。

### 1.4 缺陷 B（更硬）—— elaboration 顺序：改完 key 仍会静默 Bare

| 事实 | 出处 |
|---|---|
| `at_stage()` push 到 `stages_` **并**建 node | `include/cf/plugin/pipe_builder.h:112-120` |
| `declare_substage()` **只建 node，不进 `stages_`** ⇒ 不扰动排序 | `pipe_builder.h:122-132` |
| `canonical_stage_order()` = **stage 名字在 `stages_` 的首现序** | `pipe_builder.h:215-224` |
| 插件的 `at_stage` 全部在 `pb->build()` 内注册（`setup()` → `build()`） | `pipe_builder.h:156-157` |
| factory 在 `pb->build()` **之前**注册了 `at_stage("fetch", EARLY, populate)` ⇒ `"fetch"` 被钉在 `stages_` 最前 | `ip/cpu/cpu_factory_chmem.h:204` vs `:437` |
| `elaborate()` 按 canonical order 跑；同 stage 内 **EARLY(0)→NORMAL(1)→LATE(2)**，同 phase 桶内按插入序稳定排序 | `pipe_builder.h:57-61` + `:414-424` |
| CH_MEM 消费点 stage 名：ibus = `("fetch", NORMAL)`；dmem = `("memory", NORMAL)` | `ip/cpu/plugins/ibus_chmem.h:70` / `ip/cpu/plugins/dmem_chmem.h:63` |

**推演**：若 `MMUPlugin_chmem` 注册 `at_stage("tlb_lookup_ifetch", NORMAL, ...)`：
- **ifetch 侧**：该名字首现于 `pb->build()` 内 ⇒ 排在 `"fetch"` **之后** ⇒ **消费端先跑**，`has()` 恒 false ⇒ **永久 Bare**（`elaborate()` 只跑一遍，没有 TLM "下一拍 cell 还在"的补偿）。
- **loadstore 侧**：能否排在 `("memory", NORMAL)` 之前**取决于 MMU 与 DMem 的 plugin 注册顺序**（碰巧成立才成立）。

⇒ 一条腿死、一条腿活，且 `has()` 守卫让它**完全静默**。这是比缺陷 A 更硬的阻塞：**编译通过、测试全绿、翻译永不发生**。

**修法（零新机制）**：producer **不注册新 stage 名**，改挂到**与消费端同名 stage 的 EARLY phase** ⇒ 同 stage 内 EARLY 必先于 NORMAL（`Phase` enum 顺序保证）⇒ **与 plugin 注册顺序完全无关**。node 仍用 `declare_substage()` 只建 node。

### 1.5 下游被卡住的 4 个节点

| 下游 | 卡在哪 |
|---|---|
| Phase B 的 B.3（`MMUPlugin_chmem` 真翻译） | 缺陷 A（通道）+ 缺陷 B（顺序） |
| Phase B 的 B.4（`test_mmu_chmem_basic` 判据） | 缺陷 A ⇒ AC-S3-3 会被常量满足（无鉴别力） |
| Phase 3.5 BLOCKER-B5（ch_bool PTW_ACTIVE） | 缺陷 A ⇒ 运行期 PTW_ACTIVE 永远交不出来 |
| Phase C.1（`mmu_exception_handler_chmem` 接线） | 同族（`CPU_EXCEPTION_CODE`），**归姊妹 change**，见 §3 Out |

---

## 二、范围

### In Scope

1. `ip/mmu/tlm/mmu_keys.h` —— 5 个目标 key 改 mode-aware（D-1/D-2）
2. `ip/cpu/plugins/ibus_chmem.h:144-148` + `ip/cpu/plugins/dmem_chmem.h:144-147` —— 2 个 CH_MEM 消费点改写为「读 DAG 节点」（D-5）
3. **顺序契约探针**（Step 0.4）—— 测试内最小探针插件，实证「producer 挂 `("fetch", EARLY)` → consumer 挂 `("fetch", NORMAL)` ⇒ consumer 拿到 producer 写的**同一节点**且随拍更新」
4. TLM 侧 4 个消费点 + 6 个 TLM 测试 —— **兼容性复核**（预期零改动）
5. 新增 1 个 CH_MEM 单测文件（鉴别力核心 + 变异对照 ≥2）
6. `tools/check_plugin_portability.sh` —— 新增 **WARN-only** 模式化检查 + known-violations allowlist（D-7/S-3）
7. 文档：ADR-040 新增「payload key 的 mode-aware 约束」节 + `openspec/specs/payload-key-modality/spec.md`（含顺序契约）+ `.rddf/improvements/chmem-mmu-keys-modality.md` + `.planner-handoff.json` / `.planner-state.json` 刷新 + `CHANGELOG.md` + `AGENTS.md` 纪律段
8. `ip/cpu/plugins/mmu_exception_handler_chmem.h:86` —— 注释代码处置（S-5）

### Out of Scope（严禁触碰）

| 项 | 归属 | 排除理由 |
|---|---|---|
| `MMUPlugin_chmem.h` / `ip/cpu/plugins/mmu_chmem.h` | Phase B | 本 change 只开通道 + 证顺序，不写插件本体 |
| `cpu_factory_chmem.h` 注册 MMU plugin | Phase C.4 | Phase B 期间**不得**注册（否则 fetch 会用未翻译地址取指） |
| `ip/cpu/tlm/cpu_keys.h` 的 4 个 key（`CPU_EXCEPTION_CODE` / `SAT` / `SFENCE_VADDR` / `SFENCE_ASID`） | **姊妹 change `chmem-cpu-keys-modality`**，挂 **Phase C.1 入口** | Phase B 不需要；且 `cpu_keys.h:34-36` static_assert 与 `mmu_exception_handler_chmem.h:86` 注释代码需连带处理，部分改会留 3 个雷 |
| `bundles/csr_bundles.h` 的 4 个 `Payload<bool>` key（`CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING`） | 同上姊妹 change | 同根因第三处；只改 mmu 会留下同款陷阱 |
| `mmu_keys` 的 `SATP_MODE` / `SATP_PPN` / `PTW_VADDR` 等其余 10 个 key | 同上姊妹 change | CH_MEM 侧 satp 写入路径（C.1/C.5）未落地 ⇒ 现在改**无消费点可验证** |
| `check_plugin_portability.sh` 的 Check 2/7 扩到 `ip/mmu/` 扫描 | follow-up `portability-check-mmu-coverage` | 需先建 `ip/mmu/plugins/` 目录 + 处理既有两个盲区文件，本 change 只加 WARN-only 的 key 模式化检查 |
| `ip/mmu/tlm/` → `ip/mmu/plugins/` 整体迁移 | 已有决策：独立治理 change | — |
| 修 `test_7stage_riscv.cpp:124` 的 known fail / 4 个 pre-existing spec fail | **不做** | 与本 change 无关；failing-set 相等是唯一判据（防 scope creep） |

---

## 三、影响面清单

| # | 文件:行 | 现状 | 动作 | 风险 |
|---|---|---|---|---|
| 1 | `ip/mmu/tlm/mmu_keys.h:32-56` | 5 个目标 key 裸类型 | 改 mode-aware | 低（TLM 地址 key 类型逐字节等价） |
| 2 | `ip/cpu/plugins/ibus_chmem.h:141-149` | `ch_bool(cell)` 折常量 | `ch_uint<1>` 直取 + `select()` | **中**（宽度转换语义待实测） |
| 3 | `ip/cpu/plugins/dmem_chmem.h:142-148` | 同上 | 同上 | **中** |
| 4 | `ip/cpu/plugins/ibus.h:74,76,93-98` | `static_cast<bool>` / `static_cast<uint64_t>` | **零改动**（复核） | 无 |
| 5 | `ip/cpu/plugins/dbus.h:73-78` | 同上 | **零改动**（复核） | 无 |
| 6 | `ip/cache/plugins/L1CachePlugin.cpp:152-159` | `static_cast<uint_t<kAddrBits>>` | **零改动**（TLM 源仍是 uint64_t） | 无 |
| 7 | `ip/cpu/plugins/mmu.cpp:128-129` | `const std::uint8_t exc_code = ...` | **零改动**（TLM `uint_t<8>` ≡ uint8_t） | 无 |
| 8 | `ip/mmu/tlm/MMUPlugin.cpp:110-143` | TLM 生产者写 5 key | **零改动**（`= true` → uint8_t ✅） | 低 |
| 9 | `tests/cache/test_mmu_cache_integration.cpp:43-44,91` | `put(MMU_VADDR, static_cast<uint64_t>(...))` | **零改动** | 无 |
| 10 | `tests/cpu/test_mmu_paddr_propagation.cpp:110-111,169-175` | `= true` / `static_cast<bool>` | **零改动** | 无 |
| 11 | `tests/mmu/test_mmu_plugin.cpp:236-288` | `static_cast<bool>` | **零改动** | 无 |
| 12 | `tests/mmu/test_ptw_stall_integration.cpp:42-61` | `= true/false` | **零改动** | 无 |
| 13 | `tests/cpu/test_cpu_riscv_mmu_hooks.cpp:125-130` | `put(EXCEPTION_CODE, ...)` | **零改动** | 无 |
| 14 | `tests/cpu/test_ibus_chmem_sv32.cpp` / `test_dmem_chmem_sv32.cpp` | 断言 "PADDR_VALID 恒 false → Bare" | **预期仍 PASS**（无人写 cell ⇒ `has()` 仍 false） | 低 |
| 15 | `tests/CMakeLists.txt:118-148` | `CHMEM_TEST_SOURCES` 手工 list | 新测试**手动加入** | 低 |
| 16 | `ip/mmu/plugins/`（**目录不存在**） | — | 本 change 只建目录 + Check 2 可见性自证；`MMUPlugin_chmem.h` 由 Phase B 落 | 低 |
| 17 | `tools/check_plugin_portability.sh` | 无 key 模式化检查 | 加 WARN-only 检查 + allowlist | 低 |
| 18 | `ip/cpu/plugins/mmu_exception_handler_chmem.h:80-90` | `:86` 注释代码引用 `cpu_keys<ch_uint<8>>`（撞 `cpu_keys.h:34-36` static_assert） | 删除或改写为指向姊妹 change 的 TODO | 低 |

---

## 四、关键设计决策

**D-1 —— key 类型映射**（`mmu_keys<T>` 的模板参数 T 保持 raw，`static_assert` 不动）

```cpp
template <typename T = std::uint64_t>
struct mmu_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");
  static constexpr unsigned kBits = sizeof(T) * 8;   // 32 / 64 —— 两模式同值

  // ── 目标 5 key: mode-aware ──
  // TLM: uint_t<64> ≡ uint64_t, uint_t<8> ≡ uint8_t, uint_t<1> ≡ uint8_t
  // CH_MEM: ch_uint<64> / ch_uint<8> / ch_uint<1>  ⇒ 可承载运行期 DAG 节点
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> MMU_VADDR{"mmu.mmu_vaddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> PADDR{"mmu.paddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>>       EXCEPTION_CODE{"mmu.exception_code"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PTW_ACTIVE{"mmu.ptw_active"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<1>>       PADDR_VALID{"mmu.paddr_valid"};
  // 其余 10 个 key 不动
};
```

- key 的**字符串名字全部保留**（`"mmu.mmu_vaddr"` / `"mmu.paddr"` / `"mmu.paddr_valid"` …）⇒ 日志与断言零漂移。
- `kBits = sizeof(T) * 8` 在两模式下同值（T 始终是 raw 整型）⇒ `static_assert` 在 CH_MEM 模式**不会**失败（若改用 `uint_t<64>` 作模板参数会失败 —— 这是必须保留 raw T 的原因）。
- `mmu_keys` 仍放 `ip/mmu/tlm/`（本轮不做目录迁移）；**文件头注释**须写明"本文件在 CH_MEM 模式会经 `cf::plugin::uint_t` 实例化 `ch_uint`，非 TLM-only"。

**D-2 —— 标志位用 `uint_t<1>`（ch_uint\<1\>），不用 `bool_t`（ch_bool）**

- `ip/cpu/plugins/csr_chmem.h:188-190` 自陈 cpphdl 5.052 的 bool↔ch_bool 互转歧义 ⇒ 避开 `ch_bool`。
- `ch_uint<1>` 满足 `select()` 的 `HardwareType` 条件（`core/operators.h:40-42,147`）⇒ 可**直接**作 mux 条件，无需 `ch_bool()` 包装（包装 = 折成常量 = 要消灭的行为）。
- `ch_uint(const ch_bool&)` 有 `requires(N == 1)`（`core/uint.h:69-72`）⇒ `uint_t<1>` cell 可直接 `put(ch_bool 表达式)`。
- **接缝**：`uint_t<8>` / `uint_t<64>` cell 若 producer 端是 ch_bool 表达式需**显式扩宽**（无 `ch_uint<N>(ch_bool)`，N≠1）；跨宽构造 `ch_uint(const ch_uint<M>&)` 走 M<N→`zext`、**M>N→`bits<N-1,0>` 静默截断**（`core/uint.h:104-121`）⇒ 写进设计文档。反向 `operator ch_bool()`（`uint.h:123-125`，仅 N==1）隐式可用。

**D-3 —— TLM 侧唯一类型变化是 2 个标志位 bool→uint8_t**

`uint_t<1>` 在 TLM = `uint8_t`（`uint_t.h:74-84`）。全部读点均为 `static_cast<bool>(...)` 或 `= true/false` ⇒ **行为等价**（typeid 变了，但不是类型等价 —— AC-MK-4 按行为等价验收）。地址 key 在 TLM 下 `uint_t<64>` ≡ `uint64_t` ⇒ **类型逐字节等价**。

**D-4 —— 顺序契约（写入 spec，Phase B 必须遵守）**

> CH_MEM 模式下 payload cell 里的 ch 代理是「C++ 侧重绑的指针」：生产者把 cell 的 proxy **重绑**到 DAG 节点（`build/_deps/install/include/cpp-hdl/lnode/logic_buffer.tpp:21-26` 的 `logic_buffer<T>::operator=` 只重绑 `node_impl_`，不发射 assign），消费者**读取瞬间快照**该指针。
> ⇒ **生产者必须先写**。唯一不依赖 plugin 注册顺序的做法：**producer 与 consumer 挂同一个 stage 名，producer 用 EARLY phase，consumer 用 NORMAL phase**（`Phase` enum: EARLY=0<NORMAL=1<LATE=2，`pipe_builder.h:57-61` + `:414-424` 同 stage 内按 phase 顺序、同 phase 桶内按插入序稳定排序）。
> ⇒ producer **不得**注册新的 stage 名（会进 `stages_` 改变 canonical order）；需要 node 时用 `declare_substage(parent, sub)`（只建 node，不进 `stages_`，`pipe_builder.h:122-132`）。
> ⇒ **输入侧**（vaddr → MMU）走插件句柄（仿 `ip/cpu/plugins/mmu_ptw_chmem.h:176-191` 的 `start()/done()` 返回 `ch_uint<1>&`）⇒ 天然顺序无关。
> ⇒ **PTW_ACTIVE → fetch stall** 走 CtrlLink 聚合路径（`elaborate()` 的聚合循环在全部闭包**之后**跑，`pipe_builder.h:435-452`；Phase 1 已落 `halt_when_stateful`，capability `ctrllink-continuous-handle`）⇒ producer-after-consumer 天然安全。注意 payload cell 是 `ch_uint<1>` 而 `halt_when_stateful` 要 `ch_reg<ch_bool>` ⇒ 聚合点需一次显式转换（写进设计文档）。

**D-5 —— 消费点改写 + 宽度转换语义**

```cpp
// 现状（折成常量）:
paddr_valid = ch::core::ch_bool(tlb_n->operator()(MmuKeys::PADDR_VALID));
paddr       = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
// 改后（读节点）:
ch::core::ch_uint<1>  paddr_valid = tlb_n->operator()(MmuKeys::PADDR_VALID);
ch::core::ch_uint<32> paddr       = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
```
- `ch_uint<32>(ch_uint<64>)` 的截断语义**必须先探针后落**（`payload.h:109` 的"zext/trunc 推断"只是注释，非实测）。退路：`bits<31, 0>(...)`。
- 截断安全性：sv32 下 vaddr/paddr 均 ≤32bit ⇒ 上 32bit 恒 0（需断言）。

**D-6 —— cell 探测纪律（写进测试注释与 spec）**

- 一律 `has()` + **const** `get()`（`include/cf/plugin/payload.h:134-148` 的 const-get 缺失时**抛异常**）。
- **禁止**非 const `operator()`（`payload.h:151-161` 会 emplace `T{}` = `ch_uint<N>()` **null impl**，污染 `has()` 与 DAG）。
- 动态行为断言必须**经 Simulator 读值**（`sim.get_value(node)`）+ 相邻两拍节点值不同；**禁止** C++ 侧 `should_*()` / 直接读 cell 判动态性（那是陈旧值，见 capability `ctrllink-continuous-handle` 的契约）。
- **K5 边界约束**：CppHDL 的表达式即节点 ⇒ 同一 API（如 `OR_combined_halt_conds()`）两次调用得到两个**语义等价但指针不同**的节点 ⇒ **断言「值等价」而非「指针相等」**；只有「cell 传递」路径（consumer 读 producer 重绑进 cell 的 proxy）才可断言指针相等。出处：`.rddf/plans/20261022-v0110-mmu-chmem-execution-sequence.md` §4 K5。
- 范式：`tests/framework/test_ctrl_link_stateful.cpp:204-240`（`reg <<= !reg` 自翻转 + `sim.tick()` ≥2 拍 + `sim.get_value`）。⚠ `ch_reg::operator<<=` 必须在 **Simulator 创建前**调用。

**D-7 —— 新增 CI 检查（WARN-only + allowlist）**

`tools/check_plugin_portability.sh` 新增一条：扫 `ip/**` + `bundles/**` 的非 `*_chmem.h` 文件，出现 `Payload<bool>` / `Payload<std::uint` ⇒ **WARN**（不阻塞），known-violations allowlist 预置 `bundles/csr_bundles.h`（4 key）+ `ip/cpu/tlm/cpu_keys.h`（4 key）。升级为 FAIL 留待姊妹 change 完成。
> 理由：仓库内**没有任何成文或 CI 规则**约束 payload key 声明类型（ADR-040 Tier-1 #3 与 AGENTS.md 架构约束 #3 只覆盖 **Bundle 字段**；Check 7 只 grep 字面 `ch_reg<ch_uint|ch_mem<`）⇒ 不加规则必然回退。

**D-8 —— 已知违规登记（known-violations register）**

登记进 ADR-040 新节：
1. `bundles/csr_bundles.h:42-55` —— 4 个 `Payload<bool>` key（`CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING`），且 `csr_chmem.h:191-194,258` 把运行期表达式**常量化**后写入。
2. `bundles/csr_bundles.h:17` 声称 `TRAP_PENDING` 由 `ibus_chmem.h` NORMAL select 消费 —— **实测 `ibus_chmem.h` 中 `TRAP_PENDING` 出现 0 次** ⇒ 文档漂移 / CH_MEM trap 重定向路径疑似未接线（**仅登记，不在本 change 修**）。
3. `bundles/csr_bundles.h:12` 注释自陈 "1-bit (ch_bool)" 与实际声明 `Payload<bool>` 矛盾。
4. `ip/cpu/tlm/cpu_keys.h:39-48` —— 4 个裸类型 key（`SAT` / `SFENCE_VADDR` / `SFENCE_ASID` / `CPU_EXCEPTION_CODE`）。
5. `ip/mmu/tlm/mmu_keys.h` —— 其余 10 个裸类型 key（`SATP_MODE` / `SATP_PPN` / `PTW_VADDR` …）。

---

## 五、实施步骤（TDD 5 步）

### Step 0.0 — 前置 baseline（0.05d）

- [ ] 记录 HEAD（`176a7e1`）+ baseline：
  - TLM `chipforge_tests` = **481 case / 67135 assertions / 480 passed / 1 failed**（唯一失败 `tests/cpu/integration/test_7stage_riscv.cpp:124 REQUIRE(pb->node_count() == 17)` —— **已知 infra drift，本 change 不修**）
  - CH_MEM `chipforge_tests_chmem --list-tests` = **114 case**
  - `openspec validate --specs --strict` = **71 passed / 4 failed**（4 个 pre-existing change：cpu-pipeline-tick-fix / cpu-pipeline-tick-rca / feat-dhrystone-dmips-gate / mmu-cache-integration-test）
  - 5 架构 gate 全绿
  - **逐 case cycle 计数**（不只 PASS/FAIL）—— `[chmem]` 全量 550s/轮（AGENTS.md 记的 833s 是旧数）
- [ ] 确认 `CHMEM_TEST_SOURCES` 现状（`tests/CMakeLists.txt:118-148`）

### Step 0.1 — [RED] 新增 CH_MEM 单测骨架（0.2d）

- [ ] 新建 `tests/mmu/test_mmu_keys_modality.cpp`（约 220 LOC）
- [ ] **手动加入** `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`（GLOB 只覆盖 TLM target，漏加 ⇒ 测试**静默不编译不运行**）
- [ ] 4 个 case：
  1. **编译期类型断言**（CH_MEM 下 cell 类型 ≡ `ch_uint<1>&` / `ch_uint<64>&`）—— 当前必 FAIL
  2. **鉴别力 case**：producer 写 ch 表达式 → consumer 读到**同一节点**（`impl()` 指针相等）**且随拍更新**（`sim.get_value` 相邻两拍不同）
  3. **顺序 case**：consumer 先于 producer 读 ⇒ `has() == false`（不得读到陈旧值）
  4. **变异对照**（≥2，见 Step 0.5）
- [ ] ⚠ **RED = 编译失败**（不是运行时红）——测试 binary 根本 link 不出来。**必须捕获编译错误输出并贴进 `tasks.md`**（AC-MK-16）

### Step 0.2 — [GREEN-1] `mmu_keys` 模式化（0.1d）

- [ ] 按 D-1 改 `ip/mmu/tlm/mmu_keys.h` 的 5 个目标 key
- [ ] 文件头注释补"CH_MEM 模式会经 `cf::plugin::uint_t` 实例化 `ch_uint`"
- [ ] **不改**其余 10 个 key、不改 key 字符串名、不改模板参数 T / static_assert

### Step 0.3 — [GREEN-2] 2 个 CH_MEM 消费点改写（0.2d）

- [ ] **先探针后落**：`ch_uint<32>(ch_uint<64>)` 的截断语义实测（退路 `bits<31,0>`）
- [ ] 改 `ip/cpu/plugins/ibus_chmem.h:141-149`（`paddr_valid` 声明改 `ch_uint<1>`；`ch_bool(cell)` → 直取）
- [ ] 改 `ip/cpu/plugins/dmem_chmem.h:142-148`（同上）
- [ ] Bare 路径保持 byte-identical（`has()` 守卫保留 ⇒ 未写 cell 时仍走 vaddr）

### Step 0.4 — [GREEN-3] 顺序契约探针（0.25d）

- [ ] 在 `tests/mmu/test_mmu_keys_modality.cpp` 内加**探针插件**（**仅测试内定义，不进生产代码**）：
  - producer 插件：`at_stage("fetch", EARLY, ...)` 写 cell（node 由 `declare_substage("fetch", "probe_stage")` 建）
  - consumer 插件：`at_stage("fetch", NORMAL, ...)` 读 cell 并记录节点指针
- [ ] **自建 PipeBuilder**（`ip/cpu/cpu_factory_chmem.h:119` 当前**不注册任何 MMU plugin** ⇒ 从 factory 里找不到 MMU 是正常的，**不要去改 factory**，那是 C.4 的地盘）；参考 `cpu_factory_chmem.h:156-209` 的 populate 模式
- [ ] 断言：consumer 拿到的节点指针 == producer 写的节点指针；且经 `sim.get_value` 相邻两拍值不同
- [ ] **反例探针**（可选但建议）：producer 若改挂 `("fetch", LATE)` ⇒ consumer 应拿不到 producer 的值 ⇒ 证明契约是**必要**的而非巧合

### Step 0.5 — 变异对照 ≥2（0.1d）

- [ ] ① 消费点退回 `ch_bool(cell)`（折常量）⇒ 鉴别力 case 应红
- [ ] ② producer 写 `PADDR_VALID` 但 consumer 读 `MMU_VADDR` ⇒ 应红
- [ ] （建议 ③）producer 顺序改 LATE ⇒ 顺序 case 应红

### Step 0.6 — TLM 侧兼容性复核 + 回归（0.15d）

- [ ] 6 个 TLM 测试文件 + 4 个 TLM 消费点：**零改动**通过（若编译失败 ⇒ 某处依赖 `bool` 语义 ⇒ 逐个显式 `static_cast<bool>`）
- [ ] TLM 全量：**failing-set 与 baseline 逐条相等**（481 case / 1 known fail 不变）
- [ ] CH_MEM 全量：**114 + 新增 case** 全 PASS

### Step 0.7 — Check 2 可见性自证（0.05d）

- [ ] 在 `ip/mmu/plugins/`（**新建目录**）临时放一个不含 `ch_*` 的 `_probe_chmem.h` ⇒ `check_plugin_portability.sh` **必须 FAIL**
- [ ] 删除该文件 ⇒ **必须 PASS**
- [ ] ⇒ 证明 Phase B 的落点 CI 看得见（把 Phase B 的 AC-S3-1 前移）

### Step 0.8 — CI 检查 + 文档（0.3d）

- [ ] `tools/check_plugin_portability.sh` 加 D-7 的 WARN-only 检查 + allowlist
- [ ] ADR-040 新增「payload key 的 mode-aware 约束」节（含 D-4 顺序契约 + D-8 已知违规登记 + D-2 的 `ch_uint(const ch_bool&)` N==1 接缝 + 跨宽静默截断告警）
- [ ] `openspec/specs/payload-key-modality/spec.md`（Requirement + Scenario，格式参照 `openspec/specs/payload-store-get-miss-semantics/spec.md`）
- [ ] `ip/cpu/plugins/mmu_exception_handler_chmem.h:80-90` 的 `:86` 注释代码处置（S-5）
- [ ] `CHANGELOG.md` v0.11.0 段 + `AGENTS.md` 纪律段（payload key 必须 mode-aware）
- [ ] `.rddf/improvements/chmem-mmu-keys-modality.md`（5 段 draft，本 plan 落盘时已建）
- [ ] `.rddf/state/.planner-handoff.json` + `.planner-state.json` 刷新（`awaiting_builder` 加本 change）

### Step 0.9 — 终态验证 + archive（0.15d）

- [ ] 5 架构 gate 全绿（含新增 WARN 检查 —— **WARN 数不增加**）
- [ ] `openspec validate --specs --strict` ⇒ **71 passed / 4 failed 不变**（4 fail 逐条相等）
- [ ] `bash tools/run_chipforge_tests.sh --all` 全 PASS（除已知 baseline 项）
- [ ] `openspec archive chmem-mmu-keys-modality -y`
- [ ] **下游文档同步**（与本 change 同批，见 §9）

---

## 六、验收标准（AC）

### 类型与通道

- [ ] **AC-MK-1**：CH_MEM 模式下 `PADDR_VALID`/`PTW_ACTIVE` cell 类型 ≡ `ch_uint<1>&`；`PADDR`/`MMU_VADDR` ≡ `ch_uint<64>&`；`EXCEPTION_CODE` ≡ `ch_uint<8>&`（**编译期断言**，非口头）
- [ ] **AC-MK-2**：**鉴别力** —— producer 写入的 ch 表达式被 consumer 读到时**节点指针相同**（`impl()` 相等），且该值**随拍更新**（`sim.get_value` 相邻两拍不同）。**禁止**用 C++ 侧直接读 cell 判动态性
  > ⚠ **K5 边界约束**（CppHDL 表达式即节点 ⇒ 同一 API 两次调用产生**不同指针**）：**指针相等断言仅适用于「cell 传递」路径**（consumer 读 producer 重绑进 payload cell 的那个 proxy）。若断言涉及 **API 生成的节点**（如 `OR_combined_halt_conds()` 这类每次调用新建节点的 API），必须改断言**值等价**而非指针相等 —— 否则会写出必然失败或恒真的断言。出处：`.rddf/plans/20261022-v0110-mmu-chmem-execution-sequence.md` §4 K5
- [ ] **AC-MK-3**：`ch_uint<32>(ch_uint<64>)` 与旧 `ch_uint<32>(uint64_t POD)` 的**截断语义等价**（实测断言；退路 `bits<31,0>`）
- [ ] **AC-MK-4**：TLM 侧**行为等价** —— flag key 值语义 0/1 + `static_cast<bool>` 读点全兼容；地址 key **类型逐字节等价**（`uint_t<64>` ≡ `uint64_t`）；6 个 TLM 测试文件**零改动**通过
- [ ] **AC-MK-14**：`ibus_chmem.h:141-149` 与 `dmem_chmem.h:142-148` 改后**在 CH_MEM 下编译通过**（比单测绿更强的信号）

### 顺序契约

- [ ] **AC-MK-12**：顺序契约探针通过 —— producer `("fetch", EARLY)` → consumer `("fetch", NORMAL)` ⇒ consumer 读到 producer 写的**同一节点**；且**不依赖** plugin 注册顺序
  > ⚠ 同 **K5 边界约束**（见 AC-MK-2）：本项的"同一节点"指 **cell 传递**路径；若探针改为经某个**每次调用新建节点**的 API 传递信号，则断言须改为**值等价**。
- [ ] **AC-MK-13**：反例探针 —— producer 改挂 `("fetch", LATE)` ⇒ consumer 拿不到 producer 的值（证明契约必要）

### 回归归因

- [ ] **AC-MK-5**：**failing-set 逐条相等** —— HEAD `176a7e1` baseline：TLM 481 case（含 1 known fail `test_7stage_riscv.cpp:124 node_count==17`）/ CH_MEM 114 case / `openspec validate --specs --strict` 71 passed + 4 failed（4 个 change 名逐条列出）。实施后**同样三组数字的 failing set 逐字符相等**；**不得**顺手修 known fail
- [ ] **AC-MK-15**：新测试**已手动加入** `CHMEM_TEST_SOURCES`，且 `./build/bin/chipforge_tests_chmem --list-tests | grep <case>` 能列出
- [ ] **AC-MK-16**：**RED 证据** —— HEAD 状态下新增测试文件的编译错误输出被捕获并贴进 `tasks.md`
- [ ] **AC-MK-10**：**无惰性代码** —— 不引入任何"已接上但永不触发"的通道；Bare 路径 byte-identical（`[chmem]` 逐 case cycle 比对）

### 变异对照

- [ ] **AC-MK-6**：变异对照 **≥2 全红**（退回 `ch_bool(cell)` / 读错 key / producer 顺序改 LATE）

### 门禁与规则

- [ ] **AC-MK-7**：Check 2 可见性自证通过（`ip/mmu/plugins/` 故意写坏 ⇒ FAIL；正常 ⇒ PASS）
- [ ] **AC-MK-8**：5 架构 gate 0 **失败** + `openspec validate --specs --strict` PASS-or-equal-baseline + `doc_link_check.sh` 0 失败 + **新增 WARN 检查的 WARN 数不增加**
- [ ] **AC-MK-9**：spec `openspec/specs/payload-key-modality/spec.md` 落地，**含 D-4 顺序契约**（C.4 注册 MMU 时必须遵守）
- [ ] **AC-MK-11**：**反回归 grep** —— `grep -rn "mmu_keys<std::uint" ip/ --include='*_chmem.h'` **零命中**（防止新 chmem 文件再硬编码 raw 类型）
- [ ] **AC-MK-17**：**下游文档同批修订**（见 §9 表格 5 行）—— 37KB plan 的 7 处残留（`:61 :68 :245 :714 :49 :99 :51`）+ `mmu-chmem-phase-b-c-launch.md:99` B.0 依赖 + `fetch_stall_requirements_v0.11.0.md` §1.5 的 CtrlLink 聚合路径注明

---

## 七、验证命令

```bash
# 编译期类型探针（复现 §1.1 的实证）
g++-14 -std=c++23 -DCF_PLUGIN_USE_CH_MEM -fsyntax-only \
  -I. -Iinclude -Iip -Ibuild/_deps/install/include/cpp-hdl{,/core,/ast,/bundle,/utils,/bv,/cpu,/lnode} \
  /tmp/opencode/probe_payload_modality.cpp       # 当前 PASS(POD 断言成立) → 改后应 FAIL

# 回归
./build/bin/chipforge_tests                      # 期望 481 | 480 passed | 1 failed（同 infra drift）
./build/bin/chipforge_tests_chmem                # 期望 114 + 新增 case 全 PASS
bash tools/run_chipforge_tests.sh --all

# 门禁
bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh \
  && bash tools/check_plugin_portability.sh && bash tools/doc_link_check.sh \
  && bash tools/roadmap-r9-check.sh
openspec validate --specs --strict

# 反回归 grep（AC-MK-11）
grep -rn "mmu_keys<std::uint" ip/ --include='*_chmem.h'   # 期望零命中

# 新测试入列核对（AC-MK-15）
./build/bin/chipforge_tests_chmem --list-tests | grep <case-name>
```

---

## 八、风险与回滚

| # | 风险 | 概率 | 影响 | 缓解 | 回滚 |
|---|---|---|---|---|---|
| MK-R1 | `ch_uint<32>(ch_uint<64>)` 无构造路径 | 中 | Step 0.3 阻塞 | 先探针后落；退路 `bits<31,0>` | 只回退消费点，key 保持 mode-aware |
| MK-R2 | TLM 侧 bool→uint8_t 触发隐式转换告警/行为差 | 低 | TLM 回归红 | 6 个读点已逐一复核为 `static_cast<bool>` | 显式 `static_cast<bool>` 补齐 |
| MK-R3 | CH_MEM `PayloadStore` const-get 抛异常路径被触发 | 中 | elaboration 抛 | 消费点保留 `has()` 守卫；探测用 const-get | — |
| MK-R4 | `mmu_keys.h` 影响面被低估（漏掉某个 `put`/`get`） | 低 | 编译失败 | 编译期一次性暴露全部调用点 | 单文件 revert |
| MK-R5 | 顺序契约的 EARLY 挂载与 factory 的 populate 顺序冲突 | 中 | cell 被覆盖成默认值 | 同 stage 同 phase 桶内**插入序**稳定（factory populate 先插 ⇒ 先跑 ⇒ MMU 后覆写） | 探针 case 捕获 |
| MK-R6 | 改完后仍"全绿但零翻译" | 低 | 语义空心 | AC-MK-2（节点指针级）+ AC-MK-12/13（顺序级）双重鉴别 | — |
| MK-R7 | scope creep（顺手修 known fail / 扩到 csr_bundles） | 中 | 工期失控 | §2 Out of Scope 表 + failing-set 相等判据 | 逐文件回退 |
| MK-R8 | 新增 WARN 检查误报 | 中 | WARN 噪声 | allowlist 预置既有违规；AC-MK-8 要求 WARN 数不增加 | 改 allowlist |

**回滚粒度**：触及 4 个源文件 + 1 个新测试 + 1 个 gate 脚本 + 文档 ⇒ 单 commit `git revert` 可完全回退；不涉及持久化 schema / 公开 API 签名（`Payload<T>` 的**类型**变了但 key 对象与字符串名不变）。

---

## 九、下游交接契约

**本 change 交出**：
1. MMU 5 个 key 在 CH_MEM 下可承载 DAG 节点（AC-MK-1/2）
2. 2 个 CH_MEM 消费点已改为「读节点」（AC-MK-14）
3. **顺序契约**已实证并写入 spec + ADR-040（AC-MK-9/12/13）—— Phase B 的 `MMUPlugin_chmem` 必须遵守
4. `ip/mmu/plugins/` 是 CI 可见落点（AC-MK-7）
5. 基线快照（TLM / CH_MEM / openspec validate 三组数字 + 5 gate）

**Phase B 接手后的变化**：
- B.3「ch_mem TLB 8 项 + PtWalkFsmPlugin」从"不可实现"变"可实现" ⇒ **15d 工作量假设成立**（本 change 是解锁，不是加码）
- B.4 单测的鉴别力要求升级：除 AC-S3-3 外追加"翻译结果是 DAG 节点而非常量"的断言（同 AC-MK-2）
- B.5 集成回归期间**仍不得**把 `MMUPluginChmem` 注册进 `cpu_factory_chmem`（C.4 范围）⇒ B.4 单测须**自建 PipeBuilder**（MMU + IBus 同注册）验证 producer→consumer
- **Phase 3.5 的 BLOCKER-B5 依赖链现在才真正成立**

**必须同步修订的文档（与本 change 同批）**：
| 文件:行 | 修订 |
|---|---|
| `.rddf/plans/mmu-chmem-pipeline-integration.md:33,467-472` | key 裁决补「类型 mode-aware 化 + 生产者 EARLY 挂载」两段 |
| `.rddf/plans/mmu-chmem-pipeline-integration.md:61,68,245,714` | **B-2 残留**：`ip/mmu/tlm/MMUPlugin_chmem.h` / `bundles/mmu_bundles.h` 两处被否决方案仍在表里 ⇒ 必删 |
| `.rddf/plans/mmu-chmem-pipeline-integration.md:49,99,51` | **B-3 残留**：硬门禁表 / Phase 映射表 / HEAD 引用仍是旧排期 ⇒ 必改 |
| `.rddf/improvements/mmu-chmem-phase-b-c-launch.md:99` | B.0 改为「依赖 `chmem-mmu-keys-modality` archive」 |
| `.rddf/protocols/fetch_stall_requirements_v0.11.0.md` §1.5 | BLOCKER-B5 注明 PTW_ACTIVE 经 **CtrlLink 聚合路径**兑现（`pipe_builder.h:435-452`） |

**姊妹 change（另立，不在本 change）**：`chmem-cpu-keys-modality` —— `ip/cpu/tlm/cpu_keys.h` 4 key + `bundles/csr_bundles.h` 4 key + `cpu_keys.h:34-36` static_assert 放宽 + `csr_chmem.h:191-194` 常量化修复 ⇒ **挂 Phase C.1 入口**。

---

## 十、防 slop 护栏（强制）

1. **禁止**顺手修 known fail（`test_7stage_riscv.cpp:124`）与 4 个 pre-existing spec fail —— failing-set 相等是唯一判据。
2. **禁止**造模态适配层（`key_modal_adapter` / `is_modal_key` / `make_modal_key` / 统一 factory）—— 修法就是改声明 + 改消费点。
3. **禁止**造通用 fixture / 参数化 macro —— 单个 test case 照 `tests/framework/test_ctrl_link_stateful.cpp` 的范式写。
4. **禁止**在本 change 内扩 `check_plugin_portability.sh` 的 Check 2/7 扫描范围（只加 D-7 的 WARN-only key 模式化检查）。
5. **禁止**改 `cpu_factory_chmem.h` 的 plugin 注册（那是 C.4 的地盘；CH_MEM binary 当前不注册 MMU plugin 是**已知正常态**）。
6. **禁止**在本 change 内创建 `ip/mmu/plugins/MMUPlugin_chmem.h`（Phase B 的产物）。
7. **禁止**在 payload 通道上用非 const `operator()` 探测 cell（会 emplace null impl，污染 `has()` 与 DAG）。
8. **禁止**把"编译通过"当"行为正确"—— AC-MK-2 / AC-MK-12 是**行为级**断言。
9. **禁止**为观测动态性而 C++ 侧直读 cell（陈旧值）—— 必须走 `sim.get_value`。
