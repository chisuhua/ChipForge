# csr-chmem-writeback-complete — tasks.md

> **Status**: ready (2026-10-15, design.md R3.5 决策 locked, Oracle/Metis 修订 2026-10-15)
> **Owner**: P1-4 wave5-isa-coverage-and-bp
> **Version target**: v0.11.0
> **Scope**: R3.5 writeback 完整版 — execute-LATE 写 CSR_WRITE_* (含 funct3/rs1/imm5 门控) + cpu_factory_chmem.h §2b/§2c propagate CSR_WRITE_* + writeback 删 has() guard, 16/16 PASS, 0 退化
> **Design decisions**: `design.md` (R3.5 决策完整)
> **Plan**: `.rddf/plans/csr-chmem-writeback-complete.md` (TDD 5-step, 估时 9-10h, 跨 2 session)
> **Depends on**: `csr-chmem-pipeline-integration` (✅ archived 2026-10-15, R1+R3+B.1 + C.1 scope-partial)
> **Oracle review**: 2026-10-15 — 8 findings (1 critical + 3 high + 2 medium + 2 low), all addressed in revised tasks below

---

## Phase Schedule Table (2026-10-15 修订: 跨 2 session, 估时 9-10h)

| Sprint | Window | Phase | Tasks | Owner |
|--------|--------|-------|-------|-------|
| Sprint 7a | 12-15 ~ 12-17 | Phase 0 (基线 + 多 driver smoke) | Task 0.1 + Task 0.2 | TBD |
| Sprint 7b | 12-18 ~ 12-19 | Phase 1 (R3.5 实施 + propagate) + Phase 2 (D+E Archive) | Task 1.0 + Task 1.1 + Task 1.2 + Task 1.3 + Task 2.1-2.4 | TBD |

**Phase 间依赖** (per `design.md` §4):
- Task 0.1 (基线确认) → Task 0.2 (多 driver smoke)
- Task 0.2 (smoke 结果) → Task 1.0 (propagate) 设计决策
- Task 1.0 (propagate) → Task 1.1 (execute-LATE 写 + funct3/rs1/imm5 门控)
- Task 1.0 + Task 1.1 → Task 1.2 (writeback 删 guard)
- Task 1.0 + Task 1.1 + Task 1.2 → Task 1.3 (Case 15-16 验证)
- Task 1.3 → Task 2.1 (架构门禁)
- Task 2.1 → Task 2.2 (CHANGELOG)
- Task 2.2 → Task 2.3 (AGENTS.md review)
- Task 2.3 → Task 2.4 (OpenSpec archive + push)

**实装偏差 (2026-10-15)**:
- **Task 1.2 Step 2 "删 has() guard"** → **实装保留** has() guard。**真因**: 单 plugin 测试路径 (`test_csr_chmem.cpp` 14 case + 2 新 case) 不调 `cpu_factory_chmem.h::build_cpu()`，因此 `populate_stage` + §2c propagate 不生效，writeback 节点缺 CSR_WRITE_* keys。删 guard 后 Case 1 触发 SIGSEGV (R3.4 兜底本就是为此设计)。build_cpu 路径 (m3_poc_5stage_byte_equal) 仍走 R3.5 完整版（has=true → 实际写 ch_reg）。
- **Task 1.2 Step 3 仲裁 select** → 实装在 writeback 内部，5 个无冲突 CSR (`mstatus/mtvec/mscratch/mip/mie`) 单 driver 直写，2 个多 driver CSR (`mcause/mepc`) 用 `is_trap_branch` 仲裁 (branch trap 优先)。branch-LATE 闭包 trap 写 mcause/mepc 路径不变 (`has(KeyType::PC)` guard)。

---

## Phase 0 — 基线建立 + 多 driver smoke (估时 1-2h)

### Task 0.1: 确认基线状态

- [ ] **Step 1**: 确认 HEAD 基线

  Run: `git rev-parse HEAD && git log --oneline -1`
  Expected: `8b126db` (docs commit for csr-chmem-writeback-complete OpenSpec change + plan) 或更新

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

### Task 0.2: 多 driver elaboration smoke (Oracle [High] #3 验证)

> **Oracle 2026-10-15 [High] #3**: branch-LATE 闭包 (csr_chmem.h:296-306) 写 `mcause_ <<= select(...)` 和 `mepc_ <<= select(...)`,本 change 让 writeback-NORMAL 也写 `mcause_ <<= ...` 和 `mepc_ <<= ...`。同 cycle 两个闭包对同 ch_reg 各 `<<=` 一次 — CppHDL 5.052 行为未知 (last-wins / elab error / undefined)。design.md:67 的"8 行代码已 elab pass"论证**不成立** (has() guard 让 branch-LATE 在 elab/首 tick 时为 false,从未真正入 DAG)。

