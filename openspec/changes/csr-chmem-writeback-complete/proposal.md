---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on:
  - csr-chmem-pipeline-integration  # ✅ archived 2026-10-15 (R1+R2 部分 + R3 + B.1 + C.1 scope-partial), R3.5 writeback 完整版留本 change
oracle_date: 2026-10-15
---

# csr-chmem-writeback-complete — R3.5 writeback 完整版 (v0.11.0)

> **Scope**: 本 change 关闭 `csr-chmem-pipeline-integration` archive 时 scope-partial deferred 的 R3.5 段——8 个 M-mode CSR (mstatus/mtvec/mepc/mcause/mie/mip/mscratch/mcycle) 中前 7 个**实际可写** (R3.5 完整版), mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数 维持)。使 `[cpu][csr-chmem]` 14/14 → 16/16 PASS, 0 退化。

## Why

`csr-chmem-pipeline-integration` (commit `c150b2e`, archived 2026-10-15) 实装 R1+R2(部分)+R3+B.1+C.1(scope-partial), **R3.5 writeback 完整版** 显式 deferred:

- R3.3 写读通路 ✅ (execute-LATE 闭包写 CSR_READ_VALUE/CSR_READ_VALID)
- R3.4 writeback ch_reg 写代码 ✅ (8 个 `ch_reg <<= select(...)` 写) — **但** writeback-NORMAL 闭包 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) return;` 兜底
- R3.5 完整版 ❌ (execute-LATE 闭包**未**写 CSR_WRITE_ADDR/VALUE/EN payload, writeback 闭包**未**删 has() guard)

**当前现状**:
- 7 个 M-mode CSR (mstatus/mtvec/mepc/mcause/mie/mip/mscratch) 的 writeback 通路**代码就位但被 has() guard 阻止**
- 用户跑 `csrrw mstatus, x1` → 8 选 1 读 mux 输出 mstatus 旧值 (R3.3 ✅), **但 mstatus 不会被新值覆盖** (R3.5 ❌)
- 写 enable / write addr / write value payload keys **存在定义** (`bundles/csr_bundles.h:45-47`) **但从未被写出**

**owner 真空**:
- `csr-chmem-pipeline-integration` 已 archive, R3.5 已显式 deferred 标"无外部依赖, 后续 session 完成"
- 任何 session 启动 csr-chmem R3.5 完整版**不需要** mmu C.2 / cpu-pipeline-tick-rca fix, 可立即推进

## What Changes

### 1. execute-LATE 闭包追加写 CSR_WRITE_ADDR / CSR_WRITE_VALUE / CSR_WRITE_EN payload

**Modify**: `ip/cpu/plugins/csr_chmem.h` execute-LATE 闭包 (line 168-180, 在 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 之后)

新增 3 个 `n->payloads().put`:
- `CSR_WRITE_ADDR` = `last_csr_addr_` (ch_uint<32> DAG signal, 已有)
- `CSR_WRITE_VALUE` = `n->operator()(KeyType::RS1)` (CSRRW 写 rs1, CSRRS/CSRRC 写 mask; 完整 R3 通路覆盖 RS1 写入)
- `CSR_WRITE_EN` = `zicsr_valid && !csr_addr_eq_B00` (R3 决策: mcycle 永不被 CSRR* 写)

`CSR_WRITE_EN` 1-bit 用 ch_uint<1> select 树实现 (AGENTS.md §架构约束 #4: 无 if 早返; #8: 禁 if(ch_bool)):
```cpp
const bool write_en_bool = static_cast<bool>(
    select(zicsr_valid_ch,
           select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                       ch_uint<1>(ch_literal<1, 1>{})),
           ch_uint<1>(ch_literal<0, 1>{})));
n->payloads().put(CsrKT::CSR_WRITE_EN, write_en_bool);
```

### 2. writeback-NORMAL 闭包删 has() guard 兜底, 让 8 个 ch_reg 实际写

**Modify**: `ip/cpu/plugins/csr_chmem.h` line 209 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {` 删除, **同时**删除对应 `}` 闭括号 (line 228)

**保留**: `if (n) {` node guard (无 writeback node 时 no-op 零退化)
**保留**: 8 个 `mstatus_ <<= select(we_X, write_value, mstatus_);` 写代码 (line 219-225, 已有)
**保留**: `mcycle_` 不写注释 (R3 决策 + R2.5 自由计数, write_en 永 false 因 csr_addr_eq_B00)
**新增**: `last_csr_write_en_` ch_uint<1> 观察口 (R3.5 单测用, 单 cycle 验证写 enable 信号)

### 3. `[cpu][csr-chmem]` 单测追加 2 case (14 → 16)

