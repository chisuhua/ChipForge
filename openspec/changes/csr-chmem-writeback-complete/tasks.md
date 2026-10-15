# csr-chmem-writeback-complete — tasks.md

> **Status**: ready (2026-10-15, design.md R3.5 决策 locked)
> **Owner**: P1-4 wave5-isa-coverage-and-bp
> **Version target**: v0.11.0
> **Scope**: R3.5 writeback 完整版 — execute-LATE 写 CSR_WRITE_* + writeback 删 has() guard, 16/16 PASS, 0 退化
> **Design decisions**: `design.md` (R3.5 决策完整)
> **Plan**: `.rddf/plans/csr-chmem-writeback-complete.md` (TDD 5-step, 估时 5-6h, 1 session)
> **Depends on**: `csr-chmem-pipeline-integration` (✅ archived 2026-10-15, R1+R3+B.1 + C.1 scope-partial)

---

## Phase Schedule Table (2026-10-15 修订: 1 session 完整交付)

| Sprint | Window | Phase | Tasks | Owner |
|--------|--------|-------|-------|-------|
| Sprint 7 | 12-15 ~ 12-19 | Phase 0 (基线) + Phase 1 (R3.5 实施) + Phase 2 (D+E Archive) | Task 0.1 + Task 1.1-1.3 + Task 2.1-2.4 | TBD |

**Phase 间依赖** (per `design.md` §4):
- Task 0.1 (基线确认) → Task 1.1 (execute-LATE 写)
- Task 1.1 (execute-LATE 写) → Task 1.2 (writeback 删 guard)
- Task 1.1 + Task 1.2 → Task 1.3 (Case 15-16 验证)
- Task 1.3 → Task 2.1 (架构门禁)
- Task 2.1 → Task 2.2 (CHANGELOG)
- Task 2.2 → Task 2.3 (AGENTS.md review)
- Task 2.3 → Task 2.4 (OpenSpec archive + push)

---

## Phase 0 — 基线建立 (估时 15 min)

### Task 0.1: 确认基线状态

- [ ] **Step 1**: 确认 HEAD 基线

  Run: `git rev-parse HEAD && git log --oneline -1`
  Expected: `c150b2e` (csr-chmem-pipeline-integration archive commit)

- [ ] **Step 2**: 确认 writeback has() guard 位置

  Run: `grep -n "if (n->payloads().has(CsrKT::CSR_WRITE_EN))" ip/cpu/plugins/csr_chmem.h`
  Expected: 1 行命中 (line 209)

- [ ] **Step 3**: 确认 CSR_WRITE_* keys 已存在

  Run: `grep -n "CSR_WRITE" bundles/csr_bundles.h`
  Expected: 3 行 (CSR_WRITE_ADDR / CSR_WRITE_VALUE / CSR_WRITE_EN)

- [ ] **Step 4**: 跑现有 baseline

  Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: `All tests passed (105 assertions in 14 test cases)` (基线 14/14 PASS)

