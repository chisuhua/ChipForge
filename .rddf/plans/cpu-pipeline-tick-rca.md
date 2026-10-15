# cpu-pipeline-tick-rca Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Diagnose root cause of 5 RISC-V simulation tests FAILING on `cpu_sim` CLI while PASSING (48/48) on C++ API path, using a 2×2 control experiment matrix + compound 3-layer hypothesis (F1 config fidelity + F2 test config intent + F3 MMU transient semantics) + F4 reverse counter-factual. Pure diagnosis change — no product code fixes (those go to follow-up `cpu-pipeline-tick-fix`).

**Architecture:**
- **In-place execution** (no worktree, per `rdd-builder` P1 plan-gen — diagnostic, no debug instrumentation beyond patch-file temporary flags)
- **3-stage methodology**: §0 baseline capture → §1 2×2 control experiment (5 pipeline runs) → §2 stop-and-document per investigation line (L2/L3/L1, L4 already done in §1)
- **D4 decisions locked**: keep `CPUConfig::enable_mmu=true` default; fix direction is `(a) load_config() parse dropped fields + (b) test configs change to false + (c) cpu_sim --no-mmu override flag`
- **Acceptance anchor**: 5 FAIL tests remain FAIL until `cpu-pipeline-tick-fix` change runs (this change does NOT fix them)

**Tech Stack:** Catch2 v3 (existing), `tools/cpu_sim` CLI (`tools/cpu_sim/main.cpp`), cpu_sim ManualRunner ELF harness, `tests/cpu/{configs,integration,riscv_tests,test_cpu_sim_real_tohost.cpp}`

---

## File Structure

| File | Role |
|---|---|
| `openspec/changes/cpu-pipeline-tick-rca/design.md` | Diagnostic report (5-stage output; per-line stop-and-document blocks live here) |
| `openspec/changes/cpu-pipeline-tick-rca/tasks.md` | 38+ checkbox execution tracking |
| `openspec/changes/cpu-pipeline-tick-rca/specs/cpu-pipeline-tick-rca/spec.md` | Diagnostic methodology contract |
| `openspec/specs/cpu-pipeline-tick-rca/spec.md` | Auto-derived spec-delta (rdd-builder D3, post-P0-approve skeleton) |
| `.rddf/state/builder/cpu-pipeline-tick-rca.json` | rdd-builder handoff v1.1 |

### Read-only inspection (无写入)

| File | Read-only reason |
|---|---|
| `tools/cpu_sim/main.cpp` (--no-mmu patch + revert) | Temporary `--no-mmu` flag for §1 control experiment |
| `ip/cpu/cpu_factory.h` | Read F3 evidence chain (cpu_factory.h:136 default + 444 Sv39 auto-encode) |
| `ip/cpu/plugins/ibus.h` | Read L2 halt_when lambda |
| `ip/cpu/plugins/mmu.h` + `ip/mmu/tlm/MMUPlugin.{h,cpp}` + `ip/mmu/tlm/mmu_keys.h` | Read Bare shortcut + PTW_ACTIVE key |
| `include/cf/plugin/pipe_builder.h` | Read stall dispatch + halt_when evaluation |
| `tests/cpu/configs/{cpu_embedded,cpu_default,cpu_superscalar,cpu_deep_pipeline}.json` | Read F1/F2 evidence (declared enable_mmu value) |
| `tests/cpu/test_cpu_sim_real_tohost.cpp` + `tests/cpu/integration/test_{3,5,7,10}stage_riscv.cpp` | Read exec_cmd CLI invocation + base-addr/tohost-addr args |
| `tests/cpu/riscv_tests/elf/rv32ui-p-add` | Used in F4 reverse counter-factual |

### Forbidden (read + write)

- `build/_deps/**` (CppTLM/CppHDL)
- `**/*_chmem.h` (CH_MEM files)
- `ip/mmu/lib/**` + `ip/mmu/chmem/**`
- All `ip/cpu/plugins/*` write (L2/L3 may read but NOT modify)

---

## Phase 0 — Baseline Capture + 5-测试独立基线台账

### Task 1: 验证 build environment + 3 个架构 gate baseline

**Files:**
- Verify: `./build/bin/chipforge_tests` (TLM binary)
- Verify: `./build/bin/cpu_sim` (CPU CLI binary)
- Verify: `./build/_deps/install/` (CppTLM/CppHDL install)

- [ ] **Step 1: 确认 cpu_sim binary 可执行**

Run: `test -x ./build/src/cf_plugin/cpu_sim && echo OK || echo MISSING`
Expected: `OK` (若不存在 → 跑 `cmake --build build --target cpu_sim -j4`)

- [ ] **Step 2: 确认 5 个 FAIL 测试 ELF 存在**

Run: `test -f tests/cpu/manual_elf/add.elf && echo OK || echo MISSING`
Expected: `OK`