**Modify**: `tests/cpu/test_csr_chmem.cpp` 末尾追加

- **Case 15 (`csr_chmem_R3_5_CsrrwWriteEnableTrue`)**: CSRRW x1, mstatus, x0 (rs1=0xCAFEBABE) → 灌 execute EARLY → 1 cycle tick → 验证 `last_csr_write_en_ch() == 1` + `last_csr_write_value_ch() == 0xCAFEBABE` (单 cycle 观察口路径, 避免 2 cycle 跨 stage 复杂度)
- **Case 16 (`csr_chmem_R3_5_McycleWriteEnableFalse`)**: CSRRS mcycle (csr_addr=0xB00) → 验证 `last_csr_write_en_ch() == 0` (R3 决策 mcycle 永不被 CSRR* 写)

**关键设计**: 走"单 cycle 观察口"路径 (plan §Task 1.3 Step 3), 不依赖 2 cycle 跨 stage 端到端 (后者属 C.1 集成 e2e 范围, 等 cpu-pipeline-tick-rca fix)

### 4. CHANGELOG.md v0.11.0 加 R3.5 entry

**Modify**: `CHANGELOG.md` 在 `v0.11.0 (interim) — csr-chmem-pipeline-integration: ...` 段后加新段

新段 `## v0.11.0 (interim) — csr-chmem-writeback-complete: ...`, 描述:
- Added: writeback-NORMAL 闭包实际写 7 个 CSR ch_reg (R3.5 完整版)
- Modified: writeback-NORMAL 删 has() guard (R3.4 兜底取消)
- Added: `[cpu][csr-chmem]` 14/14 → 16/16 PASS (~118 assertions)
- Verification: 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
- Scope-partial 残留: R2.3 (等 mmu C.2) / C.1 集成 e2e (等 cpu-pipeline-tick-rca) — 与 csr-chmem-pipeline-integration archive 同源

## Risk

| 维度 | 评估 |
|---|---|
| 复杂度 | **低** — execute-LATE 加 3 行 payload 写 + writeback 删 1 行 guard, 总 LOC 改动 < 10 行 |
| CppHDL 探索 | **低** — 基线 R3.4 commit `6cf3149` 8 个 ch_reg `<<=` 已 elab pass, 删 has() guard 不改 8 个 <<= 代码 |
| 5-stage baseline regression | **中** — writeback 闭包实际写 ch_reg 后, 任何 mstatus_ 引用都会被新值覆盖. plan §Task 1.2 Step 4 守门 m3_poc_5stage_byte_equal 185/185 |
| portability Check 5 (禁 if(ch_bool)) | **低** — write_en 走 select 树 + `static_cast<bool>`, 严守 AGENTS.md §架构约束 #8 |
| 依赖 | **无外部依赖** — 不需要 mmu C.2 / cpu-pipeline-tick-rca fix, 可立即推进 |

## Out-of-Scope (确认, 留给后续 change / plan)

- **R2.3** (`ibus_chmem.h` branch-NORMAL select 覆写 PC 节点) — 强阻塞 mmu-chmem C.2 merge (3-6 月), 独立 change
- **C.1 集成 e2e** (zicsr_basic 7/8-stage tohost=1 跑 cpu_sim C++ API) — 强阻塞 cpu-pipeline-tick-rca fix, 独立 change
- **R2.5 mcycle 100 cycle 测试** (1 cycle tick Case 14 已有, 100 cycle 扩展) — 不在 R3.5 范围, 留作 R2.5 完整 plan
- **`wave5-bp-btb` 优先级协调** — R2.3 启动时协商 trap > bp 优先级
- **64-bit CSR / PMP / Smode CSR / FPU CSR** — Phase 5+ 跟踪, 与本 change 正交
- **9 270+ CSR 全表** — D1 收窄 8 个, proposal §Out-of-Scope 已声明

## Verification (HEAD `c150b2e` 基线)

- **基线数字 (本 change 起点)**: `[cpu][csr-chmem] 14/14 PASS (105 assertions)` + `[cpphdl] 6/6` + `m3_poc_5stage_byte_equal 185/185` + `[chmem] 37/38 (1 known cpphdl_poc_chbool_contextual_conversion flake)` + `[mmu] 59/59` + 架构 gate `12/12 + 8/8` 全 PASS
- **目标数字 (本 change 终点)**: `[cpu][csr-chmem] 16/16 PASS (~118 assertions)` + 其余基线全部维持 + 架构 gate `12/12 + 8/8` 全 PASS
- **回归守门**: `m3_poc_5stage_byte_equal` 185/185 (R3.5 写通路任何回归必触发此测试失败)
