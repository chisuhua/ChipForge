# ChipForge Roadmap Evolution — 架构演进 SSOT (Phase 6d → v1.3.0)

> **Status**: SSOT（2026-10-09 起，由 `docs/roadmap/strategy/execution-roadmap.md §6` + `soc/cpu/docs/roadmap/execution-roadmap.md §3.5` + `docs/roadmap/phases/phase-6d-rtl-verification.md` 合并去重而来）
> **Owner**: ChipForge Build Team
> **关联**: [`.rddf/roadmap/`](../../.rddf/roadmap/) 实施规划 + [`.rddf/roadmap/strategy.md`](../../.rddf/roadmap/strategy.md) 战略 SSOT + `openspec/changes/*/tasks.md` 任务执行
> **本文档定位**: 视觉/架构驱动的"怎么做"（与文本驱动的"做什么"分工：本文承担架构图 + 跨版本对比；`.rddf/roadmap/phases/*.md` 承担完成条件；`.rddf/roadmap/objectives/*.md` 承担决策门禁；`openspec/changes/*/tasks.md` 承担任务级执行）

---

## 0. 演进全景（Phase 6d → v0.8.0 → v0.9.0 → v0.10.0 → v1.0.0 → v1.2.0 → v1.3.0）

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  2026 Q4         2026-12         2027-02        2027 Q1        2027 Q3      2028 Q2     2029 Q1   │
│  ┌────────┐     ┌────────┐      ┌────────┐     ┌────────┐     ┌────────┐   ┌────────┐   ┌────────┐ │
│  │Phase 6d│ ──► │ v0.8.0 │ ───► │ v0.9.0 │ ──► │v0.10.0 │ ──► │ v1.0.0 │──►│ v1.2.0 │──►│ v1.3.0 │ │
│  │5-stage │     │ MMU 真  │      │ CSR +  │     │RV32IMAC│     │ S/U +  │   │ 4-way  │   │Linux-on│ │
│  │CH_MEM  │     │ 端到端  │      │ 4-way  │     │+RV32C  │     │ AMO +  │   │RRIP+   │   │ FPGA + │ │
│  │+Veril. │     │+cycle- │      │ Cache  │     │+ICache │     │ BTB +  │   │PMP +   │   │chip-   │ │
│  │+FSM    │     │precis. │      │+except.│     │+MUL/   │     │ PLIC/  │   │Debug + │   │selector│ │
│  │(前置)  │     │+MUL/DIV│      │+mis-   │     │ DIV    │     │ CLINT  │   │Spike   │   │+ADR-080│ │
│  │        │     │(Wave 3)│      │predict.│     │(Wave 5)│     │+FreeRTOS   │ lockstep   │商业化   │ │
│  └────────┘     └────────┘      └────────┘     └────────┘     └────────┘   └────────┘   └────────┘ │
│      │              │                │              │              │            │            │    │
│  ADR-040 v2.0  ADR-049         ADR-046+FSM    ADR-082        ADR-070~076  ADR-080    ADR-079           │
│  CH_MEM 双模   PADDR 真消费    CSR/exception  negotiate      S/U+AMO+     RRIP+PMP+  chip-selector      │
│  elaboration   +MemoryInter-   +4-way LRU     capability     BTB+PLIC/    Spike      商业化             │
│  +Verilog 生成  face+cycle-    +mispredict    +MUL/DIV FSM   CLINT        lockstep                       │
│               precision       recovery                      +FreeRTOS                                      │
│               +mfc multi-                                                                                 │
│               cycle+mmu-                                                                                  │
│               config-json                                                                                 │
│                                                                                                            │
│  通用架构能力演进（7 版本累积）:                                                                            │
│  ├─ Phase 6d: CH_MEM elaboration substrate + Verilator 后端 + 多周期 FSM 框架                                │
│  ├─ v0.8.0:   ADR-049 PADDR-first 真消费 + cycle-accurate 仿真 + 多周期 Plugin 框架                          │
│  ├─ v0.9.0:   CSR/exception 完整 + 4-way LRU + mispredict recovery + Phase 1.5 毕业                          │
│  ├─ v0.10.0:  ADR-082 negotiate + MUL/DIV FSM（真 radix-2）+ RV32C + ICache + Zicsr/Zifencei                 │
│  ├─ v1.0.0:   S/U mode + RV32A + BTB/GShare/RAS + PLIC/CLINT + FreeRTOS demo                                │
│  ├─ v1.2.0:   4-way RRIP + PMP + Debug（gdbstub）+ Spike lockstep + Linux-sim SOFT                          │
│  └─ v1.3.0:   Linux-on-FPGA + chip-selector CLI（ADR-079）+ ADR-080 双模对拍硬门禁                            │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 1. Phase 6d 架构目标（2026 Q4，5-stage Pipeline CH_MEM 端到端）

