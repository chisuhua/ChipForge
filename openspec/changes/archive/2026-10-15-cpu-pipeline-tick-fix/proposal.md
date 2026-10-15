# cpu-pipeline-tick-fix
**优先级**: P1


## Why

**v0.11.0 launch gate** (P0 objective, 见 `objective-v0110-launch.md §8 Sprint 1`) 判据 #2 = 5 cpu_sim CLI PASS, 现在 5/5 FAIL. 真因已通过 `cpu-pipeline-tick-rca` P2 实测 (run commit `47cd8b8` design.md §0.8.7) 确诊为 **F1+F2+F3 三层复合根因**:

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

## What Changes

**In Scope**:

- **F1** — `tools/cpu_sim/main.cpp:load_config()` 解析 8 个被静默丢字段 (`enable_mmu / mmu_mode / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / satp_ppn / n_threads`)
- **F2** — 3 个 cpu_*_test.json config (`cpu_default / cpu_superscalar / cpu_deep_pipeline`) 改 `enable_mmu: false` (与裸机烟测本意一致)
- **F3a** — `ip/mmu/tlm/MMUPlugin.cpp:109` Bare shortcut 加 `satp_mode_is_bare` 守卫, Bare-mode 走 identity translation
- **F3b** — `ip/cpu/plugins/ibus.h:62-66` `fetch_ctrl->halt_when` 加 Bare-mode 守卫 (`SATP_MODE==0` 时返回 false 跳过 PTW 检查)
- **新增** `tests/cpu/test_load_config_field_roundtrip.cpp` 单测 (覆盖 F1 8 字段)
- **3 spec-delta**:
- `openspec/specs/mmu-bare-shortcut/spec.md` 1 Scenario ADDED (F3a)
- `openspec/specs/cpu-sim-elf-base-tohost-override/spec.md` 1 Scenario ADDED (F3b)
- `openspec/specs/cpu-pipeline-tick-rca/spec.md` ref to archive 注解
- ❌ **改 `CPUConfig::enable_mmu` struct 默认值** (D4 硬约束, 见 §技术约束)
- ❌ 重写 StageLinkPlugin (HIGH 风险)
- ❌ 触碰 CH_MEM 共享头 (`_chmem.h`)
- ❌ 改 `MMUPlugin.cpp` 真 Sv32/Sv39/Sv48 翻译逻辑 (仅 Bare shortcut 守卫)
- ❌ 改 `ibus.h:62-66` 非 Bare-mode halt_when 行为
- ❌ DSE/sweep 整体 fix (只修 config 保真度; sweep matrix 协调留给独立 change)
- ❌ 重做 `cpu-sim-add-elf-tohost-fix` 工具层 workaround

**Out of Scope**:

- (no items specified)

## Capabilities

- Scenario: Bare-mode identity translation with explicit `satp_mode==0` judgment (F3a)
- Scenario: cpu_sim CLI Bare-mode fetch 不 stall (F3b halt_when)
- ❌ **新增** per-spec capability 进入 `openspec/specs/` 主 spec (除上述 3 Scenario delta 外)
- ❌ **新增** architectural concept (F3a+F3b 是局部守卫, 不引入新 abstraction)

## Impact

- ### 受影响的代码 (4 处)

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
- `objective-v0110-launch.md` §8 Sprint 1 判据 #2 status: "archive cpu-pipeline-tick-fix 后 PASS"

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

