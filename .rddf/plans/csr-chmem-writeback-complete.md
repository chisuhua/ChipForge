# csr-chmem-writeback-complete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **⚠️ Oracle 2026-10-15 修订**: 本 plan 经 Oracle/Metis 复审发现 1 critical + 3 high + 2 medium + 2 low 共 8 项缺陷。**当前版本已修订**，新增 Task 0.2 (多 driver smoke) + Task 1.0 (propagate) + Task 1.1 (4 层 funct3/rs1/imm5 门控 + 立即数变体走 uimm)。详见 tasks.md Phase 0 + Phase 1 修订段。

**Goal:** 关闭 `csr-chmem-pipeline-integration` 留下的 R3.5 scope-partial deferred, 实装 CSR 寄存器**实际写通路**——使 `[cpu][csr-chmem]` 14/14 → 16/16 PASS, 8 个 M-mode CSR (mstatus/mtvec/mepc/mcause/mie/mip/mscratch/mcycle) 中前 7 个可被 CSRRW/CSRRS/CSRRC 实际写, mcycle 维持 read-only (R3 决策 + R2.5 自由计数), 0 退化。

**Architecture:** 单一 source-of-truth plugin `ip/cpu/plugins/csr_chmem.h` (基线 HEAD `8b126db`) + 联动改动 `ip/cpu/cpu_factory_chmem.h` (Oracle [Critical] 修复 propagate CSR_WRITE_*)。四处微改: (1) `cpu_factory_chmem.h` §2b/§2c + populate_stage 加 CSR_WRITE_* propagate (Oracle [Critical]); (2) execute-LATE 闭包追加写 `csr_keys::CSR_WRITE_ADDR/VALUE/EN` (含 4 层 funct3/rs1/imm5 门控 + 立即数变体走 uimm, Oracle [High] #1+#2); (3) writeback-NORMAL 闭包**删** `if (n->payloads().has(...)) return;` 兜底 guard, 让 7 个 `ch_reg <<=` 实际写 (mcause/mepc 多 driver 仲裁由 Task 0.2 smoke 决定, Oracle [High] #3); (4) `[cpu][csr-chmem]` Case 15-16 验证 (Oracle 修正 inst encoding)。无外部依赖, 无 mmu C.2 阻塞。

**Tech Stack:** C++23、CppHDL ch_state_machine DSL (per ADR-046 v2.0 §2.1.1 豁免)、Plugin 框架 (at_stage / payload keys / CtrlLink)、Catch2 v3.7.0。ADR-040 v2.0 (TLM→HDL 移植性) / ADR-046 (FSM 豁免) / ADR-083 (Plugin-style SSOT) 锚定。Phase 6c 教训 `docs/lessons/phase-6c-elaboration-substrate.md`（emplace-on-miss null impl、跨 stage 传播、ch_bool 静默 false 三类陷阱）适用。

---

## File Structure

### Production Code (实施改动)

| File | Responsibility | Tasks |
|---|---|---|
| `ip/cpu/plugins/csr_chmem.h` (388 LOC 基线, HEAD `8b126db`) | execute-LATE 追加 CSR_WRITE_* payload 写 (含 4 层 funct3/rs1/imm5 门控 + uimm 选择); writeback-NORMAL 删 has() guard + mcause/mepc 仲裁 (若需) | Task 1.1 + Task 1.2 |
| `ip/cpu/cpu_factory_chmem.h` (433 LOC, Oracle [Critical] 修复) | §2b/§2c + populate_stage propagate `csr_keys::CSR_WRITE_ADDR/VALUE/EN` | Task 1.0 |
| `bundles/csr_bundles.h` (~55 LOC, 9 keys 已有) | 不改 (CSR_WRITE_ADDR/VALUE/EN 已在基线) | — |

### Verification Code (新)

| File | Purpose | Tasks |
|---|---|---|
| `tests/cpu/test_csr_chmem.cpp` (14 case 基线) | 加 Case 15 (CSRRW 实际写 mstatus) + Case 16 (mcycle 写被忽略) | Task 1.3 |
| `tests/CMakeLists.txt` chipforge_tests_chmem 白名单 | 基线已含 test_csr_chmem.cpp, 不改 | — |

### Verification Commands (不动)

| File | Purpose |
|---|---|
| `tests/cpu/test_cpu_decoded_inst_migration.cpp` | `[cpu][chmem][poc][decoded-inst]` 5-stage baseline 维持 |
| `tests/cpu/test_cpu_rtl_regfile_alu.cpp` | `m3_poc_5stage_byte_equal` 5-stage baseline 维持 |
| `tests/mmu/test_mmu_plugin.cpp` | `[mmu]` 59/59 baseline 维持 |

---

## Phase 0 — 基线建立 + 多 driver smoke (估时 1-2h)

### Task 0.1: 确认起点状态

**Files:**
- Verify: `ip/cpu/plugins/csr_chmem.h` line 110-185 (execute-LATE) + line 198-235 (writeback-NORMAL) + line 256-308 (branch-LATE)
- Verify: `ip/cpu/cpu_factory_chmem.h` line 145-191 (populate_stage) + line 362-401 (§2b/§2c stage links)
- Verify: `bundles/csr_bundles.h` line 45-47 (CSR_WRITE_ADDR/VALUE/EN keys)

- [ ] **Step 1: 确认 HEAD 基线**

Run: `git rev-parse HEAD && git log --oneline -1`
Expected: `8b126db` (csr-chmem-writeback-complete OpenSpec docs commit) 或更新

- [ ] **Step 2: 确认 writeback has() guard 位置**

Run: `grep -n "if (n->payloads().has(CsrKT::CSR_WRITE_EN))" ip/cpu/plugins/csr_chmem.h`
Expected: 1 行命中 (line 209)

- [ ] **Step 3: 确认 CSR_WRITE_* keys 已存在**

Run: `grep -n "CSR_WRITE" bundles/csr_bundles.h`
Expected: 3 行 (CSR_WRITE_ADDR / CSR_WRITE_VALUE / CSR_WRITE_EN)

- [ ] **Step 4: 跑现有 baseline**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: `All tests passed (105 assertions in 14 test cases)` (基线 14/14 PASS)

- [ ] **Step 5: 跑架构门禁**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh`
Expected: `verify_adr 0 fail` + `verify_plugin_decision 8/8` + `check_plugin_portability 12/12` 全 PASS

### Task 0.2: 多 driver elaboration smoke (Oracle [High] #3 验证)

> **Oracle 2026-10-15 [High] #3**: branch-LATE 闭包 (csr_chmem.h:296-306) 写 `mcause_ <<= select(...)` 和 `mepc_ <<= select(...)`,本 change 让 writeback-NORMAL 也写 `mcause_ <<= ...` 和 `mepc_ <<= ...`。同 cycle 两个闭包对同 ch_reg 各 `<<=` 一次 — CppHDL 5.052 行为未知 (last-wins / elab error / undefined)。

- [ ] **Step 1: 创建临时 smoke 测试 (`/tmp/test_multidriver_smoke.cpp`)**

  临时 smoke 代码, Phase 0 验证用, 不入 git:
  ```bash
  cat > /tmp/test_multidriver_smoke.cpp << 'EOF'
  // Smoke: 8-plugin build_cpu + ECALL inst + CSR write inst 交替, 跑 10 cycles
  //   验证 CppHDL 5.052 对 mcause_ 双 driver 行为
  #include <ch.hpp>
  #include <core/context.h>
  // ... 完整代码模板参考 tests/cpu/test_csr_chmem.cpp Case 14 末尾
  //     + ip/cpu/cpu_factory_chmem.h::build_cpu()
  EOF
  ```

- [ ] **Step 2: 跑 smoke + 记录行为**

  Run: `g++ -std=c++23 /tmp/test_multidriver_smoke.cpp -I build/_deps/install/include -L build/_deps/install/lib -lcf_plugin -lcpphdl -o /tmp/smoke && /tmp/smoke`
  Expected: 行为二选一 (A last-wins / B elab error / C first-wins), 记录到 tasks.md "Smoke Result" 段

- [ ] **Step 3: 决策 propagate + 仲裁策略**

  根据 Step 2 结果决定 Task 1.2 Step 3 是否需要仲裁 select

---

## Phase 1 — R3.5 写通路完整版 (估时 7-8h)

### Task 1.0: cpu_factory_chmem.h §2b/§2c propagate CSR_WRITE_* payload (Oracle [Critical] 修复)

> **Oracle 2026-10-15 [Critical]**: execute 节点和 writeback 节点是两个独立 KV store, CH_MEM 流水线靠 cpu_factory_chmem.h §2b/§2c 硬编码白名单传播, CSR_WRITE_* 不在白名单。删 has() guard (csr_chmem.h:209) 后 writeback 读缺失 key → emplace-on-miss null-impl SEGV。

- [ ] **Step 1**: 读 cpu_factory_chmem.h §2b/§2c + populate_stage 实码

  Run: `sed -n '145,191p' ip/cpu/cpu_factory_chmem.h` + `sed -n '361,401p' ip/cpu/cpu_factory_chmem.h`
  Expected: populate_stage (line 145-183) + §2b execute→memory (line 361-387) + §2c memory→writeback (line 389-401)

- [ ] **Step 2**: 扩 populate_stage 灌 CSR_WRITE_* 默认值

  在 populate_stage lambda 内 (line 164-166 之后 `n->payloads().put` block 之前) 加:
  ```cpp
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  n->operator()(CsrKT::CSR_WRITE_ADDR) = T(ch::core::ch_literal<0, 32>{});
  n->operator()(CsrKT::CSR_WRITE_VALUE) = T(ch::core::ch_literal<0, 32>{});
  n->payloads().put(CsrKT::CSR_WRITE_EN, false);
  ```

- [ ] **Step 3**: 扩 §2b execute→memory 白名单加 CSR_WRITE_*

  在 §2b lambda 内 (line 373 之后, line 367 `if (exe && mem) {` 块内) 加:
  ```cpp
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  mem->operator()(CsrKT::CSR_WRITE_ADDR)  = exe->operator()(CsrKT::CSR_WRITE_ADDR);
  mem->operator()(CsrKT::CSR_WRITE_VALUE) = exe->operator()(CsrKT::CSR_WRITE_VALUE);
  mem->payloads().put(CsrKT::CSR_WRITE_EN, exe->payloads().get(CsrKT::CSR_WRITE_EN));
  ```

- [ ] **Step 4**: 扩 §2c memory→writeback 白名单加 CSR_WRITE_*

  在 §2c lambda 内 (line 399 之后, line 395 `if (mem_st && wb) {` 块内) 加:
  ```cpp
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  wb->operator()(CsrKT::CSR_WRITE_ADDR)  = mem_st->operator()(CsrKT::CSR_WRITE_ADDR);
  wb->operator()(CsrKT::CSR_WRITE_VALUE) = mem_st->operator()(CsrKT::CSR_WRITE_VALUE);
  wb->payloads().put(CsrKT::CSR_WRITE_EN, mem_st->payloads().get(CsrKT::CSR_WRITE_EN));
  ```

- [ ] **Step 5**: 编译 + 跑现有 baseline 验证 propagate 不破 5-stage

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: 0 error + 185/185 PASS

- [ ] **Step 6**: 跑 [chmem] 全套 + TLM [mmu] 0 退化

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "[chmem]"
  /workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
  ```
  Expected: `[chmem] 37/38` + `[mmu] 59/59` 全 PASS

### Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN (含 4 层 funct3/rs1/imm5 门控 + uimm 选择)

> **Oracle 2026-10-15 [High] #1+#2**: write_en 不看 funct3/rs1 破坏 csrr 读惯例 + ECALL/EBREAK/MRET 判定; 立即数变体写 RS1 而非 uimm 静默错误。

- [ ] **Step 1**: 读 execute-LATE 闭包 + grep last_rs1_idx_ 存在性

  Run: `sed -n '110,185p' ip/cpu/plugins/csr_chmem.h && grep -n "last_rs1_idx\|rs1_idx" ip/cpu/plugins/csr_chmem.h | head -10`
  Expected: 看到 line 84 `last_funct3_ch()` getter, line 133 `last_funct3_ = funct3_ch;` + line 126-127 `bits<>` 重提取模式
  **若** grep 0 命中 → 需加 `last_rs1_idx_` instance + getter (Step 3)

- [ ] **Step 2**: 加 4 层 select 树门控 + CSR_WRITE_* payload 写出

  在 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 之后 (line 177 之后), `last_csr_read_value_ = read_value;` 之前 (line 180 之前), 加完整 4 层 select 树 + write_value_ch uimm/RS1 选择。

  完整代码见 tasks.md Task 1.1 Step 2 (避免 plan 文件过长)。

- [ ] **Step 3**: 加 last_rs1_idx_ instance 成员 + 公共 getter (Step 1 grep 缺失时)

  详见 tasks.md Task 1.1 Step 3。

- [ ] **Step 4**: 编译 + 跑现有 csr-chmem 14 case 仍 PASS

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: 0 error + 14/14 PASS (105 assertions) — R3.5 加 write payload + 4 层门控不应影响读通路

- [ ] **Step 5**: 跑 5-stage baseline 仍 0 退化

  Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: 185/185 PASS — 关键 regression 守门

### Task 1.2: writeback-NORMAL 闭包删 has() guard, 让 ch_reg 实际写 (含 Task 0.2 仲裁决策)

**Files:**
- Modify: `ip/cpu/plugins/csr_chmem.h` line 209 (`if (n->payloads().has(CsrKT::CSR_WRITE_EN)) return;`)

- [ ] **Step 1: 读 writeback 闭包完整段**

Run: `sed -n '198,235p' ip/cpu/plugins/csr_chmem.h`
Expected: 看到 `if (n) { ... if (n->payloads().has(CsrKT::CSR_WRITE_EN)) { ... } ... }` 嵌套

- [ ] **Step 2: 删 has() guard 兜底**

删除 line 209 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {` 这一行, **同时**删除对应 line 233 的 `}` (整个 if 块的闭括号), 让 7 个 `mstatus_ <<= select(...)` 等语句**直接在** `if (n) {` 块内执行。**mcause/mepc 写先注释掉**，等 Task 1.2 Step 3 仲裁决策后补。

- [ ] **Step 3: 应用 Task 0.2 仲裁决策**

根据 Task 0.2 smoke 结果:
- **A last-wins** (writeback 优先级高于 branch) → mcause/mepc 写直接走 writeback，但破坏 R2.4 trap 语义。**需要**仲裁: branch-LATE trap 写优先。
- **B elab error** → 必须加仲裁 select。
- **C first-wins** (branch 优先级) → 维持现状, writeback 写 mcause/mepc 不必要（branch 已覆盖）。

默认实现：加仲裁 select（branch-LATE 优先于 writeback-NORMAL），代码见 tasks.md Task 1.2 Step 3。

- [ ] **Step 4: 编译 + 跑现有 csr-chmem 14 case**

Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: 0 error + 14/14 PASS

- [ ] **Step 5: 跑 5-stage baseline + [chmem] 全套 + TLM [mmu] 0 退化**

Run:
```bash
./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
./build/bin/chipforge_tests_chmem "[chmem]"
/workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
```
Expected: `m3_poc_5stage_byte_equal 185/185` + `[chmem] 37/38` + `[mmu] 59/59` 全 PASS

- [ ] **Step 6: 跑 [verilator] [mmu-verilator] bare path byte-identical (Oracle [Medium] 补守门)**

Run:
```bash
./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"
```
Expected: `[verilator] 1/1 case PASS (48 ELF, 96 assertions)` + `[mmu-verilator] 3/3 case PASS`

- [ ] **Step 7: 跑架构门禁**

Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
Expected: 12/12 + 8/8 全 PASS

### Task 1.3: R3.5 单元验证 — 2 新 case (CSRRW 写 mstatus + mcycle read-only, 修正 inst encoding)

**Files:**
- Modify: `tests/cpu/test_csr_chmem.cpp` 末尾 (现有 14 case 后追加 Case 15-16)

- [ ] **Step 1: 读现有 Case 14 末尾模板**

Run: `tail -30 tests/cpu/test_csr_chmem.cpp`
Expected: 看到 `TEST_CASE("csr_chmem_R2_McycleFreeRunning_AfterOneTick", "[cpu][csr-chmem]") { ... }` 闭花括号

- [ ] **Step 2: 加 2 个 public 访问器 (instance 成员 + getter) 到 `ip/cpu/plugins/csr_chmem.h`**

在 public 段 (line 86-87 附近, R3.5 观察口已有) 加:
```cpp
ch_uint<32> last_csr_write_value_ch() const { return last_csr_write_value_; }
ch_uint<1>  last_csr_write_en_ch()    const { return last_csr_write_en_; }
```
在 private 段 (line 361-362 附近, R3.5 读观察口已有) 加:
```cpp
ch_uint<32> last_csr_write_value_{ch::core::ch_literal<0, 32>{}};
ch_uint<1>  last_csr_write_en_   {ch::core::ch_literal<0, 1>{}};
```
在 execute-LATE 闭包末尾 (Task 1.1 Step 2 写 CSR_WRITE_EN put 之后) 加:
```cpp
last_csr_write_value_ = write_value_ch;
last_csr_write_en_    = write_en_ch;
```

- [ ] **Step 3: 加 Case 15 (CSRRW 写 mstatus, 单 cycle 验证 write enable + value)**

**Inst encoding (Oracle 2026-10-15 修正)**: CSRRW x1, mstatus, x10 → `0x300510F3`
- bits[31:20]=0x300 (mstatus) | bits[19:15]=01010 (x10) | bits[14:12]=001 (CSRRW) | bits[11:7]=00001 (x1) | bits[6:0]=0x73
- = 0x30000000 | 0x00050000 | 0x00001000 | 0x00000080 | 0x00000073 = **0x300510F3** ✓

完整代码见 tasks.md Task 1.3 Step 3。

- [ ] **Step 4: 加 Case 16 (mcycle 永不被 CSRR* 写)**

**Inst encoding (Oracle 2026-10-15 修正)**: 原 plan `0xB0005073` 实为 `CSRRSI x0, mcycle, 0`（funct3=101, rd=x0，**不是**声称的 `csrrs x1, mcycle, x10`）。修正为 **`0xB00520F3`**:
- bits[31:20]=0xB00 (mcycle) | bits[19:15]=01010 (x10) | bits[14:12]=010 (CSRRS) | bits[11:7]=00001 (x1) | bits[6:0]=0x73
- = 0xB0000000 | 0x00050000 | 0x00002000 | 0x00000080 | 0x00000073 = **0xB00520F3** ✓

完整代码见 tasks.md Task 1.3 Step 4。

- [ ] **Step 5: 跑 [cpu][csr-chmem] 期望 16/16 PASS (新增 2 case)**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: `All tests passed (~118 assertions in 16 test cases)`

- [ ] **Step 6: 跑全套 regression 0 退化 (含 [verilator] 守门)**

Run:
```bash
./build/bin/chipforge_tests_chmem "[cpphdl]"
./build/bin/chipforge_tests_chmem "[chmem]"
./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"
/workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
```
Expected: 全 PASS, 含 `[verilator] 1/1 (48 ELF)` + `[mmu-verilator] 3/3` 守门

- [ ] **Step 7: 跑架构门禁**

Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
Expected: 12/12 + 8/8 全 PASS

---

## Phase 2 — D + E Archive 准备 (估时 1h)

### Task 2.1: D — 架构门禁 + 文档健康

- [ ] **Step 1: 跑 4 个 gate 脚本, 0 fail**

Run:
```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
```
Expected: 前 3 个 0 fail, doc_link_check 维持 pre-existing 7 broken (与本 change 无关, 记录不阻塞)

- [ ] **Step 2: 若 doc_link_check 新增 broken → 修, 否则记录"非本 change 责任"**

若 R3.5 commit 引入新 broken link, 修后重跑. 否则 commit msg 显式记录"doc_link_check 7 pre-existing broken, 0 新增".

### Task 2.2: E.1 — CHANGELOG.md v0.11.0 段加 R3.5 entry

- [ ] **Step 1: 在 v0.11.0 csr-chmem-pipeline-integration 段后加 R3.5 entry**

新 `## v0.11.0 (interim) — csr-chmem-writeback-complete: ...` 段, 描述:
- Added: writeback-NORMAL 闭包实际写 7 个 CSR ch_reg (R3.5 完整版)
- Added: `[cpu][csr-chmem]` 14/14 → 16/16 PASS
- Modified: writeback-NORMAL 删 has() guard 兜底
- Verification: 实测 `[cpu][csr-chmem] 16/16` + `[chmem] 0 新退化` + 架构 gate 全 PASS
- Scope-partial 残留: R2.3 / C.1 集成 e2e (per csr-chmem-pipeline-integration archive)

### Task 2.3: E.2 — AGENTS.md §CH_MEM 模式 review

- [ ] **Step 1: grep AGENTS.md "7-plugin" / "7 plugin" / "CSR P2 占位"**

若 0 命中: 现状已对齐 (R7 commit `0cc93cb` 时已修订), commit msg 说明"无 7→8 文字需修订"
若命中: 按 R7 commit 历史修订

### Task 2.4: E.3-E.4 — OpenSpec change + archive

- [ ] **Step 1: 创建 OpenSpec change `csr-chmem-writeback-complete`**

**注**: 不复用 archived `csr-chmem-pipeline-integration` (单 change 模式). 新建 `openspec/changes/csr-chmem-writeback-complete/`:
- `proposal.md` (R3.5 完整版描述, 1 段 Why + 1 段 What Changes + 1 段 Risk)
- `tasks.md` (Phase 0/1/2 全 checkbox 来自本 plan)
- `design.md` (引用 archived csr-chmem-pipeline-integration/design.md §R3 方法 X + 说明 R3.5 写通路补充)
- `specs/csr-chmem-csr-write/spec.md` (1 个 ADDED Requirement: 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写, mcycle 永不被 CSRR* 写)

- [ ] **Step 2: openspec change validate**

Run: `openspec change validate csr-chmem-writeback-complete 2>&1 | tail -3`
Expected: `Change "csr-chmem-writeback-complete" is valid`

- [ ] **Step 3: commit + merge fast-forward + push**

```bash
git add -A ip/cpu/plugins/csr_chmem.h tests/cpu/test_csr_chmem.cpp bundles/csr_bundles.h CHANGELOG.md
git commit -m "feat(csr-chmem-writeback-complete): R3.5 完整版 + 16/16 PASS"
# fast-forward merge to main (本 worktree 已 on main, 无需 merge)
git push origin main
```

- [ ] **Step 4: openspec archive**

Run: `openspec archive csr-chmem-writeback-complete -y 2>&1 | tail -5`
Expected: `Specs to update: csr-chmem-csr-write: create` + `Change 'csr-chmem-writeback-complete' archived as '2026-10-XX-...'`

- [ ] **Step 5: bash .rddf/sync_strategy_status.sh**

Run: `bash .rddf/sync_strategy_status.sh 2>&1 | tail -5`
Expected: `✅ [archived] 2026-10-XX-csr-chmem-writeback-complete: DONE` + `strategy.md §7 updated`

- [ ] **Step 6: commit archive + push**

```bash
git add -A openspec/ .rddf/roadmap/strategy.md
git commit -m "chore(csr-chmem-writeback-complete): archive + strategy.md §7 sync"
git push origin main
```

---

## 关键依赖与阻塞

| 依赖 | 状态 | 阻塞? |
|---|---|---|
| 基线 HEAD `c150b2e` (csr-chmem-pipeline-integration 已 archive) | ✅ merged in main | 必要, 不是阻塞 |
| `bundles/csr_bundles.h` 9 keys (含 CSR_WRITE_ADDR/VALUE/EN) | ✅ 已落地 (R3.1 commit `6cf3149`) | 必要, 不是阻塞 |
| `tests/cpu/test_csr_chmem.cpp` 14 case | ✅ 已落地 | 必要, 不是阻塞 |
| 架构 gate 12/12 + 8/8 | ✅ 基线 PASS | 必要, 不是阻塞 |
| mmu-chmem-pipeline-integration C.2 merge | ⏸ defer | **不阻塞** R3.5 (R2.3 才需要) |
| cpu-pipeline-tick-rca fix | ⏸ defer | **不阻塞** R3.5 (C.1 集成 e2e 才需要) |
| wave5-bp-btb (幽灵依赖) | ⏸ defer | 不阻塞 R3.5 (R2.3 优先级协调) |

## 已知风险 (per Oracle 2026-10-15 8 项 + 设计决策)

| 风险 | criticality | 缓解 |
|---|---|---|
| Oracle [Critical] 跨 stage payload 不传播 | **高** | Task 1.0 扩 cpu_factory_chmem.h §2b/§2c + populate_stage |
| Oracle [High] #1 write_en 不看 funct3/rs1 | **高** | Task 1.1 Step 2 4 层 select 树门控 |
| Oracle [High] #2 立即数变体写 RS1 而非 uimm | **高** | Task 1.1 Step 2 `select(funct3_high_bit, uimm_32, rs1)` |
| Oracle [High] #3 mcause/mepc 多 driver | **中** | Task 0.2 smoke 探明 + Task 1.2 Step 3 仲裁 select (如需) |
| R3.5 写通路引入 5-stage baseline regression | 中 | Task 1.0/1.1/1.2 Step 5/6 守门 m3_poc_5stage_byte_equal + [verilator] |
| spec 断言超出测试能力 | 中 | spec.md Scenario 1 降级为 DAG construction 验证 |
| Case 15/16 指令编码错误 | 中 | 2026-10-15 Oracle 报告修正 (0xB0005073 → 0xB00520F3) |
| bare path Verilator 退化 | 中 | Task 1.2/1.3 Step 6 守门 [verilator] [mmu-verilator] |
| `last_csr_write_en_ch` 新观察口破 portability Check 5 | 低 | write_en_bool 用 select 树, 严守 AGENTS.md §架构约束 #8 |
| writeback 闭包 7 个 `ch_reg <<=` 触发 cpphdl 5.052 elab 错 | 低 | R3.4 commit `6cf3149` 7 个 ch_reg 已 elab pass, 删 has() guard 不改 7 个 <<= 代码 |
| 2 cycle 写测试跨 stage 驱动问题 | 中 | 走单 cycle 观察口, 避免此问题 |
| ADR / lessons 锚定缺失 | 低 | spec.md + design.md 引用 ADR-040 v2.0 / ADR-046 / ADR-083 + lessons/phase-6c |

## 估时 (2026-10-15 Oracle 修订)

- Phase 0 Task 0.1 (基线): 15 min
- Phase 0 Task 0.2 (多 driver smoke): 1h
- Phase 1 Task 1.0 (propagate): 1.5h
- Phase 1 Task 1.1 (execute-LATE 写 + 4 层门控 + uimm): 2h
- Phase 1 Task 1.2 (writeback 删 guard + 仲裁): 1.5h
- Phase 1 Task 1.3 (Case 15-16 + 修正 inst encoding): 1.5h
- Phase 2 D + E: 1h
- **总: 9-10h, 跨 2 session (vs 原 5-6h 1 session)**

## Out-of-Scope (确认, 不在 R3.5 plan 内)

- **R2.3** (ibus_chmem.h branch-NORMAL select 覆写 PC 节点) — 等 mmu C.2 merge, 独立 plan
- **C.1 集成 e2e** (zicsr_basic 7/8-stage tohost=1 跑 cpu_sim C++ API) — 等 cpu-pipeline-tick-rca fix 或新走 C++ API 路径, 独立 plan
- **R2.5 mcycle 100 cycle 测试** (Case 15 改) — 不在 R3.5 范围, 留作 R2.5 完整 plan
- **`wave5-bp-btb` 优先级协调** — R2.3 启动时协商
- **64-bit CSR / PMP / Smode CSR / FPU CSR** — Phase 5+ 跟踪, 与本 change 正交
- **9 270+ CSR 全表** — D1 收窄 8 个, proposal §Out-of-Scope
- **CSRRS/CSRRC set/clear mask 完整展开** (`csr |= mask` / `csr &= ~mask`) — 未来 change 补 spec 严格性, 本 change 简化直写 RS1/uimm 退化为 CSRRW 语义

## 接受目标 (Oracle 2026-10-15 修订)

- `[cpu][csr-chmem]` 16/16 PASS (14 基线 + 2 新 R3.5 case, 含 Oracle 修正 inst encoding `0xB00520F3`)
- 8-plugin 流水线在 `chipforge_tests_chmem` 全套 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
- 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写 (writeback-NORMAL 闭包实际 ch_reg `<<=`)
- **funct3=000 (ECALL/EBREAK/MRET) 不触发 CSR 写** (Task 1.1 4 层门控 #1)
- **CSRRS/CSRRC rs1=x0 不触发 CSR 写** (Task 1.1 4 层门控 #2, csrr 读惯例保留)
- **CSRRWI/CSRRSI/CSRRCI uimm=0 不触发 CSR 写** (Task 1.1 4 层门控 #3, csrwi 读惯例保留)
- **立即数变体 CSRRWI/CSRRSI/CSRRCI 写值走 uimm (零扩展)** 而非 rs1 (Task 1.1 Step 2 `select(funct3_high_bit, uimm_32, rs1)`)
- mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数 维持, Task 1.1 4 层门控 #4)
- **mcause/mepc 多 driver 仲裁策略由 Task 0.2 smoke 决定并落地** (Task 1.2 Step 3, 维持 R2.4 trap + R3.5 CSR write 共存)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved (`[verilator] 1/1 case PASS (48 ELF)` + `[mmu-verilator] 3/3 case PASS` 维持)
- 架构 gate `12/12 + 8/8 + verify_adr 0 fail` 全 PASS
- OpenSpec change `csr-chmem-writeback-complete` archive + spec 升格
- strategy.md §7 派生同步