**核心**: 把 Phase 6c 的"框架能力"（CH_MEM elaboration + Verilog 生成 + Simulator）在**真实业务代码**上完整化，从"PoC 单元级" → "端到端 riscv-tests RV32I 5 指令 tohost=1"。

**子阶段拆分**（来自 `docs/roadmap/phases/phase-6d-rtl-verification.md` §2）:

| 子阶段 | 内容 | 工时 | 依赖 |
|--------|------|------|------|
| 6d.1 | DecoderPlugin 完整 CH_MEM | 1 周 | 6d prereqs |
| 6d.2 | BranchPlugin + HazardPlugin 完整 CH_MEM | 1.5 周 | 6d prereqs + 6d.1 |
| 6d.3 | CpuFactoryChmem 完整 5-stage 集成（IF/ID/EX/MEM/WB + 4 plugin 正确 stage wiring） | 1.5 周 | 6d.1 + 6d.2 |
| 6d.4 | riscv-tests RV32I 5 指令（add/addi/auipc/jal/beq）端到端 `tohost=1` CppHDL sim | 2 周 | 6d.3 + riscv64 工具链 |
| 6d.5 | Verilator 后端集成（Verilog → Verilator 编译 → VL1Cache/VRegFile） | 1.5 周 | 6d.4 + Verilator |
| 6d.6 | MMU/PTW 多周期 FSM（`chlib::ch_state_machine` + `CF_PLUGIN_USE_FSM_EXEMPT`，sv32 5 状态 IDLE→L0_WAIT→L1_WAIT→DONE→FAULT）| 1 周 | 6d.5 + ADR-046 |
| 6d.7 | L1Cache refill FSM（同 ch_state_machine，4 状态 IDLE→LOOKUP→MISS→REFILL_WAIT）| 0.5 周 | 6d.6 |
| 6d.8 | Harness 迁移（`pb.run()` → `pb.elaborate()` + CppHDL sim/Verilator runner）| 1 周 | 6d.4 |
| **总计** | | **10-12 周** | — |

**退出标准**（最小 M6d.1 = E1-E6 + 标准 M6d.2 = E7-E14）:

#### 1.1 最小退出标准 (M6d.1, "5-stage Pipeline CH_MEM 端到端")