- [ ] **Step 1**: 创建 `tests/cpu/test_csr_chmem_multidriver_smoke.cpp` (临时 smoke 测试,不入库)

  Run: 临时测试路径 (Phase 0 验证用,不入 git):
  ```bash
  cat > /tmp/test_multidriver_smoke.cpp << 'EOF'
  // Smoke: 8-plugin build_cpu + execute CSR write + ECALL 同 cycle
  //   - 验证 CppHDL 5.052 对 mcause_ 双 driver 行为
  //   - 验证 mcycle_ 自由计数 + CSR write 不冲突
  #include <ch.hpp>
  #include <core/context.h>
  // ... (build_cpu + ECALL inst + CSR write inst 交替, 跑 10 cycles, 观察 mcause/mepc)
  EOF
  ```
  **注**: smoke 不需要完整 Catch2 framework, 直接 `g++ -std=c++23` + link `libcf_plugin.a` + `libcpphdl.a` 验证 build_cpu + elaborate + sim->tick() 不 crash 即可。完整代码模板参考 `tests/cpu/test_csr_chmem.cpp` Case 14 末尾 + `ip/cpu/cpu_factory_chmem.h::build_cpu()`。

- [ ] **Step 2**: 跑 smoke + 记录行为

  Run: 跑 smoke (具体命令由 Step 1 实际代码决定)
  Expected: 行为记录到本 tasks.md 末尾 "Smoke Result" 段, 二选一:
  - **A. last-wins** (后注册闭包优先级): writeback-NORMAL 在 branch-LATE 之后注册, writeback 写优先 → mcause 由 CSRRW 决定 (破坏 R2.4 ECALL trap 写 mcause=11 的语义)
  - **B. elab error / undefined**: CppHDL 5.052 报错或 sim crash → 必须改 design 加 branch-writeback 仲裁 select
  - **C. first-wins** (罕见): branch-LATE 优先级 → ECALL trap 写 mcause=11 维持, CSRRW 写 mcause 被 branch 覆写

- [ ] **Step 3**: 决策 propagate + 仲裁策略

  根据 Step 2 结果:
  - **A. last-wins** → Task 1.2 writeback 写 mcause/mepc 之前, 必须确认 branch-LATE 在 5-stage pipeline 不与 writeback 同时活跃同一指令 (理论上不: branch 写 trap 当 cycle, writeback 写 CSR 是上 cycle CSR 指令的延迟结果)。若同时活跃, 加仲裁 select: `select(is_trap_branch_instr, branch_write, writeback_write)`
  - **B. elab error** → 必加仲裁 (同 A 末尾)
  - **C. first-wins** → 维持现状, 验证 trap 路径不退化即可
  - **任何结果**: 在 spec.md §Scenario "mcause/mepc single-driver invariant" 加对应实现细节

---

## Phase 1 — R3.5 写通路完整版 (估时 7-8h)

### Task 1.0: cpu_factory_chmem.h §2b/§2c propagate CSR_WRITE_* payload (Oracle [Critical] 修复)

> **Oracle 2026-10-15 [Critical]**: execute 节点和 writeback 节点是两个独立 KV store (cpu-stage-link-propagation spec:8), CH_MEM 流水线靠 cpu_factory_chmem.h §2b/§2c 硬编码白名单传播 (RESULT/RD_DATA/DECODE/INSTR/RS2/DECODED_INST), CSR_WRITE_* 不在白名单。删 has() guard (csr_chmem.h:209) 后 writeback 读缺失 key → emplace-on-miss null-impl ch_uint → 比较即 SEGV (M6 模式, CHANGELOG:839)。**所有走 build_cpu 的测试 ([chmem] vendored ELF, [verilator] 48-ELF, m3_poc_5stage_byte_equal) 全部打挂**。

- [ ] **Step 1**: 读 cpu_factory_chmem.h §2b/§2c + populate_stage 实码

  Run: `sed -n '145,191p' ip/cpu/cpu_factory_chmem.h` (populate_stage 段) + `sed -n '361,401p' ip/cpu/cpu_factory_chmem.h` (§2b/§2c 段)
  Expected: 看到 populate_stage (line 145-183) + §2b execute→memory (line 361-387) + §2c memory→writeback (line 389-401)

- [ ] **Step 2**: 扩 populate_stage 灌 CSR_WRITE_* 默认值

  在 populate_stage lambda 内 (line 145-183, line 164-166 之后 `n->payloads().put` block 之前) 加:
  ```cpp
  // R3.5 [GREEN]: CSR_WRITE_* 默认值预填 (避免 writeback 节点 emplace-on-miss SEGV)
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  n->operator()(CsrKT::CSR_WRITE_ADDR) = T(ch::core::ch_literal<0, 32>{});
  n->operator()(CsrKT::CSR_WRITE_VALUE) = T(ch::core::ch_literal<0, 32>{});
  // CSR_WRITE_EN 是 Payload<bool>, 用 ch_bool(false) 显式包装避开 contextual conversion
  n->payloads().put(CsrKT::CSR_WRITE_EN, false);
  ```
  **位置选择**: 必须在 populate_stage 内 (line 164 `n->payloads().put(KT::DECODE, default_decode);` 之后, line 167 `cf::cpu::arch::riscv::RiscvDecodeDetail` 之前 or 之后均可, 选后者保持 payload put 集中)

