---
id: feat-dhrystone-dmips-gate
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P0
version_target: v0.11.0
phase: phase-4
theme: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
manual_deps:
  - mfc-cpu-pipeline-multi-cycle-fsm (50/60 IN_PROGRESS; archive 后 D.4/F/G.2 顺势关闭)
  - cpu-pipeline-tick-fix (archived 2026-10-16, commit 082b07a) — cpu_sim harness 修绿, 测量底座解锁
review_by: 2026-11-15
---

# feat-dhrystone-dmips-gate — Dhrystone benchmark harness 修复 + DMIPS/MHz ≥ 1.4 硬门禁

> **核心发现 (2026-10-16, Oracle + Explore + D1 readelf 三方确认)**: `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` 中 0.0351 DMIPS/MHz 是**伪测量值**. 真实根因是 3 层 harness 契约违反 + DMIPS 公式量纲错误 + 编译 flag 缺 `-O2`, 与 CSV 描述的 "livelock" 诊断完全无关. 本 change 修 harness 拿到真基线, 再决策 feat-bp-btb-gshare / mfc G.2 投入顺序.
>
> **依赖关系逆转**: `mfc-cpu-pipeline-multi-cycle-fsm` G.2 此前标 "defer v0.11.0" 因无真基线; 本 change 修复 harness 是 G.2 的 **前置** (而非 mfc 是 feat-dhrystone 的前置).

## 架构依据

### D1 readelf 硬证据 (2026-10-16)

`riscv32-unknown-elf-readelf -h -l tests/cpu/manual_elf/dhrystone/dhrystone.elf`:
- **e_entry = 0x710** (非零, 远小于 0x10000)
- `.text` sh_addr = **0x0**, size = 0x9d0
- LOAD segment 0x1000-0x1c96 vaddr 0x0-0xc96 (RWE, 含 .text+.rodata+.eh_frame+.sdata+.bss)
- **无 .tohost section** (dhrystone.c:38 注释 "no .tohost section" 明说; elf_loader.h:248-250 找不到 → tohost_addr=UINT64_MAX → 回退 0)

### 3 层 harness 契约违反 (BLOCKERS, 真因)

**H1 (tohost 写窗口外被 drop)**:
- `dhrystone.c:69-73` `tohost_pass()`: `volatile int *p=(volatile int*)0; *p=1;` — 绝对地址 0 写 1.
- 测试配置 (test_dhrystone.cpp:39-44): `tohost_addr = 0` (无 .tohost section fallback).
- **window_base = entry & ~0xFFFF = 0; if(window_base==0 && entry!=0) window_base = entry = 0x710**.
- `picolibc_host_memory.h:72-77` `write_byte` 前置 `if(!in_window(mem_address)) return;` (静默 drop, 无告警).
- 0 < 0x710 = false → `in_window(0)` = false → `write_byte` 立即 return → **`check_tohost` 永不执行** → `mem.exited()` 永远 false → 100M cycle 满预算.
- **结论**: 无论 CPU 多快, 测试永远不可能 PASS. "livelock" 是 harness 自证预言.

**H2 (整段 .text@0x0 被 load_section 静默丢弃) — 升级 oracle H2**:
- `picolibc_host_memory.h:141-146` `load_section` 同样 `if(!in_window(sh_addr)) return;`.
- LOAD segment vaddr 0x0, 全部 .text@0x0 → in_window(0) = `0 >= 0x710` = false → 整段 LOAD 静默 drop.
- `.rodata@0x9d0` → in_window(0x9d0) = `0x9d0 >= 0x710` = true → 进 window (但**只装 .rodata 之后, .text 之前全丢**).
- 程序跑在 zero memory, fetch 读 0x00000000 = 全零指令 (永久死循环).

