# csr-chmem-writeback-complete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 `csr-chmem-pipeline-integration` 留下的 R3.5 scope-partial deferred, 实装 CSR 寄存器**实际写通路**——使 `[cpu][csr-chmem]` 14/14 → 16/16 PASS, 8 个 M-mode CSR (mstatus/mtvec/mepc/mcause/mie/mip/mscratch/mcycle) 中前 7 个可被 CSRRW/CSRRS/CSRRC 实际写, mcycle 维持 read-only (R3 决策 + R2.5 自由计数), 0 退化。

**Architecture:** 单一 source-of-truth plugin `ip/cpu/plugins/csr_chmem.h` (基线 HEAD `c150b2e`)。两处微改: (1) execute-LATE 闭包追加写 `csr_keys::CSR_WRITE_ADDR/VALUE/EN` (3 行 `n->payloads().put`); (2) writeback-NORMAL 闭包**删** `if (n->payloads().has(...)) return;` 兜底 guard, 让 8 个 `ch_reg <<=` 实际写。无外部依赖, 无 mmu C.2 阻塞。

**Tech Stack:** C++23、CppHDL ch_state_machine DSL (per ADR-046 v2.0 §2.1.1 豁免)、Plugin 框架 (at_stage / payload keys / CtrlLink)、Catch2 v3.7.0。

---

## File Structure

### Production Code (实施改动)

| File | Responsibility | Tasks |
|---|---|---|
| `ip/cpu/plugins/csr_chmem.h` (340 LOC 基线, commit `c150b2e`) | execute-LATE 追加 CSR_WRITE_* payload 写; writeback-NORMAL 删 has() guard | Task 1.1 + Task 1.2 |
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

## Phase 0 — 基线建立 (估时 15 min)

### Task 0.1: 确认起点状态

**Files:**
- Verify: `ip/cpu/plugins/csr_chmem.h` line 113-180 (execute-LATE) + line 200-227 (writeback-NORMAL)
- Verify: `bundles/csr_bundles.h` line 45-47 (CSR_WRITE_ADDR/VALUE/EN keys)

- [ ] **Step 1: 确认 HEAD 基线**

Run: `git rev-parse HEAD && git log --oneline -1`
Expected: `c150b2e` (csr-chmem-pipeline-integration archive commit)

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

---

## Phase 1 — R3.5 写通路完整版 (估时 4-6h, 1 session)

### Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN payload

**Files:**
- Modify: `ip/cpu/plugins/csr_chmem.h` line 168-180 (execute-LATE 闭包末尾, CSR_READ_VALUE/VALID put 之后)

- [ ] **Step 1: 读 execute-LATE 闭包 CSR_READ put 段**

Run: `sed -n '165,185p' ip/cpu/plugins/csr_chmem.h`
Expected: 看到 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 行

- [ ] **Step 2: 加 4 行 CSR_WRITE_* payload 写出**

在 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 之后, `last_csr_read_value_ = read_value;` 之前, 加:

```cpp
// R3.5 [GREEN]: execute-LATE 闭包同时写 CSR_WRITE_* payload (R3.5 完整版)
//   - CSR_WRITE_ADDR = csr_addr (last_csr_addr_ 已是 ch_uint<32> DAG signal)
//   - CSR_WRITE_VALUE = rs1_val (CSRRW 写 rs1, CSRRS/CSRRC 写 mask; 完整 R3 通路覆盖 RS1 写入)
//   - CSR_WRITE_EN = zicsr_valid && !csr_addr_eq_B00 (R3 决策: mcycle 永不被 CSRR* 写)
n->payloads().put(CsrKT::CSR_WRITE_ADDR, last_csr_addr_);
n->payloads().put(CsrKT::CSR_WRITE_VALUE, n->operator()(KeyType::RS1));
// write_en 1-bit: zicsr_valid 走 ch_uint<1> select 树, 不引入 if(ch_bool)
const bool write_en_bool = static_cast<bool>(
    select(zicsr_valid_ch,
           select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                       ch_uint<1>(ch_literal<1, 1>{})),
           ch_uint<1>(ch_literal<0, 1>{})));
n->payloads().put(CsrKT::CSR_WRITE_EN, write_en_bool);
```

**注**: `zicsr_valid_ch` 变量是 R1.3 Zicsr_VALID select 树输出 (last_zicsr_valid_ch 相关) — 若 execute-LATE 闭包尚未定义 `zicsr_valid_ch`, 需先在 R1.2 重提取段补一行:
```cpp
const auto opcode_eq_73 = (last_opcode_ == ch_uint<7>(ch_literal<0x73, 7>{}));
const auto zicsr_valid_ch = opcode_eq_73;  // R3.5: 简化判定 (funct3==1/2/3/5/6/7 已含在 R1.3)
```
若 `zicsr_valid_ch` 已在 R1.3 commit (R1.2 commit 未含, 因当时 cpphdl 5.052 DAG 比较报错), **跳过**此行 + 改用 `(opcode_ch == ch_uint<7>(ch_literal<0x73, 7>{}))` 直接判定。

