---
id: cpu-pipeline-tick-fix
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps:
  - cpu-pipeline-tick-rca (archived 2026-10-15, commit 23783a3)
review_by: 2026-11-15
---

# cpu-pipeline-tick-fix

> **Successor to** `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/` (P2 实测 branch 2026-10-15 archived)
> **Naming caveat (per RCA ⚠️ Naming Caveat)**: 本 fix 真实目标 = 修"fetch 永 stall" — `pb->run()` 持续推进, decode/memory payload 全 0, tohost 永不写. "tick-fix" 沿用 RCA pre-existing 命名.

**主题**: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
**优先级**: P1
**分类**: core-impl
**类型**: fix (debug + refactor mix)

# cpu-pipeline-tick-fix

> **Successor to** `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/` (P2 实测 branch 2026-10-15 archived)
> **Naming caveat (per RCA ⚠️ Naming Caveat)**: 本 fix 真实目标 = 修"fetch 永 stall" — `pb->run()` 持续推进, decode/memory payload 全 0, tohost 永不写. "tick-fix" 沿用 RCA pre-existing 命名.

## 架构依据

**v0.11.0 launch gate** (P0 objective, 见 `objective-soc-cpu-v0110-launch.md §8 Sprint 1`) 判据 #2 = 5 cpu_sim CLI PASS, 现在 5/5 FAIL. 真因已通过 `cpu-pipeline-tick-rca` P2 实测 (run commit `47cd8b8` design.md §0.8.7) 确诊为 **F1+F2+F3 三层复合根因**:

- **F1 — config 保真度缺陷**: `tools/cpu_sim/main.cpp:75-115` `load_config()` 静默忽略 8 个 JSON 字段 (`enable_mmu / mmu_mode / satp_ppn / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / n_threads`)
- **F2 — 测试 config 意图缺陷**: 4/5 FAIL 测试 config (cpu_default/superscalar/deep_pipeline) 显式 `enable_mmu: true`, 与裸机烟测本意不符 (spec `riscv-tests-fixture:74-80` 锁定 `enable_mmu=false`)
- **F3 — MMU 瞬态语义**: `enable_mmu=true` (默认, F1 副作用) + satp 未编程 (auto-encode Sv39) + Bare-shortcut 严格化 (v0.10.4 `a3afcfa`) + PTW 永不 inactivate = **fetch 永 stall**

P2 实测 DBG (临时 cerr, 已 revert):

```
DBG halt_when PTW_ACTIVE=0 (return false=GO)        ← cycle 0 fetch runs
DBG fetch called cyc=0 mem_=0x... tlb_node=0x...     ← 只跑一次
DBG halt_when PTW_ACTIVE=1 (return true=STALL)      ← cycle 1 起永久 stall
```

### D4 决策 (硬约束)

- **保持** `CPUConfig::enable_mmu` 默认 `true` (D4 user-locked, 2026-10-15)
- 修复走 F1+F2+F3a+F3b 四条独立路径, **不**通过"改默认 false"绕开
- F3a+F3b 双修 (Bare shortcut + halt_when 双 Bare-mode 守卫) 保证 Bare-mode byte-identical, `[mmu] 59/59` / `[mmu-verilator] 3/3` / `[cache] EndToEndTranslationThroughBridge` 全部 PASS 维持

### 关联 ADR / Spec

- ADR-040 v2.0 (TLM→HDL 移植性约束): F3a+F3b 在 TLM 路径, 不触碰 CH_MEM 双文件分离
- ADR-049 (MemoryInterface 抽象): PicolibcHostMemory `write_word`/`read_word` 接口不变
- ADR-082 (multi-cycle FSM negotiate): Bare-mode 守卫在 MMUPlugin 内部, 不引入新 capability
- `openspec/specs/riscv-tests-fixture/spec.md:74-80` (Wave 1 bare-metal M-mode fixture)
- `openspec/specs/mmu-bare-shortcut/spec.md` (Bare-mode 守卫语义 reference, Phase 6d.6 立项预备)

## 范围