- [ ] **Step 3: 跑架构 gate (3 件套, 预期 0 FAIL)**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh 2>&1 | tail -10`
Expected: 三件套均 `PASS` (`verify_adr 33/33` + `verify_plugin_decision 8/8` + `check_plugin_portability 12/12`)

- [ ] **Step 4: TLM 基线测试 — `[riscv-tests]` 必须 48/48 PASS**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -5`
Expected: `48/48 PASS` (硬约束)

- [ ] **Step 5: TLM 基线测试 — `[cpu]` 应为 128/129 PASS (1 FAIL 是已知)**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -5`
Expected: `128/129 PASS` 含 `1 FAIL: test_cpu_sim_real_tohost`

- [ ] **Step 6: TLM 基线测试 — `[cpu-integration]` 应为 30/35 PASS (4 FAIL 是已知)**

Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -5`
Expected: 含 `4 FAIL: test_*stage_add_elf_end_to_end` + `1 SIGTERM: dhrystone`

- [ ] **Step 7: 记录当前 HEAD commit hash (后续 control experiment 锚点)**

Run: `git rev-parse HEAD | tee /tmp/opencode/cpu-rca-baseline-head.txt`
Expected: `40-char hash` 写出

### Task 2: 固化 5-测试独立基线台账 (D3 决策, design.md §0.8 落地)

**Files:**
- Modify: `openspec/changes/cpu-pipeline-tick-rca/design.md` (新增 §0.8 节, 内嵌台账)
- Read-only: 5 个 FAIL 测试配置文件 (用于填 F1/F2 evidence)

- [ ] **Step 1: 在 `design.md` 现有 §0/§1/§2 之前插入 §0.8 节标题**

Use: `edit` tool to inject `## §0.8 5-测试独立基线台账 (D3 决策)` between current §0 and §1 markers

- [ ] **Step 2: 静态读 4 个 config 文件的 `enable_mmu` 字段值**

Run:
```bash
for f in tests/cpu/configs/cpu_embedded.json tests/cpu/configs/cpu_default.json tests/cpu/configs/cpu_superscalar.json tests/cpu/configs/cpu_deep_pipeline.json; do
  echo "=== $f ==="
  grep -E '"enable_mmu"|"pipeline_stages"|"dispatch_width"|"n_lanes"' "$f" || echo "(no match)"
done
```
Expected:
- `cpu_embedded.json`: `enable_mmu: false`, `pipeline_stages: 3`
- `cpu_default.json`: `enable_mmu: true`, `pipeline_stages: 5`
- `cpu_superscalar.json`: `enable_mmu: true`, `pipeline_stages: 7`, `dispatch_width: 2`, `n_lanes: 2`
- `cpu_deep_pipeline.json`: `enable_mmu: true`, `pipeline_stages: 10`

- [ ] **Step 3: 静态读 5 个测试的 `--config` 调用约定**

Run:
```bash
grep -nE 'config_path|--config|cpu_sim.*config' tests/cpu/test_cpu_sim_real_tohost.cpp tests/cpu/integration/test_3stage_riscv.cpp tests/cpu/integration/test_5stage_riscv.cpp tests/cpu/integration/test_7stage_riscv.cpp tests/cpu/integration/test_10stage_riscv.cpp 2>&1 | head -30
```
Expected: 5 处 `--config <path>` 字符串，分别指向 4 个不同 config (real_tohost + 5-stage 共用 cpu_default.json)

- [ ] **Step 4: 把 Step 2 + Step 3 数据填入 design.md §0.8 内嵌表 (5 行)**

Insert 5-row table with columns: 测试名 | config 路径 | config 声明 enable_mmu | 实际生效 enable_mmu (F1 静默丢) | stages + 关键差异
- 行 1: test_cpu_sim_real_tohost | cpu_default.json | true | true (F1) | 5-stage, 1-thread
- 行 2: test_3stage_riscv | cpu_embedded.json | **false** | **true (F1 静默吞)** | 3-stage
- 行 3: test_5stage_riscv | cpu_default.json | true | true | 5-stage
- 行 4: test_7stage_riscv | cpu_superscalar.json | true | true | 7-stage, dispatch_width=2, n_lanes=2
- 行 5: test_10stage_riscv | cpu_deep_pipeline.json | true | true | 10-stage

- [ ] **Step 5: 验证台账写入**

Run: `grep -A 8 "5-测试独立基线台账" openspec/changes/cpu-pipeline-tick-rca/design.md | head -15`
Expected: 含 5 行表格，行 2 显式标 "(F1 静默吞)"

### Task 3: 静态证据链 1 — F1 config 保真度缺陷 (load_config 丢字段)

**Files:**
- Read-only: `tools/cpu_sim/main.cpp` (lines 83-113)
- Read-only: `ip/cpu/cpu_factory.h` (lines 136, 444)
- Read-only: `include/cf/plugin/result_macros.h` (lines 76-81)

