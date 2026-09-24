# Phase 6c Tasks

> 9 周时间盒；每周 milestone + 退出标准见 `proposal.md` Acceptance 段。
> 状态同步：W0/M1/M2 已完成（commit 25e2672/9a03bb2/918e577/baa504b）；M3-M5 待启。
> 2026-09-17 修订 v2 (Oracle/Metis 审查后): 解决 6 项 CRITICAL + 4 项 MODERATE 错误。

## W0 (Day 1-3) — CppHDL 成熟度审计（已完成）

- [x] **W0/Day1** 读 CppHDL 关键头文件
- [x] **W0/Day1** 写审计报告 `docs/audit/cppHDL-maturity-audit.md`
- [x] **W0/Day2** W0 PoC #1-5: ch_device + toVerilog + CppHDL Simulator 跑 → **5/6 PASS** (commit 25e2672)
- [ ] **W0/Day2** W0 PoC #6: `ch_bool` 编译期错误纪律 → **FAIL (已知)**: `explicit operator bool()` + C++17 contextual conversion, 编译期通过且静默取 false; 类型系统不保护, 靠 CI grep (Check 5) 兜底
- [x] **W0/Day3** 创建 OpenSpec change
- [x] **W0/Day3** CHANGELOG v0.2.x 条目

## W1-2 (M1) — 底层翻转（已完成 commit 25e2672）

- [x] **M1/W1** `uint_t.h` 翻转: `uint_t<N> = ch::core::ch_uint<N>`, `bool_t = ch::core::ch_bool`
- [x] **M1/W1** 验证 `test_uint_t.cpp` PASS（删 `is_unsigned<ch_uint<N>>` 静态断言）
- [x] **M1/W1** `PayloadStore` cell 改造: 装 ch 代理对象
- [x] **M1/W2** `PipeBuilder::elaborate()` 桩方法 (M2/W3 完整实装)
- [ ] **M1/W2** `PipeBuilder::run()` 删除/`[[deprecated]]` — **推迟到 M5** (M3-M4 期间需要双模)
- [x] **M1/W2** hello.v PoC 5/6 PASS

## W3-4 (M2) — Stage plumbing（已完成 commit baa504b）

- [x] **M2/Spike-1..7** + **M2/W3..W4** 全部完成 (commits 9a03bb2/918e577/baa504b)
- [x] PoC #2 5 cases / 37 assertions ALL PASS
- [x] TLM↔CH_MEM 字节对标 (Spike 6 协议)
- [x] Spike 7 失败判据文档化 (chlib::PipelineChain 降级路径)

## M3 启动前必做的 6 个新子任务（Oracle/Metis 2026-09-17 审查结论）

> 重要：以下 6 项是 M3 启动的**前置硬阻塞**，不解决则 M3 第一天编译即失败或 PoC 不可达。

- [x] **M3/Prereq-1** **`PipeBuilder` 补 4 个 API** (Oracle A1):
  - `PipeBuilder(ch::core::context* ctx)` 构造 (持 ctx 指针供后续 API 用)
  - `void elaborate()` 无参重载 (内部用构造时传入的 ctx)
  - `void to_verilog(const std::string& filename)` 薄封装 (内部调 `ch::toVerilog(filename, ctx_.get())`)
  - `std::unique_ptr<ch::Simulator> create_simulator()` 薄封装 (内部 `new ch::Simulator(ctx_.get())`)
  - **理由**: 5 个 .disabled 文件 (test_plugin_elaborate_hello_poc/test_cpu_chmem_riscv_tests/cpu_factory_chmem 等) 都调用这些 API, 但 M2 交付的 pipe_builder.h 没有
  - **估时**: 0.5-1 天