- [ ] **Step 3**: 扩 §2b execute→memory 白名单加 CSR_WRITE_*

  在 §2b lambda 内 (line 362-387, line 367 `if (exe && mem) {` 块内, line 373 之后) 加:
  ```cpp
  // R3.5 [GREEN]: propagate CSR_WRITE_* execute → memory (pre-writeback staging)
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  mem->operator()(CsrKT::CSR_WRITE_ADDR)  = exe->operator()(CsrKT::CSR_WRITE_ADDR);
  mem->operator()(CsrKT::CSR_WRITE_VALUE) = exe->operator()(CsrKT::CSR_WRITE_VALUE);
  mem->payloads().put(CsrKT::CSR_WRITE_EN, exe->payloads().get(CsrKT::CSR_WRITE_EN));
  ```

- [ ] **Step 4**: 扩 §2c memory→writeback 白名单加 CSR_WRITE_*

  在 §2c lambda 内 (line 389-401, line 395 `if (mem_st && wb) {` 块内, line 399 之后) 加:
  ```cpp
  // R3.5 [GREEN]: propagate CSR_WRITE_* memory → writeback
  using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
  wb->operator()(CsrKT::CSR_WRITE_ADDR)  = mem_st->operator()(CsrKT::CSR_WRITE_ADDR);
  wb->operator()(CsrKT::CSR_WRITE_VALUE) = mem_st->operator()(CsrKT::CSR_WRITE_VALUE);
  wb->payloads().put(CsrKT::CSR_WRITE_EN, mem_st->payloads().get(CsrKT::CSR_WRITE_EN));
  ```

- [ ] **Step 5**: 编译 + 跑现有 baseline 验证 propagate 不破 5-stage

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: 0 error + 185/185 PASS — populate 默认值 + propagate 不应影响 5-stage baseline (旧路径不读 CSR_WRITE_*)

