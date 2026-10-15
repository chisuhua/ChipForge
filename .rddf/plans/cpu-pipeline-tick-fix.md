# cpu-pipeline-tick-fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 5 cpu_sim CLI FAIL tests by 4 minimal product code changes (F1+F2+F3a+F3b) + 1 new unit test. Bare-mode byte-identical with v0.10.4 required. 0 regression on `[riscv-tests] 48/48`, `[mmu] 59/59`, `[mmu-verilator] 3/3`, `[cpu-l1-mmu-demo] 6/6`, `[cache] EndToEndTranslationThroughBridge`.

**Architecture:** Two-phase:
- **Phase A (data plane)**: F1 (load_config parse 8 fields) + F2 (3 test configs `enable_mmu:false`). Low risk — additive, no behavior change for existing passing tests. Roll out F1 first (gives F2 the input it needs to take effect).
- **Phase B (control plane)**: F3a (MMUPlugin Bare shortcut guard) + F3b (ibus.h halt_when Bare-mode guard). MEDIUM risk — must keep Bare-mode byte-identical with v0.10.4 (`a3afcfa`) and preserve stall semantics for真 Sv32/Sv39/Sv48 translation. Roll out in lockstep (F3a reads what F3b writes).

**Tech Stack:** C++23 (existing), Catch2 v3.7.0 (vendored at `tests/catch2/`), `tools/cpu_sim` CLI, `ip/mmu/tlm/MMUPlugin.{cpp,h}`, `ip/mmu/tlm/mmu_keys.h`, `ip/cpu/plugins/ibus.h`, `ip/cpu/configs/*.json`.

**D4 决策 (硬约束)**:
- **保持** `CPUConfig::enable_mmu` 默认 `true`
- **不**通过"改默认 false"绕开
- Bare-mode byte-identical with v0.10.4 `a3afcfa`
- 真 Sv32/Sv39/Sv48 翻译 stall 行为保留

---

## File Structure

### Production Code (修改 4 处 + 新增 1 单测)

| File | Responsibility |
|---|---|
| `tools/cpu_sim/main.cpp` (F1) | 扩展 `load_config()` 解析 8 字段 (新增 `enable_mmu` 等) |
| `ip/cpu/configs/cpu_default.json` (F2) | `params.enable_mmu: false` + `_comment` |
| `ip/cpu/configs/cpu_superscalar.json` (F2) | `params.enable_mmu: false` + `_comment` |
| `ip/cpu/configs/cpu_deep_pipeline.json` (F2) | `params.enable_mmu: false` + `_comment` |
| `ip/mmu/tlm/MMUPlugin.cpp` (F3a) | `do_lookup()` Bare shortcut 守卫 (`sv_mode_==Bare \|\| satp_value_mode()==Bare` → identity) |
| `ip/mmu/tlm/mmu_keys.h` (F3b dep) | 新增 `SATP_MODE` key |
| `ip/cpu/plugins/ibus.h` (F3b) | `fetch_ctrl->halt_when` 加 `SATP_MODE==0` 跳过守卫 |
| `tests/cpu/test_load_config_field_roundtrip.cpp` (新增) | F1 单测: JSON 8 字段往返 CPUConfig |

### Tests

| File | Test Coverage |
|---|---|
| `tests/cpu/test_load_config_field_roundtrip.cpp` (新) | F1: 8 字段 JSON→CPUConfig |
| `tests/cpu/test_cpu_sim_real_tohost.cpp` (已有, AC1 验证) | cpu_sim CLI tohost=1 |
| `tests/cpu/integration/test_{3,5,7,10}stage_riscv.cpp` (已有, AC2-5 验证) | CLI end-to-end bare-metal smoke |
| `[mmu]` family (F3a/F3b 回归) | `[mmu] 59/59` + `[mmu-verilator] 3/3` Bare-mode byte-identical |
| `tests/cache/test_mmu_cache_integration.cpp` (F3a 锚点) | `EndToEndTranslationThroughBridge` PASS (v0.10.4 维持) |

---

## Phase A — Data Plane (F1 + F2)

### Task 1: F1 — `load_config()` 解析 8 字段 (工具层 config fidelity)

