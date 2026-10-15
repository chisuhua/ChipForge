# feat-dhrystone-dmips-gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 3-layer harness contract violations (H1 tohost out-of-window drop, H2 .text silently dropped, H3 no crt0) + DMIPS formula dimensional error (3.087×) + missing `-O2` build flag to obtain a REAL Dhrystone DMIPS/MHz baseline, on which v0.11.0 launch gate §8 Sprint 1 判据 #3 (DMIPS/MHz ≥ 1.4) and feat-bp-btb-gshare PoC-6 CoreMark both depend.

**Architecture:**
- **Phase A (harness contract repair)**: 3 changes — `link_dhrystone.ld` (new), `dhrystone.c` (`_start` + `tohost_pass`), `test_dhrystone.cpp` (window_base + formula + gate). **Must precede Phase B** — no meaningful measurements without working tohost.
- **Phase B (build flag)**: `src/cf_plugin/CMakeLists.txt` add dhrystone to `add_custom_command` foreach list with `-O2 -fno-inline` (per Dhrystone convention).
- **Phase C (real baseline)**: Run legacy + fsm mode × 2000 iter; record true DMIPS/MHz; rewrite CSV; commit.
- **Phase D (gate wiring + spec-delta)**: `[cpu-integration][dhrystone]` 2 TEST_CASE PASS + `openspec/specs/dhrystone-benchmark/spec.md` 1 Requirement + 1 Scenario.

**Tech Stack:** C++23 (existing), Catch2 v3.7.0 (vendored at `tests/catch2/`), `riscv32-unknown-elf-g++` (dhrystone.c compile, NEW — was never in build pipeline), `tests/cpu/integration/test_dhrystone.cpp` (existing test infra), `openspec` CLI.

**Hard Constraints (D4 沿用 + 新约束)**:
- **保持** `CPUConfig::enable_mmu=true` 默认 (cpu-pipeline-tick-fix D4 沿用)
- **不**改 `ip/cpu/arch/riscv/mul_div_fsm.h` (真 radix-2 已落地, 4/4 PASS)
- **不**改 `ip/cpu/cpu_factory.h` (D4)
- **不**实装 BTB / GShare (feat-bp-btb-gshare 责任)
- **不**vendor riscv-dhrystone (当前自包含源码算法完整)
- **不**改 `picolibc_host_memory.h` 静默 drop 行为 (N3 留后续)

---

## File Structure

### Production Code (改 4 文件 + 新增 1 单测)

| File | Responsibility |
|---|---|
| `tests/cpu/manual_elf/link_dhrystone.ld` (新, ~50 行) | 仿 link_add.ld 模式: base=0x80000000 + .tohost section @ 0x80001000 |
| `tests/cpu/manual_elf/dhrystone/dhrystone.c` (改 `_start` + `tohost_pass`) | 加 sp init (inline asm `li sp, 0x8000F000`); tohost_pass 写窗口内 tohost symbol |
| `tests/cpu/integration/test_dhrystone.cpp` (改 ~5 处) | mem_cfg hardcode base=0x80000000; 公式修为 `N*1e6/(1757*cycles)`; gate 813,043; kMaxCycles 5M; 删 window_base fallback |
| `src/cf_plugin/CMakeLists.txt` (加 dhrystone target) | add_custom_command foreach 加 dhrystone item, 传 `RISCV_GPP -O2 -fno-inline` |

### Tests / Specs

| File | Test Coverage |
|---|---|
| `openspec/specs/dhrystone-benchmark/spec.md` (新建) | DMIPS 公式 + 真 gate 1 Requirement + 1 Scenario |
| `[cpu-integration][dhrystone] dhrystone_legacy_tohost_pass` (改) | exited + cycles < 5M cap |
| `[cpu-integration][dhrystone] dhrystone_fsm_dmips_mhz_minimum_1_4` (改) | cycles ≤ 813,043 (= 1.4 DMIPS/MHz 真 gate) |

### Reference (不改动)

| File | Read-only reason |
|---|---|
| `tests/cpu/manual_elf/dhrystone/dhrystone.c:69-73` | `tohost_pass()` 现状 (Oracle H1 evidence) |
| `ip/cpu/picolibc_host_memory.h:72-77, 141-146` | `write_byte`/`load_section` 静默 drop 行为 (Oracle H1/H2 evidence, 本 change 不动) |
| `ip/cpu/arch/riscv/mul_div_fsm.h:207-274` | 真 radix-2 iterative (Oracle Task 4 evidence, 本 change 不动) |
| `ip/cpu/cpu_factory.h:136` | D4 sentinel (本 change 不动) |
| `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` | 旧 baseline (本 change 重写, 不读) |

---

## Phase A — Harness Contract Repair (H1 + H2 + H3)

### Task 1: H1+H2 — 新建 `link_dhrystone.ld` + 改 `dhrystone.c` 的 `tohost_pass`

**Files:**
- Create: `tests/cpu/manual_elf/link_dhrystone.ld` (新)
- Modify: `tests/cpu/manual_elf/dhrystone/dhrystone.c:69-73` (`tohost_pass`)
- Modify: `tests/cpu/manual_elf/dhrystone/dhrystone.c:219+` (`_start` — 但 H3 在 Task 2 单独做, 本 Task 仅 `tohost_pass`)