**H3 (无 crt0, sp 未初始化)**:
- `reg_file.h:84` `RegFilePlugin(T default_value=T{0})`, 全寄存器 (含 sp=x2) 初值 0.
- 测试仅设 PC, 不设 sp; dhrystone `_start` 调 Proc_5 等函数用栈 → sp=0 首次 push 写 0xFFFFFFFC (窗口外, drop) → `ret` 弹回 ra=0 → PC 归零 → 死循环.
- H1/H2 修完后必然暴露 (第三层根因).

### B2 公式量纲错误 (3.087× 宽)

测试代码 (test_dhrystone.cpp:113): `dmips_per_mhz = 1757.0 * kDhrystoneRuns / cycles`.
**标准 Dhrystone 公式**: `DMIPS/MHz = N * 1e6 / (1757 * cycles)`.
比值 = (1757²) / 1e6 = **3.087×**.
- 当前 gate cycles ≤ 2,510,000 (测试 line 27-28, `(1757*2000)/14*10`)
- 真 gate cycles ≤ 2000e6 / (1757 * 1.4) = **813,043** (标准 DMIPS 公式)
- 当前 gate 比真 gate 宽 3.087× → 公式修好后, 真 1.4 DMIPS/MHz gate 应是 813,043 cycles

### B3 编译 flag 缺 -O2

- `tools/cpu_sim/build_manual_elf.sh:37-104` **无 dhrystone target**.
- `src/cf_plugin/CMakeLists.txt:119-155` `add_custom_command` 仅 build `add/mmu_bare/l1cache_basic`, **不含 dhrystone**.
- dhrystone.elf 是**手工一次性产物**, 唯一构建命令在 `dhrystone.c:8-12` 头注释: `riscv32-unknown-elf-g++ -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static -T ../link.ld`, **无任何 -O flag** (默认 -O0).
- Dhrystone 规范要求 -O2 -fno-inline; -O0 指令数膨胀 2-3×, 1.4 物理不可达.

### mfc 真 radix-2 已落地 (CSV 过期)

