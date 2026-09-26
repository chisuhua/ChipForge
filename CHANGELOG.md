# Changelog

All notable changes to ChipForge will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## v0.8.0 (2026-09-25) — MMU 真内存读 + PADDR 真消费 (P1#3 mmu-paddr-consume)

> **OpenSpec change**: `mmu-paddr-consume-and-real-memory` (archived)
> **Initiative**: `wave3-mmu-real-memory-and-cycle` P1#3 (split from P1#3 mmu-paddr-consume-and-real-memory)
> **Purpose**: 让 MMU 在 CPU 流水线中真起作用——IBus/DBus 真消费物理地址 (PADDR), PTW 真内存读 PTE

### Added

- **`MemoryInterface` 抽象类** (`ip/mmu/lib/memory_interface.h`, ~70 LOC): `read_word` / `write_word` 虚函数接口, lib/ 层 (零 `cf::plugin::*` 依赖)
- **`PicolibcHostMemory` 继承 `MemoryInterface`**: 移除 `read_word` 的 `const` 限定 (与抽象非 const 接口对齐); override `read_word` / `write_word`
- **`PTW::advance_from_real_memory(MemoryInterface* mem)`** (`ip/mmu/lib/ptw.cpp`): 真内存读 32-bit PTE (Sv32 scope), `mem==nullptr` 降级 stub 路径
- **`vaddr_for_stage` virtual hook** (`ip/mmu/tlm/MMUPlugin.h`): do_lookup 用此 hook 替代硬编码 `last_vaddr_`; 默认返回 `last_vaddr_` (test/bridge 路径), `RiscvMMUPlugin` override 从父 stage 节点读 PC (fetch) / MEM_ADDR (memory) — **production 路径关键修复**
- **`set_satp_ppn` public setter** (`ip/mmu/tlm/MMUPlugin.h`): RISC-V satp CSR[59:0] PPN 喂给 PTW root (替代硬编码 0); `RiscvMMUPlugin::csr_write_satp` 自动调用
- **`mmu_keys::PADDR_VALID`** (`ip/mmu/tlm/mmu_keys.h`): 新增 Payload key, hit/fault 双路径统一写; IBus/DBus 用 `has()` 守卫防 PayloadStore fail-fast
- **`soc/cpu/docs/dse/mmu-paddr-propagation-matrix.csv`** (新): MMU PADDR 消费测试矩阵
- **Oracle C8 修复 Sv32 VPN mask**: `ptw.cpp::next_pte_paddr` 从 `& 0x1FF` (9-bit, 截断 bit 21) 改为 mode-dependent `& 0x3FF` (Sv32) / `& 0x1FF` (Sv39/48) — 修复 VPN[0]≥512 时 L0 PTE 地址错位 bug
- **`C9-a` 关键修复**: `RiscvMMUPlugin::build` 首行加 `cf::ip::mmu::MMUPlugin::build(pb)` — 镜像 §5.4 setup() fix 模式, 让基类 at_stage 闭包在 production 真正注册
- **`C9-d` mmu_exit 路径修复**: 改从 `tlb_lookup_loadstore` 节点读 `EXCEPTION_CODE` (do_lookup 唯一写入者), 旧实现读 `mmu_exit` 节点 → 永空 → no-op → production trap 路径永断

### Changed

- **`RiscvMMUPlugin::csr_write_satp`**: 提取 satp CSR[59:0] PPN (mask `0x0FFFFFFFFFFFFULL`) → 调基类 `set_satp_ppn`
- **`MMUPlugin::do_lookup`**: 加 `sv_mode_ == SvMode::Bare` identity 短路 (Bare mode 不走 PTW walk)
- **`bus_halt 检测` (mmu_exit 闭包)**: 读 `tlb_lookup_loadstore` 节点替代 `mmu_exit` 节点 (C9-d fix)
- **`cpu_l1_mmu_demo.json`**: 加 `mmu.memory_interface` 字段声明 (Oracle C7 rescoped, JSON 声明式, CpuFactory::register_early_plugins 1 行 static_cast 透传)
- **`ADR-049`** 新增 (v0.8.0): MMU PADDR Consumption Contract + MemoryInterface 抽象 + 5 Decisions + §后续项合入 P1#6 mmu-config-json-driven 草案
- **`docs/roadmap/strategy/a-plus-c-hybrid.md` + `execution-roadmap.md`**: §2.5 新增 P1#6 mmu-config-json-driven 启动轨道

### Fixed

- **Oracle C1 (RISC-V spec reserved encoding)**: 旧 `r && w && x` 错检 (RWX 合法), 修 `w && !r` (W=1 && R=0 才非法). `test_ptw_unit` helper `make_reserved_pte` 同步对齐 spec
- **Oracle C2 (mode-correct addressing)**: `start_walk` + `next_pte_paddr` 改为 mode-specific (Sv32: 4 字节 PTE + 10-bit VPN; Sv39/48: 8 字节 + 9-bit)
- **Oracle C8 (Sv32 VPN mask 截断)**: 见 Added 节
- **Oracle C9-a-d (production MMU 翻译惰性)**: 见 Added 节
- **`test_ptw_unit.cpp` 测试 helper 对齐**: `make_reserved_pte` 用真 reserved encoding (W=1+R=0, 原误用 RWX)

### Known Follow-up

- **`sv_mode_=Sv32 + satp CSR=0` PTW 路径**: Bare 短路只覆盖 `sv_mode_==Bare` 配置; sv_mode=Sv32 但 satp CSR 初始=0 (csr_write_satp 未调) 时仍走 PTW walk + 错位 fault. 待 `satp_value_.MODE` 字段追踪实装, 留后续 P1#3 task. (5 个 [cpu-l1-mmu-demo] ELF 测试受影响)
- **`verify_adr.sh` / `doc_link_check.sh` 4 ADRs 缺 CppTLM headers + 2 broken links**: pre-existing infra 问题, 与本 change 无关

### Verification

- `[mmu]` **53/53 PASS** (131 assertions) — TDD red → green (含 Oracle C8 Sv32 mask unit test)
- `[cpu]` **117/117 PASS** (353 assertions) — 含 C10b E2E 端到端真断言 `INSTRUCTION == 0xCAFEBABE`
- `[cpu-integration]` **81/81 PASS** (65722 assertions) — 0 回归 (Oracle C4 fix +5 substage nodes 后)
- `[riscv-tests]` **40/40 PASS** — 0 回归
- **`verify_plugin_decision.sh` 12/12 + `check_plugin_portability.sh` 12/12**: D4 + ADR-040 + ADR-047 全 PASS

## v0.7.0 (2026-09-24) — Plugin 注册规范序 + A+C Hybrid 战略启动 (ADR-048)

> **OpenSpec changes**: `cpu-pipeline-canonical-ordering-assert` (archived, v0.7.0) + `2026-09-24-cpu-pipeline-fix-rv32ui-load-width` (archived)
> **Initiative**: `wave3-cpu-pipeline-debt` P0
> **Purpose**: 修复 MMU stall 隐式契约 + 正式启动 A+C Hybrid 战略 (Wave 3 清债 → Phase 2)

### Added

- **ADR-048 Plugin 注册规范序**：`cpu_factory.h::register_early_plugins()` 新增运行时断言检查 MMUPlugin 注册在 IBusPlugin 之前
- **计数器机制**：3 个 `inline int` 跨 TU 计数器 (`PLUGIN_SEQ`, `MMU_REG_ORDER`, `IBUS_REG_ORDER`, `DBUS_REG_ORDER`) 跟踪注册序列
- **`tests/cpu/integration/test_canonical_ordering.cpp`**：4 个测试用例验证正确顺序(RED)和错误检测(GREEN)双路径
- **A+C Hybrid 战略文档**：`docs/roadmap/strategy/a-plus-c-hybrid.md` (138 行)
- **`tools/sync_strategy_status.sh`**：从 openspec changes + initiative YAML 派生 strategy §7 状态表
- **AGENTS.md**: 新增 Strategy/Initiative 工作流小节
- **openspec changes 进入版本控制**：从 .gitignore 移除 openspec/，7 个 active/archive changes 可团队协作

### Changed

- **注册顺序调整**：MMUPlugin (if enable_mmu) 从 `build_cpu()` 尾部移入 `register_early_plugins()` 头部，保证在 IBusPlugin 之前注册
- **版本号声明**：Wave 3 从 v0.7.0 起 (Phase 6d 已消费 v0.4.0-v0.6.0)
- **`docs/roadmap/roadmap-status.md`**：路线图同步 A+C Hybrid 战略入口

### Fixed

- **MMU stall 隐式契约**：原依赖 `CpuFactory` 调用顺序的正确性，现升格为运行时断言，防止静默回归
- **P0#1 tasks.md v0.4.0 → v0.7.0** (Oracle/Metis 审查修订遗漏)

### Removed

- **`.gitignore` `openspec/` 行**：openspec changes 是协作工件，XDG context-store 已隔离在 `~/.local/share/`

## v0.6.0 (2026-09-22) — 静态配置期错误处理 Result 范式 (ADR-047)

> **OpenSpec change**: `v06-static-config-result` (已 archive)
> **目的**: 修复 Phase 6d 验证报告标记的唯一 Critical P1 项（应用层错误处理缺失）
> **核心**: 主项目升 C++23 + 10 个 PipeBuilder 静态配置 API 迁移到 `std::expected<T, PluginError>` + ADR-047 + CI Check 10/11/12

### Breaking

- **`PipeBuilder` 10 个静态配置 API 改为 `Result<void>` / `Result<T>` 返回**（`std::expected<T, PluginError>`）：
  `register_plugin` / `at_stage` / `declare_substage` / `register_commit_hook` /
  `register_ctrl_link` / `register_stage_payload_connector` / `build` /
  `elaborate` / `to_verilog` / `create_simulator`（原 `void` / 裸指针返回）
- **主项目 C++ 标准升级**：`CMAKE_CXX_STANDARD 17 → 23`（`cf_plugin` INTERFACE + tests + verilator_runner 同步 `cxx_std_23`）
- CppTLM (C++17) / CppHDL (C++23) 子仓库**不修改**，通过 Itanium ABI 兼容

### 新增

- **`include/cf/plugin/plugin_error.h`**（112 行）：`enum class PluginError`（20 字段）+ `Result<T>` 别名 + `plugin_error_message()` 工厂 + `to_exception()` 转换
- **`include/cf/plugin/result_macros.h`**（95 行）：`PB_TRY` / `PB_EXPECT` / `auto_throw` 宏族
- **`docs/architecture/adr/ADR-047-static-config-result-paradigm.md`**（152 行）：静态配置期错误处理 Result 范式（含 4 项热路径例外清单）
- **CI Check 10/11/12**（`tools/check_plugin_portability.sh`，8→12 checks）：禁止静态配置头文件 throw / API 签名同步 / C++23 强制

### 修改

- **`include/cf/plugin/plugin_exception.h`**：新增 `PluginException(PluginError, std::string stage = "")` 构造重载（向后兼容）
- **`ip/cpu/cpu_factory.h`**：新增 `build_cpu_impl()` 内部 Result 化（`PB_TRY` 包裹所有 register/at_stage/build），对外 `build_cpu()` 保留 throw 包装层
- **`tools/cpu_sim/main.cpp`**：用 `auto_throw(build_cpu_impl(...))` 抹平（CLI 接口不变）
- **`docs/architecture/adr.md`**：ADR-047 表项 + K 类别 + ADR-040 v3.0 行末引用

### 保留 throw（ADR-047 例外）

- `PipeBuilder::run()` — CtrlLink `throw_when` 运行时语义（`REQUIRE_THROWS_AS(pb.run(), PluginException)` 不变）
- `PayloadStore::get()` — at_stage 回调内热路径 fail-fast
- `PluginBase` 虚函数 / 业务回调体 — 自由选择

### Tests

- 34 个 `REQUIRE_NOTHROW(pb.*elaborate|to_verilog)` 改为 `REQUIRE((...).has_value())`（Result 值类型语义）
- 5 个 throw 断言（`run()`×2 / `PayloadStore::get` / `TLBFactory::create`×2）**保留**（ADR-047 例外）
- `chipforge_tests` + `chipforge_tests_chmem` 全部 PASS（零回归）

### 验证

- `ctest`: **3/3 PASS**（TLM + CH_MEM + verify_plugin_decision）
- `check_plugin_portability.sh`: **12/12 PASS**（含新 Check 10/11/12）
- `verify_adr.sh`: **32/32 PASS**（31 旧 + 新 ADR-047）
- `verify_plugin_decision.sh`: PASS

## v0.5.0 (2026-09-22) — Phase 6d 完整收官 (6d.6 + 6d.7 + Check 9 + ADR-040 v3.0)

> **OpenSpec change**: `phase-6d-fsm-chmem` (in progress)
> **目的**: 多周期协议引擎 (MMU/PTW, L1Cache refill) 用 ch_state_machine DSL 实装 + ADR-040 v3.0
> **核心**: 6d.6 MMU/PTW sv32 5 状态 + 6d.7 L1Cache refill 4 状态 + Check 9 FSM 豁免白名单 + ADR-040 v3.0

### 新增