- [ ] **Step 1: 静态读 `link_add.ld` 完整代码** (anchor for new linker script)

Use: `read` tool on `tests/cpu/manual_elf/link_add.ld`
Expected: 30-50 行 ld script, 含 base=0x80000000 + `tohost = .` 等

- [ ] **Step 2: 写 `tests/cpu/manual_elf/link_dhrystone.ld`**

```ld
/* tests/cpu/manual_elf/link_dhrystone.ld
 *
 * feat-dhrystone-dmips-gate: Dhrystone benchmark harness repair (B1).
 *   - .text @ 0x80000000 (in-window 0x80000000-0x80010000)
 *   - tohost symbol @ 0x80001000 (1 word picolibc convention)
 *   - entry: _start (= 0x80000000)
 *
 * 跟 add.elf 的 link_add.ld 同模式 (cpu-pipeline-tick-fix precedent).
 * 关键差异: dhrystone 是 C++ 源码 (extern "C" + C++ runtime), 需 g++ 编译 (-O2 -fno-inline)
 *           而不是 riscv-as/ld (add.elf 的纯汇编路径).
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  .text : {
    *(.text.init)
    *(.text)
    *(.text.*)
  }
  . = ALIGN(0x1000);
  /* riscv-tests env/p 约定: tohost 是 4KB 对齐后的位置 */
  tohost = .;
  PROVIDE(tohost = .);  /* 双保险: 显式 PROVIDE 即使未引用也保留 */

  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss : { *(.bss) *(.bss.*) *(COMMON) }
  _end = .;
}
```

- [ ] **Step 3: 改 `dhrystone.c:69-73` `tohost_pass()` 写窗口内 tohost symbol**

Use: `edit` tool on `tests/cpu/manual_elf/dhrystone/dhrystone.c`

Replace:
```c
void tohost_pass(void) {
  volatile int *p = (volatile int *) 0;  /* 写绝对地址 0 (Oracle H1: window_base 0x710, 窗口外 drop) */
  *p = 1;
}
```

With:
```c
extern char tohost;  /* link_dhrystone.ld . = 0x80001000 */

void tohost_pass(void) {
  volatile int *p = (volatile int *) &tohost;  /* 写 link_dhrystone.ld 锚定的 tohost symbol @ 0x80001000, 在 PicolibcHostMemory 窗口 [0x80000000, 0x80010000) 内 */
  *p = 1;
}
```

- [ ] **Step 4: 验证 readelf — dhrystone.elf (re-build 后) .text @ 0x80000000, .tohost section 存在**

Run:
```bash
# 临时手 build (Task 4 之前)
PATH="/workspace/main/opt/riscv/bin:$PATH" \
  riscv32-unknown-elf-g++ -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32 \
    -nostdlib -static \
    -T tests/cpu/manual_elf/link_dhrystone.ld \
    tests/cpu/manual_elf/dhrystone/dhrystone.c \
    -o /tmp/opencode/dhrystone_v1.elf
# Verify
/workspace/main/opt/riscv/bin/riscv32-unknown-elf-readelf -h /tmp/opencode/dhrystone_v1.elf | grep Entry
/workspace/main/opt/riscv/bin/riscv32-unknown-elf-readelf -S /tmp/opencode/dhrystone_v1.elf | grep -E "tohost|text"
```
Expected: `Entry point address: 0x80000000` + `.text` sh_addr=0x80000000 + `.tohost` section (or `tohost` symbol visible)

- [ ] **Step 5: Defer commit** (本 Task 改动在 Phase A 末尾一起 commit)

### Task 2: H3 — `_start` 加 sp init (crt0 替代)

**Files:**
- Modify: `tests/cpu/manual_elf/dhrystone/dhrystone.c:219` (`_start` 加 inline asm sp init)

- [ ] **Step 1: 静态读 `_start` 完整代码**

Use: `read` tool on `tests/cpu/manual_elf/dhrystone/dhrystone.c` line 215-230
Expected: `extern "C" void _start(void) { ... }` 调 Proc_5/Proc_4 等

- [ ] **Step 2: 改 `_start` 加 inline asm sp init**

Use: `edit` tool

Replace:
```c
extern "C" void _start(void) {
  /* (原始 _start body, 调 Proc_5/Proc_4 等) */
}
```

With:
```c
extern "C" void _start(void) {
  /* feat-dhrystone-dmips-gate H3: sp init (crt0 替代).
   *   RegFilePlugin default T{0} (reg_file.h:84), sp=x2 恒 0.
   *   若 sp=0 调 Proc_5 (用栈) → push 写 0xFFFFFFFC (窗口外, drop) → ret 弹回 ra=0 → 死循环.
   *   解法: _start 入口设 sp = base + 60K (避开 .bss 末尾 0x80010000 - 0x8000F000 区域).
   */
  __asm__ volatile (
    "li sp, 0x8000F000\n\t"  /* sp = 0x80000000 + 60K (PicolibcHostMemory 64K 窗口内) */
    :
    :
    : "sp"
  );
  /* (原始 _start body, 调 Proc_5/Proc_4 等) */
}
```

注: sp=0x8000F000 留 0x1000 给 stack growth, 0x80010000 是 window end.

- [ ] **Step 3: 验证 sp init 编译 OK**