- [ ] **Step 5**: 跑架构门禁

  Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh`
  Expected: `verify_adr 0 fail` + `verify_plugin_decision 8/8` + `check_plugin_portability 12/12` 全 PASS

---

## Phase 1 — R3.5 写通路完整版 (估时 4-5h)

### Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN payload

- [ ] **Step 1**: 读 execute-LATE 闭包 CSR_READ put 段

  Run: `sed -n '165,185p' ip/cpu/plugins/csr_chmem.h`
  Expected: 看到 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 行

- [ ] **Step 2**: 加 4 行 CSR_WRITE_* payload 写出 + write_en_bool 临时变量

  在 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 之后, `last_csr_read_value_ = read_value;` 之前, 加 (per design.md §R3.5 决策 #1):
  ```cpp
  // R3.5 [GREEN]: execute-LATE 闭包同时写 CSR_WRITE_* payload (R3.5 完整版)
  //   - CSR_WRITE_ADDR = csr_addr (last_csr_addr_ 已是 ch_uint<32> DAG signal)
  //   - CSR_WRITE_VALUE = rs1_val (CSRRW 写 rs1, 简化: CSRRS/CSRRC 退化为 CSRRW 语义)
  //   - CSR_WRITE_EN = zicsr_valid && !mcycle_addr (R3 决策: mcycle 永不被 CSRR* 写)
  const bool write_en_bool = static_cast<bool>(
      select(zicsr_valid_ch,
             select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                         ch_uint<1>(ch_literal<1, 1>{})),
             ch_uint<1>(ch_literal<0, 1>{})));
  n->payloads().put(CsrKT::CSR_WRITE_ADDR, last_csr_addr_);
  n->payloads().put(CsrKT::CSR_WRITE_VALUE, n->operator()(KeyType::RS1));
  n->payloads().put(CsrKT::CSR_WRITE_EN, write_en_bool);
  ```

  **注**: `zicsr_valid_ch` 是 R1.3 Zicsr_VALID select 树输出 — 若 execute-LATE 闭包尚未定义, 需先在 R1.2 重提取段补一行 `const auto zicsr_valid_ch = opcode_ch == ch_uint<7>(ch_literal<0x73, 7>{});` (csr-chmem-pipeline-integration R1.2 commit `0cc93cb` 因 cpphdl 5.052 DAG 比较报错未定义, 本 change 简化补上)

- [ ] **Step 3**: 编译 + 跑现有 csr-chmem 14 case 仍 PASS

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: 0 error + 14/14 PASS (105 assertions) — R3.5 加 write payload 不应影响读通路

- [ ] **Step 4**: 跑 5-stage baseline 仍 0 退化

  Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: `All tests passed (185 assertions in 1 test case)` — 关键 regression 守门

### Task 1.2: writeback-NORMAL 闭包删 has() guard, 让 ch_reg 实际写

- [ ] **Step 1**: 读 writeback 闭包完整段

  Run: `sed -n '200,230p' ip/cpu/plugins/csr_chmem.h`
  Expected: 看到 `if (n) { ... if (n->payloads().has(CsrKT::CSR_WRITE_EN)) { ... } ... }` 嵌套

- [ ] **Step 2**: 删 has() guard 兜底

  删除 line 209 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {` 这一行, **同时**删除对应 line 228 的 `}` (整个 if 块的闭括号), 让 8 个 `mstatus_ <<= select(...)` 等语句**直接在** `if (n) {` 块内执行。

  修改后 writeback 闭包结构:
  ```cpp
  pb.at_stage("writeback", cfc::Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("writeback").get();
    if (n) {  // 保留 node guard (无 writeback node 时 no-op)
      using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
      auto write_addr  = n->operator()(CsrKT::CSR_WRITE_ADDR);
      auto write_value = n->operator()(CsrKT::CSR_WRITE_VALUE);
      // write_en 是 bool (csr_keys 定义 Payload<bool>), 显式 ch_bool() 包装
      auto write_en_ch = ch_bool(n->operator()(CsrKT::CSR_WRITE_EN));

      // 8 个独立 write-enable + select 树 (R4 强制: 不用 if(ch_bool))
      auto we_300 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x300, 32>{}));
      auto we_305 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x305, 32>{}));
      auto we_340 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x340, 32>{}));
      auto we_341 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x341, 32>{}));
      auto we_342 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x342, 32>{}));
      auto we_344 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x344, 32>{}));
      auto we_304 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x304, 32>{}));

      mstatus_  <<= select(we_300, ch_uint<32>(write_value), ch_uint<32>(mstatus_));
      mtvec_    <<= select(we_305, ch_uint<32>(write_value), ch_uint<32>(mtvec_));
      mscratch_ <<= select(we_340, ch_uint<32>(write_value), ch_uint<32>(mscratch_));
      mepc_     <<= select(we_341, ch_uint<32>(write_value), ch_uint<32>(mepc_));
      mcause_   <<= select(we_342, ch_uint<32>(write_value), ch_uint<32>(mcause_));
      mip_      <<= select(we_344, ch_uint<32>(write_value), ch_uint<32>(mip_));
      mie_      <<= select(we_304, ch_uint<32>(write_value), ch_uint<32>(mie_));
      // mcycle_ 不写 (R3 决策 + R2.5 自由计数, write_en 永 false 因 csr_addr_eq_B00)
    }
  });
  ```