- `ip/cpu/arch/riscv/mul_div_fsm.h` 实际 **424 行** (CSV 引 475-500 不存在).
- `mul_div_fsm.h:207-274` 方法名 `on_tick(State)` (CSV 引 advance_fsm 已改名; FsmBase 抽取时), 真 radix-2 restoring division (line 240-245 `remainder_=(remainder_<<1)|(dividend>>(31-iter))`, `iter_<32` 逐 bit, 33 拍写回), MUL=1c/MULH=3c/DIV=33c. commit `df3bcea` (2026-10-10) 已 merge.
- `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS 实测 (legacy + fsm × mul + div).
- `[riscv-tests] rv32um 8/8 PASS` 实测 (mfc-extract-fsm-h archive).
- **结论**: MUL/DIV 真 radix-2 已落地, 不是 dhrystone 0.0351 的 binding constraint.

### BTB / Branch Predictor 现状 (Task 5 验证)

- `ip/cpu/plugins/branch_predictor.h:121-131` `BranchPredictorPlugin` 类已存在 (BTB+Bimodal+GShare).
- `cpu_factory.h:457-459` 无条件注册 BranchPredictor, `btb_entries` 默认 64.
- **但未接线**: `branch_predictor.h:126-129` fetch 阶段 `predict()` 结果被丢弃 (注释 "M2 阶段仅存储"); `flush_when` 恒 false 桩 (149-151) → 对 PC 零影响, 无真分支预测.
- `feat-bp-btb-gshare` 状态 `active` 但实现未启动, phase-4 的 "pc_reg mux 接入 BP 预测目标" 未打勾.
- **Dhrystone 高分支密度** (每 iter ~8-10 个 data-dependent 分支 + 函数 call/ret): 5-stage flush 3-cycle 惩罚, 无预测 CPI 恶化 ~25-40%. VexiiRiscv 1.4 DMIPS/MHz 依赖 BTB + 全 forwarding. **大概率是 BTB 的 binding constraint** (待 harness 修好后实测确认).

## 范围

### In Scope (本 change 必做)

- **B1 harness 修复 (3 项)**:
  1. **link_dhrystone.ld** — 仿 link_add.ld 模式, base=0x80000000 + .tohost section @ 0x80001000 (或类似窗口内地址). 链接脚本就位 → `*(volatile int*)0` 改 `*(volatile int*)tohost_addr` 写窗口内.
  2. **crt0 栈初始化** — 加 `_start` 汇编 stub (`li sp, base+60K; jal main_body`) 或链接器提供 `_sp` symbol. 否则 H3 必咬.
  3. **测试 window_base 修** — `test_dhrystone.cpp:42-44` `window_base = entry & ~0xFFFF; if(window_base==0 && entry!=0) window_base=entry` 改为 `window_base = base_from_linker_script` (e.g. `kElfBase=0x80000000`), 与新 link_dhrystone.ld 一致.
- **B2 公式修正**:
  - `test_dhrystone.cpp:113` 改 `dmips_per_mhz = (double)kDhrystoneRuns * 1e6 / (1757.0 * cycles)`.
  - `test_dhrystone.cpp:27-28` 改 `kMaxCyclesForDmips14 = (kDhrystoneRuns * 1'000'000ULL) / (1757 * 14) = 81,266` 错; 真 gate `cycles ≤ N*1e6/(1757*1.4) = 813,043`. 注释清晰说明.
- **B3 编译 flag -O2**:
  - 选 (a) 增 `tools/cpu_sim/build_manual_elf.sh` dhrystone target + 调 `src/cf_plugin/CMakeLists.txt:119-155` add_custom_command 含 dhrystone, 传 `-O2 -fno-inline`. (推荐 — 自动化构建)
  - 选 (b) 手动重 build + commit 新 dhrystone.elf (有 .tohost section + base=0x80000000) 到仓库, dhrystone.c 加链接脚本注释. (次选 — 不引入新自动化)
- **真基线测量**: 修好 harness + 公式后, 跑 legacy / fsm mode 各 2000 iter, 记录 DMIPS/MHz (真值, 不是伪 0.0351).
- **Gate 接线**: 1.4 DMIPS/MHz = cycles ≤ 813,043 (真 gate). `dhrystone_legacy_tohost_pass` + `dhrystone_fsm_dmips_mhz_minimum_1_4` 两个 TEST_CASE 改用真 gate.
- **N1 CSV 重写**: `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` 重写为 HEAD 实测真基线 + 公式说明, 引用本 change commit. 遵守 AGENTS.md 数字维护原则.
- **N2 cap 降**: `kMaxCycles = 5'000'000` (gate 813,043 × 6 余量, 顺带治愈 AGENTS.md §known SIGTERM 60s+ timeout).
- **Spec-delta**: `openspec/specs/dhrystone-benchmark/spec.md` 新增 1 Requirement (DMIPS 公式 + 真 gate) + 1 Scenario (Bare-mode byte-identical evidence).

### Out of Scope (严禁触碰)

- ❌ **不**改 `CPUConfig::enable_mmu` 默认 (D4 硬约束, 沿用 cpu-pipeline-tick-fix D4)
- ❌ **不**动 mfc-cpu-pipeline-multi-cycle-fsm (D.4/F/G/H 12 task 是 mfc owner 责任)
- ❌ **不**实装 BTB / GShare 真接线 (branch_predictor.h 已是 M2 stub, 留给 feat-bp-btb-gshare)
- ❌ **不**vendor riscv-dhrystone (当前自包含源码算法完整, 无 printf/malloc 依赖)
- ❌ **不**改 `picolibc_host_memory.h` 静默 drop 行为 (tohost 窗口外 WARN 建议 N3, 但本 change 不动 framework)
- ❌ **不**改 `ip/cpu/arch/riscv/mul_div_fsm.h` (真 radix-2 已落地, 4/4 PASS)

## Why

- **不修 harness 无法做 G.2**: 0.0351 DMIPS/MHz 是伪值, 任何 G.2 决策基于伪值都是猜测.
- **不修公式 1.4 是假绿**: 3.087× 宽 gate 让测试过 = 假绿 = release 后回归风险.
- **不补 -O2 物理不可达**: -O0 指令膨胀 2-3×, 406 cycles/iter (gate) 物理无解 (VexiiRiscv 1.4 跑 1.4 DMIPS/MHz 需 -O2 -fno-inline).
- **三大本 v0.11.0 launch gate 判据**: 判据 #1 (rv32um 8/8 ✅)、#2 (5 cpu_sim CLI PASS ✅, 本会话 cpu-pipeline-tick-fix 兑现)、#3 (DMIPS/MHz ≥ 1.4 仍 pending, 本 change 主目标).
- **解锁 feat-bp-btb-gshare (PoC-6 CoreMark ≥ 1.9)**: 同样需要真 benchmark 测量, 修好的 harness 是 PoC-6 前置.

## What Changes

### 1. 修 harness 3 项 (B1)

**a. `tests/cpu/manual_elf/link_dhrystone.ld`** (新增, ~50 行, 仿 link_add.ld):
```ld
OUTPUT_ARCH("riscv")
ENTRY(_start)
SECTIONS {
  . = 0x80000000;
  .text : { *(.text.init) *(.text) *(.text.*) }
  . = ALIGN(0x1000);
  tohost = .;
  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss : { *(.bss) *(.bss.*) *(COMMON) }
  _end = .;
}
```

**b. `tests/cpu/manual_elf/dhrystone/dhrystone.c`** (改 `_start`):
- 加 `static const int tohost_addr = 0x80001000;` (或用 linker symbol `extern char tohost`).
- `tohost_pass()` 改 `volatile int *p = (volatile int *)&tohost; *p=1;` (相对 add.elf 同样模式, 写窗口内).
- 加 `_start` 汇编 stub 或 inline: `__asm__ volatile ("li sp, 0x8000F000\n\tjal _start_real");` (sp = base + 60K, 避开 .bss 末尾).

**c. `tests/cpu/integration/test_dhrystone.cpp`** (改 line 39-44):
- 删 `window_base = entry & ~0xFFFF; if(window_base==0 && entry!=0) window_base=entry;` fallback.
- 改 `cf::cpu::PicolibcHostMemory::Config mem_cfg{.base_addr = 0x80000000, .size = 64*1024, .tohost_addr = 0x80001000};` (硬编码 base, 与 link_dhrystone.ld 锁).

### 2. 修公式 (B2)

`test_dhrystone.cpp:113`:
```cpp
// Standard DMIPS/MHz = N * 1e6 / (1757 * cycles).
// (B1+B2+B3 修复前 CSV 0.0351 是伪值, 因 tohost 永远不写 → 100M cap 满预算反推).
constexpr double kDhrystoneRef = 1757.0;  // VAX 11/780 reference DMIPS = 1 cycle/iter
// ... (after runDhrystone):
const double dmips_per_mhz = (double)kDhrystoneRuns * 1e6 / (kDhrystoneRef * (double)cycles);
```

`test_dhrystone.cpp:27-28`:
```cpp
// True gate: cycles <= N * 1e6 / (1757 * 1.4) = 813,043
constexpr std::uint64_t kMaxCyclesForDmips14 =
    (kDhrystoneRuns * 1'000'000ULL) / (1757 * 14) * 10;  // = 813,043