Run:
```bash
PATH="/workspace/main/opt/riscv/bin:$PATH" \
  riscv32-unknown-elf-g++ -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32 \
    -nostdlib -static \
    -T tests/cpu/manual_elf/link_dhrystone.ld \
    -c tests/cpu/manual_elf/dhrystone/dhrystone.c \
    -o /tmp/opencode/dhrystone_v2.o 2>&1 | tail -5
```
Expected: 0 error, /tmp/opencode/dhrystone_v2.o 写出

- [ ] **Step 4: Defer commit**

### Task 3: 测试 window_base 修 + 公式修为标准量纲 + gate 真值

**Files:**
- Modify: `tests/cpu/integration/test_dhrystone.cpp:27-28, 39-44, 89-93, 112-113`

- [ ] **Step 1: 静态读 test_dhrystone.cpp 完整 (重点 line 20-130)**

Use: `read` tool
Expected: `kMaxCycles`, `kMaxCyclesForDmips14`, `window_base` fallback, `runDhrystone`, 2 TEST_CASE (`dhrystone_legacy_tohost_pass` line 84, `dhrystone_fsm_dmips_mhz_minimum_1_4` line 109)

- [ ] **Step 2: 改 `kMaxCycles` 5M (从 100M)**

Use: `edit` tool
Replace:
```cpp
constexpr std::uint64_t kMaxCycles = 100000000;  // 100M cycle cap (mfc Phase G C': 验证 livelock vs performance vs 10M cap)
```

With:
```cpp
/* feat-dhrystone-dmips-gate AC2: 降 cap 5M (从 100M).
 *   旧 100M cap 永远 timeout (100M * pb->run() wall-clock 超 ctest timeout → SIGTERM).
 *   5M cap 是真 gate 813,043 × 6 余量. N1 fix 顺带治愈 AGENTS.md §known SIGTERM.
 */
constexpr std::uint64_t kMaxCycles = 5000000;  // 5M cycle cap
```

- [ ] **Step 3: 改 `kMaxCyclesForDmips14` 真 gate 813,043**

Use: `edit` tool
Replace:
```cpp
constexpr std::uint64_t kMaxCyclesForDmips14 =
    (1757ULL * kDhrystoneRuns) / 14 * 10;  // = 2,510,000 (= 1757*2000*10/14)
```

With:
```cpp
/* feat-dhrystone-dmips-gate B2: 真 gate 813,043 cycles (= N*1e6/(1757*1.4)).
 *   旧公式 1757*N/14*10 = 2,510,000 是错误 (3.087× 宽). 旧 commit 修 B1+B2+B3 后, 公式重对.
 *   N = kDhrystoneRuns = 2000, target = 1.4 DMIPS/MHz → cycles ≤ 2000*1e6/(1757*1.4) = 813,043.
 */
constexpr std::uint64_t kMaxCyclesForDmips14 =
    (kDhrystoneRuns * 1'000'000ULL) / (1757 * 14) * 10;  // = 813,043
```

- [ ] **Step 4: 改 window_base fallback (删 + hardcode base=0x80000000)**

Use: `edit` tool, in `runDhrystone` 找 `window_base` 计算 block (line 39-44)

Replace:
```cpp
std::uint64_t window_base = elf.entry_addr &
                            ~static_cast<std::uint64_t>(0xFFFF);
if (window_base == 0 && elf.entry_addr != 0) window_base = elf.entry_addr;
cf::cpu::PicolibcHostMemory::Config mem_cfg{
    .base_addr = window_base, .size = 64 * 1024,
    .tohost_addr = tohost_addr};
```

With:
```cpp
/* feat-dhrystone-dmips-gate B1 part c: hardcode base=0x80000000.
 *   旧 window_base = entry & ~0xFFFF; if(window_base==0 && entry!=0) window_base=entry;
 *   让 .text@0x0 (link.ld base=0) 整段被 load_section 静默 drop (Oracle H2 升级).
 *   修法: 与 link_dhrystone.ld 锁 base=0x80000000 一致.
 */
constexpr std::uint64_t kDhrystoneBase = 0x80000000;
cf::cpu::PicolibcHostMemory::Config mem_cfg{
    .base_addr = kDhrystoneBase, .size = 64 * 1024,
    .tohost_addr = kDhrystoneBase + 0x1000};  /* tohost @ base + 4K, 与 link_dhrystone.ld ALIGN(0x1000) 一致 */
```

注: 也删 `tohost_addr` 单独 fallback (link_dhrystone.ld 有 `tohost` symbol, 优先级同 add.elf)

- [ ] **Step 5: 改 DMIPS 公式 (line 89-93 + 112-113)**

Use: `edit` tool, in 2 TEST_CASE 改 `dmips_per_mhz` 计算

Replace (line 89-93 + 112-113 出现 2 次, 各改):
```cpp
INFO("cycles=" << cycles << " ec=" << ec
     << " dmips_per_mhz=" << (1757.0 * kDhrystoneRuns / cycles));
```

With:
```cpp
/* feat-dhrystone-dmips-gate B2: 标准 DMIPS 公式 N*1e6/(1757*cycles).
 *   旧 1757*N/cycles 错 (3.087× 宽, 因 1757²/1e6 = 3.087).
 *   标准 DMIPS/MHz = N * 1e6 / (1757 * cycles).
 */
INFO("cycles=" << cycles << " ec=" << ec
     << " dmips_per_mhz=" << ((double)kDhrystoneRuns * 1e6 / (1757.0 * (double)cycles)));
```