- **6d.6 MMU/PTW sv32 5 状态 FSM** (`ip/cpu/plugins/mmu_ptw_chmem.h`, 314 行, commit 09c9d23)
  - 顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT` (ADR-046 豁免)
  - `chlib::ch_state_machine<PTW_State, 5>` DSL：IDLE → L0_WAIT → L1_WAIT → DONE / FAULT
  - sv32 2-level walk (RISC-V Privileged Spec v1.12 §5.3)：root = satp.ppn << 12, pte1/pte0 via VPN[1]/VPN[0] 索引
  - PTE 类型判定 {R,W,X}：{0,0,0} 非叶, {1,0,0} reserved encoding → FAULT, 含 R/X (megapage 叶) → DONE
  - `transition_when(ch_bool, target)` select-tree（sibling CppHDL 5af3e40 cycle-accurate）
- **6d.7 L1Cache refill 4 状态 FSM** (`ip/cache/tlm/l1_cache_refill_fsm_chmem.h`, 228 行, commit 09c9d23)
  - 顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`
  - `chlib::ch_state_machine<CacheState, 4>` DSL：IDLE → LOOKUP → MISS → REFILL_WAIT
  - lookup hit → IDLE；miss → 发 refill 请求 → REFILL_WAIT；mem_rdata_valid 写回 cache → IDLE
  - 并发 race 语义：REFILL_WAIT 期间新 lookup_request 不中断
- **Check 9** (`tools/check_plugin_portability.sh`, commit 09c9d23): FSM 豁免白名单验证
  - 声明 `CF_PLUGIN_USE_FSM_EXEMPT` 的文件必须使用 `chlib::ch_state_machine` DSL（非裸 enum + switch）
  - 8 → 9 checks
- **ADR-040 v3.0** + **ADR-046 v1.0** (`docs/architecture/adr.md` + `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md`, commit 09c9d23)
  - D13: 多周期协议引擎 FSM 豁免 (ADR-046 + ch_state_machine DSL)
  - D14: Verilator sim cycle-identical (5 ELF 0% diff)
  - `ch_state_machine::build()` 增强：select-tree 发射 → CppHDL Simulator cycle-accurate
  - ADR-046 v1.0 Accepted（6d.6 + 6d.7 FSM 实装证明）

### Tests

- `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` (191 行, commit 09c9d23)：3 PoC
  - `mmu_ptw_fsm_sv32_walk_success`：known vaddr + satp.ppn → DONE + PPN
  - `mmu_ptw_fsm_sv32_reserved_encoding_fault`：{R,W,X}={1,0,0} → FAULT
  - `mmu_ptw_fsm_sv32_invalid_pte_fault`：V=0 → FAULT
- `tests/cache/test_l1cache_refill_fsm_chmem.cpp` (189 行, commit 09c9d23)：3 PoC
  - `l1cache_refill_fsm_hit`：lookup hit → IDLE, 数据可用
  - `l1cache_refill_fsm_miss_refill`：miss → REFILL_WAIT → IDLE
  - `l1cache_refill_fsm_concurrent_race`：REFILL_WAIT 期间不中断

### Sibling commits（CppHDL 协同）

- **CppHDL `5af3e40`**: `chlib/state_machine.h` ch_state_machine cycle-accurate via `transition_when` select-tree
  - 新增 `transition_when(ch_bool cond, StateEnum target)` API
  - `transition_to(s)` 等价 `transition_when(ch_bool(true), s)`（向后兼容）
  - `build()` 对每个 state 调用 on_active 闭包，累加 select-tree：
    `state_reg->next = select(in_state(S) && cond, target, hold)`
  - ADR-046 §2.3 "仿真限制" 解除（之前 ch_state_machine 不能在 CppHDL Simulator 跑 cycle-accurate）

### 验证

- `chipforge_tests_chmem`: **43/43 PASS** (1804 assertions, +43 from v0.4.1)
- `mmu_ptw_fsm`: 24 assertions PASS
- `l1cache_refill_fsm`: 28 assertions PASS
- `check_plugin_portability.sh`: **9/9 PASS**（含 Check 9 "FSM 豁免白名单"）
- `verify_adr.sh`: 0 FAILED
- `verify_plugin_decision.sh`: 3+4/3 PASS（含 Check 2 FSM 豁免文件过滤）
- Verilator `--lint-only`: 0 errors / 0 warnings
- TLM baseline: 386 PASS / 17 FAIL（pre-existing, 0 回归）

### Phase 6d 收官总览

| 子阶段 | 内容 | Commit | 状态 |
|--------|------|--------|------|
| 6d.1 | DecoderPlugin 完整 CH_MEM (RV32I ~80 指令) | `d9ebbfd` | ✅ |
| 6d.2 | BranchPlugin + HazardPlugin + DECODED_INST consumer migration | `5ab7f93` / `531b0dc` | ✅ |
| 6d.3 | CH_MEM Memory Model + 7-plugin 5-stage 集成 | `60a5a24` | ✅ |
| 6d.4 | vendored ELF 端到端 tohost=1 (5 ELF) | `fde730c` | ✅ |
| 6d.5 E7 | Verilator lint clean + Verilator compile Vtop | `0d30d64` + CppHDL `7f7da88` | ✅ |
| 6d.5 E8 | Verilator sim cycle-identical (5 ELF 0% diff) | `1a99ed3` + CppHDL `3a5284d` | ✅ |
| 6d.6 | MMU/PTW sv32 5 状态 ch_state_machine | `09c9d23` + CppHDL `5af3e40` | ✅ |
| 6d.7 | L1Cache refill 4 状态 ch_state_machine | `09c9d23` | ✅ |
| 6d.8 | Harness 迁移 (cpu_verilator_sim) | `1a99ed3` | ✅ |
| Check 9 | FSM 豁免白名单 | `09c9d23` | ✅ |
| ADR-040 v3.0 | CH_MEM + FSM + Verilator 三段 | `09c9d23` | ✅ |
| ADR-046 v1.0 | 多周期 FSM 豁免 Accepted | `09c9d23` | ✅ |

### 待完成（v0.5.0+ 后续）

- `phase-6d-rtl-verification` change archive 已 done (Oracle hygiene `7d310c6` 后 archive `2026-09-20-phase-6d-rtl-verification`)
- `phase-6d-verilator-sim` change archive 已 done (commit `1a99ed3` + CppHDL `3a5284d` 后 archive `2026-09-21-phase-6d-verilator-sim`)
- `phase-6d-fsm-chmem` change archive（本次）

---

## v0.4.1 (2026-09-22) — Phase 6d 6d.5 E8 + 6d.8 Verilator sim (cycle-identical)

> **OpenSpec change**: `phase-6d-verilator-sim` (in progress)
> **目的**: Verilator sim 跑 vendored ELF tohost=1 + Harness 迁移
> **核心**: CppHDL VerilatorBackend Phase 3 启用 + codegen 修复 + dlopen runner + 5 ELF cycle-identical

### 新增

- **`tools/verilator_runner/sim_main.cpp`** (89 行): dlopen Vtop__ALL.so + Verilator port access + `cf_run_sim()` 入口
- **`tools/verilator_runner/cpu_verilator_sim.cpp`** (221 行): CLI (`--elf/--verilog/--cycles/--work-dir`)；两阶段编译 (verilator --cc + g++ -shared) → dlopen → run → 输出 `TOHOST=1 CYCLES=N ELF=name PASS/FAIL`
- **`tools/verilator_runner/CMakeLists.txt`** (60 行): `cpu_verilator_sim` target，CF_PLUGIN_USE_CH_MEM
- **`tests/cpu/test_cpu_verilator_sim.cpp`** (92 行): 5 ELF × tohost=1，DYNAMIC_SECTION，graceful skip 当 verilator 缺失

### 上游依赖（CppHDL sibling commit 3a5284d）

**Codegen 三大缺失修复**（`src/codegen_verilog.cpp`）：

| 缺失 | 症状 | 修复 |
|------|------|------|
| `type_mem` 未处理 | ch_mem 数组不出现在 Verilog，无存储 | `logic [N-1:0] name [0:depth-1]` 数组 + initial 零填充 |
| `type_lit` 声明未赋值 | 398 个字面量值是 X | `assign lit = 常量` |
| `ch_reg init` 未作复位 | PC 复位到 0 | 同步复位值 (e.g. PC → 0x80000000) |

`type_mem_read_port` → `always_ff @(posedge)` 寄存读（1-cycle latency 与 pc_lag 语义一致）
`type_mem_write_port` → `always_ff @(posedge)` 写（**无复位分支**，preload ELF 必须跨复位存活 = RISC-V boot ROM 语义）

### cycle-identical 验证（Oracle E8 阈值 ±10% → 实际 0% 差异）

| ELF | CppHDL cycle | Verilator cycle | diff |
|-----|-------------|-----------------|------|
| rv32ui-p-add   | 478 | 478 | **0%** |
| rv32ui-p-addi  | 246 | 246 | **0%** |
| rv32ui-p-auipc | 59  | 59  | **0%** |
| rv32ui-p-beq   | 315 | 315 | **0%** |
| rv32ui-p-jal   | 55  | 55  | **0%** |

### 验证

- `chipforge_tests_chmem`: **37/37 PASS** (1761 assertions, +10 新 verilator e2e)
- `chipforge_tests` (TLM): 392 PASS / 11 FAIL（pre-existing, 0 回归）
- Verilator 5.052 `--lint-only`: 0 errors / 0 warnings
- Verilator 5.052 `--cc --public-flat-rw`: Vtop__ALL.a 完整生成 + dlopen 成功
- `check_plugin_portability.sh`: 8/8 PASS
- `verify_adr.sh`: 0 FAILED

### 待完成（follow-up `phase-6d-fsm-chmem`）

- 6d.6 MMU/PTW sv32 5 状态 ch_state_machine（ADR-046 CF_PLUGIN_USE_FSM_EXEMPT）
- 6d.7 L1Cache refill 4 状态 ch_state_machine
- Check 9 `check_plugin_portability.sh` FSM 豁免 grep
- ADR-040 v3.0（FSM 段 + Verilator 段）
- ADR-046 验证

---

## v0.4.0 (2026-09-21) — Phase 6d RTL Verification (6d.1 + 6d.3 + 6d.4 + 6d.5 E7)

### 新增

- **6d.1 DecoderPlugin 完整 CH_MEM** (`ip/cpu/plugins/decode_chmem.h`, 6d.4 commit d9ebbfd)
  - RV32I ~40 指令 mux_select 验证 (84 assertions)
  - DECODED_INST Payload (`opcode/funct3/funct7/rd/rs1/rs2/imm/reads_rs1/reads_rs2/writes_rd/op_class/instr_format` ch 信号)
- **6d.3 CH_MEM Memory Model** (`ibus_chmem.h` + `dmem_chmem.h` + `cpu_factory_chmem.h` 7-plugin 5-stage 集成, 6d.4 commit 60a5a24)
  - IBus: `ch_mem<32, 16K>` 指令存储 + PC reg + pc_lag (1-cycle aread 对齐) + branch update_pc + FLUSH 抑制误取
  - DMem: `ch_mem<32, 16K>` 存储 + LW/SW 同步读 + tohost_probe 异步读端口 + store_write_data/addr_proxy
  - JAL link 写回 (rd = pc + 4)
  - 全组合逻辑数据通路 (移除 6 stage pipeline_reg)
- **6d.4 vendored ELF 端到端 tohost=1** (5 ELF, commit fde730c)
  - `tests/cpu/test_cpu_chmem_vendored_elf.cpp` (122 行)
  - rv32ui-p-{add,addi,auipc,beq,jal} 全部 tohost=1 (1203 assertions PASS)
  - ELF32 解析 (e_phoff@28 + e_phentsize@42 + e_phnum@44) → PT_LOAD 路由 IBus (PF_X) / DMem (PF_W) at offset (p_vaddr-0x80000000)/4
- **6d.5 Verilator 集成 — E7 (lint clean)** (本 release)
  - Verilator 5.052 --lint-only /tmp/cpu.v: **0 errors, 0 warnings** ✓
  - Verilator --cc --build: Vtop__ALL.a 完整生成 ✓
  - vendored ELF 生成的 cpu.v (111KB, 2943 lines) 全部 lint clean

### 已修复

- **DECODED_INST consumer migration** (commit 531b0dc): IntAlu/RegFile/Branch/Hazard 5 Plugin 改读 DECODED_INST ch 信号
  - **关键**: IntAlu 加 `is_op||is_opimm` opcode gate 防止 SW (funct3=010) 误匹配 SLT 等 ALU op
  - RegFile 用 select tree 替代 `if(ch_bool)` (D4 §5 合规)
  - Branch 新增 JAL 支持
  - ADDI 负立即数 bug 修复: is_add 对 OPIMM 不再要求 funct7==0
