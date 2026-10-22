---
id: chmem-cpu-keys-modality
status: done
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: []
review_by: 2026-12-15
---

# chmem-cpu-keys-modality — CPU/CSR payload key 模式化 + 拆除 csr_chmem 常量折叠

> **定位**：`mmu-chmem-phase-b-c-launch` **Phase C.1 的前置**（非 Phase B 前置 —— Phase B 的 vaddr 源走 `ip/cpu/core/payload_common.h` 的 `keys<T,XLEN>`，已按 T 参数化、无需本 change，见 phase B `tasks.md` 侦察结论 #2）。
> **施工依据**：`.rddf/plans/chmem-cpu-keys-modality.md`（TDD 5 步，AC-CK-1..14）。
> **工期**：1.0-1.5 人日 ｜ **窗口**：Phase C.1 入口（Sprint 3）。
> **同族先例**：`chmem-mmu-keys-modality`（已 archive 2026-10-22）—— 同一缺陷类、同一修法、同一顺序契约；本 change 复用其规则与 CI 检查，不重复发明。

**主题**: CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)
**优先级**: P1

## 架构依据

### 缺陷 — 与 `chmem-mmu-keys-modality` 同一类，两处残留

| 文件:行 | 声明 | CH_MEM 下的实际 cell |
|---|---|---|
| `ip/cpu/tlm/cpu_keys.h:40` | `Payload<T> SAT` | `Payload<std::uint64_t>`（`:31-33` 的 `static_assert` **无** `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 ⇒ 永远只能裸 `T`）⇒ POD |
| `ip/cpu/tlm/cpu_keys.h:43` | `Payload<T> SFENCE_VADDR` | 同上 ⇒ POD |
| `ip/cpu/tlm/cpu_keys.h:46` | `Payload<T> SFENCE_ASID` | 同上 ⇒ POD |
| `ip/cpu/tlm/cpu_keys.h:48` | `Payload<std::uint8_t> CPU_EXCEPTION_CODE` | POD |
| `bundles/csr_bundles.h:40` | `Payload<bool> CSR_READ_VALID` | POD |
| `bundles/csr_bundles.h:45` | `Payload<bool> CSR_WRITE_EN` | POD |
| `bundles/csr_bundles.h:48` | `Payload<bool> Zicsr_VALID` | POD |
| `bundles/csr_bundles.h:51` | `Payload<bool> TRAP_PENDING` | POD |
| `bundles/csr_bundles.h:49` | `Payload<std::uint8_t> Zicsr_OP` | POD |

对照已修好的同族（`ip/mmu/tlm/mmu_keys.h` 5 个输出 key）与原生正确的写法（`ip/cpu/core/payload_common.h:129,141` `Payload<cf::plugin::uint_t<32>>`）。

### 活体缺陷实例：`csr_chmem.h` 的常量折叠

`ip/cpu/plugins/csr_chmem.h:191-194`：

```cpp
const bool read_valid_bool = static_cast<bool>(
    select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                           ch_uint<1>(ch_literal<1, 1>{})));
