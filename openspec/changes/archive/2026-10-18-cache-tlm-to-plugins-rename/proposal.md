---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0+post-launch
revision_history:
  - date: 2026-10-18
    author: rdd-builder (Sisyphus bootstrap, critic 2026-10-18 BLOCKER 修正)
    summary: |
      Plan 重写基于 critic BLOCKER/SHOULD_FIX:
      1. include 真站点从 4 → 9 (L1CachePlugin.cpp 自包含 + test_l1cache_refill + test_mmu_cache_integration + ip/cache/README.md L124)
      2. cache docs (README L3+L29+L124+L126 + integration.md L103+L106 + ADR-044 4 处) + cpu docs (4 处) + mmu docs (3 处) + AGENTS.md (实际 0 hits) 全部重统计
      3. openspec/changes/archive/ 从 23 → **58 行跨 32 文件** (impl 计数)
      4. doc_link_check KNOWN_LEGACY 是空中楼阁, 删除; 改用已有 doc_checker.py:76 DEFAULT_SKIP_PATTERNS
      5. proposal 加 frontmatter (initiative + priority + revision_history)
      6. [cache] family baseline 数字锁定 (实测 7 cpp + 28 TEST_CASE)
      7. Phase C 拆细 (40+ files → 按文件清单逐文件成 task)
---

# cache-tlm-to-plugins-rename — `ip/cache/tlm/` 目录重命名为 `ip/cache/plugins/`（对齐 ADR-083 v2.0）

## Why

本次项目目录归属梳理产出（`/tmp/opencode/code-test-ownership-report.md §F.1`）发现：**ChipForge 8 个 IP 中只有 `ip/cache/` 仍在用旧 `tlm/` 命名**，与 ADR-083 v2.0（Plugin-style 单一 source of truth）以及实际 ip/cpu 已落地的 `plugins/` 命名不一致。

| IP | 当前业务代码目录 | ADR-083 v2.0 合规 |
|----|---------|---------|
| `ip/cpu/` | `ip/cpu/plugins/` (28 个 .h) | ✅ 已落地 |
| `ip/mmu/` | `ip/mmu/tlm/` + `ip/mmu/lib/` 拆分 | ✅ lib 算法 + tlm 业务 |
| `ip/cache/` | `ip/cache/tlm/` (L1CachePlugin.{h,cpp} + cache_keys.h + l1_cache_refill_fsm_chmem.h) | ⚠️ **本次治理对象** |
| `ip/{interconnect,memory,peripheral}/` | 仅 README+STATUS | N/A (placeholder) |

**两条具体 drift**：