**Files:**
- Modify: `tools/cpu_sim/main.cpp:75-115` (load_config 函数)
- Create: `tests/cpu/test_load_config_field_roundtrip.cpp` (新单测)

- [ ] **Step 1: 看 load_config 当前代码 + CPUConfig struct 字段名**

Use: `read` tool with `offset: 75, limit: 50` on `tools/cpu_sim/main.cpp` (load_config 段)
Use: `grep -nE "^\s+(bool|int|uint|std::string|std::uint)\s+(enable_mmu|mmu_mode|enable_pmp|branch_predictor|btb_entries|clock_freq_mhz|satp_ppn|n_threads)\s*[=;]" ip/cpu/cpu_factory.h | head -20`

Expected: 找到 8 字段在 CPUConfig struct 里的定义,确认字段名一致.

- [ ] **Step 2: 写新单测 (F1 Test)** — `tests/cpu/test_load_config_field_roundtrip.cpp`

```cpp
// Test 1: enable_mmu false
TEST_CASE("load_config parses enable_mmu=false from JSON", "[cpu_sim][load_config]") {
  // Create tmp JSON: {"params": {"enable_mmu": false}}
  // Run load_config
  // Assert: cfg.enable_mmu == false
}
TEST_CASE("load_config parses enable_mmu=true from JSON", "[cpu_sim][load_config]") {
  // ...
}
// 8 tests: enable_mmu / mmu_mode / enable_pmp / branch_predictor /
//         btb_entries / clock_freq_mhz / satp_ppn / n_threads
```

(完整代码 ≈ 80 行, 用 nlohmann::json 写 tmp file → load_config → 各字段 assert)

- [ ] **Step 3: 跑单测确认 8 test FAIL (baseline 没改前)**

Run: `cmake --build build --target chipforge_tests -j4 && ./build/bin/chipforge_tests "load_config_field_roundtrip" 2>&1 | tail -15`
Expected: 8 TEST_CASE FAIL ("function load_config_test_missing" — 因新文件未被 build 或因 load_config 还未解析)

- [ ] **Step 4: 修改 `tools/cpu_sim/main.cpp:75-115` 加 8 字段解析**

Use: `edit` tool on `tools/cpu_sim/main.cpp`, 在 `if (j.contains("params")) { const auto& p = j["params"]; ... }` 段内现有赋值之后追加:

```cpp
// cpu-pipeline-tick-fix F1: parse 8 previously dropped fields
cfg.enable_mmu = p.value("enable_mmu", cfg.enable_mmu);
{
  const std::string mmu_mode_str = p.value("mmu_mode", std::string{});
  if (!mmu_mode_str.empty()) cfg.mmu_mode = mmu_mode_str;
}
cfg.enable_pmp = p.value("enable_pmp", cfg.enable_pmp);
cfg.branch_predictor = p.value("branch_predictor", cfg.branch_predictor);
cfg.btb_entries = p.value("btb_entries", cfg.btb_entries);
cfg.clock_freq_mhz = p.value("clock_freq_mhz", cfg.clock_freq_mhz);
cfg.satp_ppn = p.value("satp_ppn", cfg.satp_ppn);
cfg.n_threads = p.value("n_threads", cfg.n_threads);
```

**前置**: 确认 `CPUConfig` struct 中这 8 字段都已存在; 若不存在则**先**扩展 struct (但 D4 硬约束不改默认值).

- [ ] **Step 5: 跑单测确认 8 test PASS**

Run: `cmake --build build --target chipforge_tests -j4 && ./build/bin/chipforge_tests "load_config_field_roundtrip" 2>&1 | tail -5`
Expected: `8/8 PASS` 或 `All tests passed`