- [ ] **Step 6: 验证 dhrystone test compile OK (无 syntax error)**

Run:
```bash
cd /workspace/project/ChipForge
# 因为 test_dhrystone.cpp 改动后, 必须重新 build chipforge_tests (前 build env 恢复链已 OK)
cmake --build build --target chipforge_tests -j4 2>&1 | grep -E "test_dhrystone|error:" | head -5
```
Expected: `test_dhrystone.cpp.o` 重建, 0 error

- [ ] **Step 7: Defer commit**

---

## Phase B — Build Flag (B3: `-O2 -fno-inline`)

### Task 4: CMakeLists.txt add_custom_command 加 dhrystone target

**Files:**
- Modify: `src/cf_plugin/CMakeLists.txt:119-155` (add_custom_command foreach)

- [ ] **Step 1: 静态读现有 add_custom_command foreach 完整代码**

Use: `read` tool on `src/cf_plugin/CMakeLists.txt` line 115-160
Expected: foreach 循环 4 ELF_PAIR items (add/mmu_bare/l1cache_basic/zicsr_basic), 用 `riscv*-as/ld` 命令, 无 dhrystone

- [ ] **Step 2: 加 dhrystone 到 foreach 列表 + RISCV_GPP 工具链探测**

Use: `edit` tool

Replace:
```cmake
find_program(RISCV_AS riscv64-unknown-elf-as)
find_program(RISCV_LD riscv64-unknown-elf-ld)
if(RISCV_AS AND RISCV_LD)
  # cpu-sim-add-elf-tohost-fix: add.elf uses link_add.ld (riscv-tests style),
  # other ELF (mmu_bare/l1cache_basic) keep link.ld (legacy base=0x0).
  foreach(ELF_PAIR IN ITEMS
      "add;add.S;link_add.ld"
      "mmu_bare;build_mmu_bare.S;link.ld"
      "l1cache_basic;build_l1cache_basic.S;link.ld")
```

With:
```cmake
find_program(RISCV_AS riscv64-unknown-elf-as)
find_program(RISCV_LD riscv64-unknown-elf-ld)
find_program(RISCV_GPP riscv32-unknown-elf-g++)  # feat-dhrystone-dmips-gate B3
if(RISCV_AS AND RISCV_LD)
  # cpu-sim-add-elf-tohost-fix: add.elf uses link_add.ld (riscv-tests style),
  # other ELF (mmu_bare/l1cache_basic) keep link.ld (legacy base=0x0).
  # feat-dhrystone-dmips-gate: dhrystone uses link_dhrystone.ld (base=0x80000000 + .tohost section)
  # + g++ -O2 -fno-inline 编译 (Dhrystone 规范要求开优化).
  foreach(ELF_PAIR IN ITEMS
      "add;add.S;link_add.ld"
      "dhrystone;dhrystone/dhrystone.c;link_dhrystone.ld"  # NEW: harness repair B1
      "mmu_bare;build_mmu_bare.S;link.ld"
      "l1cache_basic;build_l1cache_basic.S;link.ld")
```

- [ ] **Step 3: 加 dhrystone 编译分支 (用 RISCV_GPP, 区别于其他 ELF 用 riscv-as/ld)**

Use: `edit` tool, after `list(GET ELF_PAIR 2 LINK_SCRIPT)` block, 加 dhrystone 特殊路径

```cmake
    list(GET ELF_PAIR 0 ELF_NAME)
    list(GET ELF_PAIR 1 SRC_FILE)
    list(GET ELF_PAIR 2 LINK_SCRIPT)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${SRC_FILE})
      if(ELF_NAME STREQUAL "dhrystone" AND RISCV_GPP)
        # feat-dhrystone-dmips-gate B3: dhrystone C++ 源码, 用 g++ -O2 -fno-inline
        add_custom_command(
          OUTPUT ${CMAKE_BINARY_DIR}/${ELF_NAME}.elf
          COMMAND ${RISCV_GPP} -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32
            -nostdlib -static
            -T ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${LINK_SCRIPT}
            ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${SRC_FILE}
            -o ${CMAKE_BINARY_DIR}/${ELF_NAME}.elf
          DEPENDS
            ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${SRC_FILE}
            ${CMAKE_SOURCE_DIR}/tests/cpu/manual_elf/${LINK_SCRIPT}
          COMMENT "Building ${ELF_NAME}.elf from ${SRC_FILE} + ${LINK_SCRIPT} (-O2 -fno-inline)"
          VERBATIM
        )
      else()
        # 既有 add/mmu_bare/l1cache_basic 路径: riscv-as/ld (无 -O)
        add_custom_command(
          OUTPUT ${CMAKE_BINARY_DIR}/${ELF_NAME}.elf
          ...
        )
      endif()
      add_custom_target(${ELF_NAME}_elf ALL
        DEPENDS ${CMAKE_BINARY_DIR}/${ELF_NAME}.elf})
    endif()
  endforeach()
```

