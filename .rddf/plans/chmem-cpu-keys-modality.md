# chmem-cpu-keys-modality — TDD 实施计划

> **施工依据**：`.rddf/improvements/chmem-cpu-keys-modality.md`（AC-CK-1..14）
> **前置**：`chmem-mmu-keys-modality`（已 archive 2026-10-22）—— 复用其规则、测试范式、CI 检查
> **工期**：1.0-1.5 人日 ｜ **窗口**：Phase C.1 入口（Sprint 3）
> **基线（HEAD `962ecb2`，2026-10-22 更新）**：TLM `481 case / 1 known fail`（`test_7stage_riscv.cpp:124`）；CH_MEM `125 case / 19661 assertions`（原写 118 是 Phase B 之前的数，已过期）；`openspec validate --specs --strict` `72 passed + 4 failed`

## Architecture

两处 CPU/CSR 侧的裸类型 payload key ⇒ CH_MEM 下 cell 是 POD ⇒ 只能装 elaboration 期常量：

- `ip/cpu/tlm/cpu_keys.h` 4 key（`static_assert` **无** `#ifndef` 守护 ⇒ `T` 永远裸整型 ⇒ `Payload<T>` 是 POD）
- `bundles/csr_bundles.h` 5 个裸类型 key（4 × `Payload<bool>` + 1 × `Payload<uint8_t>`）

修法与先行 change 完全同构：key 的 **value type** 改 `cf::plugin::uint_t<N>`，**模板参数 `T` 保持 raw**（`PayloadStore` 按指针身份索引，换 T 会得到不同静态实例）。消费点从「`ch_bool(常量)` / `static_cast<bool>`」改为「直取 DAG 节点」。

活体缺陷：`ip/cpu/plugins/csr_chmem.h:191-194` 用 `static_cast<bool>(select(...))` 把运行期表达式折成常量写进 `Payload<bool>` ⇒ `CSR_READ_VALID` 通道语义为零（测试因走 `last_csr_write_en_` 观察口而全绿）。本 change 拆掉它。

**Tech Stack**：C++23 / Catch2 v3.7.0 / CppHDL `ch_uint<N>` / `cf::plugin::PayloadStore`

**关键约束**（违反即静默失效）：
1. key 模板参数保持 **raw**（`cpu_keys<std::uint64_t>` / `csr_keys<ch_uint<32>>` 是各自消费端的既有实例，不得改）
2. flag key 用 `uint_t<1>`（`ch_uint<1>`），**不用** `bool_t` —— 规避 cpphdl 5.052 bool↔ch_bool 歧义，且 `ch_uint<1>` 可直接作 `select()` 条件
3. `csr_keys<T>` 的 4 个 `Payload<T>` key（`CSR_READ_VALUE` / `CSR_WRITE_ADDR` / `CSR_WRITE_VALUE` / `TRAP_TARGET`）**不动** —— `cpu_factory_chmem.h:184` 以 `T = ch_uint<32>` 实例化，已 mode-aware

---

## Task 1: `cpu_keys` 模式化 (0.2 day)

**Files:**
- Modify: `ip/cpu/tlm/cpu_keys.h:31-49`
- Test: `tests/cpu/test_cpu_chmem_keys_modality.cpp`（Task 4 建）

- [ ] **Step 1.1: 写失败断言（RED）**

在 Task 4 的测试文件里先落编译期断言：