```

### 3. 编译 flag -O2 (B3)

`src/cf_plugin/CMakeLists.txt` add_custom_command (line 119-155) 加 dhrystone target, 传 `-O2 -fno-inline`:
```cmake
foreach(ELF_PAIR IN ITEMS
    "add;add.S;link_add.ld"
    "dhrystone;dhrystone.c;link_dhrystone.ld"   # NEW
    "mmu_bare;build_mmu_bare.S;link.ld"
    ...)
  ...
  COMMAND ${RISCV_GPP} -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32
    -nostdlib -static -T ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${LINK_SCRIPT}
    ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${SRC_FILE}
    -o ${CMAKE_BINARY_DIR}/${ELF_NAME}.elf
  ...
```

注: `RISCV_GPP` (riscv32-unknown-elf-g++) 是 C++ 编译, dhrystone.c 头文件 `extern "C"` 已 OK. 沿用 add.elf 的 `riscv-as`/`ld` 路径可能更稳, 但 dhrystone 是 C++ 源码, 必须 g++.

### 4. 跑真基线 + 接 gate

修好后跑 (legacy + fsm) × 2000 iter:
- 跑通: 100M cap 内 exited, 真实 cycles 实测 (期望 ~4000-8000 cycles/iter, gate 813,043 cycles = 406 cycles/iter; v0.11.0 launch 真 DMIPS/MHz 期望 ~0.5-1.0, 1.4 是 hard gate 推到 feat-bp-btb-gshare).
- 不跑通: 重检 H1/H2/H3 修是否真到位 (可能 H3 sp init 仍需 inline asm, 或 _start 真在 .text.init 而非 .text).

### 5. CSV 重写 (N1)

`dhrystone-baseline-matrix.csv` 改:
```
elf,status,mode,kMaxCycles,dmips_per_mhz_measured,notes
dhrystone,PASS,legacy,5000000,<真值>,2000 iter Dhrystone PASS; DMIPS/MHz <真值>; cycles per iter <X>; feat-dhrystone-dmips-gate commit <hash>.
dhrystone,PASS,fsm,5000000,<真值>,FSM mode PASS; 1.4 DMIPS/MHz 期望 feat-bp-btb-gshare 落地后达; 本 change 真基线建立.
```

## Capabilities

### 新增 spec-delta

`openspec/specs/dhrystone-benchmark/spec.md` (新建, ~30 行):
```md
## Requirements