### In Scope

- **F1** — `tools/cpu_sim/main.cpp:load_config()` 解析 8 个被静默丢字段 (`enable_mmu / mmu_mode / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / satp_ppn / n_threads`)
- **F2** — 3 个 cpu_*_test.json config (`cpu_default / cpu_superscalar / cpu_deep_pipeline`) 改 `enable_mmu: false` (与裸机烟测本意一致)
- **F3a** — `ip/mmu/tlm/MMUPlugin.cpp:109` Bare shortcut 加 `satp_mode_is_bare` 守卫, Bare-mode 走 identity translation
- **F3b** — `ip/cpu/plugins/ibus.h:62-66` `fetch_ctrl->halt_when` 加 Bare-mode 守卫 (`SATP_MODE==0` 时返回 false 跳过 PTW 检查)
- **新增** `tests/cpu/test_load_config_field_roundtrip.cpp` 单测 (覆盖 F1 8 字段)
- **3 spec-delta**:
  - `openspec/specs/mmu-bare-shortcut/spec.md` 1 Scenario ADDED (F3a)
  - `openspec/specs/cpu-sim-elf-base-tohost-override/spec.md` 1 Scenario ADDED (F3b)
  - `openspec/specs/cpu-pipeline-tick-rca/spec.md` ref to archive 注解

### Out Scope (严禁触碰)

- ❌ **改 `CPUConfig::enable_mmu` struct 默认值** (D4 硬约束, 见 §技术约束)
- ❌ 重写 StageLinkPlugin (HIGH 风险)
- ❌ 触碰 CH_MEM 共享头 (`_chmem.h`)
- ❌ 改 `MMUPlugin.cpp` 真 Sv32/Sv39/Sv48 翻译逻辑 (仅 Bare shortcut 守卫)
- ❌ 改 `ibus.h:62-66` 非 Bare-mode halt_when 行为
- ❌ DSE/sweep 整体 fix (只修 config 保真度; sweep matrix 协调留给独立 change)
- ❌ 重做 `cpu-sim-add-elf-tohost-fix` 工具层 workaround

## Why

`cpu-pipeline-tick-rca` P2 (commit `47cd8b8` + archived `23783a3`) 确诊 fetch 永 stall 真因是 F1+F2+F3 复合根因, 不是单因子 bug. 三个修复方向 (F1/F2/F3a+F3b) 必须并行落地:

| 单独修 | 不修其他时 5 FAIL 行为 |
|---|---|
| 只 F1 (load_config 解析 enable_mmu) | 5 测试 config 显式声明 `enable_mmu: true`, 写 MMU 真翻译 → 仍 stall |
| 只 F2 (config 改 false) | `CPUConfig::enable_mmu` 默认 true 不动, MMUPlugin 仍注册 → 仍 stall |
| 只 F3a (Bare shortcut 守卫) | 默认 satp=Sv39 MODE=8, Bare 守卫激活条件是 `sv_mode==Bare || satp_mode==0` — 真 Sv39 仍走 strict judgment → 仍 stall |
| 只 F3b (halt_when 守卫) | `SATP_MODE==0` 守卫激活; 默认 satp=8<<60 MODE=8, 不满足 → 仍 stall |

所以必须**四修**:
1. **F1** 让 `enable_mmu` 从 JSON 真正消费 (F2 的 config `enable_mmu: false` 才能生效)
2. **F2** 让 4 测试意图 (裸机烟测) 与 config 一致 (避免 OpenSpec 永不消费 `enable_mmu:true` 但 defaults 直接生效)
3. **F3a** 让 satp=0 / sv_mode=Bare 显式走 identity (而非 v0.10.4 翻车的 strict judgment)
4. **F3b** 让 halt_when Bare-mode 跳过 PTW 检查 (`SATP_MODE==0` 守卫)

**为什么不是改默认 enable_mmu=false (D4 排除)**:

- v0.10.4 hotfix `a3afcfa` Bare shortcut 严格化与默认 true 配套 → 改默认会重新打破
- `cpu_factory.h:444` `make_satp_value(Sv39, 0)` 自动编码与默认 true 关联
- `[mmu] 59/59` + `[mmu-verilator] 3/3` + `[cpu-l1-mmu-demo] 6/6` 全 PASS 依赖默认 true
- 改默认会再次打破 `EnableMMU5StageBuilds` `node_count()==13` 与 `mmu-riscv-isa-adapter/spec.md:95` "MUST return exactly 12 plugins" 等既有契约
- 历史: `cpu-mmu-integration` B2 (CHANGELOG:1303) 已显示"改 MMU 默认"导致 cascade regression 的前科

## What Changes

### F1 — load_config 字段扩展 (LOW 风险)

**修改**: `tools/cpu_sim/main.cpp:load_config()` (lines 75-115)

在 `if (j.contains("params"))` 段内新增 8 字段解析:
- `enable_mmu` (bool) → `cfg.enable_mmu` (已有 struct 字段)
- `mmu_mode` (string) → `cfg.mmu_mode`
- `enable_pmp` (bool) → `cfg.enable_pmp`
- `branch_predictor` (string) → `cfg.branch_predictor`
- `btb_entries` (uint32) → `cfg.btb_entries`
- `clock_freq_mhz` (uint32) → `cfg.clock_freq_mhz`
- `satp_ppn` (uint64) → `cfg.satp_ppn`
- `n_threads` (uint32, 已有部分支持, 保留)

**新增单测**: `tests/cpu/test_load_config_field_roundtrip.cpp` (S1 Scenario 验证)

### F2 — 4 测试 config `enable_mmu: false` (LOW 风险)

**修改 3 文件 1-line 各 + note 注释**:
- `ip/cpu/configs/cpu_default.json` (lines 9 改 false, 末尾加 note)
- `ip/cpu/configs/cpu_superscalar.json` (lines 16 改 false, 末尾加 note)
- `ip/cpu/configs/cpu_deep_pipeline.json` (lines 18 改 false, 末尾加 note)

(`cpu_embedded.json:9` 已是 false, 保持)

每改加 `"_comment": "Bare M-mode bare-metal smoke (per openspec/specs/riscv-tests-fixture/spec.md:74-80)"` 字段.

### F3a — MMUPlugin Bare shortcut 守卫 (MEDIUM 风险)

**修改**: `ip/mmu/tlm/MMUPlugin.cpp` `do_lookup()` (line 109-115)

当前:
```cpp
if (sv_mode_ != Bare) {  // v0.10.4 a3afcfa strict
  return unexpected(...);
}
```

修复:
```cpp
// cpu-pipeline-tick-fix F3a: Bare-mode 显式 identity translation
if (sv_mode_ == Bare || satp_value_mode(vaddr) == Bare) {
  return identity_translation(vaddr);
}
if (sv_mode_ != Bare) {  // existing path, 真 Sv32/Sv39/Sv48 翻译
  return unexpected(...);
}
```

**Bare-mode byte-identical 必须保持**: `[mmu] 59/59` + `[mmu-verilator] 3/3` + `[cache] EndToEndTranslationThroughBridge` 不破.

**SATP_MODE 字段写入**: `MMUPlugin` 应在 `tlb_lookup_ifetch` 节点写 `SATP_MODE` (Bare=0, Sv32=1, Sv39=8, Sv48=9) 供 F3b halt_when 守卫使用.

### F3b — ibus.h halt_when Bare-mode 守卫 (MEDIUM 风险)

**修改**: `ip/cpu/plugins/ibus.h` `fetch_ctrl->halt_when` lambda (line 62-66)

```cpp
fetch_ctrl->halt_when([&pb]() {
  auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
  if (!n) return false;  // 无 MMU registered
  // cpu-pipeline-tick-fix F3b: Bare-mode 跳过 PTW 检查
  auto mode = static_cast<int>((*n)(MmuKeys::SATP_MODE));
  if (mode == 0) return false;  // Bare = identity, no stall
  return static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE));
});
```