- [ ] **Step 1: 静态读 `tools/cpu_sim/main.cpp:83-113` 完整代码 (确认实际解析 10 字段)**

Use: `read` tool with `offset: 83, limit: 35`
Expected: `load_config()` 内含 ~10 个 `cfg.field = json.value()` 赋值

- [ ] **Step 2: 列出被静默忽略的字段清单**

Assert (write to design.md §F1 段):
- ✅ 解析: `name / isa / pipeline_stages / mul_latency / mul_impl / dispatch_width / n_lanes / icache_latency / dcache_latency / n_threads` (10 字段)
- ❌ 静默忽略: `enable_mmu / mmu_mode / satp_ppn / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / fetch_width / commit_width / retire_width / rob_size / lsq_size / rename_size / ghr_bits / ext_*`

- [ ] **Step 3: 静态读 `ip/cpu/cpu_factory.h:136` 默认值**

Use: `read` tool with `offset: 132, limit: 10`
Expected: 找到 `bool enable_mmu = true;` (CPUConfig struct 默认)

- [ ] **Step 4: F1 evidence block 写入 design.md §F1**

Write into `## §F1 (config 保真度缺陷) Stop-and-Document`:
- **结论**: F1 confirmed (静态读已足够)
- **证据**: main.cpp:83-113 + cpu_factory.h:136 (file:line + ≤5 行引用)
- **信心度**: HIGH (静态)
- **下一步**: §1 control experiment 用 `--no-mmu` 验证 F1 的运行时影响

### Task 4: 静态证据链 2 — F3 MMU 瞬态语义 + Bare-shortcut 放大

**Files:**
- Read-only: `ip/cpu/cpu_factory.h:444-450` (make_satp_value)
- Read-only: `ip/mmu/tlm/MMUPlugin.cpp:83-115` (Bare shortcut judgment)
- Read-only: `ip/cpu/plugins/ibus.h:54-67` (fetch CtrlLink halt_when)

- [ ] **Step 1: 静态读 `ip/cpu/cpu_factory.h:444-450` Sv39 自动编码**

Use: `read` tool with `offset: 440, limit: 12`
Expected: `make_satp_value(Sv39, 0)` 返回 `8ULL << 60` (= `0x8000_0000_0000_0000`)

- [ ] **Step 2: 静态读 `ip/mmu/tlm/MMUPlugin.cpp:109-115` Bare shortcut judgment**

Use: `read` tool with `offset: 105, limit: 15`
Expected: Bare shortcut 严格判断 (sv_mode_ != Bare → 走真翻译)

- [ ] **Step 3: 静态读 `ip/cpu/plugins/ibus.h:54-67` halt_when lambda**

Use: `read` tool with `offset: 50, limit: 20`
Expected: lambda 形式 `[](...) { if (!n) return false; return (*n)(PTW_ACTIVE); }`

- [ ] **Step 4: F3 evidence block 写入 design.md §F3**

Write: "结论: F3 chain confirmed (静态读已足够). 链路 `enable_mmu=true` + satp=0 → Sv39 → Bare shortcut 不成立 → TLB miss → PTW_ACTIVE=1 → ibus.h halt_when 持续 true → fetch 永 stall. 信心度: HIGH."

---

## Phase 1 — 2×2 Control Experiment (5 pipeline runs + 1 7-stage variant)

### Task 5: 临时加 `--no-mmu` CLI flag to cpu_sim (patch file, 必 revert)

**Files:**
- Modify: `tools/cpu_sim/main.cpp` (临时, 必 revert 在 Phase 3 收尾)
- Backup: `git stash` 或独立 patch 文件 (推荐 git stash, 简单)

- [ ] **Step 1: 创建 git stash 占位 (登记起始状态)**

Run: `git status --porcelain tools/cpu_sim/main.cpp`
Expected: empty (即文件干净). 若 dirty → 暂存其他未相关改动后再继续

- [ ] **Step 2: 修改 `main.cpp` 增加 `--no-mmu` flag**

Use: `edit` tool on argv parsing section (around line 119-161). Add:
```cpp
} else if (arg == "--no-mmu") {
    cfg.enable_mmu = false;  // P0 diagnostic, MUST revert before archive
} else if ...
```
Expected: 修改落在 argv for-loop 内

- [ ] **Step 3: 编译验证 (`--target cpu_sim`)**

Run: `cmake --build build --target cpu_sim -j4 2>&1 | tail -5`
Expected: `cpu_sim` 重新编译成功,无 warning/error

- [ ] **Step 4: 验证 flag 可识别 (--help 或 dry-run)**

Run: `./build/src/cf_plugin/cpu_sim --no-mmu --cycles 1 --elf tests/cpu/manual_elf/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 2>&1 | head -10`
Expected: 不报 "unknown flag", 输出 cycles 报告

### Task 6: Control experiment [A] — 4 次 `--no-mmu` (3/5/7/10-stage)