### Requirement: Dhrystone DMIPS/MHz measurement MUST use standard formula
- Formula: DMIPS/MHz = N * 1e6 / (1757 * cycles)
- True gate cycles <= 813,043 (for N=2000, 1.4 DMIPS/MHz)
- (No-CSV-formula: 1757*N/cycles is incorrect, off by 3.087x)

#### Scenario: dhrystone test PASS with true gate (B1+B2+B3 fixed)
- GIVEN harness 修好 (link_dhrystone.ld base=0x80000000 + .tohost + crt0 sp init + window_base=0x80000000)
- AND 公式修对 (DMIPS/MHz = N*1e6/(1757*cycles))
- AND 编译 -O2 -fno-inline
- WHEN 跑 2000 iter Dhrystone (legacy mode)
- THEN exited = true, cycles < 5,000,000 cap, DMIPS/MHz 实测
- AND test_dhrystone_legacy_tohost_pass PASS
- AND test_dhrystone_fsm_dmips_mhz_minimum_1_4 PASS (gate 813,043)
```

## Impact

### Affected tests

- `tests/cpu/integration/test_dhrystone.cpp` (改 3 处: window_base, 公式, gate)
- `tests/cpu/manual_elf/dhrystone/dhrystone.c` (改 `_start` + crt0 + tohost_pass 写窗口内)
- `tests/cpu/manual_elf/link_dhrystone.ld` (新增)
- `src/cf_plugin/CMakeLists.txt` (add_custom_command 加 dhrystone target)
- `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` (重写 HEAD 实测)
- `openspec/specs/dhrystone-benchmark/spec.md` (新建 spec-delta)

### Affected code (严禁改动)

- ❌ `ip/cpu/arch/riscv/mul_div_fsm.h` (真 radix-2 已落地, 4/4 PASS)
- ❌ `ip/cpu/cpu_factory.h` (D4 硬约束, 沿用 cpu-pipeline-tick-fix)
- ❌ `ip/cpu/plugins/mul_div_fsm_chmem.h` (CH_MEM DSL, 与 harness 修复正交)
- ❌ `ip/cpu/plugins/branch_predictor.h` (M2 stub 留给 feat-bp-btb-gshare)

### Affected users

- **v0.11.0 launch gate** 判据 #3 (DMIPS/MHz ≥ 1.4 硬门禁) — 本 change 兑现真基线
- **mfc-cpu-pipeline-multi-cycle-fsm G.2** — 此前 defer, 现可基于真基线推进
- **feat-bp-btb-gshare PoC-6 CoreMark** — 同 benchmark 真基线
- **chipforge_tests `[dhrystone]` family** — 1 FAIL → 2 PASS (legacy_tohost + fsm_dmips_mhz)

### Out of Scope (硬约束)

- 不改 enable_mmu 默认 (D4)
- 不重写 picolibc_host_memory.h 静默 drop 行为 (N3 留作后续可选)
- 不vendor riscv-dhrystone (当前自包含算法完整)
- 不实装 BTB / GShare (M2 stub 留给 feat-bp-btb-gshare)

## Acceptance

- [ ] **AC1**: `link_dhrystone.ld` 新增, base=0x80000000 + .tohost section @ 0x80001000
- [ ] **AC2**: `dhrystone.c` `_start` 加 sp init (汇编 stub 或 inline asm), `tohost_pass()` 改写窗口内 tohost symbol
- [ ] **AC3**: `test_dhrystone.cpp` 删 window_base fallback, 改 mem_cfg hardcode base=0x80000000
- [ ] **AC4**: 公式修为 `DMIPS/MHz = N*1e6/(1757*cycles)`, gate 改 813,043 cycles
- [ ] **AC5**: `kMaxCycles` 降 5,000,000 (从 100,000,000)
- [ ] **AC6**: `src/cf_plugin/CMakeLists.txt` add_custom_command 加 dhrystone target, 传 `-O2 -fno-inline`
- [ ] **AC7**: `[cpu-integration][dhrystone] dhrystone_legacy_tohost_pass` PASS (实跑 2000 iter exited)
- [ ] **AC8**: `[cpu-integration][dhrystone] dhrystone_fsm_dmips_mhz_minimum_1_4` PASS (gate 813,043 cycles)
- [ ] **AC9**: 真实 DMIPS/MHz baseline 测量 (legacy + fsm mode × 2000 iter), 写 csv
- [ ] **AC10**: `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` 重写 HEAD 实测真基线
- [ ] **AC11**: `openspec/specs/dhrystone-benchmark/spec.md` 新建 spec-delta (1 Requirement + 1 Scenario)
- [ ] **AC12**: 0 regression: `[cpu]` 129/129, `[riscv-tests] 48/48`, `[mmu] 59/59, `[cpu-integration][mul-div-fsm-integration]` 4/4
- [ ] **AC13**: 架构 gate 0 FAIL (verify_adr 33+verify_plugin_decision 8+check_plugin_portability 12)
- [ ] **AC14**: 0 product code change 超出 harness 修复 4 文件 (test_dhrystone.cpp, dhrystone.c, link_dhrystone.ld, CMakeLists.txt) — 严禁动 mfc mul_div_fsm.h / cpu_factory.h