| # | 标准 | 验证命令 |
|---|------|----------|
| E1 | `m4_poc_5stage_simulator_tick` 升级为完整 5-stage 版本 | `./bin/chipforge_tests_chmem "m4_poc_5stage_full_simulator"` |
| E2 | DecoderPlugin/BranchPlugin/HazardPlugin 完整 CH_MEM 实现 | `git log` + `tests/cpu/test_cpu_5stage.cpp` PASS |
| E3 | riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` CppHDL sim 端到端 PASS | `./bin/chipforge_tests_chmem "riscv_tests_rv32ui_*"` |
| E4 | 生成 `cpu.v` 含 5-stage 完整 Verilog | `ch::toVerilog("cpu.v", ctx)` 输出含 5 个 stage `always_ff` |
| E5 | 8/8 `check_plugin_portability.sh` 仍 PASS（含新增 Check 9: `_chmem.h` 必须有 `CF_PLUGIN_USE_FSM_EXEMPT` 标注 if FSM）| `bash tools/check_plugin_portability.sh` |
| E6 | `verify_adr.sh` 0 FAILED + `verify_plugin_decision.sh` PASS | 同上 |

#### 1.2 标准退出标准 (M6d.2, "+ Verilator + MMU FSM")

| # | 标准 | 验证命令 |
|---|------|----------|
| E7 | Verilator 编译 `cpu.v` 无 error | `verilator --lint-only cpu.v` |
| E8 | Verilator sim 跑 riscv-tests add.elf `tohost=1` 与 CppHDL sim byte-equal | TLM↔Verilog 对比 trace |
| E9 | MMU PTW 5 状态 FSM (`chlib::ch_state_machine`) 实装 + `CF_PLUGIN_USE_FSM_EXEMPT` 标记 | `ip/cpu/plugins/mmu_ptw_chmem.h` |
| E10 | L1Cache refill FSM (4 状态) 实装 | `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` |
| E11 | Harness 切换 (`tools/cpu_sim/main.cpp` → CppHDL sim runner) + 保留 CLI 兼容 | `tools/cpu_sim --elf add.elf` 跑 CppHDL sim |
| E12 | `chipforge_tests` TLM baseline 0 回归（v0.10.0 基线 419 PASS / 1 known FAIL / 420 total）| `ctest -R chipforge_tests` |
| E13 | `chipforge_tests_chmem` 完整 5-stage + riscv-tests + Verilator 全 PASS | `ctest -R chipforge_tests_chmem` |
| E14 | CHANGELOG v0.10.0/v0.11.0 entry + ADR-037 v2.0 Accepted 状态 + ADR-040 v3.0 (Verilator 集成段) | `CHANGELOG.md` + `docs/architecture/adr.md` |

---

## 2. v0.8.0 架构目标（2026-12 中旬，Wave 3-mmu-real-memory-and-cycle）

**核心**: 让 MMU 在 CPU 流水线中**真起作用**（IBus/DBus 真消费 `pl::PADDR`）+ cycle-accurate 仿真 + MUL/DIV 多周期 FSM。

### 2.1 Pipeline 真消费 PADDR（ADR-049 D1=A）

```
IF  → IBusPlugin 真消费 pl::PADDR (ADR-049 D1=A)
       ↓ if (paddr_valid_) mem_->read_word(pl::PADDR)
       ↓ else fallback pl::PC
ID  → DecoderPlugin (Phase 6d 沉淀)
EX  → IntAluPlugin / BranchPlugin / HazardPlugin
MEM → DBusPlugin 真消费 pl::PADDR (对称)
WB  → RegFilePlugin writeback
     + MUL/DIV Plugin (mfc supersede, 多模板 LATENCY)
```

### 2.2 MMUPlugin 真内存走线（ADR-049 D2+D3+D4）

```cpp
MMUPlugin(SvMode, TLBConfig[], PTWConfig, MemoryInterface* mem = nullptr)
  - mem == nullptr → advance_from_stub()  // 旧 47 测试
  - mem != nullptr → advance_from_real_memory(mem_)  // 新
  - + paddr_valid_ explicit flag (新增 Payload key)
  - ADR-048 canonical ordering: MMU before IBus