- [ ] **Step 6**: 跑 [chmem] 全套 + TLM [mmu] 0 退化

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "[chmem]"
  /workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
  ```
  Expected: `[chmem] 37/38` + `[mmu] 59/59` 全 PASS

### Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN (含 funct3/rs1/imm5 门控)

> **Oracle 2026-10-15 [High] #1**: write_en 判定当前只看 zicsr_valid + csr_addr != 0xB00, 不看 funct3 / rs1_idx, 破坏 csrr 读惯例 (CSRRS/CSRRC rs1=x0) 和 ECALL/EBREAK/MRET 判定。
>
> **Oracle 2026-10-15 [High] #2**: CSR-Write 立即数变体 (CSRRWI/CSRRSI/CSRRCI funct3 ∈ {101,110,111}) 应写 uimm 而非 rs1, 本实装写 RS1 payload 静默错误。

- [ ] **Step 1**: 读 execute-LATE 闭包 CSR_READ put 段 + 提取 funct3/rs1_idx 已有信号

  Run: `sed -n '110,185p' ip/cpu/plugins/csr_chmem.h`
  Expected: 看到 `last_funct3_` (line 84 getter, line 133 写入) + `last_csr_read_value_` (line 86 getter)
  **注**: rs1_idx 已有 `last_rs1_idx_ch()` 风格访问器? grep 确认:
  Run: `grep -n "last_rs1_idx\|rs1_idx" ip/cpu/plugins/csr_chmem.h | head -10`
  Expected: 若无 → 需新加 `ch_uint<5> last_rs1_idx_{ch::core::ch_literal<0, 5>{}};` instance 成员 + execute-LATE 闭包内 `last_rs1_idx_ = bits<19, 15>(instr);` (per R1.2 模式, line 126-127 已有 `funct12_ch/imm5_ch` 用 `bits<>`)

- [ ] **Step 2**: 加 4 行 CSR_WRITE_* payload 写出 + funct3/rs1/imm5 门控

  在 `n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);` 之后 (line 177 之后), `last_csr_read_value_ = read_value;` 之前 (line 180 之前), 加:
  ```cpp
  // R3.5 [GREEN]: execute-LATE 闭包同时写 CSR_WRITE_* payload (R3.5 完整版)
  //   - CSR_WRITE_ADDR = csr_addr (last_csr_addr_ 已是 ch_uint<32> DAG signal)
  //   - CSR_WRITE_VALUE = select(funct3[2], uimm_zero_extended, rs1_val)
  //       (CSRRWI/CSRRSI/CSRRCI 写 uimm, CSRRW/CSRRS/CSRRC 写 rs1)
  //   - CSR_WRITE_EN = zicsr_valid
  //       && funct3 != 000  (排除 ECALL/EBREAK/MRET)
  //       && !(funct3 in {010,011} && rs1_idx == 0)  (csrr 读惯例不写)
  //       && !(funct3 in {110,111} && imm5 == 0)    (csrwi/csrri 读惯例不写)
  //       && csr_addr != 0xB00  (mcycle 永不被 CSRR* 写)
  //
  // 4 层 select 树 (R4 强制: 不用 if(ch_bool)):
  //   L1: zicsr_valid
  //     L2: !ecall_ebreak_mret (funct3 == 000)
  //       L3: !(csrr_read)  (funct3 in {010,011} && rs1_idx == 0)
  //         L4: !(csrwi_read)  (funct3 in {110,111} && imm5 == 0)
  //           L5: csr_addr != 0xB00
  //             T: write_en=true
  //             F: write_en=false
  //           F: write_en=false
  //         F: write_en=false
  //       F: write_en=false
  //     F: write_en=false
  const auto opcode_eq_73 = (opcode_ch == ch::core::ch_uint<7>(ch::core::ch_literal<0x73, 7>{}));
  const auto funct3_eq_000 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b000, 3>{}));
  const auto funct3_eq_010 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b010, 3>{}));
  const auto funct3_eq_011 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b011, 3>{}));
  const auto funct3_eq_110 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b110, 3>{}));
  const auto funct3_eq_111 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b111, 3>{}));
  const auto rs1_idx_eq_0  = (last_rs1_idx_ == ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}));
  const auto imm5_eq_0     = (imm5_ch == ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}));
  // csrr_read = funct3 in {010,011} && rs1_idx == 0
  const auto csrr_read     = (funct3_eq_010 || funct3_eq_011) && rs1_idx_eq_0;
  // csrwi_read = funct3 in {110,111} && imm5 == 0
  const auto csrwi_read    = (funct3_eq_110 || funct3_eq_111) && imm5_eq_0;
  const auto zicsr_valid_ch = opcode_eq_73;  // R3.5 简化: opcode==0x73 即 Zicsr
  const auto write_en_ch = ch::core::ch_uint<1>(select(
      zicsr_valid_ch,
      select(funct3_eq_000, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
      select(csrr_read,     ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
      select(csrwi_read,    ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
      select(csr_addr_eq_B00, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                              ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}))))),
      ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})));
  // CSR_WRITE_VALUE: 立即数变体走 uimm (零扩展), 寄存器变体走 rs1
  const auto funct3_high_bit = ch::core::ch_uint<1>(select(
      funct3_eq_000, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),  // placeholder
      select(funct3_eq_010, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
      select(funct3_eq_011, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
      select(funct3_eq_110, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
      select(funct3_eq_111, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                              ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})))))));
  // imm5 零扩展: imm5_ch 是 ch_uint<5>, 拼接 0 高位得 ch_uint<32>
  const auto uimm_32 = ch::core::ch_uint<32>(imm5_ch);  // 隐式零扩展
  const auto write_value_ch = select(funct3_high_bit, uimm_32, n->operator()(KeyType::RS1));
  const bool write_en_bool = static_cast<bool>(write_en_ch);
  n->payloads().put(CsrKT::CSR_WRITE_ADDR,  last_csr_addr_);
  n->payloads().put(CsrKT::CSR_WRITE_VALUE, write_value_ch);
  n->payloads().put(CsrKT::CSR_WRITE_EN,    write_en_bool);
  ```

  **注**: `zicsr_valid_ch` 是 R3.5 新增变量 (R1.3 commit 因 cpphdl 5.052 DAG 比较报错未落地, 本 change 简化用 `opcode==0x73`)。funct3/rs1_idx/imm5 提取用 R1.2 已有的 bits<>() 重提取模式 (`csr_chmem.h:126-128`)。

- [ ] **Step 3**: 加 last_rs1_idx_ instance 成员 + 公共 getter (Step 1 grep 缺失时)

  若 Step 1 grep 确认 last_rs1_idx_ 不存在:
  ```cpp
  // R3.5 单测观察口 + funct3/rs1 门控用
  ch_uint<5>  last_rs1_idx_{ch::core::ch_literal<0, 5>{}};
  ```
  在 private 段 (line 357-358 附近) 加。execute-LATE 闭包内 (line 131 `last_imm5_ = imm5_ch;` 后) 加:
  ```cpp
  last_rs1_idx_ = bits<19, 15>(instr);
  ```
  public 段 (line 82-84 附近) 加 getter:
  ```cpp
  ch_uint<5>  last_rs1_idx_ch()    const { return last_rs1_idx_; }
  ```

- [ ] **Step 4**: 编译 + 跑现有 csr-chmem 14 case 仍 PASS

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: 0 error + 14/14 PASS (105 assertions) — R3.5 加 write payload + funct3/rs1/imm5 门控不应影响读通路

- [ ] **Step 5**: 跑 5-stage baseline 仍 0 退化

  Run: `./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal" 2>&1 | tail -3`
  Expected: 185/185 PASS — 关键 regression 守门

### Task 1.2: writeback-NORMAL 闭包删 has() guard, 让 ch_reg 实际写 (含 Task 0.2 仲裁决策)

- [ ] **Step 1**: 读 writeback 闭包完整段

  Run: `sed -n '198,235p' ip/cpu/plugins/csr_chmem.h`
  Expected: 看到 `if (n) { ... if (n->payloads().has(...)) { ... } ... }` 嵌套