1. **目录命名不一致** (F.1)：`ip/cache/tlm/` vs `ip/cpu/plugins/` — 同一仓库两个 IP 用不同目录名表达 "业务 Plugin 代码"。
2. **chmem 文件混在 tlm/ 下** (F.2)：`ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 与 TLM 文件混在同一目录—违反 ADR-040 v2.0 "CH_MEM 是新正道" 隐含的双文件分离组织惯例。

### 评审修正 (per critic 2026-10-18 BLOCKER 1-3 + SHOULD-FIX §2)

| 类别 | 原 plan | 修正后 |
|------|---------|--------|
| 真 `#include` 站点数 | 4 处 | **9 处**：原 4 处 + `L1CachePlugin.cpp:26` 自包含 + `tests/cache/test_l1cache_refill_fsm_chmem.cpp:36` (include `_chmem.h`) + `tests/cache/test_mmu_cache_integration.cpp:10` + `ip/cache/README.md:124` (代码示例) |
| archive 命中数 | 23 处 | **58 行跨 32 文件** (`grep -c` 实测) |
| archive 处理策略 | 加 KNOWN_LEGACY (空中楼阁) | **不依赖 doc_link_check**：改用 `doc_checker.py:76 DEFAULT_SKIP_PATTERNS` 已含 `openspec/changes/archive` 自动豁免 |
| cache docs 路径 | 只 README 2 行 | **README 4 处 (L3+L29+L124+L126) + integration.md 2 处 (L103+L106) + ADR-044.md 4 处 (L16+L32+L251+L254+L274)** |
| cpu docs 路径 | implementation-plan/* 7 | **dse_architecture.md + M1-cpu-skeleton + README + ooo_forward_compat 4 文件** |
| mmu docs 路径 | 0 | **ip/mmu/docs/architecture.md 3 处** |
| ADR 文本引用 | 0 | **ADR-040:59 + ADR-041:16:138 + ADR-046:265 (3 文件共 4 行)** |
| AGENTS.md | "AGENTS.md 多处" | 实测 **0 命中** → 删除任务 |
| proposal frontmatter | 缺 `initiative:` | 已加 (本文件 §frontmatter) |
| [cache] family 基线 | "7/7" (估) | **实测 7 cpp / 28 TEST_CASE** (含 refill 3 + integration 4) |

## What Changes

### In Scope

#### Phase A — 真编译依赖 (9 处 #include)

| 步骤 | 文件 | 行 | 当前 | 改 |
|------|------|----|------|-----|
| A.1 | `src/cf_plugin/bridge/l1_cache_bridge.h` | 27 | `#include "ip/cache/tlm/L1CachePlugin.h"` | `#include "ip/cache/plugins/L1CachePlugin.h"` |
| A.2 | `src/cf_plugin/bridge/l1_cache_bridge.cpp` | 26 | `#include "ip/cache/tlm/L1CachePlugin.h"` | 同上 |
| A.3 | `src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp` | 23 | `#include "ip/cache/tlm/L1CachePlugin.h"` | 同上 |
| A.4 | `tests/cache/test_l1_cache_bridge.cpp` | 17 | 同上 | 同上 |
| A.5 | `tests/cache/test_l1_cache_plugin_unit.cpp` | 34 | 同上 | 同上 |
| A.6 | `tests/cache/test_mmu_cache_integration.cpp` | 10 | 同上 | 同上 |
| A.7 | `tests/cache/test_l1cache_refill_fsm_chmem.cpp` | 36 | `#include "ip/cache/tlm/l1_cache_refill_fsm_chmem.h"` | `#include "ip/cache/plugins/l1_cache_refill_fsm_chmem.h"` |
| A.8 | `ip/cache/README.md:124` (代码示例) | 124 | `\`#include "ip/cache/tlm/L1CachePlugin.h"\`` | `\`#include "ip/cache/plugins/L1CachePlugin.h"\`` |
| A.9 | `ip/cache/tlm/L1CachePlugin.cpp` (自包含) | 26 | `#include "ip/cache/tlm/L1CachePlugin.h"` | `#include "ip/cache/plugins/L1CachePlugin.h"` |

注：proposal §What Changes 误把 `l1_cache_bridge_adapter.h` 列入 Task 3，但该 .h 无 `L1CachePlugin.h` include（critic BLOCKER 1 已经定位，已剔除）。

#### Phase B — git mv (4 文件，保留 history)

| 步骤 | 源 | 目标 |
|------|----|------|
| B.1 | `ip/cache/tlm/L1CachePlugin.h` | `ip/cache/plugins/L1CachePlugin.h` |
| B.2 | `ip/cache/tlm/L1CachePlugin.cpp` | `ip/cache/plugins/L1CachePlugin.cpp` |
| B.3 | `ip/cache/tlm/cache_keys.h` | `ip/cache/plugins/cache_keys.h` |
| B.4 | `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` | `ip/cache/plugins/l1_cache_refill_fsm_chmem.h` |

#### Phase C — CMake + 工具 + 业务 docs 路径字符串 (~50 文件)

逐文件列：
- **CMake** (1): `tests/CMakeLists.txt:20` (cache impl source path)
- **工具** (2): `tools/verify_adr.sh` (directory test pattern) + (实质验证脚本内部 grep 模式, 不动 doc_link_check)
- **ip/cache/ 内 docs** (3): `ip/cache/README.md` (L3 + L29 + L124 + L126 四处) + `ip/cache/docs/integration.md` (L103 + L106) + `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` (L16 + L32 + L251 + L254 + L274 五处)
- **ip/cpu/docs** (4): `ip/cpu/docs/dse_architecture.md` + `ip/cpu/docs/implementation-plan/README.md` + `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md` + `ip/cpu/docs/research/ooo_forward_compat_gap_analysis.md`
- **ip/mmu/docs** (1): `ip/mmu/docs/architecture.md` (L77 + L85 + L149 + L159 等四处)
- **docs/architecture/** (3 ADR + 4 docs): ADR-040:59 + ADR-041:16:138 + ADR-046:265 (3 ADR 文件各 1-2 处) + `declarative-hybrid-framework.md` + `interface-design.md` + `roadmap-evolution.md` + `adr.md`
- **docs/lessons** (1): `phase-1.2-l1cacheplugin.md`
- **docs/methodology** (1): `plugin-style-design-methodology-v1.md`
- **docs/superpowers** (1): `plans/2026-06-30-catch2-test-framework.md`
- **.rddf/roadmap/archive** (1): `phase-1-tlm-foundation.md` (6 处)
- **.rddf/roadmap** (1): `strategy.md` (多处)
- **openspec/specs/** (2 而非 6): `openspec/specs/mmu-cache-idx-vipt/spec.md` + `openspec/specs/mmu-vipt-data-flow/spec.md` (实测只有 2 文件, 不是我原估的 6)
- **CHANGELOG.md** (1): 仅历史 release 记录 (≥7 处), 详 SHOULD-FIX §2)

> **AGENTS.md / ip/cache/STATUS.md**: 实测 0 命中 (`grep` 验证) → 删除原计划 Task 19/25。

#### Phase D — openspec/changes/archive/ 58 行豁免化

**Action**: NO-OP (archive 不可改) + 依赖 `tools/doc_checker.py:76` 已有 `DEFAULT_SKIP_PATTERNS` 自动豁免 `openspec/changes/archive`。本 change **不依赖** `tools/doc_link_check.sh` 加 KNOWN_LEGACY — 该机制不存在且不需要 (per critic BLOCKER 3)。

#### Phase E — 4 架构 gate 全跑

详见 §Acceptance。

### Out of Scope

- **不改任何 .h/.cpp 业务代码语义**（仅 #include 路径字符串，9 处）。
- **不改 ADR 内容**（仅改 ADR 文件中 `ip/cache/tlm` 字符串作为 path-rename 一致性维护，**不**改 ADR 语义/字段/Decision 表）。
- **不动 ip/mmu/tlm/**（mmu 命名合规）。
- **不动 placeholder IP**（`ip/{interconnect,memory,peripheral,tilecopy,tilecore}/`）。
- **不动 tests/soc/**（独立 change `tests-soc-topology-restructure`）。
- **不动 openspec/changes/archive/ 任何文件**（archive 不可改，靠 doc_checker 自动豁免）。

### 关联 ADR / Spec / Issue

- **ADR-083** (Plugin-style 单一 source of truth) — 直接依据
- **ADR-040 v2.0** (TLM→HDL portability) — DRIFT F.2 修复依据
- **`/tmp/opencode/code-test-ownership-report.md` §F.1 + §F.2** — 本次 drift 来源
- **`/tmp/opencode/cpu-docs-ownership-report.md`** — 上次 session 6 trace 未来 work（独立）
- **复现命令**: `grep -rn 'ip/cache/tlm' . | grep -v '/build/' | grep -v '/.git/'`

## Capabilities

### MUST

- 9 处 `#include "ip/cache/..."` (含 1 处 ip/cache/README.md 代码示例 + 1 处 L1CachePlugin.cpp 自包含) 全部改为 `plugins/`。
- `tests/CMakeLists.txt` L20 路径改 `plugins/`。
- `tools/verify_adr.sh` 内部 grep pattern (相关 cache 路径测试) 改 `plugins/`。
- ip/cache/{README,STATUS}.md + cache/docs/{integration,adr/ADR-044}.md 全部 `tlm` → `plugins` 字符串改完。
- ip/cpu/docs/* + ip/mmu/docs/architecture.md 中 `ip/cache/tlm` 引用改 `plugins/`。
- docs/architecture/adr/{adr.md,ADR-040,ADR-041,ADR-046}.md + docs/architecture/{declarative-hybrid-framework,interface-design,roadmap-evolution}.md 中 `ip/cache/tlm` 字符串改完。
- docs/lessons/phase-1.2-l1cacheplugin.md + docs/methodology/plugin-style-design-methodology-v1.md + docs/superpowers/plans/2026-06-30-catch2-test-framework.md 字符串改完。
- .rddf/roadmap/{strategy.md,archive/phase-1-tlm-foundation.md} 字符串改完。
- openspec/specs/{mmu-cache-idx-vipt,mmu-vipt-data-flow}/spec.md 字符串改完。
- CHANGELOG.md ≥7 处历史 release 路径字符串 (详 §Impact R-CHANGELOG, 改后加 "legacy:" 标记保持历史真实性)。

### MUST NOT

- 不修改任何 .h/.cpp 业务代码语义（仅 #include 路径字符串 + file header 第 1 行注释 `// ip/cache/tlm/<name>` 一并更新 / 不更新 per critic NIT §3）。
- 不动 ip/mmu/tlm/ 命名。
- 不动 placeholder IP。
- 不动 tests/soc/。
- **不动 openspec/changes/archive/** — 严格 archive 不可变。
- **不依赖** `tools/doc_link_check.sh` 加 KNOWN_LEGACY (该机制不存在且不必要)。
- 不删除任何 catch2 test case。

## Impact

### 受影响文件（精确清单 per critic BLOCKER）

#### 真编译依赖 (9)

- `ip/cache/tlm/L1CachePlugin.cpp` (自包含 #include)
- `src/cf_plugin/bridge/l1_cache_bridge.h`
- `src/cf_plugin/bridge/l1_cache_bridge.cpp`
- `src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp`
- `tests/cache/test_l1_cache_bridge.cpp`
- `tests/cache/test_l1_cache_plugin_unit.cpp`
- `tests/cache/test_l1cache_refill_fsm_chmem.cpp`
- `tests/cache/test_mmu_cache_integration.cpp`
- `ip/cache/README.md` (L124 代码示例 in markdown)

#### CMake (1)

- `tests/CMakeLists.txt:20`

#### 工具 (1)

- `tools/verify_adr.sh`

#### ip/cache/docs (3)

- `ip/cache/README.md` (L3 + L29 + L124 + L126, 4 处)
- `ip/cache/STATUS.md` (0 命中, 已剔除原任务)
- `ip/cache/docs/integration.md` (L103 + L106)
- `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` (5 处)

#### ip/cpu/docs (4)

- `ip/cpu/docs/dse_architecture.md`
- `ip/cpu/docs/implementation-plan/README.md`
- `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
- `ip/cpu/docs/research/ooo_forward_compat_gap_analysis.md`

#### ip/mmu/docs (1)

- `ip/mmu/docs/architecture.md` (L77 + L85 + L149 + L159)

#### docs/architecture/adr (3 ADR + 1 index)

- `docs/architecture/adr.md`
- `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md` (L59)
- `docs/architecture/adr/ADR-041-bridge-tick-pattern.md` (L16 + L138)
- `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` (L265)

#### docs/architecture/* (3)

- `declarative-hybrid-framework.md`
- `interface-design.md`
- `roadmap-evolution.md`

#### docs/lessons (1)

- `phase-1.2-l1cacheplugin.md`

#### docs/methodology (1)

- `plugin-style-design-methodology-v1.md`

#### docs/superpowers (1)

- `plans/2026-06-30-catch2-test-framework.md`

#### .rddf/roadmap (1 + 1 archive)

- `strategy.md`
- `archive/phase-1-tlm-foundation.md` (6 处)

#### openspec/specs/ (2 而非 6)

- `mmu-cache-idx-vipt/spec.md`
- `mmu-vipt-data-flow/spec.md`

#### 元文档 (1)

- `CHANGELOG.md` (≥7 处, 历史 release 字符串 + "legacy:" 标记)

#### 新增 openspec/spec-delta

- `openspec/changes/cache-tlm-to-plugins-rename/specs/cache-tlm-to-plugins-rename/spec.md` (D3, 已写, 7 Requirement)

**总计: ~50 文件路径字符串改 + 1 个 verify_adr.sh 改 + 9 处真 include 改, 0 行业务逻辑改动.**

### 风险 (per critic BLOCKER + SHOULD-FIX)

- **R-INCLUDE-1** (BLOCKER 1): 9 处 include 改完必须 `cmake --build build` 0 error → 缓解：Phase A task 1-9 逐处验证。
- **R-DOC-1** (BLOCKER 3): 改 doc_link_check.sh 是不必要的（per critic）→ 缓解：本 proposal 删除该任务；改用 doc_checker.py:76 已有豁免。
- **R-FRONTMATTER** (SHOULD-FIX): proposal 必须含 YAML frontmatter → 缓解：本文件已加 `---` + `initiative:` + `priority:` + `revision_history:`。
- **R-CHANGELOG** (NIT): CHANGELOG 历史 release 改写失真 → 缓解：保留 + 添加 `legacy:` 注释标记保持历史真实性。
- **R-COMPILE-1**: CMakeCache 残留 → 缓解：`rm -rf build/CMakeCache.txt && cmake -B build` (Task A.10 列出)。
- **R-TEST-1**: [cache] family baseline 数字不对 → 缓解：Task A.10 列锁定 `tests/cache/*.cpp` 7 文件 / 28 TEST_CASE 基线数。
- **R-ADR-1**: ADR-040/041/046 中 `ip/cache/tlm` 字符串改 → 缓解：路径一致性维护，in-scope；不改 ADR 语义。

### 下游依赖

- 上游: 无
- 下游:
  - `cache-phase1.5-4way` (openspec/changes/2026-10-08-cache-phase1.5-4way/) — 直接收益，新 plugin 在 plugins/ 下创建。
  - `cache-dse-sweep` (Wave 3)
  - `verilator-l1cache-e2e-coverage` — 该 change 引用路径同步可借此机会落地。
  - `tests-soc-topology-restructure` (并列 change) — 本 change 后 archive 23→58 行豁免自动延伸。

## Acceptance

### 必达 (Hard)

- [ ] 9 处真 #include 全部 `cmake --build build` 0 error。
- [ ] git mv 4 文件完成 (history preserved)。
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS。
- [ ] `bash tools/verify_plugin_decision.sh` 0 fail (8/8 PASS)。
- [ ] `bash tools/verify_adr.sh` 0 fail。
- [ ] `python tools/doc_checker.py --format text` 0 新增 fail (默认已豁免 archive)。
- [ ] `[cache] 7 cpp / 28 TEST_CASE PASS` (基线锁定)。
- [ ] `[cpu-l1-mmu-demo] 6/6 PASS`。
- [ ] `[mmu] 59/59 PASS`。
- [ ] `[mmu-verilator] 3/3 PASS`。
- [ ] `[riscv-tests] 48/48 PASS`。
- [ ] `bash tools/run_chipforge_tests.sh --all` 0 regression。
- [ ] `grep -rn 'ip/cache/tlm' . | grep -v '/build/' | grep -v '/.git/' | grep -v 'openspec/changes/archive' | grep -v 'cache-tlm-to-plugins-rename/' | grep -v '.rddf/improvements/' | grep -v 'CHANGELOG.md' | grep -v '/ip/cache/tlm/' | grep -v '.rddf/roadmap/archive/' | grep -v 'docs/architecture/adr.md:1200-1210' | grep -v 'docs/architecture/adr.md:.*annotation 2026-10-18'` 返回 **0 hits** (排除 openspec archive + 本 change 自身 + 同期其他 improvement + CHANGELOG 历史 release [legacy] + 改名前目录内旧内容 + `.rddf/roadmap/archive/` 历史归档 + `docs/architecture/adr.md` ADR-038 描述段含的 annotation 行)。
- [ ] **CHANGELOG.md** 仍含 7 处历史 release 的 `ip/cache/tlm` 字符串 (legacy `[legacy: ip/cache/tlm/ pre-rename]` 标记形式) — 这是有意的历史真实性保留。
- [ ] openspec archive `cache-tlm-to-plugins-rename` 成功。