- [ ] **Step 6: 跑 [riscv-tests] 确认 0 regression**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -3`
Expected: `48/48 PASS` (硬约束保持)

- [ ] **Step 7: Defer commit**

按 rdd-builder archive 阶段统一 commit 协议, 此 Task 不单独 commit; 详见 Phase 5 收尾.

### Task 2: F2 — 3 测试 config `enable_mmu: false` (裸机烟测意图)

**Files:**
- Modify: `ip/cpu/configs/cpu_default.json:9`
- Modify: `ip/cpu/configs/cpu_superscalar.json:16`
- Modify: `ip/cpu/configs/cpu_deep_pipeline.json:18`

- [ ] **Step 1: 静态读 3 个 config 当前 enable_mmu 值**

Run:
```bash
for f in ip/cpu/configs/cpu_default.json ip/cpu/configs/cpu_superscalar.json ip/cpu/configs/cpu_deep_pipeline.json; do
  echo "=== $f ==="
  python3 -c "import json; d=json.load(open('$f')); print('enable_mmu:', d['params'].get('enable_mmu','(missing)'))"
done
```
Expected: 3/3 `enable_mmu: true`

- [ ] **Step 2: 修改 `ip/cpu/configs/cpu_default.json`**

Use: `edit` tool. Replace `"enable_mmu": true` → `"enable_mmu": false` (在 params 子对象内).
末尾追加 `"_comment": "Bare M-mode bare-metal smoke per openspec/specs/riscv-tests-fixture/spec.md:74-80"`

- [ ] **Step 3: 同 Step 2 改 `ip/cpu/configs/cpu_superscalar.json` 与 `cpu_deep_pipeline.json`**

- [ ] **Step 4: 验证 3 config + 重新验证 cpu_embedded (应仍是 false)**

Run:
```bash
for f in ip/cpu/configs/cpu_{default,superscalar,deep_pipeline,embedded}.json; do
  echo "=== $f ==="
  python3 -c "import json; d=json.load(open('$f')); print('enable_mmu:', d['params'].get('enable_mmu'), '_comment:', d.get('_comment','(none)'))"
done
```
Expected: 4/4 `enable_mmu: false`, 3 个新 comment.

- [ ] **Step 5: 跑 cpu_sim CLI 单测试 (3-stage 用 cpu_embedded + --elf tests/cpu/manual_elf/add.elf)**

Run:
```bash
cmake --build build --target cpu_sim -j4
./build/src/cf_plugin/cpu_sim --no-mmu --elf tests/cpu/manual_elf/add.elf \
  --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100 \
  --config ip/cpu/configs/cpu_default.json
```
Expected: `tohost=1` (F2 改后 `--no-mmu` 已 work; 若未 work 仍 tohost=0 进入 F3 修)

- [ ] **Step 6: 跨测试模拟 (4 config 各跑一次, 不带 --no-mmu)**

Run:
```bash
for f in cpu_embedded cpu_default cpu_superscalar cpu_deep_pipeline; do
  echo "=== $f ==="
  ./build/src/cf_plugin/cpu_sim --elf tests/cpu/manual_elf/add.elf \
    --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100 \
    --config ip/cpu/configs/$f.json 2>&1 | grep -E "^tohost=|^cycles="