**同时需修**: `ip/mmu/tlm/mmu_keys.h` 加 `SATP_MODE` key definition.

**stall_matrix 16/16 必须保持**: Bare-mode 既不 stall, 真 Sv32/Sv39 PTW 完成前仍 stall.

## Capabilities

### 新增 spec-delta (3 Scenario ADDED)

`openspec/specs/mmu-bare-shortcut/spec.md` 1 Scenario ADDED:
- Scenario: Bare-mode identity translation with explicit `satp_mode==0` judgment (F3a)

`openspec/specs/cpu-sim-elf-base-tohost-override/spec.md` 1 Scenario ADDED:
- Scenario: cpu_sim CLI Bare-mode fetch 不 stall (F3b halt_when)

`openspec/specs/cpu-pipeline-tick-rca/spec.md` ref to archived design.md §0.8.7 (RCA successor 链接)

### Modified Capabilities

无 modified per-spec capability (本 change 是修复, 不修改既有 capability 语义).

### NOT added

- ❌ **新增** per-spec capability 进入 `openspec/specs/` 主 spec (除上述 3 Scenario delta 外)
- ❌ **新增** architectural concept (F3a+F3b 是局部守卫, 不引入新 abstraction)

## Impact

### 受影响的代码 (4 处)

| 文件 | F-tag | 修改行数 (est) |
|---|---|---|
| `tools/cpu_sim/main.cpp` | F1 | ~15 行 (8 字段 + 注释) |
| `ip/cpu/configs/cpu_default.json` | F2 | 2 行 (params.enable_mmu + _comment) |
| `ip/cpu/configs/cpu_superscalar.json` | F2 | 2 行 |
| `ip/cpu/configs/cpu_deep_pipeline.json` | F2 | 2 行 |
| `ip/mmu/tlm/MMUPlugin.cpp` | F3a | ~10 行 (Bare 守卫 + comment) |
| `ip/mmu/tlm/MMUPlugin.h` | F3a | 0 行 (复用现有 sv_mode_ enum) |
| `ip/mmu/tlm/mmu_keys.h` | F3b | 1 行 (SATP_MODE key) |
| `ip/cpu/plugins/ibus.h` | F3b | ~5 行 (Bare 守卫 + comment) |
| `tests/cpu/test_load_config_field_roundtrip.cpp` | F1 验证 | 新文件 ~80 行 |

### 受影响的测试

| 测试 | 当前 | 目标 |
|---|---|---|
| `[cpu] test_cpu_sim_real_tohost` | 1 FAIL | PASS |
| `[cpu-integration] test_3stage_add_elf_end_to_end` | FAIL | PASS |
| `[cpu-integration] test_5stage_add_elf_end_to_end` | FAIL | PASS |
| `[cpu-integration] test_7stage_add_elf_end_to_end` | FAIL | PASS |
| `[cpu-integration] test_10stage_add_elf_end_to_end` | FAIL | PASS |
| `[cpu]` 综合 | 128/129 | **129/129 PASS** |
| `[cpu-integration]` 综合 | 30/35 | **35/35 PASS** (含 dhrystone SIGTERM timeout 已知) |

### 0 regression 约束 (AC 强约束)

- `[riscv-tests]` 48/48 PASS
- `[mmu]` 59/59 PASS (Bare-mode byte-identical)
- `[mmu-verilator]` 3/3 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS
- `[cache]` EndToEndTranslationThroughBridge PASS (v0.10.4 hotfix 不破)
- 架构 gate `verify_adr + verify_plugin_decision + check_plugin_portability` 0 FAIL

### 受影响的 users

- CPU pipeline 测试 owner (5 FAIL 翻转 5 PASS)
- DSE/sweep owner (`tools/dse/sweep_driver.py` AC12: 可重新消费 enable_mmu 字段)
- `feat-dhrystone-dmips-gate` owner (DMIPS/MHz 测量底座解锁)
- `feat-bp-btb-gshare` owner (CoreMark ≥ 1.9 测量底座解锁)
- v0.11.0 launch gate 判据 #2 (5 cpu_sim CLI PASS) — 本 change AC1+AC2+AC3+AC4 直接兑现