- [ ] **Step 3**: 编译 + 跑现有 csr-chmem 14 case

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: 0 error + 14/14 PASS — writeback 闭包现在实际写 ch_reg, 7 个 mstatus/mtvec/etc 写使 CSR_READ_VALUE 下一 cycle 改变, 但 R1.4/R3.5 现有 case 只测**单 cycle**, 不读"上一 cycle 写"状态, 故 14/14 仍 PASS

- [ ] **Step 4**: 跑 5-stage baseline 仍 0 退化

  Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: 185/185 PASS — 关键守门

- [ ] **Step 5**: 跑 [chmem] 全套 (含 1 known flake)

  Run: `./build/bin/chipforge_tests_chmem "[chmem]" 2>&1 | tail -3`
  Expected: 37/38 (1 known cpphdl_poc_chbool_contextual_conversion context pollution flake, 与本 change 无关)

- [ ] **Step 6**: 跑 TLM `[mmu]` 仍 PASS

  Run: `/workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3`
  Expected: 59/59 PASS — TLM path 维持

- [ ] **Step 7**: 跑架构门禁

  Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
  Expected: 12/12 + 8/8 全 PASS — R3.5 走 select 树, 不破 portability Check 5

### Task 1.3: R3.5 单元验证 — 2 新 case (实际写 + mcycle read-only)

- [ ] **Step 1**: 读现有 Case 14 末尾模板

  Run: `tail -30 tests/cpu/test_csr_chmem.cpp`
  Expected: 看到 `TEST_CASE("csr_chmem_R2_McycleFreeRunning_AfterOneTick", "[cpu][csr-chmem]") { ... }` 闭花括号

- [ ] **Step 2**: 加 2 个 public 访问器 (instance 成员 + getter) 到 `ip/cpu/plugins/csr_chmem.h`

  在 public 段 (R2.4 观察口附近, 约 line 73-77) 加:
  ```cpp
  // R3.5 单元测试用
  ch_uint<32> last_csr_write_value_ch() const { return last_csr_write_value_; }
  ch_uint<1>  last_csr_write_en_ch()    const { return last_csr_write_en_; }
  ```

  在 private 段 (R2.4 观察口附近, 约 line 264) 加:
  ```cpp
  // R3.5 单测观察口: execute-LATE 闭包写通路输出
  ch_uint<32> last_csr_write_value_{ch::core::ch_literal<0, 32>{}};
  ch_uint<1>  last_csr_write_en_   {ch::core::ch_literal<0, 1>{}};
  ```

  在 execute-LATE 闭包末尾 (Task 1.1 Step 2 写 CSR_WRITE_EN put 之后) 加:
  ```cpp
  // R3.5 单测观察口
  last_csr_write_value_ = n->operator()(KeyType::RS1);
  last_csr_write_en_    = ch_uint<1>(select(
      zicsr_valid_ch,
      select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                  ch_uint<1>(ch_literal<1, 1>{})),
      ch_uint<1>(ch_literal<0, 1>{})));
  ```