**Files:**
- Use binary: `./build/src/cf_plugin/cpu_sim` (含 --no-mmu patch)
- Read: 4 个测试 config

- [ ] **Step 1: 跑 [A1] 3-stage (cpu_embedded.json + --no-mmu)**

Run:
```bash
./build/src/cf_plugin/cpu_sim --no-mmu \
  --elf tests/cpu/manual_elf/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 \
  --cycles 1000 --config tests/cpu/configs/cpu_embedded.json 2>&1 | tee /tmp/opencode/cpu-rca-A1.log | tail -5
```
Expected: 记录 `TOHOST=X CYCLES=Y` 输出

- [ ] **Step 2: 跑 [A2] 5-stage (cpu_default.json + --no-mmu)**

Run: 同上, 把 `--config` 改 `cpu_default.json`, teefile 改 `/tmp/opencode/cpu-rca-A2.log`
Expected: 记录 TOHOST/CYCLES

- [ ] **Step 3: 跑 [A3] 7-stage (cpu_superscalar.json + --no-mmu)**

Run: 同上, `--config cpu_superscalar.json`, teefile `cpu-rca-A3.log`
Expected: 7-stage 可能 cycles 较高, 记录实际值

- [ ] **Step 4: 跑 [A4] 10-stage (cpu_deep_pipeline.json + --no-mmu)**

Run: 同上, `--config cpu_deep_pipeline.json`, teefile `cpu-rca-A4.log`
Expected: 10-stage cycles 最长

- [ ] **Step 5: 判定 [A] 列整体结果**

Compare TOHOST 4 run:
- **若 4/4 TOHOST=1**: F1+F2+F3 复合根因 confirmed (CLI 路径在 enable_mmu=false 时通)
- **若 4/4 TOHOST=0**: L4 排除, pivot 到 §2 L2 deep dive (但 F1 静态链仍可作 partial)
- **若 mixed (e.g. 7-stage 仍 0, 3/5/10 转 1)**: 7-stage lane dispatch 路径独立 bug, fix 需分支处理

- [ ] **Step 6: 把 4 行结果写入 design.md §1.5 [A] 列**

Format: `[A1] 3-stage enable_mmu=false TOHOST=1 CYCLES=N` × 4

### Task 7: Control experiment [B] — 5 FAIL 测试当前 baseline (5 次跑, 已有 + 4 新跑)

**Files:**
- Use binary: `./build/src/cf_plugin/cpu_sim` (默认路径, 必先 git checkout main.cpp revert `--no-mmu` !!!)
- 验证: 5 FAIL 测试的 exec_cmd 调用

- [ ] **Step 1: Revert `--no-mmu` patch (必须在跑默认路径前完成)**

Run: `git checkout tools/cpu_sim/main.cpp && cmake --build build --target cpu_sim -j4 2>&1 | tail -3`
Expected: `cpu_sim` 重新编译, baseline `chipforge_tests [cpu]` 128/129 仍保持

- [ ] **Step 2: 跑 [B1] real_tohost test 模拟 (cpu_default.json)**

Run: `./build/bin/chipforge_tests "test_cpu_sim_real_tohost" 2>&1 | tail -10`
Expected: FAIL (已知 baseline)

- [ ] **Step 3: 跑 [B2-5] 4 个 stage test 模拟**

Run (4 次):
```bash
for t in test_3stage_riscv test_5stage_riscv test_7stage_riscv test_10stage_riscv; do
  echo "=== $t ==="
  ./build/bin/chipforge_tests "$t" 2>&1 | tail -5
done
```
Expected: 4/4 FAIL (已知 baseline)

- [ ] **Step 4: 把 5 行结果写入 design.md §1.5 [B] 列**

Format: `[B1-B5] 5 测试 baseline enable_mmu=true (默认, F1 静默吞) TOHOST=0 CYCLES=1000`

### Task 8: Control experiment [A2 variant] — 7-stage `dispatch_width=2` 是否真因 (D3 决策)

**Files:**
- Modify: `tests/cpu/configs/cpu_superscalar.json` (临时改 `dispatch_width: 1`, 必 revert)
- Use binary: `./build/src/cf_plugin/cpu_sim` (含 --no-mmu patch)

- [ ] **Step 1: 重加 `--no-mmu` patch (若 Phase 3 已 revert)**

Run: `git diff tools/cpu_sim/main.cpp | head -5`
Expected: 显示 `--no-mmu` patch 内容. 若空 → 重复 Task 5

- [ ] **Step 2: 临时改 cpu_superscalar.json `dispatch_width: 1`**

Use: `edit` tool on `tests/cpu/configs/cpu_superscalar.json`
Change: `"dispatch_width": 2` → `"dispatch_width": 1`

- [ ] **Step 3: 跑 [A3 variant] dispatch_width=1 + --no-mmu**