注: 实际 edit 时需把既有 4-args `add_custom_command` 段包在 `else()` 内. 简化: 仅在 dhrystone 路径加 `if()`, 其他路径保留原代码 (避免大块 diff).

- [ ] **Step 4: 删旧 dhrystone.elf (gitignored, 但 stale 缓存可能干扰) + re-config + build**

Run:
```bash
rm -f build/dhrystone.elf build/tests/cpu/manual_elf/dhrystone/dhrystone.elf 2>/dev/null
cmake -S . -B build 2>&1 | tail -2
cmake --build build --target dhrystone_elf -j4 2>&1 | tail -5
ls -la build/dhrystone.elf 2>&1 | head -3
```

Expected: `dhrystone.elf` 重新 build, size ~10-20KB (含 `.text` ~2.5K, `.rodata` ~0.1K, `.bss` ~10K)

- [ ] **Step 5: 验证 readelf — entry=0x80000000, .text @ 0x80000000, .tohost section 存在**

Run:
```bash
/workspace/main/opt/riscv/bin/riscv32-unknown-elf-readelf -h build/dhrystone.elf | grep Entry
/workspace/main/opt/riscv/bin/riscv32-unknown-elf-readelf -S build/dhrystone.elf | grep -E "tohost|text" | head -5
```
Expected: `Entry point address: 0x80000000` + `.text @ 0x80000000` + `.tohost` section visible

- [ ] **Step 6: Defer commit**

---

## Phase C — Real Baseline Measurement

### Task 5: 跑 [cpu-integration][dhrystone] 2 TEST_CASE 验证 (target #1 + gate #1.4)

- [ ] **Step 1: 跑 `dhrystone_legacy_tohost_pass`**

Run:
```bash
cd /workspace/project/ChipForge
cmake --build build --target chipforge_tests -j4 2>&1 | tail -3
./build/bin/chipforge_tests "[cpu-integration][dhrystone] dhrystone_legacy_tohost_pass" 2>&1 | tail -8
```

Expected: `All tests passed` (2000 iter, cycles < 5M cap, exited=true, ec=0)

- [ ] **Step 2: 跑 `dhrystone_fsm_dmips_mhz_minimum_1_4`**

Run:
```bash
./build/bin/chipforge_tests "[cpu-integration][dhrystone] dhrystone_fsm_dmips_mhz_minimum_1_4" 2>&1 | tail -8
```

Expected: `All tests passed` (FSM mode 也进 gate 813,043)

注: 第一跑可能 DMIPS/MHz 仍 < 1.4 (因为缺 BTB). 若 cycles < 813,043 → PASS; cycles > 813,043 → FAIL (暴露 1.4 是 BTB 的 binding constraint).

- [ ] **Step 3: 记录真 DMIPS/MHz baseline**

无论 PASS/FAIL, 记录真 cycles + DMIPS/MHz (extract from INFO log) for CSV 重写.

Run:
```bash
./build/bin/chipforge_tests "[cpu-integration][dhrystone]" -s 2>&1 | grep -E "cycles=|dmips_per_mhz=" | head -10
```

- [ ] **Step 4: 跑 cpu_sim CLI 验证 5 测试仍 5 PASS (回归 baseline)**

Run:
```bash
for f in cpu_embedded cpu_default cpu_superscalar cpu_deep_pipeline; do
  ./build/src/cf_plugin/cpu_sim --elf tests/cpu/manual_elf/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100 --config ip/cpu/configs/$f.json 2>&1 | grep -E "^(tohost|cycles)=" | tr '\n' ' '
  echo "  ($f)"
done
```
Expected: 4/4 cycles=7 tohost=1

- [ ] **Step 5: 跑 [riscv-tests] 0 regression 验证**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -2`
Expected: `48/48 PASS`

- [ ] **Step 6: 跑 [mmu] 0 regression 验证 (D4 sentinel)**

Run: `./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -2`
Expected: `59/59 PASS` (Bare-mode byte-identical)

- [ ] **Step 7: Defer commit**

---

## Phase D — Gate Wiring + Spec-delta + CSV Rewrite

### Task 6: CSV 重写 (N1) + spec-delta (AC11)

**Files:**
- Modify: `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv` (N1 重写)
- Create: `openspec/specs/dhrystone-benchmark/spec.md` (AC11)

- [ ] **Step 1: 看现有 CSV**

Use: `read` tool on `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv`

- [ ] **Step 2: 重写 CSV (N1 满足 AGENTS.md 数字维护原则)**

Use: `write` tool

```csv
elf,status,mode,kMaxCycles,dmips_per_mhz_measured,cycles_per_iter,notes
dhrystone,<PASS|FAIL>,legacy,5000000,<真值>,<真值>,2000 iter Dhrystone harness 修 (link_dhrystone.ld + crt0 sp init); DMIPS/MHz <真值> 标准公式 N*1e6/(1757*cycles); feat-dhrystone-dmips-gate commit <hash>.
dhrystone,<PASS|FAIL>,fsm,5000000,<真值>,<真值>,FSM mode; 1.4 DMIPS/MHz 真 gate cycles ≤ 813,043; BTB 是后续 binding constraint (待 feat-bp-btb-gshare).
```

`<真值>` 填 Step 3 (Task 5) 实测值; commit hash 后续 commit 后填.

- [ ] **Step 3: 看现有 dhrystone spec 路径 (若有)**

Run: `ls openspec/specs/dhrystone* 2>&1 | head -3`

- [ ] **Step 4: 创建 `openspec/specs/dhrystone-benchmark/spec.md` (新 spec-delta)**

Use: `mkdir -p openspec/specs/dhrystone-benchmark && write` tool

```md
## Purpose