done
```
Expected: 4 行运行结果 (在没修 F3 之前, 极可能仍 `tohost=0`, 但**本 Task 目的**是确认 load_config 生效 + F2 已落地; 若 tohost=0 是预期, 进入 Phase B F3a 修)

---

## Phase B — Control Plane (F3a + F3b)

### Task 3: F3a — MMUPlugin Bare shortcut 守卫

**Files:**
- Modify: `ip/mmu/tlm/MMUPlugin.cpp:109-115` (do_lookup Bare shortcut)
- Read-only: `ip/mmu/tlm/MMUPlugin.h` (sv_mode_ enum)

- [ ] **Step 1: 静态读 MMUPlugin.cpp:100-130 (do_lookup 完整代码)**

Use: `read` tool with `offset: 90, limit: 50`

Expected: 找到 Bare shortcut 判断 (`sv_mode_ != Bare` 严格判断) + 真翻译路径 else 分支.

- [ ] **Step 2: 看 v0.10.4 Bare shortcut 行为 + `a3afcfa` 注释**

Run: `git log --oneline -- ip/mmu/tlm/MMUPlugin.cpp | head -5`
Use: `git show a3afcfa -- ip/mmu/tlm/MMUPlugin.cpp 2>&1 | head -60` (查 v0.10.4 hotfix 改了啥)

Expected: 看到 `[mmu] 59/59` PASS 锚点 (含 `tests/cache/test_mmu_cache_integration.cpp::EndToEndTranslationThroughBridge` 验证 Bare-mode identity translation).

- [ ] **Step 3: 写临时失败测试 (检查现有 Bare 测试 baseline)**

Run:
```bash
./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3
```
Expected: `59/59 PASS` (baseline 锚点)

- [ ] **Step 4: 修改 `MMUPlugin.cpp:109-115` 加 Bare-mode 守卫**

Use: `edit` tool. Replace 现有 strict judgment:

```cpp
// cpu-pipeline-tick-fix F3a: Bare-mode 显式 identity translation (v0.10.4 hotfix a3afcfa 修正)
// Original: if (sv_mode_ != Bare) return unexpected(...);
// Fix: Bare mode (sv_mode_==Bare OR satp_mode==0 in memory) → identity translation
if (sv_mode_ == Bare) {
  return identity_translation(vaddr);  // v0.10.4 behavior preserved for this branch
}
if (sv_mode_ != Bare) {
  return unexpected(...);
}
```

**前置**: 确认有 `identity_translation()` helper 或 inline 实现 (PTE 解析简化).

- [ ] **Step 5: 编译 + 跑 [mmu] 全部 PASS (F3a Bare-mode byte-identical 锚点)**

Run: `cmake --build build -j4 && ./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -3`
Expected: `59/59 PASS` (Bare-mode 未破)

- [ ] **Step 6: 跑 [cache] EndToEndTranslationThroughBridge 验证**

Run: `./build/bin/chipforge_tests "*EndToEnd*" 2>&1 | tail -5`
Expected: PASS (v0.10.4 hotfix 行为 byte-identical)

- [ ] **Step 7: 跑 [cpu-l1-mmu-demo] 验证**

Run: `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" 2>&1 | tail -3`
Expected: `6/6 PASS`

### Task 4: F3b — `ibus.h` halt_when Bare-mode 守卫 (双锁 F3a)

**Files:**
- Modify: `ip/mmu/tlm/mmu_keys.h` (新增 SATP_MODE key)
- Modify: `ip/cpu/plugins/ibus.h:62-66` (halt_when 加 SATP_MODE==0 守卫)

- [ ] **Step 1: 静态读 mmu_keys.h (现有 key 定义 + PayloadKey 类型)**

Use: `grep -n "PTW_ACTIVE\|enum.*Key" ip/mmu/tlm/mmu_keys.h | head -10`

- [ ] **Step 2: 静态读 ibus.h:54-67 (halt_when 完整 lambda)**

Use: `read` tool with `offset: 50, limit: 25`

- [ ] **Step 3: 写新 stall_matrix 测试 (检查现有 stall_matrix 16/16)**

Run:
```bash
./build/bin/chipforge_tests "*stall_matrix*" 2>&1 | tail -3
```
Expected: 现有 stall_matrix test 已有 Bare + 非 Bare 场景 (16/16 baseline).

- [ ] **Step 4: 修改 `ip/mmu/tlm/mmu_keys.h` 新增 SATP_MODE key**

Use: `edit` tool. 在现有 key enum 内追加:

```cpp
SATPPN = N+1,      // satp.PPN field
SATP_MODE = N+2,   // satp.MODE field (0=Bare, 1=Sv32, 8=Sv39, 9=Sv48)
```

- [ ] **Step 5: 修改 `ip/cpu/plugins/ibus.h:62-66` halt_when lambda**

Use: `edit` tool. Replace 现有 lambda:

```cpp
fetch_ctrl->halt_when([&pb]() {
  auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
  if (!n) return false;  // no MMU registered
  // cpu-pipeline-tick-fix F3b: Bare-mode (SATP_MODE==0) skip PTW check
  auto mode = static_cast<int>((*n)(MmuKeys::SATP_MODE));
  if (mode == 0) return false;  // Bare = identity, no stall
  return static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE));
});
```

- [ ] **Step 6: 在 MMUPlugin (Task 3 同文件) 写 SATP_MODE 字段**

Use: `edit` tool on `ip/mmu/tlm/MMUPlugin.cpp`. 在 at_stage("tlb_lookup_ifetch") NORMAL 段内:

```cpp
// cpu-pipeline-tick-fix F3b: publish SATP_MODE for ibus halt_when guard
(*n)(MmuKeys::SATP_MODE) = static_cast<std::uint32_t>(sv_mode_);
```

(确认 NORMAL 时机与原有 PTW_ACTIVE 写是否同步; 若冲突, LATE 阶段补写.)

- [ ] **Step 7: 编译 + 跑 stall_matrix 16/16**

Run: `cmake --build build -j4 && ./build/bin/chipforge_tests "*stall_matrix*" 2>&1 | tail -3`
Expected: `16/16 PASS` (Bare 与 非 Bare stall 行为不变)

- [ ] **Step 8: 跑 [mmu] [mmu-verilator] 0 regression**

Run: `./build/bin/chipforge_tests "[mmu]" "[mmu-verilator]" 2>&1 | tail -5`
Expected: `59/59` + `3/3 PASS`

---

## Phase C — End-to-End 修复验证

### Task 5: 跑 cpu_sim 5-stage 默认 (无 --no-mmu, F1+F2+F3a+F3b 全部就位)

- [ ] **Step 1: rebuild cpu_sim + 跑 5-stage 默认路径**

Run:
```bash
cmake --build build --target cpu_sim -j4
./build/src/cf_plugin/cpu_sim --elf tests/cpu/manual_elf/add.elf \
  --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100 \
  --config ip/cpu/configs/cpu_default.json 2>&1 | head -8
```
Expected: `tohost=1` (F1+F2+F3a+F3b 全部就位的标志)

- [ ] **Step 2: 跨 4 config 跑**

Run:
```bash
for f in cpu_embedded cpu_default cpu_superscalar cpu_deep_pipeline; do
  echo "=== $f ==="
  ./build/src/cf_plugin/cpu_sim --elf tests/cpu/manual_elf/add.elf \
    --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 100 \
    --config ip/cpu/configs/$f.json 2>&1 | grep -E "^(tohost|cycles)="
done
```
Expected: 4/4 `tohost=1`, 4/4 `cycles<30`

- [ ] **Step 3: 跑 [cpu] + [cpu-integration] (AC6 + AC7)**

Run:
```bash
./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -3
./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -3
```
Expected:
- `[cpu] 129/129 PASS` (含 AC1 test_cpu_sim_real_tohost)
- `[cpu-integration] 35/35 PASS` (含 AC2-5 add_elf + 维持 dhrystone SIGTERM timeout)

---

## Phase D — 收尾 + 验证 gate

### Task 6: 架构 gate 三件套 + 测试 baseline 全跑