### 受影响的 docs

- `AGENTS.md` §known test status: 5 FAIL 描述翻转 + 引用本 change archive
- `CHANGELOG.md` v0.11.0 entry: "5 cpu_sim CLI PASS — cpu-pipeline-tick-fix (F1+F2+F3a+F3b)"
- `objective-soc-cpu-v0110-launch.md` §8 Sprint 1 判据 #2 status: "archive cpu-pipeline-tick-fix 后 PASS"

## Acceptance

(每条独立可验证, 偏重自动测试 + DBG 行为, 避免人工 art review 拖延)

### 必须 AC (5 FAIL → 5 PASS)

- [ ] **AC1**: `[cpu] test_cpu_sim_real_tohost` PASS (baseline 1 FAIL 翻转)
- [ ] **AC2**: `[cpu-integration] test_3stage_add_elf_end_to_end` PASS
- [ ] **AC3**: `[cpu-integration] test_5stage_add_elf_end_to_end` PASS
- [ ] **AC4**: `[cpu-integration] test_7stage_add_elf_end_to_end` PASS
- [ ] **AC5**: `[cpu-integration] test_10stage_add_elf_end_to_end` PASS
- [ ] **AC6**: `[cpu] 129/129 PASS` (含 AC1)
- [ ] **AC7**: `[cpu-integration] 35/35 PASS` (含 AC2-5 + dhrystone SIGTERM timeout)

### 0 regression 硬约束

- [ ] **AC8**: `[riscv-tests] 48/48 PASS`
- [ ] **AC9**: `[mmu] 59/59 PASS` (Bare-mode byte-identical with v0.10.4)
- [ ] **AC10**: `[mmu-verilator] 3/3 PASS`
- [ ] **AC11**: `[cpu-l1-mmu-demo] 6/6 PASS`
- [ ] **AC12**: `[cpu-integration][mul-div-fsm-integration] 4/4 PASS`
- [ ] **AC13**: `[cache] EndToEndTranslationThroughBridge` PASS (v0.10.4 hotfix 维持)
- [ ] **AC14**: 架构 gate 0 FAIL (verify_adr 33/33 + verify_plugin_decision 8/8 + check_plugin_portability 12/12)

### F-tag 独立验证

- [ ] **AC15-F1**: 新增 `test_load_config_field_roundtrip` 单测 PASS (8 字段 JSON→CPUConfig 往返)
- [ ] **AC16-F3a**: Bare-mode identity translation 行为 byte-identical with v0.10.4 (`[mmu]` 含 Bare-shortcut 测试 PASS)
- [ ] **AC17-F3b**: Bare-mode fetch 不 stall (新 `[mmu]` 测试覆盖 SATP_MODE=0 → halt_when 返回 false)
- [ ] **AC18-F3b**: 真 Sv32/Sv39 PTW stall 行为保留 (现 `stall_matrix 16/16` 测试通过)

### DSE / out-of-band

- [ ] **AC19**: `tools/dse/sweep_driver.py` 重新消费 enable_mmu 字段 (sweep 输出 diff: enable_mmu:false task 走 Bare vs enable_mmu:true task 走真翻译)
- [ ] **AC20**: `tools/cpu_sim` `--no-mmu` CLI override flag (P0 RCA 临时 patch 已 revert, 本 change 重新以产品级 flag 形式落地)

### 收尾约束

- [ ] **AC21**: 0 product code change 超出 F1+F2+F3a+F3b 4 处文件 (即不动 `cpu_factory.h` struct 默认值, 验证 D4 sentinel)
- [ ] **AC22**: `git status --porcelain` 收尾仅含本 change artifacts + 4 处文件改动 + 3 spec-delta + 新单测 (无意外 stall logic 改动泄露)
- [ ] **AC23**: `code/pipeline_tick_fix_design.md` 综合诊断段 (P1 plan 必须含) 写到 §5.1 排名 + §5.2 final root cause + §5.3 D4-aligned fix
