## 1. 6d.1 DecoderPlugin 完整 CH_MEM

- [x] 1.1 新建 `ip/cpu/plugins/decode_chmem.h` (Oracle 修正: 原"重启用 .disabled"措辞错误, 该文件不存在, 从零建) — **commit d9ebbfd**
- [x] 1.2 实装 RV32I 主 opcode 7 bit + funct3 3 bit + funct7 7 bit 译码 (17 bit, ~80 项) — **d9ebbfd**
- [x] 1.3 输出 DECODED_INST Payload (含 opcode, funct3, funct7, rd_idx, rs1_idx, rs2_idx, imm, reads_rs1, reads_rs2, writes_rd, op_class, instr_format) — **d9ebbfd + 531b0dc**
- [x] 1.4 与 IntAluPlugin + BranchPlugin 通过 Payload 接口 — **531b0dc (DECODED_INST consumer migration)**
- [x] 1.5 新增 `tests/cpu/test_cpu_decoder_chmem.cpp::decoder_complete_elaborate` PoC — **d9ebbfd**
- [x] 1.6 验证: `decoder.v` 含 ~80 项 mux_select — **d9ebbfd (84 mux_select verified)**

## 2. 6d.2 BranchPlugin + HazardPlugin 完整 CH_MEM

- [x] 2.1 `branch_chmem.h` 完整 6-op B-type (BEQ/BNE/BLT/BGE/BLTU/BGEU) + `branch_target = pc + imm` — **531b0dc + fde730c (JAL link rd=pc+4)**
- [x] 2.2 `hazard_chmem.h` 完整 RAW 9 条件 OR-merge (6 数据 + 3 x0 屏蔽) — **5ab7f93 (PoC follow-up Fix #3) + 531b0dc (DECODED_INST read)**
- [x] 2.3 与 `CtrlLink::halt_when(ch_bool)` 集成, 接到 ID stage stall — **5ab7f93 + 531b0dc**
- [x] 2.4 新增 `tests/cpu/test_cpu_5stage.cpp::branch_hazard_complete_elaborate` PoC — **5ab7f93**
- [x] 2.5 验证: `branch.v` 含 6 mux_select + `hazard.v` 含 9 mux_select — **5ab7f93 (9 condition verified)**

## 3. 6d.3 CpuFactoryChmem 完整 5-stage 集成 (含 Oracle 新增 memory model 任务)

- [x] 3.0 **CH_MEM instruction fetch + memory model (Oracle 2026-09-20 关键发现)** ⏸
  - [x] 3.0.1 `cpu_factory_chmem.h` `T* memory` 当前 unused + 注释"PoC: 无 fetch stage, PC 暂恒值"; 6d.3 必须消除该 PoC 限制 — **60a5a24 (cpu_factory_chmem.h 7-plugin 5-stage 集成)**
  - [x] 3.0.2 新建 `ip/cpu/plugins/ibus_chmem.h` (oracle 关键缺失): IBusPlugin 完整 CH_MEM, 每周期从 ch_mem 读 32-bit instruction (PC+4 推进), wire 到 DECODE Payload — **60a5a24 + fde730c (pc_lag 1-cycle aread 对齐 + FLUSH)**
  - [x] 3.0.3 Memory model: `ip/cpu/plugins/dmem_chmem.h` (新增): ch_mem 预载 ELF image (PicolibcHostMemory::Config{base_addr, tohost_addr} → `load_elf_full`) + store 数据通路 (ch_mem write enable) — **60a5a24 + fde730c (preload_segment + tohost_probe + store_write_data/addr_proxy)**
  - [x] 3.0.4 新增 `tests/cpu/test_cpu_memory_model_chmem.cpp::cpu_memory_model_chmem_elaborate` PoC — **60a5a24 (3 PoC, 14 assertions)**
  - [x] 3.0.5 此任务**先于 6d.4**完成; 否则 riscv-tests 端到端无可执行目标 — **60a5a24 + fde730c**
- [x] 3.1 `cpu_factory_chmem.h::build_cpu()` 完整 5-stage elaboration (IF/ID/EX/MEM/WB) — **60a5a24 + fde730c (5 stage wire + EARLY populate + LATE linking)**
- [x] 3.2 5+ Plugin 注册: IBus + Decoder + IntAlu + Branch + Hazard + RegFile + DMem (新增) — **60a5a24 (7 plugins)**
- [x] 3.3 正确 stage wiring (每 Plugin 的 at_stage 顺序 + payload pre-population 沿用 v0.3.1 M6) — **60a5a24 + 531b0dc (Decoder 移到最前)**
- [x] 3.4 Stage linking connectors (M2/Spike-1..7) — **60a5a24 (全组合逻辑数据通路, 移除 6 stage pipeline_reg per 7d310c6)**
- [x] 3.5 新增 `tests/cpu/test_cpu_factory_5stage_complete.cpp::cpu_factory_5stage_complete_elaborate` PoC — **60a5a24 (m4_poc_5stage_elaborate_verilog)**
- [x] 3.6 验证: `cpu_5stage.v` 含 5 stage always_ff + ~80 decoder mux + 11 ALU mux + 6 branch mux + 9 hazard mux + 32 regfile select + IBus fetch mux + DMem store mux — **60a5a24 + fde730c (34 always_ff, 32 reg select, fetch+store mux)**

## 4. 6d.4 riscv-tests RV32I 端到端

- [x] 4.1 新建 `tests/cpu/test_cpu_chmem_vendored_elf.cpp` (Oracle 路径修正: 不重用 .disabled, 新建 vendored ELF 测试) — **fde730c**
- [x] 4.2 5 指令 ELF 准备 (add/addi/auipc/beq/jal) (Oracle 修正: 仓库无 riscv-tests submodule; 选项 B 采纳 — vendored ELF) — **fde730c (40 ELF 文件已 vendor 到 `tests/cpu/riscv_tests/elf/`)**
  - [x] 4.2.1 ~~选项 A: 为 addi/auipc/jal/beq 编写 `tests/cpu/manual_elf/*.S`~~ (已弃用 — vendored 路径已含 addi/auipc/jal/beq/add 5 个目标)
  - [x] 4.2.2 ELF32 段解析 (e_phoff@28 + e_phentsize@42 + e_phnum@44) → PT_LOAD 路由 IBus (PF_X) / DMem (PF_W) at offset (p_vaddr - 0x80000000) / 4
  - [x] 4.2.3 验证: 40 ELF 文件存在 + ELF magic 正确 (`riscv64-unknown-elf-readelf -h` PASS)
- [x] 4.3 CppHDL Simulator 跑 5 ELF 各 30+ cycle — **fde730c (30 cycles)**
- [x] 4.4 验证: 全部 tohost=1 (内存 0x80001000 写入 1) — **fde730c (1203 assertions PASS, 5 ELF PASS)**
- [x] 4.5 5 指令全部 PASS 后, 归档此 milestone — **b5fc979 (CHANGELOG v0.4.0) + 7d310c6 (Oracle hygiene)**

## 5. 6d.5 Verilator 后端集成 (E7 lint clean 已完成, E8 sim 拆 follow-up)

- [x] 5.1 ~~新建 `tools/verilator_runner/main.cpp`~~ (Oracle 2026-09-21 路径修正: E8 拆为独立 change, 因 Verilator sim 需要 CppHDL VerilatorBackend Phase 3.2-3.6 dlopen 实装, 是 sibling repo ADR-035 上游工作)
- [x] 5.2 `verilator --lint-only cpu_5stage.v` 无 error — **Oracle E7 已达成 (0 errors / 0 warnings)**
- [x] 5.3 `verilator --cc --build cpu_5stage.v` Vtop__ALL.a 生成 — **session 内验证成功**
- [x] 5.4 VL1Cache/VRegFile shared library 生成 — **N/A: E8 拆 follow-up (`phase-6d-verilator-sim`)**
- [ ] 5.5 Verilator sim 跑 5 ELF (同 6d.4) 各 10+ cycle — **拆 → `phase-6d-verilator-sim` change**
- [ ] 5.6 验证: Verilator sim 与 CppHDL sim 行为一致 (Oracle 2026-09-20 降级: 原"trace byte-equal"基线不存在, 接受 `tohost=1` + cycle 数 ±10%) — **拆 → `phase-6d-verilator-sim` change**
- [ ] 5.7 可选: `yosys -p "read_verilog cpu_5stage.v; synth; stat"` 综合验证 — **拆 → `phase-6d-verilator-sim` change**

## 6. CI 门禁扩展 (Oracle hygiene 已修, 9/9 在 follow-up)

- [x] 6.1 `check_plugin_portability.sh` 8/8 PASS (hygiene: 331c9af Check 1 awk 修复 + 7d310c6 §L.4 elaboration-time null guard 例外) — **session 内达成**
- [x] 6.2 `verify_adr.sh` 0 FAILED (ADR-037 v2.0 + ADR-040 v2.0 + ADR-046) — **session 内达成**
- [x] 6.3 `verify_plugin_decision.sh` 仍 PASS — **session 内达成**
- [x] 6.4 `./bin/chipforge_tests` TLM baseline 0 回归 (392 PASS / 11 baseline 已知 FAIL = 10 rv32ui LOAD + 1 7stage superscalar segfault) — **session 内达成**
- [x] 6.5 `./bin/chipforge_tests_chmem` 全量 PASS (36 test cases, 1751 assertions, 含 5 ELF tohost=1) — **session 内达成**
- [ ] 6.6 `check_plugin_portability.sh` 9/9 (新增 Check 9 CF_PLUGIN_USE_FSM_EXEMPT grep) — **拆 → `phase-6d-fsm-chmem` (6d.6/6d.7) change**

## 7. ADR 修订 + CHANGELOG (v0.4.0 已发, v3.0 等 follow-up)

- [x] 7.1 ~~ADR-040 v3.0 修订~~ — **拆 → follow-up `phase-6d-fsm-chmem` 完成 6d.6/6d.7 后再做 v3.0 (含 FSM 段)**
- [x] 7.2 ADR-037 v2.0 已在 prerequisites change 完成 — **9f7ad84**
- [x] 7.3 ~~ADR-046 验证 (ch_state_machine + CF_PLUGIN_USE_FSM_EXEMPT)~~ — **拆 → follow-up**
- [x] 7.4 CHANGELOG v0.4.0 发布: Phase 6d 6d.1-6d.4 + 6d.5 E7 + 5 指令 tohost=1 + Verilator lint clean — **b5fc979 + 7d310c6 (392/11 数字修正)**

## 8. Archive (当前 change scope)

- [x] 8.1 commit + push: 5 原子 commit (按子阶段) — **d9ebbfd / 60a5a24 / 531b0dc / fde730c / 0d30d64 / b5fc979 / 7d310c6 + CppHDL 7f7da88**
- [x] 8.2 `openspec archive phase-6d-rtl-verification --skip-validation` — **本 commit 完成 → archive**

## 9. 验证 (Acceptance Criteria)

### 已达成 (本次 change scope)
- [x] `tests/cpu/test_cpu_chmem_vendored_elf.cpp` 5 ELF (add/addi/auipc/beq/jal) `tohost=1` PASS — **fde730c (1203 assertions)**
- [x] `pb.to_verilog("cpu.v")` 含 5 stage always_ff (34 actual) + 32 regfile select — **60a5a24 + 0d30d64**
- [x] `bash tools/check_plugin_portability.sh` 8/8 PASS (9/9 在 FSM follow-up)
- [x] `bash tools/verify_adr.sh` 0 FAILED
- [x] TLM `chipforge_tests` 0 回归 (392 PASS / 11 baseline 已知 FAIL)
- [x] `verilator --lint-only cpu.v` 无 error — **Oracle E7**

### 拆 follow-up (`phase-6d-verilator-sim`)
- [ ] Verilator sim 跑 5 ELF 与 CppHDL sim `tohost=1` 一致 (Oracle 2026-09-20 降级: 不要求 trace byte-equal)
- [ ] MMU PTW sv32 5 状态 FSM (`ch_state_machine` + `CF_PLUGIN_USE_FSM_EXEMPT`)
- [ ] L1Cache refill 4 状态 FSM
- [ ] Harness 迁移 + ADR-040 v3.0

---

## Oracle 2026-09-21 路径修正

**本次 change 范围**: 6d.1 + 6d.2 + 6d.3 + 6d.4 + 6d.5 E7 + 6d hygiene (commit 7d310c6)

**拆为 follow-up**:
1. `phase-6d-verilator-sim`: 6d.5 E8 (Verilator sim 跑 ELF tohost=1) + 6d.8 (Harness 迁移)
   - 依赖: CppHDL VerilatorBackend Phase 3.2-3.6 (sibling repo ADR-035 上游)
2. `phase-6d-fsm-chmem`: 6d.6 (MMU/PTW sv32 5 状态) + 6d.7 (L1Cache refill 4 状态) + 9/9 check + ADR-040 v3.0 + ADR-046 验证
   - 依赖: 6d.5 E8 (Oracle 6d.6 task 6.6: ch_state_machine 必须 Verilator sim 验证 cycle-accurate)

**Oracle 关键发现** (audit session_id=ses_f405b4b2dffedOAwvAlKeXWW1p):
- 6d.4 vendored ELF 端到端真实可信 (1203 assertions, 5 ELF 全 PASS)
- 6d.5 E7 Verilator lint clean 真实可信 (0 errors / 0 warnings)
- Oracle #2 _reg_N Duplicate 修复根因 100% 正确 (std::array 默认构造泄漏孤儿节点)
- CHANGELOG 数字 386/17 → 392/11 已修 (17 是 stale binary 假象, 11 = 10 LOAD + 1 superscalar 全 pre-existing)
- D4 §4 例外 (elaboration-time null guard) 已文档化 (ADR-040 §L.4 Check 1)
- 死代码 (make_connector + Config) 已删 (commit 7d310c6)
- CppHDL 全局 lint_off 是 sibling issue, 不阻塞本 change (PoC 可接受, ADR-035 follow-up)
