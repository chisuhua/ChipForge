# csr-chmem-pipeline-integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 Oracle 2026-10-13 锁定的 R1-R3 设计决策, 实装 CH_MEM 模式 Zicsr 8 指令 + trap 3 指令 + 8 个 M-mode CSR, 使 `[cpu][csr-chmem]` 11/11 PASS + `[cpu-integration] 91/93` PASS + `[verilator] 1/1 (48 ELF)` 0 退化。

**Architecture:** 单一 source-of-truth plugin `ip/cpu/plugins/csr_chmem.h` (R7 移路径) + 3 处跨 plugin 小改 (`ibus_chmem.h` R1.1+R2.3, `IntAlu_chmem` R3.2) + 1 个新 bundles (`bundles/csr_bundles.h` R3.1+R2.1)。三段决策互锁: **R1 (csr_addr 重提取) → R3 (CSR 读值 RD_DATA 通路) → R2 (trap PC 重定向)**, 必须按序。

**Tech Stack:** C++23、CppHDL ch_state_machine DSL (per ADR-046 v2.0 §2.1.1 豁免)、Plugin 框架 (at_stage / payload keys / CtrlLink)、Catch2 v3.7.0。

---

## File Structure

### Production Code (实施改动)

| File | Responsibility | Tasks |
|---|---|---|
| `ip/cpu/arch/riscv/csr_chmem.h` (88 LOC skeleton, commit `c277799`) | CSR plugin 主实现 | R1.2+R1.3+R3.3+R3.4+R2.2+R2.5 (改 ~250 LOC) |
| `ip/cpu/plugins/csr_chmem.h` (R7 路径) | 同上, 移路径走 portability Check 2 | R7 (move + import fix) |
| `ip/cpu/plugins/ibus_chmem.h` (~155 LOC) | fetch aread + branch 改 2 处 | R1.1 (1 行) + R2.3 (4 行) |
| `ip/cpu/plugins/IntAlu_chmem` (or `int_alu_chmem.h`) | execute-LATE 加 4 行消费 CSR_READ_VALUE | R3.2 (4 行) |
| `bundles/csr_bundles.h` (新) | 9 个 payload key 定义 | R3.1 (7 keys) + R2.1 (2 keys) |
| `ip/cpu/cpu_factory_chmem.h` (8 plugin 注册) | 7→8 plugin | B.1-B.3 |

### Verification Code (新)

| File | Purpose |
|---|---|
| `tests/cpu/test_csr_chmem.cpp` (新, ~280 LOC) | `[cpu][csr-chmem]` 11 case: A.1 5 + R1.4 3 + R2.4 2 + R2.5 1 |
| `tests/cpu/manual_elf/zicsr_basic.S` (新, ~30 LOC) | Zicsr e2e ELF (csrrs+csrrw+ecall+tohost=1) |
| `tests/cpu/manual_elf/build_manual_elf.sh` (改) | 加 zicsr_basic.S → zicsr_basic.elf |
| `tests/cpu/integration/test_7stage_zicsr_e2e.cpp` (新, ~120 LOC) | 7-stage + 8-stage pipeline 跑 zicsr_basic.elf |

### Verification Commands (不动)

| File | Purpose |
|---|---|
| `tests/cpu/test_rv32ui_runner.cpp` | `[riscv-tests]` 40 ELF baseline |
| `tests/cpu/test_rv32um_runner.cpp` | `[riscv-tests]` 8 ELF baseline |
| `tests/mmu/test_mmu_plugin.cpp` | `[mmu][bare-shortcut]` 3 case (R1 不影响) |

---

## Phase 0 — 环境验证 + 基线建立 (估时 0.5 day)

### Task 0.1: 确认 build 环境 + 现有 skeleton commit

**Files:**
- Verify: `ip/cpu/arch/riscv/csr_chmem.h` (88 LOC, 来自 commit `c277799`)
- Verify: `ip/cpu/plugins/ibus_chmem.h` (现 ~155 LOC)

- [ ] **Step 1: 确认 csr_chmem.h skeleton 存在**

Run: `wc -l ip/cpu/arch/riscv/csr_chmem.h && git log -1 --oneline -- ip/cpu/arch/riscv/csr_chmem.h`
Expected: 88 LOC + commit msg 含 "A.2 RiscvCsrPluginChmem skeleton"