Document the Dhrystone benchmark harness contract + DMIPS/MHz formula + real gate, to support v0.11.0 launch gate §8 Sprint 1 判据 #3.

## Requirements

### Requirement: Dhrystone benchmark MUST use standard DMIPS formula

The Dhrystone benchmark in `tests/cpu/integration/test_dhrystone.cpp` MUST use the standard DMIPS/MHz formula:
- `DMIPS/MHz = N * 1e6 / (1757 * cycles)`
- N = iteration count (kDhrystoneRuns = 2000)
- cycles = measured cycle count from `pb->run()` loop

The 1.4 DMIPS/MHz hard gate corresponds to: `cycles ≤ 813,043` (= N * 1e6 / (1757 * 1.4)).

The legacy formula `1757 * N / cycles` (off by 3.087x) is BANNED.

#### Scenario: 2000 iter Dhrystone PASS with 1.4 DMIPS/MHz gate (B1+B2+B3 fixed)
- **GIVEN** `link_dhrystone.ld` (base=0x80000000 + .tohost section @ 0x80001000) + crt0 sp init (inline asm `li sp, 0x8000F000`)
- **AND** `test_dhrystone.cpp` window_base=0x80000000 hardcode (no entry & ~0xFFFF fallback)
- **AND** `RISCV_GPP -O2 -fno-inline` build flag
- **WHEN** 跑 `[cpu-integration][dhrystone] dhrystone_legacy_tohost_pass` + `dhrystone_fsm_dmips_mhz_minimum_1_4`
- **THEN** both TEST_CASE PASS, cycles < 813,043 (= 1.4 DMIPS/MHz 真 gate), exited=true
- **AND** 公式 `DMIPS/MHz = N*1e6/(1757*cycles)` 计算正确 (无 3.087x 宽)

#### Scenario: CSV baseline 引用本 change commit (N1 AGENTS.md 数字维护)
- **GIVEN** `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv`
- **WHEN** 写入 DMIPS/MHz 真值
- **THEN** notes 字段 MUST 引用本 change commit hash (符合 AGENTS.md 数字维护原则)
```

- [ ] **Step 5: 验证 CSV + spec 路径 OK**

Run: `cat openspec/specs/dhrystone-benchmark/spec.md | head -5`
Run: `cat soc/cpu/docs/dse/dhrystone-baseline-matrix.csv | head -3`

- [ ] **Step 6: Defer commit**

### Task 7: 架构 gate 0 FAIL (AC13)

- [ ] **Step 1: 跑架构 gate 三件套**

Run:
```bash
bash tools/verify_adr.sh 2>&1 | grep -E "FAILED|All ✅" | head -3
bash tools/verify_plugin_decision.sh 2>&1 | grep -E "全部通过" | head -1
bash tools/check_plugin_portability.sh 2>&1 | grep -E "全部通过" | head -1
```
Expected: verify_adr ❌ FAILED: 0; verify_plugin_decision 全部通过 (8/8); check_plugin_portability 全部通过 (12/12)

- [ ] **Step 2: 0 regression baseline (AC12) 实测**

Run:
```bash
./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2
./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -2
./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -2
./build/bin/chipforge_tests "[cpu-integration][mul-div-fsm-integration]" 2>&1 | tail -2
./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" 2>&1 | tail -2
```
Expected: 129/129 + 48/48 + 59/59 + 4/4 + 6/6 (cpu-pipeline-tick-fix D4 sentinel 维持)

- [ ] **Step 3: 0 product code change 超出 harness 4 文件 (AC14)**

Run: `git status --porcelain`
Expected:
- `M tests/cpu/integration/test_dhrystone.cpp`
- `M tests/cpu/manual_elf/dhrystone/dhrystone.c`
- `A tests/cpu/manual_elf/link_dhrystone.ld` (new)
- `M src/cf_plugin/CMakeLists.txt`
- `M soc/cpu/docs/dse/dhrystone-baseline-matrix.csv`
- `A openspec/specs/dhrystone-benchmark/spec.md` (new)
- `M openspec/changes/feat-dhrystone-dmips-gate/{proposal,design,tasks,specs/...}.md` (4 件套)
- `A openspec/specs/dhrystone-benchmark/spec.md` (D3 spec-delta 落位)
- `A .rddf/improvements/feat-dhrystone-dmips-gate.md` (草案)
- `A .rddf/plans/feat-dhrystone-dmips-gate.md` (plan)
- **禁止**: `ip/cpu/arch/riscv/mul_div_fsm.h` (D4-already-locked), `ip/cpu/cpu_factory.h` (D4 sentinel), `ip/cpu/plugins/mul_div_fsm_chmem.h` (CH_MEM 共享头)

- [ ] **Step 4: Defer commit**

### Task 8: 1 commit per CONTRIBUTING.md:25 prefix `fix(dhrystone):` 或 `chore(dhrystone):`

- [ ] **Step 1: git add 8-10 files**

Run:
```bash
git add tests/cpu/integration/test_dhrystone.cpp \
        tests/cpu/manual_elf/dhrystone/dhrystone.c \
        tests/cpu/manual_elf/link_dhrystone.ld \
        src/cf_plugin/CMakeLists.txt \
        soc/cpu/docs/dse/dhrystone-baseline-matrix.csv \
        openspec/specs/dhrystone-benchmark/spec.md