Run: `./build/src/cf_plugin/cpu_sim --no-mmu --elf ... --config tests/cpu/configs/cpu_superscalar.json --cycles 1000 2>&1 | tail -5`
Expected: 记录 TOHOST/CYCLES

- [ ] **Step 4: 判定 — 7-stage 是否同因**

- **若 TOHOST=1**: 7-stage 与其余 4 测试同因 (F1+F2+F3 confirmed 覆盖)
- **若 TOHOST=0**: `cpu_factory.h:367` lane dispatch 路径独立 bug, 后续 fix 需分支处理

- [ ] **Step 5: Revert cpu_superscalar.json dispatch_width 改回 2**

Run: `git checkout tests/cpu/configs/cpu_superscalar.json`
Expected: `dispatch_width: 2` 恢复

### Task 9: Control experiment [D] — F4 反向对照 (C++ API 路径 enable_mmu=true)

**Files:**
- Create (临时): `tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp` (临时副本, **必 revert 删除**)
- Use binary: `./build/bin/chipforge_tests` (CMake 需重启扫描新文件, 但本 change 不提交 CMake)

- [ ] **Step 1: 复制 test_rv32ui_runner.cpp 为临时副本**

Run: `cp tests/cpu/integration/test_rv32ui_runner.cpp tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp`
Expected: 文件复制成功

- [ ] **Step 2: 修改副本置 `enable_mmu = true`**

Use: `edit` tool on `test_rv32ui_runner_mmu_on.cpp:109`
Change: `cfg.enable_mmu = false;` → `cfg.enable_mmu = true;`
Expected: 改动可见

- [ ] **Step 3: (可选) 临时加 CMake 注册 — 通常 `tests/CMakeLists.txt` 用 GLOB_RECURSE, 自动发现**

Run: `grep -A1 "GLOB_RECURSE\|file(GLOB" tests/CMakeLists.txt | head -10`
Expected: 若 GLOB 则新文件自动进 build, 无需手动改

- [ ] **Step 4: 跑 F4 reverse — 1 ELF**

Run: `./build/bin/chipforge_tests "test_rv32ui_runner_mmu_on" --elf tests/cpu/riscv_tests/elf/rv32ui-p-add 2>&1 | tail -10`
Acceptable outcomes (per Oracle 风险记录):
- **PASS (TOHOST=1)**: F4 confirmed + 三层复合根因 confirmed, L2 demoted
- **FAIL + cycles 耗尽 + TOHOST=0**: F4 confirmed + 三层根因覆盖
- **terminate with exception (PTW fault throw)**: F4 confirmed (算复合证据)
- **PASS 但 cycles 异常高**: F4 partial, L2 升为主要嫌疑

- [ ] **Step 5: F4 evidence block 写入 design.md §1.5 [D]**

Write: `[D] reverse C++ API + enable_mmu=true OUTCOME=<PASS|FAIL|terminate>`. 关键判定: 是否进了 L2 调查轨道

- [ ] **Step 6: Revert — 删除临时副本**

Run: `rm tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp`
Expected: 文件删除. 若 CMake 已重生成, 重 build 即可清掉

### Task 10: 2×2 控制实验综合判定 (design.md §1.7 stop-and-document)

**Files:**
- Modify: `openspec/changes/cpu-pipeline-tick-rca/design.md` (§1.7 stop-and-document 段)

- [ ] **Step 1: 汇总 [A] + [B] + [D] 三组数据 → 写综合判定**

Decision tree:
- **(A)=4/4 TOHOST=1 + (D)=FAIL/terminate**: F1+F2+F3 confirmed 4-layer ← **MOST LIKELY, 跳到 Phase 2 仅做 L1 静态排除 → Phase 3 收尾**
- **(A)=4/4 TOHOST=1 + (D)=PASS**: F4 confirmed + F1+F2+F3 confirmed (3-layer, no CLI/API 路径差异) → skip L2
- **(A) mixed (e.g. 7-stage still 0)**: F1+F2+F3 partial + 7-stage lane dispatch 独立 bug → 需后续 fix change 分支
- **(A)=4/4 TOHOST=0 + (D)=FAIL**: F1+F2+F3 不成立, pivot 到 Phase 2 L2 deep dive
- **(A)=4/4 TOHOST=1 + (D)=terminate**: 复合根因 confirmed (PTW fault throw path 触发)

- [ ] **Step 2: Revert 所有临时修改**

```bash
git checkout tools/cpu_sim/main.cpp
git checkout tests/cpu/configs/cpu_superscalar.json
cmake --build build --target cpu_sim -j4  # 重 build
```
Expected: baseline 0 diff, 128/129 保持

- [ ] **Step 3: stop-and-document 块写入**

Write into `## §1.7 Control Experiment Stop-and-Document`:
- **结论**: F1+F2+F3 复合根因 [confirmed / partial / excluded] per decision tree
- **证据**: [A] 4 行 + [B] 5 行 + [D] 1 行 (≤15 行)
- **信心度**: HIGH / MEDIUM / LOW
- **下一步**: skip to Phase 3 / pivot 到 Phase 2 L2