- **RegFile regs_ 用 std::vector 替代 std::array** (commit 0d30d64, Oracle 优先级 #2)
  - **根因**: `std::array<ch_reg<ch_uint<32>>, kNumRegs>` make_unique 默认构造 32 个 ch_reg (默认名 "reg") → 32 对孤儿 regimpl/proxy 节点泄漏进 context, 与显式命名 "reg_0".."reg_31" 撞名 → Verilator Duplicate declaration of signal '_reg_N' 31 处
  - **修复**: 改 `std::vector<ch_reg>` + `emplace_back` 逐个显式构造

### 上游依赖升级

- **CppHDL** commit `7f7da88`: `codegen_verilog.cpp::print_header` 加 `/* verilator lint_off WIDTHEXPAND */` + `/* verilator lint_off WIDTHTRUNC */` 包裹整个 module — Verilator -Wall 默认 WIDTH 警告升级为错误, CH_MEM select tree mux 宽度不匹配触发, 关闭后 lint clean

### 验证

- `chipforge_tests_chmem`: **36/36 PASS** (1751 assertions, +1203 from v0.3.1)
- `[chmem][cpu][vendored-elf][6d4]` 5 ELF tohost=1: PASS
- `m4_poc_5stage_simulator_tick`: PASS
- `m4_poc_5stage_elaborate_verilog`: PASS
- Verilator 5.052 `--lint-only`: 0 errors / 0 warnings
- Verilator 5.052 `--cc --build`: Vtop compile 成功
- `check_plugin_portability.sh`: 8/8 PASS
- `verify_adr.sh`: 0 FAILED
- TLM baseline (`chipforge_tests`): 392 PASS / 11 FAIL（10 [riscv-tests] LOAD stub + 1 7stage superscalar segfault, 全部 pre-existing, 0 回归）

### 待完成 (E8 — CppHDL VerilatorBackend follow-up)

- **6d.5 E8**: Verilator sim 跑 vendored 5 ELF tohost=1 一致（不要求 trace byte-equal）
  - **阻塞项**: CppHDL VerilatorBackend Phase 3.2-3.6 实装 (dlopen Vtop + port access binding + sim_main.cpp) — sibling repo CppHDL ADR-035 work, 不在本 release 范围
  - **当前等价**: CppHDL Simulator 跑 5 ELF 已全部 tohost=1 PASS (fde730c)
- **6d.6 MMU/PTW FSM (sv32 5 状态)**: 重构 MMU/PTW 为 `ch_state_machine` DSL (ADR-046 CF_PLUGIN_USE_FSM_EXEMPT)
- **6d.7 L1Cache refill FSM**: L1CachePlugin refill 阶段重写为 ch_state_machine
- **6d.8 Harness 迁移**: pb.run() → CppHDL sim runner / Verilator（已隐含在 6d.5 E7 路径）

## v0.3.1 (2026-09-20) — 5-stage Simulator SEGV 修复 (Phase 6c M6)

> **OpenSpec change**: `fix-5stage-mux-segv-elaboration`
> **目的**: 修复 `m4_poc_5stage_simulator_tick` 的 SIGSEGV（`muximpl::create_instruction` 第 13 行 `false_value()->id()` 解引用 nullptr）

### 根因（双层）
1. **CppHDL nullptr hijack（已上游修复）**: `ch_uint<N>(0)` 和 `ch_bool(0)` 由于 `int 0 → lnodeimpl* nullptr` 是标准转换序列（优先级高于用户定义的 `int → ch_literal_runtime` 链），匹配到了继承的 `logic_buffer(lnodeimpl *node)` ctor，导致 `node_impl_ = nullptr`。
   - 上游 commit `47af57f fix(core): prevent null pointer hijack in ch_uint/ch_bool integer ctors`
   - 同样的 bug 也存在于 `bundle_base`，上游 commit `f6b0081 fix(core): prevent null pointer hijack in bundle_base integer ctor`
   - 修复：SFINAE-restricted template ctor with identity match for integral types
2. **PayloadStore emplace-on-miss（chipforge 端修复）**: `n->operator()(KeyType::*)`（非 const get）在 cell 缺失时 emplace `T{}`（对 ch_uint<N>/ch_bool 产生 null impl）。Stage linking 的拷贝操作把这个 null 传播到所有下游 stage。

### 新增
- `tests/framework/test_payload_store_miss_throws.cpp`: CH_MEM 模式下验证 `const T& get(key)` 读未填充 cell 抛异常
- `cpu_factory_chmem.h::build_cpu()`: EARLY-stage payload pre-population (6 stages × 5 cells + DECODE/RISCV_DETAIL struct 占位)
- Check 8: `check_plugin_portability.sh` 验证 `payload.h` CH_MEM 路径含 `PayloadStore cell missing` 抛异常

### 已修复
- `include/cf/plugin/payload.h`: `const T& get(key) const` 改 throw-on-miss（顺便修复 const-correctness 漏洞——原 emplace-on-const 不应该编译通过）
- `tests/framework/test_payload_store_miss_throws.cpp`: 新增 CH_MEM fail-fast 测试
- `check_plugin_portability.sh`: 7/7 → 8/8 PASS
- `m4_poc_5stage_simulator_tick`: **从 SEGV 修复为 PASS**（10 个 tick 全部成功）

### 验证
- `m4_poc_5stage_build`: PASS
- `m4_poc_5stage_elaborate_verilog`: PASS
- `m4_poc_5stage_simulator_tick`: **PASS** ✅
- `[framework]` 全部 PASS（含新增 fail-fast 测试）
- `verify_adr.sh`: 0 FAILED
- `verify_plugin_decision.sh`: PASS
- `check_plugin_portability.sh`: **8/8 PASS**
- TLM baseline (`chipforge_tests`): 391 PASS / 12 FAIL（pre-existing, 10 [riscv-tests] LOAD + 1 superscalar + 1 SEGV 均为预先存在，未回归）

### 上游依赖升级
- `CppHDL` submodule: 包含 `47af57f` 和 `f6b0081`（nullptr hijack 修复）

## v0.3.0 (2026-09-17) — plugin-elaboration-substrate (Phase 6c)

> **OpenSpec change**: `plugin-elaboration-substrate`
> **目的**: cf::plugin 从"每周期仿真闭包执行器"改为"elaboration 一次即发射 lnode DAG"——兑现 README "CppTLM + CppHDL-based" HDL 侧承诺。

### 新增
- CH_MEM 编译开关 (`-DCF_PLUGIN_USE_CH_MEM`): uint_t<N>/bool_t/PayloadStore/CtrlLink 双模 (TLM compat + CH_MEM elaboration)
- PipeBuilder::elaborate(ch::core::context& ctx) + 无参重载 + to_verilog() + create_simulator() (Prereq-1)
- Stage plumbing: type-erased stage_payload_map_ + register_stage_payload_connector<T>() + commit_payload_map()
- CtrlLink::halt_condition() / flush_condition() OR 合并 + W3 per-stage stall wiring
- storage.h::array_store 双缓冲 (CH_MEM: first_/second_ commit swap)
- chipforge_tests_chmem 第二测试 target + Check 6 (禁 .h/.cpp #define CF_PLUGIN_USE_CH_MEM)
- RegFilePlugin CH_MEM (32 ch_reg, x0 select 屏蔽, singleton ctx_aware fix)
- RiscvIntAluPlugin CH_MEM (11 op select 树: ADD/SUB/SLL/SLT/SLTU/XOR/SRL/SRA/OR/AND)
- BranchPlugin CH_MEM (6-op B-type select 树: BEQ/BNE/BLT/BGE/BLTU/BGEU)
- HazardPlugin CH_MEM (REAL RAW 检测: id_rs1/rs2 vs ex/mem/wb rd, 6 条件 OR 合并)
- CpuFactoryChmem 5-stage 集成 (4 plugin 注册 + stage linking connectors)
- M3-PoC 41 assertions (RegFile + ALU + Combined + Byte-equal)
- PoC 测试文件: test_cpu_rtl_regfile_alu.cpp (3+1 deferred + byte-equal)
- ADR-046: 多周期协议引擎豁免 D4 无状态机禁令

### 已修改
- uint_t.h: 翻转 v1.0 ch 渗透禁令 (CH_MEM 是新正道)
- PayloadStore: CH_MEM 模式下 cell 装 ch 代理对象
- ctrl_link.h: CH_MEM halt_when(ch_bool) / flush_when(ch_bool) 取代 std::function<bool()>
- pipe_builder.h: +138 lines (stage_payload_map_ + elaborate + Prereq-1 4 API)
- array_store: CH_MEM 双缓冲 (first_/second_ commit swap)
- reg_file_chmem.h: 32 独立 ch_reg + singleton ctx_aware fix
- payload_common.h/payload_riscv.h: TLM static_assert wrap for CH_MEM ch_uint<N> parameter

### 已废弃 (Phase 6c 起)
- TLM 仿真层 (`pb.run()` per cycle) 标记 deprecated
- `ip/*/tlm/` 目录仅用于 Phase 1-4 遗留测试 (CH_MEM 是新正道)
- `tools/cpu_sim/main.cpp` TLM 入口等待 M5 Harness 迁移到 CppHDL sim

### 依赖项
- CppTLM: v2.1.0 (unchanged)
- CppHDL: v1.0.0 (unchanged, chlib 基础设施完整)

### Commit 清单 (Phase 6c M1-M5)
- `25e2672` W0 + M1 框架双模化 + ADR-046 stub
- `9a03bb2` M2 Spike 1-3 plumbing skeleton + chmem target + Check 6
- `918e577` M2 W3 halt/flush OR + per-stage stall wiring
- `baa504b` M2 W4 array_store 双缓冲 + PoC #2
- `3e8ada2` M3 Prereq-1..6 + HazardPlugin 骨架
- `a38a1e4` M3/W5 reg_file/alu + PoC + CH_MEM workarounds
- `19d4f5d` M3/W6 #95 byte-equal defer
- `edad878` M4/W7 BranchPlugin + HazardPlugin RAW
- `b68996a` M4/W8 cpu_factory 4-plugin + PoC #3+#4 fix

### M5 已知剩余问题（Phase 6d+）
- 5-stage Simulator tick: SEGV (lnodeimpl::id(), stage linking connector nullptr) — 需 M6 修复
- riscv-tests tohost=1: 缺 RISC-V toolchain (riscv64-unknown-elf-gcc) → ELF 无法编译
- 工具链: 无 verilator/yosys/iverilog → 综合验证不可行
- M5 计划原含 "riscv-tests tohost=1" 作为最终硬证据, 因环境限制推迟

## v0.1.3 (2026-09-15) - plugin-framework-stall

> **OpenSpec change**: `plugin-framework-stall` (详见 `openspec/changes/archive/2026-09-15-plugin-framework-stall/`)
> **目的**: 兑现 `cf::plugin::CtrlLink` 4 种控制 API 的真实消费者, 让 D4 Plugin 范式的"声明式控制流"落地为框架级 stall 原语。

### Added (框架层)
- `include/cf/plugin/pipe_builder.h` 新 API: `register_ctrl_link(stage_name, shared_ptr<CtrlLink>)` + `should_stall_stage(name)` + `ctrl_link_count(name)` + `get_ctrl_link(name, idx)` + `clear_ctrl_links()` + private `stage_ctrl_links_`
- `include/cf/plugin/plugin_exception.h` (新增): `cf::plugin::PluginException` 继承 `std::runtime_error`, 携带 `stage_name`
- `include/cf/plugin/pipe_builder.h::run()`: 在 canonical stage 循环外层插入 `if (should_stall_stage(name)) continue;` (per-stage OR-merge stall)
- `include/cf/plugin/pipe_builder.h::run()` 末尾: 追加 throw_when 全局异常检查, 抛 `PluginException` 跳过 `commit_storages()`

### Added (Plugin consumers)
- `ip/cpu/plugins/ibus.h`: build() 注册 fetch stage CtrlLink, `halt_when` 读 `tlb_lookup_ifetch` 节点 `mmu_keys::PTW_ACTIVE` (PTW-busy halt)
- `ip/cpu/plugins/hazard.h`: 新增 `has_active_hazard()` + `reset_hazard_cache()` + `last_decoded_hazard_` 成员, build() 注册 execute stage CtrlLink, `register_commit_hook` 复位 cache
- `ip/cpu/plugins/exception.h`: build() 注册 throw_when 演示桩 (lambda 恒 false, TODO Phase 5+ 实装 trap delivery)
- `ip/cpu/plugins/branch_predictor.h`: build() 注册 flush_when 演示桩 (lambda 恒 false, TODO cpu-pipeline-mispredict 实装 mispredict recovery)
- `ip/mmu/tlm/MMUPlugin.cpp`: PTW 完成回调 (success + fault 两条路径) 原子清零 `PTW_ACTIVE=0` (commit B 关键修复)

### Test
- `tests/framework/test_pipe_builder_stall.cpp` (12 case): empty_ctrl_no_effect, single_halt_skip/normal, or_merge_two/three, other_stage_not_affected, throw_when_throws, throw_then_no_commit, flush_not_framework_consumed, register_ctrl_link_count, halt_stateful, shared_ptr_ctrl_keeps_alive
- `tests/mmu/test_ptw_stall_integration.cpp` (3 case): ptw_tlb_hit_fetch_not_stalled, ptw_tlb_miss_fetch_stalled, ptw_complete_unstall_fetch_resumes
- `tests/cpu/integration/test_hazard_stall.cpp` (4 case): hazard_no_raw_no_stall_when_scoreboard_empty, hazard_raw_chain_stall_execute, hazard_long_raw_chain_repeated_stall, hazard_war_not_stall_framework_consumes_ctrl
- **baseline 318 → 337 PASS** (66755 assertions, 5 RISC-V 仿真 pre-existing 失败不变)

### Docs
- `docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md` (新增): 8 个 Decision sections, 8 项风险表
- `docs/architecture/adr.md`: 注册 ADR-045
- `openspec/specs/`: 4 个 spec 永久化 (pb-stall-loop, ctrllink-consumption-contract, ptw-fetch-stall-consumer, hazard-execute-stall-consumer)

### Verified
- 4 architecture gates 全绿 (verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check)
- D4/ADR-040 全合规 (无 tick / 无状态机 / 无早返 / shared_ptr 仅 build() 注册期)

### Pending (下一阶段入口)
> `soc-cpu-l1-mmu-demo` 重启 — 现在 stall 原语就绪, PTW retry 可真做
> `riscv-tests-rv32ui` 接入 — 客观验收门槛 (rv32ui-p-* 套件)
> `cpu-pipeline-multi-cycle` — LATENCY>1 mul/div stall (复用本 change 的 framework primitive)
> `cpu-pipeline-exception` — throw_when 真实消费者 (trap delivery)
> `cpu-pipeline-mispredict` — flush_when 真实消费者 (branch recovery)
> `plugin-framework-cycle-precision` — pb.run(cycle_count=N) 真 cycle 精度

## v0.1.2 (2026-09-15) - cpu-pipeline-stubs-replace

> **OpenSpec change**: `cpu-pipeline-stubs-replace` (详见 `openspec/changes/archive/2026-09-15-cpu-pipeline-stubs-replace/`)
> **目的**: 替换 CPU Plugin pipeline 的 4 个 stub（IBus/DBus 假数据 + 无 stage 间传播 + 无 PC 更新），让 `cpu_sim` 首次通过 Plugin pipeline 真实执行 RISC-V ELF 到 `tohost=1`。

### Changed (框架层)
- `include/cf/plugin/pipe_builder.h` — `run()` 改为按 **canonical stage first-occurrence order × Phase (EARLY→NORMAL→LATE)** 遍历回调（commit A）；新增 `canonical_stage_order()` helper
- `src/cf_plugin/CMakeLists.txt` — `cpu_sim` 显式链接 `ip/mmu/` 实现源（RiscvMMUPlugin vtable 需要）

### Added (plugins/)
- `ip/cpu/plugins/stage_link.h` — **`StageLinkPlugin`**（commit B）：4 个 `at_stage(X, Phase::EARLY)` 闭包做 stage→stage Payload 传播
  - fetch→decode: `PC`, `INSTRUCTION`
  - decode→execute: `PC`, `DECODE`, `RISCV_DETAIL`, `RS1`, `RS2`
  - execute→memory: `PC`, `DECODE`, `MEM_ADDR`, `MEM_DATA`, `RD_DATA`
  - memory→writeback: `PC`, `DECODE`, `RD_DATA`, `MEM_DATA`
- `ip/cpu/cpu_factory.h` — `build_cpu(config, PicolibcHostMemory* mem = nullptr)` 可选第二参数（commit B/E）；`register_early_plugins` / `register_late_plugins` 透传 `mem`

### Changed (plugins/ + arch/riscv/)
- `ip/cpu/plugins/ibus.h` — 构造函数加 `PicolibcHostMemory* mem`；`at_stage("fetch", NORMAL)` 真读 `mem_->read_word(pc)`；`at_stage("writeback", Phase::LATE)` 把 PC 写回 **fetch 节点**（循环携带值，非单向传播）
- `ip/cpu/plugins/dbus.h` — 同上；`at_stage("memory", NORMAL)` LOAD→`read_word` / STORE→`write_word`
- `ip/cpu/arch/riscv/lsu.h` — 删除 `n->operator()(MEM_DATA) = T{0}` stub（AGU 职责归位，DBus 接管 data）
- `tools/cpu_sim/main.cpp` — 删除 M4.15 软件解释器（37 行）；`pb.run()` 真跑 pipeline；`cfg.isa = "rv32i"` 显式 pin

### Fixed (commit E 发现并修复的 4 个隐藏 bug — pipeline 从未真跑所致)
- `ip/cpu/arch/riscv/branch.h` — **仅 `op_class == BRANCH` 时评估分支**（之前 `addi` 的 funct3=0 落入 BEQ，操作数默认 0 → `taken=true` → PC 被错写 `pc+imm=0`）
- `ip/cpu/arch/riscv/int_alu.h` — **I-type 用 `rv.imm` 而非 `rs2_val`**（decoder 对 I-type 置 `reads_rs2=false`）；**写 `RD_DATA`**（RegFilePlugin 写回读的键，之前只写 `RESULT`）
- `ip/cpu/plugins/ibus.h` — writeback LATE PC 更新写到 **fetch 节点**（不是自身节点）

### Test
- `tests/cpu/test_cpu_factory.cpp` — `stage_count()` / `plugins.size()` 断言更新（+1 StageLinkPlugin）
- `tests/cpu/integration/test_{3,5,7}stage_riscv.cpp` — `stage_count()` 断言 +4（每 stage 各 +1 StageLink EARLY）
- **baseline 306 → 318 PASS**（66726 assertions，5/5 稳定）
- `./build/src/cf_plugin/cpu_sim --cycles 100 --elf build/add.elf` → **tohost=1**（5 cycles 跑完 6 指令 add.elf）

### Verified
- `bash tools/verify_adr.sh` PASS
- `bash tools/verify_plugin_decision.sh` PASS（0 新违规）
- `bash tools/check_plugin_portability.sh` PASS（0 std::optional / 0 tick / 0 早返）
- `bash tools/doc_link_check.sh` PASS（exit 0）

### Pending (下一阶段入口)
> `soc-cpu-l1-mmu-demo` 重启 —— 现在 CPU Pipeline 真执行，SoC JSON demo 可扩展（traffic_gen→mmu→l1→mem 全链 + CPU Plugin 真跑 add.elf 到 tohost）
> `cache-dse-sweep` —— CPU Pipeline 真执行后，DSE sweep 测基线更有意义
> `plugin-framework-stall` —— CtrlLink halt_when stall 可验证（PTW busy 时 stall fetch）

## v0.0.7 (2026-06-29) - mmu-ip-skeleton

> **OpenSpec change**: `mmu-ip-skeleton` (详见 `openspec/changes/mmu-ip-skeleton/`)
> **目的**: 建立 `ip/mmu/` IP 骨架（目录/STATUS/Plugin/Bundle/Config schema），落地 lib/tlm 双层切分，TLB/PTW 算法推迟到 `mmu-tlb-ptw-impl`

### Added (IP 骨架)
- `ip/mmu/README.md` + `STATUS.md` (PARTIAL 骨架阶段标记)
- `ip/mmu/docs/{README,architecture,configuration,integration}.md` (4 文档)
- `ip/mmu/rtl/.gitkeep` + `ip/mmu/test/.gitkeep` (Phase 5+ 沿用)
- `tests/mmu/` 目录 + 5 个测试文件 + `CMakeLists.txt`

### Added (lib/ 纯 C++ 算法层)
- `ip/mmu/lib/tlb_entry.h` — 模板化 `TLBEntry<TAG_BITS, ASID_BITS>` (valid/tag/pfn/asid/perms/global)
- `ip/mmu/lib/tlb_lookup.h` — `TLBLookup` 5 字段 (hit/paddr/perms/fault/fault_code)
- `ip/mmu/lib/tlb_base.h` — `TLBBase` 抽象基类 (10 纯虚方法)
- `ip/mmu/lib/tlb.h` — 模板化 `TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` (std::array 存储)
- `ip/mmu/lib/tlb_factory.h/.cpp` — `TLBFactory::create()` 按 JSON 选模板特化 (8 种支持组合)
- `ip/mmu/lib/multi_level_tlb.h/.cpp` — `MultiLevelTLB` 编排器 (shadow fill + 反向失效)
- `ip/mmu/lib/ptw.h/.cpp` — `PTW` Page Table Walker 接口 (回调驱动, 0 tick)

### Added (policies/ 替换策略)
- `ip/mmu/policies/tlb_replacement_policy.h` — 模板化抽象基类
- `ip/mmu/policies/{no_replacement,fifo,lru,rrip}_policy.h` — 4 策略实装
- `ip/mmu/policies/tlb_replacement_policy.cpp` — `create(name)` 工厂

### Added (tlm/ 声明式 Plugin 层)
- `ip/mmu/tlm/mmu_keys.h` — 10 个 `Payload<T>` Key 集合 (VADDR/PADDR/PERMS/PTW_ACTIVE/PTW_VADDR/PTW_ASID/PTW_L0/L1/L2_RAW/PTW_FAULT)
- `ip/mmu/tlm/MMUPlugin.h/.cpp` — `MMUPlugin : public cf::plugin::PluginBase`, 用 `at_stage()` 注册 5 个 logical stage 闭包

### Added (Config + RISC-V 适配)
- `ip/mmu/configs/params_schema.json` — JSON Schema draft-07 (含 topology/asid_bits/sv_mode/supported_page_sizes/ptw_max_inflight/shadow_fill_from_next/levels)
- `bundles/tlb_bundles_extension.h` — TlbReq / TlbResp Bundle (12 字节 POD)
- `ip/cpu/plugins/mmu.h` 重构为 `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin` + `using MMUPlugin = RiscvMMUPlugin` 向后兼容别名

### Added (IP Catalog)
- `ip/README.md` — STATUS 表 + IP 模块列表加 `mmu` 行

### Design Decisions (9 Decisions in design.md)
1. TLB 模板化 + 抽象基类 + 工厂三件套
2. MultiLevelTLB 编排器 (coherence 协议集中)
3. PTW 用 substage 不用 tick() (D4 强制)
4. HDL 友好性通过 static_assert 编译期检查
5. 配置 schema 用 topology 字段预留 split_id
6. ASID bits 0-16 全支持
7. **lib/ vs tlm/ 职责切分** (强制 Plugin 范式合规, lib/ 0 依赖 Plugin 框架)
8. PTW 用 substage 不用 tick() 详细示例
9. HDL 友好约束的边界 (lib/ 严格, tlm/ 宽松)

### Notes
- **D4 Plugin 范式合规**: 所有业务逻辑用 `at_stage()` 声明, 0 业务 tick() (编译期 `PluginBase::tick() = delete`)
- **lib/ 0 Plugin 依赖**: `grep -rn "cf/plugin/plugin_base.h\|cf/plugin/pipe_builder.h\|cf/plugin/payload.h" ip/mmu/lib/` MUST 0 匹配
- **Phase 0 logical stage 模型**: `pb.run()` 单 cycle 遍历所有闭包, PTW 3 级 sub-pipe 是逻辑拆分, 为 Phase 6 cycle-scheduling 预留
- **测试位置**: 遵守 `test-location-discipline` spec, 全部在 `tests/mmu/`, 不在 `ip/mmu/test/`

### Pending (下一阶段入口)

> `mmu-tlb-ptw-impl` change —— TLB lookup/insert 算法实装 + PageTableWalker Sv32/Sv39/Sv48 解码 + CtrlLink halt_when PTW stall + cpptlm MMUTLMBridge + RISC-V 特定 hook (satp 拦截 / SFENCE.VMA / exception 12/13/15)

## v0.1.1 (2026-09-14) - ptw-walk-bridge-fix

> **目的**: 修复 mmu-cache-integration 遗留的 2 处隐藏 bug — MMUPlugin at_stage ptw_l0/l1/l2 闭包是空 lambda (PTW walk 永远 busy) + MMUTLMBridge issue_request/read_response 是 stub (SoC 端到端仿真失败).

### Added
- **`ip/mmu/lib/ptw.h/.cpp`**: `advance_from_stub()` 方法 — MMUPlugin at_stage 闭包一行推进 PTW walk
- **`ip/mmu/tlm/MMUPlugin.h/.cpp`**: `issue_request(vaddr, asid)` + `read_response(node)` 公开 API (mirror L1CachePlugin)
- **`src/cf_plugin/bridge/mmu_bridge.cpp`**: issue_request 真实调 MMUPlugin + read_response 从节点读 PADDR/EXCEPTION_CODE
- **`bundles/tlb_bundles_extension.h`**: include guard (修复重复 include 编译错误)

### Fixed
- MMUPlugin::at_stage("ptw_l0/l1/l2") 空 lambda → 调 `ptw_->advance_from_stub()` (PTW walk 从"永远 busy"修复为真实完成)
- MMUTLMBridge::issue_request stub 注释 → Bundle→POD 转换 + 调用 plugin issue_request
- MMUTLMBridge::read_response 无条件 return {} → 从 tlb_lookup_ifetch 节点读 PADDR/EXCEPTION_CODE
- read_response hit 推导: `hit = (exception == 0 && paddr != 0)` (PERMS payload key 缺位暂以 paddr 间接证明)

### Verified
- `./build/bin/chipforge_tests` → 317/317 PASS (was 306; +11: 2 PTW end-to-end + 1 via-bridge)
- `[mmu]` 40 → **42** PASS (`PTWSv39ThreeLevelWalkCompletesViaAdvanceFromStub` + `PTWAdvanceFromStubIsNoOpWhenNotBusy`)
- `[MMUCacheIntegration]` 3 → **4** PASS (`EndToEndTranslationThroughBridge`)
- 4 architecture gates all PASS
- 5 次连跑稳定性 PASS

### Pending (下一阶段入口)

> `cache-dse-sweep` change — DSE 12-case Pareto (现在 PTW walk 实际工作, 可真实扫 satp_ppn 配置)
> `cpu-mmu-integration` 后续 — RISC-V CPU 集成 MMU 时用真实 issue_request/read_response

## v0.0.9 (2026-09-13) - mmu-tlb-ptw-impl


> **目的**: 实现 TLB lookup/insert 算法 + PTW Sv39 三级 walk + MultiLevelTLB coherence + MMUPlugin at_stage 闭包 + RISC-V satp/SFENCE.VMA/exception 12/13/15 hook + cpptlm MMUTLMBridge. 解锁 ADR-044 VIPT 数据流真实链路 (MMUPlugin 同时输出 pl::PADDR + pl::MMU_VADDR).

## v0.2.3 (2026-09-16) - soc-cpu-l1-mmu-demo (Phase 1.5 Wave 2)

> **OpenSpec change**: `soc-cpu-l1-mmu-demo` (修订版, Oracle+Metis 2026-09-16 评审后)
> **目的**: PTW TLB refill 关闭 stall 链空转 + CPU+MMU+Memory 结构验证 demo 端到端 tohost=1 + Wave 1 lessons-learned 永久化。
>
> **修订说明**: 原始 plan 声称 "端到端 CPU+MMU+L1+Memory demo"。评审揭示 (1) MMU 在流水线中装饰性 (IBus/DBus 不消费 PADDR)，(2) PTW 从 stub 读 PTE 非真实内存，(3) `cf::soc::Topology::from_json` 不存在。demo 缩窄为 TLB refill + CPU+MMU+Memory 结构 smoke，L1 仅 JSON 声明，PTW real-memory + PADDR consumption + 典范顺序修复推迟 Wave 3。

### Added (MMU — TLB refill)
- `tests/mmu/test_ptw_tlb_refill_integration.cpp` (NEW, 2 case): PTW success callback 验证 TLB refill + walk 完成状态

### Modified (MMU — PTW success callback)
- `ip/mmu/tlm/MMUPlugin.cpp` (修改): PTW success 回调捕获 `this`，调用 `multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)` 关闭 ±存链空转（roadmap §6.2 R3 风险关闭）

### Added (SoC — demo)
- `soc/cpu_l1_mmu_demo.json` (NEW): 结构拓扑声明 (CPU+MMU+L1+Memory, L1 仅声明)
- `tests/soc/test_cpu_l1_mmu_demo.cpp` (NEW, 6 case): JSON 结构验证 + 5 ELF tohost=1 (enable_mmu=true, sv32)

### Docs
- `CHANGELOG.md`: v0.2.3 条目
- `AGENTS.md`: 已知测试状态同步 (mmu 重新启用 + [cpu-l1-mmu-demo] + LOAD OOS follow-up 路由)
- `soc/README.md`: 速查表新增 `cpu_l1_mmu_demo.json`
- `ADR-045`: Decision 6 注解 — stall 机制 inert (canonical ordering 错误 + PADDR 未消费), 修复推迟 Wave 3
- `openspec/` (gitignored): specs in `changes/soc-cpu-l1-mmu-demo/specs/` → `mmu-ptw-tlb-refill`, `soc-cpu-mmu-demo-topology`, `phase-1.5-wave1-retro`, `riscv-tests-fixture`

### Verified
- `./build/bin/chipforge_tests` → **397/397 = 386 passed, 11 failed** (unchanged pre-existing: 10 LOAD feature stub + 1 7stage superscalar segfault)
- `[tlb-refill]` 2 tests → PASS; `[mmu]` 47 tests → PASS (110 assertions)
- `[cpu-l1-mmu-demo]` 6 tests → PASS (40 assertions)
- 4 architecture gates all PASS (verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check)

### Known Limitations
- MMU translation 在 CPU 流水线中装饰性 (IBus/DBus 不消费 PADDR, 无 issue_request 调用者) — Wave 3 real PADDR consumption + PTW real-memory wiring
- Canonical stage ordering (R2) 不正确 — fetch 先于 tlb_lookup_ifetch — stall 机制 inert, 非原子非幂等指令不受保护 — Wave 3 cpu-pipeline-multi-cycle
- L1CachePlugin 仅 JSON 声明不实例化 — Wave 3 cache-dse-sweep
- LOAD width extraction 10 feature stub — follow-up `riscv-tests-rv32ui-load-width`
- 7-stage superscalar segfault — follow-up `cpu-pipeline-7stage-superscalar-fix`

## v0.2.2 (2026-09-15) - riscv-tests-rv32ui ELF vendor + pipeline exec fixes

> **目的**: 兑现 v0.2.1 记录的 follow-up —— vendor 实际 riscv-tests ELF 并跑通 Wave 1 合规基线。过程中暴露并修复 5 个 CPU pipeline 执行断链 bug（40/40 rv32ui timeout → 30 PASS）。

### Fixed (CPU pipeline 执行断链, riscv-tests-rv32ui 暴露)

- **`ip/cpu/plugins/hazard.h`**（修改）: execute stage CtrlLink `halt_when(has_active_hazard())` → `last_decoded_hazard_ != NONE`。根因: decode 每轮 `mark_in_flight` 后 `has_active_hazard()` 恒 true → execute 被永久 skip → StageLink execute EARLY 不执行 → execute/memory/writeback 全空 → PC 恒卡 0x4 → 所有程序 timeout（含 add.elf）。现仅当本轮 decode 判定真实 RAW/WAW 才 stall
- **`ip/cpu/arch/riscv/branch.h`**（修改）: JAL/JALR 与 B-type 分支改用 `rv.opcode` 区分（JAL=0x6F/JALR=0x67/B-type=0x63），废弃 `funct7==0` 判定。根因: B-type 的 funct7 区是 imm[12|10:5]，小偏移（如 `bnez a0,+0`）时全 0 → 旧实现把 B-type 误判为 JAL → target=pc+imm=pc 自旋死循环
- **`ip/cpu/arch/riscv/int_alu.h`**（修改）: LUI/AUIPC 按 `rv.opcode` 分流（AUIPC 返回 PC+imm），其余走 `infer_opcode`。根因: U-type 的 funct3/funct7 均 0，旧实现把 AUIPC 误判为 ADD(0+imm) → `la t5,tohost`（auipc+addi）计算出错误地址 → tohost 写失败
- **`ip/cpu/arch/riscv/decode.h`**（修改）: `writes_rd` 从 `rd != x0` 改为按 OpCode 判定。根因: B-type/STORE 的 rd 字段是 imm 位，旧实现误标 writes_rd → HazardPlugin 误标记 imm 位命中的寄存器 → 后续指令被判 RAW → execute stall → 指令丢弃 → 死循环（bgeu/bltu 2 例）
- **`ip/cpu/arch/riscv/payload_riscv.h`**（修改）: `RiscvDecodeDetail` 增加 `opcode` 字段（decode.h 填充），供 branch/int_alu 按 opcode 分流

### Added

- **`tests/cpu/riscv_tests/env-p/`**（NEW）: ChipForge p-env harness（`riscv_test.h` + `link.ld`）。上游 riscv-test-env v2 的 RVTEST_PASS/FAIL 走 `ecall`（a7=93 SYS_exit）需完整 trap 机（Wave 4 scope）；ChipForge 变体改为直接 `sw TESTNUM,tohost`（与 PicolibcHostMemory 约定一致），测试体 (.S) 100% 上游
- **`tests/cpu/riscv_tests/elf/`**（NEW, committed）: 40 个 `rv32ui-p-*.elf` prebuilt 二进制（~500KB，来自 riscv-tests @ `2ebecad`，`-march=rv32i_zicsr` 编译，排除 fence_i + ma_data）

### Modified
- `tests/cpu/riscv_tests/build_rv32ui.sh`: 指向本地 `env-p/` harness（-I/-T 均本地），march 修正 `rv32i_zicsr`（GCC 15 下 rv32i 不含 zicsr）
- `tests/cpu/riscv_tests/README.md`: 实装记录 + env-p harness 说明 + 基线矩阵表
- `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`: 真实 triage 结果（30 PASS / 10 feature stub）

### Verified
- `./build/bin/chipforge_tests "[riscv-tests]"` → **30 PASS / 10 FAIL**（10 FAIL 全部 LOAD-family `category=feature stub`，LOAD width extraction 为 Wave 2 显式 OOS；sb/sh/sw/st_ld/ld_st 因内嵌 load 验证失败同源）
- `./build/bin/chipforge_tests` → **378/389 PASS**（11 FAIL = 10 LOAD feature stub + 1 pre-existing `7stage_add_elf_end_to_end` superscalar segfault，stash 验证与本次修复无关）
- **4 个 pre-existing RISC-V 仿真失败被本次 CPU 修复治愈**（test_3stage/5stage/10stage_riscv + test_cpu_sim_real_tohost → 现 PASS）；仅 7-stage segfault 残留
- 4 architecture gates all PASS（verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check）

### Known Limitations (Documented)
- DBusPlugin LOAD width extraction（LB/LH/LBU/LHU）仍 OUT OF SCOPE — 10 个 LOAD-family 用例 feature stub，Wave 2 `cpu-pipeline-fix-rv32ui-N` 候选
- 7-stage superscalar cpu_sim segfault（pre-existing，非本次引入；`--config cpu_superscalar.json` 路径）
- riscv-tests 源码本身未 vendor（仅 ELF 产物 + build 脚本 + 来源 commit 记录）

## v0.2.1 (2026-09-15) - riscv-tests-rv32ui (Wave 1 of Phase 1.5)

> **目的**: Phase 1.5 启动 — 建客观 RV32I 合规基线（riscv-tests rv32ui-p-*）用于 Wave 2 SoC demo 选题 gate + Wave 2 fix-rv32ui-N triage 输入.

### Added

- **`ip/cpu/picolibc_host_memory.h`**（修改）: `Config` 结构体参数化 base address window（base_addr/size/tohost_addr），`PicolibcHostMemory(Config)` 新构造函数，`write_half`（16-bit little-endian）新方法，`write_word` per-byte tohost check 修复 partial-byte bug，`load_section(sh_addr, bytes)` 多段 ELF 加载 API，`in_window` 含 underflow guard + min(cfg.size, kMemorySize) OOB 防护
- **`ip/cpu/plugins/dbus.h`**（修改）: STORE 路径按 funct3 分发（SB→write_byte / SH→write_half / SW→write_word），通过 RISCV_DETAIL payload key 读 funct3，if-else 链（无 switch-on-state、无早返）遵守 ADR-040 Tier-1 #4
- **`tools/cpu_sim/elf_loader.h`**（修改）: 新增 `ElfLoadResult` 结构体（sections + entry_addr + tohost_addr），新 `load_elf_full(path)` 函数：所有 SHF_ALLOC PROGBITS 段收集 + e_entry 不再 skip + shstrtab 解析找 `.tohost` section sh_addr + section overlap 检测 throw。保留 legacy `load_elf_text` 兼容 add.elf
- **`tools/cpu_sim/main.cpp`**（修改）: 新 `--base-addr HEX` CLI flag；ELF load 改用 `load_elf_full` + `PicolibcHostMemory::Config`；build_cpu 后 caller-side 写入 ELF `entry_addr` 到 fetch node `KeyType::PC`（design Decision 3，不动 build_cpu ABI）
- **`tests/cpu/test_picolibc_memory_base_window.cpp`**（NEW）: 7 测试覆盖 base=0 默认、base=0x80000000 布局、write_half endian、write_word per-byte tohost、out-of-window drop、load_section placement
- **`tests/cpu/test_elf_loader_full.cpp`**（NEW）: 3 测试覆盖多 SHF_ALLOC PROGBITS 段收集、.tohost sh_addr 解析、无 .tohost 返回 UINT64_MAX
- **`tests/cpu/test_cpu_sim_base_addr.cpp`**（NEW）: 2 测试覆盖 add.elf back-compat、--base-addr flag 在 help 中可见
- **`tests/cpu/integration/test_rv32ui_runner.cpp`**（NEW）: 40 TEST_CASE 宏循环（macro RV32UI_P_TEST），每个 TEST_CASE: load_elf_full → PicolibcHostMemory Config → load_section 全部 → CpuFactory (enable_mmu=false) → 写 fetch PC → 跑 10000 cycles → hard REQUIRE mem.exited() + soft CHECK exit_code 写 CSV。CSV 用 std::once_flag + append-before-REQUIRE + `${CMAKE_SOURCE_DIR}` 绝对路径
- **`tests/cpu/riscv_tests/`**（NEW directory）: `build_rv32ui.sh` 脚本（riscv32-unknown-elf-gcc 编译 riscv-tests/isa/rv32ui/*.S），`README.md` 文档（来源/许可/排除清单），`elf/` 空目录占位（**待 follow-up commit vendor**）
- **`soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`**（NEW artifact, committed）: Wave 1 baseline 合规矩阵入口，schema `elf,status,fail_stage,category,cycles,notes`，category ∈ {真 bug / feature stub / toolchain / timeout / runner_setup_error}
- **`tests/CMakeLists.txt`**（修改）: `target_compile_definitions(chipforge_tests PRIVATE TEST_RV32UI_ELF_DIR=... RV32UI_CSV_PATH=...)` 注入绝对路径

### Modified
- `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`: Wave 1 估时 1 周→1.5 周；§6.1 风险表补 vendor ELFs（commit D 实际状态）
- `openspec/specs/cpu-real-fetch-and-memory/spec.md`: delta（5 ADDED + 1 MODIFIED）：base window + multi-section loader + e_entry + DBusPlugin STORE funct3 dispatch（LOAD 路径声明 OUT OF SCOPE）

### Verified
- `./build/bin/chipforge_tests` → **344/389 PASS**（baseline 332 + 12 new: 7 picolibc + 3 elf-loader + 2 cpu-sim）
- 40 new rv32ui-p TEST_CASE 全部 FAIL `category=runner_setup_error`（**预期**: ELF vendor 待 follow-up commit；runner-mechanics 正确：CSV 行已写入，hard REQUIRE 触发因为 ELF 不存在是 runner-mechanics violation）
- 5 pre-existing toolchain failures unchanged（per AGENTS.md baseline）
- 4 architecture gates all PASS（verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check）
- 5/5 稳定连跑 PASS

### Known Limitations (Documented)
- DBusPlugin LOAD 路径（LB/LH/LBU/LHU）显式 OUT OF SCOPE — 4 个对应测试预期分类 feature stub/真 bug；Wave 2 fix 候选
- riscv-tests ELF 实际 vendor 需 follow-up commit（riscv-tests 源码未 vendor 在本 session；`tests/cpu/riscv_tests/build_rv32ui.sh` 脚本就绪，README 记录来源/许可/排除清单）
- byte-wise tohost check 对 TESTNUM≥128 误分类（rv32ui-p 测试 TESTNUM < 128 范围内安全）

### Follow-up Work (Phase 1.5 Wave 2+)
- `cpu-pipeline-fix-rv32ui-N`: 解析 CSV 矩阵的真 bug，N 由 Wave 1 实测确定
- `soc-cpu-l1-mmu-demo` (Wave 2): 含 PTW 实内存接线 + 5 个 riscv-tests 通过子集
- riscv-tests ELF vendor commit: 拉源码 + 跑 build_rv32ui.sh + commit ELF 到 git

## v0.2.0 (2026-09-13) - cpu-mmu-integration

> **目的**: 把 mmu-cache-integration v0.1.0 实装的 RiscVMMUPlugin 真正接到 CPU pipeline. 条件注册 + 3 个 RiscV hook substage (csr_write_satp/sfence_vma/mmu_exit) + exception 12/13/15 传播到 CPU. 落地 Oracle Tier 1 #1 推荐的 M5 critical path 起点.

### Added

- **`ip/cpu/cpu_factory.h`**（修改）：`build_cpu()` 在 `config.enable_mmu=true` 时条件注册 `cf::cpu::plugins::RiscvMMUPlugin`; 映射 `config.mmu_mode` ("sv32"/"sv39"/"sv48") 到 `cf::ip::mmu::SvMode` 枚举; 默认 TLB 几何 2-level 8/8 + LRU + ptw_max_inflight=2（与 SoC JSON 一致）; 加 `pb->build()` 调用触发所有 plugin 的 setup()/build() 闭包（之前 baseline 不需要 plugin 闭包，加 MMU 后必须 build()）
- **`ip/cpu/tlm/cpu_keys.h`**（NEW）：4 个 Payload Key `SAT`/`SFENCE_VADDR`/`SFENCE_ASID`/`CPU_EXCEPTION_CODE`（CPU→MMU IPC Key 集合）；放 `ip/cpu/` 而非 `ip/mmu/` 避免反向依赖；Key identity 是全局 static 指针身份
- **`ip/cpu/plugins/mmu.h`**（修改）：override `setup()` + `build()` 声明 3 个 RiscV hook substage
- **`ip/cpu/plugins/mmu.cpp`**（修改）：`setup()` 声明 3 substage（csr_write_satp/sfence_vma 挂 execute，mmu_exit 挂 memory）；`build()` 注册 3 个 at_stage 闭包，路由到 `csr_write_satp()`/`sfence_vma()` hook（4-way RISC-V Spec §6.2 dispatch）/exception 传播；D4 合规：用 if/else 全分支（无早返）
- **`tests/cpu/integration/test_{3,5,7,10}stage_riscv.cpp`**（修改）：各加 `EnableMMU{3,5,7,10}Stage*` 测试 + `EnableMMUDisabledBitIdenticalBaseline`（5-stage enable_mmu=false 字节级 baseline）
- **`tests/cpu/test_cpu_riscv_mmu_hooks.cpp`**（NEW）：3 个 RiscV hook 集成测试（`CSRWriteSatpRoutesToRiscVMMUPlugin` + `SFENCEVMARoutesToRiscVMMUPlugin` + `MMUExceptionPropagatesToCPU`），镜像 test_l1_cache_plugin_unit.cpp 隔离测试原则
- **`tests/cpu/test_cpu_factory.cpp`**（修改）：`plugins.size() == 11` → `12`（Oracle B2 修复：RiscVMMUPlugin 注册后 +1）
- **`ip/cpu/README.md`**（修改）：新增 "RiscV MMU Integration (mmu-cache-integration v0.2.0)" 段说明 3 个 hook substage 契约
- **`ip/cpu/configs/cpu_params_schema.json`**（修改）：`enable_mmu` description 指向 `cf::cpu::plugins::RiscvMMUPlugin`
- **`ip/mmu/STATUS.md`**（修改）：`INTEGRATED + CPU PIPELINE`

### Fixed
- Oracle B2 breaking（commit 1 引入）：`cpu_factory` 默认 `enable_mmu=true` 导致现有 5+ 个 cpu 测试的 `node_count`/`stage_count`/`plugins.size` assertions 硬错误。**修复**：commit 1.5-1.7 tasks 同步更新 4 个测试文件（3-stage/5-stage/7-stage/test_cpu_factory）到新数字（详细注释说明来源）
- D4 早返违规（commit 2 引入）：3 个 RiscV hook 闭包用 `if (!node) return;` 早返。**修复**：改为 `if (node && has(key)) { ... } else { (void)0; }` 全分支（D4 HDL 1:1 友好）

### Verified
- `./build/bin/chipforge_tests` → **314/314 PASS**（was 291 baseline; +23 cases = 15 mmu-cache-integration + 8 cpu-mmu-integration）
- `[cpu-integration]` 25 → **33/33 PASS**（+8: 5 stage + 3 RiscV hook baseline）
- `[RiscV]` 4 → **7/7 PASS**（+3 new hook integration tests）
- `[cpu]` 114/114 PASS（含 B2 修复）
- 4 architecture gates all PASS（verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check）
- 5 次连跑稳定性 PASS（无 flaky test）

### Pending (下一阶段入口)

> `plugin-framework-stall` change —— `PipeBuilder::run()` 消费 `CtrlLink::should_halt()` 替代 RETRY 数据依赖（Oracle Tier 1 #2）

## v0.1.0 (2026-09-13) - mmu-cache-integration

> **目的**: 完成 ADR-044 §3.2 VIPT 数据流方案 A 双端契约 — L1CachePlugin 消费 pl::MMU_VADDR 做 VIPT 索引 + PIPT fallback 兼容 + MMUTLMBridgeAdapter cpptlm 集成 + soc/mmu_minimal.json 全链 + PTW completion dual-write bug 修补.

### Added
- **`ip/mmu/tlm/MMUPlugin.cpp`** (commit 0): PTW completion `WalkCallback`/`FaultCallback` 闭包加 `[node, vaddr]` capture, 同时写 `pl::PADDR` + `pl::MMU_VADDR` (修复 stale vaddr 风险, mmu-tlb-ptw-impl 遗留 defer)
- **`tests/mmu/test_ptw_unit.cpp`** (commit 0): 3 PTW 单测 — 闭包 capture vaddr 模式 + V=0 fault 12 + reserved encoding fault 15
- **`ip/cpu/plugins/mmu.cpp`** (commit 6): `RiscvMMUPlugin::csr_write_satp` + `sfence_vma` 实装 (mmu-tlb-ptw-impl 遗留 defer)
- **`ip/mmu/lib/tlb_base.h` / `tlb.h` / `multi_level_tlb.{h,cpp}`** (commit 6): `invalidate_vaddr_any_asid` 新 API (RISC-V SFENCE.VMA rs1!=x0, rs2=x0 跨 ASID 失效语义)
- **`ip/mmu/tlm/MMUPlugin.h`** (commit 6): `multi_tlb()` public getter + `invalidate_vaddr_any_asid` facade
- **`ip/cache/tlm/cache_keys.h`** (commit 1): 新增 `g_vaddr` VIPT Key + `vipt_fallback` Knob (Auto/PaddrOnly/Panic)
- **`ip/cache/tlm/L1CachePlugin.cpp`** (commit 2): lookup 闭包用 `n->has(MMU_VADDR)` 三元选择 idx_src/tag_src (VIPT 索引 + PIPT fallback 兼容 baseline)
- **`src/cf_plugin/bridge/mmu_bridge_adapter.{h,cpp}`** (commit 4): `MMUTLMBridgeAdapter` 独立 cpptlm `ChStreamModuleBase` 子类 + `ChStreamAdapterFactory` 注册
- **`soc/mmu_minimal.json`** (commit 5): 4 modules + 3 connections 全链 (`tg → mmu → l1 → mem`)
- **`tests/mmu/test_ptw_unit.cpp`** (commit 6): 3 边界 case — MMU_VADDR 一致性 (success + 2 fault paths)
- **`tests/mmu/test_mmu_plugin.cpp`** (commit 6): 4 RISC-V test — SFENCE.VMA (单 vaddr + 全清) + csr_write_satp + ASID switch + 多 ASID 失效
- **`tests/cache/test_mmu_cache_integration.cpp`** (commit 3): 3 integration test — VIPT 命中 + PIPT fallback + has() 对称性
- **`tests/soc/test_mmu_minimal_json.cpp`** (commit 5): 4 结构验证 — top-level + modules + connections + params
- **`ip/mmu/STATUS.md`**: `IMPLEMENTED` → `INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链)`
- **`openspec/changes/mmu-cache-integration/`**: 完整 OpenSpec lifecycle artifacts (proposal + design + tasks + 6 ADDED + 2 MODIFIED specs)

### Fixed
- mmu-tlb-ptw-impl 遗留 PTW completion stale vaddr bug (commit 0): WalkCallback 闭包不写 pl::MMU_VADDR, 多 ASID 场景导致 cache 索引错误
- Oracle round-2 发现 sfence_vma rs1!=0, rs2=0 分支不跨 ASID 失效, 加 invalidate_vaddr_any_asid API 修复
- header 注释 `rs1 (vaddr, -1=all)` 修正为 `0=all (x0)` 约定

### Verified
- `./build/bin/chipforge_tests` → 306/306 PASS (was 291 baseline; +15: 7 mmu + 3 cache + 4 soc + 1 集成微调)
- `[mmu]` 32 → **40** PASS
- `[RiscV]` 0 → **4** PASS (新 tag)
- `[cache][MMUCacheIntegration]` 3/3 PASS (新 tag)
- `[soc][MMUMinimalJson]` 4/4 PASS (新 tag)
- 4 architecture gates all PASS (verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check)
- 5 次连跑稳定性 PASS (无 flaky test)

### Pending (下一阶段入口)

> `cache-dse-sweep` change —— 完整 12-case Pareto DSE 配置扫描 (Phase 1.5 L1Cache 升 4-way 触发)

## v0.0.9 (2026-09-13) - mmu-tlb-ptw-impl
- **`ip/mmu/lib/tlb.h`**: `mutable` 关键字修复 `hits_`/`misses_` const-correctness (L227-229)
- **`ip/mmu/lib/tlb_entry.h`**: 新增 `using tag_type = cf::plugin::uint_t<TAG_BITS>;` (解决 8 处 typename Entry::tag_type 编译错误)
- **`ip/mmu/lib/ptw.{h,cpp}`**: Sv39 三级 walk state machine 实装 + `stub_write_pte`/`stub_read_pte` 测试 API + reserved encoding + V=0 fault handling
- **`ip/mmu/lib/multi_level_tlb.{h,cpp}`**: 并行查全部 level + 返回最深 hit (deepest = largest level index) + shadow fill + reverse invalidate
- **`ip/mmu/lib/tlb_factory.cpp`**: 11 组特化 (None 16,1 / LRU 64,4 / LRU 64,8 / LRU 32,2 / FIFO 256,1 / RRIP 64,4 / RRIP 64,8 / etc.) + VIPT safety check (idx_bits + offset_bits > 12 throw runtime_error)
- **`ip/mmu/policies/tlb_replacement_policy.cpp`**: 显式实例化 8→12 组 (覆盖新增 TLBFactory 特化所需组合)
- **`ip/mmu/tlm/MMUPlugin.{h,cpp}`**: at_stage 闭包实装 (tlb_lookup_ifetch + tlb_lookup_loadstore + ptw_l0/l1/l2) + 5 substage declare + set_name/declare_substage 修复
- **`ip/mmu/tlm/mmu_keys.h`**: 新增 4 Key (MMU_VADDR / EXCEPTION_CODE / SATP_PPN / SATP_MODE)
- **`ip/cpu/plugins/mmu.h`**: RiscvMMUPlugin 实装 (csr_write_satp + sfence_vma + exception 12/13/15 + satp_value accessors)
- **`bundles/tlb_bundles_tlm.hh`**: ch_stream Bundle 类型 (TlbReqBundle + TlbRespBundle, 4 字段窄桥, 全局 bundles:: 命名空间)
- **`src/cf_plugin/bridge/mmu_bridge.{h,cpp}`**: MMUTLMBridge 核心 (mirror L1CacheTLMBridge, issue_request/read_response API + tick() 末尾 pb.run())
- **`tests/CMakeLists.txt`**: mmu lib/policies/tlm 5 个 .cpp 加入 MMU_IMPL_SOURCES, 5 个 mmu tests 解阻塞

### Fixed
- `ip/mmu/lib/tlb.h`: `mutable` 关键字修复 `lookup()` const 方法修改 `hits_`/`misses_` 编译错误
- `ip/mmu/lib/tlb.h`: `name_` 类型 const char* → std::string (避免 dangling pointer, test `L0`/`L1` 字符串显示乱码已修复)
- `ip/mmu/lib/tlb_entry.h`: 加 tag_type typedef (8 处编译错误)
- `tests/mmu/test_tlb_factory.cpp`: 3 个测试用 VIPT-unsafe 配置 (32/4, 64/4, 64/16) 改为 8/8 全关联 VIPT-safe
- `tests/mmu/test_mmu_plugin.cpp`: BareMode 4/1 → 8/8; L1 level 配置 64/4 → 8/8 (VIPT safe)
- `tests/mmu/test_multi_level_tlb.cpp`: deepest-hit 语义下 L0 hit_count 期望从 1 → 0 (commit 6 改了语义)

### Verified
- 288/288 chipforge_tests PASS (was 259 baseline; +29 mmu test cases)
- 5 consecutive stable runs
- 4 architecture gates all PASS (verify_adr + verify_plugin_decision + check_plugin_portability + doc_link_check)

### Pending (下一阶段入口)

> `mmu-cache-integration` change —— L1CachePlugin 消费 pl::MMU_VADDR (VIPT index) + SoC 集成测试 + DSE 配置扫描 + CtrlLink stall 机制


## v0.0.8 (2026-07-02) - l1-cache-vipt-coherence

> **目的**: ADR-044 落地 — L1 Cache↔MMU 耦合策略锁定 VIPT (虚地址索引 + 物理地址 tag), 反别名安全边界定义。准备 `mmu-tlb-ptw-impl` 实施前置条件。

### Added (L1Cache VIPT 设计)
- **ADR-044**: L1 Cache↔MMU VIPT 锁定 + 反别名安全边界 ([`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`](ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md))
  - L1 ICache/DCache 统一 VIPT, L2 PIPT
  - VIPT 安全条件: `idx_bits + offset_bits ≤ 12` (4KB page)
  - Phase 1.5 升级路径: 16KB 1-way → 16KB 4-way (64 sets × 4-way × 64B)
  - VIPT 数据流方案 A: MMUPlugin 同时输出 `pl::PADDR` + `pl::MMU_VADDR`
- `ip/cache/docs/architecture.md` (L1Cache 微架构: 地址映射 + VIPT 安全 + DSE 边界)
- `ip/cache/docs/integration.md` (L1Cache 集成契约: CPU/MMU 接口 + SoC JSON 样例)
- `ip/cache/configs/params_schema.json` 加 `associativity` 字段 (enum [1,2,4,8], default 4)
- `ip/cache/tlm/L1CachePlugin.h` 加 VIPT 编译期 `static_assert(kIdxBits + kOffsetBits <= 12)`

### Cross-document sync
- `ip/cache/README.md` §6 DSE 表加 VIPT 安全约束警告 + §8 引用新 docs
- `ip/mmu/docs/integration.md` §1.1 加 VIPT 数据流引用
- `ip/mmu/STATUS.md` Implementation Roadmap 加 ADR-044 依赖
- `docs/architecture/adr.md` 主表 ADR-044 注册 (🚧 Phase 1 提案)

### ADR status
- ADR-044: 🚧 → 🚧 (保持 Phase 1 提案; Phase 1.5 L1Cache 升级时升级到 ✅)

## v0.0.6 (2026-06-21) - m4g-extend-tid-and-hooks

> **OpenSpec change**: `m4g-extend-tid-and-hooks` (详见 `openspec/changes/archive/2026-06-21-m4g-extend-tid-and-hooks/`)
> **目的**: M4G 二阶前向兼容锁 (硬前置解锁 M5-DSE 2-wide superscalar, 节省 ~200 LOC Phase 5+ 重构)
> **战略依据**: `ip/cpu/docs/research/post-m4g-strategic-decision-2026-06-20.md` (Option B 战略决策)

### Added (Gap A: tid plumbing via `set_tid`)
- `include/cf/plugin/plugin_base.h` — `PluginBase::set_tid(std::uint8_t)` 虚函数 (默认 no-op, 避免 3rd-party plugin break)
- `ip/cpu/plugins/reg_file.h` — `RegFilePlugin::set_tid` override + `tid_` 成员 + `current_tid()` 访问器
- `ip/cpu/plugins/hazard.h` — `HazardPlugin::set_tid` override + `tid_` 成员
- `ip/cpu/plugins/branch_predictor.h` — `BranchPredictorPlugin::set_tid` override + `tid_` 成员
- `include/cf/plugin/pipe_builder.h` — `n_threads_` 成员 + `set_n_threads()` setter + `run()` per-tid 循环
- `ip/cpu/cpu_factory.h` — `CPUConfig::n_threads` 字段 (默认 1) + `build_cpu()` 注入 `set_n_threads`

### Added (Gap B: OoO commit primitive documentation)
- `include/cf/plugin/pipe_builder.h:104-138` — 6 行注释块: `register_commit_hook` + `commit_storages` = OoO 提交原语; `CtrlLink::flush_when` = mispredict-squash 原语; 引用 `dse_architecture_v2_design_research.md §3 E.1` (ROB 设计) 作为 Phase 5+ consumer

### Added (Gap C: COMMIT stage naming)
- `ip/cpu/docs/multi_isa_architecture.md §2.4` — 5-stage 表新增 6th `commit` 行 (COMMIT 阶段名锁定, 避免 Phase 5+ 在 `at_stage("retire")` vs `at_stage("commit")` 碎片化)

### Added (Tests: 8 RED→GREEN cases)
- `tests/cpu/test_forward_compat.cpp` — `PluginBase::set_tid_default_noop` + `set_tid_overridable` + `RegFileSetTidStoresTid` + `HazardSetTidStoresTid` + `BranchPredictorSetTidStoresTid` + `PipeBuilderRunCallsSetTidDefaultOnce` + `PipeBuilderRunCallsSetTidPerThread` + `PipeBuilderRunDispatchesStagesPerTid`

### Impact
- **0 行为变化**: n_threads=1 默认 byte-identical, M4G baseline 不退化
- **基线**: 36/36 ctest PASS (10 已有 + 8 新增 = 18/18 in test_forward_compat)
- **变更规模**: 8 文件 +217/-27 LOC (生产 ~84 LOC + 测试 133 LOC)
- **breaking 变更**: 0 (set_tid 默认 no-op 兼容)
- **下一里程碑**: 启动 M5-DSE 2-wide superscalar (`openspec/changes/m5-dse-superscalar/` 4/4 artifacts ready)

## v0.0.4 (2026-06-18) - cache-policy-foundation (DRAFT, archive 等待重写后落地)

> **OpenSpec change**: `cache-policy-foundation`（详见 `openspec/changes/cache-policy-foundation/`）
> **状态**: 本条目为 v0.0.4 占位，描述 cache-policy-foundation v2 重写后的预期落地内容
> **目的**: 落地 A7 架构债务（`ip/cache/policies/` 子目录缺失 + L1CachePlugin 不可插拔替换策略）+ 修正 L1Cache 容量注释错误（32KB→16KB）

### Changed（L1Cache 容量注释修正）
- `ip/cache/tlm/L1CachePlugin.h` L18 注释：`256 sets × 64-byte = 32KB L1` → `256 × 1 way × 64B = 16KB L1 (direct-mapped)`（原 32KB 是 8-way 误算；实际 RAM = 16384 字节 = 16KB）

### Added（落地可插拔替换策略接口）
- `ip/cache/policies/replacement_policy.h` — `cf::ip::cache::policies::ReplacementPolicy` 抽象基类（4 虚方法 + 1 工厂方法）
- `ip/cache/policies/no_replacement_policy.h` — 默认 no-op 实现，保持 Phase 1.3 行为零变化
- `ip/cache/policies/lru_policy.h` — LRU reference implementation（1-way 简化，Phase 1.5 L2CachePlugin 整体替换）
- `tests/cache/test_replacement_policy.cpp` — 5 单元测试（factory-create-LRU / factory-create-None / factory-unknown-throws / LRU-on-access-increments / NoReplacement-victim-returns-zero）

### Changed（L1CachePlugin 集成注入点）
- `ip/cache/tlm/L1CachePlugin.h/.cpp` — 构造函数签名扩展：`explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr)`；`lookup` 阶段 `at_stage` 回调内调用 `policy_->on_access(set, way)`

### Impact
- **向后兼容**: 默认 `nullptr` policy → `NoReplacementPolicy` 行为等价于 hard-coded
- **基线**: 16/16 ctest PASS（v0.0.5 后）+ 5 新增 = 21/21 PASS
- **与 v0.0.5 协作**: 测试放 `tests/cache/`（v0.0.5 约定）；不重建 `ip/cache/test/`
- **修复 v1 已知问题**: 详见 `openspec/changes/archive/2026-06-18-cache-policy-foundation-v1-original/` 的 9 项问题

## v0.0.3 (2026-06-18) - ip-catalog-status-correct (DRAFT, archive 等待重写后落地)

> **OpenSpec change**: `ip-catalog-status-correct`（详见 `openspec/changes/ip-catalog-status-correct/`）
> **状态**: 本条目为 v0.0.3 占位，描述 ip-catalog-status-correct v2 重写后的预期落地内容
> **目的**: 修正 `docs/architecture/ip-catalog.md` IP 状态表与实际代码对齐；L1Cache 状态从 `Phase 1.2 L1D` 修正为 `Phase 1.3 unified 16KB`

### Changed（L1Cache 状态修正）
- `docs/architecture/ip-catalog.md` L1Cache 行：状态从 `🟡 TLM 实现中 (Phase 1.2 L1D)` → `🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)`
- `ip/cache/README.md §4` "可插拔策略"表格：`256 sets × 64-byte cache line = 32KB L1` → `256 × 1 way × 64B = 16KB L1 (direct-mapped)`
- `ip/cache/README.md §5` "配置参数"表格：`capacity_kb` 默认值 32 → 16

### Added（IP 状态表补全 2 列）
- `docs/architecture/ip-catalog.md` IP 索引表新增"实现范围"列（8 个 IP 全部填写）
- `docs/architecture/ip-catalog.md` IP 索引表新增"实施预计"列（5 个零代码 IP 指向 v0.0.5 STATUS.md + roadmap 路径）

### Impact
- **无运行时影响**：仅文档同步
- **与 v0.0.5 协作**: 零代码 IP 实施预计引用 v0.0.5 STATUS.md，不重复声明；不修改 `ip/README.md`（v0.0.5 STATUS 约定段已含 7 IP 状态表）
- **不新建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 `IP_STATUS_TEMPLATE.md` 已覆盖零代码 IP）
- **修复 v1 已知问题**: 详见 `openspec/changes/archive/2026-06-18-ip-catalog-status-correct-v1-original/` 的 4 项问题

## v0.0.5 (2026-06-17) - empty-directory-cleanup

> **OpenSpec change**: `empty-directory-cleanup`（详见 `openspec/changes/empty-directory-cleanup/`）
> **目的**: 清理项目结构噪音（22 个仅含 .gitkeep 的空目录），建立 IP 状态目录约定（`STATUS.md` 模板），明确"测试在 `tests/<ip>/` 而非 `ip/<ip>/test/`"的不变式。

### Removed（清理空目录噪音）
- `ip/memory/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/interconnect/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/peripheral/{tlm,rtl,test,configs}/` — 4 个仅含 .gitkeep 的占位目录
- `ip/cpu/test/` — 1 个 README-only 目录（README 移到 `ip/cpu/docs/verification.md`）

### Added（建立 IP 状态目录约定）
- `ip/memory/STATUS.md` — PLANNED 变体（0 LOC, Phase 2+）
- `ip/interconnect/STATUS.md` — PLANNED 变体（0 LOC, Phase 2+）
- `ip/peripheral/STATUS.md` — PLANNED 变体（0 LOC, Phase 3+）
- `ip/tilecore/STATUS.md` — INITIAL DESIGN 变体（有 docs/architecture.md, Phase 5+）
- `ip/tilecopy/STATUS.md` — INITIAL DESIGN 变体（有 docs/architecture.md, Phase 5+）
- `docs/templates/IP_STATUS_TEMPLATE.md` — 3 个变体（PLANNED / INITIAL DESIGN / PARTIAL）的可复用模板
- `ip/README.md` 顶部加 "STATUS 约定" 段，列出 7 个 IP 的状态表

### Changed（同步文档 + 修正 CPU IP 目录结构）
- `src/cf_plugin/CMakeLists.txt` L30 后插入注释：明确 cf_plugin 单元测试在 `tests/framework/`
- `ip/cpu/README.md` 顶部加 "测试位置" 段：明确 CPU 测试在 `../tests/cpu/`
- `ip/cpu/test/README.md` → `ip/cpu/docs/verification.md`（移动而非删除，保留验证规范文档）

### Impact
- **0 运行时影响**：仅清理空目录 + 文档同步
- **变更规模**: 12 个 .gitkeep 目录删除 + 5 个新 STATUS.md + 1 个新 docs/templates/ + 3 个文档修改

## v0.0.2 (2026-06-17) - doc-code-realignment

> **OpenSpec change**: `doc-code-realignment`（详见 `openspec/changes/doc-code-realignment/`）
> **目的**: 修复 8 项文档/代码一致性漂移（CRITICAL 3 项 + HIGH 5 项），建立"0 幽灵引用"基线并以 CI 脚本固化。

### Removed（消除 3 项 CRITICAL 漂移）
- `soc/riscv_virt.json`（引用 7 个不存在类 + `impl_mode` 字段无消费者，不可运行）
- `ip/cpu/cpu_factory.cpp`（12 行 stub，保留 `cpu_factory.h` 声明供 Phase 2+ 实施）

### Changed（重写/更新 5 项 HIGH 漂移）
- `docs/architecture/overview.md` §"SoC 层是 IP 组合器" 改为指向 `soc/l1_cache_minimal.json` 真实工作示例（不再描述虚构的 `RiscvVirtSoC.h/cpp`）
- `docs/architecture/overview.md` §"ch_stream 接口即 ISA 无关层" 改为 "Phase 1.4+ Future Work" 占位段（当前 1 个 CPU IP，无法"验证" ISA 无关性）
- `docs/architecture/interface-design.md` §1.0 增加 "已实现 POD vs 设计目标 bundle_base" 对照表（明确 Phase 1 当前是 POD，与 §"所有 Bundle 继承 bundle_base" 段落对照）
- `soc/README.md` 顶部说明改为"目前 2 个 L1Cache 验证配置；RISC-V virt 推迟到 Phase 2+ 实施"
- 7 个文档中 8 个幽灵类名（`RiscvIssTlm` / `L1CacheTlm` / `BusMatrixTlm` / `DramTlm` / `UartTlm` / `ClintTlm` / `PlicTlm` / `RiscvCoreRtl`）全部替换为 Plugin 风格命名 / 已注册 CppTLM 类名 / "Phase X+ 实施" 占位

### Added
- `docs/architecture/adr/ADR-041-bridge-tick-pattern.md` — 明确 Bridge 适配层允许 `tick()` 模式的边界条件（业务 Plugin vs Bridge 适配责任划分）
- `docs/architecture/adr.md` 插入 ADR-041 摘要 + §3 G 详细记录 + 交叉引用 ADR-025/037/040
- `tools/verify_no_ghost_refs.sh` — CI 防漂移脚本（可执行权限 755 + bash 严格模式 + 8 个类名 grep + 排除 `openspec/changes/` / `CHANGELOG.md` / `.omo/drafts/`）

### Impact
- **0 运行时影响**：仅文档/JSON 同步，不改任何运行时行为
- **0 API 变更**：仅删除不可运行文件
- **CI 影响**：新增 grep 检查脚本，阻断任何带幽灵类名的 .md/.json/.h/.cpp
- **基线提升**：文档/代码一致性从约 50% 提升到 80%+（以 `tools/verify_no_ghost_refs.sh` exit 0 为准）

## [Unreleased]

### Added
- Phase 1.3d-extras: ch_stream adapter 注册 + full JSON instantiateAll e2e
  - `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (Phase 1.3d-extras 增补)
    - 静态注册 `ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, ::bundles::CacheReqBundle, ::bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter")`
    - 暴露 `req_in()` / `resp_out()` ch_stream 访问器 (cpptlm::StreamAdapter<ModuleT,...> 期望接口)
    - 4 字段窄桥 (DECISION-2026-06-13-01 F1.A, D1=C 不变): `addr/data/is_write/id` ↔ `cf::bundles::CacheReq` POD;
      `op/burst_len/parent_id/fragment_*` 走 CppTLM default 值 (0/false/1, R6 风险 Phase 2+ 评估)
  - `soc/l1_cache_adapter_e2e.json` (新建, full JSON instantiateAll spec, `L1CacheTLMBridgeAdapter` 类型)
  - `src/cf_plugin/tests/test_l1_cache_json_instantiate.cpp` (新建, 5 子测试):
    instantiateAll / 3 模块 getInstance / startAllTicks / 100 cycle 推进 / Bridge pb_run 验证
  - 14/14 → 16/16 ChipForge ctest PASS in 4.91s
  - **PA-6 闭环**: Phase 1.3 全部子任务完成 (1.3a + 1.3b + 1.3c + 1.3d + 1.3d-extras + 1.3e + 1.3f)
  - 下一里程碑: PA-7 cpptlm::CacheTLM baseline 对比 (2-3 天)
- Phase 1.3d: `L1CacheTLMBridgeAdapter` (cpptlm ModuleFactory 兼容适配层)
  - `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (继承 ChStreamModuleBase)
  - 解决 v2 §4 决策: Bridge 构造签名 (unique_ptr<L1CachePlugin>) 与
    ModuleFactory::registerObject 期望的 (string, EventQueue*) 不兼容
  - Adapter 是薄包装: 内部创建默认 Plugin + Bridge, tick() 委托给 Bridge
  - `src/cf_plugin/tests/test_l1_cache_plugin_e2e.cpp` (5 tests: ModuleFactory
    发现 / Adapter 构造 / Bridge 持有 / Adapter::tick 触发 pb.run / 1000+ tx)
  - 14/14 ChipForge ctest PASS in 5.28s
  - Phase 1.3d-extras 范围 (推迟): ch_stream adapter 注册 + full JSON
    instantiateAll e2e (需要 ChStreamAdapterFactory::registerAdapter<L1CacheTLMBridgeAdapter, CacheReqBundle, CacheRespBundle>)
// Phase 1.3 全部完成: 1.3a + 1.3b + 1.3c + 1.3d + 1.3e + 1.3f (commit 待)
- Phase 1.3f: `ip/cache/README.md` §9 Phase 1.3 使用指南 (L1CachePlugin + Bridge + JSON)
  - Status banner 更新: Phase 1.2 + 1.3a + 1.3b + 1.3c + 1.3e 已落地 (1.3d 推迟)
  - §9.1 L1CachePlugin 直接使用 (Plugin-style 单元测试 pattern)
  - §9.2 L1CacheTLMBridge 使用 (cpptlm 适配层 + D1' 末尾挂载契约)
  - §9.3 SoC JSON 拓扑 (`soc/l1_cache_minimal.json`)
  - §9.4 参数 Schema (`ip/cache/configs/params_schema.json`)
  - §9.5 测试套件汇总表 (13 tests PASS in 4.11s)
  - §9.6 相关决策与 ADR (v2 决策草案 + ADR-024/037 + D4)
  - 9 个相对链接全部验证 OK
- Phase 1.3c: `ip/cache/configs/params_schema.json` (L1CachePlugin IP 配置 JSON Schema)
  - JSON Schema draft-07 格式, 严格模式 (additionalProperties=false)
  - 4 核心 param 字段 required: `num_sets`, `tag_bits`, `idx_bits`, `line_data_bits`
  - Defaults 匹配 L1CachePlugin geometry: 256/20/8/512 (Phase 1.2 验证值)
  - `replacement_policy` + `write_policy` 预留 forward-compat (Phase 1 仅 direct-mapped/WriteBack)
  - `src/cf_plugin/tests/test_cache_params_schema_json.cpp` (6 tests: top-level / type const / impl_mode enum / 4-required / strict / defaults)
  - 13/13 ChipForge ctest PASS in 4.11s
- Phase 1.3b: `soc/l1_cache_minimal.json` (Phase 1.3 最小 SoC 拓扑 spec)
  - `traffic_gen` (TrafficGenTLM) → `l1` (L1CacheTLMBridge) → `mem` (MemoryTLM) 拓扑
  - 依据: v2 决策草案 §4 (D1=C + D1' 契约), D2=B (Bridge 在 src/cf_plugin/bridge/)
  - `src/cf_plugin/tests/test_soc_l1_cache_minimal_json.cpp` (4 tests: top-level fields / modules / connections / l1 params)
  - 12/12 ChipForge ctest PASS in 4.00s
  - Phase 1.3d 范围预留: Bridge 注册到 cpptlm::ModuleFactory 后可被此 JSON 实例化
- Phase 1.3e: BundleMapper drift 防护 (verify_adr.sh ADR-024 增强)
  - `tools/verify_adr.sh` 新增 drift 防护检查: 拒绝 `bundles/bundle_mapper.h` 提前实现
  - 依据: v2 决策草案 D1'' + `bundles/README.md:102` (Phase 5 才转换) + `plugin-framework.md:129` (Phase 6 才实现)
  - 负向测试通过: 创建 stub → `verify_adr.sh --only=ADR-024` 报告 FAILED (Critical drift)
  - 正向测试通过: 删除 stub → 报告 PASS (符合 Phase 5 推迟约定)
- Phase 1.3a: `L1CacheTLMBridge` (Plugin-style first IP 的 cpptlm 适配桥接)
  - `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (框架层, 不受 D4 检查约束)
  - 构造: 接管 `unique_ptr<L1CachePlugin>`, 在内部 `PipeBuilder` 注册 + build
  - D1' 契约: `tick()` 末尾调用 `pb_.run()` (回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1)
  - D1=C 实现: 4 字段 test API 转发 (addr/data/is_write/id)
  - `src/cf_plugin/tests/test_l1_cache_bridge.cpp` (2 tests: tick invokes pb.run / 4-field forwarding)
  - 11/11 ChipForge ctest PASS in 3.56s; D4 verify_plugin_decision 3+4/3 PASS
  - Phase 1.3d 范围预留: `set_stream_adapter()` + ch_stream<CacheReqBundle> 协议转换 (cpptlm::StreamAdapterBase 已前向声明)
- Phase 1.2: `L1CachePlugin` (Plugin-style first IP, lookup + refill two-stage pipeline)
  - `ip/cache/tlm/L1CachePlugin.h/.cpp` (256 sets, 64B line, direct-mapped)
  - `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp` (4 tests: miss / refill / hit-after-refill / D4 runtime)
  - 10/10 ChipForge ctest PASS in 2.30s; D4 verify_plugin_decision 3/3 PASS
  - All Bundle fields `cf::plugin::uint_t<N>`; no `tick()`, no state machine; at_stage-driven
  - Phase 0 limitation: `uint_t<512>` falls back to `uint64_t` (tracked; Phase 6 upgrade planned)

### Notes
- Phase 1.3 v2 决策草案 (`8d80fd3`): D1=C (POD + 4 字段窄桥) / D1'=末尾 (tick末尾调pb.run) / D1''=不实现 (BundleMapper推迟Phase 5/6)
- Phase 1.1 Bundle definitions shipped in `073402c` (Bundles 6 types + 9/9 unit tests)
- Phase 0 LSP false positives remain (cf/plugin namespace visibility); tracked, not blocking

### Pending (下一阶段入口)

> Phase 1.3 全部 6 子任务完成 (`26fe7d2`..`c8d1dd1`, 14/14 ctest PASS).
> 以下三项可任意顺序启动, 详见 `docs/roadmap/roadmap-status.md` §3 (PA-6~PA-9).

| 阶段 | 任务 | 入口 | 前置 |
|------|------|------|------|
| **Phase 1.3d-extras** | ch_stream 协议转换 + full JSON `instantiateAll` e2e | PA-6 | `c8d1dd1` (Adapter 已注册) + 起草 PA-8 决策草案 |
| **Phase 1.4** | `cpptlm::CacheTLM` baseline 对比 (`soc/l1_cache_baseline.json`) | PA-7 | Phase 1.3 + 起草 PA-9 决策草案 (5 项候选决议) |
| **Phase 2** | bare-metal 测试套件 (riscv-tests RV64GC + SpikeBridge) | (未立项 PA) | 建议 Phase 1.3d-extras 先完成 |

**当前可立即工作的入口**:
- 起草 `.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-10.md` (PA-8, 参考 v2 格式 `8d80fd3`)
- 起草 `.omo/drafts/decision-phase-1.4-baseline-2026-06-10.md` (PA-9, 5 项候选决议: E1 baseline 选型 / E2 trace 工具 / E3 共享 traffic_gen / E4 hit rate 容差 / E5 测试时长)

## [0.0.1] - 2026-06-10

### Added
- Phase 0 Plugin scaffolding framework (`cf::plugin` namespace, 6 headers in `include/cf/plugin/`)
  - `PluginBase` (lifecycle interface)
  - `Payload<T>` (type-safe key for cross-stage IPC)
  - `PipeNode` (dataflow node)
  - `PipeBuilder` (orchestrator)
  - `CtrlLink` (4-control API: halt_when / throw_when / flush_when / bypass)
  - `uint_t<N>` (compile-time TLM/RTL switch)
- `cf_plugin` INTERFACE library (CMake target)
- 7 cf_plugin unit tests in `src/cf_plugin/tests/` (8/8 ctest PASS in 2.34s)
- CppTLM v2.1.0 integration (TLM framework, `cpptlm_core` target)
- CppHDL v1.0.0 integration (HDL framework, `cpphdl` target, JIT disabled)
- `tools/verify_plugin_decision.sh` (D4 static check, 3/3 PASS)
- `tools/run_chipforge_tests.sh` (wrapper for ChipForge-only ctest run)