- [ ] **Step 2: 确认 ibus_chmem.h 当前状态**

Run: `wc -l ip/cpu/plugins/ibus_chmem.h && head -30 ip/cpu/plugins/ibus_chmem.h`
Expected: ~155 LOC, 头注释含 CH_MEM plugin 描述

- [ ] **Step 3: 确认 bundles/mem_bundles.h 命名风格 (R3.1 模板)**

Run: `head -40 bundles/mem_bundles.h 2>/dev/null || find bundles -name "*.h" | head -5`
Expected: 找到 mem_bundles.h 或同类, R3.1 模仿其命名空间/结构

- [ ] **Step 4: 跑现有 baseline**

Run: `./build/bin/chipforge_tests "[mmu][bare-shortcut]"`
Expected: `All tests passed (12 assertions in 3 test cases)` (= R1.1 改动前的基线)

- [ ] **Step 5: 跑架构门禁**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh`
Expected: 3 个脚本均 0 失败

---

## Phase 1 — R1 决策实施: csr_addr / funct12 信号源 (估时 1.5 day, Sprint 5 Week 1)

### Task 1.1: R1.1 — ibus_chmem.h 改 1 行 写 INSTRUCTION 统一 key

**Files:**
- Modify: `ip/cpu/plugins/ibus_chmem.h` ~line 146-155 (fetch aread 闭包)

- [ ] **Step 1: 读 ibus_chmem.h aread 闭包段**

Run: `sed -n '140,160p' ip/cpu/plugins/ibus_chmem.h`
Expected: 看到 `instr = aread(...)` 局部变量声明

- [ ] **Step 2: 查 cpu_keys<T>::INSTRUCTION 是否存在**

Run: `grep -rn "INSTRUCTION" bundles/cpu_bundles.h bundles/cpu_keys.h 2>/dev/null | head -5`
Expected: 命中 (key 存在) 或 未命中 (R1.1 副任务: 新增 key)

- [ ] **Step 3: 改 1 行 (写 INSTRUCTION)**

在 aread 闭包内, `instr = aread(...)` 之后加:
```cpp
(*node)(cpu_keys<T>::INSTRUCTION) = instr;
```
如 INSTRUCTION key 不存在, 改 `bundles/cpu_bundles.h` (仿 mem_bundles.h 风格) 加 1 行 `static inline cf::plugin::Payload<T> INSTRUCTION{"cpu.instruction"};`

- [ ] **Step 4: 编译 + 跑 baseline**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -10 && ./build/bin/chipforge_tests_chmem "[verilator]"`
Expected: 编译 0 error, `[verilator] 1/1 (48 ELF, 96 assertions)` 0 退化 (rv32ui-p-* 不读 INSTRUCTION)

- [ ] **Step 5: 跑 mmuchmem 回归**

Run: `./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3`
Expected: 59/59 PASS, 0 退化

### Task 1.2: R1.2 — csr_chmem.h execute-LATE 闭包: 重提取 csr_addr / funct12

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` (现 88 LOC skeleton)

- [ ] **Step 1: 读 csr_chmem.h skeleton build() 段**

Run: `sed -n '50,90p' ip/cpu/arch/riscv/csr_chmem.h`
Expected: 看到 `void build() override` 函数, 内部空或仅注册 (commit `c277799` 现状)

- [ ] **Step 2: 加 at_stage("execute", LATE, [...]) 闭包 (R1 重提取)**

在 `build()` 末尾 (现有闭包后) 加:
```cpp
pb->at_stage("execute", cf::plugin::Stage::LATE, [this](auto& payload) {
  using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
  const auto instr = static_cast<std::uint32_t>(payload[KT::INSTRUCTION].value());
  // R1 决策: 不扩 DecodedInst, 重提取 bits<31,20> = funct12 & csr_addr
  const auto csr_addr = (instr >> 20) & 0xFFF;
  const auto funct12  = (instr >> 20) & 0xFFF;  // 12-bit SYSTEM 字段
  const auto imm5     = (instr >> 15) & 0x1F;   // CSRRWI/CSRRSI/CSRRCI 立即数
  // 暂存到 instance 成员 (下一步 R1.3 用)
  csr_addr_ = csr_addr;
  funct12_  = funct12;
  imm5_     = imm5;
});
```

- [ ] **Step 3: 编译 + 跑 R1.4 Case 6-8**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5`
Expected: 编译 0 error (csr_addr_/funct12_/imm5_ 成员须先在 skeleton 加, 否则 link error; 如缺, 补到 private 段)