- [x] **M3/Prereq-2** **修正 `int_alu_chmem.h.disabled` 语法错误** (Oracle A3):
  - 第 125 行 `select(dec ==(dec.reads_rs2), rs2_val, imm)` → 改为 `select(dec.reads_rs2, rs2_val, imm)`
  - 实装 6 op (auipc/lui/add/sub/sll/srl) 扩展到 11 op (add/addi/sub/sll/srl/sra/and/or/slt/sltu/xor) — 借鉴 `CppHDL/examples/riscv-mini/src/rv32i_alu.h:172` 11 层 select 嵌套
  - **估时**: 0.5-1 天

- [x] **M3/Prereq-3** **修正 `reg_file_chmem.h.disabled` 双 static thread_local regs 架构缺陷** (Oracle A3):
  - 问题: decode 闭包和 writeback 闭包各自持有独立的 `static thread_local std::array<ch_reg<...>, 32> regs{}`——写回的 regs 和读出的 regs 是两个不同对象, 写永不影响读
  - 修复: 提升 regs 为 plugin 实例成员 (`std::unique_ptr<ch_reg<ch_uint<kXlenBits>>[]> regs_`), 在 `build()` 中通过 PipeBuilder 传入的 ctx 构造, 单一数组供读/写闭包共享
  - **估时**: 0.5-1 天

- [x] **M3/Prereq-4** **`test_plugin_elaborate_hello_poc.cpp.disabled` 标记为 M1 时代过时** (Metis A2):
  - 原因: 该 PoC 对应 M1 框架双模化, M1 已用 5 个正式测试 (`test_cppHDL_hello_poc.cpp`) 完成
  - 行动: git rm + 提交说明移出 M3 范围
  - **估时**: 0.1 天

- [x] **M3/Prereq-5** **删除 M3/W5 bullet 2 技术错误** (Oracle A4):
  - 原 spec: "内部 `array_store<ch_reg<ch_uint<32>>,32>` 配合双缓冲 commit"
  - 问题: (a) `array_store` 的 `static_assert(is_trivially_copyable<T>)` 会被 ch_reg 触发编译失败; (b) ch_reg 本身是寄存器, 双缓冲语义无意义
  - 修复: 按 .disabled 的 "32 个独立 ch_reg" 方案 (Prereq-3 已修复双 static 缺陷)
  - **估时**: 0 天 (删一行)

- [x] **M3/Prereq-6** **明确 M3-PoC 范围降级** (Metis A4 + Oracle A2):
  - 原 spec: "跑 riscv-tests add.elf 在 CppHDL Simulator 下 tohost=1"
  - 问题: RegFile+ALU 两个 plugin 无 fetch/无 instruction memory (cpu_factory 骨架明示 `memory = nullptr`)/无 decode/branch/tohost 检测, 跑 ELF 端到端超出 M3 范围
  - 降级: M3-PoC = "RegFile+ALU 单元级 sim + TLM↔CH_MEM 字节对标" (手喂 payload 验证 read/write, 不跑 ELF); add.elf 端到端移到 M4
  - **估时**: 0.1 天 (修改 spec)

## W5-6 (M3) — RegFilePlugin + IntAluPlugin PoC

> **M2 交付依赖 (commit baa504b/918e577/9a03bb2)**:
> - `PipeBuilder::elaborate(ch::core::context& ctx)` + M3/Prereq-1 补 4 API
> - `register_stage_payload_connector<T>()` 注入 pipeline_reg
> - `CtrlLink::halt_condition() / flush_condition()` OR 合并
> - `chipforge_tests_chmem` 编译 target + tests/CMakeLists.txt 显式列表扩展
> - TLM↔CH_MEM 验证模式 (M2/Spike 6 协议)

### M3 任务

- [x] **M3/W5** 重启用 `ip/cpu/plugins/reg_file_chmem.h` (从 .disabled → 正式路径, 应用 Prereq-3 修正):
  - 32 个独立 `ch_reg<ch_uint<32>>` (不用 array_store) + 读/写端口 + x0 屏蔽用 `select(rd_addr != 0, ...)`
  - **不**用 static thread_local (Prereq-3 修复), 改用 plugin 实例成员, 单一数组
  - 复用 M2/W3 `register_stage_payload_connector<ch_uint<32>>` API