```

### 2.3 5 个并行轨道

| 轨道 | change | 内容 | 工时 |
|------|--------|------|------|
| L4 vendor | — | rv32um M 扩展 ELF（mul/mulh/div/... 8 个）| 1-2 天 |
| P1#3 主路径 | `mmu-paddr-consume-and-real-memory` | IBus/DBus 真消费 PADDR + PTW 真内存 | 2.5-3 周 |
| P1#4 并行 | `plugin-framework-cycle-precision` | `pb.run(N)` 真 cycle 数（已 🗑️ archive 2026-10-08, Phase F optional 降级）| 1-2 周 |
| P1#5 SUPERSEDED | `cpu-pipeline-multi-cycle` | 被 wave5 `mfc-cpu-pipeline-multi-cycle-fsm` 取代（FSM 化 + ADR-082 negotiate 集成）| — |
| P1#6 并行 | `mmu-config-json-driven` | MMU 配置 JSON 化（已 archive 2026-10-08，由 mmu-chmem-pipeline-integration 承担）| ≤1 周 |

**验证**:
- `[cpu-l1-mmu-demo]` ≥8/8（含 ≥2 个新 PADDR 传播用例）
- ≥3/5 MUL/DIV ELF 0% diff（cycle-identical）
- riscv-tests ≥40/40 + 0 新 fail
- 3 门禁全 PASS

---

## 3. v0.9.0 架构目标（2027-02 下旬，Wave 4-csr-cache-dse）

**核心**: Phase 1.5 毕业标准达成（RV32I ≥85% PASS + SoC demo ≥5 ELF + cache-dse-sweep CSV + D4+ADR-040+ADR-044+ADR-045+ADR-048+ADR-049 全合规）+ CSR/exception 完整 + 4-way Cache。

### 3.1 phase-1.5-wave-4（P2#6, 4-6 周）

- **RiscV CSR Plugin**（扩展当前 RiscvCsrPlugin stub）: mstatus / mtvec / mepc / mcause / mtval / satp
- **exception 路由**（RiscV spec §1.6）: `CtrlLink::throw_when` 真实消费者，mmu_exit hook 真生效，exception 12/13/15（page fault/load fault/store）路由完整
- **mispredict recovery**: `CtrlLink::flush_when` 真实消费者，branch recovery + flush ROB，PC ← branch_target

### 3.2 cache-phase1.5-4way（P2#7, 3-4 周，cycle-identical 5 ELF）

- **L1CachePlugin 4-way LRU 升级**:
  - Set: 256 → 64（4-way）
  - Way: 1 → 4
  - Replacement: Direct-mapped → LRU
- + TLM+CH_MEM 双轨（ADR-040 v2.0）
- + cache-dse-sweep CSV（size × assoc × repl × line）
- + ADR-044 §2.5 VIPT 正式安全
- **E8 约束**: 与 Phase 6d 6d.5 baseline byte-equal

**验证**:
- RV32I ≥ 85% riscv-tests PASS
- SoC demo ≥ 5 ELF tohost=1
- cache-dse-sweep CSV 落盘
- D4 + 6 个 ADR 全合规

---

## 4. v0.10.0 架构目标（2027 Q1，Wave 5-isa-coverage-and-bp）

**核心**: RV32IMAC + RV32C + ICache + Zicsr/Zifencei + MUL/DIV FSM + ADR-082 negotiate capability

### 4.1 Plugin 框架扩展: ADR-082 negotiate(CapabilityTable&)

```cpp
pb.build() 编排:
  1. 注册所有 Plugin
  2. 拓扑排序 (基于 negotiate 输出的 requires 边)
  3. for each Plugin:
     cap_table = new CapabilityTable
     plugin->negotiate(cap_table) ← NEW 钩子
     if plugin->validate() returns err → throw
  4. for each Plugin in topo order: build()

CI 第 8 条门禁: build() 内 dynamic_cast = 0
```

### 4.2 5-stage Pipeline（CH_MEM 完整，来自 Phase 6d）

- IF → IBusPlugin / DBusPlugin（CH_MEM，来自 Phase 6d）
- ID → DecoderPlugin（完整 RV32IMAC 主 opcode 7+funct3+funct7）
- EX → IntAluPlugin + BranchPlugin + HazardPlugin（CH_MEM）
- WB → RegFilePlugin（32 ch_reg，CH_MEM）

### 4.3 关键能力

| 能力 | 来源 | 关联 PoC |
|------|------|---------|
| MUL/DIV FSM（真 radix-2）| mfc Phase H | PoC-1 |
| RV32C 16-bit 压缩 | ADR-070 | PoC-2 |
| ICache ≥90% 命中 | ADR-040 v3.0 | PoC-3 |
| Zicsr/Zifencei | Zicsr ext | — |
| MUL/DIV FSM 实例契约 | ADR-082 | — |

**ADR-082 negotiate 实例**:
- `MulDivFsmPlugin::negotiate()` → `cap.provide<MCFHandle>("multi_cycle_fsm")` + `cap.require<FlushBroad>(...)` + `cap.require<WBArbiter>(...)`
- `BranchPlugin::negotiate()` → `cap.provide<FlushBroad>("flush_broadcaster")`
- `HazardPlugin::negotiate()` → `cap.provide<WBArbiter>(...)`

---

## 5. v1.0.0 架构目标（2027 Q3，Wave 6-linux-and-productization §1）

**核心**: S/U mode 完整 + RV32A 原子操作 + 分支预测 BTB+GShare+RAS + PLIC/CLINT 中断 + FreeRTOS 验证

### 5.1 Privilege Architecture（S-mode + U-mode 完整）

```
M-mode (machine)
  ↑ mret / ecall / trap entry
S-mode (supervisor) ← FreeRTOS / Linux kernel 运行
  ↑ sret