---

## Phase 2 — 后续调查线 (L1 静态排除; L2/L3 仅在 F1+F2+F3 excluded 后启动)

### Task 11: L1 静态排除 (build_cpu_impl + auto_throw + unique_ptr) — 任何时候可执行

**Files:**
- Read-only: `ip/cpu/cpu_factory.h:406-418` (build_cpu_impl)
- Read-only: `include/cf/plugin/result_macros.h:76-81` (auto_throw)
- Read-only: `include/cf/plugin/pipe_builder.h:185-187` (should_stall_stage)

- [ ] **Step 1: 静态读 build_cpu_impl**

Use: `read` tool with `offset: 405, limit: 15`
Expected: thin try/catch wrapper around `CpuFactory::build_cpu()`

- [ ] **Step 2: 静态读 auto_throw macro**

Use: `read` tool with `offset: 75, limit: 8`
Expected: 解包 Result, 失败 throw

- [ ] **Step 3: 静态读 unique_ptr RAII 行为**

不读代码 (语言级语义), 直接结论: unique_ptr 与 stack auto var 在 C++ 标准下 RAII 等价 (`pb->run()` 循环在 unique_ptr 析构前完成)

- [ ] **Step 4: L1 evidence block 写入 design.md §L1**

Write: "结论: **L1 excluded** (静态分析已足够, 无 tick 差异机制). 证据: build_cpu_impl + auto_throw 各 ≤5 行 reference + RAII 语言级语义. 信心度: HIGH (5 分钟静态排除). 下一步: skip."

### Task 12: L2 deep dive (ibus.h halt_when) — 仅在 Phase 1 F1+F2+F3 excluded 时启动

**Files:**
- Read-only: `ip/cpu/plugins/ibus.h:54-67`
- Modify (临时, 必 revert): `include/cf/plugin/pipe_builder.h:185-187` (cerr log)

> **P0 重做决策**: 仅在 Phase 1 综合判定显示 F1+F2+F3 excluded (即 [A] 列全部 TOHOST=0) 时启动本任务. 否则 skip 到 Phase 3.

- [ ] **Step 1: 静态读 ibus.h halt_when lambda (已在 Task 4 Step 3 读过, 复用)**

- [ ] **Step 2: 临时加 cerr log 到 ibus.h:62-66 lambda 内**

Use: `edit` tool on `ip/cpu/plugins/ibus.h`
Insert: `std::cerr << "DEBUG halt_when called, cycle=" << cycle << " PTW_ACTIVE=" << (n ? (*n)(PTW_ACTIVE) : -1) << "\n";`
Constraint: 必须 `if (cycle < 20) ...` 限制输出

- [ ] **Step 3: 编译 + 跑 cpu_sim 1000 cycles, 记录 stderr**

Run: `cmake --build build --target cpu_sim -j4 2>&1 | tail -3 && ./build/src/cf_plugin/cpu_sim --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 2>&1 | grep DEBUG | head -25`
Expected: ≤20 行 DEBUG 输出

- [ ] **Step 4: 判定 L2 结论**

- **halt_when 持续返回 true**: L2 confirmed (fetch stall 永远触发)
- **halt_when 仅初始 true 后 false**: L2 partial (init 时序)
- **halt_when 永远 false**: L2 excluded (fetch 不 stall)

- [ ] **Step 5: Revert ibus.h**

Run: `git checkout ip/cpu/plugins/ibus.h && cmake --build build -j4`
Expected: binary 重 build, baseline 0 diff

- [ ] **Step 6: L2 stop-and-document 块写入 design.md §L2**

Format per "### <line_id> Stop-and-Document" template

### Task 13: L3 deep dive (StageLink EARLY) — 仅在 L1+L2 全部 excluded 时启动

**Files:**
- Read-only: `ip/cpu/plugins/stage_link.h` (EARLY closures)

> **P0 重做决策**: 仅在 L2 excluded 后启动. 否则 skip (HIGH 风险, 不属本 change scope).

> **本 RCA 决策**: 按 proposal §Out of scope "不重写 StageLinkPlugin", L3 仅读 + log. 重写留给后续独立 change.

- [ ] **Step 1: 静态读 stage_link.h EARLY closures**

- [ ] **Step 2: 临时加 cerr log 到 EARLY 闭包入口 (输出限流 cycle<10)**

- [ ] **Step 3: 跑 + 判定 + revert + stop-and-document**

(与 Task 12 同构, 略)

---

## Phase 3 — 综合诊断 + 5-测试台账 + 修复建议 + 收尾

### Task 14: design.md §5 综合诊断段写入

**Files:**
- Modify: `openspec/changes/cpu-pipeline-tick-rca/design.md` (§5.1 + §5.2 + §5.3 + §5.4)

