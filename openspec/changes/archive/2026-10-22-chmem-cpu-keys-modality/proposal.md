---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on:
  - openspec/changes/archive/2026-10-22-chmem-mmu-keys-modality
supersedes: []
related_to:
  - ADR-040 v2.0/v3.0 (TLM↔CH_MEM 双模同语义 + payload key 模态约束 §10.5/§10.8)
  - openspec/changes/mmu-chmem-phase-b-c-launch (Phase C.1 依赖本 change)
  - .rddf/improvements/chmem-cpu-keys-modality.md (立项草案 + AC-CK-1..14)
  - .rddf/plans/chmem-cpu-keys-modality.md (TDD 5 步施工计划)
---

# chmem-cpu-keys-modality — CPU/CSR payload key 模式化 + 拆除 csr_chmem 常量折叠

> **状态**: 🟢 ready → 实施中（2026-10-22 build agent）。
> **定位**: **`mmu-chmem-phase-b-c-launch` Phase C.1 的硬前置** —— 本 change archive 之前 C.1 交付的将是"闭包已接线、条件恒假"的惰性骨架。
> **同族先例**: `chmem-mmu-keys-modality`（已 archive 2026-10-22）—— 同一缺陷类、同一修法、同一顺序契约；本 change 复用其规则、测试范式与 CI 检查，不重复发明。

## Why

先行 change 把 MMU 侧的 5 个输出 key 模式化了，但 **CPU / CSR 侧还有 9 个裸类型 key**，它们是同一缺陷类的**残留**：

1. **`ip/cpu/tlm/cpu_keys.h` 4 个 key** —— `SAT` / `SFENCE_VADDR` / `SFENCE_ASID` 声明为 `Payload<T>`，而 `T` 被 `static_assert` 锁死在裸整型（`:34-36` 的 `static_assert` **无** `#ifndef CF_PLUGIN_USE_CH_MEM` 守护）⇒ `T` 永远只能是 `std::uint32_t` / `std::uint64_t` ⇒ `Payload<T>` 是 POD；`CPU_EXCEPTION_CODE` 直接写死 `Payload<std::uint8_t>`。
2. **`bundles/csr_bundles.h` 5 个 key** —— `CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING` 是 `Payload<bool>`，`Zicsr_OP` 是 `Payload<std::uint8_t>` ⇒ 全是 POD。

⇒ CH_MEM 模式下这些 cell **只能装 elaboration 期常量**，装不下 `ch_uint` / `ch_bool` 节点（`PayloadStore::put` 是精确类型匹配，`include/cf/plugin/payload.h:122-128`）。

**活体缺陷实例（本 change 的核心交付）** —— `ip/cpu/plugins/csr_chmem.h` 有 **3 处常量折叠**：

| 位置 | 代码 | 后果 |
|---|---|---|
| `:188-194` | `const bool read_valid_bool = static_cast<bool>(select(csr_addr_eq_B00, ...)); put(CSR_READ_VALID, read_valid_bool);` | `CSR_READ_VALID` 恒为**折叠常量** ⇒ `int_alu_chmem.h:374` 的 `select(csr_read_valid, CSR_READ_VALUE, RD_DATA)` 条件是常量 ⇒ **CSR 读值覆写 `RD_DATA` 的通道语义为零**（`CSRRW` 的 old value / `CSRRS` 的读值） |
| `:258` / `:277` | `const bool write_en_bool = static_cast<bool>(write_en_ch); put(CSR_WRITE_EN, write_en_bool);` | `CSR_WRITE_EN` 恒为折叠常量 ⇒ `csr_chmem.h:318` 的写使能恒 false ⇒ **CH_MEM 的 CSR 写回通路整体失效** |
| `:449` | `put(TRAP_PENDING, static_cast<bool>(trap_pending));` | `TRAP_PENDING` 恒为折叠常量（该 key 目前**无消费者**，见 ADR-040 v3.0 §10.8） |

现有 `[chmem]` 全绿，是因为 CSR 测试走 `last_csr_write_en_` **观察口**（绕过 payload 通道），且 vendored ELF 47 个用例里没有 CSR 读-改-写向量 ⇒ 这是"**测试全绿但语义为零**"的**第三个活体实例**（前两个：`mmu_keys` 的 2 个 CH_MEM 消费点、`mmu_exception_handler_chmem.h` 的 no-op 闭包）。

**下游被卡住的节点**：