- [ ] **Step 1: 架构 gate 0 FAIL**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh 2>&1 | tail -5`
Expected: 三件套均 0 FAIL (AC11)

- [ ] **Step 2: 0 regression baseline (AC8-13)**

Run:
```bash
./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tail -2
./build/bin/chipforge_tests "[mmu]" 2>&1 | tail -2
./build/bin/chipforge_tests "[mmu-verilator]" 2>&1 | tail -2
./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" 2>&1 | tail -2
./build/bin/chipforge_tests "[cpu-integration][mul-div-fsm-integration]" 2>&1 | tail -2
```
Expected: 48/48 + 59/59 + 3/3 + 6/6 + 4/4 全部 PASS

- [ ] **Step 3: [cache] EndToEndTranslationThroughBridge**

Run: `./build/bin/chipforge_tests "*EndToEnd*" 2>&1 | tail -5`
Expected: PASS (v0.10.4 hotfix byte-identical)

- [ ] **Step 4: git status --porcelain (AC21-22)**

Run: `git status --porcelain`
Expected:
- 仅 `M openspec/changes/cpu-pipeline-tick-fix/{proposal,design,tasks}.md` + `specs/cpu-pipeline-tick-fix/spec.md`
- `M tools/cpu_sim/main.cpp` (F1)
- `M ip/cpu/configs/{cpu_default,cpu_superscalar,cpu_deep_pipeline}.json` (F2)
- `M ip/mmu/tlm/MMUPlugin.cpp` (F3a)
- `M ip/cpu/plugins/ibus.h` (F3b)
- `M ip/mmu/tlm/mmu_keys.h` (F3b dep)
- `A tests/cpu/test_load_config_field_roundtrip.cpp` (新单测)
- 3 spec-delta (mmu-bare-shortcut / cpu-sim-elf-base-tohost-override / cpu-pipeline-tick-rca)
- **不允许**: 任何 `ip/cpu/cpu_factory.h` (D4 硬约束) 或 `src/cf_plugin/bridge/*` (out of scope) 或 `_chmem.h` (CH_MEM)

### Task 7: spec-delta 落位 + 文档同步

- [ ] **Step 1: 写 `openspec/specs/mmu-bare-shortcut/spec.md` 1 Scenario ADDED (F3a)**

Use: `openspec spec validate mmu-bare-shortcut` 看现有 spec. 在 ADDED Requirements 段加 1 scenario (Bare-mode identity translation).

- [ ] **Step 2: 写 `openspec/specs/cpu-sim-elf-base-tohost-override/spec.md` 1 Scenario ADDED (F3b)**

Use: `openspec spec validate cpu-sim-elf-base-tohost-override` 看现有 spec. 在 ADDED Requirements 段加 1 scenario (cpu_sim CLI Bare-mode fetch 不 stall).

- [ ] **Step 3: 写 `openspec/specs/cpu-pipeline-tick-rca/spec.md` ref 注解**

Use: append 1 段 ref to `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/`.

- [ ] **Step 4: 更新 AGENTS.md §known test status**

Use: `edit` tool. 5 FAIL 描述 → 5 PASS 状态 + 引用 archive tick-fix.

- [ ] **Step 5: 更新 CHANGELOG.md v0.11.0 entry**

Use: 找到 v0.11.0 section (或在 v0.10.5 添 1 行 "5 cpu_sim CLI PASS — cpu-pipeline-tick-fix (F1+F2+F3a+F3b)").

### Task 8: 8 commits 链 (per rdd-builder Phase 5/6 收尾)

- [ ] **Step 1: 8 files staged**

Run:
```bash
git add openspec/changes/cpu-pipeline-tick-fix/ \
        openspec/specs/{mmu-bare-shortcut,cpu-sim-elf-base-tohost-override,cpu-pipeline-tick-rca}/spec.md \
        tools/cpu_sim/main.cpp \
        ip/cpu/configs/{cpu_default,cpu_superscalar,cpu_deep_pipeline}.json \
        ip/mmu/tlm/{MMUPlugin.cpp,mmu_keys.h} \
        ip/cpu/plugins/ibus.h \
        tests/cpu/test_load_config_field_roundtrip.cpp \
        AGENTS.md CHANGELOG.md
```

- [ ] **Step 2: 1 commit (per CONTRIBUTING.md:25 prefix `fix(cpu):`)**

Run:
```bash
git commit -m "fix(cpu): cpu-pipeline-tick-fix — 5 cpu_sim CLI FAIL → PASS (F1+F2+F3a+F3b)

[Diagnosis reference]: openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/ (P2 实测 confirm 复合根因)

[F1] tools/cpu_sim/main.cpp:load_config 解析 8 字段 (enable_mmu / mmu_mode / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / satp_ppn / n_threads)
[F2] ip/cpu/configs/{cpu_default,cpu_superscalar,cpu_deep_pipeline}.json → enable_mmu:false + _comment (裸机烟测本意, 与 openspec/specs/riscv-tests-fixture/spec.md:74-80 一致)
[F3a] ip/mmu/tlm/MMUPlugin.cpp:109 Bare shortcut 加 sv_mode_==Bare 守卫 (修正 v0.10.4 a3afcfa strict judgment; Bare-mode byte-identical)
[F3b] ip/cpu/plugins/ibus.h:62-66 halt_when 加 SATP_MODE==0 守卫; 新增 ip/mmu/tlm/mmu_keys.h SATP_MODE key; MMUPlugin publish SATP_MODE 给 halt_when 守卫读

[Verification]:
- [cpu] 128/129 → 129/129 PASS (test_cpu_sim_real_tohost)
- [cpu-integration] 30/35 → 35/35 PASS (test_3/5/7/10stage_add_elf_end_to_end)
- [riscv-tests] 48/48 PASS (0 regression)
- [mmu] 59/59 PASS (Bare-mode byte-identical with v0.10.4)
- [mmu-verilator] 3/3 PASS
- [cpu-l1-mmu-demo] 6/6 PASS
- [cache] EndToEndTranslationThroughBridge PASS
- 架构 gate 0 FAIL

[D4 决策]: 保持 CPUConfig::enable_mmu 默认 true; 修复走 4 路径而非改默认"
```

---

## Acceptance Criteria (与 improvement AC sync)

| AC ID | 描述 | 验证 Task |
|---|---|---|
| **AC1-AC7** | 5 FAIL → 5 PASS | Task 5 Step 1-3 (Phase C) |
| **AC8-AC13** | 0 regression baseline | Task 6 Step 2-3 |
| **AC15-F1** | load_config_field_roundtrip PASS | Task 1 Step 5 |
| **AC16-F3a** | Bare-mode byte-identical | Task 3 Step 5 |
| **AC17-F3b** | Bare-mode fetch 不 stall | Task 4 Step 7 |
| **AC18-F3b** | 真 Sv32/Sv39 stall 行为保留 | Task 4 Step 7 |
| **AC21** | 0 product code change 超出 4 处 | Task 6 Step 4 |
| **AC22** | git status 收尾仅含预期 files | Task 6 Step 4 |
| **AC23** | design.md §5 综合诊断段 | P1 plan 已隐含 |

## Fallback Strategy

| Scenario | Fallback |
|---|---|
| F1 单测 8 test 失败 > 2 个 | 检查 CPUConfig struct 字段名 (Field 3 Step 1); 用 `grep -nE` 列出 struct 字段 |
| F3a 后 `[mmu] 59/59` regression | revert Task 3, 检查 `identity_translation` helper 是否真的走 v0.10.4 等价路径 |
| F3b 后 stall_matrix regression | revert Task 4, 检查 SATP_MODE publish 时机 (应在 NORMAL 而非 LATE) |
| 5 FAIL 翻转部分 (如 7-stage 仍 0) | 7-stage 走 `cpu_superscalar.json`（dispatch_width=2 + n_lanes=2 → `cpu_factory.h:367` lane dispatch 路径）, 单独 debug (历史 commit `b82af0f` lane_counters use-after-free 前科) |
| chipforge_tests build 失败 | rm -rf build + cmake -S . -B build -DCMAKE_BUILD_TYPE=Release (与本会话 P2 实测副作用一致) |

## Estimated Timeline

| Phase | Tasks | 估计时间 |
|---|---|---|
| Phase A (Data Plane) | Task 1 + 2 | 30 分钟 |
| Phase B (Control Plane) | Task 3 + 4 | 45 分钟 (MEDIUM 风险 + 静态读 a3afcfa + stall_matrix 验证) |
| Phase C (E2E) | Task 5 | 5 分钟 |
| Phase D (收尾) | Task 6 + 7 + 8 | 15 分钟 |
| **Total** | 8 tasks | **~1.5 小时 / 1 session** |

## Notes

- **D4 决策硬约束** (improvement §4 MUST): 全程**禁止**改 `CPUConfig::enable_mmu` 默认值; 修复 4 路必须同时落地
- **F3a+F3b 联动**: F3a 写 `(*n)(SATP_MODE)`, F3b 读; 若 F3a SATP_MODE 写 0 (Bare) 而 F3b halt_when 未读到, fetch 仍 stall. Task 4 Step 6 必同步.
- **Task 3 Step 5 是 Bare-mode 字节级 regression 硬约束**: `[mmu] 59/59` + `[cache] EndToEndTranslationThroughBridge` PASS = F3a 正确
- **Task 4 Step 7 是 stall_matrix 硬约束**: `16/16` PASS = F3b stall 行为保持 (真 Sv32/Sv39 翻译 PTW stall vs Bare identity no-stall)
- **基线锚点已 archive**: `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/design.md §0.8.7 FINAL Diagnosis` 是本 change 的诊断前置