- [ ] **Step 1: §5.1 4 调查线结论汇总**

Format:
- L4 (=F1+F2+F3): confirmed / partial / excluded + evidence
- L2: confirmed / excluded / partial + evidence
- L3: confirmed / excluded + evidence
- L1: **excluded** + evidence

- [ ] **Step 2: §5.2 最可能根因 (ranked)**

List ranked 候选 (按 confidence):
1. F1+F2+F3 复合 + D4-compliant fix direction 路径
2. F4 第 4 层 (CLI/API 路径差异) — 仅在 F4 反向实验 FAIL 时
3. L2 fetch stall — 仅在 L1+L2 deep dive 时

- [ ] **Step 3: §5.3 修复建议 (D4-compliant, 5 候选 hard-excluded)**

Write 5 candidates (per tasks.md §5.3):
- 候选 1 (F1 PRIMARY): `load_config()` 解析 `enable_mmu/mmu_mode/satp_ppn/...` (LOW, 2-3 file)
- 候选 2 (F2 PRIMARY): 4 个测试 config `enable_mmu:false` (LOW, 4 file + 1 对照测试)
- 候选 3 (F4 SECONDARY): CLI/API wrapper 差异 (MEDIUM, 1-2 file)
- 候选 4 (L2 SECONDARY): `ibus.h` halt_when Bare-mode guard (MEDIUM)
- 候选 5 (L3 HIGH): StageLink 重写 (本 change 不动)
- **HARD-EXCLUDED**: 改 `CPUConfig::enable_mmu` 默认 false (D4)

- [ ] **Step 4: §5.4 cpu-pipeline-tick-fix 启动建议**

Include:
- 与 `mmu-chmem-pipeline-integration` 协调点 (config 传播路径)
- D4 强制约束 (不改默认)
- 与 `feat-bp-btb-gshare` / `feat-dhrystone-dmips-gate` 测量底座解锁时间线

### Task 15: AGENTS.md 漂移修正 (推迟到 §6.4 archive 时同步)

**Files:**
- Modify: `AGENTS.md` (line 61 "framework-level bug" + "pipeline 不能 tick" 措辞)

> **前置约束**: 本 Task 在 archive phase 触发. Phase 3 当前仅做 design 段写入 + 验证, AGENTS.md 同步留给 archive 时.

### Task 16: 架构 gate + 测试 baseline + git status 兜底

**Files:**
- Verify: 全 3 架构 gate + 测试 baseline