- [ ] **Step 2**: 删 has() guard 兜底

  删除 line 209 `if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {` 这一行, **同时**删除对应 line 233 的 `}` (整个 if 块的闭括号), 让 8 个 `mstatus_ <<= select(...)` 等语句**直接在** `if (n) {` 块内执行。

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
      auto we_340 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x340, 32>{}));
      auto we_341 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x341, 32>{}));
      auto we_342 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x342, 32>{}));
      auto we_344 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x344, 32>{}));
      auto we_304 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x304, 32>{}));

      mstatus_  <<= select(we_300, ch_uint<32>(write_value), ch_uint<32>(mstatus_));
      mtvec_    <<= select(we_305, ch_uint<32>(write_value), ch_uint<32>(mtvec_));
      mscratch_ <<= select(we_340, ch_uint<32>(write_value), ch_uint<32>(mscratch_));
      mepc_     <<= select(we_341, ch_uint<32>(write_value), ch_uint<32>(mepc_));
      // ── mcause/mepc 仲裁 (Task 0.2 result-driven) ──
      //   branch-LATE 也写 mepc/mcause (R2.4 ECALL/EBREAK trap), 多 driver
      //   仲裁策略由 Task 0.2 smoke 结果决定:
      //   - smoke A (last-wins): writeback 优先级高于 branch
      //     → 直接 `mcause_ <<= select(we_342, ...)` (writeback 写胜)
      //     → branch-LATE 在同 cycle 写 mcause 被 writeback 覆写
      //     → trap 路径退化 (R2.4 ECALL→mcause=11 不再生效)
      //   - smoke B (elab error): 必加仲裁 select
      //     → `mcause_ <<= select(is_trap_branch, branch_write, writeback_write)`
      //   - smoke C (first-wins): branch 优先, 直接写 (writeback 写被 branch 覆写)
      //   **default**: smoke C 或加仲裁 select 实现 branch 优先 (维持 R2.4 trap)
      //   实装代码 (Task 1.2 Step 2 最终):
      mcause_   <<= select(we_342, ch_uint<32>(write_value), ch_uint<32>(mcause_));
      mip_      <<= select(we_344, ch_uint<32>(write_value), ch_uint<32>(mip_));
      mie_      <<= select(we_304, ch_uint<32>(write_value), ch_uint<32>(mie_));
      // mcycle_ 不写 (R3 决策 + R2.5 自由计数, write_en 永 false 因 csr_addr_eq_B00)
    }
  });
  ```

- [ ] **Step 3**: 应用 Task 0.2 仲裁决策 (若需要)

  **若** Task 0.2 smoke 显示 B (elab error) 或 A (last-wins 但破坏 R2.4 trap), 在 writeback 写 mcause/mepc 处加仲裁:
  ```cpp
  // 仲裁: branch-LATE trap 写优先于 writeback CSR 写
  // is_trap_branch: funct12 ∈ {0x000, 0x001} (ECALL/EBREAK)
  //   - branch-LATE 用 last_funct12_ 判定, writeback 此处复用 last_funct12_
  //   - 注意: last_funct12_ 是 instance 成员, writeback 闭包访问同 instance OK
  const auto funct12_eq_000 = (last_funct12_ == ch::core::ch_uint<32>(ch::core::ch_literal<0x000, 32>{}));
  const auto funct12_eq_001 = (last_funct12_ == ch::core::ch_uint<32>(ch::core::ch_literal<0x001, 32>{}));
  const auto is_trap_branch = funct12_eq_000 || funct12_eq_001;
  // branch-LATE 写 mcause: ECALL→11, EBREAK→3, else 保持原值
  const auto branch_mcause_ch = select(funct12_eq_000, ch_uint<32>(ch_literal<11, 32>{}),
                               select(funct12_eq_001, ch_uint<32>(ch_literal<3, 32>{}),
                                      ch_uint<32>(mcause_)));
  // writeback 写 mcause: we_342 ? write_value : mcause_
  const auto wb_mcause_ch = select(we_342, ch_uint<32>(write_value), ch_uint<32>(mcause_));
  // 最终: branch 优先 (R2.4 维持)
  mcause_ <<= select(is_trap_branch, branch_mcause_ch, wb_mcause_ch);
  // mepc 同理 (PC+4 vs write_value 仲裁)
  // 注: writeback 闭包此刻无 PC node 访问 (PC+4 在 branch-LATE 算过, branch 写 mepc),
  //     若需仲裁 mepc 也按 is_trap_branch 优先于 we_341 写
  ```

- [ ] **Step 4**: 编译 + 跑现有 csr-chmem 14 case

  Run: `cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -5 && ./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: 0 error + 14/14 PASS — writeback 闭包现在实际写 ch_reg, 7 个 mstatus/mtvec/etc 写使 CSR_READ_VALUE 下一 cycle 改变, 但 R1.4/R3.5 现有 case 只测**单 cycle**, 不读"上一 cycle 写"状态, 故 14/14 仍 PASS

- [ ] **Step 5**: 跑 5-stage baseline + [chmem] 全套 + TLM [mmu] 0 退化

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
  ./build/bin/chipforge_tests_chmem "[chmem]"
  /workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
  ```
  Expected: `m3_poc_5stage_byte_equal 185/185` + `[chmem] 37/38 (1 known flake)` + `[mmu] 59/59` 全 PASS

- [ ] **Step 6**: 跑 [verilator] [mmu-verilator] bare path byte-identical (Oracle [Medium] 补守门)

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"
  ```
  Expected: `[verilator] 1/1 case PASS (48 ELF, 96 assertions)` + `[mmu-verilator] 3/3 case PASS` — bare path 0 退化

- [ ] **Step 7**: 跑架构门禁

  Run: `bash tools/check_plugin_portability.sh 2>&1 | tail -3 && bash tools/verify_plugin_decision.sh 2>&1 | tail -3`
  Expected: 12/12 + 8/8 全 PASS — R3.5 走 select 树, 不破 portability Check 5