```

- [ ] **Step 2: commit (per CONTRIBUTING.md:25 prefix `fix(dhrystone):`)**

Run:
```bash
git commit -m "fix(dhrystone): feat-dhrystone-dmips-gate — 修 harness 3 层契约违反 + 公式 + -O2 (commit <hash>)

[Diagnosis reference]: Oracle + Explore + D1 readelf (2026-10-16)
  旧 CSV baseline 0.0351 DMIPS/MHz 是伪测量值 (100M cap 满预算反推).
  真因: 3 层 harness 契约违反 + 公式量纲错 3.087x + 缺 -O2.

[F1 H1] tests/cpu/manual_elf/dhrystone/dhrystone.c:tohost_pass 写窗口外
  - 旧: volatile int *p = (volatile int*)0  (写绝对地址 0, window_base=0x710, 静默 drop)
  - 新: volatile int *p = (volatile int*)&tohost  (写 link_dhrystone.ld 锚定 @ 0x80001000, 窗口内)

[F1 H2] tests/cpu/manual_elf/link_dhrystone.ld 新建 (50 行 ld script, 仿 link_add.ld)
  - base=0x80000000 + tohost = . (4KB 对齐) + PROVIDE(tohost=.) 双保险
  - 旧 link.ld base=0x0, window_base=entry 0x710 → 整段 .text@0x0 被 load_section drop

[F1 H3] dhrystone.c:_start 加 inline asm sp init (crt0 替代)
  - __asm__ volatile ('li sp, 0x8000F000') 避开 .bss 末尾
  - 旧 RegFilePlugin default T{0} (reg_file.h:84), sp=0 → 调 Proc_5 push 写 0xFFFFFFFC 窗口外

[F1 B1 part c] tests/cpu/integration/test_dhrystone.cpp window_base 修
  - 旧: window_base = entry & ~0xFFFF; if(==0 && entry!=0) window_base=entry
  - 新: hardcode constexpr kDhrystoneBase=0x80000000; mem_cfg.base_addr=kDhrystoneBase
  - tohost_addr = kDhrystoneBase + 0x1000 (与 link_dhrystone.ld ALIGN 锁)

[F2 B2] DMIPS 公式修为标准量纲
  - 旧: 1757 * kDhrystoneRuns / cycles (差 3.087x, 因 1757²/1e6)
  - 新: (double)kDhrystoneRuns * 1e6 / (1757.0 * (double)cycles)
  - 真 gate: cycles ≤ 813,043 (= N*1e6/(1757*1.4))
  - 旧 2,510,000 (kMaxCyclesForDmips14) 修为 (kDhrystoneRuns*1e6)/(1757*14)*10 = 813,043

[B3] -O2 -fno-inline 编译 flag
  - src/cf_plugin/CMakeLists.txt: add_custom_command foreach 加 dhrystone item
  - find_program(RISCV_GPP riscv32-unknown-elf-g++) + 特殊分支 (-O2 -fno-inline -march=rv32im_zicsr)
  - 旧 dhrystone.elf 是手工一次性产物 (-O0); 新增 build automation 强制 -O2

[N2] kMaxCycles 降 5M (从 100M)
  - 100M cap 永远 timeout → wall-clock 超 ctest timeout → SIGTERM (AGENTS.md §known)
  - 5M cap 是真 gate 813,043 × 6 余量, 顺带治愈 SIGTERM

[N1] soc/cpu/docs/dse/dhrystone-baseline-matrix.csv 重写
  - 旧: 0.0351 FAIL ad-hoc busy counter (mul_div_fsm.h:475-500) (pre-df3bcea 过期)
  - 新: <真值> PASS legacy/fsm × 2000 iter + 公式说明 + commit hash 引用

[Spec-delta] openspec/specs/dhrystone-benchmark/spec.md 新建
  - 1 Requirement: 标准 DMIPS 公式 + 真 gate 813,043
  - 2 Scenario: 1.4 DMIPS/MHz gate PASS + CSV commit hash 引用

[Verification - AC 兑现 (实测 2026-10-16)]:
- AC1-AC3 link_dhrystone.ld + dhrystone.c + test_dhrystone.cpp 修
- AC4 公式修为标准量纲 + gate 813,043
- AC5 kMaxCycles 5M
- AC6 CMakeLists.txt add_custom_command 加 dhrystone target
- AC7-AC8 [cpu-integration][dhrystone] 2 TEST_CASE <PASS|FAIL by 真基线>
- AC9 真 DMIPS/MHz baseline 测量
- AC10 CSV 重写 HEAD 实测
- AC11 openspec/specs/dhrystone-benchmark/spec.md 新建
- AC12 0 regression: [cpu] 129/129 + [riscv-tests] 48/48 + [mmu] 59/59 + [cpu-integration][mul-div-fsm-integration] 4/4
- AC13 架构 gate 0 FAIL
- AC14 0 product code change 超出 harness 4 文件 (mfc mul_div_fsm.h / cpu_factory.h 不动)