- [ ] **Step 1: 跑架构 gate 三件套**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh 2>&1 | tail -5`
Expected: 三件套 0 FAIL (P0 文档改动纯新增 → 0 变化)

- [ ] **Step 2: 测试 baseline 跑 (硬约束)**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -3`
Expected: 48/48 PASS (硬约束)

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -3`
Expected: 128/129 PASS (1 FAIL 保持, 本 change 不修)

- [ ] **Step 3: git status 校验 (除 .rddf/plans/ 与 openspec/, 其他目录必须干净)**

Run: `git status --porcelain`
Expected: 仅含:
- `openspec/changes/cpu-pipeline-tick-rca/{proposal,design,tasks,specs/cpu-pipeline-tick-rca/spec,README}.md` 5 个文件 (P0 重做)
- `.rddf/plans/cpu-pipeline-tick-rca.md` (本 plan 文件)
- `openspec/specs/cpu-pipeline-tick-rca/spec.md` (D3 auto-derived)
- **不允许**: `tools/cpu_sim/main.cpp` / `tests/cpu/configs/*` / `tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp` 等临时改动

- [ ] **Step 4: revert 全部临时改动 (兜底)**

```bash
git checkout tools/cpu_sim/main.cpp 2>/dev/null || true
git checkout tests/cpu/configs/cpu_superscalar.json 2>/dev/null || true
rm -f tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp
cmake --build build --target cpu_sim -j4 2>&1 | tail -3
```
Expected: 全部 revert, baseline 0 diff

### Task 17: 提交 P0 改造 + plan + 中间 5 artifact (per proposal §6.6)

**Files:**
- All 5 artifact + 1 plan + 1 spec-delta = 7 files staged

- [ ] **Step 1: git add 7 个文件**

Run:
```bash
git add openspec/changes/cpu-pipeline-tick-rca/proposal.md
git add openspec/changes/cpu-pipeline-tick-rca/design.md
git add openspec/changes/cpu-pipeline-tick-rca/tasks.md
git add openspec/changes/cpu-pipeline-tick-rca/specs/cpu-pipeline-tick-rca/spec.md
git add openspec/changes/cpu-pipeline-tick-rca/README.md
git add openspec/specs/cpu-pipeline-tick-rca/spec.md
git add .rddf/plans/cpu-pipeline-tick-rca.md
```
Expected: 7 个文件 staged

- [ ] **Step 2: commit 按 proposal §6.6 prefix**

Run: `git commit -m "$(cat <<'COMMITMSG'
chore(openspec): cpu-pipeline-tick-rca P0 重做 — 复合根因叙事 + 5-测试独立基线台账 + 2×2 control experiment + D4 对齐

[B5 fix: prefix 'diag(' 改为 'chore(openspec):' 对齐 git log 历史先例 (commit 5403718)]
[B6 fix: 'framework-level bug' / 'pipeline 不能 tick' 措辞修正]
[F1 + F2 + F3 compound root-cause: config fidelity + test config intent + MMU transient semantics, per Metis+Oracle+Critic 2026-10-15 review]
[D4-aligned fix: keep CPUConfig::enable_mmu default true; candidates are (a) load_config parse + (b) test configs false + (c) --no-mmu CLI override]

P0 重做后 plan:
- Phase 0: baseline + 5-测试台账 (Task 1-4)
- Phase 1: 2×2 control experiment (Task 5-10)
- Phase 2: L1 静态 + L2/L3 备用 (Task 11-13)
- Phase 3: 综合诊断 + 修复建议 + 收尾 (Task 14-17)

Verification plan: 架构 gate 0 FAIL + [riscv-tests] 48/48 + [cpu] 128/129 (1 FAIL 保持) + [cpu-integration] 30/35 (4 FAIL 保持)
COMMITMSG
)"`
Expected: 1 commit, 7 文件

---

## Acceptance Criteria (与 improvement AC 同步 — 本 change 用 tasks.md §6.2)

| AC 编号 | 描述 | 验证 |
|---|---|---|
| AC1 | 5 个 FAIL 测试**保持 FAIL** (本 change 纯诊断, 不修) | `[cpu] 128/129` + `[cpu-integration] 30/35` |
| AC2 | `[riscv-tests] 48/48 PASS` 硬约束 (0 regression) | 同名测试跑 |
| AC3 | 0 product code 改动 (除临时 debug log 全 revert) | `git status --porcelain` 仅 openspec/ + .rddf/ |
| AC4 | design.md §5 综合诊断含 ranked 候选 + D4-compliant 修复方向 | grep "D4-compliant" |
| AC5 | 5-测试独立基线台账内嵌 design.md §0.8 (D3 决策) | grep "5-测试独立基线台账" |
| AC6 | 2×2 control experiment 4/5 cell 数据填齐 | design.md §1.5 |
| AC7 | 架构 gate 三件套 0 FAIL | verify_adr + verify_plugin_decision + check_plugin_portability |

## Fallback Strategy

| Scenario | Fallback |
|---|---|
| `[A]` 4/4 TOHOST=0 (L4 完全 excluded) | pivot 到 Phase 2 L2 deep dive (Task 12). L2 confirmed 时仍可输出诊断 |
| `[A]` mixed (7-stage 仍 0, 3/5/10 转 1) | 记为 7-stage 独立 bug, fix change 分支处理 (写 design.md §5.4 OQ6 答案) |
| `[D]` reverse FAIL (CLI/API 路径同因) | F4 confirmed 第 4 层, 复合根因扩 4 层; fix candidate 3 列首位 |
| `--no-mmu` patch 编译失败 | 回退 plan, 直接静态分析 (但会降低根因 confidence) |
| CONTROL 实验跑出 5 FAIL → 5 PASS | 提前收尾 (但违反本 change scope "不修复" 决策, 需先 user approval) |

## Estimated Timeline

| Phase | Tasks | 估计时间 |
|---|---|---|
| Phase 0 baseline + 台账 | Task 1-4 | 30 分钟 |
| Phase 1 control experiment | Task 5-10 | 60 分钟 (4+1 次跑 + 1 reverse) |
| Phase 2 L1+L2/L3 | Task 11-13 | 30 分钟 (L1 5 分钟 + L2 视 F1+F2+F3 结论) |
| Phase 3 综合 + 收尾 | Task 14-17 | 30 分钟 |
| **Total** | 17 tasks | **2.5 小时 / 1-2 session** |

## Notes

- **D3 决策** (5-测试独立基线台账优先): 在原 Phase 0 §0.8 注入 5 行内嵌表, 不另立文件 (避免污染 openspec/changes/ 与 .rddf/state/)
- **D4 决策** (保持默认 true): 5 处违规全替换为 D4-compliant 路径, 含 design.md:266 sentinel 段
- **P0 重做决策** (保留 tick-rca 命名): 改目录名 ≥8 处外部引用开销 > 收益, 仅在 5 artifact 内 caveat 修正事实
- **Comm 决策** (B5 prefix): git log 验证 `diag(` 0 先例 + `chore(openspec):` 1 先例 (commit 5403718), 选后者
- **关键守门**: Task 16 Step 3 git status --porcelain 校验, 不通过则 Task 17 commit 不准执行 (B1 防御)