## 技术约束 (MUST / MUST NOT)

### MUST
- **MUST** 保持 `CPUConfig::enable_mmu=true` 默认 (D4 沿用)
- **MUST** Bare-mode byte-identical with v0.10.4 (cpu-pipeline-tick-fix AC9 evidence 沿用)
- **MUST** 0 regression on `[cpu] 129/129` (cpu-pipeline-tick-fix 已锁定)
- **MUST** 真基线数据写入 csv 时引用本 change commit (AGENTS.md 数字维护原则)
- **MUST** `RISCV_GPP -O2 -fno-inline` 编译 flag (B3 硬约束)
- **MUST** 标准 DMIPS 公式 `N*1e6/(1757*cycles)` (B2 硬约束)
- **MUST** 真 gate 813,043 cycles (从 2,510,000 改正)

### MUST NOT
- **MUST NOT** 改 `CPUConfig::enable_mmu=false` (D4 沿用)
- **MUST NOT** 改 `ip/cpu/arch/riscv/mul_div_fsm.h` (真 radix-2 已落地, 4/4 PASS)
- **MUST NOT** 改 `picolibc_host_memory.h` 静默 drop 行为 (N3 留后续)
- **MUST NOT** 实装 BTB / GShare (feat-bp-btb-gshare 责任)
- **MUST NOT** vendor riscv-dhrystone (当前源码够用)

