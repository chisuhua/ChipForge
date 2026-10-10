# Phase 6d.7: CHMEM LW/SW load-store replay stall — 探索结论与下会话 bootstrap guide

> 日期: 2026-10-10
> 状态: 🔴 架构级阻塞 — 方案 D (replay stall) 已尝试, 验证了根本限制, 未完成
> 关联: v0.11.0-prep 9 个 rv32ui-p-* load/store ELF 失败 (TOHOST=3)

## 1. 问题回顾

**29/38 rv32ui-p-* ELF PASS, 9 个 FAIL 全部是 load/store 系列**:
`lb, lbu, ld_st, lh, lhu, lw, sb, sh, sw` (全部 TOHOST=3, cycles=46-53)。

**根因 (Oracle 2026-10-10 确认)**:
1. `ch_mem` read port (aread **和** sread 一样) 总是 codegen 为 `always_ff` (1-cycle latency) —
   `CppHDL/codegen_verilog.cpp:1071-1074` 注释明确是**刻意设计** (pc_lag/FLUSH 依赖它)
2. `cpu_factory_chmem.h:395-400` 声明 datapath 是 **fully combinational** (无 stage register)
3. 结果: MEM stage 的 `mem.sread(word_addr)` 读的是 **上一 cycle 的 byte_addr** 对应的 dmem 值

**关键对照**: IBusPlugin 用 `pc_lag` (ch_reg 延迟) 对齐 PC/INSTRUCTION; dmem 没有等效补偿。

## 2. 已尝试的修复 (全部失败, 原因如下)

### 2.1 `sread → aread` (用户最初假设)
- ❌ aread 也是 always_ff (1-cycle latency), 无区别
- 证据: `ibus_chmem.h` 的 imem aread 同样是 always_ff

### 2.2 EX stage 提前 sread + ch_reg 延迟
- ❌ combinational pipeline 无 stage register 边界, EX/MEM 同一 cycle 跑同一指令,
  延迟寄存器无法对齐

### 2.3 方案 D: Load/Store Replay Stall (Oracle 推荐, 上游 riscv-mini 正道)
```
cycle N (LW first pass): sread enable fires; mem_busy <<= 1; stall=1
cycle N+1 (LW replay):   sread proxy carries dmem[addr_N]; stall=0; wb proceeds
```
- ✅ dmem 侧实现正确: `mem_busy_` ch_reg + `MEM_STALL`/`MEM_BUSY`/`LW_RD`/`LOAD_PENDING` payloads
- ✅ `update_pc` 消费 stall 停 pc_reg/pc_lag/br_taken_buf
- ✅ `reg_file_chmem.h` 用 `LOAD_PENDING` + `LW_RD` 在 replay 后写回 LW 的 rd
- ✅ 实测: LW replay 正确读出 `dmem[0x800]=0x00ff00ff`, a4 (x14) 正确写回
- ❌ **阻塞点**: fetch 无法在 stall 期间用 `pc_lag` 作为 fetch addr —
  fetch NORMAL 读不到当前 cycle 的 MEM_STALL (fetch 在 memory NORMAL 之前) —
  导致 **stall 后 fetch 重复取 lui t2 (下一指令执行 2 次)**:
  - `n->operator()(KT::INSTRUCTION)` 读当前指令 → 循环依赖 (INSTRUCTION 由 fetch 写)
  - probe aread 多端口 → 两个 always_ff 端口循环, fetch 整体卡死

## 3. 根本限制

**Combinational pipeline + ch_mem always_ff (1-cycle latency) + "当前指令"
只能通过 fetch 自产的 INSTRUCTION 观察到** — 三者组合意味着:
- fetch 无法"提前"知道下一指令是 LW (stall 检测需要 INSTRUCTION, 而 INSTRUCTION 由 fetch 写)
- stall 期间 fetch 必须继续取 LW (pc_lag), 但 fetch 读不到 stall 信号
- 任何"让 fetch 感知 stall"的方案都陷入组合循环

## 4. 可行的修复路径 (下会话选择)

| 路径 | 改动 | 工作量 | 风险 |
|------|------|--------|------|
| **A. 真 5-stage pipeline** (EX/MEM stage register) | factory 层把 EX→MEM 数据用 ch_reg 延迟 | 2-3 周 | hazard/flush/pc_lag 全改 |
| **B. 改 CppHDL codegen** (combinational read) | codegen_verilog.cpp: 让 read port 发 assign | 3-5 天 | 打爆 pc_lag (Oracle 警告) |
| **C. fetch 在 ID 前加 1 拍** (IF 真寄存器化) | 最小: 只在 fetch 加 ch_reg 边界 | 1-2 周 | 局部, 需验证 pc_lag/FLUSH |
| **D. dmem 提前 1 stage 读** (EX stage sread + MEM stage 完成) | 把 LW 的 mem 访问提前到 EX | 1-2 周 | 需要 EX stage 有 RESULT (byte_addr) |

**Oracle 评估 (weighted scoring)**: A=5.30, B=3.75, C=4.65, D=8.25 (方案 D 是原推荐,
但 fetch 阻塞未预料到; 现在 C 或 A 更现实)。

**最新推荐**: 路径 C (fetch 寄存器化) 或 A (真 pipeline)。B 明确不可行 (Oracle 有
codegen 注释证据)。D 的 dmem 侧逻辑 (mem_busy/load_reg/load_rd/LW_RD/LOAD_PENDING)
是**可复用的基础**, 下会话保留。

## 5. 已交付 (本会话, 保留在 git)

| 文件 | 改动 | 验证 |
|------|------|------|
| `ip/cpu/arch/riscv/int_alu_chmem.h` | shift_amount `bits<4,0>` + SRA sign-fill 公式 | sll/slli/srl/srli/sra/srai PASS |
| `ip/cpu/arch/riscv/int_alu_chmem.h` | RESULT = LD/ST?addr:ALU-result (修复自循环) | decoded_inst_intalu PASS |
| `ip/cpu/plugins/branch_chmem.h` | JALR 实装 (target=(rs1+imm)&~1) | jalr PASS |
| `ip/cpu/cpu_factory_chmem.h` | JALR link (rd=pc+4) | jalr PASS |

**回归**: `ctest chipforge_tests_chmem` PASS (277s), `cpu_verilator_sim_add` PASS,
`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` 全 PASS,
29/38 ELF PASS。

## 6. 下会话接手点

1. **决策**: 选路径 A / C (真 pipeline 或 fetch 寄存器化)
2. **复用**: dmem 的 `mem_busy_` / `addr_latch_` / `load_reg_` / `load_rd_` / `load_pending_`
   设计 (git checkout 回退了, 但代码在本文件历史中) — 建议在真 pipeline 下重新实现
3. **测试**: `test_cpu_chmem_vendored_elf.cpp` 只覆盖 5 个 PASS ELF, 需扩展到全部 38 个
   (其中 29 PASS + 9 load/store 待修)
4. **验证**: `cpu_verilator_sim --elf rv32ui-p-lw` tohost=1 是首个目标