U-mode (user) ← Application 运行
```

- + satp.MODE = sv32（依赖 v0.9.0 ADR-049 §后续项 JSON 化）
- + sstatus/sepc/scause/stvec CSR 完整

### 5.2 关键能力

| 能力 | 来源 | 关联 PoC |
|------|------|---------|
| RV32A atomic（LR/SC + AMO* 6 类）| — | PoC-5 |
| BTB 4K-entry + GShare 13-bit + RAS 8-entry | — | PoC-6 |
| PLIC + CLINT 中断控制 | — | PoC-7 |
| FreeRTOS demo 10M cycle 稳定 | — | PoC-7 |
| **HARD 门禁**: FPGA CoreMark/MHz 与 VexiiRiscv single-issue 偏差 ≤10% | 2026-09-29 新增 | **PoC-14** |

---

## 6. v1.2.0 架构目标（2028 Q2，验证基建版）

**核心**: 4-way RRIP Cache + PMP 内存保护 + Debug（gdbstub）+ Spike lockstep 对拍 + Linux-sim shell（SOFT 交付）

### 6.1 4-way RRIP Cache（PoC-8，替换 LRU）

```
Cache Line (4 ways × N sets)
  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
  │ Way 0  │ │ Way 1  │ │ Way 2  │ │ Way 3  │
  │ tag    │ │ tag    │ │ tag    │ │ tag    │
  │ RRPV   │ │ RRPV   │ │ RRPV   │ │ RRPV   │
  │ 2-bit  │ │ 2-bit  │ │ 2-bit  │ │ 2-bit  │
  └────────┘ └────────┘ └────────┘ └────────┘
RRIP 算法: hit → RRPV=0; miss → RRPV=RRIP_MAX
           periodic → RRPV++ (aging)
E8 约束: 与 Phase 6d 6d.5 baseline byte-equal
```

### 6.2 PMP（Physical Memory Protection）

- 8/16 regions
- pmpcfg0-3（4 × 8-bit region config）
- pmpaddr0-15（16 × physical address）
- M-mode: 配置；S/U-mode: 受限访问
- 阻止 S-mode 访问 CLINT/PLIC 寄存器空间

### 6.3 Spike lockstep 对拍（PoC-9，HARD 门禁）

```
ChipForge sim ──────┐
(TLM/CH_MEM)       ├──► Trace Comparator ──► 1M 条
                   │   (指令 + PC + reg state) 零分歧
Spike ISS ─────────┘
(黄金参考)         允许已记录分歧 ≤ 5/1M
```

### 6.4 gdbstub（PoC-10，RISC-V Debug Spec 0.13）

- CPU Pipeline ──── Debug Transport Module ──── TCP socket（halt/resume/step）
- 断点 / 单步 / 寄存器读写 / 内存读写

---

## 7. v1.3.0 架构目标（2029 Q1，产品化版）

**核心**: Linux-on-FPGA 端到端 + chip-selector CLI 商业化工具 + ADR-080 双模对拍硬门禁

### 7.1 chip-selector CLI（PoC-11，商业化核心，ADR-079）

```bash
$ chipforge select --isa rv32imac --cache 4way \
                   --mmu sv32 --perf-target 100MHz

┌─────────────┐    ┌─────────────┐    ┌────────────┐
│ 8 配置 TLM  │ ─► │  Pareto     │ ─► │ chip.toml  │
│ 并行扫描    │    │ 前沿计算    │    │ + Verilog  │
│ < 10 min    │    │ CoreMark×   │    │ bundle     │
│             │    │ FMAX×资源   │    │            │
└─────────────┘    └─────────────┘    └────────────┘
```

**ADR-079 chip-selector 契约**:
- 输入: ISA/Cache/MMU/性能目标 JSON/YAML
- 输出: Pareto 最优配置 + chip.toml + 可综合 Verilog bundle
- 约束: TLM↔CH_MEM IPC 偏差 ≤15%（R3 PoC-11 触发条件）

### 7.2 Linux-on-FPGA 端到端

```
FPGA 板级 (Digilent/VexRiscv 风格 board bundle)
  ├─ CPU (CH_MEM 编译产出)
  ├─ MMU (sv32 + PTW FSM, 来自 Phase 6d 6d.6)
  ├─ Cache (4-way RRIP, 来自 v1.2.0)
  ├─ PLIC/CLINT (来自 v1.0.0)
  ├─ PMP (来自 v1.2.0)
  ├─ Debug (gdbstub, 来自 v1.2.0)
  └─ DDR controller + UART + VirtIO (新)