[D4 沿用 cpu-pipeline-tick-fix]: CPUConfig::enable_mmu=true 默认 保持, 不动 ip/cpu/cpu_factory.h.

[Out of scope]: BTB / GShare 实装 (feat-bp-btb-gshare 责任) + picolibc_host_memory.h 静默 drop WARN (N3 留后续) + vendor riscv-dhrystone (当前自包含源码算法完整)"

- [ ] **Step 3: 验证 commit + git log**

Run: `git log -3 --oneline | head -5`
Expected: 1 commit + 上一 commit (cpu-pipeline-tick-fix archive `082b07a`)

---

## Acceptance Criteria (与 improvement AC sync)

| AC | 描述 | 验证 Task |
|---|---|---|
| **AC1** | link_dhrystone.ld 新建 | Task 1 Step 2 |
| **AC2** | dhrystone.c _start + tohost_pass 修 | Task 1 Step 3 + Task 2 Step 2 |
| **AC3** | test_dhrystone.cpp window_base hardcode | Task 3 Step 4 |
| **AC4** | 公式修为标准量纲 + gate 813,043 | Task 3 Step 3 + Step 5 |
| **AC5** | kMaxCycles 5M | Task 3 Step 2 |
| **AC6** | CMakeLists.txt add dhrystone target + -O2 -fno-inline | Task 4 |
| **AC7** | [cpu-integration][dhrystone] dhrystone_legacy_tohost_pass PASS | Task 5 Step 1 |
| **AC8** | [cpu-integration][dhrystone] dhrystone_fsm_dmips_mhz_minimum_1_4 PASS | Task 5 Step 2 |
| **AC9** | 真 DMIPS/MHz baseline 测量 | Task 5 Step 3 |
| **AC10** | CSV 重写 HEAD 实测 | Task 6 Step 2 |
| **AC11** | spec-delta 新建 | Task 6 Step 4 |
| **AC12** | 0 regression baseline | Task 7 Step 2 |
| **AC13** | 架构 gate 0 FAIL | Task 7 Step 1 |
| **AC14** | 0 product code change 超出 4 文件 | Task 7 Step 3 |

## Fallback Strategy

| Scenario | Fallback |
|---|---|
| dhrystone.c compile error (g++ -O2 -fno-inline 不接受 inline asm) | 改用启动 stub 在 link script `_start = _start_real; _start_real: { li sp, 0x8000F000; jal _start_cpp }` (.S 文件) |
| 5M cap 仍 timeout (B1 修复未到位, e.g. window_base 仍错) | 退化到 baseline 重测, 加 cerr log 到 load_section 看实际 drop 行为 |
| DMIPS/MHz 仍 0 (FSM mode 走 busy counter 而非 radix-2) | 验证 mfc-extract-fsm-h 落地, 显式检查 mul_div_fsm.h:207-274 on_tick 实装 |
| 1.4 DMIPS gate < 真基线 (e.g. 0.8) | R1 高风险显化, 标 BTB 是 binding constraint, 启动 feat-bp-btb-gshare 优先 |
| 5 测试 family 跑不回来 (build env regression) | 重新跑 commit `4e5ae36` 的 build env recovery 流程 (CXX=g++-14 + cpphdl_external-install) |

## Estimated Timeline

| Phase | Tasks | 估计时间 |
|---|---|---|
| Phase A (Harness) | Task 1-3 | 60 分钟 (3 文件改动 + 编译验证) |
| Phase B (Build) | Task 4 | 30 分钟 (CMakeLists.txt 加 dhrystone target + 验证) |
| Phase C (Baseline) | Task 5 | 30 分钟 (跑 2 TEST_CASE + 0 regression baseline) |
| Phase D (Spec) | Task 6-8 | 30 分钟 (CSV + spec + 1 commit) |
| **Total** | 8 tasks | **~2.5 小时 / 1 session** |

## Notes

- **Oracle 4 项 BLOCKERS** (B1 三项 + B2 + B3) 是本 change 真 scope. Oracle 5 项 SHOULD_FIX (R1/R2/R3) + 4 NITS 留 follow-up.
- **mfc G.2 不在本 change scope**: mfc-cpu-pipeline-multi-cycle-fsm 50/60 IN_PROGRESS 仍由 mfc owner 推进. 本 change 拿真基线后, mfc G.2 可重启动.
- **R1 决策点**: 真基线 ≥ 1.0 DMIPS/MHz → mfc G.2 可独立重启动; < 1.0 → feat-bp-btb-gshare 优先. 此决策由 mfc owner + feat-bp-btb-gshare owner 联合, 本 change 提供数据.
- **CSV 引用 commit hash**: 实际 commit 后填. AGENTS.md §数字维护原则硬约束.
- **Phase B 风险**: RISCV_GPP 路径在某些 minimal Docker 镜像可能缺失. 主会话已确认 `/workspace/main/opt/riscv/bin/riscv32-unknown-elf-g++` 存在 (build env 已预装 GNU 16.1.0), OK.
- **build env 状态**: 上一 change `4e5ae36` (chipforge_tests binary 80MB) + commit `23783a3` 等已知 commit 已恢复完整 C++ API 测试链. 本 change 不再 build env 风险.
