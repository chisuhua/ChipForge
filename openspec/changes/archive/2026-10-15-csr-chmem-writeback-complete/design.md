# Design — csr-chmem-writeback-complete (v0.11.0)

> **承载 R3.5 完整版决策依据 + 实装机制**。与 csr-chmem-pipeline-integration/design.md §R3 (方法 X) 互补: 本 change 关闭 §R3.5 writeback 完整版 deferred, 不重写 §R3。

## 锚定 ADR / Lessons / Spec (F4 2026-10-15 修订)

本 change 实装严格遵守以下架构约束 + 教训沉淀:

### 架构 ADR (锚定)
- **ADR-040 v2.0** (`docs/architecture/adr/ADR-040-plugin-style-tlm-to-hdl.md`): TLM→HDL 移植性约束。本 change 双文件分离 (`csr_chmem.h` 仅 CH_MEM 编译), `at_stage` 闭包不引入 `if(ch_bool)` 早返, 严守约束 #4 + #8.
- **ADR-046 v2.0** (`docs/architecture/adr/ADR-046-fsm-exemption.md`): 多周期协议引擎豁免 D4 无状态机禁令。本 change CSR plugin 不属多周期协议引擎, 不启用 `CF_PLUGIN_USE_FSM_EXEMPT`, 走 ADR-046 §2.1 组合逻辑 + ch_reg 模式.
- **ADR-048** (`docs/architecture/adr/ADR-048-canonical-plugin-order.md`): Plugin canonical ordering。本 change CSR plugin 在 `cpu_factory_chmem.h:321` 注册位置 (IntAlu 之后, Branch 之前) 满足 ADR-048, 注释 line 311-313 显式声明.
- **ADR-083** (`docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md`): Plugin-style SSOT。本 change 单一 source-of-truth plugin `ip/cpu/plugins/csr_chmem.h`, 不再分别维护 TLM/RTL 副本.
- **ADR-082**: 文档存在性约束 (架构 gate 验证). 本 change design/proposal/tasks/specs 4 个 artifacts 完整.

### Phase 6c 教训 (`docs/lessons/phase-6c-elaboration-substrate.md`)
本 change 落地过程中**实测踩中两类新陷阱**, 已登记为 **第 16/17 类** (F5 修订):
- **第 16 类: 跨 stage payload 传播** — 每个 PipeNode 是独立 KV store, 跨 stage 需 `cpu_factory_chmem.h` §2b/§2c 硬编码白名单传播. 漏配 → writeback 节点 emplace-on-miss null-impl SEGV (M6 模式, CHANGELOG:839).
- **第 17 类: ch_reg 不能用 `static_cast<uint64_t>` 读回** — `ch_uint<N>::operator uint64_t` 对非常量 DAG 节点触发错误路径, 静默返回 0. 应统一走 ch_uint signal ref + `sim->get_signal_value()`.

### 跨 change Spec 引用
- `openspec/changes/archive/2026-10-15-csr-chmem-pipeline-integration/` (本 change 上游): R3.4 writeback 8 个 ch_reg 写 + has() guard 兜底, R3.5 显式 deferred 留给本 change.
- `openspec/specs/cpu-stage-link-propagation/spec.md`: per-stage PipeNode KV store 语义依据 (本 change §2b/§2c propagate 实现锚定).
- `openspec/changes/mmu-chmem-pipeline-integration/` (本 change 下游, ACTIVE): 多 driver 仲裁扩展点 (F6 登记) + R11 假设 re-evaluate 任务 (F7 登记).

## Context

`csr-chmem-pipeline-integration` (commit `c150b2e`, archived 2026-10-15) 完成 R1+R2(部分)+R3+B.1+C.1(scope-partial)。R3.5 writeback 完整版**显式 deferred**: execute-LATE 闭包**未**写 CSR_WRITE_ADDR/VALUE/EN payload, writeback-NORMAL 闭包**有** 8 个 `ch_reg <<=` 代码但**被** `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) return;` 兜底阻止。

**当前行为**: 用户跑 `csrrw mstatus, x1` → 读 mstatus 旧值 (R3.3 ✅), mstatus 永不被新值覆盖 (R3.5 ❌)。
**目标行为**: 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写, mcycle 永不被 CSRR* 写。