## 依赖 / 协调

### 前置 (manual_deps)

- ✅ `cpu-pipeline-tick-fix` (archived 2026-10-16, commit `082b07a`) — cpu_sim harness 修绿, 测量底座解锁
- ⏸ `mfc-cpu-pipeline-multi-cycle-fsm` (50/60 IN_PROGRESS) — 不直接相关 (本 change 不动 mfc), 但 G.2 受益
- ⏸ `feat-bp-btb-gshare` (active feature, phase-4) — 不阻塞, 但本 change 决策 PoC-6 投入顺序

### 并行 (本 change 期间)

- `mfc-cpu-pipeline-multi-cycle-fsm` D.4 / F / G.2 / H.1-5 (12 task pending) — mfc owner 责任, 本 change 不动
- `feat-bp-btb-gshare` 可启动 design.md (本 change 拿到真基线后)

### 后置 (本 change archive 后)

- `mfc G.2 (DMIPS/MHz ≥ 1.4)` 重启 (本 change 拿到真基线, 决策 mfc G.2 是否可独立达成, 还是必须等 BTB)
- `feat-bp-btb-gshare PoC-6 CoreMark` 启动 (本 change harness 复用)
- v0.11.0 launch gate 判据 #3 兑现 (本 change 决定能否)

## 风险 / 决策点

- **R1 (高)**: 修好 harness 后, 1.4 DMIPS/MHz 物理是否可达? 期望 ~0.5-1.0 (本 change 真基线), 推到 1.4 需 BTB. **决策点**: 真基线 < 1.0 → 本 change archive 后立刻启动 feat-bp-btb-gshare; ≥ 1.0 → mfc G.2 可独立重启动.
- **R2 (中)**: `load_section` 静默 drop 仍存在, 后续若其他 benchmark 踩此坑, 需修 picolibc_host_memory.h 加 debug WARN (N3 候选). 本 change 不动.
- **R3 (低)**: dhrystone 基准 vs VexiiRiscv 偏差 ≤10% 的验收 (D3) 需等 feat-bp-btb-gshare + mfc G.2 落地. 本 change 仅给真基线, 不报偏差.

## 主题 / 架构依据

**主题**: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
**来源**: `objective-soc-cpu-v0110-launch.md §8 Sprint 1 判据 #3` + `.rddf/roadmap/features/feat-dhrystone-dmips-gate.md` (现有 stub 状态 active, 4 acceptance 未打勾) + 本会话 Oracle + Explore D1 确认

**架构引用**:
- **ADR-049** (MemoryInterface 抽象): harness 修不破 MemoryInterface 契约
- **AGENTS.md §数字维护原则**: CSV 重写必须引本 change commit hash
- **`openspec/specs/riscv-tests-fixture:74-80`**: 与本 change 修 harness 模式一致 (base=0x80000000 + .tohost section)
- **`openspec/changes/archive/2026-10-15-cpu-sim-add-elf-tohost-fix/`**: add.elf harness 修复先例 (link_add.ld 模式)

**关联 ADR/决定**:
- **D4 沿用**: `CPUConfig::enable_mmu=true` 默认 (cpu-pipeline-tick-fix 锁定, 2026-10-15 user-locked)
- **mfc 真 radix-2 已落地** (commit `df3bcea` 2026-10-10, `[riscv-tests] rv32um 8/8 PASS`): 不是 1.4 DMIPS/MHz 的 binding constraint
- **feat-bp-btb-gshare (PoC-6 CoreMark ≥ 1.9)**: 大概率是 BTB 的 binding constraint (待本 change 真基线后确认)