### Task 1.3: R1.3 — Zicsr 指令判定 (select 树, 不用 if)

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` execute-LATE 闭包 (续 1.2)

- [ ] **Step 1: 加 Zicsr_OP / Zicsr_VALID / CSR_WRITE_EN 写出**

在 R1.2 闭包末尾, 写 3 个新 payload key (key 定义属 R3.1 范畴, 此处先 forward-declare, R3.1 落地后激活):
```cpp
// R1.3 决策: Zicsr 指令 select 树判定, 严格不用 if(ch_bool)
const auto funct3 = (instr >> 12) & 0x7;
const auto opcode = instr & 0x7F;
const bool zicsr_valid = (opcode == 0b1110011);  // SYSTEM
// Zicsr_OP 映射: 001=CSRRW 010=CSRRS 011=CSRRC 101=CSRRWI 110=CSRRSI 111=CSRRCI
const std::uint8_t zicsr_op = ...;  // select 树 6 选 1, 不用 if/else
const bool is_write = (funct3 & 0b011) != 0;  // CSRRS/CSRRC 不写除非 mask!=0 (细节 deferred)
const bool csr_write_en = zicsr_valid & is_write & !is_mcycle_addr(csr_addr);
```

- [ ] **Step 2: 编译验证**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5`
Expected: 0 error

- [ ] **Step 3: A.1 5 case 仍 FAIL (预期, 缺 R3/R2)**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -10`
Expected: 5/5 FAIL (R1 仅做信号源, 通路靠 R3/R2)

### Task 1.4: R1.4 — 单元验证 3 case

**Files:**
- Modify: `tests/cpu/test_csr_chmem.cpp` (新, 现应已存在 A.1 5 case 框架, 如缺, 补建)

- [ ] **Step 1: 追加 Case 6-8 ~50 LOC**

```cpp
TEST_CASE("ZicsrAddressReextract_FromInstruction", "[cpu][csr-chmem]") {
  // 构造 INSTRUCTION = 0x30052073 = csrrs x4, mstatus; x10
  // 期望 csr_addr == 0x300
}

TEST_CASE("ZicsrImmediateReextract_FromInstruction", "[cpu][csr-chmem]") {
  // 构造 CSRRWI 0x30552073 + imm5=5
  // 期望 imm5 == 5
}

TEST_CASE("NonZicsrSystemInstruction_NoExtract", "[cpu][csr-chmem]") {
  // 构造 ECALL 0x00000073
  // 期望 Zicsr_VALID == false 但 funct12 == 0x000
}
```

- [ ] **Step 2: 跑 R1.4 3 case**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -5`
Expected: 3/3 PASS (R1.4 验证 R1 决策)

- [ ] **Step 3: 跑 mmuchmem 回归 (R1.4 须不破坏 R1.1+R1.2+R1.3 已落)**

Run: `./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3 && ./build/bin/chipforge_tests_chmem "[chmem]"`
Expected: 59/59 + 9/9 PASS

---

## Phase 2 — R3 决策实施: CSR 读值 → rd 写回通路 (估时 3 day, Sprint 5 Week 2)

### Task 2.1: R3.1 — 新建 bundles/csr_bundles.h

**Files:**
- Create: `bundles/csr_bundles.h` (~30 LOC)

- [ ] **Step 1: 仿 mem_bundles.h 头写**