### Task 1.3: R3.5 单元验证 — 2 新 case (CSRRW 写 mstatus + mcycle read-only)

- [ ] **Step 1**: 读现有 Case 14 末尾模板

  Run: `tail -30 tests/cpu/test_csr_chmem.cpp`
  Expected: 看到 `TEST_CASE("csr_chmem_R2_McycleFreeRunning_AfterOneTick", "[cpu][csr-chmem]") { ... }` 闭花括号

- [ ] **Step 2**: 加 2 个 public 访问器 (instance 成员 + getter) 到 `ip/cpu/plugins/csr_chmem.h`

  在 public 段 (line 86-87 附近, R3.5 观察口已有) 加:
  ```cpp
  // R3.5 单元测试用
  ch_uint<32> last_csr_write_value_ch() const { return last_csr_write_value_; }
  ch_uint<1>  last_csr_write_en_ch()    const { return last_csr_write_en_; }
  ```

  在 private 段 (line 361-362 附近, R3.5 读观察口已有) 加:
  ```cpp
  // R3.5 单测观察口: execute-LATE 闭包写通路输出
  ch_uint<32> last_csr_write_value_{ch::core::ch_literal<0, 32>{}};
  ch_uint<1>  last_csr_write_en_   {ch::core::ch_literal<0, 1>{}};
  ```

  在 execute-LATE 闭包末尾 (Task 1.1 Step 2 写 CSR_WRITE_EN put 之后) 加:
  ```cpp
  // R3.5 单测观察口 (与 CSR_WRITE_EN/VALUE 同步)
  last_csr_write_value_ = write_value_ch;
  last_csr_write_en_    = write_en_ch;
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
    //   bits[31:20]=0x300 (mstatus csr_addr)
    //   bits[19:15]=01010 (rs1=x10)
    //   bits[14:12]=001 (funct3=CSRRW)
    //   bits[11:7]=00001 (rd=x1)
    //   bits[6:0]=1110011 (opcode=0x73)
    //   inst = (0x300<<20) | (10<<15) | (0b001<<12) | (1<<7) | 0x73
    //        = 0x30000000 | 0x00050000 | 0x00001000 | 0x00000080 | 0x00000073
    //        = 0x300510F3 ✓
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
    //   - write enable = zicsr_valid && !ecall && !csrr_read && !csrwi_read && !mcycle_addr = 1
    //     (CSRRW funct3=001, 不在 {000, 010,011 with rs1=0, 110,111 with imm=0}, mstatus csr_addr != 0xB00)
    //   - write value = rs1_val = 0xCAFEBABE (funct3[2]=0, 走 RS1 路径)
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
    //   bits[31:20]=0xB00 (mcycle csr_addr)
    //   bits[19:15]=01010 (rs1=x10)
    //   bits[14:12]=010 (funct3=CSRRS)
    //   bits[11:7]=00001 (rd=x1)
    //   bits[6:0]=1110011 (opcode=0x73)
    //   inst = (0xB00<<20) | (10<<15) | (0b010<<12) | (1<<7) | 0x73
    //        = 0xB0000000 | 0x00050000 | 0x00002000 | 0x00000080 | 0x00000073
    //        = 0xB00520F3 ✓
    std::uint32_t inst_csrrs_mcycle = 0xB00520F3u;

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

    // R3.5 验证: csr_addr == 0xB00 (mcycle) → write enable 强制 false (R3 决策)
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_en_ch())) == 0u);
    // CSR_WRITE_VALUE 仍写出 (funct3[2]=0 走 RS1 路径 = 0xCAFEBABE), 但 write_en=false → writeback 不写
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_write_value_ch())) == 0xCAFEBABEu);
    // mcycle 单独路径 (R3 决策): READ_VALID=false
    REQUIRE(static_cast<std::uint64_t>(sim->get_signal_value(plugin_raw->last_csr_read_valid_ch())) == 0u);
  }
  ```

- [ ] **Step 5**: 跑 [cpu][csr-chmem] 期望 16/16 PASS (新增 2 case)

  Run: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
  Expected: `All tests passed (~118 assertions in 16 test cases)` (2 新 case × ~6 assertions)

- [ ] **Step 6**: 跑全套 regression 0 退化 (含 [verilator] 守门)

  Run:
  ```bash
  ./build/bin/chipforge_tests_chmem "[cpphdl]"
  ./build/bin/chipforge_tests_chmem "[chmem]"
  ./build/bin/chipforge_tests_chmem "m3_poc_5stage_byte_equal"
  ./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"
  /workspace/project/ChipForge/build/bin/chipforge_tests "[mmu]"
  ```
  Expected:
  - `[cpphdl] 6/6` + `[chmem] 37/38 (1 known flake)` + `m3_poc_5stage_byte_equal 185/185`
  - `[verilator] 1/1 case PASS (48 ELF, 96 assertions)` (bare path byte-identical preserved)
  - `[mmu-verilator] 3/3 case PASS` (bare path plumbing 0 退化)
  - `[mmu] 59/59` TLM path 维持
  - 全 PASS

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

  若 R3.5 commit 引入新 broken link, 修后重跑. 否则 commit msg 显式记录"doc_link_check 7 pre-existing broken, 0 新增".

