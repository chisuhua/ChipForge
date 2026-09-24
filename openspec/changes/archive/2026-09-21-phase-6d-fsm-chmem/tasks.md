## 1. 6d.6 MMU/PTW sv32 5 状态 FSM

- [x] 1.1 新建 `ip/cpu/plugins/mmu_ptw_chmem.h` (顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`) — **commit 09c9d23 (314 行)**
- [x] 1.2 `enum class PTW_State { IDLE, L0_WAIT, L1_WAIT, DONE, FAULT }` (5 状态) — **09c9d23**
- [x] 1.3 `ch_state_machine<PTW_State, 5>` DSL 实装 sv32 2-level walk — **09c9d23 + CppHDL 5af3e40 (transition_when select-tree)**
- [x] 1.4 sv32 PTE 类型判定 {R,W,X}: {0,0,0} 非叶 / {1,0,0} reserved → FAULT / 含 R 或 X (megapage) → DONE — **09c9d23**
- [x] 1.5 新增 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 3 PoC — **09c9d23 (191 行)**
- [x] 1.6 验证: 24 assertions PASS — **实测**

## 2. 6d.7 L1Cache refill FSM

- [x] 2.1 新建 `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` (顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`) — **commit 09c9d23 (228 行)**
- [x] 2.2 `enum class CacheState { IDLE, LOOKUP, MISS, REFILL_WAIT }` (4 状态) — **09c9d23**
- [x] 2.3 `ch_state_machine<CacheState, 4>` DSL 实装 (lookup hit/miss + refill wait) — **09c9d23**
- [x] 2.4 并发 race 语义: REFILL_WAIT 期间新 lookup_request 不中断 — **09c9d23 (PoC #3)**
- [x] 2.5 新增 `tests/cache/test_l1cache_refill_fsm_chmem.cpp` 3 PoC — **09c9d23 (189 行)**
- [x] 2.6 验证: 28 assertions PASS — **实测**

## 3. Check 9 (CI 扩展)

- [x] 3.1 `check_plugin_portability.sh` 新增 Check 9: grep CF_PLUGIN_USE_FSM_EXEMPT 验证使用 ch_state_machine DSL — **commit 09c9d23**
- [x] 3.2 `verify_plugin_decision.sh` Check 2 过滤豁免文件 — **09c9d23**
- [x] 3.3 8/8 → **9/9 PASS** (实测)

## 4. ADR-040 v3.0 + ADR-046 v1.0

- [x] 4.1 `docs/architecture/adr.md` ADR-040 v3.0 (D13 FSM 豁免 + D14 Verilator cycle-identical + ch_state_machine DSL 增强) — **commit 09c9d23**
- [x] 4.2 `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` 更新 "Phase 6d.6 解除" 段 — **09c9d23**
- [x] 4.3 ADR-046 v1.0 Accepted (6d.6 + 6d.7 实装证明) — **09c9d23**

## 5. CI 门禁

- [x] 5.1 `check_plugin_portability.sh` 9/9 PASS — **实测**
- [x] 5.2 `verify_adr.sh` 0 FAILED — **实测**
- [x] 5.3 `verify_plugin_decision.sh` 3+4/3 PASS — **实测**
- [x] 5.4 `./bin/chipforge_tests_chmem` 43/43 PASS (1804 assertions) — **实测**
- [x] 5.5 `./bin/chipforge_tests` TLM baseline 0 回归 — **实测**
- [x] 5.6 Verilator --lint-only 0 errors / 0 warnings — **实测**

## 6. Archive

- [x] 6.1 commit + push: 1 原子 commit (09c9d23) + CHANGELOG v0.5.0 (174b5d9) + sibling CppHDL 5af3e40 — **已落地**
- [x] 6.2 `openspec archive phase-6d-fsm-chmem -y --skip-specs`

## 7. Acceptance

- [x] `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 3 PoC PASS (24 assertions)
- [x] `tests/cache/test_l1cache_refill_fsm_chmem.cpp` 3 PoC PASS (28 assertions)
- [x] `check_plugin_portability.sh` 9/9 PASS
- [x] `verify_adr.sh` 0 FAILED
- [x] `chipforge_tests_chmem` 43/43 PASS (1804 assertions, 0 回归)
- [x] TLM baseline 386/17 (pre-existing, 0 回归)
- [x] ADR-040 v3.0 + ADR-046 v1.0 文档化

## Phase 6d 完整收官

11 个子阶段全部 ✅:
- 6d.1 DecoderPlugin (d9ebbfd)
- 6d.2 Branch + Hazard + DECODED_INST (5ab7f93 / 531b0dc)
- 6d.3 Memory Model (60a5a24)
- 6d.4 vendored ELF 端到端 (fde730c)
- 6d.5 E7 Verilator lint (0d30d64 + CppHDL 7f7da88)
- 6d.5 E8 + 6d.8 Verilator sim cycle-identical (1a99ed3 + CppHDL 3a5284d)
- 6d.6 MMU/PTW sv32 FSM (09c9d23 + CppHDL 5af3e40)
- 6d.7 L1Cache refill FSM (09c9d23)
- Check 9 (09c9d23)
- ADR-040 v3.0 + ADR-046 v1.0 (09c9d23)

OpenSpec archives:
- 2026-09-20-phase-6d-prerequisites ✅
- 2026-09-20-phase-6d-rtl-verification ✅
- 2026-09-21-phase-6d-verilator-sim ✅
- phase-6d-fsm-chmem ✅ (本次)