- [ ] **Step 3**: 加 Case 15 (CSRRW 写 mstatus, 单 cycle 验证 write enable + value 写出)

  ```cpp
  TEST_CASE("csr_chmem_R3_5_CsrrwWriteEnableAndValue", "[cpu][csr-chmem]") {
    ch::core::context ctx("csr_chmem_r3_5_csrrw");
    ctx.set_as_current_context();
    ch::core::ctx_swap guard(&ctx);

    cfc::PipeBuilder pb(&ctx);
    CsrPluginGuard plugin_guard(pb);
    cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

    using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
    // CSRRW x1, mstatus, x10 (rs1=x10=0xCAFEBABE): 写 mstatus=0xCAFEBABE
    //   bits[31:20]=0x300, bits[19:15]=x10=10, bits[14:12]=funct3=001, bits[11:7]=rd=1, bits[6:0]=0x73
    //   inst = (0x300<<20) | (10<<15) | (0b001<<12) | (1<<7) | 0x73
    //        = 0x30000000 | 0x50000 | 0x1000 | 0x80 | 0x73 = 0x300510F3
    std::uint32_t inst_csrrw = 0x300510F3u;

    pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrw]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrw);
        n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
        n->operator()(KT::RS1)         = ch_uint<32>(0xCAFEBABEu);  // 写 mstatus = 0xCAFEBABE
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
    REQUIRE_NOTHROW(sim->tick());

    // R3.5 验证 (单 cycle 观察口):
    //   - write enable = zicsr_valid && !mcycle_addr = 1 (CSRRW mstatus 非 mcycle)
    //   - write value = rs1_val = 0xCAFEBABE
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_en_ch())) == 1u);
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_value_ch())) == 0xCAFEBABEu);
  }
  ```

- [ ] **Step 4**: 加 Case 16 (mcycle 永不被 CSRR* 写, write enable = false)

  ```cpp
  TEST_CASE("csr_chmem_R3_5_McycleWriteEnableFalse", "[cpu][csr-chmem]") {
    ch::core::context ctx("csr_chmem_r3_5_mcycle_we");
    ctx.set_as_current_context();
    ch::core::ctx_swap guard(&ctx);

    cfc::PipeBuilder pb(&ctx);
    CsrPluginGuard plugin_guard(pb);
    cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>* plugin_raw = plugin_guard.raw;

    using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
    // CSRRS x1, mcycle, x10: mcycle 永不被 CSRR* 写 (R3 决策)
    //   bits[31:20]=0xB00, bits[19:15]=x10=10, bits[14:12]=funct3=010, bits[11:7]=rd=1, bits[6:0]=0x73
    //   inst = (0xB00<<20) | (10<<15) | (0b010<<12) | (1<<7) | 0x73
    //        = 0xB0000000 | 0x50000 | 0x2000 | 0x80 | 0x73 = 0xB0005073
    std::uint32_t inst_csrrs_mcycle = 0xB0005073u;

    pb.at_stage("execute", cfc::Phase::EARLY, [&pb, inst_csrrs_mcycle]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        n->operator()(KT::INSTRUCTION) = ch_uint<32>(inst_csrrs_mcycle);
        n->operator()(KT::PC)          = ch_uint<32>(0x80000000u);
        n->operator()(KT::RS1)         = ch_uint<32>(0xCAFEBABEu);  // CSRRS mask, mcycle 仍不写
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
    REQUIRE_NOTHROW(sim->tick());

    // R3.5 验证: csr_addr == 0xB00 (mcycle) → write enable 强制 false
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_en_ch())) == 0u);
    // CSR_WRITE_VALUE 仍写出 (execute-LATE 闭包无条件写), 但 write_en=false → writeback 不写
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_value_ch())) == 0xCAFEBABEu);
    // mcycle 单独路径 (R3 决策): READ_VALID=false
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_valid_ch())) == 0u);
  }
  ```

- [ ] **Step 5**: 跑 [cpu][csr-chmem] 期望 16/16 PASS (新增 2 case)

  Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: `All tests passed (~118 assertions in 16 test cases)` (2 新 case × ~6 assertions)