- [x] **M3/W5** 重启用 `ip/cpu/arch/riscv/int_alu_chmem.h` (从 .disabled → 正式路径, 应用 Prereq-2 修正):
  - 11 个 opcode 分流 (add/addi/sub/sll/srl/sra/and/or/slt/sltu/xor) → `select(opcode == X, ...)` 树
  - 借鉴 `CppHDL/examples/riscv-mini/src/rv32i_alu.h:172` 11 层 select 嵌套
  - 验证 Check 5 grep 0 违规 (运行期 `if(ch_bool)` 全 0)
- [ ] **M3/W5** `RegFilePlugin` + `IntAluPlugin` 在 `CpuFactoryChmem::build_cpu()` 中正确连接 (Prereq-1 提供的 `PipeBuilder(ch::core::context*)` 构造)
- [x] **M3/W5** 生成 `regfile.v` + `alu.v`, 位宽/端口检查:
  - 验证 `always @(posedge)` 出现 (ch_reg 证据, Verilog-2001)
  - 验证 mux (select 树) 出现
  - 验证 module 名/端口位宽断言清单
- [~] **M3/W6** **M3-PoC RegFile+ALU 单元级 sim** (避免与 W3 PoC #1-#4 编号冲突):
  - **降级范围** (Prereq-6): 不跑 add.elf 端到端; **手喂** RegFile write/read payload + ALU op 输入, 验证 read back 值正确
  - 降级原因: RegFile+ALU 无 fetch/无 instruction memory/cpu_factory 骨架明示 memory=nullptr; 跑 ELF 超出 M3 范围, 推到 M5
  - **TLM↔CH_MEM 字节对标** (复用 M2/Spike-6 协议: 纯 C++ `tlm_regfile_alu_simulate()` vs CppHDL Simulator)
  - 在 **CppHDL Simulator** (内置, `/workspace/project/CppHDL/include/simulator.h`) 下跑
  - **状态 (2026-09-17)**: PoC #1 (RegFile 单独 elaborate + regfile.v) **PASS**; PoC #2 (ALU 单独 elaborate + alu.v) **PASS**; PoC #3 (Combined) + PoC #4 (Byte-equal) **FIXED** — get_regs() singleton 改为 ctx_aware (key by ch::core::context*), Singleton 悬垂+cell put/get 问题解决, 41 assertions ALL PASS
  - **Defer 原因**: (a) `reg_file_chmem.h::get_regs()` static singleton 跨 context 悬垂 (Oracle 风险预警 #3); (b) `PayloadStore` cell put/get 在 CH_MEM 模式下不稳定 (put `ch_uint<32>(5)` 后 at_stage 闭包内 get 返回 0); (c) 提取 `ch_uint<N>` 值需 `ch::Simulator` 端口驱动路径
  - **M4/W8 修复路径**: 改用 `ch::Simulator` + `ch_in<T>` 端口驱动 (非 PayloadStore cell); 修复 `get_regs()` 为 ctx_aware (key by `ch::core::context*`); 单元化 ALU 为 `ch::Component` (如 W0 PoC HelloComponent)
- [x] **M3/W6** M3-PoC 迁入 `tests/cpu/test_cpu_rtl_regfile_alu.cpp` (用 `chipforge_tests_chmem` target 编译)
- [x] **M3/W6** **tests/CMakeLists.txt 扩展**: `chipforge_tests_chmem` 显式文件列表 (line 109-113 当前只含 framework/) 需添加 `${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_rtl_regfile_alu.cpp`

### M3 估时（修订 v2, Oracle/Metis 反馈）: 1.5-2 周（不是原 spec 的 1-1.5 周）

- M3/Prereq 1-6: 2-3 天 (4 API + 2 .disabled 修正 + 1 范围降级)
- W5: 4-5 天 (重启用 .disabled → 正式路径 + 验证 select 树)
- W6: 3-4 天 (M3-PoC 单元级 sim + TLM↔CH_MEM 字节对标 + CMake 扩展)
- 缓冲: 1-2 天 (Spike 7 降级演练)

## W7-8 (M4) — 5 级流水线算术子集

> **范围明确 (Oracle/Metis 反馈)**: "**最小** RV32I 算术子集" = add/addi/auipc/jal/beq 5 指令。**不是完整 RV32I**。riscv-tests 端到端 **推 M5** (本 M4 阶段接受"5-stage elaborate + toVerilog + Sim tick 不 crash" 而非"5 tohost=1")。

> **2026-09-17 修订 v2 (Oracle/Metis)**: M4 spec 包含以下**致命错误**已修:
> 1. ❌ 原 `ch_mem<ch_uint<X>, 2^32>` ROM 方案 (4 GB, 不可能) → 改 select 树
> 2. ❌ 原 `ip/cpu/plugins/decode.{h,cpp}` 独立文件 (不存在) → 合并到 int_alu_chmem.h
> 3. ❌ 原 spec 声称 M3 已实装 11 op, 实际 .disabled 只有 6 op (Prereq-2 修正)
> 4. ❌ 原 M4 接受 "5 riscv-tests tohost=1" 需 hazard detection, 但 cpu_factory_chmem 骨架明示"不实现 hazard" → HazardPlugin 是 M4 W7 **必须**, 不可推迟

### M4 任务

- [x] **M4/W7** **删除原 spec 错误** `ch_mem<ch_uint<X>, 2^32>` ROM 方案 (2^32 = 4 GB, 不可能编译) → **改为 select 树分发表**:
  - 借鉴 `CppHDL/examples/riscv-mini/src/rv32i_alu.h:172` 11 层 select 嵌套模式
  - RV32I 主 opcode 7 bit + funct3 3 bit + funct7 7 bit = 17 bit 总空间, 有效指令只占 ~80 项
  - 11 个分支 select 树足够, 不需要 ROM
- [x] **M4/W7** **删除原 spec 错误** `ip/cpu/plugins/decode.{h,cpp}` 独立文件方案 (该文件不存在, 也没在 .disabled) → **改为合并到 `int_alu_chmem.h`**:
  - 理由: RV32I 译码 + ALU 紧耦合, 独立文件反而难维护
  - 译码逻辑内嵌在 IntAlu 的 at_stage 闭包中
- [x] **M4/W7** 新建 `ip/cpu/plugins/branch_chmem.h`:
  - branch decision 用 select 树 (`branch_taken = select(branch_op, ...)` × `branch_target = pc + imm`)
  - **重命名**: 原 spec 写 "branch.{h,cpp}" 实际不存在; 用 `_chmem.h` 后缀与 TLM 模式分离
- [x] **M4/W7** **新建 `ip/cpu/plugins/hazard_chmem.h` (HazardPlugin, 不可推迟)**:
  - RAW 检测组合逻辑 + `CtrlLink::halt_when(ch_bool)` 接到 stall 门控
  - 复用 M2/W3 的 `CtrlLink::halt_condition()` (OR 合并) — **不**借鉴 chlib::stream_halt_when (操作 ch_stream, 不是 raw ch_bool)
  - **理由** (Metis A4 确认): 无 hazard, 5 级流水线的 rv32ui 测试无法 tohost=1 (riscv-tests ELF 含背靠背数据依赖)
  - 借鉴 `CppHDL/examples/riscv-mini/src/hazard_unit.h` (确认存在)
- [ ] **M4/W8** 重启用 `ip/cpu/cpu_factory_chmem.h` (从 .disabled → 正式路径, 6.8 KB):
  - `CpuFactoryChmem::build_cpu()` 整体在 elaboration 语义下生成完整 5 级流水线 DAG
  - 5 级: IF/ID/EX/MEM/WB
  - 4 个 Plugin: RegFilePlugin + RiscvIntAluPlugin + BranchPlugin + HazardPlugin
  - Hazard 注入: `pb.register_ctrl_link("ID", ctrl_id_halt);` 把 HazardPlugin 的 RAW 检测 ch_bool 接到 ID stage stall
- [ ] **M4/W8** 生成 `cpu.v` (**最小** RV32I 算术子集: add/addi/auipc/jal/beq 5 指令)
  - **不是完整 RV32I** (原 spec 措辞失准)
  - 对齐现有 `[cpu-l1-mmu-demo]` 5 用例
- [ ] **M4/W8** **M4-PoC 5-stage sim** (避免与 W3 PoC #1-#4 编号冲突, 改用 M4-PoC 前缀):
  - **降级** (Prereq-6 + Metis A4): M4 接受标准 = "5-stage elaborate + toVerilog + CppHDL Simulator tick 不 crash" 而非 "5 riscv-tests tohost=1"
  - riscv-tests 端到端 tohost=1 推 M5
- [ ] **M4/W8** (可选) `cpu.v` 经 **Yosys** 综合验证 synthesizable:
  - 命令: `yosys -p "read_verilog cpu.v; synth; stat"`
  - 期望: 无 syntax error, 输出 gate-level netlist
  - **Fallback**: 不可综合 → 标记"可仿真 RTL" (非"可综合 RTL"), 仍满足 README 承诺
  - 验证前先跑 `iverilog -t null` 或 `verilator --lint-only` 作为初检

### M4 估时（修订 v2, Oracle/Metis 反馈）: 2.5-3 周（不是原 spec 的 2-3 周）

- W7: 7-10 天 (译码 select 树 + branch_chmem + hazard_chmem 三个新文件 + 修正 cpu_factory_chmem 注释 "无 hazard" 实际化)
- W8: 5-7 天 (5-stage 集成 + M4-PoC 单元级 sim + 字节对标)
- 缓冲: 2-3 天 (Hazard 调试 + 综合验证)

## W9 (M5) — Harness 迁移 + ADR 收口 + riscv-tests 端到端

> **M5 范围扩展 (Oracle/Metis 反馈)**: riscv-tests 端到端 tohost=1 从 M4 推到 M5。M5 不再仅是文档收口, 还要包含 **Harness 迁移 + riscv-tests 端到端验证**。

### M5 任务

- [ ] **M5/W9** `tests/cpu/integration/test_*stage_riscv.cpp` 从 `pb.run()` 迁到 CppHDL sim runner
- [ ] **M5/W9** `tools/cpu_sim/main.cpp` 迁到 CppHDL sim 入口 (保留 CLI 兼容性)
- [ ] **M5/W9** **新增**: 重启用 `tests/cpu/test_cpu_chmem_riscv_tests.cpp.disabled` (7.9 KB), 在 CppHDL Simulator 下跑 riscv-tests add/addi/auipc/jal/beq 5 个 ELF tohost=1
  - 复用 M4-PoC 验证过的 5-stage + HazardPlugin
  - 对齐现有 `[cpu-l1-mmu-demo]` 5 用例
  - **这是 Phase 6c 的最终硬证据**
- [ ] **M5/W9** `tools/check_plugin_portability.sh` 增 Check 7 (TLM-only 文件不应含 `ch_reg<ch_uint` 等 ch 类型实例化) — 避免混合编译模式污染
- [ ] **M5/W9** `tools/run_chipforge_tests.sh` 跑全 ctest, 验证双 target 绿 (chipforge_tests TLM 47+ tests + chipforge_tests_chmem 8+ tests)
- [ ] **M5/W9** ADR-040 修订 (含 M1-M5 实证 commit hash): 列出 commit 25e2672/9a03bb2/918e577/baa504b + M3-M5 commit
- [ ] **M5/W9** ADR-037 修订: D4 范式在 elaboration 语义下兑现
- [ ] **M5/W9** 新增 ADR-046 完整化: 多周期协议引擎豁免 D4 无状态机禁令 (commit 25e2672 stub 已创建, M5 补完整)
- [ ] **M5/W9** CHANGELOG v0.3.0: TLM 仿真层废弃标记 + 列出 M1-M5 commit + Phase 6c 收官声明
- [ ] **M5/W9** 归档 OpenSpec change: `openspec archive plugin-elaboration-substrate`

## Out of Scope（明确推迟到 Phase 6d+）

- ❌ IBus/DBus LOAD width extraction (仍走原 TLM 路径或脱机分支)
- ❌ MMU/PTW 多周期 FSM (`ch_state_machine` 简化版不能在 CppHDL sim 跑 cycle-accurate)
- ❌ L1Cache refill FSM
- ❌ Branch predictor / OoO / ROB / LSQ (ARM 推迟到 Phase 2 baremetal)
- ❌ riscv-tests 完整 RV32GC 合规基线 (仍在 TLM 路径)
- ❌ Linux 启动 (Phase 4+)

## 风险与回退（2026-09-17 修订 v2, Oracle/Metis 反馈）

| 风险 | 回退 | 状态 |
|------|------|------|
| ~~W0 PoC 失败~~ | — | ✅ M2 已实证, 不适用 |
| ~~M1 翻转导致 TLM 测试失败~~ | — | ✅ M2 已实证零回归, 不适用 |
| M3 RegFile+ALU 验证失败 | M2 plumbing 已实证; 风险来自业务代码 (select 树边界/x0 屏蔽) → **回退**: M2/Spike-7 降级为 chlib::PipelineChain 薄封装 | 监控中 |
| **M4 Hazard 检测不工作** (Metis A4 关键风险) | 5-stage 流水线无 hazard → riscv-tests 静默失败; → M4 必须**先**完成 HazardPlugin, **后**做 5-stage 集成; 失败时降级: 跳过 riscv-tests 端到端 (M5 也可降级) | 🔴 关键监控 |
| M4 Yosys 综合失败 | 生成 Verilog 不可综合 → Phase 6c 范围缩小为"可仿真 RTL" (仅 CppHDL Simulator 跑通) | 监控中 |
| **M4 rv32ui 测试背靠背依赖** (Metis B3) | 即使 HazardPlugin 工作, 5 个 riscv-tests 全部 tohost=1 仍可能因 memory model 不全 (缺 instruction memory) 失败; → M4 接受降级为"5-stage sim tick 不 crash" | 监控中 |

## 总时间盒（修订 v2, Oracle/Metis 反馈）: 6-8 周（不是原 5-7 周）

| 阶段 | 任务 | 估时 | 状态 |
|------|------|------|------|
| W0 (Day 1-3) | CppHDL 审计 + W0 PoC | 0.5 周 | ✅ 完成 (commit 25e2672) |
| W1-2 (M1) | 框架双模化 | 1 周 | ✅ 完成 (commit 25e2672) |
| W3-4 (M2) | Stage plumbing | 1.5 周 | ✅ 完成 (commit baa504b) |
| W5-6 (M3) | RegFile+IntAlu + 6 Prereq | **1.5-2 周** | ⏳ 待启 (估时上调: Prereq + 缺陷修正) |
| W7-8 (M4) | 5-stage + Hazard + 综合 | **2.5-3 周** | ⏳ 待启 (估时上调: Hazard 必要) |
| W9 (M5) | Harness + riscv-tests 端到端 + ADR | **1-1.5 周** | ⏳ 待启 (范围扩展) |
| **总计** | — | **6-8 周** | **M3-M5 估 5-7 周** |

> 修订 v2 理由 (Oracle/Metis 反馈):
> - M3 估时 +0.5 周 (Prereq 1-6: 4 框架 API + 2 .disabled 修正 + 范围降级)
> - M4 估时 +0.5 周 (HazardPlugin 从零写, W7 必含, 调试预留)
> - M5 估时 +0.5 周 (riscv-tests 端到端从 M4 推到 M5, 范围扩展)