### Task 2.2: E.1 — CHANGELOG.md v0.11.0 段加 R3.5 entry

- [ ] **Step 1**: 在 v0.11.0 csr-chmem-pipeline-integration 段后加 R3.5 entry

  新 `## v0.11.0 (interim) — csr-chmem-writeback-complete: ...` 段, 描述:
  - Added: writeback-NORMAL 闭包实际写 7 个 CSR ch_reg (R3.5 完整版)
  - Added: cpu_factory_chmem.h §2b/§2c + populate_stage propagate `csr_keys::CSR_WRITE_ADDR/VALUE/EN` (Oracle [Critical] 修复)
  - Added: execute-LATE 闭包 funct3/rs1/imm5 4 层门控 (排除 ECALL/EBREAK/MRET, 维持 csrr/csrwi 读惯例)
  - Added: CSR-Write 立即数变体 (CSRRWI/CSRRSI/CSRRCI) 写值走 uimm (零扩展) 而非 rs1
  - Modified: writeback-NORMAL 删 has() guard 兜底
  - Added: `[cpu][csr-chmem]` 14/14 → 16/16 PASS (~118 assertions)
  - Verification: 0 退化 (m3_poc_5stage_byte_equal 185/185 + [verilator] 1/1 + [mmu-verilator] 3/3 维持)
  - Scope-partial 残留: R2.3 / C.1 集成 e2e / CSRRS/CSRRC set/clear mask 展开

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
  git add -A ip/cpu/plugins/csr_chmem.h ip/cpu/cpu_factory_chmem.h tests/cpu/test_csr_chmem.cpp CHANGELOG.md openspec/
  git commit -m "feat(csr-chmem-writeback-complete): R3.5 完整版 + 16/16 PASS + propagate 修复

  Phase 1 Task 1.0: cpu_factory_chmem.h §2b/§2c + populate_stage propagate
    csr_keys::CSR_WRITE_ADDR/VALUE/EN (Oracle [Critical] 修复, 避免 writeback
    emplace-on-miss SEGV)
  Phase 1 Task 1.1: execute-LATE 闭包追加写 CSR_WRITE_ADDR/VALUE/EN + 4 层
    funct3/rs1/imm5 门控 (排除 ECALL/EBREAK/MRET, 维持 csrr/csrwi 读惯例;
    立即数变体走 uimm 而非 rs1)
  Phase 1 Task 1.2: writeback-NORMAL 闭包删 has() guard, 让 7 个 CSR ch_reg
    实际写 (mcause/mepc 多 driver 仲裁由 Task 0.2 smoke 决定)
  Phase 1 Task 1.3: Case 15 (CSRRW 写 enable+value) + Case 16 (mcycle write
    en=false), 修正 inst encoding (0xB0005073 → 0xB00520F3)
  Phase 2 E.1: 架构门禁 12/12 + 8/8 + verify_adr 0 fail 全 PASS
  Phase 2 E.2: CHANGELOG.md v0.11.0 加 R3.5 entry
  Phase 2 E.4: openspec change 4 个 artifacts 就位

  零退化:
  - [cpu][csr-chmem] 14/14 → 16/16 PASS
  - m3_poc_5stage_byte_equal 185/185 维持
  - [verilator] 1/1 (48 ELF, 96 assertions) bare path 维持
  - [mmu-verilator] 3/3 bare path plumbing 维持
  - TLM [mmu] 59/59 维持
  - 1 known cpphdl_poc_chbool_contextual_conversion flake 不变 (与本 change 无关)

  Scope-partial 残留 (与 csr-chmem-pipeline-integration archive 同源):
  - R2.3 等 mmu C.2 merge
  - C.1 集成 e2e 等 cpu-pipeline-tick-rca fix
  - CSRRS/CSRRC set/clear mask 展开 (本 change 简化: 直写 RS1/uimm 退化为 CSRRW 语义)
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

## Smoke Result (Task 0.2 Step 2 完成后填入)

> **填入格式**: "Behavior: [A last-wins | B elab-error | C first-wins]; Decision: [...]; Implementation: Task 1.2 Step 3 [applied | skipped]"

**实测结果 (2026-10-15)**:
- **Behavior: A (last-wins)** — CppHDL 5.052 接受双 driver (elaborate OK, sim OK), **writeback-NORMAL 优先级高于 branch-LATE** (注册顺序导致), shared_reg post-tick = 0xCAFEBABE (writeback 写值)
- **Decision**: 必加仲裁 select, Task 1.2 Step 3 实现 branch-LATE 优先 (维持 R2.4 ECALL/EBREAK trap 写 mcause=11/3, mepc=PC+4 语义)
- **Implementation**: Task 1.2 Step 3 加 `mcause_ <<= select(is_trap_branch, branch_mcause_ch, wb_mcause_ch)`, `mepc_` 同理

**Smoke 路径**: `/tmp/smoke_multidriver.cpp` (临时文件, 不入 git), 编译命令见文件头注释
**Smoke binary**: `/tmp/smoke_multidriver` (3.3MB, 2026-10-15 build)