n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);
```

`csr_addr_eq_B00` 是 `ch_bool`（运行期量）⇒ `static_cast<bool>` 在 elaboration 期求值 ⇒ 写进 `Payload<bool>` 的只能是**常量**。消费端 `ip/cpu/arch/riscv/int_alu_chmem.h:374` 读回同一个常量 ⇒ **CSR 读值覆写 `RD_DATA` 的通道（`CSRRW` old value / `CSRRS`）在 CH_MEM 下语义为零**。

现有 `[chmem]` 73/73 全绿，是因为测试经 `csr_chmem.h` 的 `last_csr_write_en_` 观察口断言（绕开 payload 通道），且 vendored ELF 47 个用例里无 CSR 读-改-写向量。⇒ 这是"测试全绿但语义为零"的**第三个活体实例**（前两个：`mmu_keys` 的 2 个 CH_MEM 消费点、`mmu_exception_handler_chmem.h` 的 no-op 闭包）。

### 下游被卡住的节点

| 下游 | 卡点 |
|---|---|
| Phase C.1（`mmu_exception_handler_chmem.h` 接线，`mmufault_clear = (CPU_EXCEPTION_CODE != 0)`） | `CPU_EXCEPTION_CODE` 是 POD ⇒ 只能读常量 ⇒ `clear_mmufault` 无法成为运行期信号 |
| Phase C.1 的 trap 路径（`csr_keys::TRAP_PENDING` → `ibus_chmem.h` 重定向） | `Payload<bool>` POD；且 `TRAP_PENDING` 在 CH_MEM 侧 **grep=0 消费者**（`csr_bundles.h:17` 的声称与代码不符，见 ADR-040 v3.0 §10.8） |
| `check_plugin_portability.sh` Check 13 的 allowlist 收敛 | 现预置 4 个文件；本 change 清掉 2 个（`cpu_keys.h` / `csr_bundles.h`）⇒ allowlist 从 4 收到 2 |

## 范围

### In Scope

1. `ip/cpu/tlm/cpu_keys.h` —— `static_assert` 加 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护（对齐 `payload_common.h:113-120`）+ `kBits` 常量 + 4 个 key 改 mode-aware（`SAT` / `SFENCE_VADDR` / `SFENCE_ASID` 用 `uint_t<kBits>`，`CPU_EXCEPTION_CODE` 用 `uint_t<8>`）
2. `bundles/csr_bundles.h` —— 5 个裸类型 key 改 mode-aware（4 × `bool` → `uint_t<1>`，`Zicsr_OP` → `uint_t<8>`）；14 处注释更正（`ch_bool` 措辞与实际声明对齐）
3. `ip/cpu/plugins/csr_chmem.h:191-194` + `:258` —— **拆除常量折叠**，直接 `put` `ch_uint<1>` 节点
4. `ip/cpu/arch/riscv/int_alu_chmem.h:369-374` —— 消费点改「读节点」（`ch_uint<1>` 直取作 `select` 条件）
5. `ip/cpu/plugins/hazard_chmem.h` / `mmu_exception_handler_chmem.h` —— 若有 `CPU_EXCEPTION_CODE` / `CSR_WRITE_EN` 消费点，同步改「读节点」（实施时逐点核）
6. 新增 1 个 CH_MEM 单测（类型断言 + 节点鉴别力 + 变异对照 ≥2）
7. `tools/check_plugin_portability.sh` Check 13 allowlist 从 4 文件收到 2 文件
8. 文档：ADR-040 v3.0 §10.8 违规表删 2 行 + `AGENTS.md` 纪律段补"CPU/CSR key 亦已模式化" + CHANGELOG

### Out of Scope (严禁触碰)

- `ip/mmu/tlm/mmu_keys.h` 其余 8 个裸 key（`PERMS` / `PTW_ASID` / `PTW_L0_RAW` / `PTW_L1_RAW` / `PTW_L2_RAW` / `PTW_FAULT` / `SATP_MODE` / `SATP_PPN`）—— **其 CH_MEM 消费点尚未落地**（`MMUPlugin_chmem` 由 Phase B 产、注册由 C.4 做）⇒ 现在改无消费点可验证；保留在 §10.8 登记表，归"引入其 CH_MEM 消费者的那个 change"
- **接线** `TRAP_PENDING`（`csr_bundles.h:51`）到 `ibus_chmem.h` —— 这是 Phase C.1 的 trap 重定向行为改动。本 change **只**做类型模式化（`bool` → `uint_t<1>`，值语义等价）以防未来接线时读常量，**不新增消费者**
- Phase B 的 `MMUPlugin_chmem.h` / `ip/cpu/plugins/mmu_chmem.h`（Phase B 产物）
- `ip/cpu/cpu_factory_chmem.h` 的 plugin 注册（C.4 范围）
- 修已知 fail（`test_7stage_riscv.cpp:124`）与 4 个 pre-existing spec fail（**failing-set 相等是唯一判据**）

## Why

### 1. Phase C.1 的 `mmufault_clear` 现在拿不到运行期信号

C.1 要求 `stall_ctrl_->halt_when(exc_code != 0)`（`mmu_exception_handler_chmem.h:86-89` 的注释代码）。`CPU_EXCEPTION_CODE` 是 `Payload<std::uint8_t>` POD ⇒ 读回的是常量 0 ⇒ `halt_when` 条件恒 false ⇒ **MMU fault 永不 stall fetch**。若不先修，C.1 交付的是"闭包已接线、条件恒假"的惰性骨架 —— 正是计划最警惕的 G3 陷阱。

### 2. `csr_chmem` 的常量折叠是一颗已装填的雷，且它就在被修的同一个缺陷类里

`CSR_READ_VALID` 通道在 CH_MEM 下语义为零。任何依赖 CSR 读值覆写 `RD_DATA` 的后续特性（`mstatus.MPRV` 读改、`mepc` 读取、Zicsr 完整回归）都会踩到它，且**表现为"实现写了但没生效"**，排查成本极高。

### 3. 不做这步，ADR-040 的新规则只覆盖了一半

`chmem-mmu-keys-modality` 把"payload key 必须 mode-aware"写成 ADR-040 v3.0 §10.5 的 MUST，但 allowlist 里还留着 2 个 CPU/CSR 文件。规则带 2 个豁免文件、且其中一个还在用 `static_cast<bool>(select(...))` 主动制造常量 ⇒ 规则的示范效应会被豁免清单吃掉。

### 4. 成本极低、收益是"解除 C.1 的硬阻塞"

与先行 change 同构（同一修法、同一测试范式、同一 CI 检查）。先行 change 的实测工期 1.5-2.0d 里约 40% 花在"发现缺陷 + 定顺序契约 + 建 CI 检查"，这些本 change 全部复用 ⇒ 1.0-1.5d 可完成。

## What Changes

### 1. `ip/cpu/tlm/cpu_keys.h`

```cpp
template <typename T = std::uint64_t>
struct cpu_keys {
  static constexpr unsigned kBits = sizeof(T) * 8;   // 32 / 64 —— 两模式同值
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_same<T, std::uint32_t>::value ||
                std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");
#endif
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SAT{"cpu.sat"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_VADDR{"cpu.sfence_vaddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_ASID{"cpu.sfence_asid"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>> CPU_EXCEPTION_CODE{"cpu.exception_code"};
};
```

- 模板参数 `T` 保持 raw（与 `mmu_keys` 同理：`PayloadStore` 按**指针身份**索引，`cpu_keys<ch_uint<64>>` 与消费端 `cpu_keys<std::uint64_t>` 是不同实例）。
- `static_assert` 加 `#ifndef` 守护是**必需**的：否则 CH_MEM 下 `Payload<T>` 的 `T` 仍只能是裸整型，模式化无从谈起。

### 2. `bundles/csr_bundles.h`

```cpp
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> CSR_READ_VALID{"csr.read_valid"};
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> CSR_WRITE_EN{"csr.write_en"};
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> Zicsr_VALID{"csr.zicsr_valid"};
static inline cf::plugin::Payload<cf::plugin::uint_t<8>> Zicsr_OP{"csr.zicsr_op"};
static inline cf::plugin::Payload<cf::plugin::uint_t<1>> TRAP_PENDING{"csr.trap_pending"};
```

- 5 个 key 的**字符串名与顺序全部保留**（key 名是调试/追踪标识，改名会破坏日志与文档引用）。
- TLM 侧 `uint_t<1>` ≡ `uint8_t`、`uint_t<8>` ≡ `uint8_t` ⇒ 读点 `static_cast<bool>` 仍编译且值等价（0/1）。

### 3. `ip/cpu/plugins/csr_chmem.h` —— 拆折叠

```cpp
// 改后：直写节点，禁 static_cast<bool>
n->payloads().put(CsrKT::CSR_READ_VALID,
                  select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                         ch_uint<1>(ch_literal<1, 1>{})));
```

`:258` 若存在同类折叠（`CSR_WRITE_EN` / `Zicsr_VALID` / `Zicsr_OP`）一并拆。

### 4. 消费点改「读节点」

```cpp
// int_alu_chmem.h:369-374 —— 旧: ch_bool / static_cast<bool> 折常量
// 新: 直取 ch_uint<1> 作 select 条件
auto csr_read_valid = n->operator()(CsrKT::CSR_READ_VALID);   // ch_uint<1>
```

`ch_uint<1>` 满足 `select()` 的 `HardwareType` 约束（`core/operators.h:40-42,147`），且 `ch_uint<1>::operator ch_bool()` 仅 `N==1` 时可用（`core/uint.h:123-125`）⇒ 需要 `ch_bool` 的位置可无损转换（**不是**常量折叠）。

### 5. 测试与 CI

- `tests/cpu/test_cpu_chmem_keys_modality.cpp`（**手动加入 `CHMEM_TEST_SOURCES`**）：编译期类型断言（9 个 key 的 cell 类型）+ 节点鉴别力（consumer 读到同一 `impl()` 指针且随拍翻转）+ 变异对照（折常量 / 读错 key）
- `check_plugin_portability.sh` Check 13 allowlist：`csr_bundles.h` / `cpu_keys.h` 从 allowlist 删除 ⇒ 从 4 文件收到 2（剩 `cache_keys.h` + `mmu_keys.h`）

## Acceptance

- [x] **AC-CK-1**：CH_MEM 下 `cpu_keys<std::uint64_t>::SAT` / `SFENCE_VADDR` / `SFENCE_ASID` ≡ `ch_uint<64>&`；`CPU_EXCEPTION_CODE` ≡ `ch_uint<8>&`（编译期断言）
- [x] **AC-CK-2**：CH_MEM 下 `csr_keys<ch_uint<32>>::CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING` ≡ `ch_uint<1>&`；`Zicsr_OP` ≡ `ch_uint<8>&`
- [x] **AC-CK-3**：**鉴别力** —— `csr_chmem` 写入的 `CSR_READ_VALID` 被消费端读到时**节点指针相同**且**随拍翻转**（`get_value` 相邻两拍不同）
- [x] **AC-CK-4**：**反回归 grep** —— `grep -rn "static_cast<bool>(\s*select" ip/cpu/plugins/*_chmem.h` **零命中**；`grep -rn "cpu_keys<ch_uint\|csr_keys<ch_uint<1>>" ip/` 零命中
- [x] **AC-CK-5**：TLM 侧行为等价（`uint_t<N>` ≡ `uintN_t`；9 个读点 `static_cast<bool>` 全兼容）；`[cpu]` / `[cpu-integration]` / `[cache]` 的 TLM 用例**零改动**通过
- [x] **AC-CK-6**：**failing-set 逐条相等** —— baseline 钉死：TLM `481 case / 1 known fail`（`test_7stage_riscv.cpp:124`）/ CH_MEM **125 case / 19661 assertions**（2026-10-22 Phase B 后更新；原写 118 已过期）/ `openspec validate --specs --strict` **72 passed + 4 failed**
- [x] **AC-CK-7**：**变异对照 ≥2 全红**（恢复 `static_cast<bool>` 折叠 / 消费端继续 `ch_bool(常量)` / 读错 key）
- [x] **AC-CK-8**：Check 13 allowlist 收缩后 **0 新增 WARN**（除 allowlist 剩余 2 文件外全场干净）
- [x] **AC-CK-9**：5 架构 gate 全 PASS + `doc_link_check.sh` 0 失败
- [x] **AC-CK-10**：`csr_bundles.h` 的 14 处 `ch_bool` 措辞注释与实际声明一致（无自相矛盾残留）
- [x] **AC-CK-11**：**无惰性代码** —— 不新增 `TRAP_PENDING` 消费者；不触碰 `mmu_keys` 其余 8 key
- [x] **AC-CK-12**：新测试已加入 `CHMEM_TEST_SOURCES`，`--list-tests` 可列出
- [x] **AC-CK-13**：**RED 证据** —— HEAD 状态下新测试的所有断言在旧代码上失败（编译期断言 = 编译失败；运行期断言 = 断言失败），输出贴进 `tasks.md`
- [x] **AC-CK-14**：ADR-040 §10.8 违规表删 `cpu_keys.h:48` + `csr_bundles.h:42-54` 两行，`csr_chmem.h:191-194,258` 行标注已修

## Capabilities

| capability | 状态 | 说明 |
|---|---|---|
| `payload-key-modality` | **扩展** | 规则已由 `chmem-mmu-keys-modality` 建立；本 change 把覆盖范围从 MMU 扩到 CPU/CSR，并把 allowlist 从 4 收到 2 |
| `mmu-chmem-phase-b-c-launch` | 依赖本 change | Phase C.1 的 `mmufault_clear` 与 CSR 读值覆写通道 |
| `chmem-csr-semantics` | 潜在新增（实施时判定） | `CSR_READ_VALID` 拆折叠后需要首个端到端向量验证（`CSRRS` 读值覆写 `RD_DATA`）；若纳入则同时补 1 个 CH_MEM ELF 或定向单测 |

## Impact

### 受影响文件

| 类别 | 文件 |
|---|---|
| 改 | `ip/cpu/tlm/cpu_keys.h`、`bundles/csr_bundles.h`、`ip/cpu/plugins/csr_chmem.h`、`ip/cpu/arch/riscv/int_alu_chmem.h`、`tools/check_plugin_portability.sh` |
| 新增 | `tests/cpu/test_cpu_chmem_keys_modality.cpp` |
| 视情况改 | `ip/cpu/plugins/mmu_exception_handler_chmem.h`、`ip/cpu/plugins/hazard_chmem.h`（若含 `CPU_EXCEPTION_CODE` / `CSR_WRITE_EN` 消费点） |
| 同步 | `tests/CMakeLists.txt`（`CHMEM_TEST_SOURCES`）、ADR-040 v3.0 §10.8、`AGENTS.md`、`CHANGELOG.md`、`.rddf/state/.planner-handoff.json` |

### 风险

| 风险 | 缓解 |
|---|---|
| `cpu_keys` 的 `static_assert` 一松，可能有人写出 `cpu_keys<ch_uint<64>>` 造成身份不等 | Check 13 已覆盖（非 `_chmem.h` 文件的裸类型 key 仍 WARN）+ AC-CK-4 反回归 grep + ADR-040 §10.5 明文"模板参数保持 raw" |
| 拆 `csr_chmem` 折叠后，原本"碰巧全绿"的路径可能变红 | 这正是判据的价值；若变红说明该路径原先就是常量语义，需补向量或记录 defer |
| TLM 侧 `uint_t<1>` ≡ `uint8_t` 使 `Payload<bool>` → `Payload<uint8_t>` 的读点类型变化 | 9 个读点逐一复核，显式 `static_cast<bool>` 补齐（先行 change 的同类改动零回归） |
| `TRAP_PENDING` 只改类型不接线，可能被视为"半成品" | Out of Scope 明文写清：接线是 C.1 行为改动，本 change 只消除"未来接线必读常量"的隐患 |
| scope creep（顺手接 `TRAP_PENDING` / 修 8 个 `mmu_keys`） | Out of Scope 表 + failing-set 相等判据 |

### 下游依赖

- **阻塞**：`mmu-chmem-phase-b-c-launch` 的 **C.1**（`mmufault_clear` 运行期信号 + trap 重定向）
- **解锁**：`check_plugin_portability.sh` Check 13 的 allowlist 从 4 收到 2（为最终"0 豁免"铺路）
- **前置**：`chmem-mmu-keys-modality`（已 archive，规则 + CI 检查 + 测试范式全部复用）

### 决策日志

| 决策 | 理由 | 出处 |
|---|---|---|
| 挂 Phase C.1 入口，不并入 Phase B | Phase B 的 vaddr 源走 `payload_common::keys<T,XLEN>`（按 T 参数化，CH_MEM 下 T=`ch_uint<32>` ⇒ 本就 mode-aware），不依赖本 change | 侦察实证（`cpu_factory_chmem.h` + `tests/cpu/test_cpu_chmem_vendored_elf.cpp:146`） |
| `cpu_keys` 的 `static_assert` 加 `#ifndef` 守护，而非删除 | 保留 TLM 侧的"RV32/RV64 only"约束；CH_MEM 下 `T` 仍是裸整型（key 身份稳定性），只是 `Payload` 的 value 类型走 `uint_t` ⇒ 约束对 TLM 有意义、对 CH_MEM 无意义 | 对齐 `payload_common.h:113-120` 既有写法 |
| 拆 `static_cast<bool>(select(...))` 折叠 | 它是**活体缺陷**（CSR 读值覆写通道语义为零），不是风格问题；且留着会抵消 §10.5 的规则示范 | ADR-040 v3.0 §10.8 已登记 |
| `TRAP_PENDING` 只做类型模式化、**不**接线 | 接线是 C.1 的行为改动（且 `csr_bundles.h:17` 的"由 ibus_chmem 消费"声称与代码不符，需先定设计）；类型先改可防未来接线读常量 | scope 最小化 |
| `mmu_keys` 其余 8 key 继续 defer | 无 CH_MEM 消费者 ⇒ 改了无法验证（与先行 change 同理由）；保留 §10.8 登记 | 先行 change Out of Scope 承接 |
| `csr_keys` 的 `Payload<T>` 4 个 key（`CSR_READ_VALUE` / `CSR_WRITE_ADDR` / `CSR_WRITE_VALUE` / `TRAP_TARGET`）**不动** | `cpu_factory_chmem.h:184` 以 `T = ch_uint<32>` 实例化 ⇒ 已 mode-aware，无需改 | 侦察实证 |
| 工期 1.0-1.5d（先行 change 为 1.5-2.0d） | 缺陷发现 / 顺序契约 / CI 检查 / 测试范式全部复用；本 change 增量是 9 个 key + 1 处折叠拆除 | 同族实测比值 |

---

## 归档注记（2026-10-22）

> **已实施并 archive** ⇒ `openspec/changes/archive/2026-10-22-chmem-cpu-keys-modality/`
> （capability `payload-key-modality` 扩展：**+2 added, ~1 modified**）。

**实测要点（与立项时的预估对照）**：

| 项 | 立项预估 | 实测 |
|---|---|---|
| 工期 | 1.0-1.5 人日 | 1 session（Phase B 之后同日完成） |
| 缺陷范围 | 9 个裸 key + 1 处折叠（`csr_chmem.h:191-194`） | 9 个裸 key + **3 处折叠**（`CSR_READ_VALID` / `CSR_WRITE_EN` / `TRAP_PENDING`）—— `CSR_WRITE_EN` 那处是施工前 grep 时新发现的，且它导致 **CH_MEM 的 CSR 写回通路整体失效**（此前整条通路是死的） |
| 测试 | 4 case | **5 case / 25 assertions**（多 1 个 = **直接用生产代码 `RiscvCsrPluginChmem` 驱动的端到端判据**：断言 cell 持有的是**计算节点**而非 literal） |
| 变异对照 | ≥2 | **6 个**（其中 2 个打在生产代码上，且是**编译期**拦住：折回 `static_cast<bool>` ⇒ `put(Payload<ch_uint<1>>&, bool)` 无匹配重载） |
| Check 13 allowlist | 4 → 2 | 4 → 2 ✅（并补 `[INFO]` 段让剩余 2 文件的既有违规可见） |
| 回归 | failing-set 相等 | TLM `481/1 known fail`（不变）；CH_MEM `125 → 130 case`（+5 全来自本 change），All passed |

**遗留（明确不在本 change）**：`cache_keys.h` 1 key + `mmu_keys` 8 key（无 CH_MEM 消费点 ⇒ 无验证的改动）；`TRAP_PENDING` 接线（C.1）；`mmu_exception_handler_chmem` 闭环接线（C.1，且剩余阻塞 = 无 CH_MEM 生产者 ⇒ 归 `chmem-ptw-walk`）。