```cpp
static_assert(std::is_same_v<decltype(cf::plugin::PayloadStore{}.get(
    cf::cpu::tlm::payload::cpu_keys<std::uint64_t>::SAT)), ch_uint<64>&>);
static_assert(std::is_same_v<decltype(cf::plugin::PayloadStore{}.get(
    cf::cpu::tlm::payload::cpu_keys<std::uint64_t>::CPU_EXCEPTION_CODE)), ch_uint<8>&>);
```

Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc)`
Expected: **编译失败**（当前是 `std::uint64_t&` / `std::uint8_t&`）

- [ ] **Step 1.2: `static_assert` 加 Mode 守护**

```cpp
  static constexpr unsigned kBits = sizeof(T) * 8;
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");
#endif
```

- [ ] **Step 1.3: 4 个 key 改 mode-aware**

```cpp
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SAT{"cpu.sat"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_VADDR{"cpu.sfence_vaddr"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<kBits>> SFENCE_ASID{"cpu.sfence_asid"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>> CPU_EXCEPTION_CODE{"cpu.exception_code"};
```

Run: `cmake --build build -j$(nproc)`
Expected: 编译通过（TLM + CH_MEM 两个 target）

> ⚠ key 字符串名全部保留 → 日志/文档引用不断。

---

## Task 2: `csr_bundles` 模式化 (0.2 day)

**Files:**
- Modify: `bundles/csr_bundles.h:39-52`

- [ ] **Step 2.1: 写失败断言（RED）**

```cpp
static_assert(std::is_same_v<decltype(cf::plugin::PayloadStore{}.get(
    cf::cpu::csr::payload::csr_keys<ch_uint<32>>::CSR_READ_VALID)), ch_uint<1>&>);
```

Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc)`
Expected: **编译失败**（当前是 `bool&`）

- [ ] **Step 2.2: 5 个裸类型 key 改 mode-aware**

`CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING` → `Payload<cf::plugin::uint_t<1>>`；`Zicsr_OP` → `Payload<cf::plugin::uint_t<8>>`。key 顺序与字符串名不动。

- [ ] **Step 2.3: 注释更正**

文件头 `:12` 「1-bit (ch_bool)」→「`uint_t<1>`（TLM=uint8_t / CH_MEM=ch_uint<1>）」；`:18` `TRAP_PENDING` 的「由 `ibus_chmem.h` NORMAL select 消费」→「**消费者尚未接线**（ADR-040 v3.0 §10.8）」。

Run: `cmake --build build -j$(nproc)`
Expected: 编译通过

---

## Task 3: 拆 `csr_chmem` 常量折叠 + 消费点改「读节点」(0.3 day)

**Files:**
- Modify: `ip/cpu/plugins/csr_chmem.h:186-194` + `:258` 附近
- Modify: `ip/cpu/arch/riscv/int_alu_chmem.h:369-374`

- [ ] **Step 3.1: 写失败断言（RED）**

```cpp
// 鉴别力: CSR_READ_VALID 必须随 csr_addr 变化翻转（旧实现恒 false）
// 演示: 同一 PipeBuilder 跑两个 csr_addr（0x300 / 0xB00）→ 读回值必须不同
```

Run: `./build/bin/chipforge_tests_chmem "[cpu-chmem-keys]"`
Expected: **FAIL**（旧实现两拍都是 false，`static_cast<bool>` 折常量）

- [ ] **Step 3.2: 拆折叠（生产者侧）**

```cpp
// 旧: const bool read_valid_bool = static_cast<bool>(select(...));
//     n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);
// 新:
n->payloads().put(CsrKT::CSR_READ_VALID,
                  select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                         ch_uint<1>(ch_literal<1, 1>{})));
```

`:258` 若存在 `CSR_WRITE_EN` / `Zicsr_VALID` / `Zicsr_OP` 的同类折叠，同法拆除（实施时逐点核）。

- [ ] **Step 3.3: 消费点改「读节点」**

```cpp
// int_alu_chmem.h:374 —— 旧: auto csr_read_valid = ch_bool(n->operator()(CsrKT::CSR_READ_VALID));
// 新: 直取 ch_uint<1>（可直接作 select 条件）
auto csr_read_valid = n->operator()(CsrKT::CSR_READ_VALID);
```

- [ ] **Step 3.4: 顺带核 `CSR_WRITE_EN` / `CPU_EXCEPTION_CODE` 消费点**

```bash
grep -rn "CSR_WRITE_EN\|CPU_EXCEPTION_CODE" ip/ --include="*_chmem.h"
```

落到的每个读点都改成「读节点」；不存在的读点不新增。

Run: `cmake --build build -j$(nproc)`
Expected: 编译通过；Step 3.1 的断言转绿

---

## Task 4: 单元测试 (0.2 day)

**Files:**
- Create: `tests/cpu/test_cpu_chmem_keys_modality.cpp`
- Modify: `tests/CMakeLists.txt`（**手动**加进 `CHMEM_TEST_SOURCES`）

沿用 `tests/mmu/test_mmu_keys_modality.cpp` 的探针范式（自建 `PipeBuilder` + 最小探针插件 + 变异对照；`ctx_swap` 必须是 fixture **成员**）。

- [ ] **Step 4.1: 4 个 case**

| case | 断言 |
|---|---|
| `cpu_chmem_keys_cells_are_mode_aware` | Task 1/2 的 9 条编译期类型断言 |
| `cpu_chmem_keys_producer_node_reaches_consumer` | consumer 读到**同一 `impl()` 指针** 且 6 拍内 ≥5 次翻转 |
| `cpu_chmem_keys_consumer_before_producer_sees_no_cell` | consumer `EARLY` / producer `LATE` ⇒ `has()` false |
| `cpu_chmem_keys_mutations_are_detectable` | 冻结常量（0 翻转）对比活节点（≥5 翻转）；读错 key ⇒ `has()` false |

- [ ] **Step 4.2: 手动加入构建**

`tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 追加 `cpu/test_cpu_chmem_keys_modality.cpp`（该 list 是手工维护，`GLOB_RECURSE` 不覆盖）。

Run:
```bash
cmake --build build --target chipforge_tests_chmem -j$(nproc)
./build/bin/chipforge_tests_chmem "[cpu-chmem-keys]"
```
Expected: `4 test cases / ≥14 assertions / All passed`

- [ ] **Step 4.3: 变异对照（≥2 全红）**

| 变异 | 期望 |
|---|---|
| 生产者改回 `static_cast<bool>(select(...))` | ≥2 断言 RED（翻转次数掉到 0） |
| 消费者改回 `ch_bool(常量)` | ≥2 断言 RED |
| 读错 key（`CSR_WRITE_EN` 当 `CSR_READ_VALID`） | 1 断言 RED（`has()` false） |

每次变异后回退并复跑确认全绿。

---

## Task 5: CI allowlist 收缩 + 文档 + 回归 (0.3 day)

**Files:**
- Modify: `tools/check_plugin_portability.sh`（Check 13 allowlist）
- Modify: `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md` §10.8
- Modify: `AGENTS.md`（架构约束 #10 补充）、`CHANGELOG.md`

- [ ] **Step 5.1: Check 13 allowlist 从 4 收到 2**

删 `bundles/csr_bundles.h` 与 `ip/cpu/tlm/cpu_keys.h` 两行；保留 `ip/cache/plugins/cache_keys.h` + `ip/mmu/tlm/mmu_keys.h`（后者的 8 个 defer key 仍在）。

Run: `bash tools/check_plugin_portability.sh`
Expected: PASS，WARN 数 = 剩余 2 个文件的既有 WARN，**不比改前多**

- [ ] **Step 5.2: ADR-040 §10.8 违规表更新**

删 `cpu_keys.h:48` / `csr_bundles.h:42-54` 两行；`csr_chmem.h:191-194,258` 行标注 `✅ 已修 (2026-10-xx chmem-cpu-keys-modality)`；保留 `TRAP_PENDING` 未接线行与 `mmu_keys` 其余 8 key 行。

- [ ] **Step 5.3: 全套件回归（failing-set 逐条比对）**

```bash
./build/bin/chipforge_tests 2>&1 | tail -3          # 期望 481 case / 1 known fail
./build/bin/chipforge_tests_chmem 2>&1 | tail -3    # 期望 125 → 129 case / All passed
for s in verify_adr verify_plugin_decision check_plugin_portability doc_link_check roadmap-r9-check; do
  bash tools/$s.sh >/dev/null 2>&1 && echo "$s PASS" || echo "$s FAIL"
done
openspec validate --specs --strict 2>&1 | grep Totals   # 期望 72 passed + 4 failed
```

判据 = **failing-set 逐条相等**（不是"全绿"）。任何新增失败 = 回退排查，不修改 baseline。

- [ ] **Step 5.4: 归档**

`openspec archive chmem-cpu-keys-modality -y` → 补 `openspec/specs/payload-key-modality/spec.md` 的覆盖范围（本 change 属**扩展**，archive 会 merge 进既有 capability）。

---

## 验证命令速查

```bash
# 单家族快速迭代
./build/bin/chipforge_tests_chmem "[cpu-chmem-keys]"

# 定向
./build/bin/chipforge_tests_chmem "[chmem]"

# 反回归 grep（AC-CK-4）
grep -rn "static_cast<bool>(\s*select" ip/cpu/plugins/*_chmem.h    # 零命中
grep -rn "cpu_keys<ch_uint\|csr_keys<ch_uint<1>>" ip/              # 零命中
```