| 下游 | 卡点 |
|---|---|
| Phase C.1（`mmu_exception_handler_chmem.h` 接线 `mmufault_clear = (CPU_EXCEPTION_CODE != 0)`） | `CPU_EXCEPTION_CODE` 是 POD ⇒ 读回常量 ⇒ `clear_mmufault` 无法成为运行期信号 |
| Phase C.1 的 trap 路径（`TRAP_PENDING` → `ibus_chmem.h` 重定向） | `Payload<bool>` POD；且 `TRAP_PENDING` 在 CH_MEM 侧 **grep=0 消费者**（`csr_bundles.h:17` 的声称与代码不符） |
| `chmem-ptw-walk` 的 `csr_write_satp` 适配器 | 需要 `cpu_keys::SAT` 是可承载节点的 key |
| `check_plugin_portability.sh` Check 13 的 allowlist 收敛 | 现预置 4 个文件；本 change 清掉 2 个 ⇒ allowlist 从 4 收到 2 |

## What Changes

| # | 改动 | 文件 |
|---|---|---|
| 1 | `cpu_keys` 4 个 key 改 mode-aware + `static_assert` 加 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 | `ip/cpu/tlm/cpu_keys.h` |
| 2 | `csr_bundles` 5 个裸类型 key 改 mode-aware + 14 处注释更正 | `bundles/csr_bundles.h` |
| 3 | **拆 3 处常量折叠**（`CSR_READ_VALID` / `CSR_WRITE_EN` / `TRAP_PENDING`），生产者直写 ch 节点 | `ip/cpu/plugins/csr_chmem.h` |
| 4 | 消费点改「读节点」 | `ip/cpu/arch/riscv/int_alu_chmem.h`、`ip/cpu/plugins/csr_chmem.h`、`ip/cpu/cpu_factory_chmem.h` |
| 5 | 新增 CH_MEM 单测（9 条编译期类型断言 + 鉴别力 + 顺序契约 + 变异对照） | `tests/cpu/test_cpu_chmem_keys_modality.cpp`（**手动加入 `CHMEM_TEST_SOURCES`**） |
| 6 | Check 13 allowlist 从 4 文件收到 2 文件 | `tools/check_plugin_portability.sh` |
| 7 | 文档：ADR-040 v3.0 §10.8 违规表 + `openspec/specs/payload-key-modality/spec.md` 覆盖范围扩展 + AGENTS.md + CHANGELOG | 多处 |

**不做**（scope 最小化）：

- `ip/mmu/tlm/mmu_keys.h` 其余 8 个裸 key（`PERMS` / `PTW_ASID` / `PTW_L0_RAW` / `PTW_L1_RAW` / `PTW_L2_RAW` / `PTW_FAULT` / `SATP_MODE` / `SATP_PPN`）—— 其 CH_MEM 消费点尚未落地（`MMUPluginChmem` 由 Phase B 产、`RiscvMMUPluginChmem` 归 `chmem-ptw-walk`）⇒ 现在改**无消费点可验证** ⇒ 归属"引入其 CH_MEM 消费者的那个 change"
- `ip/cache/plugins/cache_keys.h` 的 1 个裸 key —— 同上（cache CH_MEM 消费点未落地）
- **接线** `TRAP_PENDING` 到 `ibus_chmem.h` —— 属 Phase C.1 的行为改动；本 change 只消除"未来接线必读常量"的隐患
- Phase C.1 的 `mmu_exception_handler_chmem` 闭环接线本身（只解除其**类型**阻塞，并更新注释指向 C.1）
- 修 `test_7stage_riscv.cpp:124` 等已知 fail（**failing-set 相等是唯一判据**）

## 关键约束（违反即静默失效）

1. **key 模板参数保持 raw** —— `cpu_keys<std::uint64_t>` / `csr_keys<ch_uint<32>>` 是各自消费端的既有实例；换 `T` 会得到**不同静态实例**（`PayloadStore` 按 `&key` 指针身份索引）⇒ `has()` 恒 false
2. **flag key 用 `uint_t<1>`**（CH_MEM = `ch_uint<1>`），**不用** `bool_t` —— 规避 cpphdl 5.052 的 `bool`↔`ch_bool` 歧义；`ch_uint<1>` 可直接作 `select()` 条件（`select` 的条件只要 `HardwareType`）
3. **`csr_keys<T>` 的 4 个 `Payload<T>` key 不动**（`CSR_READ_VALUE` / `CSR_WRITE_ADDR` / `CSR_WRITE_VALUE` / `TRAP_TARGET`）—— `cpu_factory_chmem.h:184` 以 `T = ch_uint<32>` 实例化，已 mode-aware
4. **TLM 侧零影响** —— `uint_t<1>` ≡ `uint8_t`、`uint_t<8>` ≡ `uint8_t`、`uint_t<64>` ≡ `uint64_t` ⇒ 既有 TLM 读点全部照常编译

## 验收（AC-CK-1..14，详见 tasks.md）

**判据 = failing-set 逐条相等**（baseline：TLM `481 case` 含 1 known fail `test_7stage_riscv.cpp:124`；CH_MEM `125 case / 19661 assertions`；`openspec validate --specs --strict` `72 passed + 4 pre-existing failed`），**不是"全绿"**。