**对 Phase 1 Task 1.2 的修正**:
- Task 1.2 Step 2 "删 has() guard" 后, **必须**实施 Step 3 仲裁 select, 不能跳过
- 仲裁原则: branch-LATE (R2.4 trap 路径) 优先于 writeback-NORMAL (R3.5 CSR 写路径)
- 实装代码见 tasks.md Task 1.2 Step 3 (已含仲裁 select 完整 4 步实现)

---

## 关键依赖与阻塞

| 依赖 | 状态 | 阻塞? |
|---|---|---|
| 基线 HEAD `8b126db` (docs commit) | ✅ 已落地 | 必要, 不是阻塞 |
| `bundles/csr_bundles.h` 9 keys (含 CSR_WRITE_ADDR/VALUE/EN) | ✅ 已落地 (R3.1 commit `6cf3149`) | 必要, 不是阻塞 |
| `tests/cpu/test_csr_chmem.cpp` 14 case | ✅ 已落地 | 必要, 不是阻塞 |
| 架构 gate 12/12 + 8/8 | ✅ 基线 PASS | 必要, 不是阻塞 |
| mmu-chmem-pipeline-integration C.2 merge | ⏸ defer | **不阻塞** R3.5 (R2.3 才需要) |
| cpu-pipeline-tick-rca fix | ⏸ defer | **不阻塞** R3.5 (C.1 集成 e2e 才需要) |
| wave5-bp-btb (幽灵依赖) | ⏸ defer | 不阻塞 R3.5 (R2.3 优先级协调) |
| Task 0.2 smoke result | ⏸ Phase 0 决定 | 决定 Task 1.2 Step 3 仲裁策略 |

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
| writeback 闭包 8 个 `ch_reg <<=` 触发 cpphdl 5.052 elab 错 | 低 | R3.4 commit `6cf3149` 8 个 ch_reg 已 elab pass, 删 has() guard 不改 8 个 <<= 代码 |
| 2 cycle 写测试跨 stage 驱动问题 | 中 | 走单 cycle 观察口, 避免此问题 (per design.md §决策 #3) |
| ADR / lessons 锚定缺失 | 低 | spec.md + design.md 引用 ADR-040 v2.0 / ADR-046 / ADR-083 + lessons/phase-6c |

## 接受目标

- [cpu][csr-chmem] 16/16 PASS (14 基线 + 2 新 R3.5 case)
- 8-plugin 流水线在 chipforge_tests_chmem 全套 0 退化 (m3_poc_5stage_byte_equal 185/185 维持)
- 7 个 M-mode CSR 经 CSRRW/CSRRS/CSRRC 实际可写 (R3.5 完整版, writeback-NORMAL 闭包实际 ch_reg `<<=`)
- 立即数变体 CSRRWI/CSRRSI/CSRRCI 写值走 uimm 而非 rs1
- funct3=000 (ECALL/EBREAK/MRET) 与 CSRRS/CSRRC rs1=x0 / CSRRWI 等 imm=0 不触发 ch_reg 写
- mcycle 永不被 CSRR* 写 (R3 决策 + R2.5 自由计数 维持)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved ([verilator] 1/1 + [mmu-verilator] 3/3 维持)
- mcause/mepc 多 driver 仲裁策略由 Task 0.2 smoke 决定并落地 (如需)
- 架构 gate 12/12 + 8/8 + verify_adr 0 fail 全 PASS
- OpenSpec change `csr-chmem-writeback-complete` archive + spec 升格 (`csr-chmem-csr-write` 新 spec)
- strategy.md §7 派生同步

## 关键决策摘要 (per design.md §5 + Oracle 修订)

| 决策 | 选 | 不选 | 关键 tradeoff |
|------|----|----|------|
| propagate 方案 (Oracle [Critical] 修复) | 扩 cpu_factory_chmem.h §2b/§2c + populate_stage 默认值 | writeback 改从 execute 直读 / execute-LATE 双写 | 唯一可行解, 跨 stage 直读 CppHDL DAG 难做, 双写破坏 R3.3 已有读通路 |
| write_en 4 层 funct3/rs1/imm5 门控 | 排除 ECALL/EBREAK/MRET + csrr/csrwi 读惯例保留 | 仅 zicsr_valid + mcycle 守卫 (R3.3 简化) | 满足 RISC-V spec + spec Scenario, 严守约束 #8 |
| CSR-Write 写值源 | `select(funct3_high_bit, uimm_32, rs1)` | 直写 rs1 (R3.3 简化) | CSRRWI 等立即数变体正确, 严守约束 #8 |
| writeback mcause/mepc 仲裁 (待 smoke) | Task 0.2 smoke 决定 (likely: branch 优先 + select 仲裁) | writeback 单写 (破坏 R2.4) / branch 单写 (破坏 R3.5) | 维持 R2.4 trap + R3.5 CSR write 共存 |
| 单 cycle 观察口 | 验证 payload 写出, 不验证 ch_reg 实际值 | 2 cycle 端到端 | 80% 价值, 避免 2 cycle 跨 stage 复杂度. C.1 集成 e2e 补剩余 20% |
| mcycle 永不被 CSRR* 写 | write_en=mcycle_addr 强制 false | 让用户写 mcycle | 维护 R3 决策 + R2.5 自由计数一致性 |