OpenSBI → Linux → Shell 实际跑在 FPGA
FMAX ≥ 100 MHz
```

### 7.3 ADR-080 转硬门禁（TLM↔CH_MEM 双模对拍）

- v1.2.0 试点 → v1.3.0 强制
- chip-selector 产出必须 TLM ↔ CH_MEM byte-equal
- PoC-11 验证: 8 配置 TLM <10min + IPC 偏差 ≤15%

---

## 8. 跨版本节点架构演进对比表

| 架构维度 | Phase 6d | v0.8.0 | v0.9.0 | v0.10.0 | v1.0.0 | v1.2.0 | v1.3.0 |
|---------|---------|--------|--------|---------|--------|--------|--------|
| **指令集** | RV32I（5 指令）| RV32I + rv32um | + RV32I ≥85% | RV32IMAC + RV32C | + RV32A | 同 v1.0.0 | + RV32F (soft-float) |
| **Privilege** | M-only | M-only | M-only | M-only | M+S+U | 同 v1.0.0 | + PMP |
| **CSR** | 无 | 无 | mstatus/mtvec/mepc/mcause/mtval | + mcycle/minstret/Zicsr | + sstatus/sepc/scause/stvec | + pmpcfg/pmpaddr | + debug CSR |
| **分支预测** | B-type 6-op | 同 Phase 6d | + flush ROB recovery | Static | BTB + GShare + RAS | 同 v1.0.0 | 同 v1.0.0 |
| **Cache** | L1Cache 256×1 | 同 Phase 6d | 4-way LRU | L1D 4-way (LRU) | 同 v0.10.0 | 4-way RRIP | + ICache |
| **MMU** | sv32 PTW FSM (5 状态) | PADDR-first 真消费 | + JSON 配置驱动 | sv32 (v0.9.0 沉淀) | + S-mode satp | 同 v1.0.0 | 同 v1.0.0 |
| **Cycle 精度** | CppHDL sim + Verilator | + cycle counter | 持续完善 | 持续完善 | 持续完善 | 持续完善 | 持续完善 |
| **多周期** | MMU/PTW + L1Cache refill FSM | MUL/DIV 多模板 (mfc supersede) | 持续完善 | MUL/DIV 真 radix-2 FSM | 持续完善 | 持续完善 | 持续完善 |
| **异常** | CtrlLink::throw_when stub | stub | 12/13/15 路由完整 + trap delivery | 持续完善 | 持续完善 | 持续完善 | 持续完善 |
| **中断** | 无 | 无 | 无 | 无 | PLIC + CLINT | 同 v1.0.0 | 同 v1.0.0 |
| **调试** | 无 | 无 | 无 | 无 | 无 | gdbstub + Debug 0.13 | 同 v1.2.0 |
| **对拍** | 无 | 无 | 无 | 无 | 无 | Spike lockstep HARD | + ADR-080 硬门禁 |
| **VIPT 安全** | ADR-044 §2.5 风险标记 | 同 Phase 6d | 正式锁 (ADR-044 §2.5 实施) | 持续完善 | 持续完善 | 持续完善 | 持续完善 |
| **DSE** | 无 | 无 | cache-dse-sweep CSV | 持续完善 | 持续完善 | 持续完善 | chip-selector 商业化 |
| **工具链** | Verilator + riscv64 集成 | + cycle-precision 框架 | 持续完善 | + ADR-082 negotiate | 持续完善 | 持续完善 | + FPGA board bundle |
| **OS 验证** | riscv-tests 5 指令 | riscv-tests 40/40 + rv32um 0/8 | RV32I ≥85% | riscv-tests 100% | FreeRTOS demo | Linux-sim SOFT | Linux-on-FPGA HARD |
| **退出标准** | 8 指令 tohost=1 + Verilator byte-equal | [cpu-l1-mmu-demo] ≥8/8 + MUL/DIV ≥3/5 | Phase 1.5 毕业 | rv32ui+um+uc 100% / DMIPS ≥1.4 | rv32ua+si 100% / CoreMark ≥2.3 | lockstep 1M 零分歧 HARD | FPGA CoreMark ≥2.5 + ADR-080 |
| **核心 PoC** | — | — | — | PoC-1/2/3 | PoC-4/5/6/7/14 | PoC-8/9/10 | PoC-11/14 复测 |
| **CI 门禁数** | 8 → 9 | 9 | 9 | 9 → 11 | 11 | 11 (lockstep HARD) | 11 (+ ADR-080) |
| **Plugin 框架** | ADR-040 v2.0 + ADR-046 | + ADR-049 PADDR-first | + ADR-048 canonical ordering (v0.7.0) | + ADR-082 negotiate | 同 v0.10.0 | 同 v0.10.0 | 同 v0.10.0 |

---

## 9. 验收与同步机制

### 9.1 CI 门禁时间轴

> **编号双轨说明**：`adr-matrix.md` §6 用行号 1–11 列 11 条门禁（含 4 条 "各 PoC 相关" 占位）；历史命名 "第 8/10/11 条" 与 §6 行号错位（如 §6 行 5 = "第 8 条"，行 4 = "第 10 条"，行 10 = "第 11 条"）。本文档 §9.1 引用统一改用 **§6 行号**，避免双编号歧义。

11 条 CI 门禁（按 Phase 6d → v1.3.0 累积生效）:

| §6 行号 | 门禁 | 引入版本 | 验证命令 |
|--------|------|---------|---------|
| 行 1-4 | D4 决策 1-4（无 void tick / 无 enum class state / Bundle 字段用 uint_t / at_stage 闭包内禁早返）| Phase 0+ | `bash tools/verify_plugin_decision.sh` |
| 行 5 | `check_plugin_portability.sh` Check 5（at_stage 闭包内禁运行期 `if(ch_bool)`）| ADR-040 v2.0 | `bash tools/check_plugin_portability.sh` |
| 行 6 | C++23 编译标志强制（4 CMake 文件禁 cxx_std_17/20）| v0.6 ADR-047 | `check_plugin_portability.sh` Check 12 |
| 行 7 | static_assert 运行时断言（canonical ordering）| v0.7.0 ADR-048 | `[cpu-integration]` 4 测试 |
| 行 8 | build() 内 dynamic_cast = 0 | v1.0.0 PoC-4 决策 1 | `check_plugin_portability.sh` Check |
| 行 9 | 核内禁 `#ifdef FPGA` | v1.0.0 PoC-4 决策 2 | `check_plugin_portability.sh` Check |
| 行 10 | 静态配置头文件 throw 禁止 | v0.6 ADR-047 | `check_plugin_portability.sh` Check 10 |
| 行 11 | ADR-080 TLM↔CH_MEM byte-equal 硬门禁 | v1.3.0 §3.5.5 | `check_plugin_portability.sh` Check + 4-way RRIP E8 + Spike lockstep E9 |