- [ ] **Step 6**: 跑全套 regression 0 退化

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "[cpphdl]"
  ./build/bin/chipforge_tests_chmem "[chmem]"
  ./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
  /workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
  ```
  Expected: `[cpphdl] 6/6` + `[chmem] 37/38 (1 known flake)` + `m3_poc_5stage_byte_equal 185/185` + `[mmu] 59/59` 全 PASS

- [ ] **Step 7**: 跑架构门禁

  Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
  Expected: 12/12 + 8/8 全 PASS

---

## Phase 2 — D + E Archive 准备 (估时 1h)

### Task 2.1: D — 架构门禁 + 文档健康

- [ ] **Step 1**: 跑 4 个 gate 脚本, 0 fail

  Run:
  ```bash
  bash tools/verify_adr.sh
  bash tools/verify_plugin_decision.sh
  bash tools/check_plugin_portability.sh
  bash tools/doc_link_check.sh
  ```
  Expected: 前 3 个 0 fail, doc_link_check 维持 pre-existing 7 broken (与本 change 无关, 记录不阻塞)

- [ ] **Step 2**: 若 doc_link_check 新增 broken → 修, 否则记录"非本 change 责任"

  若 R3.5 commit 引入新 broken link, 修后重跑. 否则 commit msg 显式记录"doc_link_check 7 pre-existing broken, 0 新增"

### Task 2.2: E.1 — CHANGELOG.md v0.11.0 段加 R3.5 entry

- [ ] **Step 1**: 在 v0.11.0 csr-chmem-pipeline-integration 段后加 R3.5 entry

  新 `## v0.11.0 (interim) — csr-chmem-writeback-complete: ...` 段, 描述:
  - Added: writeback-NORMAL 闭包实际写 7 个 CSR ch_reg (R3.5 完整版)
  - Modified: writeback-NORMAL 删 has() guard (R3.4 兜底取消)
  - Added: `[cpu][csr-chmem]` 14/14 → 16/16 PASS (~118 assertions)
  - Added: `last_csr_write_value_ch()` + `last_csr_write_en_ch()` 公共访问器
  - Verification: 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
  - Scope-partial 残留: R2.3 (等 mmu C.2) / C.1 集成 e2e (等 cpu-pipeline-tick-rca) — 与 csr-chmem-pipeline-integration archive 同源

### Task 2.3: E.2 — AGENTS.md §CH_MEM 模式 review

- [ ] **Step 1**: grep AGENTS.md "7-plugin" / "7 plugin" / "CSR P2 占位"

  若 0 命中: 现状已对齐 (R7 commit `0cc93cb` 时已修订), commit msg 说明"无 7→8 文字需修订"
  若命中: 按 R7 commit 历史修订

### Task 2.4: E.3-E.4 — OpenSpec change + archive

- [ ] **Step 1**: 验证 openspec change 完整性

  Run: `openspec change validate csr-chmem-writeback-complete 2>&1 | tail -3`
  Expected: `Change "csr-chmem-writeback-complete" is valid`

- [ ] **Step 2**: commit + push

  ```bash
  git add -A ip/cpu/plugins/csr_chmem.h tests/cpu/test_csr_chmem.cpp CHANGELOG.md openspec/
  git commit -m "feat(csr-chmem-writeback-complete): R3.5 完整版 + 16/16 PASS

  Phase 1 Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN
  Phase 1 Task 1.2: writeback-NORMAL 闭包删 has() guard 让 8 ch_reg 实际写
  Phase 1 Task 1.3: Case 15 (CSRRW 写 enable+value) + Case 16 (mcycle write en=false)
  Phase 2 E.1: 架构门禁 12/12 + 8/8 + verify_adr 0 fail 全 PASS
  Phase 2 E.2: CHANGELOG.md v0.11.0 加 R3.5 entry
  Phase 2 E.4: openspec change 4 个 artifacts 就位

  零退化:
  - [cpu][csr-chmem] 14/14 → 16/16 PASS
  - m3_poc_5stage_byte_equal 185/185 维持
  - TLM [mmu] 59/59 维持
  - 1 known cpphdl_poc_chbool_contextual_conversion flake 不变 (与本 change 无关)

  Scope-partial 残留 (与 csr-chmem-pipeline-integration archive 同源):
  - R2.3 等 mmu C.2 merge
  - C.1 集成 e2e 等 cpu-pipeline-tick-rca fix
  - CSRRS/CSRRC set/clear mask 展开 (本 change 简化: 直写 RS1 退化为 CSRRW 语义)
  "
  git push origin main
  ```

