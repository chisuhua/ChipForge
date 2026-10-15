# feat-dhrystone-dmips-gate

## Why

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

## What Changes

**In Scope**:

- **B1 harness 修复 (3 项)**:
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

**Out of Scope**:

- ❌ **不**改 `CPUConfig::enable_mmu` 默认 (D4 硬约束, 沿用 cpu-pipeline-tick-fix D4)
- ❌ **不**动 mfc-cpu-pipeline-multi-cycle-fsm (D.4/F/G/H 12 task 是 mfc owner 责任)
- ❌ **不**实装 BTB / GShare 真接线 (branch_predictor.h 已是 M2 stub, 留给 feat-bp-btb-gshare)
- ❌ **不**vendor riscv-dhrystone (当前自包含源码算法完整, 无 printf/malloc 依赖)
- ❌ **不**改 `picolibc_host_memory.h` 静默 drop 行为 (tohost 窗口外 WARN 建议 N3, 但本 change 不动 framework)
- ❌ **不**改 `ip/cpu/arch/riscv/mul_div_fsm.h` (真 radix-2 已落地, 4/4 PASS)

## Capabilities

- (no items specified)

## Impact

- ### Affected tests

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