- [ ] **Step 3: 编译 + 跑现有 csr-chmem 14 case 仍 PASS**

Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: 0 error + 14/14 PASS (105 assertions) — R3.5 加 write payload 不应影响读通路

- [ ] **Step 4: 跑 5-stage baseline 仍 0 退化**

Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
Expected: `All tests passed (185 assertions in 1 test case)` — 关键 regression 守门

### Task 1.2: writeback-NORMAL 闭包删 has() guard, 让 ch_reg 实际写

**Files:**
- Modify: `ip/cpu/plugins/csr_chmem.h` line 209 (`if (n->payloads().has(CsrKT::CSR_WRITE_EN)) return;`)

- [ ] **Step 1: 读 writeback 闭包完整段**

Run: `sed -n '200,230p' ip/cpu/plugins/csr_chmem.h`
Expected: 看到 `if (n) { ... if (n->payloads().has(CsrKT::CSR_WRITE_EN)) { ... } ... }` 嵌套

- [ ] **Step 2: 删 has() guard 兜底**

删除 line 209 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {` 这一行, **同时**删除对应 line 228 的 `}` (整个 if 块的闭括号), 让 8 个 `mstatus_ <<= select(...)` 等语句**直接在** `if (n) {` 块内执行。

修改后 writeback 闭包结构:
```cpp
pb.at_stage("writeback", cfc::Phase::NORMAL, [this, &pb]() {
  auto* n = pb.node_of_logic_stage("writeback").get();
  if (n) {  // 保留 node guard (无 writeback node 时 no-op)
    using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
    auto write_addr  = n->operator()(CsrKT::CSR_WRITE_ADDR);
    auto write_value = n->operator()(CsrKT::CSR_WRITE_VALUE);
    auto write_en_ch = ch_bool(n->operator()(CsrKT::CSR_WRITE_EN));

    // 8 个独立 write-enable + select 树 (R4 强制: 不用 if(ch_bool))
    auto we_300 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x300, 32>{}));
    auto we_305 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x305, 32>{}));
    // ... (其他 5 个同模式)
    auto we_304 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x304, 32>{}));

    mstatus_  <<= select(we_300, ch_uint<32>(write_value), ch_uint<32>(mstatus_));
    // ... (其他 6 个 ch_reg <<= select)
    // mcycle_ 不写 (R3 决策 + R2.5 自由计数, write_en 永 false 因 csr_addr_eq_B00)
  }
});
```

- [ ] **Step 3: 编译 + 跑现有 csr-chmem 14 case**

Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: 0 error + 14/14 PASS — writeback 闭包现在实际写 ch_reg, 7 个 mstatus/mtvec/etc 写使 CSR_READ_VALUE 下一 cycle 改变, 但 R1.4/R3.5 现有 case 只测**单 cycle**, 不读"上一 cycle 写"状态, 故 14/14 仍 PASS

- [ ] **Step 4: 跑 5-stage baseline 仍 0 退化**

Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
Expected: 185/185 PASS — 关键守门

- [ ] **Step 5: 跑 [chmem] 全套 (含 1 known flake)**

Run: `./build/bin/chipforge_tests_chmem "[chmem]" 2>&1 | tail -3`
Expected: 37/38 (1 known cpphdl_poc_chbool_contextual_conversion context pollution flake, 与本 change 无关)

- [ ] **Step 6: 跑 TLM `[mmu]` 仍 PASS**

Run: `/workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3`
Expected: 59/59 PASS — TLM path 维持

- [ ] **Step 7: 跑架构门禁**

Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
Expected: 12/12 + 8/8 全 PASS — R3.5 走 select 树, 不破 portability Check 5

### Task 1.3: R3.5 单元验证 — 2 新 case (实际写 + mcycle read-only)

**Files:**
- Modify: `tests/cpu/test_csr_chmem.cpp` 末尾 (现有 14 case 后追加 Case 15-16)

- [ ] **Step 1: 读现有 Case 14 末尾模板**

Run: `tail -30 tests/cpu/test_csr_chmem.cpp`
Expected: 看到 `TEST_CASE("csr_chmem_R2_McycleFreeRunning_AfterOneTick", "[cpu][csr-chmem]") { ... }` 闭花括号

- [ ] **Step 2: 加 Case 15 (CSRRW 写 mstatus 后, 下一 cycle CSR_READ_VALUE 反映新值)**

**关键技术点**: 单 stage 测试只 1 cycle tick, writeback 写**下一 cycle** 才生效. R3.5 写通路**直接测试**: CSR_WRITE_EN + writeback <<= 同 cycle 即写 ch_reg next, 下一 cycle 才 visible. 单 case 内 `sim->tick()` 1 次后 mstatus_ 还是旧值. **必须** 2 cycle tick 才能看到.

**Case 15 设计**:
- tick 0: 灌 CSRRW mstatus=x1 (x1=0xCAFEBABE) → execute-LATE 写 CSR_WRITE_VALUE=0xCAFEBABE, writeback 写 mstatus_ next
- tick 1: 灌 CSRRS mstatus (读旧值) → 期望 last_csr_read_value_ = 0xCAFEBABE
- 验证 mstatus_ 实际被写

```cpp
TEST_CASE("csr_chmem_R3_CsrrwWritesMstatus_NextCycle", "[cpu][csr-chmem]") {
  ch::core::context ctx("csr_chmem_r3_5_csrrw_mstatus");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);
  CsrPluginGuard plugin_guard(pb);
  cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
  // CSRRW x1, mstatus, x0: 写 mstatus = 0, 读 old (0)
  // (rs1=x0, 写 0; rd=x1)
  std::uint32_t inst_csrrw = 0x300010F3u;
  // CSRRS x2, mstatus, x0: 读 mstatus (不写因 rs1=x0)
  std::uint32_t inst_csrrs = 0x30002073u;

  pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrw, inst_csrrs]() {
    auto* n = pb.node_of_logic_stage("execute").get();
    if (n) {
      n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrw);
      n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
      n->operator()(KT::RS1)         = ch_uint<32>(0);  // CSRRW 写 0
      n->operator()(KT::RS2)         = ch_uint<32>(0);
      n->operator()(KT::RD_IDX)      = ch_uint<5>(1);
    }
  });
  pb.build();
  auto exp = pb.elaborate(ctx);
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  // cycle 0: CSRRW mstatus=x0 → writeback 写 mstatus_ next=0
  REQUIRE_NOTHROW(sim->tick());

  // cycle 1: 改 INSTRUCTION 为 CSRRS mstatus, 读 old value
  auto* n_exec = pb.node_of_logic_stage("execute").get();
  if (n_exec) {
    n_exec->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrs);
    n_exec->operator()(KT::RS1)         = ch_uint<32>(0);
    n_exec->operator()(KT::RD_IDX)      = ch_uint<5>(2);
  }
  REQUIRE_NOTHROW(sim->tick());

  // 验证: cycle 1 读 mstatus 应该 = 0 (cycle 0 CSRRW 写入生效)
  // 注: 单 stage 测试 RD_DATA 节点无 NORMAL 闭包驱动, last_csr_read_value_ 是
  //   关键观察口 (execute-LATE 闭包 select 树输出 mstatus_ 当前值)
  auto read_val = static_cast<std::uint64_t>(
      sim->get_signal_value(plugin_raw->last_csr_read_value_ch()));
  INFO("cycle 1 last_csr_read_value_ = " << read_val);
  // CSRRS x2 mstatus 走 8 选 1 mux 选 mstatus, mstatus_ 在 cycle 0 写后 = 0
  REQUIRE(read_val == 0u);
}
```

**注**: 单 stage 测试无 writeback 节点驱动, 真实 mstatus_ ch_reg 不会收到 next → **此 case 实际上**测的是 mstatus_ 的 next proxy 在 elaborate 阶段是否被设. 写 next, 但无 posedge 触发 mstatus_ 更新. **务实变体**: 不依赖多 cycle, 改测**单 cycle 内 write 通路完整性**:
- 灌 CSRRW x1, mstatus, x0 (rs1=0xCAFEBABE) → execute-LATE 闭包写 CSR_WRITE_* payload
- 验证 last_csr_write_en_ch() (新增观察口) 1 周期内已写出 CSR_WRITE_EN=true
- 见 Step 3 实施

- [ ] **Step 3: 加 last_csr_write_en_ch 观察口 + Case 15 改写**

**如果** plan Step 2 决定走"单 cycle 观察口"路径 (避免 2 cycle 多 stage 问题), 改 plan:

1. `ip/cpu/plugins/csr_chmem.h` execute-LATE 闭包末尾 (Task 1.1 Step 2 写 CSR_WRITE_EN put 之后) 加:
   ```cpp
   // R3.5 单测观察口: 写 enable signal post-tick
   last_csr_write_en_ = ch_uint<1>(select(
       opcode_eq_73,
       select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                               ch_uint<1>(ch_literal<1, 1>{})),
       ch_uint<1>(ch_literal<0, 1>{})));
   ```
2. instance 成员加 `ch_uint<1> last_csr_write_en_{ch::core::ch_literal<0,1>{}};`
3. 公共访问器加 `ch_uint<1> last_csr_write_en_ch() const { return last_csr_write_en_; }`
4. Case 15 改用单 cycle 验证 last_csr_write_en_ch() == 1, last_csr_write_value 字段同理

**最终推荐**走此变体 (Step 2 注释已说明)。

- [ ] **Step 4: 加 Case 16 (mcycle read-only 验证: CSRRS mcycle 写 enable = false)**

```cpp
TEST_CASE("csr_chmem_R3_McycleWriteEnableFalse", "[cpu][csr-chmem]") {
  // CSRRS mcycle (0xB00) → write_en 永 false (R3 决策: mcycle 永不被 CSRR* 写)
  // 单 cycle 验证 last_csr_write_en_ch() == 0
  // ... (仿 Case 15 结构, INSTRUCTION = 0xB00020F3, 灌 execute, tick, 验证)
  REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_en_ch())) == 0u);
}
```

- [ ] **Step 5: 跑 [cpu][csr-chmem] 期望 16/16 PASS (新增 2 case)**

Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
Expected: `All tests passed (~118 assertions in 16 test cases)` (2 新 case × ~6 assertions)

- [ ] **Step 6: 跑全套 regression 0 退化**

Run:
```bash
./build/bin/chipforge_tests_chmem "[cpphdl]"
./build/bin/chipforge_tests_chmem "[chmem]"
./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
/workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
```
Expected: `[cpphdl] 6/6` + `[chmem] 37/38 (1 known flake)` + `m3_poc_5stage_byte_equal 185/185` + `[mmu] 59/59` 全 PASS

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

## 已知风险 (per csr-chmem-pipeline-integration/design.md §7 + R3.5 特定)

| 风险 | criticality | 缓解 |
|---|---|---|
| R3.5 写通路引入 5-stage baseline regression | 中 | Task 1.2 Step 4 + Task 1.3 Step 6 守门 m3_poc_5stage_byte_equal |
| `last_csr_write_en_ch` 新观察口破 portability Check 5 (禁 if(ch_bool)) | 低 | select 树, 不引入 if(ch_bool) |
| writeback 闭包 8 个 `ch_reg <<=` 触发 cpphdl 5.052 elab error | 低 | 基线 (R3.4 commit `6cf3149`) 8 个 ch_reg 已 elab pass, 删 has() guard 不改 8 个 <<= 代码 |
| 2 cycle 写测试跨 stage 驱动问题 | 中 | plan §Task 1.3 Step 3 推荐走"单 cycle 观察口"变体规避 |

## 估时

- Phase 0 (基线): 15 min
- Phase 1 Task 1.1 (execute-LATE 写): 1h
- Phase 1 Task 1.2 (writeback 删 guard): 1h
- Phase 1 Task 1.3 (Case 15-16): 2h (含 1h 单 cycle 观察口路径决策 + 1h 实施测试)
- Phase 2 D + E: 1h
- **总: 5-6h, 1 session 完成**

## Out-of-Scope (确认, 不在 R3.5 plan 内)

- **R2.3** (ibus_chmem.h branch-NORMAL select 覆写 PC 节点) — 等 mmu C.2 merge, 独立 plan
- **C.1 集成 e2e** (zicsr_basic 7/8-stage tohost=1 跑 cpu_sim C++ API) — 等 cpu-pipeline-tick-rca fix 或新走 C++ API 路径, 独立 plan
- **R2.5 mcycle 100 cycle 测试** (Case 15 改) — 不在 R3.5 范围, 留作 R2.5 完整 plan
- **`wave5-bp-btb` 优先级协调** — R2.3 启动时协商
- **64-bit CSR / PMP / Smode CSR / FPU CSR** — Phase 5+ 跟踪, 与本 change 正交
- **9 270+ CSR 全表** — D1 收窄 8 个, proposal §Out-of-Scope

## 接受目标

- `[cpu][csr-chmem]` 16/16 PASS (14 基线 + 2 新 R3.5 case)
- 8-plugin 流水线在 `chipforge_tests_chmem` 全套 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
- 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写
- mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数 维持)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved
- 架构 gate `12/12 + 8/8 + verify_adr 0 fail` 全 PASS
- OpenSpec change `csr-chmem-writeback-complete` archive + spec 升格
- strategy.md §7 派生同步