### 9.2 OpenSpec 同步规则

- 任何 PoC 状态变更 → 同步本文件 §1-§7 + `.rddf/roadmap/objectives/` 决策门 + `openspec/changes/*/tasks.md` checkbox
- 任何 ADR 编号变更 → 同步本文件 §1-§7 ADR 引用 + `docs/architecture/adr.md` 注册表

### 9.3 文档同步规则

- 任何架构变更 → 同步本文件 + `.rddf/roadmap/strategy.md` §6 版本节点表
- 任何 PoC 完成/失败 → 同步本文件 §4-§7 + `.rddf/roadmap/objectives/objective-*.md` §11 跟踪台账

---

## 10. 关联文档

- 战略 SSOT: [`.rddf/roadmap/strategy.md`](../../.rddf/roadmap/strategy.md)（A+C Hybrid 战略选择 + 版本节点 + Go/No-Go）
- 实施规划: [`.rddf/roadmap/`](../../.rddf/roadmap/)（phases/features/objectives）
- 决策门禁: `.rddf/roadmap/objectives/objective-*.md` §9.2 Go/No-Go
- 跟踪台账: `.rddf/roadmap/objectives/objective-*.md` §11 append-only
- 任务执行: `openspec/changes/*/tasks.md`
- ADR 注册表: [`docs/architecture/adr.md`](../architecture/adr.md)
- AGENTS.md 已知测试状态: `AGENTS.md`（数字维护原则，不双写）

---

## 11. 变更日志

| 日期 | 版本 | 变更 | 来源 |
|------|------|------|------|
| 2026-10-09 | v1.0 | 由 `docs/roadmap/strategy/execution-roadmap.md §6` + `soc/cpu/docs/roadmap/execution-roadmap.md §3.5` + `docs/roadmap/phases/phase-6d-rtl-verification.md` 合并去重而来（约 30% 重叠描述删除）| rdd-workflow 迁移 |