- [ ] **Step 3**: openspec archive

  Run: `openspec archive csr-chmem-writeback-complete -y 2>&1 | tail -5`
  Expected: `Specs to update: csr-chmem-csr-write: create` + `Change 'csr-chmem-writeback-complete' archived as '2026-10-XX-...'`

- [ ] **Step 4**: bash .rddf/sync_strategy_status.sh

  Run: `bash .rddf/sync_strategy_status.sh 2>&1 | tail -5`
  Expected: `✅ [archived] 2026-10-XX-csr-chmem-writeback-complete: DONE` + `strategy.md §7 updated`

- [ ] **Step 5**: commit archive + push

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

## 已知风险 (per design.md §7)

| 风险 | criticality | 缓解 |
|---|---|---|
| execute-LATE + writeback 同步改失误 (单边改 SEGV) | 中 | Task 1.1 Step 3 + Task 1.2 Step 3 强制跑 `[cpu][csr-chmem] 14/14` + `m3_poc_5stage_byte_equal 185/185` 守门 |
| 8 个 ch_reg `<<=` 触发 cpphdl 5.052 elab 错 | 低 | R3.4 commit `6cf3149` 8 个 <<= 已 elab pass, 删 has() guard 不改这 8 行 |
| `last_csr_write_en_` ch_signal 触发 portability Check 5 (禁 if(ch_bool)) | 低 | write_en_bool 用 select 树, 严守 AGENTS.md §架构约束 #8 |
| 5-stage baseline regression (mstatus 实际被写影响 baseline) | 中 | Task 1.2 Step 4 守门 m3_poc_5stage_byte_equal 185/185 |
| write enable bool 转换歧义 | 中 | 显式 `ch_bool(n->operator()(CsrKT::CSR_WRITE_EN))` 包装 (per R3.4 教训) |
| 2 cycle 写测试跨 stage 驱动问题 | 中 | 走单 cycle 观察口, 避免此问题 (per design.md §决策 #3) |

## 接受目标

- [cpu][csr-chmem] 16/16 PASS (14 基线 + 2 新 R3.5 case)
- 8-plugin 流水线在 chipforge_tests_chmem 全套 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
- 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写 (R3.5 完整版, writeback-NORMAL 闭包实际 ch_reg `<<=`)
- mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数 维持)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved
- 架构 gate 12/12 + 8/8 + verify_adr 0 fail 全 PASS
- OpenSpec change `csr-chmem-writeback-complete` archive + spec 升格 (`csr-chmem-csr-write` 新 spec)
- strategy.md §7 派生同步

## 关键决策摘要 (per design.md §5)

| 决策 | 选 | 不选 | 关键 tradeoff |
|------|----|----|------|
| execute-LATE 写 RS1 通用 | 直写 RS1 | funct3 判 set/clear mask 展开 | CSRRW 正确, CSRRS/CSRRC 退化为 CSRRW 语义. 未来 change 补 spec 严格性 |
| write_en 1-bit 走 select 树 | select + static_cast<bool> | if(ch_bool) 直接布尔 | AGENTS.md 约束 #8 |
| 单 cycle 观察口 | 验证 payload 写出, 不验证 ch_reg 实际值 | 2 cycle 端到端 | 80% 价值, 避免 2 cycle 跨 stage 复杂度. C.1 集成 e2e 补剩余 20% |
| mcycle 永不被 CSRR* 写 | write_en=mcycle_addr 强制 false | 让用户写 mcycle | 维护 R3 决策 + R2.5 自由计数一致性 |