```cpp
#ifndef CF_BUNDLES_CSR_BUNDLES_H
#define CF_BUNDLES_CSR_BUNDLES_H
#include <cstdint>
#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf { namespace cpu { namespace csr { namespace payload {
template <typename T = std::uint32_t>
struct csr_keys {
  static inline cf::plugin::Payload<T> CSR_READ_VALUE{"csr.read_value"};
  static inline cf::plugin::Payload<bool> CSR_READ_VALID{"csr.read_valid"};
  static inline cf::plugin::Payload<T> CSR_WRITE_ADDR{"csr.write_addr"};
  static inline cf::plugin::Payload<T> CSR_WRITE_VALUE{"csr.write_value"};
  static inline cf::plugin::Payload<bool> CSR_WRITE_EN{"csr.write_en"};
  static inline cf::plugin::Payload<bool> Zicsr_VALID{"csr.zicsr_valid"};
  static inline cf::plugin::Payload<std::uint8_t> Zicsr_OP{"csr.zicsr_op"};
  static inline cf::plugin::Payload<bool> TRAP_PENDING{"csr.trap_pending"};
  static inline cf::plugin::Payload<T> TRAP_TARGET{"csr.trap_target"};
};
}}}}
#endif
```

- [ ] **Step 2: 编译 + 跑 [csr-chmem] 仍 FAIL (R3 后续 task 没做)**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -3`
Expected: 0 error

### Task 2.2: R3.2 — IntAlu_chmem 改 4 行 消费 CSR_READ_VALUE

**Files:**
- Modify: `ip/cpu/plugins/IntAlu_chmem` (or 类似名, 先 `ls ip/cpu/plugins/*alu*` 找)

- [ ] **Step 1: 定位 IntAlu CH_MEM 文件**

Run: `ls ip/cpu/plugins/*alu* ip/cpu/plugins/IntAlu* 2>&1 | head -5`
Expected: 找到 `int_alu_chmem.h` 或 `IntAlu_chmem.h` (若不存在, 标 deferred + ADR 提请)

- [ ] **Step 2: 加 4 行 select 树**

在 execute-LATE 闭包内, RD_DATA 节点写点后:
```cpp
// R3 决策: 消费 CSR_READ_VALUE 覆写 default RD_DATA
(*node)(cpu_keys<T>::RD_DATA) = select(
    csr_keys<T>::CSR_READ_VALID,
    csr_keys<T>::CSR_READ_VALUE,
    default_rd_data);
```

- [ ] **Step 3: 编译 + 跑 [cpu] baseline 不退化**

Run: `cmake --build build 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu]"`
Expected: 0 error + `[cpu]` baseline 不退化 (R3.2 单独无害, 无 CSR_READ_VALID 时 select 退化)

- [ ] **Step 4: 跑 [mmu] 回归**

Run: `./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3`
Expected: 59/59 PASS

### Task 2.3: R3.3 — csr_chmem.h execute-LATE 写 CSR_READ_VALUE (旧值)

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` execute-LATE 闭包 (续 R1.3)

- [ ] **Step 1: 加 CSR_READ_VALUE / CSR_READ_VALID 写出 (select 树读 8 CSR)**

在 R1.3 闭包末尾, 加:
```cpp
// R3.3 决策: 8 选 1 mux 读旧值 (select 树, 不用 if)
const std::uint32_t read_value = select(
    (csr_addr == 0x300), mstatus_,
    select((csr_addr == 0x305), mtvec_,
    select((csr_addr == 0x340), mscratch_,
    select((csr_addr == 0x341), mepc_,
    select((csr_addr == 0x342), mcause_,
    select((csr_addr == 0x344), mip_,
    select((csr_addr == 0x304), mie_,
    mcycle_)))))));  // 0xB00 default
(*node)(csr_keys<T>::CSR_READ_VALUE) = select(zicsr_valid, read_value, 0u);
(*node)(csr_keys<T>::CSR_READ_VALID) = zicsr_valid & (csr_addr != 0xB00);  // mcycle 单独路径
```

- [ ] **Step 2: 编译 + 跑 A.1 Case 1 (CSRRW x1, mstatus)**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -5`
Expected: Case 1 PASS (CSRRW 基础), Case 2-5 仍 FAIL

- [ ] **Step 3: 跑 [verilator] baseline**

Run: `./build/bin/chipforge_tests_chmem "[verilator]"`
Expected: 1/1 (48 ELF) 0 退化 (R3.3 不影响 rv32ui-p-*)

### Task 2.4: R3.4 — csr_chmem.h writeback-NORMAL 写 CSR 寄存器 (新值, select 树)

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` (新闭包, writeback-NORMAL)

- [ ] **Step 1: 加 at_stage("writeback", NORMAL, [...]) 闭包**

```cpp
pb->at_stage("writeback", cf::plugin::Stage::NORMAL, [this](auto& payload) {
  // R3.4 决策: 8 个独立 write-enable + ch_reg <<= 赋值 (禁用 if (ch_bool))
  using csrKT = cf::cpu::csr::payload::csr_keys<T>;
  const auto write_addr  = payload[csrKT::CSR_WRITE_ADDR].value();
  const auto write_value = payload[csrKT::CSR_WRITE_VALUE].value();
  const bool write_en    = payload[csrKT::CSR_WRITE_EN].value();
  // 8 个 CSR 写 (mcycle 永不被 CSRR* 写)
  mstatus_  <<= select(write_en & (write_addr == 0x300), write_value, mstatus_);
  mtvec_    <<= select(write_en & (write_addr == 0x305), write_value, mtvec_);
  mscratch_ <<= select(write_en & (write_addr == 0x340), write_value, mscratch_);
  mepc_     <<= select(write_en & (write_addr == 0x341), write_value, mepc_);
  mcause_   <<= select(write_en & (write_addr == 0x342), write_value, mcause_);
  mip_      <<= select(write_en & (write_addr == 0x344), write_value, mip_);
  mie_      <<= select(write_en & (write_addr == 0x304), write_value, mie_);
  // mcycle_ 不写 (自由计数, R2.5)
});
```

- [ ] **Step 2: 编译 + 跑 A.1 Case 1+2**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -5`
Expected: Case 1 (CSRRW mstatus) + Case 2 (CSRRS mcycle) PASS

- [ ] **Step 3: 跑 [cpu] baseline + 架构 gate**

Run: `./build/bin/chipforge_tests_chmem "[cpu]" && bash tools/check_plugin_portability.sh 2>&1 | tail -3`
Expected: 0 error + 12/12 PASS (R3.4 走 select 树, 不破 portability Check 5)

### Task 2.5: R3.5 — R3 单元验证 (5 case PASS)

**Files:**
- 无新文件, 跑 Phase A.1 5 case + R1.4 3 case

- [ ] **Step 1: 跑 [cpu][csr-chmem]**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -3`
Expected: 5/5 PASS (A.1 5 case R3 通路全绿)

- [ ] **Step 2: 跑全量 TLM regression**

Run: `./build/bin/chipforge_tests "[mmu]"`
Expected: 59/59 PASS (R3 跨 plugin 改动验证 0 退化)

- [ ] **Step 3: 跑全量 CH_MEM regression**

Run: `./build/bin/chipforge_tests_chmem "[chmem]" "[verilator]"`
Expected: 9/9 + 1/1 (48 ELF) 不退化

---

## Phase 3 — R2 决策实施: MRET / ECALL / EBREAK PC 重定向 (估时 2.5 day, Sprint 6 Week 1-2)

### Task 3.1: R2.1 — 扩 csr_bundles.h 加 2 个新 key (TRAP_PENDING / TRAP_TARGET)

**Files:**
- Modify: `bundles/csr_bundles.h` (续 R3.1)

- [ ] **Step 1: 加 2 行 (已含在 R3.1 模板, 仅核对)**

Run: `grep -n "TRAP_PENDING\|TRAP_TARGET" bundles/csr_bundles.h`
Expected: 2 行 (如 R3.1 模板已含, 跳过本 task; 否则加)

- [ ] **Step 2: 编译验证**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -3`
Expected: 0 error

### Task 3.2: R2.2 — csr_chmem.h branch-LATE 闭包: 写 TRAP_PENDING / TRAP_TARGET

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` (新闭包, branch-LATE)

- [ ] **Step 1: 加 at_stage("branch", LATE, [...]) 闭包**

```cpp
pb->at_stage("branch", cf::plugin::Stage::LATE, [this, &payload](auto& payload) {
  using csrKT = cf::cpu::csr::payload::csr_keys<T>;
  // R2 决策: trap 优先级 trap > branch > pc+4
  const bool trap_pending = (funct12_ == 0x000) | (funct12_ == 0x001) | (funct12_ == 0x302);
  // ECALL/EBREAK → mtvec, MRET → mepc
  const auto trap_target = select(
      (funct12_ == 0x302), mepc_,
      select((funct12_ == 0x000) | (funct12_ == 0x001), mtvec_, 0u));
  // ECALL/EBREAK 时写 mcause + mepc
  mcause_ <<= select((funct12_ == 0x000), ch::core::ch_literal<11, 32>{},
              select((funct12_ == 0x001), ch::core::ch_literal<3, 32>{}, mcause_));
  mepc_   <<= select((funct12_ == 0x000) | (funct12_ == 0x001), pc_plus_4_, mepc_);
  payload[csrKT::TRAP_PENDING] = trap_pending;
  payload[csrKT::TRAP_TARGET]  = trap_target;
});
```

- [ ] **Step 2: 编译 + 跑 A.1 Case 3-5 仍 FAIL (预期, 缺 IBus NORMAL 消费)**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -5`
Expected: 编译 0 error, Case 3-5 FAIL (无 R2.3)

### Task 3.3: R2.3 — ibus_chmem.h branch-NORMAL 闭包: select 覆写 PC 节点

**Files:**
- Modify: `ip/cpu/plugins/ibus_chmem.h` (续 R1.1 改 1 行之后, 第二次改)

- [ ] **Step 1: 读 ibus_chmem.h branch 段 (现 R1.1 改 1 行后)**

Run: `sed -n '146,170p' ip/cpu/plugins/ibus_chmem.h`
Expected: 看到 pc_mux / next_pc 写出 (R1.1 改 INSTRUCTION 之后)

- [ ] **Step 2: 加 4 行 R2 select 树**

```cpp
// R2 决策: trap > branch > pc+4 (R2 优先级固化, 与 wave5-bp-btb 后续协调)
const auto next_pc = select(
    csr_keys<T>::TRAP_PENDING, csr_keys<T>::TRAP_TARGET,
    select(br_taken, BRANCH_TARGET, pc_plus_4));
payload[cpu_keys<T>::NEXT_PC] = next_pc;  // 或现 PC 节点名
```

- [ ] **Step 3: 编译 + 跑 A.1 Case 3-5 真 PASS**

Run: `cmake --build build --target chipforge_tests_chmem 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -5`
Expected: Case 1-5 全部 PASS (R2 决策落地)

- [ ] **Step 4: 跑 [verilator] baseline (R2.3 改 IBus, 须 0 退化)**

Run: `./build/bin/chipforge_tests_chmem "[verilator]"`
Expected: 1/1 (48 ELF) 0 退化 (rv32ui-p-* 不触发 ECALL/MRET)

- [ ] **Step 5: 协调 mmu C.2**

**必读**: mmu-chmem-pipeline-integration C.2 (ibus_chmem.h bare byte-identical) 与本 task 改同一文件。**mmu C.2 merge 之前不可合本 task**。建议顺序:
1. 等 mmu C.2 PR merge (estimated Sprint 6 Week 1)
2. 本 task rebase on mmu C.2 后
3. 跑 `[mmu]` 验证 59/59 (R2.3 select 树 0 退化)
4. 开本 task PR

### Task 3.4: R2.4 — R2 单元验证 (Case 9-10: trap 优先级 + MRET PC=mepc)

**Files:**
- Modify: `tests/cpu/test_csr_chmem.cpp` (续 R1.4 加 2 case)

- [ ] **Step 1: 追加 Case 9-10 ~50 LOC**

```cpp
TEST_CASE("TrapPriorityOverBranch", "[cpu][csr-chmem]") {
  // 构造 pipeline state: 同时 mret + beq 触发 (通过手动 mock, 不是真并发)
  // 验证 next_pc = mepc (R2 优先级)
}

TEST_CASE("MretPCEqualsMepc", "[cpu][csr-chmem]") {
  // 序列: ECALL → MRET, 验证 MRET 后 PC = mepc = ECALL PC + 4
}
```

- [ ] **Step 2: 跑 R2.4 2 case**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]" 2>&1 | tail -3`
Expected: 10/10 PASS (5+3+2)

### Task 3.5: R2.5 — mcycle 自由计数详化 (Q1 收口)

**Files:**
- Modify: `ip/cpu/arch/riscv/csr_chmem.h` (新闭包, fetch-ALWAYS)

- [ ] **Step 1: 加 at_stage("fetch", ALWAYS, [...]) 闭包**

```cpp
pb->at_stage("fetch", cf::plugin::Stage::ALWAYS, [this](auto& payload) {
  mcycle_ <<= mcycle_ + ch::core::ch_literal<1, 32>{};  // carry 自增
});
```

- [ ] **Step 2: 跑 100 cycle 测 mcycle 值**

Run: 在 test_csr_chmem.cpp Case 11 加 100 cycle + 读 mcycle
Expected: mcycle ≈ 100 (允许 ±1 延迟)

- [ ] **Step 3: 跑 [cpu][csr-chmem] 11/11**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: 11/11 PASS

---

## Phase 4 — B + C 集成 + e2e 验证 (估时 2 day, Sprint 6 Week 2)

### Task 4.1: B.1-B.3 — cpu_factory_chmem.h 7→8 plugin 集成

**Files:**
- Modify: `ip/cpu/cpu_factory_chmem.h:50-58` (7 plugin → 8 plugin)

- [ ] **Step 1: 改 factory 注册**

在 RegFile (位置 4) 之后, IntAlu (位置 6) 之前, 加:
```cpp
pb->register_plugin(std::make_unique<RiscvCsrPluginChmem<T>>());
```

- [ ] **Step 2: 注释更新 "7-plugin 流水线" → "8-plugin 流水线"**

- [ ] **Step 3: 编译 + 跑 [chmem] [verilator] [cpu] baseline**

Run: `cmake --build build 2>&1 | tail -3 && ./build/bin/chipforge_tests_chmem`
Expected: 0 error, 全部 baseline 不退化 (R7 移路径可在 Phase 4 末做)

### Task 4.2: R7 — csr_chmem.h 路径修正 (Sprint 5 第 1 周完成, 早合)

**Files:**
- Move: `ip/cpu/arch/riscv/csr_chmem.h` → `ip/cpu/plugins/csr_chmem.h`

- [ ] **Step 1: git mv + 修 include 路径**

```bash
git mv ip/cpu/arch/riscv/csr_chmem.h ip/cpu/plugins/csr_chmem.h
# 修 cpu_factory_chmem.h include
```

- [ ] **Step 2: 跑 portability Check 2 验证**

Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -5`
Expected: 12/12 PASS (R7 落, gate 不再假阴性)

### Task 4.3: C.1 — 新建 zicsr_basic.S + zicsr_basic.elf + 7/8-stage e2e 测试

**Files:**
- Create: `tests/cpu/manual_elf/zicsr_basic.S` (~30 LOC)
- Modify: `tests/cpu/manual_elf/build_manual_elf.sh`
- Create: `tests/cpu/integration/test_7stage_zicsr_e2e.cpp` (~120 LOC)

- [ ] **Step 1: 写 zicsr_basic.S**

```asm
.section .text
.globl _start
_start:
    csrrs x1, mstatus, x0    # 读 mstatus 到 x1
    csrrw x2, mstatus, x1    # 写 mstatus = x1
    la   x3, tohost          # tohost 地址
    sw   x2, 0(x3)           # 写 tohost = 1
1:  j 1b
.section .tohost
.align 4
tohost: .word 0
```

- [ ] **Step 2: build_manual_elf.sh 加 zicsr_basic.S 编译规则**

仿 add 现有规则, 加:
```bash
riscv64-unknown-elf-gcc -march=rv32imc -mabi=ilp32 -nostdlib -T link.ld -o zicsr_basic.elf zicsr_basic.S
```

- [ ] **Step 3: 写 test_7stage_zicsr_e2e.cpp (2 case: 7-stage + 8-stage)**

```cpp
TEST_CASE("zicsr_basic_7stage_tohost1", "[cpu-integration][zicsr]") {
  // exec ./build/src/cf_plugin/cpu_sim --elf zicsr_basic.elf --cycles 5000
  // 期望 tohost=1
}

TEST_CASE("zicsr_basic_8stage_tohost1", "[cpu-integration][zicsr]") {
  // 同上, 8-stage config
}
```

- [ ] **Step 4: 跑 [cpu-integration] 89/93 baseline + 2/2 新**

Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -3`
Expected: 91/93 PASS (4 已知 cpu_sim FAIL 不动)

### Task 4.4: C.2 — 全量回归

- [ ] **Step 1: 跑 chipforge_tests TLM**

Run: `./build/bin/chipforge_tests 2>&1 | tail -5`
Expected: `[mmu] 59/59 + [cpu] 128/129 + [cpu-integration] 91/93` (含 zicsr 2 新)

- [ ] **Step 2: 跑 chipforge_tests_chmem**

Run: `./build/bin/chipforge_tests_chmem 2>&1 | tail -5`
Expected: `[chmem] 9/9 + [cpphdl] 6/6 + [verilator] 1/1 (48 ELF) + [mmu-verilator] 3/3 + [cpu][csr-chmem] 11/11`

- [ ] **Step 3: 跑 run_chipforge_tests.sh --all**

Run: `bash tools/run_chipforge_tests.sh --all 2>&1 | tail -10`
Expected: 全 PASS (~167s)

- [ ] **Step 4: 跑架构门禁**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh`
Expected: 0 fail / 8/8 / 12/12

---

## Phase 5 — D + E Archive 准备 (估时 0.5 day, Sprint 7)

### Task 5.1: D — 架构门禁 (tasks.md Phase D 全部 checkbox)

- [ ] 跑 4 个 gate 脚本, 全部 0 失败
- [ ] 历史失败项 (如有) 记录但不阻塞 archive

### Task 5.2: E.1 — CHANGELOG.md 同步

- [ ] 在 `CHANGELOG.md` v0.11.0 段加 entry
- [ ] 标注 R4 (if(ch_bool) 陷阱已用 select 树规避) + R7 (gate 假阴性已修)

### Task 5.3: E.2 — AGENTS.md §CH_MEM 模式 7→8 plugin 文字修订

- [ ] 改 7→8 plugin 描述
- [ ] 改 `ip/cpu/STATUS.md` 移除 "🔴 RISC-V CSR P2 占位" 状态

### Task 5.4: E.3-E.4 — Archive 触发

- [ ] 跑 `openspec change validate csr-chmem-pipeline-integration` 0 error
- [ ] 跑 `openspec archive csr-chmem-pipeline-integration -y`
- [ ] 验证目录移到 `openspec/changes/archive/2026-11-XX-csr-chmem-pipeline-integration/`
- [ ] 跑 `bash .rddf/sync_strategy_status.sh` 更新 strategy.md §7

---

## 总任务数

47 task (5 Phase × ~10 task) + 47 sub-step (~5 step / task) = 235 sub-step。

## 估时

- Phase 0: 0.5 day
- Phase 1 (R1): 1.5 day
- Phase 2 (R3): 3 day
- Phase 3 (R2): 2.5 day (含等 mmu C.2 merge 时间)
- Phase 4 (B+C): 2 day
- Phase 5 (D+E): 0.5 day
- **总: 10 day = 2 周** (per Oracle/proposal 估时, R5 [Medium] ch_state_machine 生产率风险保留乐观)

## 已知风险 (per design.md §7)

- R4 [High] if(ch_bool) 陷阱: A.3 旧 `if (addr == 0x300)` 全删, 改 R3.4 select 树; 强制 CI grep 兜底
- R5 [Medium] ch_state_machine 生产率风险: 开工前 owner 读 `mmu_ptw_chmem.h` 全文, 抄 2 状态 FSM 模式
- R7 [Low] gate 假阴性: R7 移路径走 portability Check 2 强制检查 (Phase 4.2 完成)

## 关键依赖与阻塞

- **mmu-chmem-pipeline-integration C.2**: R2.3 改同一 ibus_chmem.h 文件, **mmu C.2 merge 之前不可合 R2.3** (Task 3.3 Step 5)
- **mmu-chmem-pipeline-integration Phase B mmu_chmem.h**: csr R3.3 写 mcause/mepc 依赖真 sv32 fault path, Phase B defer 2026-12-01, 但 A.1 Case 3 (ECALL) 不依赖 mmu (ECALL mcause=11 由 funct12 直接赋值, 不走 mmu path), **不阻塞本 change 主体**
- **`wave5-bp-btb` change 目录不存在** (幽灵依赖): R2.3 trap > bp 优先级显式声明, 后续 bp-btb 启动时协调
- **ch_state_machine DSL (mmu_ptw_chmem 已有先例)**: ✅ available
- **chipforge_tests_chmem + Verilator 后端**: ✅ ready