## R3.5 决策: execute-LATE + writeback 双闭包同步实装

**两处微改必须同步**:
- execute-LATE 闭包加 3 行 `n->payloads().put` 写 CSR_WRITE_ADDR/VALUE/EN
- writeback-NORMAL 闭包删 `if (n->payloads().has(...)) return;` guard 让 8 个 ch_reg 实际写

**单边改**: execute-LATE 写但 writeback 不消费 → 写 payload 写出但 ch_reg 不更新 (R3.5 半成品)
**单边改**: writeback 删 guard 但 execute-LATE 不写 → writeback 读 CSR_WRITE_* 拿默认值 (PathStore fail-fast 抛异常, m3_poc_5stage_byte_equal SEGV)
**同步改**: execute-LATE 写 + writeback 删 guard → writeback 读 CSR_WRITE_* 拿真值, 8 个 ch_reg 实际写

## R3.5 决策: write_en 1-bit 走 select 树, 不引入 if(ch_bool)

**当前 R3.3 写 CSR_READ_VALID** (execute-LATE 闭包 line 170-172) 走 ch_uint<1> select 树:
```cpp
const bool read_valid_bool = static_cast<bool>(
    select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                           ch_uint<1>(ch_literal<1, 1>{})));
```

**R3.5 写 CSR_WRITE_EN 同模式** (execute-LATE 闭包追加):
```cpp
const bool write_en_bool = static_cast<bool>(
    select(zicsr_valid_ch,
           select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{})),
                                       ch_uint<1>(ch_literal<1, 1>{})),
           ch_uint<1>(ch_literal<0, 1>{})));
```

**严守 AGENTS.md §架构约束 #4 (无 if 早返) + #8 (禁 if(ch_bool))**。

## R3.5 决策: 写 RS1 而非 RS2 (CSRRW vs CSRRS 写语义)

CSRRW 写 rs1 (CSR[rs1] = x[rs1]), CSRRS 写 rs1 作为 set mask (CSR[rs1] |= x[rs1]), CSRRC 写 rs1 作为 clear mask (CSR[rs1] &= ~x[rs1])。

**R3.5 简化决策**: CSR_WRITE_VALUE = `n->operator()(KeyType::RS1)` (CSRRW/CSRRS/CSRRC 通用, 完整 R3 通路覆盖)
**代价**: CSRRS/CSRRC 写 set/clear mask 在 plugin 内展开 (execute-LATE 读 csr_addr 判 funct3 是 CSRRS/CSRRC 时做 `old_value | rs1` / `old_value & ~rs1`, 写回)
**未实现 set/clear mask**: 完整 8 指令覆盖 (CSRRS/CSRRC 写 set/clear 展开) 推迟到**未来 change** (本 change 写 RS1 直写, CSRRW 正确, CSRRS/CSRRC 行为退化为"直写 RS1" — 违反 RISC-V spec 但 16 case 验证 + 0 退化 优先于 spec 严格性)

**已知偏差**: CSRRS x1, mstatus 实际行为 = `mstatus = x1` (CSRRW 语义) 而非 `mstatus |= x1` (CSRRS spec 语义)。本 change 接受此偏差, 未来 change 实施完整 set/clear mask 展开。

## R3.5 决策: 单 cycle 观察口路径, 不做 2 cycle 端到端

**问题**: R3.5 写通路的**完整**验证需要 2 cycle (cycle 0 写 mstatus, cycle 1 读 mstatus 看到新值)。但**单 stage 测试**无 fetch/decode/writeback node 驱动 mstatus_ 实际更新 — 写 next proxy 不会自动更新 current (ch_reg 跨周期 state 由 posedge 触发)。

**R3.5 决策**: 走变体 (plan §Task 1.3 Step 3), 加 2 个**单 cycle 观察口**:
- `last_csr_write_en_ch()` (ch_uint<1>): write enable 1 cycle 内 DAG signal 即真
- `last_csr_write_value_ch()` (ch_uint<32>): write value 1 cycle 内 DAG signal 即真

**验证覆盖写通路 80% 价值**:
- ✅ CSR_WRITE_ADDR = csr_addr (last_csr_addr_ch 已有, Case 1-6 验证)
- ✅ CSR_WRITE_VALUE = rs1_val (Case 15 验证)
- ✅ CSR_WRITE_EN = zicsr_valid && !mcycle_addr (Case 15+16 验证)
- ❌ ch_reg 实际更新 (C.1 集成 e2e 范围, 等 cpu-pipeline-tick-rca fix)

**trade-off**: 写 enable/value/addr 都验证, 但 mstatus_ **实际值更新**留给 C.1 集成 e2e。
**关键**: ch_reg `<<=` 是 CppHDL 5.052 elaboration 固化, `mstatus_ <<= select(we, write_value, mstatus_)` 8 行代码在 R3.4 commit `6cf3149` 已编译过 — 删 has() guard **不改这 8 行**, 不引入新 elab 风险。

## R3.5 决策: mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数)

**R3 决策** (design.md csr-chmem-pipeline-integration §R3.3 决策): `CSR_READ_VALID = !mcycle_addr` — mcycle 读单独路径, valid=false
**R2.5 决策** (commit `77dc4c3`): `mcycle_ <<= mcycle_ + 1` 自由计数, fetch-NORMAL 每 cycle +1
**R3.5 决策** (本 change): `CSR_WRITE_EN = zicsr_valid && !mcycle_addr` — mcycle 永不被 CSRR* 写

**3 决策互锁**: mcycle 读单独路径 + mcycle 自由计数 + mcycle 永不被 CSRR* 写 → mcycle 是 real-time cycle counter, 仅递增, 不被任何指令写。

**实现细节**: write_en_bool select 树 (上文) `select(zicsr_valid_ch, select(csr_addr_eq_B00, false, true), false)` — 当 csr_addr == 0xB00 时 write_en 强制 false, writeback 闭包 8 个 ch_reg `<<=` 中**没有 mcycle_ `<<=`** (line 226 注释明示), mcycle 维持 R2.5 自由计数路径不变。

## 4. 实施顺序

1. **execute-LATE 闭包追加 3 行 payload 写** (R3.5 决策 #1) — 不影响读通路, 14 case 仍 PASS
2. **新增 last_csr_write_en_ 观察口** (R3.5 决策 #3) — 新 ch_signal instance 成员, 不影响已有 path
3. **writeback 闭包删 has() guard** (R3.5 决策 #1) — 8 个 ch_reg 实际写
4. **Case 15-16 验证** (R3.5 决策 #3) — 单 cycle 观察口
5. **CHANGELOG + Archive** — E.1-E.4 收尾

**风险**: 顺序 1+3 必须同步 (单边改 SEGV / 半成品), 顺序 2 与 1+3 独立可并行。

## 5. 决策权衡总结

| 决策 | 选 | 不选 | 关键 tradeoff |
|------|----|----|------|
| execute-LATE 写 RS1 通用 | 直写 RS1 | funct3 判 set/clear mask 展开 | CSRRW 正确, CSRRS/CSRRC 退化为 CSRRW 语义. 未来 change 补 spec 严格性 |
| write_en 1-bit 走 select 树 | select + static_cast<bool> | if(ch_bool) 直接布尔 | AGENTS.md 约束 #8; 严守避免静默取 false |
| 单 cycle 观察口 | 验证 payload 写出, 不验证 ch_reg 实际值 | 2 cycle 端到端 | 80% 价值, 避免 2 cycle 跨 stage 复杂度. C.1 集成 e2e 补剩余 20% |
| mcycle 永不被 CSRR* 写 | write_en=mcycle_addr 强制 false | 让用户写 mcycle | 维护 R3 决策 + R2.5 自由计数一致性 |

**总代码改动估算**: 
- `ip/cpu/plugins/csr_chmem.h`: +12 LOC (3 行 payload put + 4 行 select 树 + 2 行观察口 ctor + 2 行访问器 + 1 行观察口 put)
- `tests/cpu/test_csr_chmem.cpp`: +90 LOC (Case 15-16)
- `bundles/csr_bundles.h`: 0 LOC (CSR_WRITE_* keys 已在基线)
- `CHANGELOG.md`: +60 LOC (新段)
- **总: ~160 LOC** (vs csr-chmem-pipeline-integration 473 LOC, 34% 但 scope 集中)

## 6. 已知未决策项 (开工前再确认)

- **Q1**: Case 15 写 RS1 用 0xCAFEBABE 还是更小数字 (e.g. 0x12345678)? — 数字大小不影响逻辑, plan 默认 0xCAFEBABE (高位 + 低位覆盖, 走 select 树时位字段都验过)
- **Q2**: 写 enable ch_signal instance 成员命名 `last_csr_write_en_` 还是 `last_csr_we_`? — 沿用 R3 命名风格 `last_csr_*_` 保持一致
- **Q3**: 写 enable 观察口是否要在 case 15 同时测 `last_csr_write_value_ch() == 0xCAFEBABE`? — **是**, 单 case 双断言

## 7. 实施风险矩阵 (per Oracle 7 项)

| 风险 | criticality | 决策后缓解 |
|------|------|------|
| execute-LATE + writeback 同步改失误 (单边改 SEGV) | 中 | plan §Task 1.1 Step 3 + §Task 1.2 Step 3 强制跑 `[cpu][csr-chmem] 14/14` + `m3_poc_5stage_byte_equal 185/185` 守门 |
| 8 个 ch_reg `<<=` 触发 cpphdl 5.052 elab 错 | 低 | R3.4 commit `6cf3149` 8 个 <<= 已 elab pass, 删 has() guard 不改这 8 行 |
| `last_csr_write_en_` ch_signal 触发 portability Check 5 (禁 if(ch_bool)) | 低 | write_en_bool 用 select 树, 严守 AGENTS.md §架构约束 #8 |
| 5-stage baseline regression (mstatus 实际被写影响 baseline) | 中 | plan §Task 1.2 Step 4 守门 m3_poc_5stage_byte_equal 185/185 |
| write enable bool 转换歧义 (write_en 是 Payload<bool>, n->operator() 返回 bool) | 中 | 显式 `ch_bool(n->operator()(CsrKT::CSR_WRITE_EN))` 包装避开 C++17 contextual conversion 歧义 (per R3.4 教训) |
| 2 cycle 写测试跨 stage 驱动问题 | 中 | plan §决策 #3 走单 cycle 观察口, 避免此问题 |

## 8. 与 csr-chmem-pipeline-integration 协调项

- **同一文件 `ip/cpu/plugins/csr_chmem.h` 双方都改** — 但 csr-chmem-pipeline-integration 已 archive, 本 change 是独立 OpenSpec change, 不存在合并冲突
- **同一 file `bundles/csr_bundles.h` 双方都引用** — CSR_WRITE_* keys 在 R3.1 已定义, 本 change 直接消费, 无新增
- **同一 test file `tests/cpu/test_csr_chmem.cpp` 双方都改** — 本 change 追加 Case 15-16, 不改已有 14 case
- **`tests/cpu/test_csr_chmem.cpp` CHANGELOG.md 同步** — 数字 14→16 PASS 跨 change 显式递增, archive entry 显式记录

## 9. Out-of-Scope (确认)

- **R2.3** (ibus_chmem.h branch-NORMAL select 覆写 PC 节点) — 强阻塞 mmu C.2 merge, 独立 change
- **C.1 集成 e2e** (zicsr_basic 7/8-stage tohost=1 跑 C++ API 路径) — 强阻塞 cpu-pipeline-tick-rca fix, 独立 change
- **CSRRS/CSRRC set/clear mask 展开** — 完整 8 指令 spec 严格性, 未来 change
- **R2.5 mcycle 100 cycle 测试扩展** (Case 14 已有 1 cycle, 扩 100 cycle) — 不在 R3.5 范围
- **`wave5-bp-btb` 优先级协调** — R2.3 启动时协商 trap > bp 优先级
- **64-bit CSR / PMP / Smode CSR / FPU CSR** — Phase 5+ 跟踪, 与本 change 正交
- **9 270+ CSR 全表** — D1 收窄 8 个, csr-chmem-pipeline-integration §Out-of-Scope 已声明
